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

#ifdef min
#undef min
#undef max
#endif
#include <type_traits>

#ifdef WSMCMND_ENUM_EXPORT
#undef WSMCMND_ENUM_EXPORT
#undef WSMCMND_ENUM_NAMESPACE
#endif

//#include "client/WASimClient.h"
#define WSMCMND_ENUM_EXPORT public
#define WSMCMND_ENUM_NAMESPACE WASimCommander::CLI::Enums
#include "enums_impl.h"
#include "client/enums_impl.h"

/// WASimCommander::CLI::Enums namespace. C++/CLI specific definitions only.
/// See documentation for `WASimCommander::Enums` and `WASimCommander::Client` namespaces for main API and Client enum classes respectively.
namespace WASimCommander::CLI::Enums
{

	/// Custom "+" operator for strong C++ enum types to cast to underlying type.
	template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
	constexpr auto operator+(T e) noexcept { return static_cast<std::underlying_type_t<T>>(e); }

	/// <summary> Method return status values; HRESULT "alias" </summary>
	public enum class HR
	{
		OK = S_OK,       ///< Success status.
		FAIL = E_FAIL,   ///< General error status,
		INVALIDARG    = E_INVALIDARG,  ///< Invalid method argument.
		NOT_CONNECTED = int(/*ERROR_NOT_CONNECTED*/ 2250L | (/*FACILITY_WIN32*/ 7 << 16) | 0x80000000),  ///< Error result: server not connected.
		TIMEOUT       = int(/*ERROR_TIMEOUT*/       1460L | (/*FACILITY_WIN32*/ 7 << 16) | 0x80000000),  ///< Error result: timeout communicating with server.
	};

}
