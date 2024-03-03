# WASimCommander - Change Log

## 1.3.0.0 (next)

### WASimModule
* The server will now handle connection, ping, and custom calculator events initiated with `SimConnect_TransmitClientEvent_EX1()` (as well as the older `TransmitClientEvent()` version). ([8f42b51d])
* Updated reference list of KEY event names for MSFS SDK v0.23.1.0. ([b115d8b6])

### WASimClient (C++) and WASimClient_CLI (managed .NET)
* Added ability to use "custom named simulator events" (such as "Custom.Event") directly from `WASimClient`, w/out needing a separate `SimConnect` session ([61912e66]):
  * Added `registerCustomKeyEvent()` method for mapping event names to numeric IDs (similar in purpose to `SimConnect_MapClientEventToSimEvent()` function). See [documentation][registerCustomKeyEvent] for details.
  * `sendKeyEvent()` can now be used to trigger registered custom events (similar to `SimConnect_TransmitClientEvent[_EX1]()` functions), by ID or name. See [documentation][sendKeyEvent] for details.
  * Note: This is a client-side feature, not involving the server-side `WASimModule` at all, provided for convenience.
  * Added basic test/example of usage in `CPP_BasicConsole` and `CS_BasicConsole` project code.
  * Many thanks to [Hans Billiet] for the motivation and [original code][hb-custom-events]!
* Fixed that the key event name lookup cache used with `sendKeyEvent(eventName)` string overload wasn't entirely thread-safe. Thanks to [Hans Billiet] for reporting! ([03753d8b])

### WASimClient_CLI (managed .NET)
* Fixed possible `null` `String` references in `RegisteredEvent` and `VariableRequest` structs when created with some constructor overloads (for example when attempting to send a `new VariableRequest(localVariableId)`). Thanks to [Hans Billiet] for reporting! ([1632ed18])

### WASimUI
* Updated database of imported Event IDs and Simulator Variables from published online SDK docs as of Mar-1-2024. ([35a495c6])

### Documentation
* Split [WASimClient's][WASimClient_docs] "high level API" methods overview into more specific groups.
* Added/improved some details about `WASimClient`'s event callbacks and possible concurrency issues. ([e474c7fd])

**[Full Change Log](https://github.com/mpaperno/WASimCommander/compare/v1.2.0.0...next)**

[8f42b51d]: https://github.com/mpaperno/WASimCommander/commit/8f42b51d1d94f704e001940abe2ff3f1434a9481
[b115d8b6]: https://github.com/mpaperno/WASimCommander/commit/b115d8b64e77eaf8112de7a15c9d0dfc2fa29904
[03753d8b]: https://github.com/mpaperno/WASimCommander/commit/03753d8b834bfd0239aee2df1043c3b0377d4a55
[1632ed18]: https://github.com/mpaperno/WASimCommander/commit/1632ed1883108f0ba5ff03610ce4fc902d84387c
[35a495c6]: https://github.com/mpaperno/WASimCommander/commit/35a495c6a3dc9beb13ffc05bdeab105f3b7a9fae
[61912e66]: https://github.com/mpaperno/WASimCommander/commit/61912e66f5e51791d51fce42bb96de38f7d5b6a0
[e474c7fd]: https://github.com/mpaperno/WASimCommander/commit/e474c7fdb9074d29064392354e2829c3d2267dee
[custom-key-events]: https://github.com/mpaperno/WASimCommander/compare/dbd4c469611750a8a9e7222740428fd4851b516f..e98fbd42b8e6aea34e0e013d261c8a48ebd47fcf
[hb-custom-events]: https://github.com/HansBilliet/WASimCommander/compare/b1519c988bf44ce43af8a50880092391566af48a...83adc91fb73fa29dcc07f1461435dafe41f7d366
[registerCustomKeyEvent]: https://wasimcommander.max.paperno.us/class_w_a_sim_commander_1_1_client_1_1_w_a_sim_client.html#a6e7bf0b7c6b741081bc2ce43d937ba11
[sendKeyEvent]: https://wasimcommander.max.paperno.us/class_w_a_sim_commander_1_1_client_1_1_w_a_sim_client.html#adf5b8df4cb657fefaa97f97b0ebea42c
[WASimClient_docs]: https://wasimcommander.max.paperno.us/class_w_a_sim_commander_1_1_client_1_1_w_a_sim_client.html
[Hans Billiet]: https://github.com/HansBilliet


---
## 1.2.0.0 (07-Nov-2023)

### WASimModule
* Fix binary data representation in results for named variable requests with 1-4 byte integer value sizes (`int8` - `int32` types) -- the result data would be encoded as a float type instead. ([8c7724e6])
* Restore ability to use Unit type specifiers when setting and getting Local vars. ([e16049ac])
* Added ability to specify/set a default 'L' var value and unit type in `GetCreate` command to use if the variable needs to be created. ([61a52674])
* `GetCreate` and `SetCreate` commands for non-L types now silently fall back to `Get` and `Set` respectively. ([61a52674])
* Fixed that command response for `GetCreate` was always sent as if responding to a `Get` command. ([61a52674])
* Added `requestId` to error logging and response output for data requests and add more info for `Get` command errors. ([17791eef])
* Added ability to return string type results for Get commands and Named data requests by converting them to calculator expressions automatically on the server. ([983e7ab6])
* Improved automatic conversion to calc code for other variable types by including the unit type, if given, and narrowing numeric results to integer types if needed. ([983e7ab6])
* Prevent possible simulator hang on exit when quitting with active client(s) connections. ([70e0ef31])
* Event loop processing is now paused/restarted also based on whether any connected client(s) have active data requests and if they are paused or not (previously it was only based on if any clients were connected at all). ([90242ed4])
* Fixes a logged SimConnect error when trying to unsubscribe from the "Frame" event (cause unknown). ([90242ed4])
* Data requests with `Once` type update period are now queued if data updates are paused when the request is submitted. These requests will be sent when/if updates are resumed again by the client. Fixes that data would be sent anyway when the request is initially submitted, even if updates are paused. ([fe99bbb2])
* Update reference list of KEY events and aliases as of MSFS SDK v0.22.3.0. ([f045e150])

[8c7724e6]: https://github.com/mpaperno/WASimCommander/commit/8c7724e60ed94e622d5ee2669cf7e000031c2c18
[e16049ac]: https://github.com/mpaperno/WASimCommander/commit/e16049ac69ff15cdcdd9084c7fdab6920a1ffba1
[61a52674]: https://github.com/mpaperno/WASimCommander/commit/61a52674e0dff7e1f3e63ed73a0bed711bb2c479
[17791eef]: https://github.com/mpaperno/WASimCommander/commit/17791eefecc86454c031636a5da9c19d56e21139
[983e7ab6]: https://github.com/mpaperno/WASimCommander/commit/983e7ab609e81af81525ff84431b1c4557447d87
[70e0ef31]: https://github.com/mpaperno/WASimCommander/commit/70e0ef31b01a1a772d9e49102e0a77ec6f3e928b
[90242ed4]: https://github.com/mpaperno/WASimCommander/commit/90242ed494069aba5bcdad839914b9fcfc6521e2
[fe99bbb2]: https://github.com/mpaperno/WASimCommander/commit/fe99bbb25c5dd907e8a4d513769759c4b430580f
[f045e150]: https://github.com/mpaperno/WASimCommander/commit/f045e15007abd6b7b05b97c004a7a55488a33a9b

### WASimClient and WASimClient_CLI (managed .NET)
* Fixed incoming data size check for variable requests which are less than 4 bytes in size. ([c8e74dfa])
* Fixed early timeout being reported on long-running `list()` requests (eg.thousands of L vars). ([a05a28c3])
* Restored ability to specify Unit type for L vars and support for GetCreate with default value/unit and added extra features: ([3090d534], [0a30646d])
  * Added unit name parameter to `setLocalVariable()` and `setOrCreateLocalVariable()`.
  * Added `getOrCreateLocalVariable()`.
  * Added `VariableRequest::createLVar` property.
  * Add optional `create` flag and unit name to `VariableRequest()` c'tor overloads.
* Added async option to `saveDataRequest()` which doesn't wait for server response (`saveDataRequestAsync()` for the C# version). ([82ea4252], [0a30646d])
* Added ability to return a string value with `getVariable()` to make use of new WASimModule feature. ([8e75eb8c], [0e54794b])
* The request updates paused state (set with `setDataRequestsPaused()`) is now saved locally even if not connected to server and will be sent to server upon connection and before sending any queued data requests. 
  This allows connecting and sending queued requests but suspending any actual value checks until needed. ([bea8bccb])
* The `setVariable()` method now verifies that the specified variable type is settable before sending the command to the server. ([576914a2])
* Removed logged version mismatch warning on Ping response.
* Documentation updates.

[c8e74dfa]: https://github.com/mpaperno/WASimCommander/commit/c8e74dfa706647cf785c7e6c811731d8945e49c6
[a05a28c3]: https://github.com/mpaperno/WASimCommander/commit/a05a28c3d1af56444be3fbe54f619e62548736a0
[3090d534]: https://github.com/mpaperno/WASimCommander/commit/3090d5344c3a34c62e81f61237fe1fd91f6b11c5
[0a30646d]: https://github.com/mpaperno/WASimCommander/commit/0a30646d0ae985580d67ed40c8a441a0f5a0ba17
[82ea4252]: https://github.com/mpaperno/WASimCommander/commit/82ea4252bd25423bbeab354799d6be41f053880e
[8e75eb8c]: https://github.com/mpaperno/WASimCommander/commit/8e75eb8c087f5a39fee93c2b7d073500e4f14664
[0e54794b]: https://github.com/mpaperno/WASimCommander/commit/0e54794b2ec8411f42d34a7696426724ffc5e932
[bea8bccb]: https://github.com/mpaperno/WASimCommander/commit/bea8bccba38fae987690d5af259f6f8b22fbc781
[576914a2]: https://github.com/mpaperno/WASimCommander/commit/576914a235c81b73ba0ea85655d913b61cbc5015

### WASimClient_CLI (managed .NET)
* Fixed possible exception when assembling list lookup results dictionary in the off-case of duplicate keys. ([cf46967b])

[cf46967b]: https://github.com/mpaperno/WASimCommander/commit/cf46967b499a9bb19a77a14a47bd2ac29b4d0989

### WASimUI
* Added database of Simulator Variables, Key Events, and Unit types imported from SimConnect SDK online documentation. This is used for:
  * Typing suggestions in the related form fields when entering names of 'A' vars, Key Events, or Unit types.
  * Available as a popup search window from each related form (Variables, Key Events, Data Requests) via button/menu/CTRL-F shortcut.
  * Can be opened as a standalone window for browsing and searching all imported data by type.
* Added ability to import and export Data Requests in _MSFS/SimConnect Touch Portal Plugin_ format with a new editor window available to adjust plugin-specific data before export (category, format, etc.)
* Fixed that the state of current item selections in tables wasn't always properly detected and buttons didn't get enabled/disabled when needed (eg. "Remove Requests" button).
* Added ability to toggle visibility of each main form area of the UI from the View menu (eg. Variables or Key Events groups). Choices are preserved between sessions.
* Simplified the connection/disconnection procedure by providing one action/button for both Sim and Server connections (independent actions still available via extension menu).
* Typing suggestions in combo boxes now use a drop-down menu style selection list by default, and the behavior can be configured independently for each one.
* String type variables can now be used in the "Variables" section for `Get` commands.
* Unit type specifier is now shown and used for 'L' variables as well (unit is optional).
* Added "Get or Create" action/button for 'L' vars.
* The list of 'L' variables loaded from simulator is now sorted alphabetically.
* The Size field in Data Request form is automatically populated with a likely match when a new Unit type is selected.
* Many improvements in table views (all options are saved to user settings and persist between sessions):
  * All column widths are now re-sizable in all tables.
  * Columns can be toggled on/off in the views (r-click for context menu).
  * Can now be sorted by multiple columns (CTRL-click).
  * Option to show filtering (searching) text fields for each column. Filters support wildcards and optional regular expressions.
  * Font size can be adjusted (using context menu or CTRL key with `+`, `-`, or `0` to reset.
  * Tooltips shown with data values when hovered over table cells (readable even if text is too long to fit in the column).
* Numerous shortcuts and context menus added throughout, each relevant to the respective forms/tables currently being used or clicked.
* Last selected variable types and data request type are saved between sessions.
* Most actions/buttons which require a server connection to work are now disabled when not connected.
* When loading data requests from a file while connected to the server, the requests are now sent asynchronously, improving UI responsiveness.
* More minor quality-of-life improvements!

**Full log:** [v1.1.2.0...1.2.0.0](https://github.com/mpaperno/WASimCommander/compare/1.1.2.0...1.2.0.0)

---
## 1.1.2.0 (23-Feb-2023)

### WASimModule
* Fixed KEY event alias for "AUTORUDDER_TOGGLE" -> KEY_AUTOCOORD_TOGGLE.

### WASimUI
* Fixed layout issue in "Variables" form which made the name selector/entry field too narrow.
* Renamed form group "Calculator Event" to "Evaluate Calculator Code".

---
## 1.1.1.0 (31-Jan-2023)
Patch update for MSFS SU11/SDK 0.20.5.0.

### WASimModule
* Added 73 Key Event lookup names from latest MSFS SDK v0.20.5.0.
* Added 55 Key Event lookup name aliases for published SimConnect Event IDs for which the KEY_* macro names do not match (result of my MSFS SDK Docs import project).
* KEY_PROP_FORCE_BETA_* Key Event IDs (from last update) were updated again to reflect SDK v0.20.5.0 values.
* Release version is now built using `clang:-O3` level optimization since MSFS memory corruption issue confirmed fixed.

### WASimClient and WASimClient_CLI
* Improved detection of configuration file parameter (`config`) in class constructor being a file or directory, which also validates existence.

### WASimUI
* Improved visual separation of forms by function type (Variables, Lookup, etc).
* Added/improved some of the tool-tip documentation notes for each core function (hover on section title).
* Fixed "About" dialog box transparency issue.

---
## 1.1.0.0 (2-Nov-2022)
Updates for MSFS 2020 SU10 changes and new event trigger API for sending multiple values.

### WASimModule
* Added new feature to trigger simulator Key Events with multiple value parameters (Gauge API `trigger_key_event_EX1()`).
* Removed ability to get or set Local type variables with Unit specifiers (turns out MSFS hasn't supported this from the start).
* Added Key Event lookup names for new KEY_PROP_FORCE_BETA_* events.
* Added Token variable name lookups for CIRCUIT_NAVCOM4_ON and BREAKER_NAVCOM4.
* Remove usage of deprecated `send_key_event()` for `SendKey` command in favor of `trigger_key_event_EX1()`.
* Module is now built with `/clang:-O1` optimization level (the newly-recommended `O3` level revealed a nasty MSFS memory corruption bug which 
  should be fixed in SU11; I will have an update after SU11 release).

### WASimClient
* Added `sendKeyEvent()` methods for sending simulator Key Events with up to 5 values, either by event ID or by name.
* Unit type specifiers for getting/setting Local variables are ignored (no longer sent to the server even if provided in the method call).

### WASimClient_CLI
* Added `sendKeyEvent()` methods (see above).
* Added .NET Framework 4.6 and .NET 6.0 targets/builds (pre-built DLLs added to SDK distribution).
* Re-targeted project for VS2022, MSVC v143.

### WASimUI
* Added new form for sending Key Events with up to 5 value parameters.
* Added connected WASimModule (server) version number display.
* Fixed that the calculation result display text field could not be cleared.

### Shared Components
* Minor optimization fix in SimConnect exception reporting module during lookup of a cached request record (for logging exception details).

---
## 1.0.0.9 (unpublished)
* No changes from 1.0.0.8-rc1.

---
## 1.0.0.8-rc1 (2-Aug-2022)
### WASimClient (and dependents)
* Fix possible SimConnect exception (Sim crash) when removing data request subscriptions. Seemed to only happen occasionally and when SimConnect was under load from other clients.
* Also cleans up removing data requests which were never sent to the server (eg. before Client was connected), which previously resulted in warnings from the server that the requests didn't exist.
* Ensure no SimConnect calls run concurrently (also improves request tracking/error reporting).
* Client validates that the configured ID is not zero before connecting to simulator.
* Check/warning of version match with server module is now limited to major version part only.

### WASIMClient_CLI
* Added convenience methods for setting string values in Command and DataRequest structures.
* Added more Command struct constructors.

### General
* Added Python example.

---
## 1.0.0.7-beta3 (12-Jul-2022)
### WASimClient, WASimClient_CLI, WASimUI
* Fixed WASimClient bug introduced in v1.0.0.6 that prevented Client from completing connection to Server on first attempt (but would work on the 2nd).

---
## 1.0.0.6-beta2 (11-Jul-2022)
### WASimModule
* The `SendKey` command can now accept known key event IDs by name (and do the lookup automatically).
* Stopped pre-compiling "Formatted" type calculator strings (for `format_calculator_string()`) since that seems to be broken in MSFS.
  ([Query posted to devsupport](https://devsupport.flightsimulator.com/questions/9513/gauge-calculator-code-precompile-with-code-meant-f.html))
* Pause event loop processing (`tick()`) when all clients disconnect.

### WASimClient
* Minor adjustment to list result processing to ensure proper cancellation in case something is interrupted.

### WASimClient_CLI
* Fix exception when setting a char_array<> value from empty strings. Eg. `DataRequest.unitName` or `Command.sData`.

### WASimUI
* Display a message when a string-type calculator result returns an empty string (instead of just blank space).

---
## 1.0.0.5-beta1 (04-Jul-2022)
- Initial release!
