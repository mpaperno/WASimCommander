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

// the macros _staticLibrary/_library/_exe are set in the common.props file at the src root
#ifndef WSMCMND_API
#  if defined(WSMCMND_API_STATIC) || defined(_staticlibrary) || defined(_MSFS_WASM)
#    define WSMCMND_API
#  else
#    if defined(WSMCMND_API_EXPORT) || defined(_library)
#      define WSMCMND_API __declspec(dllexport)
#      define WSMCMND_API_IMPORT
#    else  /* defined(_exe) */
#      define WSMCMND_API __declspec(dllimport)
#      define WSMCMND_API_IMPORT extern
#    endif
#  endif
#endif

// grrr....
#ifdef min
#undef min
#undef max
#endif
