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

#include "client/enums.h"

//----------------------------------------------------------------------------
//        Client-side struct definitions, used in WASimClient API.
//----------------------------------------------------------------------------

namespace WASimCommander::Client
{

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

	WASimCommander::Enums::LookupItemType listType;  ///< the type of items being listed
	HRESULT result;           ///< Execution result, one of: `S_OK`, `E_FAIL`, `E_TIMEOUT`
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
	using DataRequest::operator=;     ///< Inherits DataRequest assignment operators.
	DataRequestRecord();              ///< Default c'tor creates an invalid instance with request ID of -1, zero size, and `RequestType::None`.
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
	uint8_t simVarIndex = 0;       ///< Optional index number for SimVars ('A') which require them. If using named variables, the index can also be included in the variable name string (after a colon `:`, as would be used in a calculator string).
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

};  // namespace WASimCommander::Client
