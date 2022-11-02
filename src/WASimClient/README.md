
# WASimCommander
## WASimClient - C++ Implementation of The WASim API

This directory contains the implementation of a Client-side library for all aspects of communication with the WASimCommander Server component (`WASimModule`).

It is designed to be integrated as either a static or dynamic library into other software. 
To encourage adaptation, it is dual licensed under the terms of the GPL (dynamic or static linking) 
or LGPL (dynamic linking). 

See corresponding LICENSE.txt for licensing details.

### Building Note

When building with MSVC against the pre-built version of this library, either static or dynamic, be sure build with `/MD` or `/MDd` (debug) options.
That's the _Runtime Library_ property, "Multi-threaded DLL" setting (or "Multi-threaded Debug DLL" for debug builds). And don't mix up the debug
vs. release difference, or there will be untraceable crashing and burning. These should be the default settings on a new project, but you never know.
