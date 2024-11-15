[![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/mpaperno/WASimCommander?include_prereleases)](https://github.com/mpaperno/WASimCommander/releases)
[![GPLv3 License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE.GPL.txt)
[![LGPGv3 License](https://img.shields.io/badge/license-LGPLv3-blue.svg)](LICENSE.LGPL.txt)
[![API Documentation](https://img.shields.io/badge/API-Documentation-07A7EC?labelColor=black)](https://wasimcommander.max.paperno.us/)
[![Discord](https://img.shields.io/static/v1?style=flat&color=7289DA&&labelColor=7289DA&message=Discord%20Chat&label=&logo=discord&logoColor=white)](https://discord.gg/meWyE4dcAt)


# WASimCommander

<div align="center">
<img src="https://github.com/mpaperno/WASimCommander/wiki/images/logo/WASim-Logo_640x160.png" style="width: auto;" />
</div>

## Remote access to the Microsoft Flight Simulator 2020 "Gauge API."

**A WASM module-based Server and a full Client API combination.**

This project is geared towards other MSFS developers/coders who need a convenient way to remotely access parts of the Simulator which are normally
inaccessible via _SimConnect_, such as some variable types and 'H' events, and running RPN "calculator code" directly on the sim.

The Client API can be utilized natively from C++, or via .NET managed assembly from C#, Python, or other languages.

The system also provides direct access to functions from within the Simulator environment and in many cases could be used as a simpler alternative
to _SimConnect_ for basic functionality like reading/setting Simulation Variables or triggering Key Events. _SimConnect_ is still used as the network
"transport" layer, but this usage is abstracted into, essentially, an implementation detail.

### Motivation

One of the motivations for this project was seeing multiple MSFS tool authors and casual hackers creating their own WASM modules and protocols just to support their
own product or need. There is nothing wrong with this, of course, but for the Sim user it can be a disadvantage on several levels. They may end up running
multiple versions of modules which all do essentially the same thing, and it may be confusing which WASM module they need to support which tool,
just to name two obvious issues. For the developer, programming the WASM modules comes with its own quirks, too, not to mention the time involved.
And regardless of the supposed isolated environment a WASM module is supposed to run in, it's still very easy to take down the whole Simulator with 
some errant code... ;-)

Since MS/Asobo have been, ahem, "slow" to add further remote access features to _SimConnect_ (or come up with some other method), this project is an attempt
at establishing a "standard" or "common" way of doing so. At the least I hope it motivates _some_ kind of standards adoption.

On a more practical note, I am using it with the [MSFS Touch Portal Plugin](https://github.com/mpaperno/MSFSTouchPortalPlugin) which I'm currently maintaining.

-------------
### Features

#### Remote Capabilities
- **Execute Calculator Code**:
    - With or without a returned result; Result returned as numeric and string types.
    - Formatted results from `format_calculator_string()` using 
      [RPN String Formatting](https://docs.flightsimulator.com/html/Additional_Information/Reverse_Polish_Notation.htm#strings)
- **Get Variable**: Return value result from any type of variable accessible to a standalone WASM module 
    (basically everything but Gauge and Instrument types, but also including Token vars). 
    - With optional Unit specifier for variable types which support it.
- **Set Variable**: Set the value of any settable variable type, with optional Unit specifier for variable types which support it.
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
    - Save recurring "set events," like activating controls using calculator code, for more efficient and simpler re-use. 
        Saved calculator code is pre-compiled to a more efficient byte code representation before being passed to the calculator function. 
        This significantly improves performance for recurring events.
    - Registered events can be executed "natively" via _WASim API_ by simply sending a short command with the saved event ID.
    - Saved events can also be named and executed via standard SimConnect commands `SimConnect_MapClientEventToSimEvent(id, "event_name")` and `SimConnect_TransmitClientEvent(id)`.
    - Event names can be completely custom (including a `.` (period) as per SimConnect convention), or derive from the connected Client's name (to ensure uniqueness).
- **Send Simulator "Key Events"** directly by ID or name (instead of going through the SimConnect mapping process or executing calculator code). Much more efficient than the other methods.
    - **New in v1.1.0:** Send Key Events with up to 5 values (like the new `SimConnect_TransmitClientEvent_EX1()`).
    - **New in v1.3.0:** Send Custom Key Events (the ones with a "." in the name that are defined by particular models) by name or ID with up to 5 values.
- **Remote Logging**: Log messages (errors, warnings, debug, etc) can optionally be sent to the Client, with specific minimum level (eg. only warnings and errors).
- **Ping** the Server to check that the WASM module is installed and running before trying to connect or use its features.

#### Core Components
- WASM module-based Server supports multiple simultaneous, independent client connections.
- Full-featured Client implementation available as a C++ library and a C#/.NET assembly.
- Well-defined message API for communication between Server module and any client implementation.
- Uses standard SimConnect messages for the base network "transport" layer.
- All data allocations are on client side, so SimConnect limits in WASM module are bypassed (can in theory support unlimited clients).
- No wasted data allocations, each data/variable subscription is stored independently avoiding complications with offsets or data overflows.
- Minimum possible impact on MSFS in terms of memory and CPU usage; practically zero effect for Sim user when no clients are connected (Server is idle).
- Server periodically checks that a client is still connected by sending "heartbeat" ping requests and enforcing a timeout if no response is received.
- Extensive logging at configurable levels (debug/info/warning/etc) to multiple destinations (file/console/remote) for both Server and Client. 
    - Uses an efficient **lazy logging** implementation which doesn't evaluate any arguments if the log message will be discarded anyway 
    (eg. a DEBUG level message when minimum logging level is INFO).
    - Logging levels can be set at startup via config files and changed at runtime for each facility (including remotely on the server).
- Includes a SimConnect request/exception tracking feature for detailed diagnostics.
- Extensive documentation for core API and all Client library features.
- Efficiency and runtime safety are core requirements.

#### Desktop GUI
- Includes a full-featured desktop application which demonstrates/tests all available features of the API.
- Fully usable as a standalone application which saves preferences, imports/exports lists of data subscriptions/registered events, and other friendly features.
- Very useful for "exploring" the simulator in general, like checking variable values, testing effects of key events and RPN calculator code.
- Can be used with the [MSFS/SimConnect Touch Portal Plugin](https://github.com/mpaperno/MSFSTouchPortalPlugin) for import/export of custom variable request definitions.

<p> &nbsp; </p>
<div align="center">
<a href="https://github.com/mpaperno/WASimCommander/wiki/images/gui/wasimui-dark-new-v1005.png"><img src="https://github.com/mpaperno/WASimCommander/wiki/images/gui/wasimui-dark-new-v1005.png" style="width: 65%;" /></a>
</div>
<p> &nbsp; </p>


-------------
### Downloads and Updates

Over in the [Releases](https://github.com/mpaperno/WASimCommander/releases) there are 3 packages provided. (The actual file names have version numbers appended.)
- `WASimCommander_SDK` - All header files, pre-built static and dynamic libs, packaged WASM module, pre-build GUI, reference documentation, and other tools/examples.
- `WASimModule` - Just the WASM module component, ready to be dropped into a MSFS _Community_ folder.
- `WASimUI` - Just the GUI application, which is very handy in combination with the WASM module for exploring the system.

_Watch_ -> _Custom_ -> _Releases_ this repo (button at top) or subscribe to the [ATOM feed](https://github.com/mpaperno/WASimCommander/releases.atom) for release notifications.

Update announcements are also posted on my Discord server's [WASimCommander release announcement channel](https://discord.gg/StbmZ2ZgsF).

The SDK and updates are [published on Flightsim.to](https://flightsim.to/file/36474/wasimcommander) where one could "subscribe" to release notifications (account required).

-------------
### Documentation & Examples

There are three basic console-style tests/examples included for `C++`, `C#`, and `Python` in the [src/Testing](https://github.com/mpaperno/WASimCommander/tree/main/src/Testing) folder.
If you like reading code, this is the place to start.

API documentation generated from source comments is published here: https://wasimcommander.max.paperno.us/ <br/>
A good place to start with the docs is probably the [`WASimClient`](https://wasimcommander.max.paperno.us/class_w_a_sim_commander_1_1_client_1_1_w_a_sim_client.html) page.

The GUI is written in C++ (using Qt library for UI), and while not the simplest example, _is_ a full implementation of almost all the available
API features. The main `WASimClient` interactions all happen in the `MainWindow::Private` class at the top of the 
[WASimUi.cpp](https://github.com/mpaperno/WASimCommander/tree/main/src/WASimUI/WASimUI.cpp#L80) file.

More to come... or [Just Read The Source](https://github.com/mpaperno/WASimCommander/tree/main/src) :-)

#### Using .NET Libraries

Please note that when using the .NET builds in your project, it is **vital to include the `Ijwhost.dll`** in your runtime directory along with `WASimCommander.WASimClient.dll`.

While the latter will get copied to your build output automatically as a dependency, the `Ijwhost.dll` will *not*. This will result in a runtime error that says
`Could not load file or assembly 'WASimCommander.WASimClient.dll'. The specified module could not be found.` even though it is clearly there in the runtime directory.

Likewise you will probably want to copy the default `client_conf.ini` configuration file to your build output/runtime as well (this file defines some default options like logging).

Both files should be included in the build as "content" files. This can be done via the VS UI or by editing the project file directly.
As an example, assuming you copied the WASimCommander managed libraries to a `./lib` folder of your source, the following two entries
from a .csproj file illustrate the settings:

```xml
  <ContentWithTargetPath Include=".\lib\WASimCommander\Ijwhost.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    <TargetPath>Ijwhost.dll</TargetPath>
  </ContentWithTargetPath>
  <ContentWithTargetPath Include=".\lib\WASimCommander\client_conf.ini">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    <TargetPath>client_conf.ini</TargetPath>
  </ContentWithTargetPath>
```

-------------
### Troubleshooting

#### WASM Module
To check status of the WASM module, enable Developer Mode in MSFS, show the "WASM Debug" window (_Options_ -> _WASM_) and check if the module shows up
in the list in that window. If not, then it's not installed. If it does and any of the text is RED, it is installed but crashed for some reason (let me know!).
If the text is green then everything is good on that side.

Also in Dev Mode, check the Console for messages. It may show MSFS errors related to the module, and the module itself logs to the console as well.
There should be 2 console log messages from the module when it starts up, the latter showing the version number.

The module also logs to a file, though it's a bit tricky to find. On my edition (from MS store) the "working directory" for the modules is <br/>
`D:\WpSystem\S-1-5-21-611220451-769921231-644967174-1000\AppData\Local\Packages\Microsoft.FlightSimulator_8wekyb3d8bbwe\LocalState\packages\wasimcommander-module\work`

To enable more verbose logging on the module at startup, edit the `server_conf.ini` file which is found in the module's install folder 
(`Community\wasimcommander-module\modules`). There are comments in there indicating the options. 

Keep in mind that the server logging level can also be changed remotely at runtime, but
of course that only works if you can establish a connection to the module in the first place.

#### WASimClient (or anything using it, like WASimUI)

Basically the log is the primary source of information here. By default it logs at the "Info" level to:
1) The current console window, assuming there is one (the host app is started from a console). So if using `WASimUI`, for example, just start it from a command prompt.
2) A file in whatever current directory it is running in (so, for the UI, that would be the UI's install folder). 

Of course if you're using `WASimUI`, it also provides a full logging interface and you can set all the log levels from there, for both client and server sides.

There should also be a `client_conf.ini` file alongside whatever is using the Client
where initial logging location, levels and network configuration (timeout and SimConnect.cfg index) is set. 
The config file has comments indicating the available options.

-------------
### Issues, Support, Suggestions, Discussion

The GitHub repository is the primary source of all these things. You know what to do...

Use the [Issues](https://github.com/mpaperno/WASimCommander/issues) feature for bug reports and concise feature suggestions.

Use [Discussions](https://github.com/mpaperno/WASimCommander/discussions) for any other topic.

There is also a [Discord support forum](https://discord.gg/QhUDFX6Kun) on my server, 
along with [release announcement](https://discord.gg/StbmZ2ZgsF) and [general chat](https://discord.gg/meWyE4dcAt) channels.

Most flight simulator forums seem fairly strict about _not_ using their site to provide product support. So please use GitHub,
unless you're absolutely sure no rules would be broken or toes stepped upon otherwise.

-------------
### Credits

This project is written, tested, and documented by myself, Maxim (Max) Paperno.<br/>
https://github.com/mpaperno/

Uses and includes a slightly modified version of [_logfault_ by Jarle Aase](https://github.com/jgaa/logfault), used under the MIT license. Changes documented in the code.

Uses and includes [_IniPP_ by Matthias C. M. Troffaes](https://github.com/mcmtroffaes/inipp), used under the MIT license.

Uses the _Microsoft SimConnect SDK_ under the terms of the _MS Flight Simulator SDK EULA (11/2019)_ document.

WASimUI (GUI):
- Uses portions of the [_Qt Library_](http://qt.io) under the terms of the GPL v3 license.
- Uses and includes the following symbol fonts for icons, under the terms of their respective licenses:
  - [IcoMoon Free](https://icomoon.io/#icons-icomoon) - IcoMoon.io, GPL v3.
  - [Material Icons](https://material.io/) - Google, Apache License v2.0.
- Uses modified versions of `FilterTableHeader` and `FilterLineEdit` components from [DB Browser for SQLite](https://github.com/sqlitebrowser/sqlitebrowser) under GPL v3 license.
- Uses modified version of `MultisortTableView` from <https://github.com/dimkanovikov/MultisortTableView> under GPL v3 license.
- Uses Natural (alpha-numeric) sorting algorithm implementation for _Qt_ by Litkevich Yuriy (public domain).


Documentation generated with [Doxygen](https://www.doxygen.nl/) and styled with the most excellent [Doxygen Awesome](https://jothepro.github.io/doxygen-awesome-css).

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
