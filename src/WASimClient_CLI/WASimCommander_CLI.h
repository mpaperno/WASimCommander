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

#pragma once

#include "./Enums.h"
#include "./Structs.h"

using namespace System;

/// WASimCommander::CLI namespace.
/// Container for implementation of the C++ `WASimCommander` API and `WASimClient` as a C++/CLI .NET "wrapper."
/// The primary documentation is for the C++ version equivalents. Documentation for everything in the CLI namespace
/// focuses primarily on any (non-obvious) differences from the C++ version.
namespace WASimCommander::CLI
{
	/// <summary> Predefined value types </summary>
	/// Using these constants for the `WASimCommander::CLI::Structs::DataRequest::valueSize` property will allow delta epsilon comparisons.
	public value struct ValueTypes {
		literal UInt32 DATA_TYPE_INT8   = DATA_TYPE_INT8;    ///<  8-bit integer number (signed or unsigned)
		literal UInt32 DATA_TYPE_INT16  = DATA_TYPE_INT16;   ///< 16-bit integer number (signed or unsigned)
		literal UInt32 DATA_TYPE_INT32  = DATA_TYPE_INT32;   ///< 32-bit integer number (signed or unsigned)
		literal UInt32 DATA_TYPE_INT64  = DATA_TYPE_INT64;   ///< 64-bit integer number (signed or unsigned)
		literal UInt32 DATA_TYPE_FLOAT  = DATA_TYPE_FLOAT;   ///< 32-bit floating-point number
		literal UInt32 DATA_TYPE_DOUBLE = DATA_TYPE_DOUBLE;  ///< 64-bit floating-point number
	};
}
