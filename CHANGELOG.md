# WASimCommander - Change Log

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
