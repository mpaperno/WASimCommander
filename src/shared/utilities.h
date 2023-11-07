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

// ----------  NOTE ----------------
// THE CONTENTS OF THIS FILE ARE NOT PART OF THE PUBLIC API.
// The functions here are shared between implementations of the WASimCommander Server
// and Client and are considered "implementation details" which may change at any time.
// ---------------------------------

#pragma once

#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>

//#include <SimConnect.h>

#include "WASimCommander.h"
//#include "SimConnectRequestTracker.h"

#define LOGFAULT_USE_UTCZONE 0
#define LOGFAULT_TIME_PRINT_TIMEZONE 0
#define LOGFAULT_USE_SHORT_LOG_MACROS 1
#define LOGFAULT_TIME_PRINT_MILLISECONDS 1
#define LOGFAULT_TIME_FORMAT "%m-%d %H:%M:%S."
#define LOGFAULT_LEVEL_NAMES {"[NON]", "[CRT]", "[ERR]", "[WRN]", "[INF]", "[DBG]", "[TRC]"}
#if defined(_LIBCPP_HAS_NO_THREADS) || defined(__cplusplus_cli)
#  define LOGFAULT_USE_THREADING 0
#else
#  define LOGFAULT_USE_THREADING 1
#endif
#ifndef LOGFAULT_THREAD_NAME
#  define LOGFAULT_THREAD_NAME  WSMCMND_PROJECT_NAME
#endif
#include "logfault.h"

#define WSMCMND_SHORT_NAME    "WASim"

#define STREAM_HEX    LOG_HEX
#define STREAM_HEX2   LOG_HEX2
#define STREAM_HEX4   LOG_HEX4
#define STREAM_HEX8   LOG_HEX8
#define LOG_HR(hr) "HRESULT 0x" << STREAM_HEX8(hr)

#define LOG_SC_RECV(D)             "[SIMCONNECT_RECV] ID: " << D->dwID << "; Size: " << D->dwSize
#define LOG_SC_RECV_EVENT(D)       "[SIMCONNECT_RECV_EVENT] GroupID: " << D->uGroupID << "; EventID: " << D->uEventID << "; Data: " << D->dwData
#define LOG_SC_RCV_CLIENT_DATA(D)  "[SIMCONNECT_RECV_CLIENT_DATA] ObjID: " << D->dwObjectID << "; DefID: " << D->dwDefineID << "; ReqID: " << D->dwRequestID << "; DefineCount: " << D->dwDefineCount

namespace WASimCommander {
	namespace Utilities
{

	// Event name constants for building event and data area names.
	static const char EVENT_NAME_CONNECT[]  = WSMCMND_COMMON_NAME_PREFIX WSMCMND_EVENT_NAME_CONNECT;
	static const char EVENT_NAME_PING[]     = WSMCMND_COMMON_NAME_PREFIX WSMCMND_EVENT_NAME_PING;
	static const char EVENT_NAME_PING_PFX[] = WSMCMND_COMMON_NAME_PREFIX WSMCMND_EVENT_NAME_PING ".";    // + 8 char client name
	static const char CDA_NAME_CMD_PFX[]    = WSMCMND_COMMON_NAME_PREFIX WSMCMND_CDA_NAME_COMMAND ".";   // + 8 char client name
	static const char CDA_NAME_RESP_PFX[]   = WSMCMND_COMMON_NAME_PREFIX WSMCMND_CDA_NAME_RESPONSE ".";  // + 8 char client name
	static const char CDA_NAME_LOG_PFX[]    = WSMCMND_COMMON_NAME_PREFIX WSMCMND_CDA_NAME_LOG ".";       // + 8 char client name
	static const char CDA_NAME_DATA_PFX[]   = WSMCMND_COMMON_NAME_PREFIX WSMCMND_CDA_NAME_DATA ".";      // + 8 char client name [+ "." + request ID (0-65535)]
	static const char CDA_NAME_KEYEV_PFX[]  = WSMCMND_COMMON_NAME_PREFIX WSMCMND_CDA_NAME_KEYEVENT ".";  // + 8 char client name

	static bool isIndexedVariableType(const char type) {
		static const std::vector<char> VAR_TYPES_INDEXED    = { 'A', 'L', 'T' };
		return find(VAR_TYPES_INDEXED.cbegin(), VAR_TYPES_INDEXED.cend(), type) != VAR_TYPES_INDEXED.cend();
	}

	static bool isUnitBasedVariableType(const char type) {
		static const std::vector<char> VAR_TYPES_UNIT_BASED = { 'A', 'C', 'E', 'L', 'P' };
		return find(VAR_TYPES_UNIT_BASED.cbegin(), VAR_TYPES_UNIT_BASED.cend(), type) != VAR_TYPES_UNIT_BASED.cend();
	}

	static bool isSettableVariableType(const char type) {
		static const std::vector<char> VAR_TYPES_SETTABLE = { 'A', 'C', 'H', 'K', 'L', 'Z' };
		return find(VAR_TYPES_SETTABLE.cbegin(), VAR_TYPES_SETTABLE.cend(), type) != VAR_TYPES_SETTABLE.cend();
	}

	// returns actual byte size from given size which may be one of the SimConnect_AddToClientDataDefinition() dwSizeOrType constants
	static constexpr uint32_t getActualValueSize(DWORD dwSizeOrType)
	{
		if (dwSizeOrType < DATA_TYPE_DOUBLE)
			return dwSizeOrType;
		switch (dwSizeOrType) {
			case DATA_TYPE_INT8:
				return 1;
			case DATA_TYPE_INT16:
				return 2;
			case DATA_TYPE_INT32:
			case DATA_TYPE_FLOAT:
				return 4;
			case DATA_TYPE_INT64:
			case DATA_TYPE_DOUBLE:
				return 8;
			default:
				return dwSizeOrType;
		}
	}

	static inline bool fuzzyCompare(float p1, float p2) 	{
		p1 += 1.0f; p2 += 1.0f;
		return (std::fabs(p1 - p2) * 1000000.f <= std::min(std::fabs(p1), std::fabs(p2)));
	}

	// Custom "+" operator for strong enum types to cast to underlying type.
	template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
	constexpr auto operator+(T e) noexcept {
		return static_cast<std::underlying_type_t<T>>(e);
	}

	template <typename Range>
	static int indexOfString(const Range &range, const char *c) noexcept
	{
		int i = 0;
		auto b = std::cbegin(range), e = std::cend(range);
		while (b != e && strcmp(*b, c))
			++b, ++i;
		return b == e ? -1 : i;
	}

	template <typename Enum, typename Range, std::enable_if_t<std::is_enum<Enum>::value, bool> = true>
	static const auto getEnumName(Enum e, const Range &range) noexcept
	{
		using T = typename std::iterator_traits<decltype(std::begin(range))>::value_type;
		const size_t len = std::cend(range) - std::cbegin(range);
		if ((size_t)e < len)
			return range[(size_t)e];
		return T();
	}

	template <typename T = uint8_t>
	static const std::string byteArrayToHex(const T *range, size_t len, const char sep = ':') noexcept
	{
		std::ostringstream os;
		for (size_t i=0; i < len; ++i) {
			if (i)
				os << sep;
			os << STREAM_HEX2(((unsigned short)*(range + i) & 0xFF));
		}
		return os.str();
	}

	template<typename Tp = std::chrono::system_clock::time_point>
	static std::string timePointToString(const Tp &tp, const std::string &format = std::string(LOGFAULT_TIME_FORMAT), bool withMs = true, bool utc = true)
	{
		const typename Tp::duration tt = tp.time_since_epoch();
		const time_t durS = std::chrono::duration_cast<std::chrono::seconds>(tt).count();
		std::ostringstream ss;
		if (const std::tm *tm = (utc ? std::gmtime(&durS) : std::localtime(&durS))) {
			ss << std::put_time(tm, format.c_str());
			if (withMs) {
				const long long durMs = std::chrono::duration_cast<std::chrono::milliseconds>(tt).count();
				ss << std::setw(3) << std::setfill('0') << int(durMs - durS * 1000);
			}
		}
		// gmtime/localtime() returned null
		else {
			ss << "<FORMAT ERROR>";
		}
		return ss.str();
	}

	}  // Utilities
}  // WASimCommander
