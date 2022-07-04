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

#include "global.h"

#if defined(_MSC_VER) && defined(WSMCMND_API_IMPORT)

// This exercise is mainly to silence MSVC warnings (C4251) when exporting (to DLL) some variables which use stdlib templated types.
// It does not actually make any difference since the produced DLLs are compiler-specific anyway. And we're only exporting
// very basic vector types which are simply dynamic memory allocation (std::string just being a glorified char array, after all).
// We could probably just as well disable the warning... but we'll try to do it "the right way."
//#pragma warning( disable: 4251 )

#include <string>
#include <vector>

// for string (basic_string<char>)
WSMCMND_API_IMPORT template class WSMCMND_API std::allocator<char>;
WSMCMND_API_IMPORT template union WSMCMND_API std::_String_val<std::_Simple_types<char>>::_Bxty;
WSMCMND_API_IMPORT template class WSMCMND_API std::_String_val<std::_Simple_types<char>>;
WSMCMND_API_IMPORT template class WSMCMND_API std::_Compressed_pair<std::allocator<char>,std::_String_val<std::_Simple_types<char>>,true>;
WSMCMND_API_IMPORT template class WSMCMND_API std::basic_string<char, std::char_traits<char>, std::allocator<char>>;
// for vector<uint8_t>
WSMCMND_API_IMPORT template class WSMCMND_API std::allocator<uint8_t>;
WSMCMND_API_IMPORT template class WSMCMND_API std::_Vector_val<std::_Simple_types<uint8_t>>;
WSMCMND_API_IMPORT template class WSMCMND_API std::_Compressed_pair<std::allocator<uint8_t>,std::_Vector_val<std::_Simple_types<uint8_t>>,true>;
WSMCMND_API_IMPORT template class WSMCMND_API std::vector<uint8_t, std::allocator<uint8_t>>;
// for vector< pair< int, string > >
WSMCMND_API_IMPORT template class WSMCMND_API std::_Vector_val<std::_Simple_types<std::pair<int,std::string>>>;
WSMCMND_API_IMPORT template class WSMCMND_API std::_Compressed_pair<std::allocator<std::pair<int,std::string>>,std::_Vector_val<std::_Simple_types<std::pair<int,std::string>>>,true>;
WSMCMND_API_IMPORT template class WSMCMND_API std::vector<std::pair<int,std::string>,std::allocator<std::pair<int,std::string>>>;

#endif
