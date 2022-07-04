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

#include <string>

#include "WASimCommander.h"
#include "utilities.h"
//#include "SimConnectRequestTracker.h"

namespace WASimCommander {
	namespace Utilities
{
	class SimConnectRequestTracker;  // !! this only works if SimConnectRequestTracker.h is #included before this header !!

	static bool ENABLE_SIMCONNECT_REQUEST_TRACKING = false;  // this can be changed even at runtime

	// The macro allows us to only pass the function name once to get both the string version and the actual callable.
	// "SimConnect_" is automatically prepended to the callable version.
	// (For all the fancy new C++ features, no std way to get function name even at compile time... we still need macros :-| )
	#define INVOKE_SIMCONNECT(F, ...)  WASimCommander::Utilities::SimConnectHelper::invokeSimConnect(#F, &SimConnect_##F, __VA_ARGS__)

	class WSMCMND_API SimConnectHelper
	{
	public:
		static SimConnectRequestTracker *simRequestTracker();

		// Main SimConnect function invoker template which does the actual call, logs any error or creates a request record, and returns the HRESULT from SimConnect.
		template<typename... Args>
		static HRESULT simConnectProxy(const char *fname, std::function<HRESULT(HANDLE, Args...)> f, HANDLE hSim, Args... args)
		{
			const HRESULT hr = std::bind(f, std::forward<HANDLE>(hSim), std::forward<Args>(args)...)();
			if FAILED(hr)
				LOG_ERR << "Error: " << fname << '(' << SimConnectRequestTracker::printArgs(args...) << ") failed with " << LOG_HR(hr);
			else if (ENABLE_SIMCONNECT_REQUEST_TRACKING)
				simRequestTracker()->addRequestRecord(hSim, fname, args...);
			return hr;
		}

		// Because we really want type deduction, we can invoke simConnectProxy() via this template w/out specifying each argument type.
		template<typename... Args>
		static HRESULT invokeSimConnect(const char *fname, HRESULT(*f)(HANDLE, Args...), HANDLE hSim, Args... args)
		{
			return simConnectProxy(fname, std::function<HRESULT(HANDLE, Args...)>(f), hSim, args...);
		}

		static void setMaxTrackedRequests(uint32_t maxRecords);
		static void logSimConnectException(void *pData);
		static void logSimVersion(void* pData);

		static HRESULT addClientDataDefinition(HANDLE hSim, DWORD cddId, DWORD szOrType, float epsilon = 0.0f, DWORD offset = -1 /*SIMCONNECT_CLIENTDATAOFFSET_AUTO*/);
		static HRESULT removeClientDataDefinition(HANDLE hSim, DWORD cddId);
		static HRESULT registerDataArea(HANDLE hSim, const std::string &name, DWORD cdaID, DWORD cddId, DWORD szOrType, bool createCDA, bool readonly = false, float epsilon = 0.0f);
		static HRESULT newClientEvent(HANDLE hSim, DWORD evId, const std::string &evName, DWORD grpId = -1, DWORD priority = 0, bool maskable = false);
	};

	}  // Utilities
}  // WASimCommander
