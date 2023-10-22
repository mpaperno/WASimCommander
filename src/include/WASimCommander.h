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
*/

#pragma once
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <ostream>

#include "global.h"
#include "wasim_version.h"
#include "enums.h"

/// \file

//----------------------------------------------------------------------------
//        Macros
//----------------------------------------------------------------------------

// These macros are for building event and data area names for registration with SimConnect. They could be used for custom client implementations.
#define WSMCMND_COMMON_NAME_PREFIX  WSMCMND_PROJECT_NAME "."   ///< common prefix for all event and data area names
#define WSMCMND_EVENT_NAME_CONNECT  "Connect"    ///< Initial connection event for all clients: "WASimCommander.Connect"
#define WSMCMND_EVENT_NAME_PING     "Ping"       ///< Ping event for all clients ("WASimCommander.Ping") and prefix for response event ("WASimCommander.Ping.<client_name>")
#define WSMCMND_CDA_NAME_COMMAND    "Command"    ///< Data area name prefix for `Command` data sent to Server: "WASimCommander.Command.<client_name>"
#define WSMCMND_CDA_NAME_RESPONSE   "Response"   ///< Data area name prefix for `Command` data sent to Client: "WASimCommander.Response.<client_name>"
#define WSMCMND_CDA_NAME_DATA       "Data"       ///< Data area name prefix for `DataRequest` data sent to Server ("WASimCommander.Data.<client_name>") and data value updates sent to Client: "WASimCommander.Data.<client_name>.<request_id>"
#define WSMCMND_CDA_NAME_KEYEVENT   "KeyEvent"   ///< Data area name prefix for `KeyEvent` data sent to Client: "WASimCommander.KeyEvent.<client_name>"  \since v1.1.0
#define WSMCMND_CDA_NAME_LOG        "Log"        ///< Data area name prefix for `LogRecord` data sent to Client: "WASimCommander.Log.<client_name>"

/// WASimCommander main namespace. Defines constants and structs used in Client-Server interactions. Many of these are needed for effective use of `WASimClient`,
/// and all would be useful for custom client implementations.
namespace WASimCommander
{
//----------------------------------------------------------------------------
//        Constants
//----------------------------------------------------------------------------

	/// \name Char array string size limits, including null terminator.
	/// \{
	static const size_t STRSZ_CMD   = 527;   ///< Maximum size of \refwc{Command::sData} member. Size optimizes alignment of `Command` struct.
	static const size_t STRSZ_REQ   = 1030;  ///< Maximum size for request calculator string or variable name. Size optimizes alignment of `DataRequest` struct. \sa \refwc{DataRequest::nameOrCode}
	static const size_t STRSZ_UNIT  = 37;    ///< Maximum Unit name size. Size is of longest known unit name + 1. \sa \refwc{DataRequest::unitName}
	static const size_t STRSZ_LOG   = 1031;  ///< Size of log entry message in \refwc{LogRecord::message}. Size optimizes alignment of `LogRecord` struct.
	static const size_t STRSZ_ENAME = 64;    ///< Maximum size of custom event name in \refwce{CommandId::Register} command.
	/// \}

	/// \name Time periods
	/// \{
	static const time_t TICK_PERIOD_MS   = 25;       ///< Minimum update period for data Requests in milliseconds. Also dictates rate of some other client-specific processing on server (WASM module) side. 25ms = 40Hz.
	static const time_t CONN_TIMEOUT_SEC = 60 * 10;  ///< Number of seconds after which a non-responsive client is considered disconnected. Client must respond to heartbeat pings from the server if not otherwise transmitting anything within this timeout period.
	/// \}

	/// \name Predefined value types
	/// Using these constants for the \refwc{DataRequest::valueSize} property will allow delta epsilon comparisons.
	/// \{
	static const uint32_t DATA_TYPE_INT8   = -1;  ///<  8-bit integer number (signed or unsigned)
	static const uint32_t DATA_TYPE_INT16  = -2;  ///< 16-bit integer number (signed or unsigned)
	static const uint32_t DATA_TYPE_INT32  = -3;  ///< 32-bit integer number (signed or unsigned)
	static const uint32_t DATA_TYPE_INT64  = -4;  ///< 64-bit integer number (signed or unsigned)
	static const uint32_t DATA_TYPE_FLOAT  = -5;  ///< 32-bit floating-point number
	static const uint32_t DATA_TYPE_DOUBLE = -6;  ///< 64-bit floating-point number
	/// \}

//----------------------------------------------------------------------------
//        Struct definitions
//----------------------------------------------------------------------------

// Utility functions used by structs, not public API --------------------------
	/// \private
	static void setCharArrayValue(char *dest, const size_t size, const char *str) {
		std::strncpy(dest, str, size-1);
		dest[size-1] = '\0';
	}
	#define WSE  WASimCommander::Enums
//------------------------------------------------------------------

	//using namespace WSE;

	#pragma pack(push, 1)

	/// Command data structure. The member contents depend on the command type as described in each command type of the `Enums::CommandId` enum documentation. \sa Enums::CommandId enum
	struct WSMCMND_API Command
	{
		uint32_t token;                ///< A unique ID for this command instance. Echoed back by server in command responses. Optional use for client implementations.
		uint32_t uData;                ///< DWORD command parameter value, meaning depends on the command being issued.
		double fData;                  ///< double-precision floating point command parameter value, meaning depends on the command being issued.
		WSE::CommandId commandId;      ///< What to do. \sa CommandId
		char sData[STRSZ_CMD] = {0};   ///< String command parameter value, meaning depends on the command being issued.
		                               //  544/544 B (packed/unpacked), 8/16 B aligned

		/// Default constructor with all parameters optional.
		explicit Command(WSE::CommandId id = WSE::CommandId::None, uint32_t uData = 0, const char *str = nullptr, double fData = 0.0, int32_t token = 0) :
			token(token), uData(uData), fData(fData), commandId(id)
		{
			if (str) setStringData(str);
		}

		void setStringData(const char *str) { setCharArrayValue(sData, STRSZ_CMD, str); }  ///< Set the `sData` member using a const char array.

		/// `ostream` operator for logging purposes
		friend inline std::ostream& operator<<(std::ostream& os, const Command &c)
		{
			const char *cmdName = (size_t)c.commandId < WSE::CommandIdNames.size() ? WSE::CommandIdNames.at((size_t)c.commandId) : "Invalid";
			return os << "Command{" << cmdName << "; token: " << c.token << "; uData: " << c.uData
			          << "; fData: " << std::fixed << std::setprecision(9) << c.fData << "; sData: " << std::quoted(c.sData) << '}';
		}
	};


	/// Structure for variable value subscription requests. \sa WASimCommander:CommandId::Subscribe command.
	struct WSMCMND_API DataRequest
	{
		uint32_t requestId;                  ///< Unique ID for the request, subsequent use of this ID overwrites any previous request definition (but size may not grow).
		uint32_t valueSize;                  ///< Byte size of stored value; can also be one of the predefined DATA_TYPE_* constants. \sa WASimCommander::DATA_TYPE_INT8, etc
		float deltaEpsilon;                  ///< Minimum change in numeric value required to trigger an update. The default of `0.0` is to send updates only if the value changes, but even on the smallest changes.
		                                     ///  Setting this to some positive value can be especially useful for high-precision floating-point numbers which may fluctuate within an insignifcant range,
		                                     ///  but may be used with any numeric value (for integer value types, only the integer part of the epsilon value is considered).
		                                     ///  Conversely, to send data updates _every time_ the value is read, and skip any comparison check altogether, set this to a negative value like `-1.0`.
		                                     ///< \note For the positive epsilon settings to work, the `valueSize` must be set to one of the predefined `DATA_TYPE_*` constants.
		uint32_t interval;                   ///< How many `UpdatePeriod` period's should elapse between checks. eg. 500ms or 10 ticks.
		                                     ///  Zero means to check at every `period`, `1` means every other `period`, etc.
		WSE::UpdatePeriod period;            ///< How often to read/calculate this value.
		WSE::RequestType requestType;        ///< Named variable or calculated value.
		WSE::CalcResultType calcResultType;  ///< Expected calculator result type.
		uint8_t simVarIndex;                 ///< Some SimVars require an index for access, default is 0.
		char varTypePrefix;                  ///< Variable type prefix for named variables. Types: 'L' (local), 'A' (SimVar) and 'T' (Token, not an actual GaugeAPI prefix) are checked using respecitive GaugeAPI methods.
		char nameOrCode[STRSZ_REQ] = {0};    ///< Variable name or full calculator string.
		char unitName[STRSZ_UNIT] = {0};     ///< Unit name for named variables (optional to override variable's default units). Only 'L' and 'A' variable types support unit specifiers.
		                                     //  1088/1088 B (packed/unpacked), 8/16 B aligned

		/// Constructor with required request ID, all other parameters optional. See member documentation for explanation of the homonymous parameters.
		explicit DataRequest(
			uint32_t            requestId,
			uint32_t            valueSize = sizeof(double),
			WSE::RequestType    requestType = WSE::RequestType::Calculated,
			WSE::CalcResultType calcResultType = WSE::CalcResultType::Double,
			WSE::UpdatePeriod   period = WSE::UpdatePeriod::Tick,
			const char *        nameOrCode = nullptr,
			const char *        unitName = nullptr,
			char                varTypePrefix = 'L',
			float               deltaEpsilon = 0.0f,
			uint8_t             interval = 0,
			uint8_t             simVarIndex = 0
		) :
			requestId(requestId), valueSize(valueSize), deltaEpsilon(deltaEpsilon), interval(interval), period(period),
			requestType(requestType), calcResultType(calcResultType), simVarIndex(simVarIndex), varTypePrefix(varTypePrefix)
		{
			if (nameOrCode)
				setNameOrCode(nameOrCode);
			if (unitName)
				setUnitName(unitName);
		}

		/// Constructs a request for a named variable (`requestType = RequestType::Named`) with optional update period, interval, and epsilon values.
		explicit DataRequest(uint32_t requestId, char variableType, const char *variableName, uint32_t valueSize,
		                     WSE::UpdatePeriod period = WSE::UpdatePeriod::Tick, uint32_t interval = 0, float deltaEpsilon = 0.0f) :
			DataRequest(requestId, valueSize, WSE::RequestType::Named, WSE::CalcResultType::None, period, variableName, nullptr, variableType, deltaEpsilon, interval)
		{ }
		/// Constructs a request for a named Simulator Variable (`requestType = RequestType::Named` and `varTypePrefix = 'A'`) with optional update period, interval, and epsilon values.
		explicit DataRequest(uint32_t requestId, const char *simVarName, const char *unitName, uint8_t simVarIndex, uint32_t valueSize,
		                     WSE::UpdatePeriod period = WSE::UpdatePeriod::Tick, uint32_t interval = 0, float deltaEpsilon = 0.0f) :
			DataRequest(requestId, valueSize, WSE::RequestType::Named, WSE::CalcResultType::None, period, simVarName, unitName, 'A', deltaEpsilon, interval, simVarIndex)
		{ }
		/// Constructs a calculator code request (`requestType = RequestType::Calculated`) with optional update period, interval, and epsilon values.
		explicit DataRequest(uint32_t requestId, WSE::CalcResultType resultType, const char *calculatorCode, uint32_t valueSize,
		                     WSE::UpdatePeriod period = WSE::UpdatePeriod::Tick, uint32_t interval = 0, float deltaEpsilon = 0.0f) :
			DataRequest(requestId, valueSize, WSE::RequestType::Calculated, resultType, period, calculatorCode, nullptr, 'Q', deltaEpsilon, interval)
		{ }

		void setNameOrCode(const char *name) { setCharArrayValue(nameOrCode, STRSZ_REQ, name); }  ///< Set the `nameOrCode` member using a const char array.
		void setUnitName(const char *name) { setCharArrayValue(unitName, STRSZ_UNIT, name); }     ///< Set the `unitName` member using a const char array.

		/// `ostream` operator for logging purposes
		friend inline std::ostream& operator<<(std::ostream& os, const DataRequest &r)
		{
			const char *perName = (size_t)r.period < WSE::UpdatePeriodNames.size() ? WSE::UpdatePeriodNames.at((size_t)r.period) : "Invalid";
			os << "DataRequest{" << r.requestId << "; size: " << r.valueSize << "; period: " << perName << "; interval: " << r.interval << "; deltaE: " << r.deltaEpsilon;
			if (r.requestType == WSE::RequestType::None)
				return os << "; type: None; }";
			if (r.requestType == WSE::RequestType::Named)
				return os << "; type: Named; name: " << std::quoted(r.nameOrCode) << "; unit: " << std::quoted(r.unitName) << "; varType: '" << r.varTypePrefix << "'; varIndex: " << r.simVarIndex << '}';
			const char *typeName = (size_t)r.calcResultType < WSE::CalcResultTypeNames.size() ? WSE::CalcResultTypeNames.at((size_t)r.calcResultType) : "Invalid";
			return os << "; type: Calculated; code: " << std::quoted(r.nameOrCode) << " resultType: " << typeName << '}';
		}
	};

	/// Data structure for sending Key Events to the sim with up to 5 event values. Events are specified using numeric MSFS Event IDs (names can be resolved to IDs via `Lookup` command).
	/// This supports the new functionality in MSFS SU10 with `trigger_key_event_EX1()` Gauge API function (similar to `SimConnect_TransmitClientEvent_EX1()`).
	/// The server will respond with an Ack/Nak for a `SendKey` command, echoing the given `token`. For events with zero or one value, the `SendKey` command can be used instead.
	/// \since v1.1.0  \sa Enums::CommandId::SendKey, Enums::CommandId::Lookup
	struct WSMCMND_API KeyEvent
	{
		uint32_t eventId;              ///< The event ID to trigger. Value is one of `KEY_*` macro values in MSFS/Legacy/gauges.h. Event names can be resolved to IDs via `Lookup` command.
		uint32_t values[5] = {0};      ///< Up to 5 values to pass to the event handler. All are optional, defaults are zero.
		uint32_t token;                ///< A unique ID for this event trigger. Echoed back by server in command Ack/Nak responses. Optional use for client implementations.
		uint32_t reserved;             ///< Padding for alignment, unused.
		                               //  32/32 B (packed/unpacked), 8/16 B aligned

		/// Default constructor with all parameters optional. The `values` initializer list may contain up to 5 members (any additional are ignored).
		explicit KeyEvent(uint32_t eventId = 0, std::initializer_list<uint32_t> values = {}, uint32_t token = 0) :
			eventId(eventId), token(token)
		{
			short i = 0;
			for (const uint32_t v : values) {
				this->values[i++] = v;
				if (i > 4)
					break;
			}
		}

		/// `ostream` operator for logging purposes
		friend inline std::ostream& operator<<(std::ostream& os, const KeyEvent &c)
		{
			os << "KeyEvent{eventId: " << c.eventId << "; token: " << c.token << ';';
			for (short i=0; i < 5; ++i)
				os << " v" << i << ": " << c.values[i] << ';';
			return os << '}';
		}
	};


	/// Log record structure. \sa WASimCommander:CommandId::Log command.
	struct WSMCMND_API LogRecord
	{
		time_t timestamp;               ///< ms since epoch.
		WSE::LogLevel level;            ///< Message severity.
		char message[STRSZ_LOG] = {0};  ///< The log message text.
		                                //  1040/1040 B (packed/unpacked), 8/16 B aligned

		/// Default constructor with all parameters optional.
		explicit LogRecord(const WSE::LogLevel level = WSE::LogLevel::None, const char *msg = nullptr, const std::chrono::system_clock::time_point &tp = std::chrono::system_clock::now()) :
			timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()}, level{level}
		{
			if (msg) setMessage(msg);
		}

		void setMessage(const char *msg) { setCharArrayValue(message, STRSZ_LOG, msg); }  ///< Set the `message` member using a const char array.

		/// `ostream` operator for logging purposes
		friend inline std::ostream& operator<<(std::ostream& os, const LogRecord &l)
		{
			const time_t secs = l.timestamp / 1000;
			const tm *tim = std::localtime(&secs);
			const char *levelName = (size_t)l.level < WSE::LogLevelNames.size() ? WSE::LogLevelNames.at((size_t)l.level) : "Invalid";
			if (tim)
				return os << "LogRecord{" << levelName << std::put_time(tim, " [(%z) %y-%m-%d %H:%M:%S.") << (l.timestamp - time_t(secs * 1000)) << "] " << l.message << '}';
			return os << "LogRecord{" << levelName << " [" << l.timestamp << "] " << l.message << '}';
		}
	};

	#pragma pack(pop)

	#undef WSE
};  // namespace WASimCommander
