/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

COPYRIGHT: (c) Maxim Paperno; All Rights Reserved.

This file may be used under the terms of either the GNU General Public License (GPL)
or the GNU Lesser General Public License (LGPL), as published by the Free Software
Foundation, either version 3 of the Licenses, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Copies of the GNU GPL and LGPL are included with this project
and are available at <http://www.gnu.org/licenses/>.

Except as contained in this notice, the names of the authors or
their institutions shall not be used in advertising or otherwise to
promote the sale, use or other dealings in this Software without
prior written authorization from the authors.
*/

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <set>
#include <shared_mutex>
#include <string>
#include <sstream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SimConnect.h>

#define LOGFAULT_THREAD_NAME  WSMCMND_CLIENT_NAME

// Third parties can use Simulator Custom Events in this range to communicate between 3D VC's and 2D C++ gauges.
// these defines can be found in gauges.h
#define THIRD_PARTY_EVENT_ID_MIN        		0x00011000
#define THIRD_PARTY_EVENT_ID_MAX        		0x0001FFFF

#include "client/WASimClient.h"
//#include "private/wasimclient_p.h"
#include "utilities.h"
#include "SimConnectHelper.h"
#include "inipp.h"

namespace WASimCommander::Client
{

using namespace std;
using namespace WASimCommander::Utilities;
using namespace WASimCommander::Enums;
/// \private
using Clock = std::chrono::steady_clock;

// -------------------------------------------------------------
#pragma region DataRequestRecord struct
// -------------------------------------------------------------

DataRequestRecord::DataRequestRecord() :
	DataRequestRecord(DataRequest(-1, 0, RequestType::None))
{ }

DataRequestRecord::DataRequestRecord(const DataRequest &req) :
	DataRequest(req),
	data(Utilities::getActualValueSize(valueSize), 0xFF) { }

DataRequestRecord::DataRequestRecord(DataRequest &&req) :
	DataRequest(req),
	data(Utilities::getActualValueSize(valueSize), 0xFF) { }

#pragma endregion

// -------------------------------------------------------------
#pragma region RegisteredEvent struct
// -------------------------------------------------------------

RegisteredEvent::RegisteredEvent(uint32_t eventId, const std::string &code, const std::string &name) :
	eventId{eventId}, code{code}, name{name} { }

#pragma endregion


// -------------------------------------------------------------
#pragma region WASimClient::Private class
// -------------------------------------------------------------

class WASimClient::Private
{
#pragma region Locals

	static const uint32_t DISPATCH_LOOP_WAIT_TIME = 5000;
	static const uint32_t SIMCONNECT_DATA_ALLOC_LIMIT = 1024UL * 1024UL;

	enum SimConnectIDs : uint8_t
	{
		SIMCONNECTID_INIT = 0,
		// SIMCONNECT_NOTIFICATION_GROUP_ID
		NOTIFY_GROUP_PING,     // ping response notification group
		// SIMCONNECT_CLIENT_EVENT_ID - custom named events
		CLI_EVENT_CONNECT,     // connect to server
		CLI_EVENT_PING,        // ping server
		CLI_EVENT_PING_RESP,   // ping response
		// SIMCONNECT_CLIENT_DATA_ID - data area and definition IDs
		CLI_DATA_COMMAND,
		CLI_DATA_RESPONSE,
		CLI_DATA_REQUEST,
		CLI_DATA_KEYEVENT,
		CLI_DATA_LOG,
		// SIMCONNECT_DATA_REQUEST_ID - requests for data updates
		DATA_REQ_RESPONSE,   // command response data
		DATA_REQ_LOG,        // server log data

		SIMCONNECTID_LAST    // dynamic IDs start at this value

	};

	struct TrackedRequest : public DataRequest
	{
		uint32_t dataId;  // our area and def ID
		time_t lastUpdate = 0;
		uint32_t dataSize;
		mutable shared_mutex m_dataMutex;
		vector<uint8_t> data;

		explicit TrackedRequest(const DataRequest &req, uint32_t dataId) :
			DataRequest(req),
			dataId{dataId},
			dataSize{Utilities::getActualValueSize(valueSize)},
			data(dataSize, 0xFF)
		{ }

		TrackedRequest & operator=(const DataRequest &req) {
			if (req.valueSize != valueSize) {
				unique_lock lock(m_dataMutex);
				dataSize = Utilities::getActualValueSize(req.valueSize);
				data = vector<uint8_t>(dataSize, 0xFF);
			}
			DataRequest::operator=(req);
			return *this;
		}

		DataRequestRecord toRequestRecord() const {
			DataRequestRecord drr = DataRequestRecord(static_cast<const DataRequest &>(*this));
			drr.data.assign(data.cbegin(), data.cend());
			drr.lastUpdate = lastUpdate;
			return drr;
		}

		friend inline std::ostream& operator<<(std::ostream& os, const TrackedRequest &r) {
			os << (const DataRequest &)r;
			return os << " TrackedRequest{" << " dataId: " << r.dataId << "; lastUpdate: " << r.lastUpdate << "; dataSize: " << r.dataSize << "; data: " << Utilities::byteArrayToHex(r.data.data(), r.dataSize) << '}';
		}

	};

	struct TrackedResponse
	{
		uint32_t token;
		chrono::system_clock::time_point sent { chrono::system_clock::now() };
		weak_ptr<condition_variable_any> cv {};
		shared_mutex mutex;
		Command response {};

		explicit TrackedResponse(uint32_t t, weak_ptr<condition_variable_any> cv) : token(t), cv(cv) {}
	};

	struct TrackedEvent : public RegisteredEvent
	{
		bool sentToServer = false;
		using RegisteredEvent::RegisteredEvent;
	};

	using responseMap_t = map<uint32_t, TrackedResponse>;
	using requestMap_t = map<uint32_t, TrackedRequest>;
	using eventMap_t = map<uint32_t, TrackedEvent>;

	struct TempListResult {
		atomic<LookupItemType> listType { LookupItemType::None };
		uint32_t token { 0 };
		shared_ptr<condition_variable_any> cv = make_shared<condition_variable_any>();
		atomic<Clock::time_point> nextTimeout {};
		ListResult::listResult_t result;
		shared_mutex mutex;

		ListResult::listResult_t getResult()
		{
			shared_lock lock(mutex);
			return ListResult::listResult_t(result);
		}

		void reset() {
			unique_lock lock(mutex);
			listType = LookupItemType::None;
			result.clear();
		}

	} listResult;

	struct ProgramSettings {
		filesystem::path logFilePath;
		int networkConfigId = -1;
		uint32_t networkTimeout = 1000;
		LogLevel logLevels[2][3] = {
			// Console         File            Remote
			{ LogLevel::Info, LogLevel::Info, LogLevel::None },  // Client
			{ LogLevel::None, LogLevel::None, LogLevel::None }   // Server
		};
		LogLevel &logLevel(LogSource s, LogFacility f) { return logLevels[srcIndex(s)][facIndex(f)]; }
		LogLevel logLevel(LogSource s, LogFacility f) const { return f > LogFacility::None && f <= LogFacility::Remote ? logLevels[srcIndex(s)][facIndex(f)] : LogLevel::None; }
		static int srcIndex(LogSource s) { return s == LogSource::Server ? 1 : 0; }
		static int facIndex(LogFacility f) { return f == LogFacility::Remote ? 2 : f == LogFacility::File ? 1 : 0; }
	} settings;

	friend class WASimClient;
	WASimClient *const q;

	const uint32_t clientId;
	const string clientName;
	atomic<ClientStatus> status = ClientStatus::Idle;
	uint32_t serverVersion = 0;
	atomic<Clock::time_point> serverLastSeen = Clock::time_point();
	atomic_size_t totalDataAlloc = 0;
	atomic_uint32_t nextDefId = SIMCONNECTID_LAST;
	atomic_uint32_t nextCmdToken = 1;
	atomic_uint32_t nextCustomEventID = THIRD_PARTY_EVENT_ID_MIN;
	atomic_bool runDispatchLoop = false;
	atomic_bool simConnected = false;
	atomic_bool serverConnected = false;
	atomic_bool logCDAcreated = false;
	atomic_bool requestsPaused = false;

	HANDLE hSim = nullptr;
	HANDLE hSimEvent = nullptr;
	HANDLE hDispatchStopEvent = nullptr;
	thread dispatchThread;

	clientEventCallback_t eventCb = nullptr;  // main and dispatch threads (+ "on SimConnect_Quit" thread)
	listResultsCallback_t listCb = nullptr;   // list result wait thread
	dataCallback_t dataCb = nullptr;          // dispatch thread
	logCallback_t logCb = nullptr;            // main and dispatch threads
	commandCallback_t cmdResultCb = nullptr;  // dispatch thread
	commandCallback_t respCb = nullptr;       // dispatch thread

	mutable shared_mutex mtxResponses;
	mutable shared_mutex mtxRequests;
	mutable shared_mutex mtxEvents;
	mutable shared_mutex mtxCustomEvents;

	responseMap_t reponses {};
	requestMap_t requests {};
	eventMap_t events {};
	using eventNameCache_t = unordered_map<std::string, int32_t>;
	eventNameCache_t keyEventNameCache {};
	using customEventNameCache_t = unordered_map<std::string, uint32_t>; // keeps a mapping between the registered Simulator Custom Events and their Id's
	customEventNameCache_t customEventNameCache{};
	using customEventIdCache_t = unordered_map<uint32_t, std::string>; // keeps track of which Id's have successfully been registered (avoid exception when triggering non-registered events)
	customEventIdCache_t customEventIdCache{};
	
#pragma endregion
#pragma region  Callback handling templates  ----------------------------------------------

#if defined(WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS) && !WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
	mutable mutex mtxCallbacks;
#endif

	// Rvalue arguments version
	template<typename... Args>
	void invokeCallbackImpl(function<void(Args...)> f, Args&&... args) const
	{
		if (f) {
			try {
#if defined(WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS) && !WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
				lock_guard lock(mtxCallbacks);
#endif
				bind(f, forward<Args>(args)...)();
			}
			catch (exception *e) { LOG_ERR << "Exception in callback handler: " << e->what(); }
		}
	}

	// const Lvalue arguments version
	template<typename... Args>
	void invokeCallbackImpl(function<void(const Args...)> f, const Args... args) const
	{
		if (f) {
			try {
#if defined(WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS) && !WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
				lock_guard lock(mtxCallbacks);
#endif
				bind(f, args...)();
			}
			catch (exception *e) { LOG_ERR << "Exception in callback handler: " << e->what(); }
		}
	}

	// Type deduction helper
	template<typename F, typename... Args>
	void invokeCallback(F&& f, Args&&... args) const {
		invokeCallbackImpl<remove_cv_t<remove_reference_t<Args>>...>(forward<F>(f), forward<Args>(args)...);
	}

#pragma endregion
#pragma region  Constructor and status  ----------------------------------------------

	Private(WASimClient *qptr, uint32_t clientId, const std::string &config) :
		q{qptr},
		clientId{clientId},
		clientName{(ostringstream() << STREAM_HEX8(clientId)).str()}
	{
		error_code ec;
		filesystem::path cwd = filesystem::current_path(ec);
		if (ec) {
			cerr << ec.message() << endl;
			cwd = ".";
		}
		settings.logFilePath = cwd;
		// set default tracked requests limit based on current global setting
		int requestTrackingMaxRecords = SimConnectHelper::ENABLE_SIMCONNECT_REQUEST_TRACKING ? 200 : 0;

		// Determine config file to use.
		const char *fn = "client_conf.ini";
		filesystem::path cfgFile(cwd /+ fn);
		if (!config.empty()) {
			error_code ec;
			filesystem::file_status fs = filesystem::status(config, ec);
			if (!ec && filesystem::exists(fs)) {
				cfgFile = config;
				if (filesystem::is_directory(fs))
					cfgFile /= fn;
			}
			else {
				cerr << "Config file/path '" << config << "' not found or not accessible. Using default config location " << cwd << endl;;
			}
		}
		// Read initial config from file
		ifstream is(cfgFile);
		if (is.good()) {
			inipp::Ini<char> ini;
			ini.parse(is);
			for (const auto &e : ini.errors)
				cerr << e << endl;
			string consoleLevel(getEnumName(settings.logLevel(LogSource::Client, LogFacility::Console), LogLevelNames)),
				fileLevel(getEnumName(settings.logLevel(LogSource::Client, LogFacility::File), LogLevelNames));
			const auto &logSect = ini.sections["logging"];
			inipp::get_value(logSect, "logFilepath", settings.logFilePath);
			inipp::get_value(logSect, "fileLogLevel", fileLevel);
			inipp::get_value(logSect, "consoleLogLevel", consoleLevel);
			const auto &netSect = ini.sections["network"];
			inipp::get_value(netSect, "networkConfigId", settings.networkConfigId);
			inipp::get_value(netSect, "networkTimeout", settings.networkTimeout);
			inipp::get_value(netSect, "requestTrackingMaxRecords", requestTrackingMaxRecords);
			int idx;
			if ((idx = indexOfString(LogLevelNames, fileLevel.c_str())) > -1)
				settings.logLevel(LogSource::Client, LogFacility::File) = LogLevel(idx);
			if ((idx = indexOfString(LogLevelNames, consoleLevel.c_str())) > -1)
				settings.logLevel(LogSource::Client, LogFacility::Console) = LogLevel(idx);
			//cout << settings.logFilePath << ';' << fileLevel << '=' << (int)settings.fileLogLevel << ';' << consoleLevel << '=' << (int)settings.consoleLogLevel << ';' << settings.networkConfigId << ';' << settings.networkTimeout << endl;
		}
		else {
			cerr << "Config file '" << cfgFile << "' not found or not accessible. Setting all logging to " << getEnumName(settings.logLevel(LogSource::Client, LogFacility::Console), LogLevelNames)
				<< " levels, with file in " << quoted(settings.logFilePath.string()) << endl;
		}

		// set up logging
		settings.logFilePath /= WSMCMND_CLIENT_NAME;
		setFileLogLevel(settings.logLevel(LogSource::Client, LogFacility::File));
		setConsoleLogLevel(settings.logLevel(LogSource::Client, LogFacility::Console));
		// set up request tracking
		if ((SimConnectHelper::ENABLE_SIMCONNECT_REQUEST_TRACKING = (requestTrackingMaxRecords > 0)))
			SimConnectHelper::setMaxTrackedRequests(requestTrackingMaxRecords);
	}

	bool checkInit() const { return hSim != nullptr; }
	bool isConnected() const { return checkInit() && (status & ClientStatus::Connected) == ClientStatus::Connected; }

	void setStatus(ClientStatus s, const string &msg = "")
	{
		static const char * evTypeNames[] = {
			"None",
			"Simulator Connecting", "Simulator Connected", "Simulator Disconnecting", "Simulator Disconnected",
			"Server Connecting", "Server Connected", "Server Disconnected"
		};

		ClientStatus newStat = status;
		ClientEvent ev;
		switch (s) {
			case ClientStatus::Idle:
				ev.eventType = ClientEventType::SimDisconnected;
				newStat = s;
				break;
			case ClientStatus::Initializing:
				ev.eventType = (status & ClientStatus::SimConnected) == ClientStatus::SimConnected ? ClientEventType::SimDisconnected : ClientEventType::SimConnecting;
				newStat = s;
				break;
			case ClientStatus::SimConnected:
				ev.eventType = (status & ClientStatus::Connected) == ClientStatus::Connected ? ClientEventType::ServerDisconnected : ClientEventType::SimConnected;
				newStat = (newStat & ~(ClientStatus::Connected | ClientStatus::Connecting | ClientStatus::Initializing)) | s;
				break;
			case ClientStatus::Connecting:
				ev.eventType = ClientEventType::ServerConnecting;
				newStat = (newStat & ~ClientStatus::Connected) | s;
				break;
			case ClientStatus::Connected:
				ev.eventType = ClientEventType::ServerConnected;
				newStat = (newStat & ~ClientStatus::Connecting) | s;
				break;
			case ClientStatus::ShuttingDown:
				ev.eventType = ClientEventType::SimDisconnecting;
				newStat |= s;
				break;
		}
		if (newStat == status)
			return;
		status = newStat;
		if (eventCb) {
			ev.status = status;
			if (msg == "")
				ev.message = Utilities::getEnumName(ev.eventType, evTypeNames);
			else
				ev.message = msg;
			invokeCallback(eventCb, move(ev));
		}
	}

	// simple thread burner loop to wait on a condition... don't abuse.
	bool waitCondition(const std::function<bool(void)> predicate, uint32_t timeout, uint32_t sleepMs = 1)
	{
		const auto endTime = Clock::now() + chrono::milliseconds(timeout);
		const auto sf = chrono::milliseconds(sleepMs);
		while (!predicate() && endTime > Clock::now())
			this_thread::sleep_for(sf);
		return predicate();
	}

#if 0  // unused
	HRESULT waitConditionAsync(std::function<bool(void)> predicate, uint32_t timeout = 0)
	{
		if (predicate())
			return S_OK;
		if (!timeout)
			timeout = settings.networkTimeout;
		future<bool> taskResult = async(launch::async, &Private::waitCondition, this, predicate, timeout, 1);
		future_status taskStat = taskResult.wait_until(Clock::now() + chrono::milliseconds(timeout + 50));
		return (taskStat == future_status::ready ? S_OK : E_TIMEOUT);
	}
#endif

#pragma endregion
#pragma region  Local logging  ----------------------------------------------

	void setFileLogLevel(LogLevel level)
	{
		settings.logLevel(LogSource::Client, LogFacility::File) = level;
		const string loggerName = string(WSMCMND_CLIENT_NAME) + '_' + clientName;
		if (logfault::LogManager::Instance().HaveHandler(loggerName))
			logfault::LogManager::Instance().SetLevel(loggerName, logfault::LogLevel(level));
		else if (level != LogLevel::None)
			logfault::LogManager::Instance().AddHandler(
				make_unique<logfault::FileHandler>(settings.logFilePath.string().c_str(), logfault::LogLevel(level), logfault::FileHandler::Handling::Rotate, loggerName)
			);
	}

	void setConsoleLogLevel(LogLevel level)
	{
		settings.logLevel(LogSource::Client, LogFacility::Console) = level;
		if (logfault::LogManager::Instance().HaveHandler(WSMCMND_CLIENT_NAME))
			logfault::LogManager::Instance().SetLevel(WSMCMND_CLIENT_NAME, logfault::LogLevel(level));
		else if (level != LogLevel::None)
			logfault::LogManager::Instance().AddHandler(
				make_unique<logfault::StreamHandler>(cout, logfault::LogLevel(level), WSMCMND_CLIENT_NAME)
			);
	}

	void setCallbackLogLevel(LogLevel level)
	{
		settings.logLevel(LogSource::Client, LogFacility::Remote) = level;
		if (logfault::LogManager::Instance().HaveHandler(clientName))
			logfault::LogManager::Instance().SetLevel(clientName, logfault::LogLevel(level));
	}

	// proxy log handler callback method for delivery of log records to remote listener
	void onProxyLoggerMessage(const logfault::Message &msg) {
		invokeCallback(logCb, LogRecord((LogLevel)msg.level_, msg.msg_.c_str(), msg.when_), LogSource::Client);
	}

	void setLogCallback(logCallback_t cb)
	{
		logCb = cb;
		if (cb && !logfault::LogManager::Instance().HaveHandler(clientName)) {
			logfault::LogManager::Instance().SetHandler(clientName,
				make_unique<logfault::ProxyHandler>(
					bind(&Private::onProxyLoggerMessage, this, placeholders::_1),
					logfault::LogLevel(settings.logLevel(LogSource::Client, LogFacility::Remote)),
					clientName
				)
			);
		}
		else if (!cb && logfault::LogManager::Instance().HaveHandler(clientName)) {
			logfault::LogManager::Instance().RemoveHandler(clientName);
		}
	}

#pragma endregion
#pragma region SimConnect and Server core utilities  ----------------------------------------------

	HRESULT connectSimulator(uint32_t timeout) {
		return connectSimulator(settings.networkConfigId, timeout);
	}

	HRESULT connectSimulator(int networkConfigId, uint32_t timeout)
	{
		if ((status & ClientStatus::Initializing) == ClientStatus::Initializing)
			return E_FAIL;  // unlikely, but prevent recursion

		if (checkInit()) {
			LOG_INF << WSMCMND_CLIENT_NAME " already initialized.";
			return E_FAIL;
		}
		settings.networkConfigId = networkConfigId;
		LOG_INF << "Initializing " << WSMCMND_CLIENT_NAME << " v" WSMCMND_VERSION_INFO " for client " << clientName << " with net config ID " << settings.networkConfigId;
		setStatus(ClientStatus::Initializing);

		if (!timeout)
			timeout = settings.networkTimeout;

		if (!hDispatchStopEvent) {
			hDispatchStopEvent = CreateEvent(nullptr, true, false, nullptr);
			if (!hDispatchStopEvent) {
				LOG_ERR << "Failed to CreateEvent() for dispatch loop: " << GetLastError();
				return E_FAIL;
			}
		}

		hSimEvent = CreateEvent(nullptr, false, false, nullptr);
		simConnected = false;

		// try to open connection; may return S_OK, E_FAIL, or E_INVALIDARG. May also time out and never actually send the Open event message.
		HRESULT hr = SimConnect_Open(&hSim, clientName.c_str(), nullptr, 0, hSimEvent, settings.networkConfigId);

		if FAILED(hr) {
			LOG_ERR << "Network connection to Simulator failed with result: " << LOG_HR(hr);
			hSim = nullptr;
			setStatus(ClientStatus::Idle);
			return hr;
		}
		// start message dispatch loop. required to do this now in order to receive the SimConnect Open event and finish the connection process.
		ResetEvent(hDispatchStopEvent);
		dispatchThread = thread(&Private::dispatchLoop, this);
		dispatchThread.detach();
		// wait for dispatch loop to start
		if (!waitCondition([&]() { return !!runDispatchLoop; }, 10)) {
			LOG_CRT << "Could not start dispatch loop thread.";
			disconnectSimulator();
			return E_FAIL;
		}
		// wait until sim connects or times out or disconnectSimulator() is called.
		if (!waitCondition([&]() { return !runDispatchLoop || simConnected; }, timeout)) {
			LOG_ERR << "Network connection to Simulator timed out after " << timeout << "ms.";
			disconnectSimulator();
			return E_TIMEOUT;
		}

		// register server's Connect command event, exit on failure
		if FAILED(hr = SimConnectHelper::newClientEvent(hSim, CLI_EVENT_CONNECT, EVENT_NAME_CONNECT)) {
			disconnectSimulator();
			return hr;
		}
		// register server's ping command event
		SimConnectHelper::newClientEvent(hSim, CLI_EVENT_PING, EVENT_NAME_PING);
		// register and subscribe to server ping response (the server will append the name of our client to a new named event and trigger it).
		SimConnectHelper::newClientEvent(hSim, CLI_EVENT_PING_RESP, EVENT_NAME_PING_PFX + clientName, NOTIFY_GROUP_PING, SIMCONNECT_GROUP_PRIORITY_HIGHEST);

		// register area for writing commands (this is read-only for the server)
		if FAILED(hr = registerDataArea(CDA_NAME_CMD_PFX, CLI_DATA_COMMAND, CLI_DATA_COMMAND, sizeof(Command), true)) {
			disconnectSimulator();
			return hr;
		}
		// register command response area for reading (server can write to this channel)
		if FAILED(hr = registerDataArea(CDA_NAME_RESP_PFX, CLI_DATA_RESPONSE, CLI_DATA_RESPONSE, sizeof(Command), false)) {
			disconnectSimulator();
			return hr;
		}
		// register CDA for writing data requests (this is read-only for the server)
		registerDataArea(CDA_NAME_DATA_PFX, CLI_DATA_REQUEST, CLI_DATA_REQUEST, sizeof(DataRequest), true);
		// register CDA for writing key events (this is read-only for the server)
		registerDataArea(CDA_NAME_KEYEV_PFX, CLI_DATA_KEYEVENT, CLI_DATA_KEYEVENT, sizeof(KeyEvent), true);

		// start listening on the response channel
		if FAILED(hr = INVOKE_SIMCONNECT(
			RequestClientData, hSim, (SIMCONNECT_CLIENT_DATA_ID)CLI_DATA_RESPONSE, (SIMCONNECT_DATA_REQUEST_ID)DATA_REQ_RESPONSE,
		  (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)CLI_DATA_RESPONSE, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, (SIMCONNECT_CLIENT_DATA_REQUEST_FLAG)0, 0UL, 0UL, 0UL
		)) {
			disconnectSimulator();
			return hr;
		}

		// possibly re-register SimConnect CDAs for any existing data requests
		registerAllDataRequestAreas();
		// re-register all Simulator Custom Events
		registerAllCustomEvents();

		setStatus(ClientStatus::SimConnected);
		return S_OK;
	}

	// onQuitEvent == true when SimConnect is shutting down and sends Quit, in which case we don't try to use SimConnect again.
	void disconnectSimulator(bool onQuitEvent = false)
	{
		if (!checkInit()) {
			LOG_INF << "No network connection. Shutdown complete.";
			setStatus(ClientStatus::Idle);
			return;
		}

		LOG_DBG << "Shutting down...";
		disconnectServer(!onQuitEvent);
		setStatus(ClientStatus::ShuttingDown);
		SetEvent(hDispatchStopEvent);  // signal thread to exit
		//d->dispatchThread.join();

		// check for dispatch loop thread shutdown (this is really just for logging/debug purposes)
		if (!waitCondition([&]() { return !runDispatchLoop; }, DISPATCH_LOOP_WAIT_TIME + 100))
			LOG_CRT << "Dispatch loop thread still running!";

		// reset flags/counters
		simConnected = false;
		logCDAcreated = false;
		totalDataAlloc = 0;

		// dispose objects
		if (hSim && !onQuitEvent)
			SimConnect_Close(hSim);
		hSim = nullptr;
		if (hSimEvent)
			CloseHandle(hSimEvent);
		hSimEvent = nullptr;

		setStatus(ClientStatus::Idle);
		LOG_INF << "Shutdown complete, network disconnected.";
	}

	HRESULT connectServer(uint32_t timeout)
	{
		if ((status & ClientStatus::Connecting) == ClientStatus::Connecting)
			return E_FAIL;  // unlikely, but prevent recursion

		if (!timeout)
			timeout = settings.networkTimeout;

		HRESULT hr;
		if (!checkInit() && FAILED(hr = connectSimulator(timeout)))
			return hr;

		LOG_INF << "Connecting to " WSMCMND_PROJECT_NAME " server...";
		setStatus(ClientStatus::Connecting);

		// Send the initial "connect" event which we registered in connectSimulator(), using this client's ID as the data parameter.
		if FAILED(hr = INVOKE_SIMCONNECT(TransmitClientEvent, hSim, SIMCONNECT_OBJECT_ID_USER, (SIMCONNECT_CLIENT_EVENT_ID)CLI_EVENT_CONNECT, (DWORD)clientId, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY)) {
			setStatus(ClientStatus::SimConnected);
			return hr;
		}
		// The connection response, if any, is written by the server into the "command response" client data area, CLI_DATA_ID_RESPONSE, processed in SIMCONNECT_RECV_ID_CLIENT_DATA

		// wait until server connects or times out or disconnectSimulator() is called.
		if (!waitCondition([&]() { return !runDispatchLoop || serverConnected; }, timeout)) {
			LOG_ERR << "Server connection timed out or refused after " << timeout << "ms.";
			setStatus(ClientStatus::SimConnected);
			return E_TIMEOUT;
		}

		LOG_INF << "Connected to " WSMCMND_PROJECT_NAME " server v" << STREAM_HEX8(serverVersion);
		if ((serverVersion & 0xFF000000) != (WSMCMND_VERSION & 0xFF000000))
			LOG_WRN << "Server major version does not match WASimClient version " << STREAM_HEX8(WSMCMND_VERSION);
		setStatus(ClientStatus::Connected);

		// clear any pending list request (unlikely)
		listResult.reset();
		// make sure server knows our desired log level and set up data area/request if needed
		updateServerLogLevel();
		// set update status of data requests before adding any, in case we don't actually want results yet
		sendServerCommand(Command(CommandId::Subscribe, (requestsPaused ? 0 : 1)));
		// (re-)register (or delete) any saved DataRequests
		registerAllDataRequests();
		// same with calculator events
		registerAllEvents();

		return S_OK;
	}

	void disconnectServer(bool notifyServer = true)
	{
		if (!serverConnected || (status & ClientStatus::Connecting) == ClientStatus::Connecting)
			return;

		// send disconnect command
		if (notifyServer)
			sendServerCommand(Command(CommandId::Disconnect));

		// clear command tracking queue
		unique_lock lock(mtxResponses);
		reponses.clear();

		serverConnected = false;
		LOG_INF << "Disconnected from " WSMCMND_PROJECT_NAME " server.";
		setStatus(ClientStatus::SimConnected);
	}

	HRESULT registerDataArea(const string &name, SIMCONNECT_CLIENT_DATA_ID cdaID, SIMCONNECT_CLIENT_DATA_DEFINITION_ID cddId, DWORD szOrType, bool readonly, bool nameIsFull = false, float deltaE = 0.0f)
	{
		// Map a unique named data storage area to CDA ID.
		if (!checkInit())
			return E_NOT_CONNECTED;
		HRESULT hr;
		const string cdaName(nameIsFull ? name : name + clientName);
		if FAILED(hr = SimConnectHelper::registerDataArea(hSim, cdaName, cdaID, cddId, szOrType, true, readonly, deltaE))
			return hr;
		const uint32_t alloc = Utilities::getActualValueSize(szOrType);;
		totalDataAlloc += alloc;
		LOG_DBG << "Created CDA ID " << cdaID << " named " << quoted(cdaName) << " of size " << alloc << "; Total data allocation : " << totalDataAlloc;
		return S_OK;
	}

#pragma endregion
#pragma region  Command sending  ----------------------------------------------

	// Sends a const command as provided.
	HRESULT sendServerCommand(const Command &command) const
	{
		if (!isConnected()) {
			LOG_ERR << "Server not connected, cannot send " << command;
			return E_NOT_CONNECTED;
		}
		LOG_TRC << "Sending command: " << command;
		return INVOKE_SIMCONNECT(SetClientData, hSim,
			(SIMCONNECT_CLIENT_DATA_ID)CLI_DATA_COMMAND, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)CLI_DATA_COMMAND,
			SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL, (DWORD)sizeof(Command), (void *)&command
		);
	}

	// Sends command and enqueues a tracked response record with given condition var. Optionally returns new token.
	HRESULT sendServerCommand(Command &&command, weak_ptr<condition_variable_any> cv, uint32_t *newToken = nullptr)
	{
		if (cv.expired())
			return E_INVALIDARG;
		if (!command.token)
			command.token = nextCmdToken++;
		enqueueTrackedResponse(command.token, cv);
		const HRESULT hr = sendServerCommand(command);
		if FAILED(hr) {
			unique_lock vlock(mtxResponses);
			reponses.erase(command.token);
		}
		if (newToken)
			*newToken = command.token;
		return hr;
	}

	// Sends command and waits for response. Default timeout is settings.networkTimeout.
	HRESULT sendCommandWithResponse(Command &&command, Command *response, uint32_t timeout = 0)
	{
		uint32_t token;
		shared_ptr<condition_variable_any> cv = make_shared<condition_variable_any>();
		const CommandId origId = command.commandId;
		HRESULT hr = sendServerCommand(move(command), weak_ptr(cv), &token);
		if SUCCEEDED(hr) {
			if FAILED(hr = waitCommandResponse(token, response, timeout))
				LOG_ERR << "Command " << Utilities::getEnumName(origId, CommandIdNames) << " timed out after " << (timeout ? timeout : settings.networkTimeout) << "ms.";
		}
		return hr;
	}

#pragma endregion
#pragma region  Command Response tracking  ----------------------------------------------

	TrackedResponse *findTrackedResponse(uint32_t token)
	{
		shared_lock lock{mtxResponses};
		const responseMap_t::iterator pos = reponses.find(token);
		if (pos != reponses.cend())
			return &pos->second;
		return nullptr;
	}

	TrackedResponse *enqueueTrackedResponse(uint32_t token, weak_ptr<condition_variable_any> cv)
	{
		unique_lock lock{mtxResponses};
		return &reponses.try_emplace(token, token, cv).first->second;
	}

	// Blocks and waits for a response to a specific command token. `timeout` can be -1 to use `extraPredicate` only (which is then required).
	HRESULT waitCommandResponse(uint32_t token, Command *response, uint32_t timeout = 0, std::function<bool(void)> extraPredicate = nullptr)
	{
		TrackedResponse *tr = findTrackedResponse(token);
		shared_ptr<condition_variable_any> cv {};
		if (!tr || !(cv = tr->cv.lock()))
			return E_FAIL;

		if (!timeout)
			timeout = settings.networkTimeout;

		bool stopped = false;
		auto stop_waiting = [=, &stopped]() {
			return stopped =
				!serverConnected || (tr->response.commandId != CommandId::None && tr->response.token == token && (!extraPredicate || extraPredicate()));
		};

		HRESULT hr = E_TIMEOUT;
		if (stop_waiting()) {
			hr = S_OK;
		}
		else {
			unique_lock lock(tr->mutex);
			if (timeout > 0) {
				if (cv->wait_for(lock, chrono::milliseconds(timeout), stop_waiting))
					hr = S_OK;
			}
			else if (!!extraPredicate) {
				cv->wait(lock, stop_waiting);
				hr = stopped ? ERROR_ABANDONED_WAIT_0 : S_OK;
			}
			else {
				hr = E_INVALIDARG;
				LOG_DBG << "waitCommandResponse() requires a predicate condition when timeout parameter is < 0.";
			}
		}
		if (SUCCEEDED(hr) && !!response) {
			unique_lock lock(tr->mutex);
			*response = move(tr->response);
		}

		unique_lock vlock(mtxResponses);
		reponses.erase(token);
		return hr;
	}

#if 0  // not used
	// Waits for a response to a specific command token using an async thread.
	HRESULT waitCommandResponseAsync(uint32_t token, Command *response, uint32_t timeout = 0, std::function<bool(void)> extraPredicate = nullptr)
	{
		if (!timeout)
			timeout = settings.networkTimeout;
		future<HRESULT> taskResult = async(launch::async, &Private::waitCommandResponse, this, token, response, timeout, extraPredicate);
		const future_status taskStat = taskResult.wait_for(chrono::milliseconds(timeout * 2));
		if (taskStat == future_status::ready)
			return taskResult.get();
		return E_TIMEOUT;
	}
#endif

	// Waits for completion of list request results, or a timeout. This is used on a new thread.
	void waitListRequestEnd()
	{
		auto stop_waiting = [this]() {
			return listResult.nextTimeout.load() >= Clock::now();
		};

		Command response;
		HRESULT hr = waitCommandResponse(listResult.token, &response, -1, stop_waiting);
		if (hr == ERROR_ABANDONED_WAIT_0) {
			LOG_ERR << "List request timed out.";
			hr = E_TIMEOUT;
		}
		else if (hr != S_OK) {
			LOG_ERR << "List request failed with result: "  << LOG_HR(hr);
		}
		else if (response.commandId != CommandId::Ack) {
			LOG_WRN << "Server returned Nak for list request of " << Utilities::getEnumName(listResult.listType.load(), LookupItemTypeNames);
			hr = E_FAIL;
		}
		if (listCb)
			invokeCallback(listCb, ListResult { listResult.listType.load(), hr, listResult.getResult() });
		listResult.reset();  // reset to none to indicate the current list request is finished
	}

#pragma endregion
#pragma region  Server-side logging commands  ----------------------------------------------

	void setServerLogLevel(LogLevel level, LogFacility fac = LogFacility::Remote) {
		if (fac == LogFacility::None || fac > LogFacility::All)
			return;
		if (+fac & +LogFacility::Remote)
			settings.logLevel(LogSource::Server, LogFacility::Remote) = level;
		if (+fac & +LogFacility::Console)
			settings.logLevel(LogSource::Server, LogFacility::Console) = level;
		if (+fac & +LogFacility::File)
			settings.logLevel(LogSource::Server, LogFacility::File) = level;

		updateServerLogLevel(fac);
	}

	void updateServerLogLevel(LogFacility fac = LogFacility::Remote) {
		if (checkInit() && (!(+fac & +LogFacility::Remote) || SUCCEEDED(registerLogDataArea())))
			sendServerCommand(Command(CommandId::Log, +settings.logLevel(LogSource::Server, fac), nullptr, (double)fac));
	}

	HRESULT registerLogDataArea() {
		if (logCDAcreated || settings.logLevel(LogSource::Server, LogFacility::Remote) == LogLevel::None)
			return S_OK;
		HRESULT hr;
		// register log area for reading; server can write to this channel
		if SUCCEEDED(hr = registerDataArea(CDA_NAME_LOG_PFX, CLI_DATA_LOG, CLI_DATA_LOG, sizeof(LogRecord), false)) {
			// start/stop listening on the log channel
			INVOKE_SIMCONNECT(RequestClientData, hSim, (SIMCONNECT_CLIENT_DATA_ID)CLI_DATA_LOG, (SIMCONNECT_DATA_REQUEST_ID)DATA_REQ_LOG,
												(SIMCONNECT_CLIENT_DATA_DEFINITION_ID)CLI_DATA_LOG, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, 0UL, 0UL, 0UL, 0UL);
		}
		logCDAcreated = SUCCEEDED(hr);
		return hr;
	}

#pragma endregion
#pragma region  Remote Variables accessors  ----------------------------------------------

	const string buildVariableCommandString(const VariableRequest &v, bool forSet) const
	{
		// only L vars can be set by an index value
		const bool isIndexed = forSet ? (v.variableType == 'L') : Utilities::isIndexedVariableType(v.variableType);
		const bool hasUnits = Utilities::isUnitBasedVariableType(v.variableType);
		string sValue = (isIndexed && v.variableId > -1 ? to_string(v.variableId) : v.variableName);
		if (sValue.empty())
			return sValue;
		if (v.variableType == 'A' && v.simVarIndex)
			sValue += ':' + to_string(v.simVarIndex);
		if (hasUnits) {
			if (isIndexed && v.unitId > -1)
				sValue += ',' + to_string(v.unitId);
			else if (!v.unitName.empty())
				sValue += ',' + v.unitName;
		}
		return sValue;
	}

	HRESULT getVariable(const VariableRequest &v, double *result, std::string *sResult = nullptr, double dflt = 0.0)
	{
		const string sValue = buildVariableCommandString(v, false);
		if (sValue.empty() || sValue.length() >= STRSZ_CMD)
			return E_INVALIDARG;

		HRESULT hr;
		Command response;
		if FAILED(hr = sendCommandWithResponse(Command(v.createLVar && v.variableType == 'L' ? CommandId::GetCreate : CommandId::Get, v.variableType, sValue.c_str(), dflt), &response))
			return hr;
		if (response.commandId != CommandId::Ack) {
			LOG_WRN << "Get Variable request for " << quoted(sValue) << " returned Nak response. Reason, if any: " << quoted(response.sData);
			return E_FAIL;
		}
		if (result)
			*result = response.fData;
		if (sResult)
			*sResult = response.sData;
		return S_OK;
	}

	HRESULT setVariable(const VariableRequest &v, const double value)
	{
		if (Utilities::isSettableVariableType(v.variableType)) {
			const string sValue = buildVariableCommandString(v, true);
			if (sValue.empty() || sValue.length() >= STRSZ_CMD)
				return E_INVALIDARG;
			return sendServerCommand(Command(v.createLVar && v.variableType == 'L' ? CommandId::SetCreate : CommandId::Set, v.variableType, sValue.c_str(), value));
		}
		LOG_WRN << "Cannot Set a variable of type '" << v.variableType << "'.";
		return E_INVALIDARG;
	}

#pragma endregion
#pragma region  Data Requests  ----------------------------------------------

	TrackedRequest *findRequest(uint32_t id)
	{
		shared_lock lock{mtxRequests};
		const requestMap_t::iterator pos = requests.find(id);
		if (pos != requests.cend())
			return &pos->second;
		return nullptr;
	}

	const TrackedRequest *findRequest(uint32_t id) const {
		shared_lock lock{mtxRequests};
		const requestMap_t::const_iterator pos = requests.find(id);
		if (pos != requests.cend())
			return &pos->second;
		return nullptr;
	}

	// Writes DataRequest data to the corresponding CDA. If the DataRequest::requestType == None, the request will be deleted by the server.
	HRESULT writeDataRequest(const DataRequest &req)
	{
		if (!isConnected()) {
			LOG_ERR << "Server not connected, cannot submit DataRequest " << req;
			return E_NOT_CONNECTED;
		}
		LOG_DBG << "Sending request: " << req;
		return INVOKE_SIMCONNECT(SetClientData, hSim, (SIMCONNECT_CLIENT_DATA_ID)CLI_DATA_REQUEST, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)CLI_DATA_REQUEST, SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL, (DWORD)sizeof(DataRequest), (void *)&req);
	}

	// Writes DataRequest data to the corresponding CDA and waits for an Ack/Nak from server.
	// If the DataRequest::requestType == None, the request will be deleted by the server and the response wait is skipped.
	HRESULT sendDataRequest(const DataRequest &req, bool async)
	{
		HRESULT hr;
		if FAILED(hr = writeDataRequest(req))
			return hr;

		// check if just deleting an existing request and don't wait around for that response
		if (async || req.requestType == RequestType::None)
			return hr;

		shared_ptr<condition_variable_any> cv = make_shared<condition_variable_any>();
		enqueueTrackedResponse(req.requestId, weak_ptr(cv));
		Command response;
		if FAILED(hr = waitCommandResponse(req.requestId, &response)) {
			LOG_ERR << "Data Request timed out after " << settings.networkTimeout << "ms.";
		}
		else if (response.commandId != CommandId::Ack) {
			hr = E_FAIL;
			LOG_ERR << "Data Request returned Nak response for DataRequest ID " << req.requestId << ". Reason, if any: " << quoted(response.sData);
		}
		return hr;
	}

	HRESULT registerDataRequestArea(const TrackedRequest * const tr, bool isNewRequest, bool dataAllocChanged = false)
	{
		HRESULT hr;
		shared_lock lock{mtxRequests};
		if (isNewRequest || dataAllocChanged) {
			if (isNewRequest) {
				// Create & allocate the data area which will hold result value (server can write to this channel)
				if FAILED(hr = registerDataArea(CDA_NAME_DATA_PFX + clientName + '.' + to_string(tr->requestId), tr->dataId, tr->dataId, tr->valueSize, false, true, max(tr->deltaEpsilon, 0.0f)))
					return hr;
			}
			else if (dataAllocChanged) {
				// remove definition, ignore errors (they will be logged)
				deregisterDataRequestArea(tr);
				// re-add definition, and now do not ignore errors
				if FAILED(hr = SimConnectHelper::addClientDataDefinition(hSim, tr->dataId, tr->valueSize, max(tr->deltaEpsilon, 0.0f)))
					return hr;
			}
		}
		// (re)start listening for result
		return INVOKE_SIMCONNECT(
			RequestClientData, hSim,
		  (SIMCONNECT_CLIENT_DATA_ID)tr->dataId, (SIMCONNECT_DATA_REQUEST_ID)tr->requestId + SIMCONNECTID_LAST,
		  (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)tr->dataId, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, (tr->deltaEpsilon < 0.0f ? 0UL : SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED), 0UL, 0UL, 0UL
		);
	}

	// Note that this method does NOT acquire the requests mutex.
	HRESULT deregisterDataRequestArea(const TrackedRequest * const tr)
	{
		// We need to first suspend updates for this request before removing it, otherwise it seems SimConnect will sometimes crash
		INVOKE_SIMCONNECT(
			RequestClientData, hSim,
			(SIMCONNECT_CLIENT_DATA_ID)tr->dataId, (SIMCONNECT_DATA_REQUEST_ID)tr->requestId + SIMCONNECTID_LAST,
			(SIMCONNECT_CLIENT_DATA_DEFINITION_ID)tr->dataId, SIMCONNECT_CLIENT_DATA_PERIOD_NEVER, 0UL, 0UL, 0UL, 0UL
		);
		// Now we can safely (we hope) remove the SimConnect definition.
		return SimConnectHelper::removeClientDataDefinition(hSim, tr->dataId);
	}

	HRESULT addOrUpdateRequest(const DataRequest &req, bool async)
	{
		if (req.requestType == RequestType::None)
			return removeRequest(req.requestId);

		// preliminary validation
		if (req.nameOrCode[0] == '\0') {
			LOG_ERR << "Error in DataRequest ID: " << req.requestId << "; Parameter 'nameOrCode' cannot be empty.";
			return E_INVALIDARG;
		}

		const uint32_t actualValSize = Utilities::getActualValueSize(req.valueSize);
		if (actualValSize > SIMCONNECT_CLIENTDATA_MAX_SIZE) {
			LOG_ERR << "Error in DataRequest ID: " << req.requestId << "; Value size " << actualValSize << " exceeds SimConnect maximum size " << SIMCONNECT_CLIENTDATA_MAX_SIZE;
			return E_INVALIDARG;
		}
		if (totalDataAlloc + actualValSize > SIMCONNECT_DATA_ALLOC_LIMIT) {
			LOG_ERR << "Error in DataRequest ID: " << req.requestId << "; Adding request with value size " << actualValSize << " would exceed SimConnect total maximum size of " << SIMCONNECT_DATA_ALLOC_LIMIT;
			return E_INVALIDARG;
		}

		TrackedRequest *tr = findRequest(req.requestId);
		const bool isNewRequest = (tr == nullptr);
		bool dataAllocationChanged = false;

		if (isNewRequest) {
			unique_lock lock{mtxRequests};
			requests.try_emplace(req.requestId, req, nextDefId++);
			tr = &requests.at(req.requestId);
		}
		else {
			if (actualValSize > tr->dataSize) {
				LOG_ERR << "Value size cannot be increased after request is created.";
				return E_INVALIDARG;
			}
			// flag if the definition size or the delta E has changed
			dataAllocationChanged = (actualValSize != tr->dataSize || !fuzzyCompare(req.deltaEpsilon, tr->deltaEpsilon));
			// update the tracked request from new request data
			unique_lock lock{mtxRequests};
			*tr = req;
		}

		HRESULT hr = S_OK;
		if (isConnected()) {
			// Register the CDA and subscribe to data value changes
			if (isNewRequest || dataAllocationChanged)
				hr = registerDataRequestArea(tr, isNewRequest, dataAllocationChanged);
			if SUCCEEDED(hr) {
				// send the request and wait for Ack; Request may timeout or return a Nak.
				hr = sendDataRequest(req, async);
			}
			if (FAILED(hr) && isNewRequest) {
				// delete a new request if anything failed
				removeRequest(req.requestId);
			}
			LOG_TRC << (FAILED(hr) ? "FAILED" : isNewRequest ? "Added" : "Updated") << " request: " << req;
		}
		else {
			LOG_TRC << "Queued " << (isNewRequest ? "New" : "Updated") << " request: " << req;
		}

		return hr;
	}

	HRESULT removeRequest(const uint32_t requestId)
	{
		TrackedRequest *tr = findRequest(requestId);
		if (!tr) {
			LOG_WRN << "DataRequest ID " << requestId << " not found.";
			return E_FAIL;
		}

		unique_lock lock{mtxRequests};
		if (isConnected()) {
			if FAILED(deregisterDataRequestArea(tr))
				LOG_WRN << "Failed to clear ClientDataDefinition in SimConnect, check log messages.";
			if FAILED(writeDataRequest(DataRequest(requestId, 0, RequestType::None)))
				LOG_WRN << "Server removal of request " << requestId << " failed or timed out, check log messages.";
		}
		requests.erase(requestId);
		LOG_TRC << "Removed Data Request " << requestId;
		return S_OK;
	}

	// this (re)registers all saved data requests with the server (not SimConnect), or deletes any requests flagged for deletion while offline.
	// called from connectServer()
	void registerAllDataRequests()
	{
		if (!isConnected())
			return;
		shared_lock lock(mtxRequests);
		for (const auto & [_, tr] : requests) {
			if (tr.requestType != RequestType::None)
				writeDataRequest(tr);
		}
	}

	// this only creates the SimConnect data definitions, not the server-side entries
	// called from connectSimulator()
	void registerAllDataRequestAreas()
	{
		if (!checkInit())
			return;
		for (const auto & [_, tr]  : requests)
			registerDataRequestArea(&tr, true);
	}

#pragma endregion Data Requests

#pragma region  Calculator Events --------------------------------------------

	TrackedEvent *findTrackedEvent(uint32_t eventId)
	{
		shared_lock lock{mtxEvents};
		const eventMap_t::iterator pos = events.find(eventId);
		if (pos != events.cend())
			return &pos->second;
		return nullptr;
	}

	// sends event registration to server. If event is being deleted and sending succeeds, event is removed from tracked events list.
	HRESULT sendEventRegistration(TrackedEvent *ev, bool resendName = false)
	{
		if (!ev)
			return E_INVALIDARG;
		HRESULT hr;
		if (ev->code.empty()) { // delete event
			 hr = sendServerCommand(Command(CommandId::Register, ev->eventId));
			 if SUCCEEDED(hr) {
				 unique_lock lock(mtxEvents);
				 events.erase(ev->eventId);
			 }
			 return hr;
		}

		const string evStr = (!ev->sentToServer || resendName) && !ev->name.empty() ? ev->name + '$' + ev->code : ev->code;
		return sendServerCommand(Command(CommandId::Register, ev->eventId, evStr.c_str()));
	}

	HRESULT sendEventRegistration(uint32_t eventId) {
		return sendEventRegistration(findTrackedEvent(eventId));
	}

	// called from connectSimulator()
	void registerAllCustomEvents()
	{
		unique_lock lock(mtxCustomEvents);
		customEventIdCache.clear();
		for (auto& [name, id] : customEventNameCache) {
			HRESULT hr;
			if FAILED(hr = INVOKE_SIMCONNECT(MapClientEventToSimEvent, hSim, (SIMCONNECT_CLIENT_EVENT_ID)id, name.c_str()))
				LOG_ERR << "registerAllCustomEvents: Custom Event name " << quoted(name) << " with id " << id << " failed";
			else {
				LOG_INF << "registerAllCustomEvents: Custom Event name " << quoted(name) << " with id " << id << " succeeded";
				customEventIdCache.insert(std::pair{ id, name });
			}
		}
	}

	// called from connectServer()
	void registerAllEvents()
	{
		vector<uint32_t> removals {};
		shared_lock lock(mtxEvents);
		for (auto & [key, ev] : events) {
			if (ev.code.empty())
				removals.push_back(key);
			else
				sendEventRegistration(&ev, true);
		}
		lock.unlock();
		for (const uint32_t &id : removals)
			sendEventRegistration(id);
	}

#pragma endregion

#pragma region Simulator Custom Events -------------------------------------------

	// Sends Custom Event using TransmitClientEvent(_EX1)
	HRESULT writeCustomEvent(uint32_t keyEventId, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5)
	{
		if (!simConnected)
		{
			LOG_ERR << "writeCustomEvent: Simulator not connected, cannot send Simulator Custom Event.";
			return E_NOT_CONNECTED;
		}
		const Private::customEventIdCache_t::iterator pos = customEventIdCache.find(keyEventId);
		if (pos == customEventIdCache.cend())
		{
			LOG_ERR << "writeCustomEvent: customEvent " << keyEventId << " has not successfully been registered.";
			return E_FAIL;
		}
		LOG_DBG << "writeCustomEvent: Sending Simulator Custom Event: EventId: " << keyEventId << "; v1: " << v1 << "; v2: " << v2 << "; v3: " << v3 << "; v4: " << v4 << "; v5: " << v5;
		return INVOKE_SIMCONNECT(
			TransmitClientEvent_EX1,
			hSim,
			(DWORD)0,
			(SIMCONNECT_CLIENT_EVENT_ID)keyEventId,
			SIMCONNECT_GROUP_PRIORITY_HIGHEST,
			SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY,
			(DWORD)v1,
			(DWORD)v2,
			(DWORD)v3,
			(DWORD)v4,
			(DWORD)v5);
	}

#pragma endregion

#pragma region Simulator Key Events -------------------------------------------

	// Writes KeyEvent data to the corresponding CDA.
	HRESULT writeKeyEvent(const KeyEvent &kev)
	{
		if (!isConnected()) {
			LOG_ERR << "Server not connected, cannot send Simulator Key Event " << kev;
			return E_NOT_CONNECTED;
		}
		LOG_DBG << "Sending Simulator Key Event: " << kev;
		return INVOKE_SIMCONNECT(SetClientData, hSim, (SIMCONNECT_CLIENT_DATA_ID)CLI_DATA_KEYEVENT, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)CLI_DATA_KEYEVENT, SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0UL, (DWORD)sizeof(KeyEvent), (void *)&kev);
	}

#pragma endregion

#pragma region  SimConnect message processing  ----------------------------------------------

	static void CALLBACK dispatchMessage(SIMCONNECT_RECV *pData, DWORD cbData, void *pContext) {
		static_cast<Private*>(pContext)->onSimConnectMessage(pData, cbData);
	}

	void dispatchLoop()
	{
		LOG_DBG << "Dispatch loop started.";
		const HANDLE waitEvents[] = { hDispatchStopEvent, hSimEvent };
		DWORD hr;
		runDispatchLoop = true;
		while (runDispatchLoop) {
			hr = WaitForMultipleObjects(2, waitEvents, false, DISPATCH_LOOP_WAIT_TIME);
			switch (hr) {
				case WAIT_TIMEOUT:
					continue;
				case WAIT_OBJECT_0 + 1:  // hSimEvent
					SimConnect_CallDispatch(hSim, Private::dispatchMessage, this);
					continue;
				case WAIT_OBJECT_0:  // hDispatchStopEvent
					break;
				default:
					LOG_ERR << "Dispatch loop WaitForMultipleObjects returned: " << LOG_HR(hr) << " Error: " << LOG_HR(GetLastError());
					break;
			}
			runDispatchLoop = false;
		}
		LOG_DBG << "Dispatch loop stopped.";
	}

	void onSimConnectMessage(SIMCONNECT_RECV *pData, DWORD cbData)
	{
		LOG_TRC << LOG_SC_RECV(pData);
		switch (pData->dwID) {

			case SIMCONNECT_RECV_ID_CLIENT_DATA: {
				SIMCONNECT_RECV_CLIENT_DATA* data = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
				LOG_TRC << LOG_SC_RCV_CLIENT_DATA(data);
				// dwSize always under-reports by 4 bytes when sizeof(SIMCONNECT_RECV_CLIENT_DATA) is subtracted, and the minimum reported size is 4 bytes even for 0-3 bytes of actual data.
				const size_t dataSize = (size_t)pData->dwSize + 4 - sizeof(SIMCONNECT_RECV_CLIENT_DATA);
				switch (data->dwRequestID)
				{
					case DATA_REQ_RESPONSE: {
						// be paranoid
						if (dataSize != sizeof(Command)) {
							LOG_CRT << "Invalid Command struct data size! Expected " << sizeof(Command) << " but got " << dataSize;
							return;
						}
						// dwData is our command struct
						const Command *const cmd = reinterpret_cast<const Command *const>(&data->dwData);
						LOG_DBG << "Got Command: " << *cmd;
						bool checkTracking = true;
						switch (cmd->commandId)
						{
							case CommandId::Ack:
							case CommandId::Nak: {
								// which command is this ack/nak for?
								switch ((CommandId)cmd->uData) {
									// Connected response
									case CommandId::Connect:
										serverConnected = cmd->commandId == CommandId::Ack && cmd->token == clientId;
										serverVersion = (uint32_t)cmd->fData;
										checkTracking = false;
										break;
									default:
										break;
								}
								// invoke result callback if anyone is listening
								invokeCallback(cmdResultCb, *cmd);
								break;
							}

							// respond to pings to stay alive
							case CommandId::Ping:
								sendServerCommand(Command(CommandId::Ack, (uint32_t)CommandId::Ping));
								checkTracking = false;
								break;

							// incoming list results, one at a time until Ack of List command.
							case CommandId::List: {
								unique_lock lock(listResult.mutex);
								if (cmd->token == listResult.token) {
									listResult.nextTimeout = Clock::now() + chrono::milliseconds(settings.networkTimeout);
									listResult.result.emplace_back((int)cmd->uData, cmd->sData);
								}
								else {
									LOG_WRN << "Received unexpected list result for wrong command token. Expected " << listResult.token << " got " << cmd->token;
								}
								checkTracking = false;
								break;
							}

							// Server is disconnecting (shutting down/etc).
							case CommandId::Disconnect:
								disconnectServer(false);
								checkTracking = false;
								break;

							default:
								break;
						}  // commandId

						// Check if this command response is tracked and possibly awaited.
						if (checkTracking) {
							if (TrackedResponse *tr = findTrackedResponse(cmd->token)) {
								if (auto cv = tr->cv.lock()) {
									LOG_TRC << "Got awaited command response, notifying CV.";
									//unique_lock lock(mtxResponses);
									unique_lock lock(tr->mutex);
									tr->response = *cmd;
									lock.unlock();
									cv->notify_all();
								}
								else {
									unique_lock lock(mtxResponses);
									reponses.erase(cmd->token);
									LOG_TRC << "Got tracked command response but no CV, removing record.";
								}
							}
						}
						// invoke response callback if anyone is listening
						invokeCallback(respCb, *cmd);
						break;
					}  // DATA_REQ_RESPONSE

					case DATA_REQ_LOG: {
						// be paranoid
						if (dataSize != sizeof(LogRecord)) {
							LOG_CRT << "Invalid LogRecord struct data size! Expected " << sizeof(LogRecord) << " but got " << dataSize;
							return;
						}
						// dwData is our log record struct
						const LogRecord *const log = reinterpret_cast<const LogRecord *const>(&data->dwData);
						LOG_TRC << "Got Log Record: " << *log;
						invokeCallback(logCb, *log, LogSource::Server);
						break;
					}  // DATA_REQ_LOG

					// possible request data
					default:
						if (data->dwRequestID >= SIMCONNECTID_LAST) {
							TrackedRequest *tr = findRequest(data->dwRequestID - SIMCONNECTID_LAST);
							if (!tr){
								LOG_WRN << "DataRequest ID " << data->dwRequestID - SIMCONNECTID_LAST << " not found in tracked requests.";
								return;
							}
							// be paranoid; note that the reported pData->dwSize is never less than 4 bytes.
							if (dataSize < tr->dataSize) {
								LOG_CRT << "Invalid data result size! Expected " << tr->dataSize << " but got " << dataSize;
								return;
							}
							unique_lock datalock(tr->m_dataMutex);
							memcpy(tr->data.data(), (void*)&data->dwData, tr->dataSize);
							tr->lastUpdate = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
							datalock.unlock();
							shared_lock rdlock(mtxRequests);
							LOG_TRC << "Got data result for request: " << *tr;
							invokeCallback(dataCb, tr->toRequestRecord());
							break;
						}
						LOG_WRN << "Got unknown RequestID in SIMCONNECT_RECV_CLIENT_DATA struct: " << data->dwRequestID;
						return;
				}  // data->dwRequestID

				//serverLastSeen = Clock::now();
				break;
			}  // SIMCONNECT_RECV_ID_CLIENT_DATA

			case SIMCONNECT_RECV_ID_EVENT: {
				SIMCONNECT_RECV_EVENT *data = static_cast<SIMCONNECT_RECV_EVENT *>(pData);
				LOG_TRC << LOG_SC_RECV_EVENT(data);

				switch (data->uEventID) {
					case CLI_EVENT_PING_RESP:
						serverVersion = data->dwData;
						serverLastSeen = Clock::now();
						LOG_DBG << "Got ping response at " << Utilities::timePointToString(serverLastSeen.load()) << " with version " << STREAM_HEX8(serverVersion);
						break;

					default:
						return;
				}
				break;
			}  // SIMCONNECT_RECV_ID_EVENT

			case SIMCONNECT_RECV_ID_OPEN:
				simConnected = true;
				SimConnectHelper::logSimVersion(pData);
				break;

			case SIMCONNECT_RECV_ID_QUIT: {
				LOG_INF << "Received quit command from SimConnect, disconnecting...";
				// we're being called from within the message dispatch loop thread; to avoid deadlock we must disconnect asynchronously.
				thread([=]() { disconnectSimulator(true); }).detach();
				break;
			}

			case SIMCONNECT_RECV_ID_EXCEPTION:
				SimConnectHelper::logSimConnectException(pData);
				break;

			default:
				break;
		}
	}  // onSimConnectMessage()

#pragma endregion SimConnect

};
#pragma endregion WASimClient::Private


// -------------------------------------------------------------
#pragma region WASimClient class
// -------------------------------------------------------------


#define d_const  const_cast<const Private *>(d.get())

WASimClient::WASimClient(uint32_t clientId, const std::string &configFile) :
	d(new Private(this, clientId, configFile))
{ }

WASimClient::~WASimClient() {
	if (isInitialized())
		disconnectSimulator();
	if (d->hDispatchStopEvent)
		CloseHandle(d->hDispatchStopEvent);
}

#pragma region Connections ----------------------------------------------

HRESULT WASimClient::connectSimulator(uint32_t timeout)
{
	if (!d->clientId) {
		LOG_ERR << "Client ID must be greater than zero.";
		return E_INVALIDARG;
	}
	return d->connectSimulator(timeout);
}

HRESULT WASimClient::connectSimulator(int networkConfigId, uint32_t timeout) {
	return d->connectSimulator(networkConfigId, timeout);
}

void WASimClient::disconnectSimulator() {
	d->disconnectSimulator(false);
}

HRESULT WASimClient::connectServer(uint32_t timeout) {
	return d->connectServer(timeout);
}

void WASimClient::disconnectServer() {
	d->disconnectServer(true);
}

uint32_t WASimClient::pingServer(uint32_t timeout)
{
	if (!d->checkInit() && FAILED(d->connectSimulator(timeout)))
		return 0;

	// save the last time we saw the server
	const Clock::time_point refTp = d->serverLastSeen;
	// Send the previously-registered PING event with this client's ID as the data parameter.
	if FAILED(INVOKE_SIMCONNECT(
		TransmitClientEvent, d->hSim, SIMCONNECT_OBJECT_ID_USER, (SIMCONNECT_CLIENT_EVENT_ID)Private::CLI_EVENT_PING, (DWORD)d->clientId, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY
	))
		return 0;

	if (!timeout)
		timeout = defaultTimeout();
	// The ping response (if any) should come back via the CLI_EVENT_PING_RESP event which we've subscribed to (SIMCONNECT_RECV_ID_EVENT).
	if (d->waitCondition([&]() { return !d->runDispatchLoop || d->serverLastSeen.load() > refTp; }, timeout)) {
		LOG_INF << "Server responded to ping, reported version: " << STREAM_HEX8(d->serverVersion);
		return d->serverVersion;
	}
	LOG_WRN << "Server did not respond to ping after " << timeout << "ms";
	return 0;
}

#pragma endregion

#pragma region Exec calc ----------------------------------------------

HRESULT WASimClient::executeCalculatorCode(const std::string &code, CalcResultType resultType, double *fResult, std::string *sResult) const
{
	if (code.length() >= STRSZ_CMD) {
		LOG_ERR << "Code string length " << code.length() << " is greater then maximum size of " << STRSZ_CMD-1;
		return E_INVALIDARG;
	}

	if (resultType == CalcResultType::None)
		return d->sendServerCommand(Command(CommandId::Exec, +resultType, code.c_str()));

	HRESULT hr;
	Command response;
	if FAILED(hr = d->sendCommandWithResponse(Command(CommandId::Exec, +resultType, code.c_str()), &response))
		return hr;
	if (response.commandId != CommandId::Ack) {
		LOG_WRN << "Calculator Exec with code " << quoted(code) << " returned Nak response. Reason, if any: " << quoted(response.sData);
		return E_FAIL;
	}
	if (fResult)
		*fResult = response.fData;
	if (sResult)
		*sResult = response.sData;
	return hr;
}

#pragma endregion

#pragma region Variable accessors ----------------------------------------------

HRESULT WASimClient::getVariable(const VariableRequest & variable, double * pfResult, std::string *psResult)
{
	if (variable.variableId > -1 && !Utilities::isIndexedVariableType(variable.variableType)) {
		LOG_ERR << "Cannot get variable type '" << variable.variableType << "' by index.";
		return E_INVALIDARG;
	}
	return d->getVariable(variable, pfResult, psResult);
}

HRESULT WASimClient::getLocalVariable(const std::string &variableName, double *pfResult, const std::string &unitName) {
	return d->getVariable(VariableRequest(variableName, false, unitName), pfResult);
}

HRESULT WASimClient::getOrCreateLocalVariable(const std::string &variableName, double *pfResult, double defaultValue, const std::string &unitName) {
	return d->getVariable(VariableRequest(variableName, true, unitName), pfResult, nullptr, defaultValue);
}

HRESULT WASimClient::setVariable(const VariableRequest & variable, const double value) {
	return d->setVariable(variable, value);
}

HRESULT WASimClient::setLocalVariable(const std::string &variableName, const double value, const std::string &unitName) {
	return d->setVariable(VariableRequest(variableName, false, unitName), value);
}

HRESULT WASimClient::setOrCreateLocalVariable(const std::string &variableName, const double value, const std::string &unitName) {
	return d->setVariable(VariableRequest(variableName, true, unitName), value);
}

#pragma endregion

#pragma region Data Requests ----------------------------------------------

HRESULT WASimClient::saveDataRequest(const DataRequest &request, bool async) {
	return d->addOrUpdateRequest(request, async);
}

HRESULT WASimClient::removeDataRequest(const uint32_t requestId) {
	return d->removeRequest(requestId);
}

HRESULT WASimClient::updateDataRequest(uint32_t requestId)
{
	if (d->requests.count(requestId))
		return d->sendServerCommand(Command(CommandId::Update, requestId));
	return E_FAIL;
}

DataRequestRecord WASimClient::dataRequest(uint32_t requestId) const
{
	static const DataRequestRecord nullRecord{ DataRequest((uint32_t)-1, 0, RequestType::None) };

	const Private::TrackedRequest *r = d_const->findRequest(requestId);
	if (r)
		return r->toRequestRecord();
	LOG_ERR << "Data Request ID " << requestId << " not found.";
	return nullRecord;
}

vector<DataRequestRecord> WASimClient::dataRequests() const
{
	vector<DataRequestRecord> ret;
	shared_lock lock(d_const->mtxRequests);
	ret.reserve(d_const->requests.size());
	for (const auto & [_, value] : d_const->requests) {
		if (value.requestType != RequestType::None)
			ret.emplace_back(value.toRequestRecord());
	}
	return ret;
}

vector<uint32_t> WASimClient::dataRequestIdsList() const
{
	vector<uint32_t> ret;
	shared_lock lock(d_const->mtxRequests);
	ret.reserve(d_const->requests.size());
	for (const auto & [key, value] : d_const->requests) {
		if (value.requestType != RequestType::None)
			ret.push_back(key);
	}
	return ret;
}

HRESULT WASimClient::setDataRequestsPaused(bool paused) const {
	if (isConnected()) {
		HRESULT hr = d_const->sendServerCommand(Command(CommandId::Subscribe, (paused ? 0 : 1)));
		if SUCCEEDED(hr)
			d->requestsPaused = paused;
		return hr;
	}
	d->requestsPaused = paused;
	return S_OK;
}

#pragma endregion Data

#pragma region Calculator Events ----------------------------------------------

HRESULT WASimClient::registerEvent(const RegisteredEvent &eventData)
{
	if (eventData.code.empty())
		return removeEvent(eventData.eventId);

	const size_t len = eventData.code.length() + (eventData.name.empty() ? 0 : eventData.name.length() + 1);
	if (len >= STRSZ_CMD) {
		LOG_ERR << "Resulting command string length " << len << " is greater then maximum size of " << STRSZ_CMD-1 << " bytes.";
		return E_INVALIDARG;
	}

	Private::TrackedEvent *ev = d->findTrackedEvent(eventData.eventId);
	if (ev) {
		if (!eventData.name.empty() && ev->name != eventData.name) {
			LOG_ERR << "Cannot change name of event ID " << eventData.eventId << " after it has been registered.";
			return E_INVALIDARG;
		}
		if (ev->code == eventData.code) {
			LOG_INF << "No changes detected in code for event ID " << eventData.eventId << ", skipping update";
			return S_OK;
		}
		ev->code = eventData.code;
	}
	else {
		unique_lock lock(d->mtxEvents);
		ev = &d->events.emplace(eventData.eventId, Private::TrackedEvent(eventData.eventId, eventData.code, eventData.name)).first->second;
	}
	if (isConnected())
		ev->sentToServer = SUCCEEDED(d->sendEventRegistration(ev)) || ev->sentToServer;
	else
		LOG_DBG << "Queued event ID " << eventData.eventId << " for next server connection.";

	return S_OK;
}

HRESULT WASimClient::removeEvent(uint32_t eventId)
{
	Private::TrackedEvent *ev = d->findTrackedEvent(eventId);
	if (!ev) {
		LOG_ERR << "Event ID " << eventId << " not found.";
		return E_INVALIDARG;
	}
	ev->code.clear();
	if (!ev->sentToServer) {
		unique_lock lock(d->mtxEvents);
		d->events.erase(eventId);
		return S_OK;
	}
	if (isConnected())
		d->sendEventRegistration(ev);
	else
		LOG_DBG << "Queued event ID " << eventId << " for deletion on next server connection.";
	return S_OK;
}

HRESULT WASimClient::transmitEvent(uint32_t eventId) {
	return d->sendServerCommand(Command(CommandId::Transmit, eventId));
}

RegisteredEvent WASimClient::registeredEvent(uint32_t eventId)
{
	static const RegisteredEvent nullRecord { };
	if (Private::TrackedEvent *ev = d->findTrackedEvent(eventId))
		return RegisteredEvent(*ev);
	LOG_ERR << "Event ID " << eventId << " not found.";
	return nullRecord;
}

vector<RegisteredEvent> WASimClient::registeredEvents() const
{
	vector<RegisteredEvent> ret;
	shared_lock lock(d_const->mtxEvents);
	ret.reserve(d_const->events.size());
	for (const auto & [_, value] : d_const->events) {
		if (!value.code.empty())
			ret.emplace_back(RegisteredEvent(value));
	}
	return ret;
}

#pragma endregion Calculator Events

#pragma region Custom Events ----------------------------------------------

HRESULT WASimClient::registerCustomEvent(const std::string& customEventName, uint32_t* puiCustomEventId)
{
	if (customEventName[0] == '\0') {
		LOG_ERR << "registerCustomEvent: Parameter 'customEventName' cannot be empty.";
		return E_INVALIDARG;
	}

	if (customEventName.find('.') == string::npos) {
		LOG_ERR << "registerCustomEvent: " << quoted(customEventName) << " is not a Custom Event (doen't include '.').";
		return E_INVALIDARG;
	}

	uint32_t keyId;

	// Protect the below code with a unique_lock to guarantee correct behavior when 2 threads register the same Custom Event at the same time (unlikely to happen)
	// If not using a unique_lock, both threads might not find the Custom Event in the customEventNameCache yet, and each register the (same) Custom Event
	unique_lock lock(d->mtxCustomEvents);
	const Private::customEventNameCache_t::iterator pos = d->customEventNameCache.find(customEventName);
	if (pos != d->customEventNameCache.cend())
		// Custom Event already exists - return the eventId if required
		keyId = pos->second;
	else {
		// get a new eventId
		if (d->nextCustomEventID == THIRD_PARTY_EVENT_ID_MAX) {
			LOG_ERR << "registerCustomEvent: customEventID exceeds THIRD_PARTY_EVENT_ID_MAX.";
			return E_FAIL;
		}
		keyId = d->nextCustomEventID++;

		// Try to register the new Custom Event
		if (d->simConnected) {
			HRESULT hr;
			if FAILED(hr = INVOKE_SIMCONNECT(MapClientEventToSimEvent, d->hSim, (SIMCONNECT_CLIENT_EVENT_ID)keyId, customEventName.c_str()))
				return hr;
			else
				// Successful registration, add customEventId in the customEventIdCache
				d->customEventIdCache.insert(std::pair{ keyId, customEventName });
		}
		else
			LOG_DBG << "registerCustomEvent: customEventName " << quoted(customEventName) << " with customEventId " << keyId << " is pending.";

		// Even if not connected, we add the Custom Event in the customEventNameCache
		// When re-connecting, customEventNameCache is the (only) cache used to register Custom Events later when re-connecting
		d->customEventNameCache.insert(std::pair{ customEventName, keyId });
	}

	if (puiCustomEventId)
		*puiCustomEventId = keyId;

	return S_OK;
}

HRESULT WASimClient::removeCustomEvent(uint32_t eventId)
{
	// We need a unique_lock, because more then 1 thread might remove the same Custom Event at the same time
	unique_lock lock(d->mtxCustomEvents);
	const Private::customEventIdCache_t::iterator pos = d->customEventIdCache.find(eventId);
	if (pos == d->customEventIdCache.cend()) {
		LOG_ERR << "registerCustomEvent: customEventID " << eventId << " doesn't exist.";
		return E_INVALIDARG;
	}
	else
	{
		d->customEventNameCache.erase(pos->second);
		d->customEventIdCache.erase(pos->first);
	}
	return S_OK;
}

HRESULT WASimClient::removeCustomEvent(const std::string& customEventName)
{
	// We need a unique_lock, because more then 1 thread might remove the same Custom Event at the same time
	unique_lock lock(d->mtxCustomEvents);
	const Private::customEventNameCache_t::iterator pos = d->customEventNameCache.find(customEventName);
	if (pos == d->customEventNameCache.cend()) {
		LOG_ERR << "registerCustomEvent: customEventName " << customEventName << " doesn't exist.";
		return E_INVALIDARG;
	}
	else
	{
		d->customEventIdCache.erase(pos->second);
		d->customEventNameCache.erase(pos->first);
	}
	return S_OK;
}

#pragma endregion Custom Events

#pragma region Key Events ----------------------------------------------

HRESULT WASimClient::sendKeyEvent(uint32_t keyEventId, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5) const
{
	if (keyEventId >= THIRD_PARTY_EVENT_ID_MIN)
		// we're dealing with a Simulator Key Event
		return d->writeCustomEvent(keyEventId, v1, v2, v3, v4, v5);
	else
		// we're dealing with a Simulator Key Event
		return d->writeKeyEvent(KeyEvent(keyEventId, { v1, v2, v3, v4, v5 }, d->nextCmdToken++));
}

HRESULT WASimClient::sendKeyEvent(const std::string & keyEventName, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5)
{
	// check the customEventNameCache
	Private::customEventNameCache_t::iterator posCE = d->customEventNameCache.find(keyEventName);
	if (posCE != d->customEventNameCache.cend())
		return sendKeyEvent(posCE->second, v1, v2, v3, v4, v5);

	// check the keyEventNameCache
	int32_t keyId;
	Private::eventNameCache_t::iterator posKE = d->keyEventNameCache.find(keyEventName);
	if (posKE != d->keyEventNameCache.cend()) {
		keyId = posKE->second;
	}
	else {
		// try a lookup
		HRESULT hr;
		if ((hr = lookup(LookupItemType::KeyEventId, keyEventName, &keyId)) != S_OK)
			return hr == E_FAIL ? E_INVALIDARG : hr;
		d->keyEventNameCache.insert(std::pair{keyEventName, keyId});
	}
	return sendKeyEvent((uint32_t)keyId, v1, v2, v3, v4, v5);
}

#pragma endregion Key Events

#pragma region Meta Data ----------------------------------------------

HRESULT WASimClient::list(LookupItemType itemsType)
{
	// validation
	static const set<LookupItemType> validListLookups { LookupItemType::LocalVariable, LookupItemType::DataRequest, LookupItemType::RegisteredEvent };
	if (!validListLookups.count(itemsType)) {
		LOG_ERR << "Cannot list item of type " << Utilities::getEnumName(itemsType, LookupItemTypeNames) << ".";
		return E_INVALIDARG;
	}
	// TODO: Allow multiple simultaneous list requests for different item types
	if (d->listResult.listType != LookupItemType::None) {
		LOG_ERR << "A List request for " << Utilities::getEnumName(d->listResult.listType.load(), LookupItemTypeNames) << " is currently pending.";
		return E_INVALIDARG;
	}
	unique_lock lock(d->listResult.mutex);
	d->listResult.listType = itemsType;
	d->listResult.nextTimeout = Clock::now() + chrono::milliseconds(defaultTimeout());
	HRESULT hr;
	if FAILED(hr = d->sendServerCommand(Command(CommandId::List, +itemsType), weak_ptr(d->listResult.cv), &d->listResult.token))
		return hr;
	lock.unlock();
	try {
		thread(&Private::waitListRequestEnd, d.get()).detach();
	}
	catch (exception *e) {
		LOG_ERR << "Exception trying to schedule thread for List command: " << e->what();
		return E_FAIL;
	}
	return S_OK;
}

HRESULT WASimClient::lookup(LookupItemType itemType, const std::string &itemName, int32_t *piResult)
{
	if (itemName.length() >= STRSZ_CMD) {
		LOG_ERR << "Item name length " << itemName.length() << " is greater then maximum size of " << STRSZ_CMD-1 << " bytes.";
		return E_INVALIDARG;
	}
	HRESULT hr;
	Command response;
	if FAILED(hr = d->sendCommandWithResponse(Command(CommandId::Lookup, +itemType, itemName.c_str()), &response))
		return hr;
	if (response.commandId != CommandId::Ack) {
		LOG_WRN << "Lookup command returned Nak response. Reason, if any: " << quoted(response.sData);
		return E_FAIL;
	}
	LOG_DBG << "Lookup: Server returned ID " << (int)response.fData << " and echo name " << quoted(response.sData);
	if (piResult)
		*piResult = (int)response.fData;
	return hr;
}

#pragma endregion Meta

#pragma region Low Level ----------------------------------------------

HRESULT WASimClient::sendCommand(const Command &command) const {
	return d->sendServerCommand(command);
}

HRESULT WASimClient::sendCommandWithResponse(const Command &command, Command *response, uint32_t timeout) {
	return d->sendCommandWithResponse(Command(command), response, timeout);
}

#pragma endregion Low Level

#pragma region Status / Network / Logging / Callbacks ----------------------------------------------

ClientStatus WASimClient::status() const { return d_const->status; }
bool WASimClient::isInitialized()  const { return d_const->checkInit(); }
bool WASimClient::isConnected()    const { return d_const->isConnected(); }
uint32_t WASimClient::clientVersion() const { return WSMCMND_VERSION; }
uint32_t WASimClient::serverVersion() const { return d_const->serverVersion; }

uint32_t WASimClient::defaultTimeout() const { return d_const->settings.networkTimeout; }
void WASimClient::setDefaultTimeout(uint32_t ms) { d->settings.networkTimeout = ms; }

int WASimClient::networkConfigurationId() const { return d_const->settings.networkConfigId; }
void WASimClient::setNetworkConfigurationId(int configId) { d->settings.networkConfigId = configId; }

LogLevel WASimClient::logLevel(LogFacility facility, LogSource source) const { return d_const->settings.logLevel(source, facility); }
void WASimClient::setLogLevel(LogLevel level, LogFacility facility, LogSource source)
{
	switch (source) {
		case LogSource::Client:
			if ((+facility & +LogFacility::Console) && d_const->settings.logLevel(source, LogFacility::Console) != level)
				d->setConsoleLogLevel(level);
			if ((+facility & +LogFacility::File) && d_const->settings.logLevel(source, LogFacility::File) != level)
				d->setFileLogLevel(level);
			if ((+facility & +LogFacility::Remote) && d_const->settings.logLevel(source, LogFacility::Remote) != level)
				d->setCallbackLogLevel(level);
			break;
		case LogSource::Server:
			d->setServerLogLevel(level, facility);
	}
}

void WASimClient::setClientEventCallback(clientEventCallback_t cb) { d->eventCb = cb; }
void WASimClient::setListResultsCallback(listResultsCallback_t cb) { d->listCb = cb; }
void WASimClient::setDataCallback(dataCallback_t cb) { d->dataCb = cb; }
void WASimClient::setLogCallback(logCallback_t cb) { d->setLogCallback(cb); }
void WASimClient::setCommandResultCallback(commandCallback_t cb) { d->cmdResultCb = cb; }
void WASimClient::setResponseCallback(commandCallback_t cb) { d->respCb = cb; }

#pragma endregion Misc

#pragma endregion WASimClient

#undef d_const
#undef WAIT_CONDITION

};  // namespace WASimCommander::Client
