
# WASimCommander
## WASimClient_CLI - C++/CLI Wrapper of WASimCommander Components

This directory contains the C++/CLI "wrapper" of the `WASimClient` C++ library, used for all
aspects of communication with the WASimCommander Server component (`WASimModule`).

This code produces a .NET Core managed assembly DLL which can be used with pure C# projects,
and possibly other languages which can consume .NET assemblies.

It is designed to be intergrated as either a static or dynamic library into other software.
To encourage adaptation, it is dual licensed under the terms of the GPL or LGPL (see LICENSE.txt for details).
