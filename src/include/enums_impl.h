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

// DO NOT USE THIS FILE DIRECTLY -- NO INCLUDE GUARD
// Use enums.h

#include <cstdint>
#include <vector>

#ifndef WSMCMND_ENUM_EXPORT
	#define WSMCMND_ENUM_EXPORT
	#define WSMCMND_ENUM_NAMESPACE WASimCommander::Enums
#endif

/// WASimCommander::Enums namespace. Contains all enum definitions for the base API.
namespace WSMCMND_ENUM_NAMESPACE
{

	/// Commands for client-server interaction. Both sides can send commands via dedicated channels by writing a \refwc{Command} data structure.
	/// The fields `uData`, `sData` and `fData` referenced below refer to \refwc{Command::uData}, \refwc{Command::sData} and \refwc{Command::fData} respectively.
	/// \sa Command struct.
	WSMCMND_ENUM_EXPORT enum class CommandId : uint8_t
	{
		None = 0,     ///< An invalid command.
		Ack,          ///< Last command acknowledge. `CommandId` of the original command (which succeeded) is sent in `uData`. The `token` value from the original command is also sent back in the `token` member.
		Nak,          ///< Last command failure. `CommandId` of the original command (which failed) is sent in `uData`. `sData` _may_ contain a reason for failure. The `token` value from the original command is also sent back in the `token` member.
		Ping,         ///< Query for a response from remote server/client. The remote should respond with an `Ack` command.
		Connect,      ///< Reconnect a previously-established client (same as "WASimCommander.Connect" custom event). This CommandId is also sent back in an Ack/Nak response after a client connects (or tries to). In this case the `token` of the Ack/Nak is the client ID.
		Disconnect,   ///< Stop data updates for this client. Use the `Connect` command to resume updates. The server may also spontaneously send a Disconnect command in case it is shuttong down or otherwise terminating connections.
		List,         ///< Request a listing of items like local variables. `uData` should be one of `WASimCommander::LookupItemType` enum values (Sim and Token vars currently cannot be listed).
		              ///  List is returned as a series of `List` type response commands with `sData` as var name and `uData` is var ID, followed by an `Ack` at the end. A `Nak` response is returned if the item type cannot be listed for any reason.
		Lookup,       ///< Get information about an item, such as the ID of a variable or unit name. `uData` should be one of `WASimCommander::LookupItemType` enum values and `sData` is the name of the thing to look up.
		              ///  `Ack` is returned on success with the ID of the variable/unit in the `fData` member (as an `INT32`) and the original requested string name echoed back in `sData`. In case of lookup failure, a `Nak` reponse is returned with possible reason in `sData`.
		Get,          ///< Get a named variable value with optional unit type. `uData` is a char of the variable type, eg. 'L' for local, 'A' for SimVar, or 'T' for Token. Only 'L', 'A' and 'E' types support unit specifiers.\n
		              ///  `sData` is the variable name or numeric ID, optionally followed by comma (`,`) and unit name or numeric ID (**no spaces**). For indexed SimVars, include the index after the variable name, separated by a colon (`:`).\n
		              ///  For example, a SimVar: ```uData = 'A'; sData = "PROP BETA:2,degrees";``` \n
		              ///  Other variables types can also be requested ('B', 'E', 'M', etc) but such requests are simply converted to a calculator string and processed as an `Exec` type command (using an `Exec` command directly may be more efficient).\n
		              ///  Result is returned with the `Ack` response in `fData` as a double-precision value. In case of failure a `Nak` is returned with possible error message in `sData`.
		GetCreate,    ///< Same as `Get` but creates a local 'L' variable if it doesn't already exist (with `register_named_variable()` _Gauge API_). Use `Lookup` command to check if a variable exists.
		              ///  \n **Since v1.2:** If a variable is created, the value provided in `fData` will be used as the initial value of the variable, and will be returned as the result
		              /// (essentially providing a default value in this case). Previous versions would _not_ set a value (or unit type) on the variable after creating it and would return the default of `0.0`. \n
		              ///  **Creating variables only works with `L` (local) types.** Since v1.2, for all other types this command will be handled the same as `Get`. Previous versions would return a `Nak`.
		Set,          ///< Set a named local variable with optional unit type. `uData` is a char of the variable type, with default of 'L' for local vars.
		              ///  `sData` is the variable name or numeric ID (for local vars only), optionally followed by comma (`,`) and unit name (or numeric unit ID for local vars) (**no spaces**). The value to set is passed in `fData` member.\n
		              ///  For example, a SimVar: ```uData = 'A'; sData = "PROP RPM:2,rpm"; fData = 2200;```\n
		              ///  Other variables types can also be set this way ('A', 'H", 'K', etc) but such requests are simply converted to a calculator string and processed as an `Exec` type command (using an `Exec` command directly may be slightly more efficient).
		SetCreate,    ///< Same as `Set` but creates a local 'L' variable if it doesn't already exist (with `register_named_variable()` _Gauge API_). Use the `Lookup` command to check if a variable exists. \n
		              ///  **Creating variables only works with `L` (local) types.** Since v1.2, for all other types this command will be handled the same as `Get`. Previous versions would return a `Nak`.
		Exec,         ///< Run calculator code contained in `sData` with `WASimCommander::CalcResultType` in `uData`. Result, if any, is returned with the `Ack` response, numeric types in `fData` and strings in `sData`.
		              ///  (Due to the way the _Gauge API_ calculator function works, a string result is typically also returned even when only a numeric result is requested.)\n\n
		              ///  In case of failure a `Nak` is returned with possible error message in `sData`. Note however that the _Gauge API_ functions often happily return a "success" status even when the actual thing you're trying to do fails. The only feedback
		              ///  in this case appears to be the MSFS Console window available from "Dev Tools," which will (usually) log any actual errors.
		Register,     ///< Register a named Exec-type event. `uData` is a unique event ID, and `sData` a calculator code string. Optional event name in `sData` before a `$` separator (maximum length of \refwc{STRSZ_ENAME}). Default is to use event ID as string.\n
		              ///  If the custom event name contains a period (`.`) then it is used as-is. Otherwise "WASimCommander.[client_name]." will be prepended to the given name.\n
		              ///  Use with `SimConnect_MapClientEventToSimEvent(id, "event_name")` and `SimConnect_TransmitClientEvent()`, or the `Transmit` command (below).\n
		              ///  To change the calculator string used for an event, re-send this command with the same event ID and a new string to use. The event name cannot be modified after creation.\n
		              ///  To remove a registered event, send this command with the event ID to delete and a blank `sData`.
		Transmit,     ///< Trigger an event previously registered with the `Register` command. `uData` should be the event ID from the original registration. This is a (faster) alternative to triggering events via SimConnect mappings.
		Subscribe,    ///< Sending this command to the server with a `uData` value of `0` (zero) will suspend (pause) any and all data subscription request value updates. Updates can be resumed again by sending any non-zero value in `uData`.\n
		              ///  This command ID is also used as an `Ack/Nak` response to new Data Request records being processed. The client must write a `DataRequest` struct to the shared data area and the server should respond with this command to indicate success/failure.
		Update,       ///< Trigger data update of a previously-added Data Request, with the request ID in `uData`. This is used primarily to request updates for subscription types where the `UpdatePeriod` is `Never` or `Once`. The data is sent in the usual
		              ///  manner for subscriptions, by writing the new value to the related client data area. Note also that this command will force an upate of the data, without comparing the result to any (potentially) cached value from previous lookup.
		SendKey,      ///< `send_key_event(event_id, value)` with `event_id` in `uData` and an optional `UINT32` type `value` in `fData` (default is `0`).\n\n
		              ///  "The send_key_event function transmits a WM_COMMAND application event. This function transmits a message with the following syntax: `PostMessage(hwndMain, WM_COMMAND, event_id, (LPARAM) value);`"\n\n
		              ///  In practice this means you can send a KEY Event ID directly to the simulator, bypassing SimConnect event name mappings or calculator code evaluation. The event IDs can be found in the MSFS SDK's `MSFS/Legacy/gauges.h` header file as `KEY_*` macros,
		              ///  and are also available via the `Lookup` command. If this command is sent with `uData` == 0 and `sData` contains a string, a `LookupItemType::KeyEventId` lookup will be performed on the Key Event name first and the resulting ID (if any) used.\n\n
		              ///  Custom event IDs (registered by gauges or other modules) can also be triggered this way. There may be other uses for this command... TBD.
		Log,          ///< Set severity level for logging to the Client's `LogRecord` data area. `uData` should be one of the `WASimCommander::LogLevel` enum values. `LogLevel::None` disables logging, which is also the initial default for a newly connected Client.
		              ///  Additionally, the server-wide log levels can be set for the file and console loggers independently. To specify these levels, set `fData` to one of the `WASimCommander::LogFacility` enum values. The default of `0` assumes `LogFacility::Remote`.
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> CommandIdNames = {
		"None", "Ack", "Nak", "Ping", "Connect", "Disconnect", "List", "Lookup",
		"Get", "GetCreate", "Set", "SetCreate", "Exec", "Register", "Transmit",
		"Subscribe", "Update", "SendKey", "Log" };  ///< \refwc{Enums::CommandId} enum names.
	/// \}

	/// Types of things to request or set. \sa DataRequest struct.
	WSMCMND_ENUM_EXPORT enum class RequestType : uint8_t
	{
		None = 0,   ///< Use to remove a previously-added request.
		Named,      ///< A named variable.
		Calculated  ///< Calculator code.
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> RequestTypeNames = { "None", "Named", "Calculated" };  ///< \refwc{Enums::RequestType} enum names.
	/// \}

	/// The type of result that calculator code is expected to produce. \sa DataRequest struct, Enums::CommandId::Exec command.
	WSMCMND_ENUM_EXPORT enum class CalcResultType : uint8_t
	{
		None = 0,   ///< No result is expected (eg. triggering an event).
		Double,     ///< Expect a double ("FLOAT64") type result.
		Integer,    ///< Expect an 32bit signed integer result.
		String,     ///< Expect a string type result from `execute_calculator_code()`.
		Formatted,  ///< Execute code using `format_calculator_string()` function and expect a string result type.
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> CalcResultTypeNames = { "None", "Double", "Integer", "String", "Formatted" };  ///< \refwc{Enums::CalcResultType} enum names.
	/// \}

	/// How often to check for updated request values. \sa DataRequest struct.
	WSMCMND_ENUM_EXPORT enum class UpdatePeriod : uint8_t
	{
		Never = 0,    ///< Suspend all automatic updates, only check value on `Enums::CommandId::Update` command.
		Once,         ///< Update once when DataRequest is added/updated, and then only on `Update` command.
		Tick,         ///< Update as often as possible (see \refwc{TICK_PERIOD_MS}).
		Millisecond,  ///< Update every `interval` milliseconds (interval value must be greater than 0 with effective minimum being the "Tick" period (\refwc{TICK_PERIOD_MS})).
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> UpdatePeriodNames = { "Never", "Once", "Tick", "Millisecond" };  ///< \refwc{Enums::UpdatePeriod} enum names.
	/// \}

	/// Types of things to look up or list. \sa Enums::CommandId::List, Enums::CommandId::Lookup commands
	WSMCMND_ENUM_EXPORT enum class LookupItemType : uint8_t
	{
		None,               ///< Null type, possible internal use, ignored by server.
		LocalVariable,      ///< LVar ('L') names and IDs. Available for `List` and `Lookup` commands.
		SimulatorVariable,  ///< SimVar ('A') names and IDs. Available for `Lookup` command only.
		TokenVariable,      ///< Token Variable ('T'). Available for `Lookup` command only.
		UnitType,           ///< Measurement Unit. Available for `Lookup` command only.
		KeyEventId,         ///< Key Event ID, value of `KEY_*` macros from "guauges.h" header. Available for `Lookup` command only (use event name w/out the "KEY_" prefix).
		DataRequest,        ///< Saved value subscription for current Client, indexed by `requestId` and `nameOrCode` values. Available for `List` and `Lookup` commands.
		RegisteredEvent,    ///< Saved calculator event for current Client, indexed by `eventId` and `code` values. Available for `List` and `Lookup` commands.
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> LookupItemTypeNames = {
		"None", "LocalVariable", "SimulatorVariable", "TokenVariable", "UnitType", "KeyEventId", "DataRequest", "RegisteredEvent"
	};  ///< \refwc{Enums::LookupItemType} enum names.
	/// \}

	/// Logging levels. \sa LogRecord struct, CommandId::Log command.
	WSMCMND_ENUM_EXPORT enum class LogLevel : uint8_t
	{
		None = 0,   ///< Disables logging
		Critical,   ///< Events which cause termination.
		Error,      ///< Hard errors preventing function execution.
		Warning,    ///< Possible anomalies which do not necessarily prevent execution.
		Info,       ///< Informational messages about key processes like startup and shutdown.
		Debug,      ///< Verbose debugging information.
		Trace       ///< Very verbose and frequent debugging data, do not use with "slow" logger outputs.
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> LogLevelNames = { "None", "Critical", "Error", "Warning", "Info", "Debug", "Trace" };  ///< \refwc{Enums::LogLevel} enum names.
	/// \}

	/// Logging destination type. \sa CommandId::Log command.
	WSMCMND_ENUM_EXPORT enum class LogFacility : uint8_t
	{
		None    = 0x00,  ///< Invalid or default logging facilty. For the `Enums::CommandId::Log` command this is same as `Remote`.
		Console = 0x01,  ///< Console logging, eg. stderr/stdout.
		File    = 0x02,  ///< Log file destination.
		Remote  = 0x04,   ///< Remote destination, eg. network transmission or a callback event.
		All = Console | File | Remote
	};
#if defined(DEFINE_ENUM_FLAG_OPERATORS) && !defined(_MANAGED)
	DEFINE_ENUM_FLAG_OPERATORS(WSMCMND_ENUM_NAMESPACE::LogFacility)
#endif
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> LogFacilityNames = {
		"None", "Console", "File", "Console | File", "Remote", "Console | Remote", "File | Remote", "All"
	};  ///< \refwc{Enums::LogFacility} enum names.
	/// \}

};
