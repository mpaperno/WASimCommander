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
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "WASimCommander.h"
#include "client/exports.h"
#include "client/enums.h"

/// \file

/// \def WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
/// Define with value of 0 (zero) to invoke all callbacks consecutively, using a mutex lock. Default is (possible) concurrent invocation. Callbacks may still arrive from different threads. \relates WASimClient
#ifndef WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
	#define WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS  1
#endif

/// WASimCommander::Client namespace.
/// Defines/declares everything needed to interact with the _WASimCommander Client API_, including the WASimClient class itself.
namespace WASimCommander::Client
{

//----------------------------------------------------------------------------
//        Constants
//----------------------------------------------------------------------------

/// \name Return result values
/// \{
#ifndef E_NOT_CONNECTED
static const HRESULT E_NOT_CONNECTED = /*ERROR_NOT_CONNECTED*/ 2250L | (/*FACILITY_WIN32*/ 7 << 16) | 0x80000000; ///< Error result: server not connected
#endif
#ifndef E_TIMEOUT
static const HRESULT E_TIMEOUT       = /*ERROR_TIMEOUT*/       1460L | (/*FACILITY_WIN32*/ 7 << 16) | 0x80000000;  ///< Error result: timeout communicating with server.
#endif
/// \}

//----------------------------------------------------------------------------
//        Struct definitions
//----------------------------------------------------------------------------

#define WSE  WASimCommander::Enums

	/// Client Event data, delivered via callback. \sa WASimClient::setClientEventCallback(), ClientEventType, ClientStatus
	struct WSMCMND_API ClientEvent
	{
		ClientEventType eventType;  ///< The type of event. See enum docs for details.
		ClientStatus status;        ///< Current status flag(s). See enum docs for details.
		std::string message;        ///< A short message about the event (eg. "Server Connected")
	};


	/// Structure for delivering list results, eg. of local variables sent from Server.
	/// \sa WASimClient::list(), listResultsCallback_t, WASimCommander::LookupItemType, WASimCommander::CommandId::List command.
	struct WSMCMND_API ListResult
	{
		using listResult_t = std::vector<std::pair<int, std::string>>;  ///< A mapping of IDs to names.

		WSE::LookupItemType listType;  ///< the type of items being listed
		HRESULT result;           ///< Excecution result, one of: `S_OK`, `E_FAIL`, `E_TIMEOUT`
		listResult_t list;        ///< Mapping of numeric item IDs to name strings.
	};


	/// `DataRequestRecord` inherits and extends `WASimCommander::DataRequest` with data pertinent for use by a data consumer/Client.
	/// In particular, any value data sent from the server is stored here as a byte array in the `data` member (a `std::vector` of `unsigned char`).
	///
	/// `DataRequest` subscription results are delivered by `WASimClient` (via `dataCallback_t` callback) using this `DataRequestRecord` structure.
	/// WASimClient also holds a list of all data requests which have been added using `WASimClient::saveDataRequest()` method. These
	/// requests are available for reference using `WASimClient::dataRequest()`, `WASimClient::dataRequests()`, and `WASimClient::dataRequestIdsList()` methods.
	///
	/// An implicit data conversion `operator T()` template is available for _default constructible_ and _trivially
	/// copyable_ types (eg. numeric, char, or fixed-size arrays of such types). This returns a default-constructed value if the
	/// conversion would be invalid (size of requested type doesn't match the data array size).
	///
	/// There is also the `bool tryConvert(T &)` template which tries to populate a pre-initialized value reference
	/// of the desired type and returns true or false depending on if the conversion was valid.
	///
	/// Note that the size of the data array may or _may not_ be the same as the inherited `DataRequest::valueSize` member, since that may contain
	/// special values for preset types. Always check the actual data array size (`data.size()`) if you need to know the storage requirements.
	///
	/// \sa WASimClient::setDataCallback, dataCallback_t, WASimCommander::DataRequest
	struct WSMCMND_API DataRequestRecord : public DataRequest
	{
		time_t lastUpdate = 0;          ///< Timestamp of last data update in ms since epoch.
		std::vector<uint8_t> data {};   ///< Value data array.

		/// Implicit conversion operator for default constructible and trivially copyable types (eg. numeric, char)
		/// or fixed-size arrays of such types (eg. char strings). This returns a default-constructed value if the conversion
		/// would be invalid (size of requested type doesn't match data size).
		template<typename T,
			std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true,
			std::enable_if_t<std::is_default_constructible_v<T>, bool> = true>
		inline operator T() const {
			T ret = T();
			tryConvert(ret);
			return ret;
		}

		/// Tries to populate a pre-initialized value reference of the desired type and returns true or false
		/// depending on if the conversion was valid (meaning the size of requested type matches the data size).
		/// If the conversion fails, the original result value is not changed, allowing for any default to be preserved.
		/// The value must be trivially copyable, (eg. numerics, chars), or fixed-size arrays of such types.
		template<typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
		inline bool tryConvert(T &result) const {
			bool ret;
			if ((ret = data.size() == sizeof(T)))
				memcpy(&result, data.data(), sizeof(T));
			return ret;
		}

		// The c'tors and assignments are primarily for internal use and container storage requirements, but may also be useful for subclasses.
		using DataRequest::DataRequest;   ///< Inherits DataRequest constructors.
		using DataRequest::operator=;     ///< Inherits DataRequest assignement operators.
		DataRequestRecord();              ///< Default c'tor creates an invalid instance with reqiest ID of -1, zero size, and `RequestType::None`.
		DataRequestRecord(const DataRequest &request);  ///< Constructs new instance from a `DataRequest` instance by lvalue/copy. The data array is initialized to the corresponding size with 0xFF value for all bytes.
		DataRequestRecord(DataRequest &&request);       ///< Constructs new instance from a `DataRequest` instance by rvalue/move. The data array is initialized to the corresponding size with 0xFF value for all bytes.
	};

	/// Structure for using with `WASimClient::getVariable()` and `WASimClient::setVariable()` to specify information about the variable to set or get. Variables and Units can be specified by name or by numeric ID.
	/// Only some variable types have an associated numeric ID ('A', 'L', 'T' types) and only some variable types accept a Unit specifier ('A', 'C', 'E', 'L' types). Using numeric IDs, if already known, is more efficient
	/// on the server side since it saves the lookup step.
	struct WSMCMND_API VariableRequest
	{
		char variableType = 'L';       ///< A single character variable type identifier as documented in the MSFS SDK documentation (plus 'T' for Token vars).
		std::string variableName {};   ///< Name of the variable. Ignored if `variableId` is greater than -1.
		std::string unitName {};       ///< Unit name. For local ('L') and Sim ('A') variables the unit type will be resolved to an enum ID and, if successful, will be used in the relevant command (`(get | set)_named_variable_typed_value()`/`aircraft_varget()`).
		                               ///  Otherwise, for 'L' vars the default units are used (_Gauge API_ `(get | set)_named_variable_value()`) and for getting SimVars the behavior is undefined (-1 is passed to `aircraft_varget()` as the unit ID).
		                               ///  For the other variable types which use units ('C', 'E', or when setting 'A'), this string will simply be included in the produced calculator code (eg. `(E:SIMULATION TIME,seconds)` or `50 (>A:COCKPIT CAMERA ZOOM,percent)`).
		                               ///  The unit name is ignored for all other variable types, and the `unitId` field is preferred if it is greater than -1.
		int variableId = -1;           ///< Numeric ID of the variable to get/set. Overrides the `variableName` field if greater than -1. Only 'A', 'L', 'T' variable types can be referenced by numeric IDs.
		int unitId = -1;               ///< Numeric ID of the Unit type to use in the get/set command. Overrides the `unitName` field if greater than -1. See usage notes for `unitName` about applicable variable types.
		uint8_t simVarIndex = 0;       ///< Optional index number for SimVars ('A') which require them. If using named variables, yhe index can also be included in the variable name string (after a colon `:`, as would be used in a calculator string).
		bool createLVar = false;       ///< This flag indicates that the L var should be created if it doesn't already exist in the simulator. This applies for both "Set" and "Get" commands.

		/// Default constructor, with optional parameters for variable type, name, unit name, SimVar index and `createLVar` flag.
		explicit VariableRequest(char variableType = 'L', const std::string &variableName = std::string(), const std::string &unitName = std::string(), uint8_t simVarIndex = 0, bool createVariable = false) :
			variableType{variableType}, variableName{variableName}, unitName{unitName}, simVarIndex(simVarIndex), createLVar{createVariable} { }
		/// Construct a variable request using numeric variable and (optionally) unit IDs, and optional SimVar index.
		explicit VariableRequest(char variableType, int variableId, int unitId = -1, uint8_t simVarIndex = 0) :
			variableType{variableType}, variableId{variableId}, unitId{unitId}, simVarIndex(simVarIndex) { }
		/// Construct a variable request for a Simulator Variable (SimVar) with given name, unit, and optional index parameter.
		explicit VariableRequest(const std::string &simVarName, const std::string &unitName, uint8_t simVarIndex = 0) :
			variableType{'A'}, variableName{simVarName}, unitName{unitName}, simVarIndex(simVarIndex) { }
		/// Construct a variable request for a Simulator Variable ('A') using numeric variable and unit IDs, with optional index parameter.
		explicit VariableRequest(int simVarId, int unitId, uint8_t simVarIndex = 0) :
			variableType{'A'}, variableId{simVarId}, unitId{unitId}, simVarIndex(simVarIndex) { }
		/// Construct a variable request for a Local variable ('L') with the given name. `createVariable` will create the L var on the simulator if it doesn't exist yet
		/// (for "Get" as well as "Set" commands). An optional unit name can also be provided.
		explicit VariableRequest(const std::string &localVarName, bool createVariable = false, const std::string &unitName = std::string()) :
			variableType{'L'}, variableName{localVarName}, unitName{unitName}, createLVar{createVariable} { }
		/// Construct a variable request for a Local variable ('L') with the given numeric ID.
		explicit VariableRequest(int localVarId) :
			variableType{'L'}, variableId{localVarId} { }
	};


	/// Structure to hold data for registered (reusable) calculator events. Used to submit events with `WASimClient::registerEvent()`.
	///
	/// WASimClient also holds a list of all registered events which have been added using `WASimClient::registerEvent()`. These
	/// requests are available for reference using `WASimClient::registeredEvent()` and `WASimClient::registeredEvents()` methods.
	/// \sa WASimClient::registerEvent(), WASimCommander::CommandId::Register
	struct WSMCMND_API RegisteredEvent
	{
		uint32_t eventId = -1;  ///< A unique ID for this event. The ID can later be used to modify, trigger, or remove this event.
		std::string code {};    ///< The calculator code string to execute as the event action. The code is pre-compiled and stored on the server for quicker execution.
		                        ///< Maximum length is `WASimCommander::STRSZ_CMD` value minus the `name` string length, if one is used.
		std::string name {};    ///< Optional custom name for this event. The name is for use with `SimConnect_MapClientEventToSimEvent(id, "event_name")` and `SimConnect_TransmitClientEvent(id)`. Default is to use the event ID as a string.
		                        ///  If the custom event name contains a period (`.`) then it is used as-is. Otherwise "WASimCommander.[client_name]." will be prepended to the name.
		                        ///  \note The event name **cannot be changed** after the initial registration (it is essentially equivalent to the `eventId`). When updating an existing event on the server, it is
		                        ///  not necessary to include the name again, only the `eventId` is required (and the code to execute, of course). Maximum length for the name string is the value of `WASimCommander::STRSZ_ENAME`.

		/// Default implicit constructor.
		RegisteredEvent(uint32_t eventId = -1, const std::string &code = "", const std::string &name = "");
		RegisteredEvent(RegisteredEvent const &) = default;
	};

//----------------------------------------------------------------------------
//       Callback Types
//----------------------------------------------------------------------------

	using clientEventCallback_t = std::function<void __stdcall(const ClientEvent &)>;   ///< Callback function for Client events. \sa WASimClient::setClientEventCallback()
	using listResultsCallback_t = std::function<void __stdcall(const ListResult &)>;    ///< Callback function for delivering list results, eg. of local variables sent from Server. \sa WASimClient::setListResultsCallback()
	using dataCallback_t = std::function<void __stdcall(const DataRequestRecord &)>;    ///< Callback function for subscription result data. \sa WASimClient::setDataCallback()
	using logCallback_t = std::function<void __stdcall(const LogRecord &, LogSource)>;  ///< Callback function for log entries (from both Client and Server). \sa WASimClient::setLogCallback()
	using commandCallback_t = std::function<void __stdcall(const Command &)>;           ///< Callback function for commands sent from server. \sa WASimClient::setCommandResultCallback(), WASimClient::setResponseCallback()


// -------------------------------------------------------------
// WASimClient
// -------------------------------------------------------------

	/// WASimCommander Client implementation. Handles all aspects of communication with the WASimCommander Server WASM module.
	class WSMCMND_API WASimClient
	{
	public:
		/// Instantiate the client with a unique ID and optional configuration file path.
		/// \param clientId This ID must not be shared with any other Client accessing the Server.
		///   The ID is also used as the client name by converting it to a 8-character hexadecimal string. For example 3235839725 = "C0DEFEED". Get creative.
		///   The client name is used as a key component of data exchange with the simulator engine.
		///   The ID cannot be zero, and server connections will fail if it is. It can also be set/changed after class creation using the `setClientId()` method, but before connecting to the server.
		/// \param configFile Optionally provide the path to a configuration file for reading initial startup settings. By default the client will look for a `client_conf.ini` file in the current working directory.
		///   The parameter value may include the file name (with extension), or be only a file system path, in which case the default file name of "client_conf.ini" will be appended.
		explicit WASimClient(uint32_t clientId, const std::string &configFile = std::string());
		/// Any open network connections are automatically closed upon destruction, though it is better to close them yourself before deleting the client.
		~WASimClient();

		/// \name Network actions, status, and settings
		/// \{

		ClientStatus status() const;  ///< Get current connection status of this client. \sa WASimCommander::Client::ClientStatus
		bool isInitialized() const;   ///< Check if simulator network link is established. \sa connectSimulator()
		bool isConnected() const;     ///< Check WASimCommander server connection status.  \sa connectServer()
		uint32_t clientVersion() const;  ///< Return the current WASimClient version number. Version numbers are in "BCD" format:  `MAJOR << 24 | MINOR << 16 | PATCH << 8 | BUILD`, eg: `1.23.45.67 = 0x01234567`
		uint32_t serverVersion() const;  ///< Return the version number of the last-connected, or successfully pinged, WASimModule (sever), or zero if unknown. See `clientVersion()` for numbering details.

		/// Initialize the simulator network link and set up minimum necessary for WASimCommander server ping or connection. Uses default newtwork SimConnect configuration ID.
		/// \param timeout Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value.
		/// \return `S_OK` (0) - Success;\n
		///  `E_FAIL` (0x80004005) - General failure (most likely simulator is not running);\n
		///  `E_TIMEOUT` (0x800705B4) - Connection attempt timed out (simulator/network issue);\n
		///  `E_INVALIDARG` (0x80070057) - The Client ID set in constructor is invalid (zero) or the SimConnect.cfg file did not contain the default config index (see `networkConfigurationId()` ) ;
		/// \note This method blocks until either the Simulator responds or the timeout has expired.
		/// \sa defaultTimeout(), setDefaultTimeout(), networkConfigurationId() setNetworkConfigurationId(), connectSimulator(int, uint32_t)
		HRESULT connectSimulator(uint32_t timeout = 0);
		/// Initialize the simulator network link and set up minimum necessary for WASimCommander server ping or connection. This overload allows specifying a SimConnect configuration ID and optional timeout value.
		/// \param networkConfigId SimConnect is used for the network layer. Specify the SimConnect.cfg index to use, or -1 (default) to force a local connection.
		/// \param timeout Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value.
		/// \return `S_OK` (0) - Success;\n
		///  `E_FAIL` (0x80004005) - General failure (most likely simulator is not running);\n
		///  `E_TIMEOUT` (0x800705B4) - Connection attempt timed out (simulator/network issue);\n
		///  `E_INVALIDARG` (0x80070057) - The Client ID set in constructor is invalid (zero) or the SimConnect.cfg file did not contain the config index requested in the `simConnectConfigId` parameter;
		/// \note This method blocks until either the Simulator responds or the timeout has expired. \sa connectSimulator(), defaultTimeout(), setDefaultTimeout()
		HRESULT connectSimulator(int networkConfigId, uint32_t timeout = 0);
		/// Shut down all network connections (and disconnect WASimCommander server if connected).
		/// After calling this method, one must call `connectSimulator()`/`connectServer()`/`pingServer()` again before any other commands.
		void disconnectSimulator();

		/// Check if WASimCommander Server exists (Simulator running, the WASIM module is installed and working). Returns server version number, or zero if server did not respond.\n
		/// This will implicitly call `connectSimulator()` first if it hasn't already been done, using the default network configuration settings. Zero will be returned if the connection could not be established.
		/// \param timeout Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value.
		/// \return Server version number, or zero (`0`) if server (or simulator) didn't respond within the timeout period.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa defaultTimeout(), setDefaultTimeout()
		uint32_t pingServer(uint32_t timeout = 0);

		/// Connect to WASimCommander server.
		/// This will implicitly call \c connectSimulator() first if it hasn't already been done, using the default network configuration setting.
		/// \param timeout Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value.
		/// \return `S_OK` (0) - Success.\n
		///  `E_FAIL` (0x80004005) - General failure (most likely simulator is not running).\n
		///  `E_TIMEOUT` (0x800705B4) - Connection attempt timed out (simulator/network issue or WASimCommander WASM module is not installed/running).\n
		///  `E_INVALIDARG` (0x80070057) - The Client ID set in constructor is invalid (zero) or the SimConnect.cfg file did not contain the default config index.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa defaultTimeout(), setDefaultTimeout()
		HRESULT connectServer(uint32_t timeout = 0);
		/// Disconnect from the WASimCommander server. This does _not_ close the Simulator network connection (use \c disconnectSimulator() to do that or both at once).
		void disconnectServer();

		/// Get the current default server response timeout value, which is used in all network requests.
		/// The initial default setting is read from the `client_conf.ini` file or set to 1000ms if no config file was found. \sa setDefaultTimeout().
		uint32_t defaultTimeout() const;
		///< Set the default timeout period for server reponses. The default may be inadequate on slow network links or a very busy simulator. \sa defaultTimeout()
		void setDefaultTimeout(uint32_t ms);

		/// SimConnect is used for the network layer. This setting specifies the SimConnect.cfg index to use. The value of -1 forces a local connection.
		/// The initial default setting is read from the `client_conf.ini` file or set to `-1` if no config file was found. \sa setNetworkConfigurationId().
		int networkConfigurationId() const;
		/// SimConnect is used for the network layer. This setting specifies the SimConnect.cfg index to use, or -1 to force a local connection.
		/// Note that this must be called before `connectSimulator()` invocation in order to have any effect. \sa networkConfigurationId().
		void setNetworkConfigurationId(int configId);

		/// \}
		/// \name High level API
		/// \{

		// Calculator code -----------------------------------

		/// Run a string of MSFS _Gauge API_ calculator code in RPN format, possibly with some kind of result expected.
		/// \param code The text of the code to execute.  See https://docs.flightsimulator.com/html/Additional_Information/Reverse_Polish_Notation.htm
		/// \param resultType Expected result type, or `Enums::CalcResultType::None` (default) if no result is expected. If the type is `Enums::CalcResultType::Formatted` then the server
		///                   runs the code using `format_calculator_string()` _Gauge API_ function (see @ bottom of RPN docs for formatting options) and the result type is always a string.
		///                   Otherwise `execute_calculator_code()` is used and any of the result types can be returned (in fact `execute_calculator_code()` always returns any results in all 3 types at once, so even
		///                   if a numeric result is requested, the string result will also be populated).
		/// \param pfResult A pointer to an initialized variable of `double` to store the result into if `resultType` is `Enums::CalcResultType::Double` or `Enums::CalcResultType::Integer`.
		/// \param psResult A string pointer to store the string result into. The string version is typically populated even for numeric type requests, but definitely for `Enums::CalcResultType::String` or `Enums::CalcResultType::Formatted` type requests.
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected to server; \n
		/// If a result is expected, may also return `E_FAIL` if the server returned Nak response, or `E_TIMEOUT` on general server communication failure.
		/// \note _If_ a result is expected (`resultType` != `Enums::CalcResultType::None`) then this method blocks until either the Server responds or the timeout has expired (see `defaultTimeout()`).
		/// To request calculated results in a non-blocking fashion, use a data request instead.
		///
		/// If you need to execute the same code multiple times, it would be more efficient to save the code as either a data request (for code returning values) or a registered event (for code not returning values).
		/// The advantage is that in those cases the calculator string is pre-compiled to byte code and saved once, then each invocation of the _Gauge API_ calculator functions uses the more efficient byte code version.
		/// (To prevent automatic data updates for data requests, just set the data request period to `Enums::UpdatePeriod::Never` or `Enums::UpdatePeriod::Once` and use the `updateDataRequest()` method to poll for value updates as needed.)
		/// See `saveDataRequest()` and `registerEvent()` respecitvely for details.
		/// \sa \refwce{CommandId::Exec}, defaultTimeout(), setDefaultTimeout()
		HRESULT executeCalculatorCode(const std::string &code, WSE::CalcResultType resultType = WSE::CalcResultType::None, double *pfResult = nullptr, std::string *psResult = nullptr) const;

		// Variables accessors ------------------------------

		/// Get a Variable value by name, with optional named unit type. This is primarily useful for local ('L') variables, SimVars ('A') and token variables ('T') which can be read via dedicated _Gauge API_ functions
		/// (`get_named_variable_value()`/`get_named_variable_typed_value()`, `aircraft_varget()`,  and `lookup_var()` respectively). \n
		/// Other variables types can also be set this way ('C', 'E', 'M', etc) but such requests are simply **converted to a calculator string** and evaluated via the _Gauge API_ `execute_calculator_code()`. \n
		/// Likewise, requesting string-type variables using this method also ends up running a calculator expression on the server side. \n
		/// In both cases, using `WASimClient::executeCalculatorCode()` directly may be more efficient. Also, unlike `executeCalculatorCode()`, this method will not return a string representation of a numeric value.
		/// \param variable See `VariableRequest` documentation for descriptions of the individual fields.
		/// \param pfResult Pointer to a double precision variable to hold the numeric result.
		/// \param psResult Pointer to a string type variable to hold a string-type result. See notes above regarding string types.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or `E_FAIL` if server returns a Nak response.
		/// \note This method blocks until either the Server responds or the timeout has expired.
		/// \sa \refwcc{VariableRequest}, \refwce{CommandId::Get},  defaultTimeout(), setDefaultTimeout()
		HRESULT getVariable(const VariableRequest &variable, double *pfResult, std::string *psResult = nullptr);
		/// A convenience version of `getVariable(VariableRequest(variableName, false, unitName), pfResult)`. See `getVariable()` and `VariableRequest` for details.
		/// \param variableName Name of the local variable.
		/// \param pfResult Pointer to a double precision variable to hold the result.
		/// \param unitName Optional unit specifier to use. Most Local vars do not specify a unit and default to a generic "number" type.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or `E_FAIL` if server returns a Nak response.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa \refwce{CommandId::Get},  defaultTimeout(), setDefaultTimeout()
		HRESULT getLocalVariable(const std::string &variableName, double *pfResult, const std::string &unitName = std::string());
		/// Gets the value of a local variable just like `getLocalVariable()` but will also create the variable on the simulator if it doesn't already exist.
		/// \param variableName Name of the local variable.
		/// \param pfResult Pointer to a double precision variable to hold the result.
		/// \param defaultValue The L var will be created on the simulator if it doesn't exist yet using this initial value (and this same value will be returned in `pfResult`).
		/// \param unitName Optional unit specifier to use. Most Local vars do not specify a unit and default to a generic "number" type.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or `E_FAIL` if server returns a Nak response.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa \refwce{CommandId::GetCreate}, defaultTimeout(), setDefaultTimeout()
		HRESULT getOrCreateLocalVariable(const std::string &variableName, double *pfResult, double defaultValue = 0.0, const std::string &unitName = std::string());

		/// Set a Variable value by name, with optional named unit type. Although any settable variable type can set this way, it is primarily useful for local (`L`) variables which can be set via dedicated _Gauge API_ functions
		/// (`set_named_variable_value()` and `set_named_variable_typed_value()`). Other variables types can also be set this way ('A', 'H", 'K', etc) but such requests are simply converted to a calculator string and
		/// evaluated via the _Gauge API_ `execute_calculator_code()`. Using `WASimClient::executeCalculatorCode()` directly may be more efficient.
		/// \param variable See `VariableRequest` documentation for descriptions of the individual fields.
		/// \param value The value to set.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, or `E_FAIL` on general failure (unlikely).
		/// \sa \refwce{CommandId::Set}
		HRESULT setVariable(const VariableRequest &variable, const double value);
		/// A convenience version of `setVariable()` for Local variable types. Equivalent to `setVariable(VariableRequest(variableName, false, unitName), value)`. See `setVariable()` and `VariableRequest` for details.
		/// \param variableName Name of the local variable.
		/// \param value The value to set.
		/// \param unitName Optional unit specifier to use. Most Local vars do not specify a unit and default to a generic "number" type.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, or `E_FAIL` on general failure (unlikely).
		/// \sa \refwce{CommandId::Set}
		HRESULT setLocalVariable(const std::string &variableName, const double value, const std::string &unitName = std::string());
		/// Set a Local Variable value by variable name, creating it first if it does not already exist. This first calls the `register_named_variable()` _Gauge API_ function to get the ID from the name,
		/// which creates the variable if it doesn't exist. The returned ID (new or existing) is then used to set the value. Use the `lookup()` method to check for the existence of a variable name.
		/// Equivalent to `setVariable(VariableRequest(variableName, true, unitName), value)`. See `setVariable()` and `VariableRequest` for details.
		/// \param variableName Name of the local variable.
		/// \param value The value to set. Becomes the intial value if the variable is created.
		/// \param unitName Optional unit specifier to use. Most Local vars do not specify a unit and default to a generic "number" type.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, or `E_FAIL` on general failure (unlikely).
		/// \sa \refwce{CommandId::SetCreate}
		HRESULT setOrCreateLocalVariable(const std::string &variableName, const double value, const std::string &unitName = std::string());

		// Data subscriptions -------------------------------

		/// Add a new `WASimCommander::DataRequest` or update an existing one with the same `DataRequest::requestId`. If the client is not currently connected to the server, the request is queued until the next connection is established.
		/// \param request The `WASimCommander::DataRequest` structure to process. See `WASimCommander::DataRequest` documentation for details of the structure members.
		/// \param async `true` to wait for a response from the server before returning, or `false` (default) to wait for an `Ack`/`Nak` response. See return values and the Note below for more details.
		/// \return `S_OK` on success, `E_INVALIDARG` if there is a problem with the `DataRequest` contents. \n
		/// If currently connected to the server and `async` is `false`, may also return `E_FAIL` if the server returned `Nak` response, or `E_TIMEOUT` on general server communication failure.
		/// \note If currently connected to the server and the `async` param is `false`, this method will block until either the Server responds or the timeout has expired (see `defaultTimeout()`).
		/// \par Tracking async calls
		/// To track the status of an async request, set a callback function with `setCommandResultCallback()`. The server should respond with an \refwce{CommandId::Ack} or \refwce{CommandId::Nak}
		/// \refwc{Command} where the `uData` value is \refwce{CommandId::Subscribe} and the \refwc{Command::token} will be the `requestId` value from the given `request` struct.
		/// \sa  \refwc{DataRequest} \refwce{CommandId::Subscribe}, removeDataRequest(), updateDataRequest()
		HRESULT saveDataRequest(const DataRequest &request, bool async = false);
		/// Remove a previously-added `DataRequest`. This clears the subscription and any tracking/meta data from both server and client sides.
		/// Using this method is effectively the same as calling `dataRequest()` with a `DataRequest` of type `RequestType::None`.
		/// \param requestId ID of the request to remove.
		/// \return `S_OK` on success, `E_FAIL` if the original request wasn't found.
		/// \note If the subscription data may be needed again in the future, it would be more efficient to edit the request (using `saveDataRequest()`) and suspend updates by setting the `DataRequest::period` to `UpdatePeriod::Never`.
		/// To resume updates change the period again.
		/// \sa \refwc{DataRequest}, \refwce{CommandId::Subscribe}, saveDataRequest(), updateDataRequest()
		HRESULT removeDataRequest(const uint32_t requestId);
		/// Trigger a data update on a previously-added `DataRequest`. Designed to refresh data on subscriptions with update periods of `UpdatePeriod::Never` or `UpdatePeriod::Once`, though it can be used with any subscription.
		/// Using this update method also skips any equality checks on the server side (though any delta epsilon value remains in effect on client side).
		/// \param requestId The ID of a previously added `DataRequest`.
		/// \return `S_OK` on success, `E_FAIL` if the original request wasn't found,.
		/// \sa \refwce{CommandId::Update}, saveDataRequest(), removeDataRequest()
		HRESULT updateDataRequest(uint32_t requestId);

		/// Returns a copy of a `DataRequestRecord` which has been previously added. If the request with the given `requestId` doesn't exist, an invalid `DataRequestRecord` is returned which has
		/// the members `DataRequest::requestId` set to `-1`, `DataRequest::valueSize` set to `0`, and `DataRequest::requestType` set to `RequestType::None`.
		DataRequestRecord dataRequest(uint32_t requestId) const;
		/// Returns a list of all data requests which have been added to the Client so far. (These are returned by copy operation, so for a long list it may get "expensive.")
		std::vector<DataRequestRecord> dataRequests() const;
		/// Returns a list of all `DataRequest::requestId`s which have been added to the Client so far.
		std::vector<uint32_t> dataRequestIdsList() const;

		/// Enables or disables all data request subscription updates at the same time. Use this to temporarily suspend value update checks when they are not needed, but may be again in the future.
		/// This is a lot more efficient than disconnecting and re-connecting to the server, since all the data requests need to be re-created upon every new connection (similar to SimConnect itself).
		/// \since{v1.2}
		/// This method can be called while not connected to the server. In this case the setting is saved and sent to the server upon next connection, before sending any data request subscriptions.
		/// This way updates could be suspended upon initial connection, then re-enabled when the data is actually needed.
		/// \return `S_OK` on success; If currently connected to the server, may also return `E_TIMEOUT` on general server communication failure.
		HRESULT setDataRequestsPaused(bool paused) const;

		// Custom Event registration --------------------------

		/// Register a reusable event which executes a pre-set calculator string. The code is pre-compiled and stored on the server for quicker execution.
		/// The event can have an optional custom name for direct use with any SimConnect client. Registered events can also be triggered by using the `transmitEvent()` method.
		/// If the server is not currently connected, the event registration will be queued and sent next time a connection is established.
		/// \return `S_OK` on success, `E_INVALIDARG` if the resulting code string is too long or if trying to change the name of an already registered event.
		/// \sa RegisteredEvent, \refwce{CommandId::Register}, removeEvent(), transmitEvent()
		HRESULT registerEvent(const RegisteredEvent &eventData);
		/// Remove an event previously registered with `registerEvent()` method. This is effectively the same as calling `registerEvent()` with an empty `code` parameter.
		/// If the server is not currently connected, the removal request will be queued and sent next time a connection is established.
		/// \param eventId ID of the previously registered event.
		/// \return `S_OK` on success, `E_INVALIDARG` if the eventId wasn't found.
		/// \sa \refwce{CommandId::Register}, registerEvent(), transmitEvent()
		HRESULT removeEvent(uint32_t eventId);
		/// Trigger an event previously registered with `registerEvent()`. This is a more direct alternative to triggering events by name via SimConnect.
		/// \param eventId ID of the previously registered event. If the event hasn't been registerd, the server will log a warning but otherwise nothing will happen.
		/// \return `S_OK` on success, `E_FAIL` on general failure (unlikely), `E_NOT_CONNECTED` if not connected to server.
		/// \sa \refwce{CommandId::Transmit}, registerEvent(), removeEvent()
		HRESULT transmitEvent(uint32_t eventId);

		/// Returns a copy of a `RegisteredEvent` which has been previously added with `registerEvent()`. If the event with the given `eventId` doesn't exist, an invalid `RegisteredEvent` is returned which has
		/// the members `RegisteredEvent::eventId` set to `-1`, and `RegisteredEvent::code` and `RegisteredEvent::name` both empty.
		RegisteredEvent registeredEvent(uint32_t eventId);
		/// Returns a list of all registered events which have been added to the Client with `registerEvent()`. The list members are created by copy.
		std::vector<RegisteredEvent> registeredEvents() const;

		// Simulator Key Events --------------------------

		/// Trigger a simulator Key Event by ID, with up to 5 optional values which are passed onto the event handler.
		/// This is equivalent to the MSFS Gauge API function `trigger_key_event_EX1()` and similar to `SimConnect_TransmitClientEvent_EX1()` (MSFS SU10 and above).
		/// \param keyEventId Numeric ID of the Event to trigger. These are found in the header file 'MSFS/Legacy/gauges.h'. Event names can be resolved to IDs using the `lookup()` method.
		/// \param v1-v5 Optional values to pass to the event handler. Defaults are all zeros (same as for `trigger_key_event_EX1()`).
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or `E_FAIL` on unexpected SimConnect error.
		/// \note Server responds asynchronously with an Ack/Nak resonse to `CommandId::SendKey` command type; A Nak means the event ID is not valid.
		/// \since v1.1.0
		HRESULT sendKeyEvent(uint32_t keyEventId, uint32_t v1 = 0, uint32_t v2 = 0, uint32_t v3 = 0, uint32_t v4 = 0, uint32_t v5 = 0) const;

		/// Trigger a simulator Key Event by name, with up to 5 optional values which are passed onto the event handler.
		/// This is equivalent to the MSFS Gauge API function `trigger_key_event_EX1()` and similar to `SimConnect_TransmitClientEvent_EX1()` (MSFS SU10 and above).
		///
		/// Note that on first usage the event name is resolved to an ID (using the `lookup()` method) and the resulting ID (if valid) is cached for future uses.
		/// The cache is kept as a simple `std::unordered_map` type, so if you have a better way to save the event IDs from `lookup()`, use that and call the `sendKeyEvent(uint32_t keyEventId, ...)` overload directly.
		/// \param keyEventName Name of the Event to trigger. These are typically the same names found in MSFS Event IDs reference (and as lited in the header file 'MSFS/Legacy/gauges.h' but w/out the "KEY_" prefix).
		/// \param v1-v5 Optional values to pass to the event handler. Defaults are all zeros (same as for `trigger_key_event_EX1()`).
		/// \return `S_OK` on success, `E_INVALIDARG` if event name could not be resolved to an ID, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or `E_FAIL` on unexpected SimConnect error.
		/// \note Server responds asynchronously with an Ack resonse to `CommandId::SendKey` command type.
		/// \since v1.1.0
		HRESULT sendKeyEvent(const std::string &keyEventName, uint32_t v1 = 0, uint32_t v2 = 0, uint32_t v3 = 0, uint32_t v4 = 0, uint32_t v5 = 0);


		// Meta data retrieval --------------------------------

		/// Send a request for a list update to the server. The results are delivered using the callback set in `setListResultsCallback()`.
		/// \param itemsType The type of thing to list. Supported types are local variables (`Enums::LookupItemType::LocalVariable`, default), subscribed Data Requests (`Enums::LookupItemType::DataRequest`), and Registered Events (`Enums::LookupItemType::RegisteredEvent`).
		/// \return  `S_OK` on success, `E_INVALIDARG` if the item type is not supported, `E_NOT_CONNECTED` if not connected to server.
		/// \note The list result callback is invoked from a new thread which delivers the results (\refwcc{ListResult} structure). Also check the `ListResult::result` HRESULT return code to be sure the list command completed successfully
		/// (which may be `S_OK`, `E_FAIL` if server returned `Nak`, or `E_TIMEOUT` if the list request did not complete (results may be empty or partial)).
		/// \sa \refwce{CommandId::List}
		HRESULT list(WSE::LookupItemType itemsType = WSE::LookupItemType::LocalVariable);

		/// Request server-side lookup of an named item to find the corresponding numeric ID.
		/// \param itemType The type of item to look up. A type of variable or a measurement unit. See the `WASimCommander::LookupItemType` documentation for details.
		/// \param itemName The name of the thing to check for.
		/// \param piResult Pointer to 32-bit signed integer variable to hold the result.
		/// \return `S_OK` on success, `E_FAIL` if server returns a Nak response (typically means the item name wasn't found), `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa defaultTimeout(), setDefaultTimeout()
		HRESULT lookup(WSE::LookupItemType itemType, const std::string &itemName, int32_t *piResult);

		/// \}
		/// \name Low level API
		/// \{

		/// Sends a command, in the form of a `WASimCommander::Command` structure, to the server for processing. The various command types and the data requirements for each are described in the `WASimCommander::Enums::CommandId` documentation.
		/// To receive command responses from the server, set a callback with `setCommandResultCallback()`, then check the results for a `Command::token` which matches the `token` set in the command you're sending here.
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure.
		HRESULT sendCommand(const Command &command) const;
		/// Sends a command, in the form of a `WASimCommander::Command` structure, to the server for processing and waits for a reply (an `Ack/Nak` response `Command`).
		///  The various command types and the data requirements for each are described in the `WASimCommander::Enums::CommandId` documentation.
		/// \param command The `Command` struct defining the command and associated data to send.
		/// \param response Pointer to an initialised `Command` structure for storing the resulting response (a Command with a `commandId` of `Ack` or `Nak`), if any.
		/// \param timeout The maximum time to wait for a response, in milliseconds. If `0` (default) then the default network timeout value is used (`defaultTimeout()`, `setDefaultTimeout()`).
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure, or possibly `E_FAIL` on unknown error (check log for details).
		HRESULT sendCommandWithResponse(const Command &command, Command *response, uint32_t timeout = 0);

		/// \}
		/// \name  Logging settings
		/// \{

		/// Get the current minimum logging severity level for the specified `facility` and `source`.  \sa setLogLevel(), setLogCallback(), \refwce{CommandId::Log}
		/// \param facility **One** of `WASimCommander::LogFacility` enum flag values. This must be only one of the available flags, not a combination.  The `Remote` facility is the one delivered via the log callback handler.
		/// \param source One of \refwcc{LogSource} enum values.
		/// \return The current `WASimCommander::LogLevel` value, or `LogLevel::None` if the parameters were ivalid or the actual level is unknown (see Note below).
		/// \note The remote server logging level for `File` and `Console` facilities is unknown at Client startup. The returned values are only going to be correct if they were set by this instance of the Client (using `setLogLevel()`).
		WSE::LogLevel logLevel(WSE::LogFacility facility, LogSource source = LogSource::Client) const;
		/// Set the current minimum logging severity level for the specified `facility` and `source` to `level`. \sa logLevel(), setLogCallback(), \refwce{CommandId::Log}
		/// \param level The new minimum level. One of `WASimCommander::LogLevel` enum values. Use `LogLevel::None` to disable logging on the given faciliity/source.
		/// \param facility One or more of `WASimCommander::LogFacility` enum flags. The `LogFacility::Remote` facility is the one delivered via the log callback handler.
		/// \param source One of \refwcc{LogSource} enum values.
		void setLogLevel(WSE::LogLevel level, WSE::LogFacility facility = WSE::LogFacility::Remote, LogSource source = LogSource::Client);

		/// \}
		/// \name Callbacks
		/// \{

		/// Sets a callback for Client event updates which indicate status changes. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setClientEventCallback(std::bind(&MyClass::onClientEvent, this, std::placeholders::_1)); \endcode \sa ClientEventType, ClientEvent, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setClientEventCallback(clientEventCallback_t cb);
		/// Same as `setClientEventCallback(clientEventCallback_t)`. Convenience for avoiding a std::bind expression.
		/// \n Usage: \code client->setClientEventCallback(&MyClass::onClientEvent, this); \endcode \sa ClientEvent, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setClientEventCallback(void(__stdcall Tcaller::* const member)(const ClientEvent &), Tcaller *const caller);

		/// Sets a callback for list results arriving from the server. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setListResultsCallback(std::bind(&MyClass::onListResult, this, std::placeholders::_1)); \endcode  \sa ListResult, list(), \refwce{CommandId::List}, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setListResultsCallback(listResultsCallback_t cb);
		/// Same as `setListResultsCallback(listResultsCallback_t)`. Convenience for avoiding a `std::bind` expression.
		/// \n Usage: \code client->setListResultsCallback(&MyClass::onListResult, this); \endcode \sa list(), \refwce{CommandId::List}, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setListResultsCallback(void(__stdcall Tcaller::* const member)(const ListResult &), Tcaller *const caller);

		/// Sets a callback for value update data arriving from the server. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setDataCallback(std::bind(&MyClass::onDataResult, this, std::placeholders::_1)); \endcode  \sa DataRequestRecord, saveDataRequest(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setDataCallback(dataCallback_t cb);
		/// Same as `setDataCallback(dataCallback_t)`. Convenience overload template for avoiding a std::bind expression.
		/// \n Usage: \code client->setDataCallback(&MyClass::onDataResult, this) \endcode \sa dataCallback_t, DataRequestRecord, saveDataRequest(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setDataCallback(void(__stdcall Tcaller::* const member)(const DataRequestRecord &), Tcaller *const caller);

		/// Sets a callback for logging activity, both from the server and the client itself. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setLogCallback(std::bind(&MyClass::onLogMessage, this, std::placeholders::_1)); \endcode  \sa setLogLevel(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setLogCallback(logCallback_t cb);
		/// Same as `setLogCallback(logCallback_t)`. Convenience template for avoiding a std::bind expression.
		/// \n Usage: \code client->setLogCallback(&MyClass::onLogMessage, this) \endcode  \sa setLogLevel(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setLogCallback(void(__stdcall Tcaller::* const member)(const LogRecord &, LogSource), Tcaller *const caller);

		/// Sets a callback for delivering command results returned by the server.  Pass a `nullptr` value to remove a previously set callback.
		/// While some of the high-level Client API calls do return an immediate status or result, many of the server commands are sent asynchronously, in a fire-and-forget fashion.
		/// If you would like to be notified about _all_ command responses, set this callback. The `Command` type delivered will have the `Command::commandId` of type
		/// `CommandId::Ack` or `CommandId::Nak`. The `Command::uData` member is set to the `commandId` of the original command being responded to.
		/// Other `Command` struct members may have other meanings, depending on the actual command being responded to. See documentation for `WASimCommander::Enums::CommandId` for details.
		/// \n Usage: \code client->setCommandResultCallback(std::bind(&MyClass::onCommandResult, this, std::placeholders::_1)); \endcode  \sa \refwce{CommandId}, WASimCommander::Command, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setCommandResultCallback(commandCallback_t cb);
		/// Same as `setCommandResultCallback(commandCallback_t)`. Convenience overload template for avoiding a std::bind expression.
		/// \n Usage: \code client->setCommandResultCallback(&MyClass::onCommandResult, this); \endcode \sa setCommandResultCallback(commandCallback_t), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setCommandResultCallback(void(__stdcall Tcaller::* const member)(const Command &), Tcaller *const caller);

		/// Sets a callback for delivering response commands sent to this client by the server module. Note that the server may also initiate a few types of commands (not in response to client commands)
		/// such as `Ping`, `List`, and `Disconnect`. In contrast to `setCommandResultCallback(commandCallback_t)`, this one will report on _all_ commands from the server, not just `Ack/Nak`.
		/// This callback is meant for low-level API usage.
		/// \n Usage: \code client->setResponseCallback(std::bind(&MyClass::onServerResponse, this, std::placeholders::_1)); \endcode  \sa sendServerCommand(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setResponseCallback(commandCallback_t cb);
		/// Same as `setResponseCallback(commandCallback_t)`. Convenience overload template for avoiding a std::bind expression.
		/// \n Usage: \code client->setResponseCallback(&MyClass::onServerResponse, this); \endcode \sa setResponseCallback(responseCallback_t), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setResponseCallback(void(__stdcall Tcaller::* const member)(const Command &), Tcaller *const caller);

		/// \}

	private:
		class Private;
#pragma warning( suppress: 4251 )  // "warning C4251: ... needs to have dll-interface to be used by clients" -- no client access here, safe to suppress
		const std::unique_ptr<Private> d;
#ifndef DOXYGEN  // insists on documenting this
		friend class Private;
#endif

		// we do not share well with others
		WASimClient(WASimClient&&) = delete;
		WASimClient(const WASimClient &) = delete;
		WASimClient &operator=(WASimClient&&) = delete;
		WASimClient &operator=(const WASimClient &) = delete;

	};  // WASimClient

	template<class Tcaller>
	inline void WASimClient::setClientEventCallback(void(__stdcall Tcaller::* const member)(const ClientEvent &), Tcaller * const caller)
	{
		setClientEventCallback(std::bind(member, caller, std::placeholders::_1));
	}

	template<class Tcaller>
	inline void WASimClient::setListResultsCallback(void(__stdcall Tcaller::* const member)(const ListResult &), Tcaller * const caller)
	{
		setListResultsCallback(std::bind(member, caller, std::placeholders::_1));
	}

	template<class Tcaller>
	inline void WASimClient::setDataCallback(void(__stdcall Tcaller::* const member)(const DataRequestRecord &), Tcaller * const caller)
	{
		setDataCallback(std::bind(member, caller, std::placeholders::_1));
	}

	template<class Tcaller>
	inline void WASimClient::setLogCallback(void(__stdcall Tcaller::* const member)(const LogRecord &, LogSource), Tcaller * const caller)
	{
		setLogCallback(std::bind(member, caller, std::placeholders::_1, std::placeholders::_2));
	}

	template<class Tcaller>
	inline void WASimClient::setCommandResultCallback(void(__stdcall Tcaller::* const member)(const Command &), Tcaller * const caller)
	{
		setCommandResultCallback(std::bind(member, caller, std::placeholders::_1));
	}

	template<class Tcaller>
	inline void WASimClient::setResponseCallback(void(__stdcall Tcaller::* const member)(const Command &), Tcaller * const caller)
	{
		setResponseCallback(std::bind(member, caller, std::placeholders::_1));
	}

#undef WSE
};  // namespace WASimCommander::Client
