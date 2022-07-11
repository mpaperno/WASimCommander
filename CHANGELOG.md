# WASimCommander - Change Log

## 1.0.0.6-beta2 (Jul-11-2022)
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


## 1.0.0.5-beta1 (Jul-04-2022)
- Initial release!
