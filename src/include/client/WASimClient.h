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
#include "client/structs.h"

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

/// Starting ID range for "Custom Key Events" for use with `registerCustomKeyEvent()` generated IDs.
/// (This corresponds to the value of `1 + THIRD_PARTY_EVENT_ID_MAX` constant from SimConnect SDK header file 'MSFS/Legacy/gauges.h'.)
static const uint32_t CUSTOM_KEY_EVENT_ID_MIN = 0x00020000;


//----------------------------------------------------------------------------
//       Callback Types
//----------------------------------------------------------------------------

	using clientEventCallback_t = std::function<void __stdcall(const ClientEvent &)>;   ///< Callback function for Client events. \sa WASimClient::setClientEventCallback()
	using listResultsCallback_t = std::function<void __stdcall(const ListResult &)>;    ///< Callback function for delivering list results, eg. of local variables sent from Server. \sa WASimClient::setListResultsCallback()
	using dataCallback_t = std::function<void __stdcall(const DataRequestRecord &)>;    ///< Callback function for subscription result data. \sa WASimClient::setDataCallback()
	using logCallback_t = std::function<void __stdcall(const LogRecord &, LogSource)>;  ///< Callback function for log entries (from both Client and Server). \sa WASimClient::setLogCallback()
	using commandCallback_t = std::function<void __stdcall(const Command &)>;           ///< Callback function for commands sent from server. \sa WASimClient::setCommandResultCallback(), WASimClient::setResponseCallback()


// -------------------------------------------------------------
//       WASimClient
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

		/// Initialize the simulator network link and set up minimum necessary for WASimCommander server ping or connection. Uses default network SimConnect configuration ID.
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
		///< Set the default timeout period for server responses. The default may be inadequate on slow network links or a very busy simulator. \sa defaultTimeout()
		void setDefaultTimeout(uint32_t ms);

		/// SimConnect is used for the network layer. This setting specifies the SimConnect.cfg index to use. The value of -1 forces a local connection.
		/// The initial default setting is read from the `client_conf.ini` file or set to `-1` if no config file was found. \sa setNetworkConfigurationId().
		int networkConfigurationId() const;
		/// SimConnect is used for the network layer. This setting specifies the SimConnect.cfg index to use, or -1 to force a local connection.
		/// Note that this must be called before `connectSimulator()` invocation in order to have any effect. \sa networkConfigurationId().
		void setNetworkConfigurationId(int configId);

		/// \}
		/// \name RPN calculator code execution and reusable events
		/// \{

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
		/// See `saveDataRequest()` and `registerEvent()` respectively for details.
		/// \sa \refwce{CommandId::Exec}, defaultTimeout(), setDefaultTimeout()
		HRESULT executeCalculatorCode(const std::string &code, WASimCommander::Enums::CalcResultType resultType = WASimCommander::Enums::CalcResultType::None, double *pfResult = nullptr, std::string *psResult = nullptr) const;

		/// \}
		/// \name Variables accessor methods
		/// \{

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

		/// Set a Variable value by name, with optional named unit type. Although any settable variable type can set this way, it is primarily useful for local (`L`)
		/// variables which can be set via dedicated _Gauge API_ functions (`set_named_variable_value()` and `set_named_variable_typed_value()`). \n\n
		/// Other variables types can also be set this way but such requests are simply converted to a calculator string and
		/// evaluated via the _Gauge API_ `execute_calculator_code()`. Using `WASimClient::executeCalculatorCode()` directly may be more efficient. \n
		/// The following conditions must be observed:
		/// - The variable type in `VariableRequest::variableType` must be "settable" ('A', 'C', 'H', 'K', 'L', or 'Z'), otherwise an `E_INVALIDARG` result is returned.
		/// - Setting an 'A' type variable this way _requires_ the actual variable name in `VariableRequest::variableName` -- using just an ID returns `E_INVALIDARG`. \n
		///   (Other settable variable types don't have any associated ID anyway, so this is not an issue.)
		/// - For any variable type _other than_ 'L', a Unit can only be specified as a string (in `VariableRequest::unitName`), not an ID.
		///   Using only an ID will not cause an error, but the unit will not be included in the generated RPN code. \n
		///   For 'L' variable types, if both a name and ID are provided, the numeric ID is used insted of the name (this avoids a lookup on the server side).
		///
		/// \param variable See `VariableRequest` documentation for descriptions of the individual fields.
		/// \param value The numeric value to set.
		/// \return `S_OK` on success, `E_INVALIDARG` on parameter validation errors, `E_NOT_CONNECTED` if not connected to server, or `E_FAIL` on general failure (unlikely).
		/// \sa \refwce{CommandId::Set}
		HRESULT setVariable(const VariableRequest &variable, const double value);
		/// This is an overloaded method. This version allows setting an 'A' type (SimVar) variable to a string value. **Only 'A' type variables can be set this way.** \n
		/// Since there is actually no direct way to set string-type values from WASM code, this is just a conveneince method and simply invokes SimConnect to do the work.
		/// On first use with a new variable name it will set up a mapping of the name to an internally-assigned ID (calling `SimConnect_AddToDataDefinition()`) and cache that mapping.
		/// Then on subsequent invocations on the same variable the mapped ID will be used directly. The mappings are invalidated when disconnecting from the simulator.
		HRESULT setVariable(const VariableRequest &variable, const std::string &stringValue);

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

		/// Sets a numeric value on an 'A' (aka "SimVar" / "Simulator Variable") type variable. \n
		/// This is a convenience version of `setVariable()`, equivalent to `setVariable(VariableRequest(variableName, unitName), value)`. See `setVariable()` and `VariableRequest` for details.\n
		/// Note that `variableName` can optionally contain an index after a colon (eg. `VAR NAME:1`), or the `setSimVarVariable(const string& name, uint8_t index, const string& unit, double value)`
		/// overload could be used to provide the index separately.
		/// \since v1.3.0
		inline HRESULT setSimVarVariable(const std::string &variableName, const std::string &unitName, double value) { return setVariable(VariableRequest(variableName, unitName), value); }
		/// Sets a numeric value on an indexed 'A' (aka "SimVar" / "Simulator Variable") type variable. \n
		/// This is a convenience version of `setVariable()`, equivalent to `setVariable(VariableRequest(variableName, unitName, simVarIndex), value)`. See `setVariable()` and `VariableRequest` for details.
		/// \since v1.3.0
		inline HRESULT setSimVarVariable(const std::string &variableName, uint8_t simVarIndex, const std::string &unitName, double value) { return setVariable(VariableRequest(variableName, unitName, simVarIndex), value); }
		/// Sets a string value on an 'A' (aka "SimVar" / "Simulator Variable") type variable. \n
		/// This is a convenience version of `setVariable()`, equivalent to `setVariable(VariableRequest('A', variableName), stringValue)`. See `setVariable(const VariableRequest &, const std::string &)` for details.
		/// \since v1.3.0
		inline HRESULT setSimVarVariable(const std::string &variableName, const std::string &stringValue) { return setVariable(VariableRequest('A', variableName), stringValue); }
		/// Sets a string value on an indexed 'A' (aka "SimVar" / "Simulator Variable") type variable. \n
		/// This is a convenience version of `setVariable()`, equivalent to `setVariable(VariableRequest(variableName, {}, index), stringValue)`. See `setVariable(const VariableRequest &, const std::string &)` for details.
		/// \since v1.3.0
		inline HRESULT setSimVarVariable(const std::string &variableName, uint8_t index, const std::string &stringValue) { return setVariable(VariableRequest(variableName, {}, index), stringValue); }

		/// \}
		/// \name Data change subscriptions (variables and calculated results)
		/// \{

		/// Add a new `WASimCommander::DataRequest` for a variable or calculated result, or update an existing data request with the same `DataRequest::requestId`.
		/// Data changes (updates) are delivered asynchronously via the callback function set with `setDataCallback()`, which then passes a `DataRequestRecord` structure as the callback argument
		/// (this provides both a reference to the original `DataRequest` registered here, as well as result data).
		/// \param request The `WASimCommander::DataRequest` structure to process. See `WASimCommander::DataRequest` documentation for details of the structure members.
		/// \param async Set to `false` (default) to wait for an `Ack`/`Nak` response from the server before returning from this method, or `true` to return without waiting for a response. See return values and the Note below for more details.
		/// \return `S_OK` on success, `E_INVALIDARG` if there is a problem with the `DataRequest` contents. \n
		/// If currently connected to the server and `async` is `false`, may also return `E_FAIL` if the server returned `Nak` response, or `E_TIMEOUT` on general server communication failure.
		/// \note If currently connected to the server and the `async` param is `false`, this method will block until either the Server responds or the timeout has expired (see `defaultTimeout()`).
		/// If the client is _not_ currently connected to the server, the request is queued until the next connection is established (and this method is non-blocking regardless of `async` argument).
		/// \par Tracking async calls
		/// To track the status of an async request, set a callback function with `setCommandResultCallback()`. The server should respond with an \refwce{CommandId::Ack} or \refwce{CommandId::Nak}
		/// \refwc{Command} where the `uData` value is \refwce{CommandId::Subscribe} and the \refwc{Command::token} will be the `requestId` value from the given `request` struct.
		/// \sa  \refwc{DataRequest} \refwce{CommandId::Subscribe}, removeDataRequest(), updateDataRequest(), setDataCallback(), DataRequestRecord
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

		/// \}
		/// \name RPN calculator code execution and reusable events
		/// \{

		/// Register a reusable event which executes a pre-set RPN calculator code string. The code is pre-compiled and stored on the server for quicker execution.
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

		/// \}
		/// \name Simulator Key Events
		/// \{

		/// Can be used to trigger standard Simulator "Key Events" as well as "custom" _Gauge API/SimConnect_ events. Up to 5 optional values can be passed onto the event handler.
		/// This provides functionality similar to the _Gauge API_ function `trigger_key_event_EX1()` and `SimConnect_TransmitClientEvent[_EX1()]`.  \n\n
		/// *Standard* Key Event IDs can be found in the SimConnect SDK header file 'MSFS/Legacy/gauges.h' in the form of `KEY_*` macro values, and event names can also be
		/// resolved to IDs programmatically using the `lookup()` method. No preliminary setup is required to trigger these events, but a full connection to WASimModule ("Server") is needed.
		/// These are triggered on the simulator side using `trigger_key_event_EX1()` function calls. \n\n
		/// *Custom Events* for which a numeric ID is already known (typically in the _Gauge API_ `THIRD_PARTY_EVENT_ID_MIN`/`THIRD_PARTY_EVENT_ID_MAX` ID range)
		/// can also be triggered directly as with standard events. These types of events are also passed directly to `trigger_key_event_EX1()`. \n\n
		/// *Named Custom Events*, for which an ID is not known or predefined, **must** first be registered with `registerCustomKeyEvent()`, which creates and maps (and optionally returns)
		/// a unique ID corresponding to the custom event name. An active simulator (SimConnect) connection is required to trigger these types of events.
		/// They are invoked via `SimConnect_TransmitClientEvent[_EX1()]` method directly from this client (this is actually just a convenience for the WASimClient user to avoid needing a separate SimConnect session).
		/// Which actual SimConnect function is used depends on how the custom event was registered (default is to use the newer "_EX1" version which allows up to 5 event values). \n
		/// See docs for `registerCustomKeyEvent()` for further details on using custom simulator events.
		/// \param keyEventId Numeric ID of the Event to trigger.
		/// \param v1,v2,v3,v4,v5 Optional values to pass to the event handler. Defaults are all zeros.
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected (server or sim, see above), `E_TIMEOUT` on server communication failure, or `E_FAIL` on unexpected SimConnect error.
		/// \note For Key Events triggered via `trigger_key_event_EX1()`, Server responds asynchronously with an Ack/Nak response to \refwce{CommandId::SendKey} command type;
		///   A 'Nak' means the event ID is clearly not valid (eg. zero), but otherwise the simulator provides no feedback about event execution
		///   (from [their docs](https://docs.flightsimulator.com/html/Programming_Tools/WASM/Gauge_API/trigger_key_event_EX1.htm#return_values): "If the event requested is not appropriate, it will simply not happen."). \n\n
		///   For _custom named_ events, triggered via `SimConnect_TransmitClientEvent[_EX1()]`, SimConnect may asynchronously send EXCEPTION type response messages if the ID isn't valid
		///   (likely because the event hasn't been successfully registered with `registerCustomKeyEvent()`). These messages are passed through to WASimClient's logging facilities at the `Warning` level.
		///   But again there is no actual confirmation that the event is going to do anything.
		/// \since v1.3.0 - Added ability to trigger custom named events.
		HRESULT sendKeyEvent(uint32_t keyEventId, uint32_t v1 = 0, uint32_t v2 = 0, uint32_t v3 = 0, uint32_t v4 = 0, uint32_t v5 = 0) const;

		/// This is an overloaded method. See `sendKeyEvent(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) const` for main details.
		/// This version allows triggering Key Events by name instead of an ID. \n\n
		/// All simulator event names must be resolved, or mapped, to a numeric ID before they can be triggered (used).
		/// The first time this method is invoked with a particular event name, it tries to associate that name with an ID (based on the event name, as described below).
		/// If successful, the ID is cached, so subsequent calls to this method, with the same event name, will use the cached ID instead of trying to resolve the name again.
		/// - For _standard_ Key Events, the name is resolved to an ID using the 'lookup()' method and the resulting ID (if valid) is cached for future uses. There **must** be an active server connection for this to work.
		/// - For _custom_ events (names that contain a "." (period) or start with a "#"), the event is first registered using `registerCustomKeyEvent()` and the resulting ID is cached if that succeeds.
		///
		/// \param keyEventName Name of the Event to trigger.
		/// \param v1,v2,v3,v4,v5 Optional values to pass to the event handler. Defaults are all zeros.
		/// \return `S_OK` on success, `E_INVALIDARG` if event name could not be resolved to an ID, `E_NOT_CONNECTED` if not connected (server or sim, depending on event type),
		/// `E_TIMEOUT` on server communication failure, or `E_FAIL` on unexpected SimConnect error.
		/// \note The name-to-ID cache is kept as a simple `std::unordered_map` type, so if you have a better way to save the event IDs from `lookup()` or `registerCustomKeyEvent()`,
		/// use that instead, and call the more efficient `sendKeyEvent(eventId, ...)` overload directly.
		/// \since v1.3.0 - Added ability to trigger custom named events.
		HRESULT sendKeyEvent(const std::string &keyEventName, uint32_t v1 = 0, uint32_t v2 = 0, uint32_t v3 = 0, uint32_t v4 = 0, uint32_t v5 = 0);

		/// Register a "Custom Simulator [Key] Event" by providing an event name. The method optionally returns the generated event ID, which can later be used with `sendKeyEvent()` method instead of the event name.
		/// It can also be used to look up a previous registration's ID if the event name has already been registered. \n\n
		/// Custom event names are mapped to internally-generated unique IDs using a standard SimConnect call to
		/// [`MapClientEventToSimEvent`](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Events_And_Data/SimConnect_MapClientEventToSimEvent.htm#parameters),
		/// which briefly describes custom event usage and name syntax in the `EventName` parameter description. This method serves a similar purpose (and in fact eventually calls that same SimConnect function). \n\n
		/// The mappings must be re-established every time a new connection with SimConnect is made, which WASimClient takes care of automatically. If currently connected to the simulator, the event is immediately mapped,
		/// otherwise it will be mapped upon the next connection. An event registration can be removed with `removeCustomKeyEvent()` which will prevent any SimConnect mapping from being created upon the _next_ connection. \n\n
		/// Note that the custom event mapping/triggering feature is actually just a convenience for the WASimClient user and doesn't involve the usual Server interactions (WASimModule) at all. \n
		/// \param customEventName Name of the Event to register. The event name _must_ contain a "." (period) or start with a "#", otherwise an `E_INVALIDARG` result is returned. \n
		///   If an event with the same name has already been registered, the method returns `S_OK` and no further actions are performed (besides setting the optional `puiCustomEventId` pointer value, see below).
		/// \param puiCustomEventId Optional pointer to 32-bit unsigned integer variable to return the generated event ID. This ID can be used to trigger the event later using `sendKeyEvent()` (which is more efficient than using the event name each time).
		///   The ID will always be unique per given event name, and is always equal to or greater than the `Client::CUSTOM_KEY_EVENT_ID_MIN` constant value. \n
		///   The pointer's value will be populated even if the event name was already registered (with the result of the previously generated ID).
		/// \param useLegacyTransmit Optional, default `false`. Boolean value indicating that the deprecated `SimConnect_TransmitClientEvent()` function should be used to trigger the event instead of the newer `SimConnect_TransmitClientEvent_EX1()`.
		///   This may be necessary to support models which haven't updated to the newer version of the event handler. Note that the old `TransmitClientEvent()` function only supports sending 1 event value (vs. 5 for the "_EX1" version). \n
		///   To re-register the same event name but with a different value for `useLegacyTransmit` parameter, first remove the initial registration with `removeCustomKeyEvent()` and then call this method again.
		/// \return `S_OK` on success, `E_INVALIDARG` if the event name is invalid.
		/// \since v1.3.0
		HRESULT registerCustomKeyEvent(const std::string &customEventName, uint32_t *puiCustomEventId = nullptr, bool useLegacyTransmit = false);
		/// Remove a Custom Event previously registered with `registerCustomEvent()` method using the event's name.
		/// This will prevent the custom event from being mapped _next_ time the client connects to SimConnect.
		/// \note SimConnect provides no way to remove a registered Custom event. Any active SimConnect mapping will remain in effect until SimConnect is disconnected (and can still be invoked with the corresponding ID, but not by name).
		/// \param customEventName full name of the previously registered event. Must be the same name as used with `registerCustomKeyEvent()`.
		/// \return `S_OK` on success, `E_INVALIDARG` if the eventId wasn't found.
		/// \since v1.3.0
		HRESULT removeCustomKeyEvent(const std::string &customEventName);
		/// This is an overloaded method. Same as `removeCustomKeyEvent(const string &)` but the event can be specified using the associated numeric event ID (originally returned from `registerCustomKeyEvent()`) instead of the name.
		/// \param eventId ID of the previously registered event.
		/// \return `S_OK` on success, `E_INVALIDARG` if the eventId wasn't found.
		/// \since v1.3.0
		HRESULT removeCustomKeyEvent(uint32_t eventId);

		/// \}
		/// \name Metadata retrieval
		/// \{

		/// Send a request for a list update to the server. The results are delivered using the callback set in `setListResultsCallback()`.
		/// \param itemsType The type of thing to list. Supported types are local variables (`Enums::LookupItemType::LocalVariable`, default), subscribed Data Requests (`Enums::LookupItemType::DataRequest`), and Registered Events (`Enums::LookupItemType::RegisteredEvent`).
		/// \return  `S_OK` on success, `E_INVALIDARG` if the item type is not supported, `E_NOT_CONNECTED` if not connected to server.
		/// \note The list result callback is invoked from a new thread which delivers the results (\refwcc{ListResult} structure). Also check the `ListResult::result` HRESULT return code to be sure the list command completed successfully
		/// (which may be `S_OK`, `E_FAIL` if server returned `Nak`, or `E_TIMEOUT` if the list request did not complete (results may be empty or partial)).
		/// \sa \refwce{CommandId::List}
		HRESULT list(WASimCommander::Enums::LookupItemType itemsType = WASimCommander::Enums::LookupItemType::LocalVariable);

		/// Request server-side lookup of an named item to find the corresponding numeric ID.
		/// \param itemType The type of item to look up. A type of variable or a measurement unit. See the `WASimCommander::LookupItemType` documentation for details.
		/// \param itemName The name of the thing to check for.
		/// \param piResult Pointer to 32-bit signed integer variable to hold the result.
		/// \return `S_OK` on success, `E_FAIL` if server returns a Nak response (typically means the item name wasn't found), `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure.
		/// \note This method blocks until either the Server responds or the timeout has expired. \sa defaultTimeout(), setDefaultTimeout()
		HRESULT lookup(WASimCommander::Enums::LookupItemType itemType, const std::string &itemName, int32_t *piResult);

		/// \}
		/// \name Low level API
		/// \{

		/// Sends a command, in the form of a `WASimCommander::Command` structure, to the server for processing. The various command types and the data requirements for each are described in the `WASimCommander::Enums::CommandId` documentation.
		/// To receive command responses from the server, set a callback with `setCommandResultCallback()`, then check the results for a `Command::token` which matches the `token` set in the command you're sending here.
		/// \return `S_OK` on success, `E_NOT_CONNECTED` if not connected to server, `E_TIMEOUT` on server communication failure.
		HRESULT sendCommand(const Command &command) const;
		/// Sends a command, in the form of a `WASimCommander::Command` structure, to the server for processing and waits for a reply (an `Ack/Nak` response `Command`).
		/// The various command types and the data requirements for each are described in the `WASimCommander::Enums::CommandId` documentation.
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
		WASimCommander::Enums::LogLevel logLevel(WASimCommander::Enums::LogFacility facility, LogSource source = LogSource::Client) const;
		/// Set the current minimum logging severity level for the specified `facility` and `source` to `level`. \sa logLevel(), setLogCallback(), \refwce{CommandId::Log}
		/// \param level The new minimum level. One of `WASimCommander::LogLevel` enum values. Use `LogLevel::None` to disable logging on the given faciliity/source.
		/// \param facility One or more of `WASimCommander::LogFacility` enum flags. The `LogFacility::Remote` facility is the one delivered via the log callback handler.
		/// \param source One of \refwcc{LogSource} enum values.
		void setLogLevel(WASimCommander::Enums::LogLevel level, WASimCommander::Enums::LogFacility facility = WASimCommander::Enums::LogFacility::Remote, LogSource source = LogSource::Client);

		/// \}
		/// \name Callbacks
		/// \note In general, callbacks _may_ be invoked concurrently (and possibly from different threads).
		/// The callback handler functions should at least be reentrant (if not thread-safe) since they could be called at any time.
		/// Check the individual method documentation for more details about possible concurrency and threading. \n
		/// The client can be built with \ref WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS macro/definition set to `0` to disable concurrency.
		///
		/// <hr>
		/// \{

		/// Sets a callback for Client event updates which indicate status changes. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setClientEventCallback(std::bind(&MyClass::onClientEvent, this, std::placeholders::_1)); \endcode
		/// This callback may be invoked from the main thread (where WASimClient was created), the dedicated "dispatch" thread the client maintains, or a temporary thread in one specific case when SimConnect sends a "Quit" command.
		/// \sa ClientEventType, ClientEvent, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setClientEventCallback(clientEventCallback_t cb);
		/// Same as `setClientEventCallback(clientEventCallback_t)`. Convenience for avoiding a std::bind expression.
		/// \n Usage: \code client->setClientEventCallback(&MyClass::onClientEvent, this); \endcode \sa ClientEvent, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setClientEventCallback(void(__stdcall Tcaller::* const member)(const ClientEvent &), Tcaller *const caller);

		/// Sets a callback for list results arriving from the server. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setListResultsCallback(std::bind(&MyClass::onListResult, this, std::placeholders::_1)); \endcode
		/// This callback is invoked from temporary thread which accumulates incoming list results until the listing is complete (or times out).
		/// Currently, only one pending list request can be active at any time, though this may change in the future.
		/// \sa ListResult, list(), \refwce{CommandId::List}, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setListResultsCallback(listResultsCallback_t cb);
		/// Same as `setListResultsCallback(listResultsCallback_t)`. Convenience for avoiding a `std::bind` expression.
		/// \n Usage: \code client->setListResultsCallback(&MyClass::onListResult, this); \endcode \sa list(), \refwce{CommandId::List}, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setListResultsCallback(void(__stdcall Tcaller::* const member)(const ListResult &), Tcaller *const caller);

		/// Sets a callback for value update data arriving from the server. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setDataCallback(std::bind(&MyClass::onDataResult, this, std::placeholders::_1)); \endcode
		/// This callback is invoked from the dedicated "dispatch" thread the client maintains.
		/// \sa DataRequestRecord, saveDataRequest(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setDataCallback(dataCallback_t cb);
		/// Same as `setDataCallback(dataCallback_t)`. Convenience overload template for avoiding a std::bind expression.
		/// \n Usage: \code client->setDataCallback(&MyClass::onDataResult, this) \endcode \sa dataCallback_t, DataRequestRecord, saveDataRequest(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setDataCallback(void(__stdcall Tcaller::* const member)(const DataRequestRecord &), Tcaller *const caller);

		/// Sets a callback for logging activity, both from the server and the client itself. Pass a `nullptr` value to remove a previously set callback.
		/// \n Usage: \code client->setLogCallback(std::bind(&MyClass::onLogMessage, this, std::placeholders::_1)); \endcode
		/// This callback may be invoked from either the main thread (where WASimClient was created), or the dedicated "dispatch" thread which the client maintains.
		/// \sa setLogLevel(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
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
		/// \n Usage: \code client->setCommandResultCallback(std::bind(&MyClass::onCommandResult, this, std::placeholders::_1)); \endcode
		/// This callback is invoked from the dedicated "dispatch" thread the client maintains.
		/// \sa \refwce{CommandId}, WASimCommander::Command, WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		void setCommandResultCallback(commandCallback_t cb);
		/// Same as `setCommandResultCallback(commandCallback_t)`. Convenience overload template for avoiding a std::bind expression.
		/// \n Usage: \code client->setCommandResultCallback(&MyClass::onCommandResult, this); \endcode \sa setCommandResultCallback(commandCallback_t), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
		template<class Tcaller>
		inline void setCommandResultCallback(void(__stdcall Tcaller::* const member)(const Command &), Tcaller *const caller);

		/// Sets a callback for delivering response commands sent to this client by the server module. Note that the server may also initiate a few types of commands (not in response to client commands)
		/// such as `Ping`, `List`, and `Disconnect`. In contrast to `setCommandResultCallback(commandCallback_t)`, this one will report on _all_ commands from the server, not just `Ack/Nak`.
		/// This callback is meant for low-level API usage.
		/// \n Usage: \code client->setResponseCallback(std::bind(&MyClass::onServerResponse, this, std::placeholders::_1)); \endcode
		/// This callback is invoked from the dedicated "dispatch" thread the client maintains.
		/// \sa sendServerCommand(), WSMCMND_CLIENT_USE_CONCURRENT_CALLBACKS
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

};  // namespace WASimCommander::Client
