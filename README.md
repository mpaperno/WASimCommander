[![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/mpaperno/WASimCommander?include_prereleases)](https://github.com/mpaperno/WASimCommander/releases)
[![GPLv3 License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE.GPL.txt)
[![LGPGv3 License](https://img.shields.io/badge/license-LGPLv3-blue.svg)](LICENSE.LGPL.txt)


# WASimCommander

<div align="center">
<img src="https://github.com/mpaperno/WASimCommander/wiki/images/logo/WASim-Logo_760x190.png" style="width: auto;" />
</div>

## Remote access to the Microsoft Flight Simulator 2020 "Gauge API."


**A WASM module-based Server and a full Client API combination.**

This project is geared towards other MSFS developers/coders who need a convenient way to remotely access parts of the Simulator which are normally
inaccessible via _SimConnect_, such as locally-defined aircraft variables or custom events.

The system also provides direct access to functions from within the Simulator environment and in many cases could be used as a simpler alternative
to _SimConnect_ for basic functionality like reading/setting Simulation Variables or triggering Key Events. _SimConnect_ is still used as the network
"transport" layer, but this usage is abstracted into, essentially, an implementation detail.

### Motivation

One of the motivations for this project was seeing multiple MSFS tool authors and casual hackers creating their own WASM modules and protocols just to support their
own product or need. There is nothing wrong with this, of course, but for the Sim user it can be a disadvantage on several levels. They may end up running
multiple versions of modules which all do eseentially the same thing, and it may be confusing which WASM module they need to support which tool,
just to name two obvious issues. For the developer, programming the WASM modules comes with its own quirks, too, not to mention the time involved.
And regardless of the supposed isolated environment a WASM module is supposed to run in, it's still very easy to take down the whole Simulator with 
some errant code... ;-)

Since MS/Asobo have been, ahem, "slow" to add further remote access features to _SimConnect_ (or come up with some other method), this project is an attempt
at establishing a "standard" or "common" way of doing so. At the least I hope it motivates _some_ kind of standards adoption.

On a more practical note, I will be using it with the [MSFS Touch Portal Plugin](https://github.com/mpaperno/MSFSTouchPortalPlugin) which I'm currently maintaining.

-------------
### Features

#### Remote Capabilities
- **Execute Calculator Code**:
    - With or without a returned result; Result returned as numeric and string types.
    - Formatted results from `format_calculator_string()` using [RPN String Formatting](https://docs.flightsimulator.com/html/Samples_And_Tutorials/Samples/#strings)
- **Get Variable**: Return numeric result from any type of variable accessible to a standalone WASM module 
    (basically everything but Gauge and Instrument types, but also including Token vars). 
    - With optional Unit specifier for variable types which support it.
- **Set Variable**: Set the numeric value of any settable variable type, with optional Unit specifier for variable types which support it.
- **Create Variable**: Create (and get/set) a new Local variable if it doesn't already exist.
- **List Local Variables**: Get a list of all available 'L' variables with their names and current IDs.
- **Lookup**: Return a numeric ID for a SimVar/Local/Token variable, Unit, or Key Event name.
- **Subscribe** to _Calculator Code_ and _Get Variable_ results:
    - Get "push" notifications whenever result values change; Change monitoring can be configured at any rate down to the millisecond (~25ms minimum). 
        Can also be configured to use the "delta epsilon" feature of SimConnect (to ignore insignificant changes in value) or, conversely, to always send 
        updates even when values do not change.
    - Optionally perform manual ("polled") updates of subscriptions at any interval of your choice.
    - Any calculator code saved in subscriptions is **pre-compiled to a more efficient byte code** representation before being passed to the respective calculator
        functions. This significantly improves performance for recurring calculations.
- **Register Named Events**:
    - Save recurring "set events," like activiating controls using calculator code, for more efficient and simpler re-use. 
        Saved calculator code is pre-compiled to a more efficient byte code representation before being passed to the calculator function. 
        This significantly improves performance for recurring events.
    - Registered events can be executed "natively" via _WASim API_ by simply sending a short command with the saved event ID.
    - Saved events can also be named and executed via standard SimConnect commands `SimConnect_MapClientEventToSimEvent(id, "event_name")` and `SimConnect_TransmitClientEvent(id)`.
    - Event names can be completely custom (including a `.` (period) as per SimConnect convention), or derive from the connected Client's name (to ensure uniqueness).
- **Send Simulator "Key Events"** directly by ID (instead of going through the SimConnect mapping process or executing calculator code). Much more efficient than the other methods.
- **Remote Logging**: Log messages (errors, warnings, debug, etc) can optionally be sent to the Client, with specific minimum level (eg. only warnings and errros).
- **Ping** the Server to check that the WASM module is installed and running before trying to connect or use its features.

#### Core Components
- WASM module-based Server supports multiple simultaneous, independent client connections.
- Full-featured Client implementation available as a C++ library and a C#/.NET assembly.
- Well-defined message API for communication between Server module and any client implementation.
- Uses standard SimConnect messages for the base network "transport" layer.
- All data allocations are on client side, so SimConnect limits in WASM module are bypassed (can in theory support unlimited clients).
- No wasted data allocations, each data/variable subscription is stored independently avoiding complications with offets or data overflows.
- Minimum possible impact on MSFS in terms of memory and CPU usage; practically zero effect for Sim user when no clients are connected (Server is idle).
- Server periodically checks that a client is still connected by sending "hearbeat" ping requests and enforcing a timeout if no response is received.
- Extensive logging at configurable levels (debug/info/warning/etc) to multiple destinations (file/console/remote) for both Server and Client. 
    - Uses an efficient **lazy logging** implementation which doesn't evaluate any arguments if the log message will be discarded anyway 
    (eg. a DEBUG level message when minimum logging level is INFO).
    - Logging levels can be set at startup via config files and changed at runtime for each facility (including remotely on the server).
- Includes a SimConnect request/exception tracking feature for detailed diagnostics.
- Extensive documentation for core API and all Client library features.
- Efficiency and runtime safety are core requirements.

#### Desktop GUI
- Includes a full-featured desktop application which demonstrates/tests all available features of the API.
- Fully usable as a standalone application which saves preferences, imports/exports lists of data subscriptions/registered events, and other usabililty features.

<p> &nbsp; </p>
<div align="center">
<a href="https://github.com/mpaperno/WASimCommander/wiki/images/gui/wasimui-dark-new-v1005.png"><img src="https://github.com/mpaperno/WASimCommander/wiki/images/gui/wasimui-dark-new-v1005.png" style="width: 65%;" /></a>
</div>
<p> &nbsp; </p>


-------------
### Downloads

Over in the [Releases](https://github.com/mpaperno/WASimCommander/releases) there are 3 packages provided. (The actual file names have version numbers appended.)
- `WASimCommander_SDK` - All header files, pre-built static and dynamic libs, packaged WASM module, pre-build GUI, reference documentation, and other tools/examples.
- `WASimModule` - Just the WASM module component, ready to be dropped into a MSFS _Community_ folder.
- `WASimUI` - Just the GUI application, which is very handy in combination with the WASM module for exploring the system.

_Watch_ -> _Custom_ -> _Releases_ this repo (button at top) or subscribe to the ATOM feed  (https://github.com/mpaperno/WASimCommander/releases/atom) for release notifications.

-------------
### Documentation & Examples

API docuemntation generated from source comments is published here: https://mpaperno.github.io/WASimCommander/ <br/>
A good place to start with the docs is probably the [`WASimClient`](https://mpaperno.github.io/WASimCommander/class_w_a_sim_commander_1_1_client_1_1_w_a_sim_client.html) page.

There is one basic C# console test/example included in the 
[src/Testing/CS_BasicConsole](https://github.com/mpaperno/WASimCommander/tree/main/src/Testing/CS_BasicConsole/CS_BasicConsole.cs) 
folder (C++ example forthcoming, but it's very similar):


More to come... for now there's always The Source Code to read... :-)

-------------
### Issues, Support, Suggestions, Discussion

The GitHub repository is the primary source of all these things. You know what to do...

Use the [Issues](https://github.com/mpaperno/WASimCommander/issues) feature for bug reports and concise feature suggestions.

Use [Discussions](https://github.com/mpaperno/WASimCommander/discussions) for any other topic.

Most flight simulator forums seem fairly strict about _not_ using their site to provide product support. So please use GitHub,
unless you're absolutely sure no rules would be broken or toes stepped upon otherwise.

-------------
### Credits

This project is written, tested, and documented by myself, Maxim (Max) Paperno.<br/>
https://github.com/mpaperno/

Uses and includes a slightly modified version of [_logfault_ by Jarle Aase](https://github.com/jgaa/logfault), used under the MIT license. Changes documented in the code.

Uses and includes [_IniPP_ by Matthias C. M. Troffaes](https://github.com/mcmtroffaes/inipp), used under the MIT license.

Uses the _Microsoft SimConnect SDK_ under the terms of the _MS Flight Simulator SDK EULA (11/2019)_ document.

The GUI component uses portions of the [_Qt Library_](http://qt.io) under the terms of the GPL v3 license.

The GUI component uses and includes the following symbol fonts for icons, under the terms of their respective licenses:
- [IcoMoon Free](https://icomoon.io/#icons-icomoon) - IcoMoon.io, GPL v3.
- [Material Icons](https://material.io/) - Google, Apache License v2.0.


-------------
### Copyright, License, and Disclaimer

WASimCommander Project <br />
COPYRIGHT: Maxim Paperno; All Rights Reserved.

#### API Library and Client Components
Dual licensed under the terms of either the GNU General Public License (**GPL**)
or the GNU Lesser General Public License (**LGPL**), as published by the Free Software
Foundation, either **version 3** of the Licenses, or (at your option) any later version.

#### WASM Module Server and GUI Components
Licensed under the terms of the GNU General Public License (**GPL**) as published by 
the Free Software Foundation, either **version 3** of the License, or (at your option) 
any later version.

#### General

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Copies of the GNU GPL and LGPL are included with this project
and are available at <http://www.gnu.org/licenses/>.

Except as contained in this copyright notice, the names of the authors or
their institutions shall not be used in advertising or otherwise to
promote the sale, use, or other dealings in, any product using this 
Software, or any derivative of this Software, without prior written 
authorization from the authors.

This project may also use 3rd-party Open Source software under the terms
of their respective licenses. The copyright notice above does not apply
to any 3rd-party components used within.
