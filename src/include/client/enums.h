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

#ifdef WSMCMND_ENUM_EXPORT
#undef WSMCMND_ENUM_EXPORT
#undef WSMCMND_ENUM_NAMESPACE
#endif

#define WSMCMND_ENUM_EXPORT
#define WSMCMND_ENUM_NAMESPACE WASimCommander::Client

#include "./enums_impl.h"
