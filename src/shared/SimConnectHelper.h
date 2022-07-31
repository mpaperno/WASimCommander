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
#ifndef _LIBCPP_HAS_NO_THREADS
#  include <mutex>
#endif

#include "WASimCommander.h"
#include "utilities.h"
#include "SimConnectRequestTracker.h"

namespace WASimCommander {
	namespace Utilities {
		namespace SimConnectHelper {

	static bool ENABLE_SIMCONNECT_REQUEST_TRACKING = false;  // this can be changed even at runtime
#ifndef _LIBCPP_HAS_NO_THREADS
	static std::mutex g_simConnectMutex;
#endif

	// The macro allows us to only pass the function name once to get both the string version and the actual callable.
	// "SimConnect_" is automatically prepended to the callable version.
	// (For all the fancy new C++ features, no std way to get function name even at compile time... we still need macros :-| )
	#define INVOKE_SIMCONNECT(F, ...)  WASimCommander::Utilities::SimConnectHelper::invokeSimConnect(#F, &SimConnect_##F, __VA_ARGS__)

	static SimConnectRequestTracker *simRequestTracker()
	{
		static SimConnectRequestTracker tracker { /*maxRecords*/ 200 };
		return &tracker;
	}

	// Main SimConnect function invoker template which does the actual call, logs any error or creates a request record, and returns the HRESULT from SimConnect.
	template<typename... Args>
	static HRESULT simConnectProxy(const char *fname, std::function<HRESULT(HANDLE, Args...)> f, HANDLE hSim, Args... args)
	{
#ifndef _LIBCPP_HAS_NO_THREADS
		std::lock_guard lock(g_simConnectMutex);
#endif
		const HRESULT hr = std::bind(f, std::forward<HANDLE>(hSim), std::forward<Args>(args)...)();
		if FAILED(hr)
			LOG_ERR << "Error: " << fname << '(' << SimConnectRequestTracker::printArgs(args...) << ") failed with " << LOG_HR(hr);
		else if (ENABLE_SIMCONNECT_REQUEST_TRACKING)
			SimConnectHelper::simRequestTracker()->addRequestRecord(hSim, fname, args...);
		return hr;
	}

	// Because we really want type deduction, we can invoke simConnectProxy() via this template w/out specifying each argument type.
	template<typename... Args>
	static HRESULT invokeSimConnect(const char *fname, HRESULT(*f)(HANDLE, Args...), HANDLE hSim, Args... args)	{
		return simConnectProxy(fname, std::function<HRESULT(HANDLE, Args...)>(f), hSim, args...);
	}

	static void setMaxTrackedRequests(uint32_t maxRecords) {
		simRequestTracker()->setMaxRecords(maxRecords);
	}

	static void logSimConnectException(void *pData)
	{
		SIMCONNECT_RECV_EXCEPTION *data = reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(pData);
		if (ENABLE_SIMCONNECT_REQUEST_TRACKING)
			LOG_WRN << "SimConnect exception: " << simRequestTracker()->getRequestRecord(data->dwSendID, data->dwException, data->dwIndex);
		else
			LOG_WRN << "SimConnect exception: " << SimConnectRequestTracker::exceptionName(data->dwException) << "; SendId: " << data->dwSendID << "; Index: " << data->dwIndex;
	}

	static void logSimVersion(void* pData)
	{
		SIMCONNECT_RECV_OPEN *d = reinterpret_cast<SIMCONNECT_RECV_OPEN *>(pData);
		LOG_INF << "Connected to Simulator " << std::quoted(d->szApplicationName)
			<< " v" << d->dwApplicationVersionMajor << '.' << d->dwApplicationVersionMinor << '.' << d->dwApplicationBuildMajor << '.' << d->dwApplicationBuildMinor
			<< ", with SimConnect v" << d->dwSimConnectVersionMajor << '.' << d->dwSimConnectVersionMinor << '.' << d->dwSimConnectBuildMajor << '.' << d->dwSimConnectBuildMinor
			<< " (Server v" << d->dwVersion << ')';
	}

	static HRESULT addClientDataDefinition(HANDLE hSim, DWORD cddId, DWORD szOrType, float epsilon = 0.0f, DWORD offset = -1 /*SIMCONNECT_CLIENTDATAOFFSET_AUTO*/)
	{
		return INVOKE_SIMCONNECT(AddToClientDataDefinition, hSim, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)cddId, offset, szOrType, epsilon, SIMCONNECT_UNUSED);
	}

	static HRESULT removeClientDataDefinition(HANDLE hSim, DWORD cddId)
	{
		return INVOKE_SIMCONNECT(ClearClientDataDefinition, hSim, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)cddId);
	}

	static HRESULT registerDataArea(HANDLE hSim, const std::string &name, DWORD cdaID, DWORD cddId, DWORD szOrType, bool createCDA, bool readonly = false, float epsilon = 0.0f)
	{
		HRESULT hr;
		// Map a unique named data storage area to CDA ID.
		if FAILED(hr = INVOKE_SIMCONNECT(MapClientDataNameToID, hSim, name.c_str(), (SIMCONNECT_CLIENT_DATA_ID)cdaID))
			return hr;
		if (createCDA) {
			// Reserve data storage area for actual size using the CDA ID.
			const uint32_t actualSize = getActualValueSize(szOrType);
			if FAILED(hr = INVOKE_SIMCONNECT(CreateClientData, hSim, cdaID, (DWORD)actualSize, readonly ? SIMCONNECT_CREATE_CLIENT_DATA_FLAG_READ_ONLY : SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT))
				return hr;
		}
		// Define a single storage space within the data store using the CDA DefineID, of the full given size and automatic offset.
		if (cddId)
			return addClientDataDefinition(hSim, cddId, szOrType, epsilon);
		return S_OK;
	}

	static HRESULT newClientEvent(HANDLE hSim, DWORD evId, const std::string &evName, DWORD grpId = -1, DWORD priority = 0, bool maskable = false)
	{
		HRESULT hr;
		if FAILED(hr = INVOKE_SIMCONNECT(MapClientEventToSimEvent, hSim, (SIMCONNECT_CLIENT_EVENT_ID)evId, evName.c_str()))
			return hr;
		if ((int)grpId == -1)
			return hr;
		if FAILED(hr = INVOKE_SIMCONNECT(AddClientEventToNotificationGroup, hSim, (SIMCONNECT_NOTIFICATION_GROUP_ID)grpId, (SIMCONNECT_CLIENT_EVENT_ID)evId, (BOOL)maskable))
			return hr;
		if (priority)
			hr = INVOKE_SIMCONNECT(SetNotificationGroupPriority, hSim, (SIMCONNECT_NOTIFICATION_GROUP_ID)grpId, priority);
		return hr;
	}

		} // SimConnectHelper
	}  // Utilities
}  // WASimCommander
