# WASimCommander
## "Just Read The Source"

This directory has all the source code of the project. Contains the following components:

| directory         | description |
| ---------         | ----------- |
| `include`         | Shared C++ header files required to build or use any of the C++ API/libraries/code.<br/>This is the only folder required to **use** the `WASimAPI` or C++ `WASimClient` library with your own code. |
| `shared`          | Code which is shared between client and module projects. Not part of public API. |
| `Testing`         | Test and example code in further sub-projects. |
| `WASimClient`     | The main WASim API Client implementation. |
| `WASimClient_CLI` | The managed .NET "wrapper" of `WASimClient`, using C++/CLI. |
| `WASimModule`     | The MSFS 2020 WASM module implementation of the Server component. |
| `WASimUI`         | Full featured GUI for testing/exploring WASimCommander features. |

