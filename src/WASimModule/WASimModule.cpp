/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

COPYRIGHT: (c) Maxim Paperno; All Rights Reserved.

This file may be used under the terms of the GNU General Public License (GPL)
as published by the Free Software Foundation, either version 3 of the Licenses,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU GPL is included with this project
and is also available at <http://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <cstring>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

// the order of includes below here matters (because the MSFS/SC headers are wonky)

#include "inipp.h"

#include <MSFS/MSFS.h>
#include <MSFS/MSFS_WindowsTypes.h>
#include <MSFS/Legacy/gauges.h>
#include <SimConnect.h>

#define LOGFAULT_THREAD_NAME  WSMCMND_SERVER_NAME

#include "WASimCommander.h"
#include "utilities.h"
#include "SimConnectHelper.h"
#include "token_vars.h"
#include "key_events.h"

using namespace std;
using namespace std::chrono;
using namespace WASimCommander;
using namespace WASimCommander::Utilities;
using namespace WASimCommander::Enums;

static const time_t CONN_HEARTBEAT_SEC = CONN_TIMEOUT_SEC / 3;  // Number of seconds between checks for an active client.

//----------------------------------------------------------------------------
#pragma region Enum definitions
//----------------------------------------------------------------------------

enum SimConnectIDs : uint8_t
{
	SIMCONNECTID_INIT = 0,
	// SIMCONNECT_NOTIFICATION_GROUP_ID
	GROUP_DEFAULT,        // default notification group for all standard sim events
	// SIMCONNECT_EVENT_ID
	EVENT_FRAME,          // for frame event trigger
	// SIMCONNECT_CLIENT_EVENT_ID
	CLI_EVENT_CONNECT,    // initial client connection event
	CLI_EVENT_PING,       // incoming ping event

	SIMCONNECTID_LAST     // dynamic IDs will start here
};

enum class ClientStatus : uint8_t
{
	Unknown, Connected, Disconnected, TimedOut
};

enum class RecordType : uint8_t
{
	Unknown, CommandData, RequestData, KeyEventData
};
#pragma endregion Enums

//----------------------------------------------------------------------------
#pragma region Struct and type definitions
//----------------------------------------------------------------------------

// DataRequest tracking meta data
struct TrackedRequest : DataRequest
{
	DWORD dataId;              // our area and def ID for SimConnect
	uint32_t dataSize = 0;     // actual size of value data, since valueSize may be a special value
	ID variableId = -1;        // result of local var name lookup
	ENUM unitId = -1;          // result of unit name lookup
	steady_clock::time_point nextUpdate = steady_clock::now();  // time of next pending value check/update
	vector<uint8_t> data {};   // the last data value is stored here, for comparison to detect value changes
	string calcBytecode {};    // compiled calculator string byte code
	bool compareCheck = true;  // indicates that a result value should be compared for equality with last value before sending update

	explicit TrackedRequest(const DataRequest &req, uint32_t dataId) :
		DataRequest(req),
		dataId{dataId},
		dataSize{Utilities::getActualValueSize(req.valueSize)},
		data((size_t)dataSize, -1),
		compareCheck{req.deltaEpsilon >= 0.0f}
	{
		checkRequestType();
	}

	TrackedRequest &operator=(const DataRequest &req) {
		// reset data array size
		if (!dataSize || req.valueSize != valueSize) {
			dataSize = Utilities::getActualValueSize(req.valueSize);
			data = vector<uint8_t>(dataSize, -1);
		}
		// reset lookup data
		if (strncmp(req.nameOrCode, nameOrCode, STRSZ_REQ)) {
			variableId = -1;
			calcBytecode.clear();
		}
		if (strncmp(req.unitName, unitName, STRSZ_UNIT))
			unitId = -1;
		// reset comparison flag
		compareCheck = (req.deltaEpsilon >= 0.0f);
		DataRequest::operator=(req);
		checkRequestType();
		return *this;
	}

	void checkRequestType()
	{
		// Anything besides L/A/T type vars just gets converted to calc code, as well as any A vars with "string" unit type.
		bool isString = false;
		if (requestType == RequestType::Named && variableId < 0 &&
				(!Utilities::isIndexedVariableType(varTypePrefix) || (isString = (unitId < 0 && varTypePrefix == 'A' && !strcasecmp(unitName, "string"))))
		) {
			ostringstream codeStr = ostringstream() << '(' << varTypePrefix << ':' << nameOrCode;
			if (unitName[0] != '\0')
				codeStr << ',' << unitName;
			codeStr << ')';
			setNameOrCode(codeStr.str().c_str());
			requestType = RequestType::Calculated;
			if (isString)
				calcResultType = CalcResultType::String;
			else if (valueSize > DATA_TYPE_INT64 || valueSize < 4)
				calcResultType = CalcResultType::Integer;
			else
				calcResultType = CalcResultType::Double;
		}
	}

	friend inline std::ostream& operator<<(std::ostream& os, const TrackedRequest &c) {
		os << (const DataRequest&)c;
		return os << " TrackedRequest{dataId: " << c.dataId << "; dataSize: " << c.dataSize
			<< "; variableId: " << c.variableId << "; unitId: " << c.unitId
			<< "; calcBytecode: " << Utilities::byteArrayToHex(c.calcBytecode.data(), c.calcBytecode.size()) << "; nextUpdate: " << Utilities::timePointToString(c.nextUpdate)
			<< "; data: " << Utilities::byteArrayToHex(c.data.data(), c.dataSize) << '}';
	}
};

struct TrackedEvent
{
	uint32_t eventId;
	string code {};
	string name {};
	string execCode {};    // actual code to exec, most likely bytecode
};

typedef map<uint32_t, TrackedRequest> requestMap_t;
typedef map<uint32_t, TrackedEvent> clientEventMap_t;

// WASim Client record
struct Client
{
	const uint32_t clientId;
	const string name;
	ClientStatus status = ClientStatus::Unknown;
	LogLevel logLevel = LogLevel::None;
	bool pauseDataUpdates = false;
	steady_clock::time_point nextTimeout = steady_clock::now();
	steady_clock::time_point nextHearbeat = steady_clock::now();
	// Data Definition IDs
	DWORD cddID_command = 0;
	DWORD cddID_response = 0;
	DWORD cddID_request = 0;
	DWORD cddID_log = 0;
	DWORD cddID_keyEvent = 0;
	// request and custom event tracking
	requestMap_t requests {};
	clientEventMap_t events {};

	Client(uint32_t id, ClientStatus status = ClientStatus::Connected) :
		clientId(id),
		name{(ostringstream() << STREAM_HEX8(id)).str()},
		status(status)
	{	}
};

struct DefinitionIdRecord
{
	const RecordType type;
	Client *const client;
	explicit DefinitionIdRecord(const RecordType t, Client * const c) : type(t), client(c) {}
};

struct EventIdRecord
{
	const uint32_t eventId;
	Client *const client;
	explicit EventIdRecord(const uint32_t eId, Client * const c) : eventId(eId), client(c) {}
};

struct calcResult_t
{
	CalcResultType type;
	size_t strSize;
	int32_t varId = -1;
	int32_t unitId = -1;
	uint8_t varIndex = 0;
	const char *varName = nullptr;
	size_t resultSize = 0;
	int8_t resultMemberIndex = -1;
	FLOAT64 fVal = 0.0;
	SINT32 iVal = 0;
	string sVal;
	void setF(const FLOAT64 val) { fVal = val; resultSize = sizeof(FLOAT64); resultMemberIndex = 0; }
	void setI(const SINT32  val) { iVal = val; resultSize = sizeof(SINT32); resultMemberIndex = 1; }
	void setS(const string &&val) { sVal = std::move(val); sVal.resize(strSize); resultSize = strSize; resultMemberIndex = 2; }
};

typedef map<uint32_t, Client> clientMap_t;
typedef map<DWORD, DefinitionIdRecord> definitionIdMap_t;
typedef map<SIMCONNECT_CLIENT_EVENT_ID, EventIdRecord> eventIdMap_t;
typedef map<uint32_t, SIMCONNECT_CLIENT_EVENT_ID> pingRespEventMap_t;
#pragma endregion Structs

//----------------------------------------------------------------------------
#pragma region Globals
//----------------------------------------------------------------------------

HANDLE g_hSimConnect { INVALID_HANDLE_VALUE };
clientMap_t g_mClients {};
definitionIdMap_t g_mDefinitionIds {};
eventIdMap_t g_mEventIds {};
pingRespEventMap_t g_pingRespEventIds {};
steady_clock::time_point g_tpNextTick { steady_clock::now() };
SIMCONNECT_CLIENT_EVENT_ID g_nextClientEventId = SIMCONNECTID_LAST;
SIMCONNECT_CLIENT_DATA_DEFINITION_ID g_nextClienDataId = SIMCONNECTID_LAST;
bool g_triggersRegistered = false;
bool g_simConnectQuitEvent = false;
#pragma endregion Globals

//----------------------------------------------------------------------------
#pragma region Client Responses
//----------------------------------------------------------------------------

bool sendResponse(const Client *c, const Command &cmd)
{
	LOG_TRC << "Sending command to " << c->name << ": " << cmd;
	return INVOKE_SIMCONNECT(
		SetClientData, g_hSimConnect,
			c->cddID_response, c->cddID_response,
			SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL,
			(DWORD)sizeof(Command), (void *)&cmd
	);
}

bool sendAckNak(const Client *c, const Command &forCmd, bool ack = true, const char *msg = nullptr, double fData = 0.0) {
	return sendResponse(c, Command(ack ? CommandId::Ack : CommandId::Nak, +forCmd.commandId, msg, fData, forCmd.token));
}

bool sendAckNak(const Client *c, CommandId commandId, bool ack = true, uint32_t token = 0, const char *msg = nullptr, double fData = 0.0) {
	return sendResponse(c, Command(ack ? CommandId::Ack : CommandId::Nak, +commandId, msg, fData, token));
}

void logAndNak(const Client *c, const Command &forCmd, const ostringstream &ss) {
	sendAckNak(c, forCmd.commandId, false, forCmd.token, ss.str().c_str());
	LOG_ERR << ss.str();
}

void logAndNak(const Client *c, CommandId commandId, uint32_t token, const ostringstream &ss) {
	sendAckNak(c, commandId, false, token, ss.str().c_str());
	LOG_ERR << ss.str();
}

bool sendPing(const Client *c) {
	return sendResponse(c, Command(CommandId::Ping, c->clientId));
}

bool sendLogRecord(const Client *c, const LogRecord &log)
{
	//if (c->logLevel < LogLevel::Trace) LOG_TRC << "Sending LogRecord to " << c->name << ": " << log;
	return INVOKE_SIMCONNECT(
		SetClientData, g_hSimConnect,
		c->cddID_log, c->cddID_log,
		SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL,
		(DWORD)sizeof(LogRecord), (void *)&log
	);
}

bool writeRequestData(const Client *c, const TrackedRequest *tr, void *data)
{
	LOG_TRC << "Writing request ID " << tr->requestId << " data for " << c->name << " to CDA / CDD ID " << tr->dataId << " of size " << tr->dataSize;
	return INVOKE_SIMCONNECT(
		SetClientData, g_hSimConnect,
		tr->dataId, tr->dataId,
		SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL,
		(DWORD)tr->dataSize, data
	);
}
#pragma endregion Client Responses

//----------------------------------------------------------------------------
#pragma region Client Setup/lookup
//----------------------------------------------------------------------------

Client *findClient(uint32_t id)
{
	const clientMap_t::iterator pos = g_mClients.find(id);
	if (pos != g_mClients.cend())
		return &pos->second;
	return nullptr;
}

const DefinitionIdRecord *findDefinitionRecord(uint32_t definitionId)
{
	const definitionIdMap_t::const_iterator pos = g_mDefinitionIds.find(definitionId);
	if (pos != g_mDefinitionIds.cend())
		return &pos->second;
	return nullptr;
}

const EventIdRecord *findEventRecord(uint32_t id)
{
	const eventIdMap_t::const_iterator pos = g_mEventIds.find(id);
	if (pos != g_mEventIds.cend())
		return &pos->second;
	return nullptr;
}

TrackedRequest *findClientRequest(Client *c, uint32_t id)
{
	const requestMap_t::iterator pos = c->requests.find(id);
	if (pos != c->requests.cend())
		return &pos->second;
	return nullptr;
}

TrackedEvent *findClientEvent(Client *c, uint32_t id)
{
	const clientEventMap_t::iterator pos = c->events.find(id);
	if (pos != c->events.cend())
		return &pos->second;
	return nullptr;
}

const string *getClientEventExecString(const Client *c, uint32_t id)
{
	const clientEventMap_t::const_iterator pos = c->events.find(id);
	if (pos != c->events.cend())
		return &pos->second.execCode;
	return nullptr;
}

bool registerClientCommandDataAreas(const Client *c)
{
	// Register data area for reading commands from client; client can write to this.
	const string cmdCdaName(CDA_NAME_CMD_PFX + c->name);
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, cmdCdaName, c->cddID_command, c->cddID_command, sizeof(Command), false))
		return false;
	LOG_DBG << "Created CDA ID " << c->cddID_command << " named " << quoted(cmdCdaName) << " of size " << sizeof(Command);

	// data area for writing command responses to client; read-only for client
	const string respCdaName(CDA_NAME_RESP_PFX + c->name);
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, respCdaName, c->cddID_response, c->cddID_response, sizeof(Command), false))
		return false;
	LOG_DBG << "Created CDA ID " << c->cddID_response << " named " << quoted(respCdaName) << " of size " << sizeof(Command);

	// Listen for client commands
	return SUCCEEDED(INVOKE_SIMCONNECT(RequestClientData, g_hSimConnect, c->cddID_command, c->cddID_command, c->cddID_command, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, 0UL, 0UL, 0UL, 0UL));
}

bool registerClientRequestDataArea(const Client *c)
{
	// DataRequest area is  named "WASimCommander.Data.<client_name>"; client can write to this.
	const string cdaName(CDA_NAME_DATA_PFX + c->name);
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, cdaName, c->cddID_request, c->cddID_request, sizeof(DataRequest), false))
		return false;
	LOG_DBG << "Created CDA ID " << c->cddID_request << " named " << quoted(cdaName) << " of size " << sizeof(DataRequest);

	// Listen for client data subscription requests
	return SUCCEEDED(INVOKE_SIMCONNECT(RequestClientData, g_hSimConnect, c->cddID_request, c->cddID_request, c->cddID_request, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, 0UL, 0UL, 0UL, 0UL));
}

bool registerClientKeyEventDataArea(const Client *c)
{
	// DataRequest area is  named "WASimCommander.KeyEvent.<client_name>"; client can write to this.
	const string cdaName(CDA_NAME_KEYEV_PFX + c->name);
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, cdaName, c->cddID_keyEvent, c->cddID_keyEvent, sizeof(KeyEvent), false))
		return false;
	LOG_DBG << "Created CDA ID " << c->cddID_keyEvent << " named " << quoted(cdaName) << " of size " << sizeof(KeyEvent);

	// Listen for key events
	return SUCCEEDED(INVOKE_SIMCONNECT(RequestClientData, g_hSimConnect, c->cddID_keyEvent, c->cddID_keyEvent, c->cddID_keyEvent, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, 0UL, 0UL, 0UL, 0UL));
}

bool registerClientLogDataArea(const Client *c)
{
	// Log area is  named "WASimCommander.Log.<client_name>"; read-only for client
	const string cdaName(CDA_NAME_LOG_PFX + c->name);
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, cdaName, c->cddID_log, c->cddID_log, sizeof(LogRecord), false))
		return false;
	LOG_DBG << "Created CDA ID " << c->cddID_log << " named " << quoted(cdaName) << " of size " << sizeof(LogRecord);
	return true;
}

bool registerClientVariableDataArea(const Client *c, uint32_t areaId, uint32_t dataId, DWORD dataAreaSize, DWORD sizeOrType)
{
	// Individual variable value data areas are named "WASimCommander.Data.<client_name>.<areaId>"; read-only for client
	const string cdaName(CDA_NAME_DATA_PFX + c->name + '.' + to_string(areaId));
	if FAILED(SimConnectHelper::registerDataArea(g_hSimConnect, cdaName, dataId, dataId, sizeOrType, false))
		return false;
	LOG_DBG << "Created CDA ID " << dataId << " named " << quoted(cdaName) << " of size " << dataAreaSize;
	return true;
}

void removeClientVariableDataArea(const TrackedRequest *tr)
{
	// remove definition
	if FAILED(SimConnectHelper::removeClientDataDefinition(g_hSimConnect, tr->dataId))
		LOG_WRN << "Failed to clear ClientDataDefinition for requestId " << tr->requestId << ", check log messages.";
}

void removeClientCustomEvent(const Client *c, uint32_t eventId)
{
	SIMCONNECT_CLIENT_EVENT_ID clientEventId = 0;
	for (auto it = g_mEventIds.cbegin(), en = g_mEventIds.cend(); !clientEventId && it != en; ++it) {
		if (it->second.client == c && it->second.eventId == eventId)
			clientEventId = it->first;
	}
	if (!clientEventId) {
		LOG_WRN << "Could not find SIMCONNECT_CLIENT_EVENT_ID record for client " << c->name << " with event ID " << eventId;
		return;
	}
	INVOKE_SIMCONNECT(RemoveClientEvent, g_hSimConnect, (DWORD)c->clientId, clientEventId);
	g_mEventIds.erase(clientEventId);
	//LOG_INF << "Deleted registered event ID " << eventId << " for Client " << c->name;
}


Client *getOrCreateClient(uint32_t clientId)
{
	if (!clientId) {
		LOG_ERR << "Client ID must be greater than zero.";
		return nullptr;
	}
	if (Client *c = findClient(clientId)) {
		LOG_DBG << "Found Client " << c->name << " stat " << (int)c->status;
		return c;
	}

	// New client
	Client c(clientId);
	// assign unique IDs for SimConnect data definitions (these are used for ClientDataID, DefineID and RequestID)
	c.cddID_command = g_nextClienDataId++;
	c.cddID_response = g_nextClienDataId++;
	c.cddID_request = g_nextClienDataId++;
	c.cddID_keyEvent = g_nextClienDataId++;

	// register all data areas for this client with SimConnect
	if (!registerClientCommandDataAreas(&c))
		return nullptr;  // dispose client on failure
	registerClientRequestDataArea(&c);
	registerClientKeyEventDataArea(&c);

	// move client record into map
	Client *pC = &g_mClients.emplace(clientId, std::move(c)).first->second;
	// save mappings of the command and request data IDs (which SimConnect sends us) to the client record; for lookup in message dispatch.
	g_mDefinitionIds.emplace(piecewise_construct, forward_as_tuple(c.cddID_command), forward_as_tuple(RecordType::CommandData, pC));  // no try_emplace?
	g_mDefinitionIds.emplace(piecewise_construct, forward_as_tuple(c.cddID_request), forward_as_tuple(RecordType::RequestData, pC));
	g_mDefinitionIds.emplace(piecewise_construct, forward_as_tuple(c.cddID_keyEvent), forward_as_tuple(RecordType::KeyEventData, pC));

	LOG_INF << "Created new Client with name " << pC->name << " from ID " << clientId;
	return pC;
}

void updateClientTimeout(Client *c)
{
	const auto now = steady_clock::now();
	c->nextTimeout = now + seconds(CONN_TIMEOUT_SEC);
	c->nextHearbeat = now + seconds(CONN_HEARTBEAT_SEC);
}

Client *connectClient(Client *c)
{
	if (c) {
		c->status = ClientStatus::Connected;
		updateClientTimeout(c);
		LOG_INF << "Client connected with ID/Name " << c->name;
	}
	return c;
}

Client *connectClient(uint32_t id)
{
	return connectClient(getOrCreateClient(id));
}

void checkTriggerEventNeeded();  // forward, in Utility Functions just below

// marks client as disconnected or timed out and clears any saved requests/events
void disconnectClient(Client *c, ClientStatus newStatus = ClientStatus::Disconnected)
{
	if (!c || c->status == newStatus)
		return;

	c->status = newStatus;

	// clear all data requests
	for (const auto &req : c->requests)
		removeClientVariableDataArea(&req.second);
	c->requests.clear();
	// clear all registered events
	for (const auto &ev : c->events)
		removeClientCustomEvent(c, ev.first);
	c->events.clear();

	LOG_INF << "Disconnected Client " << c->name;
	checkTriggerEventNeeded();  // check if anyone is still connected
}

// disconnects _and_ sends a Disconnect command to client with given message
void disconnectClient(Client *c, const char *message, ClientStatus newStatus = ClientStatus::Disconnected)
{
	disconnectClient(c, newStatus);
	sendResponse(c, Command(CommandId::Disconnect, 0, message));
}

void disconnectAllClients(const char *message = nullptr)
{
	for (auto &it : g_mClients)
		disconnectClient(&it.second, message, ClientStatus::Disconnected);
}

// callback for logfault IdProxyHandler log handler
void CALLBACK clientLogHandler(const uint32_t id, const logfault::Message &msg)
{
	const Client *c = findClient(id);
	if (c && c->status == ClientStatus::Connected)
		sendLogRecord(c, LogRecord((LogLevel)msg.level_, msg.msg_.c_str(), msg.when_));
}

bool setClientLogLevel(Client *c, LogLevel level)
{
	if (c->logLevel == level)
		return true;
	c->logLevel = level;
	if (!c->cddID_log) {
		// first time level is set > None, set up log CDA and log proxy handler
		c->cddID_log = g_nextClienDataId++;
		if (!registerClientLogDataArea(c))
			return false;
		logfault::LogManager::Instance().AddHandler(
			make_unique<logfault::IdProxyHandler>(logfault::IdProxyHandler(&clientLogHandler, (logfault::LogLevel)level, c->name, c->clientId))
		);
		return true;
	}
	// change level on existing logger
	logfault::LogManager::Instance().SetLevel(c->name, (logfault::LogLevel)level);
	LOG_DBG << "Set log level for client " << c->name << " to " << Utilities::getEnumName(level, LogLevelNames);
	return true;
}
#pragma endregion Client Setup

//----------------------------------------------------------------------------
#pragma region Utility Functions
//----------------------------------------------------------------------------

// this runs once we have any requests to keep updated which essentially starts the tick() processing.
void registerTriggerEvents()
{
	// use "Frame" event as trigger for our tick() loop
	if FAILED(INVOKE_SIMCONNECT(SubscribeToSystemEvent, g_hSimConnect, (SIMCONNECT_CLIENT_EVENT_ID)EVENT_FRAME, "Frame"))
		return;
	g_triggersRegistered = true;
	LOG_INF << "DataRequest data update processing started.";
}

// and here is the opposite... if all clients disconnect we can stop the ticker loop.
void unregisterTriggerEvents()
{
	if FAILED(INVOKE_SIMCONNECT(UnsubscribeFromSystemEvent, g_hSimConnect, (SIMCONNECT_CLIENT_EVENT_ID)EVENT_FRAME))
		return;
	g_triggersRegistered = false;
	LOG_INF << "DataRequest update processing stopped.";
}

// check if any clients are connected and stop the tick() trigger if none are;
// we could also call this from removeRequest() and check each connected client if
//  they still have any active data subscriptions... but that seems excessive.
void checkTriggerEventNeeded()
{
	for (const clientMap_t::value_type &it : g_mClients) {
		if (it.second.status == ClientStatus::Connected)
			return;
	}
	unregisterTriggerEvents();
}

bool execCalculatorCode(const char *code, calcResult_t &result, bool precompiled = false)
{
	// DO NOT try to log a compiled code "string" -- it's byte code and may crash the logger
	LOG_TRC << "Executing calculator code: " << quoted(precompiled ? "Compiled code" : code) << " with result type " << Utilities::getEnumName(result.type, CalcResultTypeNames);
	if (code[0] == '\0')
		return false;
	bool ok = false;
	if (result.type == CalcResultType::None) {
		ok = execute_calculator_code(code, nullptr, nullptr, nullptr);
		LOG_TRC << "execute_calculator_code() returned: ok: " << boolalpha << ok;
		return ok;
	}
	if (result.type == CalcResultType::Formatted) {
		result.setS(string(result.strSize, '\0'));
		ok = format_calculator_string(&result.sVal[0], result.strSize, code);
		LOG_TRC << "format_calculator_string() returned: ok: " << boolalpha << ok << "; sVal: " << quoted(result.sVal);
		return ok;
	}
	PCSTRINGZ cVal = nullptr;
	if ((ok = execute_calculator_code(code, &result.fVal, &result.iVal, &cVal))) {
		result.setS(string(cVal, strnlen(cVal, result.strSize)));
		if (result.type != CalcResultType::String) {
			result.resultMemberIndex = (int8_t)result.type - 1;
			result.resultSize = 8 / (int8_t)result.type;
		}
	}
	LOG_TRC << "execute_calculator_code() returned: ok: " << boolalpha << ok << "; fVal: " << result.fVal << "; iVal: " << result.iVal << "; sVal: " << quoted(result.sVal);
	return ok;
}

bool getNamedVariableValue(char varType, calcResult_t &result)
{
	switch (varType)
	{
		case 'L':
			if (result.varId < 0)
				return false;
			if (result.unitId > -1)
				result.setF(get_named_variable_typed_value(result.varId, result.unitId));
			else
				result.setF(get_named_variable_value(result.varId));
			break;

		case 'A':
			if (result.varId < 0 || result.unitId < 0)
				return false;
			result.setF(aircraft_varget(result.varId, result.unitId, result.varIndex));
			break;

		case 'T': {
			MODULE_VAR gs_var;
			if (result.varId > -1)
				gs_var = { (GAUGE_TOKEN)result.varId };
			else if (result.varName)
				initialize_var_by_name(&gs_var, const_cast<char*>(result.varName));
			lookup_var(&gs_var);

			switch (gs_var.var_type) {
				case TYPE_FLOAT64:
				case TYPE_ANGL48:
				case TYPE_SINT48:
				case TYPE_UIF48:
				case TYPE_SIF48:
					result.setF(gs_var.var_value.n);
					break;

				case TYPE_BOOL:
				case TYPE_BOOL8:
				case TYPE_BOOL16:
				case TYPE_BOOL32:
					result.setI(gs_var.var_value.b);
					break;

				case TYPE_FLAGS:
				case TYPE_FLAGS8:
				case TYPE_FLAGS16:
				case TYPE_FLAGS32:
					result.setI(gs_var.var_value.f);
					break;

				case TYPE_ENUM:
				case TYPE_ENUM8:
				case TYPE_ENUM16:
				case TYPE_ENUM32:
					result.setI(gs_var.var_value.e);
					break;

				case TYPE_BCD16:
					result.setI(gs_var.var_value.d);
					break;

				case TYPE_BCO16:
					result.setI(gs_var.var_value.o);
					break;

				case TYPE_PUINT32:
				case TYPE_PSINT32:
					result.setI(*((int32_t*)gs_var.var_value.p));
					break;

				case TYPE_PFLOAT64:
					result.setF(*((double*)gs_var.var_value.p));
					break;

				case TYPE_PVOID: {
					const char *cVal = reinterpret_cast<const char *>(gs_var.var_value.p);
					result.setS(string(cVal, strnlen(cVal, result.strSize)));
					break;
				}

				case TYPE_VOID:
					return false;

				default:
					result.setI(gs_var.var_value.n);
					break;

			}  // result type switch
			break;
		}  // case T

		default:
			LOG_WRN << "Unknown variable type " << varType << " for getNamedVariableValue()";
			return false;
	}  // var type switch

	LOG_TRC << "Result for " << varType << " var " << result.varName << " result: f:" << result.fVal << " i:" << result.iVal << " s:" << quoted(result.sVal);
	return true;
}

int getVariableId(char varType, const char *name, bool createLocal = false)
{
	switch (varType)
	{
		case 'L':
			// local vars return -1 for invalid, and zero is a valid index
			if (createLocal)
				return register_named_variable(name);
			return check_named_variable(name);

		case 'A': {
			// SimVars return zero for invalid, normalize return value to match local vars.
			const ENUM id = get_aircraft_var_enum(name);
			return id > 0 ? id : -1;
		}

		case 'T': {
			// Token vars may also return zero for invalid (MODULE_VAR_NONE), normalize return value to match local vars.
			const GAUGE_TOKEN id = Utilities::getTokenVariableId(name);     // Intellicode erroneous error flag
			return id > MODULE_VAR_NONE ? (int)id : -1;
		}

		default:
			return -1;
	}
}

// Parse a command string to find a variable name/unit/index and populates the respective reference params.
// Lookups are done on var names, depending on varType, and unit strings, to attempt conversion to IDs.
// Used by setVariable() and getVariable(). Only handles A/L/T var types (not needed for others).
bool parseVariableString(const char varType, const char *data, ID &varId, bool createLocal, ENUM *unitId = nullptr, uint8_t *varIndex = nullptr, string *varName = nullptr, bool *existed = nullptr)
{
	string_view svVar(data, strlen(data)), svUnit{};

	if (varType != 'T') {
		// Check for unit type after variable name/id and comma
		const size_t idx = svVar.find(',');
		if (idx != string::npos) {
			svUnit = svVar.substr(idx + 1);
			svVar.remove_suffix(svVar.size() - idx);
		}
		// check for index value at end of SimVar name/ID
		if (varType == 'A' && svVar.size() > 3) {
			const string_view &svIndex = svVar.substr(svVar.size() - 3);
			const size_t idx = svIndex.find(':');
			if (idx != string::npos) {
				// strtoul returns zero if conversion fails, which works fine since zero is not a valid simvar index
				if (!!varIndex)
					*varIndex = strtoul(svIndex.data() + idx + 1, nullptr, 10);
				svVar.remove_suffix(3 - idx);
			}
		}
	}
	// empty variable name/id is invalid
	if (svVar.empty())
		return false;

	// Try to parse the remaining var name string as a numeric ID
	auto result = from_chars(svVar.data(), svVar.data() + svVar.size(), varId);
	// if number conversion failed, look up variable id
	if (result.ec != errc()) {
		const std::string vname(svVar);
		if (createLocal && !!existed) {
			varId = check_named_variable(vname.c_str());
			*existed = varId > -1;
			if (!*existed)
				varId = register_named_variable(vname.c_str());
		}
		else {
			varId = getVariableId(varType, vname.c_str(), createLocal);
		}
		if (!!varName)
			*varName = vname;
	}
	// failed to find a numeric id, whole input was invalid
	if (varId < 0)
		return false;
	// check for unit specification
	if (!!unitId && !svUnit.empty()) {
		// try to parse the string as a numeric ID
		result = from_chars(svUnit.data(), svUnit.data() + svUnit.size(), *unitId);
		// if number conversion failed, look up unit id
		if (result.ec != errc()) {
			*unitId = get_units_enum(string(svUnit).c_str());  // this may also fail but unit ID is not "critical" (caller can decide what to do)
			if (*unitId < 0)
				LOG_WRN << "Could not resolve Unit ID from string " << quoted(data);
		}
	}
	return true;
}
#pragma endregion Utility

//----------------------------------------------------------------------------
#pragma region Command Handlers
//----------------------------------------------------------------------------

void returnPingEvent(DWORD data)
{
	SIMCONNECT_CLIENT_EVENT_ID evId = 0;
	const pingRespEventMap_t::const_iterator pos = g_pingRespEventIds.find(data);
	if (pos != g_pingRespEventIds.cend()) {
		evId = pos->second;
	}
	else {
		evId = g_nextClientEventId++;
		g_pingRespEventIds.emplace(data, evId);
		const ostringstream nameBuff = ostringstream() << EVENT_NAME_PING_PFX << STREAM_HEX8(data);
		LOG_DBG << "Registering ping response event name " << quoted(nameBuff.str());
		if FAILED(SimConnectHelper::newClientEvent(g_hSimConnect, evId, nameBuff.str()))
			return;
	}
	LOG_DBG << "Responding to Ping for event name " << EVENT_NAME_PING_PFX << STREAM_HEX8(data);
	INVOKE_SIMCONNECT(TransmitClientEvent, g_hSimConnect, SIMCONNECT_OBJECT_ID_USER, evId, WSMCMND_VERSION,
		                SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
}

bool setLogLevel(Client *c, const Command *const cmd, string *ackMsg = nullptr)
{
	bool ret = true;
	bool validFac = false;
	uint8_t fac = (uint8_t)cmd->fData;
	if (fac == +LogFacility::None || (fac & +LogFacility::Remote)) {
		ret = setClientLogLevel(c, static_cast<LogLevel>(cmd->uData));
		validFac = true;
	}
	if (fac & +LogFacility::File) {
		logfault::LogManager::Instance().SetLevel("File", (logfault::LogLevel)cmd->uData);   // Intellicode erroneous error flag
		validFac = true;
		LOG_DBG << "Set log level for File logger to " << Utilities::getEnumName((LogLevel)cmd->uData, LogLevelNames);
	}
	if (fac & +LogFacility::Console) {
		logfault::LogManager::Instance().SetLevel(LOGFAULT_THREAD_NAME, (logfault::LogLevel)cmd->uData);   // Intellicode erroneous error flag
		validFac = true;
		LOG_DBG << "Set log level for Console logger to " << Utilities::getEnumName((LogLevel)cmd->uData, LogLevelNames);
	}
	if (!validFac) {
		if (ackMsg)
			*ackMsg = "Unknown LogFacility";
		LOG_ERR << "Unknown LogFacility: " << fac;
		return false;
	}
	return ret;
}

void listItems(const Client *c, const Command *const cmd)
{
	LOG_DBG << "Got List request of type " << Utilities::getEnumName((LookupItemType)cmd->uData, LookupItemTypeNames) << " for client " << c->name;
	Command resp(CommandId::List);
	resp.token = cmd->token;
	switch ((LookupItemType)cmd->uData) {
		case LookupItemType::LocalVariable: {
			int i = 0;
			while (true) {
				PCSTRINGZ varName = get_name_of_named_variable(i);
				if (!varName)
					break;
				resp.uData = i++;
				resp.setStringData(varName);
				sendResponse(c, resp);
			}
			break;
		}
		case LookupItemType::DataRequest: {
			for (const auto &r : c->requests) {
				resp.uData = r.first;
				resp.setStringData(r.second.nameOrCode);
				sendResponse(c, resp);
			}
			break;
		}
		case LookupItemType::RegisteredEvent: {
			for (const auto &r : c->events) {
				resp.uData = r.first;
				resp.setStringData(r.second.code.c_str());
				sendResponse(c, resp);
			}
			break;
		}
		default:
			logAndNak(c, *cmd, ostringstream() << "Unknown LookupItemType " << Utilities::getEnumName((LookupItemType)cmd->uData, LookupItemTypeNames) << " (" << cmd->uData << ") for List command.");
			return;
	}
	sendAckNak(c, CommandId::List, true, cmd->token, "List results completed");
}

void lookup(const Client *c, const Command *const cmd)
{
	LookupItemType type = LookupItemType(cmd->uData);
	const char *name = cmd->sData;
	LOG_TRC << "Performing Lookup of type " << Utilities::getEnumName((LookupItemType)cmd->uData, LookupItemTypeNames) << " with name " << quoted(name) << " for client " << c->name;
	int itemId = -1;
	switch (LookupItemType(cmd->uData))
	{
		case LookupItemType::LocalVariable:
			itemId = getVariableId('L', name);
			break;
		case LookupItemType::SimulatorVariable:
			itemId = getVariableId('A', name);
			break;
		case LookupItemType::TokenVariable:
			itemId = getVariableId('T', name);
			break;
		case LookupItemType::UnitType:
			itemId = get_units_enum(name);
			break;
		case LookupItemType::KeyEventId:
			itemId = Utilities::getKeyEventId(name);    // Intellicode erroneous error flag
			break;
		case LookupItemType::DataRequest:
			for (const auto &r : c->requests)
				if (!strcmp(name, r.second.nameOrCode))
					itemId = (int)r.first;
			break;
		case LookupItemType::RegisteredEvent:
			for (const auto &e : c->events)
				if (!strcmp(name, e.second.code.c_str()))
					itemId = (int)e.first;
			break;
		default:
			logAndNak(c, *cmd, ostringstream() << "Unknown LookupItemType: " << cmd->uData);
			return;
	}
	if (itemId > -1)
		sendResponse(c, Command(CommandId::Ack, (uint32_t)CommandId::Lookup, name, (double)itemId, cmd->token));
	else
		logAndNak(c, *cmd, ostringstream() << "Lookup type " << Utilities::getEnumName(type, LookupItemTypeNames) << " failed for name " << quoted(name));
}

void execCalculatorCode(const Client *c, const Command *const cmd)
{
	if (cmd->uData >= CalcResultTypeNames.size()) {
		logAndNak(c, *cmd, ostringstream() << "Unknown CalcResultType: " << cmd->uData);
		return;
	}
	const CalcResultType resultType = CalcResultType(cmd->uData);
	calcResult_t res = calcResult_t { resultType, STRSZ_CMD };
	if (!execCalculatorCode(cmd->sData, res))
		return logAndNak(c, *cmd, ostringstream() << (resultType == CalcResultType::Formatted ? "format_calculator_string() returned error status" : "execute_calculator_code() returned error status"));

	Command resp(CommandId::Ack, (uint32_t)cmd->commandId);
	resp.token = cmd->token;
	if (resultType != CalcResultType::None) {
		resp.fData = (resultType == CalcResultType::Integer ? res.iVal : res.fVal);
		resp.setStringData(res.sVal.c_str());
	}
	sendResponse(c, resp);
}

void getVariable(const Client *c, const Command *const cmd)
{
	const char varType = char(cmd->uData);
	const char *data = cmd->sData;
	LOG_TRC << "getVariable(" << varType << ", " << quoted(data) << ") for client " << c->name;

	// Anything besides L/A/T type vars just gets converted to calc code. Also if a "string" type unit A var is requested.
	size_t datalen;
	bool isString = false;
	if (!Utilities::isIndexedVariableType(varType) || (isString = varType == 'A' && (datalen = strlen(data)) > 6 && !strcasecmp(data + datalen-6, "string"))) {
		const ostringstream codeStr = ostringstream() << "(" << varType << ':' << data << ')';
		CalcResultType ctype = isString ? CalcResultType::String : CalcResultType::Double;
		const Command execCmd(cmd->commandId, +ctype, codeStr.str().c_str(), 0.0, cmd->token);
		return execCalculatorCode(c, &execCmd);
	}

	ID varId{-1};
	ENUM unitId{-1};
	uint8_t varIndex{0};
	string varName;
	bool existed = true;
	if (!parseVariableString(varType, data, varId, (cmd->commandId == CommandId::GetCreate && varType == 'L'), &unitId, &varIndex, &varName, &existed)) {
		logAndNak(c, *cmd, ostringstream() << "Could not resolve Variable ID for Get command from string " << quoted(data));
		return;
	}

	// !existed can only happen for an L var if we just created it. In that case it has a default value and unit type (0.0, number).
	if (!existed) {
		if (unitId > -1)
			set_named_variable_typed_value(varId, cmd->fData, unitId);
		else if (cmd->fData != 0.0)
			set_named_variable_value(varId, cmd->fData);
		sendResponse(c, Command(CommandId::Ack, (uint32_t)CommandId::GetCreate, nullptr, cmd->fData, cmd->token));
		LOG_DBG << "getVariable(" << quoted(data) << ") created new variable for client " << c->name;
		return;
	}

	if (unitId < 0 && varType == 'A') {
		logAndNak(c, *cmd, ostringstream() << "Could not resolve Unit ID for Get command from string " << quoted(data));
		return;
	}

	calcResult_t res = calcResult_t { CalcResultType::Double, STRSZ_CMD, varId, unitId, varIndex, varName.c_str() };
	if (!getNamedVariableValue(varType, res))
		return logAndNak(c, *cmd, ostringstream() << "getNamedVariableValue() returned error result for variable: " << quoted(data));
	Command resp(CommandId::Ack, (uint32_t)cmd->commandId);
	resp.token = cmd->token;
	switch (res.resultMemberIndex) {
		case 0: resp.fData = res.fVal; break;
		case 1: resp.fData = res.iVal; break;
		case 2: resp.setStringData(res.sVal.c_str()); break;
		default:
			return logAndNak(c, *cmd, ostringstream() << "getNamedVariableValue() for " << quoted(data) << " returned invalid result index: " << res.resultMemberIndex);
	}
	sendResponse(c, resp);
}

void setVariable(const Client *c, const Command *const cmd)
{
	const char varType = char(cmd->uData);
	const char *data = cmd->sData;
	const double value = cmd->fData;
	LOG_TRC << "setVariable(" << varType << ", " << quoted(data) << ", " << value << ") for client " << c->name;

	// Anything besides an L var just gets converted to calc code.
	if (varType != 'L') {
		const ostringstream codeStr = ostringstream() << fixed << setprecision(7) << value << " (>" << varType << ':' << data << ')';
		const Command execCmd(cmd->commandId, (uint32_t)CalcResultType::None, codeStr.str().c_str(), 0.0, cmd->token);
		return execCalculatorCode(c, &execCmd);
	}

	ID varId{-1};
	ENUM unitId{-1};
	if (!parseVariableString(varType, data, varId, (cmd->commandId == CommandId::SetCreate), &unitId)) {
		logAndNak(c, *cmd, ostringstream() << "Could not resolve Variable ID for Set command from string " << quoted(data));
		return;
	}

	if (unitId > -1)
		set_named_variable_typed_value(varId, value, unitId);
	else
		set_named_variable_value(varId, value);
	sendAckNak(c, *cmd);
}

#pragma region Data Subscription Requests  ----------------------------------------------

// Perform lookup, comparison, and storage of an individual data DataRequest. Called from tick() loop or upon demand by Client.
bool updateRequestValue(const Client *c, TrackedRequest *tr, bool compareCheck = true, string *ackMsg = nullptr)
{
	if (!tr)
		return false;

	calcResult_t res = calcResult_t { tr->calcResultType, tr->dataSize, tr->variableId, tr->unitId, tr->simVarIndex, tr->nameOrCode };

	if (tr->requestType == RequestType::Calculated) {
		if (!execCalculatorCode(tr->calcBytecode.empty() ? tr->nameOrCode : tr->calcBytecode.c_str(), res, !tr->calcBytecode.empty())) {
			// calculation error, disable further updates
			tr->period = UpdatePeriod::Never;
			if (ackMsg)
				*ackMsg = "execCalculatorCode() returned error result.";
			LOG_WRN << "updateRequestValue(" << tr->requestId << "): execCalculatorCode() returned error result, disabling further updates. " << *tr;
			return false;
		}
	}
	// named variable
	else {
		if (!getNamedVariableValue(tr->varTypePrefix, res)) {
			// lookup  error, disable further updates
			tr->period = UpdatePeriod::Never;
			if (ackMsg)
				*ackMsg = "getNamedVariableValue() returned error result.";
			LOG_WRN << "updateRequestValue(" << tr->requestId << "): getNamedVariableValue() returned error result, disabling further updates. " << *tr;
			return false;
		}
	}  // end if named var

	if (!res.resultSize || res.resultMemberIndex < 0) {
		if (ackMsg)
			*ackMsg = "Got invalid result size and/or index";
		LOG_ERR << "updateRequestValue(" << tr->requestId << ") got invalid result size: " << res.resultSize << "; and/or index: " << (int)res.resultMemberIndex;
		return false;
	}
	//if (res.resultSize > tr->dataSize) {
	//	LOG_ERR << "updateRequestValue(" << tr->requestId << "): Result size too large! Result size: " << res.resultSize << " > Request size: " << tr->dataSize;
	//	return false;
	//}

	void *data = nullptr;  // pointer to result data value
	float f32;    // buffer
	int64_t i64;  // buffer
	switch (res.resultMemberIndex) {
		// double
		case 0:
			// convert the value if necessary for proper binary representation
			switch (tr->valueSize) {
				// ordered most to least likely
				case DATA_TYPE_DOUBLE:
				case sizeof(double):
					data = &res.fVal;
					break;
				case DATA_TYPE_FLOAT:
				case sizeof(float):
					data = &(f32 = (float)res.fVal);
					break;
				case DATA_TYPE_INT32:
				case 3:
					data = &(res.iVal = (int32_t)res.fVal);
					break;
				case DATA_TYPE_INT8:
				case 1:
					data = &(res.iVal = (int8_t)res.fVal);
					break;
				case DATA_TYPE_INT16:
				case 2:
					data = &(res.iVal = (int16_t)res.fVal);
					break;
				case DATA_TYPE_INT64:
					// the widest integer any gauge API function returns is 48b (for token/MODULE_VAR) so 53b precision is OK here
					data = &(i64 = (int64_t)res.fVal);
					break;
				default:
					data = &res.fVal;
					break;
			}
			break;
		// int32
		case 1:
			data = &res.iVal;
			break;
		// string
		case 2:
			data = (void *)res.sVal.data();
			break;
	}

	if (compareCheck && tr->compareCheck && !memcmp(data, tr->data.data(), tr->dataSize)) {
		LOG_TRC << "updateRequestValue(" << tr->requestId << "): Result values are equal, skipping update";
		return true;
	}
	memcpy(tr->data.data(), data, tr->dataSize);   // Intellicode erroneous error flag
	LOG_TRC << "updateRequestValue(" << tr->requestId << "): result: " << *tr;

	writeRequestData(c, tr, data);
	return true;
}

bool removeRequest(Client *c, const uint32_t requestId)
{
	const TrackedRequest *tr = findClientRequest(c, requestId);
	if (!tr){
		logAndNak(c, CommandId::Subscribe, requestId, ostringstream() << "DataRequest ID " << requestId << " not found.");
		return false;
	}
	removeClientVariableDataArea(tr);
	c->requests.erase(tr->requestId);
	LOG_DBG << "Deleted DataRequest ID " << requestId;
	sendAckNak(c, CommandId::Subscribe, true, requestId);
	return true;
}

// returns true if request has been scheduled *and* will require regular updates (period > UpdatePeriod::Once)
bool addOrUpdateRequest(Client *c, const DataRequest *const req)
{
	LOG_DBG << "Got DataRequest from Client " << c->name << ": " << *req;

	// request type of "None" actually means to delete an existing request
	if (req->requestType == RequestType::None) {
		removeRequest(c, req->requestId);
		return false;   // no updates needed
	}

	// setup response Command for Ack/Nak
	const Command resp(CommandId::Subscribe, 0, nullptr, .0, req->requestId);

	// check for empty name/code
	if (req->nameOrCode[0] == '\0') {
		logAndNak(c, resp, ostringstream() << "Error in DataRequest ID " << req->requestId << ": Parameter 'nameOrCode' cannot be empty.");
		return false;
	}

	TrackedRequest *tr = findClientRequest(c, req->requestId);
	const bool isNewRequest = (tr == nullptr);
	const uint32_t actualValSize = Utilities::getActualValueSize(req->valueSize);

	if (isNewRequest) {
		// New request

		const SIMCONNECT_CLIENT_DATA_DEFINITION_ID newDataId = g_nextClienDataId++;
		// create a new data area and add definition
		if (!registerClientVariableDataArea(c, req->requestId, newDataId, actualValSize, req->valueSize)) {
			logAndNak(c, resp, ostringstream()  << "Error in DataRequest ID " << req->requestId << ": Failed to create ClientDataDefinition, check log messages.");
			return false;
		}
		// this may change the request from a named to a calculated type for vars/string types which don't have native gauge API access functions.
		tr = &c->requests.emplace(piecewise_construct, forward_as_tuple(req->requestId), forward_as_tuple(*req, newDataId)).first->second;  // no try_emplace?
	}
	else {
		// Existing request

		if (actualValSize > tr->dataSize) {
			logAndNak(c, resp, ostringstream() << "Error in DataRequest ID " << req->requestId << ": Value size cannot be increased after request is created.");
			return false;
		}
		// recreate data definition if necessary
		if (actualValSize != tr->dataSize) {
			// remove definition
			if FAILED(SimConnectHelper::removeClientDataDefinition(g_hSimConnect, tr->dataId)) {
				logAndNak(c, resp, ostringstream() << "Error in DataRequest ID " << req->requestId << ": Failed to clear ClientDataDefinition, check log messages.");
				return false;
			}
			// add definition
			if FAILED(SimConnectHelper::addClientDataDefinition(g_hSimConnect, tr->dataId, req->valueSize)) {
				logAndNak(c, resp, ostringstream() << "Error in DataRequest ID " << req->requestId << ": Failed to create ClientDataDefinition, check log messages.");
				return false;
			}
		}
		// update the tracked request from new request data. This resets lookup IDs or the "compiled" flag if the name/code/unit changes.
		*tr = *req;
	}

	// lookups and compiling
	if (tr->requestType == RequestType::Named) {
		// Look up variable ID if needed.
		if (tr->variableId < 0) {
			tr->variableId = getVariableId(tr->varTypePrefix, tr->nameOrCode);
			if (tr->variableId < 0) {
				if (tr->varTypePrefix == 'T') {
					LOG_WRN << "Warning in DataRequest ID " << req->requestId << ": Token variable named " << quoted(tr->nameOrCode) << " was not found. Will fall back to initialize_var_by_name().";
				}
				else {
					LOG_ERR << "Error in DataRequest ID " << req->requestId << ": Variable named " << quoted(tr->nameOrCode) << " was not found, disabling updates.";
					tr->period = UpdatePeriod::Never;
				}
			}
		}
		// look up unit ID if we don't have one already
		if (tr->unitId < 0 && tr->unitName[0] != '\0') {
			tr->unitId = get_units_enum(tr->unitName);
			if (tr->unitId < 0) {
				if (tr->varTypePrefix == 'A') {
					LOG_ERR << "Error in DataRequest ID " << req->requestId << ": Unit named " << quoted(tr->unitName) << " was not found, disabling updates.";
					tr->period = UpdatePeriod::Never;
				}
				// maybe an L var... unit is not technically required.
				else {
					LOG_WRN << "Warning in DataRequest ID " << req->requestId << ": Unit named " << quoted(tr->unitName) << " was not found, no unit type will be used.";
				}
			}
		}
	}
	// calculated value, update compiled string if needed
	// NOTE: compiling code for format_calculator_string() doesn't seem to work as advertised in the docs, see:
	//   https://devsupport.flightsimulator.com/t/gauge-calculator-code-precompile-with-code-meant-for-format-calculator-string-reports-format-errors/4457
	else if (tr->calcResultType != CalcResultType::Formatted && tr->calcBytecode.empty()) {
		// assume the command has changed and re-compile
		PCSTRINGZ pCompiled = nullptr;
		UINT32 pCompiledSize = 0;
		const bool ok = gauge_calculator_code_precompile(&pCompiled, &pCompiledSize, tr->nameOrCode);
		if (ok && pCompiled && pCompiledSize > 0) {
			tr->calcBytecode = string(pCompiled, pCompiledSize);
			// DO NOT try to log the compiled code as a string -- it's byte code now and may crash the logger
			LOG_DBG << "Got compiled calculator string with size " << pCompiledSize << ": " << Utilities::byteArrayToHex(pCompiled, pCompiledSize);
		}
		else {
			LOG_WRN << "Calculator string compilation failed. gauge_calculator_code_precompile() returned: " << boolalpha << ok
				<< " for request ID " << tr->requestId << ". Size: " << pCompiledSize << "; Result null ? " << (pCompiled == nullptr) << "; Original code : " << quoted(tr->nameOrCode);
		}
	}
	// make sure any ms interval is >= our minimum tick time
	if (tr->period == UpdatePeriod::Millisecond)
		tr->interval = max((time_t)tr->interval, TICK_PERIOD_MS);

	sendAckNak(c, resp, true);
	if (tr->period != UpdatePeriod::Never)
		updateRequestValue(c, tr, false);

	LOG_DBG << (isNewRequest ? "Added " : "Updated ") << *tr;
	return (tr->period > UpdatePeriod::Once);
}
#pragma endregion  Data Requests

#pragma region  Registered Calculator Events  ----------------------------------------------

bool removeCustomEvent(Client *c, uint32_t eventId)
{
	if (c->events.count(eventId)) {
		c->events.erase(eventId);
	}
	else {
		LOG_WRN << "Could not find event record for client " << c->name << " with event ID " << eventId;
		return false;
	}
	removeClientCustomEvent(c, eventId);
	LOG_INF << "Deleted registered event ID " << eventId << " for Client " << c->name;
	return true;
}

void createOrUpdateCustomEvent(Client *c, const Command *const cmd)
{
	const uint32_t eventId = cmd->uData;
	const char *code = cmd->sData;
	LOG_DBG << "createOrUpdateCustomEvent(" << eventId << ", " << quoted(code) << ") for client " << c->name;

	// if code string is empty then try to delete an existing event.
	if (code[0] == '\0') {
		sendAckNak(c, *cmd, removeCustomEvent(c, eventId));
		return;
	}

	TrackedEvent *ev = findClientEvent(c, eventId);
	const bool newEvent = !ev;
	if (newEvent)
		ev = &c->events.emplace(eventId, TrackedEvent{ eventId }).first->second;

	//string evName;
	string_view svCode(code);
	// Code string may contain an event name before a '$' separator.
	// Only build a full event name string for new events, since the name cannot be changed anyway.
	const size_t idx = svCode.substr(0, STRSZ_ENAME).find('$');
	if (idx != string::npos) {
		// only set names for new events
		if (newEvent) {
			const string_view &svName = svCode.substr(0, idx);
			// check that the name has a period already, otherwise prepend the default prefix
			if (svName.find('.') == string::npos)
				ev->name = WSMCMND_COMMON_NAME_PREFIX + c->name + '.';
			ev->name += svName;
		}
		svCode.remove_prefix(idx + 1);
	}
	else if (newEvent)  {
		// Default event name as the full server prefix plus event ID as a string
		ev->name = WSMCMND_COMMON_NAME_PREFIX + c->name + '.' + to_string(eventId);
	}
	if (svCode.empty()) {
		logAndNak(c, *cmd, ostringstream() << "Resulting code string " << quoted(svCode) << " is empty. Original string: " << quoted(code));
		if (newEvent)
			c->events.erase(eventId);
		return;
	}

	if (newEvent) {
		const SIMCONNECT_CLIENT_EVENT_ID clientEventId = g_nextClientEventId++;
		if SUCCEEDED(SimConnectHelper::newClientEvent(g_hSimConnect, clientEventId, ev->name, c->clientId, (c->events.empty() ? SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE : 0), true)) {
			g_mEventIds.emplace(piecewise_construct, forward_as_tuple(clientEventId), forward_as_tuple(eventId, c));
		}
		else {
			logAndNak(c, *cmd, ostringstream() << "Failed to set up new SimConnect Client Event for calculator event name " << quoted(ev->name) << "; Check log messages for details.");
			if (newEvent)
				c->events.erase(eventId);
			return;
		}
	}
	else if (!ev->code.compare(svCode)) {
		LOG_DBG << "No code changes detected for event " << eventId;
		sendAckNak(c, *cmd, true, ev->name.c_str());
		return;
	}

	// (re)compile the code string
	PCSTRINGZ pCompiled = nullptr;
	UINT32 uCompiledSize = 0;
	const bool ok = gauge_calculator_code_precompile(&pCompiled, &uCompiledSize, string(svCode).c_str());
	if (ok && pCompiled && uCompiledSize > 0) {
		c->events[eventId].execCode = string(pCompiled, uCompiledSize);
		// DO NOT try to log the compiled "string" -- it's byte code now and may crash the logger
		LOG_DBG << "Got compiled calculator string size " << uCompiledSize << ": " << Utilities::byteArrayToHex(pCompiled, uCompiledSize);
	}
	else {
		logAndNak(c, *cmd, ostringstream()
							<< "Calculator string compilation failed for event " << quoted(ev->name) << ". gauge_calculator_code_precompile() returned: " << boolalpha << ok
							<< "; size: " << uCompiledSize << "; Result null? " << (pCompiled == nullptr) << "; Original code : " << quoted(svCode));
		if (newEvent)
			c->events.erase(eventId);
		return;
	}

	LOG_INF << (newEvent ? "Added event " : "Updated event ") << eventId << ' ' << quoted(ev->name) << " for client " << c->name << " with code " << quoted(svCode);
	sendAckNak(c, *cmd, true, ev->name.c_str());
}

// Fire a registered calculator event, either directly via a client Command or from a SimConnect_TransmitClientEvent event.
bool executeClientEvent(const Client *c, uint32_t eventId, string *ackMsg = nullptr)
{
	if (const string *cmd = getClientEventExecString(c, eventId)) {
		if (execute_calculator_code(cmd->c_str(), nullptr, nullptr, nullptr)) {
			LOG_TRC << "Executed calculator event ID " << eventId;
			return true;
		}
		if (ackMsg)
			*ackMsg = "execute_calculator_code() returned false";
		LOG_WRN << "execute_calculator_code() for Event ID " << eventId << " returned false.";
	}
	else {
		if (ackMsg)
			*ackMsg = "Event ID not found.";
		LOG_ERR << "Event ID " << eventId << " not found.";
	}
	return false;
}
#pragma endregion  Registered Events

void sendKeyEvent(const Client *c, const Command *const cmd)
{
	uint32_t keyId = cmd->uData;
	if (!keyId && cmd->sData[0] > 0)
		keyId = Utilities::getKeyEventId(cmd->sData);    // Intellicode erroneous error flag

	if (keyId) {
		trigger_key_event_EX1(keyId, (UINT32)cmd->fData);
		sendAckNak(c, *cmd);
		return;
	}
	sendAckNak(c, *cmd, false, "Named Key Event not found.");
}

void sendKeyEvent(const Client *c, const KeyEvent *const kev)
{
	if (kev->eventId > KEY_NULL) {
		trigger_key_event_EX1(kev->eventId, kev->values[0], kev->values[1], kev->values[2], kev->values[3], kev->values[4]);
		sendAckNak(c, CommandId::SendKey, true, kev->token);
		return;
	}
	sendAckNak(c, CommandId::SendKey, false, kev->token, "Invalid Key Event ID.");
}

#pragma endregion Command Handlers

//----------------------------------------------------------------------------
#pragma region Core Processing
//----------------------------------------------------------------------------

void tick()
{
	const steady_clock::time_point now = steady_clock::now();
	if (g_tpNextTick > now)
		return;
	g_tpNextTick = now + milliseconds(TICK_PERIOD_MS);

	for (clientMap_t::value_type &cp : g_mClients) {
		if (cp.second.status != ClientStatus::Connected)
			continue;
		Client &c = cp.second;
		// check for timeout
		if (now >= c.nextTimeout) {
			disconnectClient(&c, "Client connection timed out.", ClientStatus::TimedOut);
			continue;
		}
		// check for heartbeat timeout and ping the client if needed
		if (now >= c.nextHearbeat) {
			c.nextHearbeat = now + seconds(CONN_HEARTBEAT_SEC);
			sendPing(&c);
		}
		if (c.pauseDataUpdates)
			continue;
		for (requestMap_t::value_type &rp : c.requests) {
			TrackedRequest &r = rp.second;
			// check if update needed
			if (r.period < UpdatePeriod::Tick || (r.interval > 0 && r.nextUpdate > now))
				continue;
			// do the update and write the result
			updateRequestValue(&c, &r);
			// schedule next update (note that updateRequestValue() may change the update period to None, for example, for invalid requests)
			if (r.period == UpdatePeriod::Millisecond)
				r.nextUpdate = now + milliseconds(r.interval);
			else if (r.period == UpdatePeriod::Tick && r.interval > 0)
				r.nextUpdate = now + milliseconds((r.interval + 1) * TICK_PERIOD_MS);
		}
	}
}

void processCommand(Client *c, const Command *const cmd)
{
	LOG_DBG << "Processing command: " << *cmd;
	bool ack = true;  // break after case to send ack/nak; return to skip it.
	string ackMsg {};
	switch (cmd->commandId) {
		case CommandId::List:
			listItems(c, cmd);
			return;

		case CommandId::Lookup:
			lookup(c, cmd);
			return;

		case CommandId::Get:
		case CommandId::GetCreate:
			getVariable(c, cmd);
			return;

		case CommandId::Set:
		case CommandId::SetCreate:
			setVariable(c, cmd);
			return;

		case CommandId::Exec:
			execCalculatorCode(c, cmd);
			return;

		case CommandId::Register:
			createOrUpdateCustomEvent(c, cmd);
			return;

		case CommandId::Transmit:
			ack = executeClientEvent(c, cmd->uData, &ackMsg);
			break;

		case CommandId::Subscribe:
			c->pauseDataUpdates = !cmd->uData;
			ackMsg = c->pauseDataUpdates ? "Data Subscription updates suspended" : "Data Subscription updates resumed";
			break;

		case CommandId::Update:
			ack = updateRequestValue(c, findClientRequest(c, cmd->uData), false, &ackMsg);  // returns false for null request
			break;

		case CommandId::SendKey:
			sendKeyEvent(c, cmd);
			return;

		case CommandId::Log:
			ack = setLogLevel(c, cmd, &ackMsg);
			break;

		case CommandId::Disconnect:
			disconnectClient(c);
			ackMsg = "Disconnected by Client command.";
			break;

		case CommandId::Ping:      // just ACK the ping
		case CommandId::Connect:   // client was already re-connected or we wouldn't be here, just ACK
			break;

		// Don't respond to Ack/Nak
		case CommandId::Ack:
		case CommandId::Nak:
			return;

		// unknown command
		default:
			ack = false;
			ackMsg = "Unknown Command ID.";
			break;
	}
	sendAckNak(c, *cmd, ack, ackMsg.c_str());
}

void CALLBACK dispatchMessage(SIMCONNECT_RECV* pData, DWORD cbData, void*)
{
	switch (pData->dwID)
	{
		case SIMCONNECT_RECV_ID_EVENT_FRAME:
			tick();
			break;

		case SIMCONNECT_RECV_ID_EVENT:
		{
			SIMCONNECT_RECV_EVENT* data = (SIMCONNECT_RECV_EVENT*)pData;
			LOG_TRC << LOG_SC_RECV_EVENT(data);

			switch (data->uEventID) {
				case CLI_EVENT_CONNECT:
					if (Client *c = connectClient(data->dwData))
						sendAckNak(c, CommandId::Connect, true, data->dwData, "Welcome To " WSMCMND_PROJECT_NAME " Server v" WSMCMND_VERSION_INFO, (double)WSMCMND_VERSION);
					break;
				case CLI_EVENT_PING:
					returnPingEvent(data->dwData);
					break;
				default:
					// possible custom event
					if (const EventIdRecord *ev = findEventRecord(data->uEventID))
						executeClientEvent(ev->client, ev->eventId);
					break;
			}
			break;
		}  // SIMCONNECT_RECV_ID_EVENT

		case SIMCONNECT_RECV_ID_CLIENT_DATA:
		{
			SIMCONNECT_RECV_CLIENT_DATA* data = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
			LOG_TRC << LOG_SC_RCV_CLIENT_DATA(data);

			const DefinitionIdRecord *dr = findDefinitionRecord(data->dwDefineID);
			if (!dr) {
				LOG_ERR << "DefinitionIdRecord " << data->dwDefineID << " not found!";
				break;
			}

			Client *c = dr->client;
			if (!c || !c->clientId) {
				LOG_CRT << "Got invalid Client pointer or ID from DefinitionIdRecord with ID " << data->dwDefineID << " of type " << (int)dr->type;
				break;
			}
			if (c->status == ClientStatus::Connected)
				updateClientTimeout(c);  // update heartbeat timer
			else if (!connectClient(c))  // assume client wants to re-connect if they're not already
				break;  // unlikely

			const size_t dataSize = (size_t)pData->dwSize + 4 - sizeof(SIMCONNECT_RECV_CLIENT_DATA);  // dwSize reports 4 bytes less than actual size of SIMCONNECT_RECV_CLIENT_DATA
			switch (dr->type) {
				case RecordType::CommandData:
					// dwData should be our Command struct, but be paranoid
					if (dataSize != sizeof(Command)) {
						LOG_CRT << "Invalid Command struct data size! Expected " << sizeof(Command) << " but got " << dataSize;
						return;
					}
					processCommand(c, reinterpret_cast<const Command *const>(&data->dwData));
					break;

				case RecordType::KeyEventData:
					// dwData should be our KeyEvent struct, but be paranoid
					if (dataSize != sizeof(KeyEvent)) {
						LOG_CRT << "Invalid KeyEvent struct data size! Expected " << sizeof(KeyEvent) << " but got " << dataSize;
						return;
					}
					sendKeyEvent(c, reinterpret_cast<const KeyEvent *const>(&data->dwData));
					break;

				case RecordType::RequestData:
					// dwData should be our DataRequest struct, but be paranoid
					if (dataSize != sizeof(DataRequest)) {
						LOG_CRT << "Invalid DataRequest struct data size! Expected " << sizeof(DataRequest) << " but got " << dataSize;
						return;
					}
					if (addOrUpdateRequest(c, reinterpret_cast<const DataRequest *const>(&data->dwData)) && !g_triggersRegistered)
						registerTriggerEvents();  // start update loop
					break;

				default:
					LOG_ERR << "Unrecognized data record type: " << (int)dr->type << " in: " << LOG_SC_RCV_CLIENT_DATA(data);
					return;
			}
			break;
		}  // SIMCONNECT_RECV_ID_CLIENT_DATA

		case SIMCONNECT_RECV_ID_EXCEPTION:
			SimConnectHelper::logSimConnectException(pData);
			break;

		case SIMCONNECT_RECV_ID_OPEN:
			SimConnectHelper::logSimVersion(pData);
			break;

		case SIMCONNECT_RECV_ID_QUIT:
			g_simConnectQuitEvent = true;
			LOG_DBG << "Received quit command from SimConnect.";
			break;

		default:
			break;
	}
}

extern "C" {

MSFS_CALLBACK void module_init(void)
{
	// config file location and name, included in project package folder
	const char *cfgFile { ".\\modules\\server_conf.ini" };
	// default logging levels
	LogLevel consoleLogLevel = LogLevel::Info, fileLogLevel = LogLevel::Info;
	// set default tracked requests limit based on current global setting
	uint32_t requestTrackingMaxRecords = SimConnectHelper::ENABLE_SIMCONNECT_REQUEST_TRACKING ? 50 : 0;

	// Read initial config from file
	ifstream is(cfgFile);
	if (is.good()) {
		inipp::Ini<char> ini;
		ini.parse(is);
		for (const auto &e : ini.errors)
			cout << e << endl;
		string consoleLevel(Utilities::getEnumName(consoleLogLevel, LogLevelNames)),
			fileLevel(Utilities::getEnumName(fileLogLevel, LogLevelNames));
		const auto &logSect = ini.sections["logging"];               // Intellicode erroneous errors next several lines
		inipp::get_value(logSect, "consoleLogLevel", consoleLevel);
		inipp::get_value(logSect, "fileLogLevel", fileLevel);
		const auto &netSect = ini.sections["network"];
		inipp::get_value(netSect, "requestTrackingMaxRecords", requestTrackingMaxRecords);
		int idx;
		if ((idx = Utilities::indexOfString(LogLevelNames, fileLevel.c_str())) > -1)
			fileLogLevel = LogLevel(idx);
		if ((idx = Utilities::indexOfString(LogLevelNames, consoleLevel.c_str())) > -1)
			consoleLogLevel = LogLevel(idx);
		//cout << cfgFile << ';' << fileLevel << '=' << (int)fileLogLevel << ';' << consoleLevel << '=' << (int)consoleLogLevel << "requestTrackingMaxRecords: " << requestTrackingMaxRecords << endl;
	}
	else {
		cout << "Config file '" << quoted(cfgFile) << "' not found, setting logging defaults to " << Utilities::getEnumName(consoleLogLevel, LogLevelNames) << " level." << endl;
	}

	logfault::LogManager::Instance().AddHandler(make_unique<logfault::StreamHandler>(cout, logfault::LogLevel(consoleLogLevel)));
	logfault::LogManager::Instance().AddHandler(make_unique<logfault::FileHandler>("\\work\\runtime", logfault::LogLevel(fileLogLevel), logfault::FileHandler::Handling::Rotate, "File"));

	LOG_INF << "Initializing " WSMCMND_PROJECT_NAME " " WSMCMND_SERVER_NAME " v" WSMCMND_VERSION_STR;

	// set up request tracking
	if ((SimConnectHelper::ENABLE_SIMCONNECT_REQUEST_TRACKING = (requestTrackingMaxRecords > 0)))
		SimConnectHelper::setMaxTrackedRequests(requestTrackingMaxRecords);

	HRESULT hr;
	if FAILED(hr = SimConnect_Open(&g_hSimConnect, WSMCMND_SERVER_NAME, (HWND)nullptr, 0, 0, -1)) {
		LOG_CRT << "SimConnect_Open failed with " << LOG_HR(hr);
		g_hSimConnect = INVALID_HANDLE_VALUE;
		return;
	}

	// register incoming client Connect event, add to notification group and set group priority
	if FAILED(SimConnectHelper::newClientEvent(g_hSimConnect, CLI_EVENT_CONNECT, string(EVENT_NAME_CONNECT, strlen(EVENT_NAME_CONNECT)), GROUP_DEFAULT, SIMCONNECT_GROUP_PRIORITY_HIGHEST))
		return;   // not much we can do w/out the connection event trigger
	// register incoming Ping event and add to notification group (this is technically not "critical" to operation so do not exit on error here
	SimConnectHelper::newClientEvent(g_hSimConnect, CLI_EVENT_PING, string(EVENT_NAME_PING, strlen(EVENT_NAME_PING)), GROUP_DEFAULT);

	// Go
	if FAILED(hr = SimConnect_CallDispatch(g_hSimConnect, dispatchMessage, nullptr)) {
		LOG_CRT << "SimConnect_CallDispatch failed with " << LOG_HR(hr);;
		return;
	}

	LOG_INF << "Initialization completed.";
}

MSFS_CALLBACK void module_deinit(void)
{
	LOG_INF << "Stopping " WSMCMND_PROJECT_NAME " " WSMCMND_SERVER_NAME;

	if (g_hSimConnect != INVALID_HANDLE_VALUE) {
		// Unloading of module would typically only happen when simulator quits, except during development and manual project rebuild.
		// The below code is just for that occasion. Normally any connected Clients should detect shutdown via the SimConnect Close event.
		if (!g_simConnectQuitEvent) {
			// Disconnect any clients (does not seem to actually send any data... SimConnect context destroyed?)
			LOG_DBG << "SimConnect apparently didn't send Quit message... disconnecting any clients.";
			disconnectAllClients("Server is shutting down.");
		}
		SimConnect_Close(g_hSimConnect);
	}
	g_hSimConnect = INVALID_HANDLE_VALUE;
}

#pragma endregion Core

}  // extern "C"
