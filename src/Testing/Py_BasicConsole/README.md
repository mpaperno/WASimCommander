
# WASimCommander
## Basic Console Test for Python

This directory contains a simple console-based application for testing and demonstrating `WASimClient` API usage in Python,
using [Python.NET](http://pythonnet.github.io/) to load the WASimClient_CLI .NET assembly.

It is in fact almost an exact copy of the `CS_BasicConsole` version except modified for Python syntax. Pretty neat.

Requires a 64-bit CPython, v3.8 or higher and Python.NET installed. Tested with `pythonnet 3.0.0rc3` which at the time of this writing requires
`pip install pythonnet --pre`.<br/>
(The current release 2.x version will probably work also but the error messages from that version are much harder to trace so I gave up on it.)

The script needs the `WASimCommander.WASimClient.dll` assembly (and the `Ijwhost.dll` dependency) to be in Python's system search path.
By default it's set up to look for the DLLs in the same folder as the script itself. To change the expected location, open the script
and edit the path directives at the top (its commented).

The `app.runtime.json` file is also required to set the correct .NET runtime, which is set to require v5.0.17 (WASimClient_CLI is currently built with .NET 5).
You _may_ need to edit this file and modify the version number.

Lastly, a `client_conf.ini` _should_ also be present in the folder, otherwise WASimClient will log a warning about not being able to find it. Though otherwise
there's no problem with that, it'll just use built-in defaults instead.

If all the above is set, then it's just a matter of running the script from a command line like you would any other Py script.
