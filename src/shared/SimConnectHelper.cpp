
#if defined(_MSFS_WASM)
#  if !defined(MAX_PATH)
#    include <MSFS/MSFS_WindowsTypes.h>
#  endif
#elif !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

#include <SimConnect.h>
#include "utilities.h"
#include "SimConnectRequestTracker.h"
#include "SimConnectHelper.h"

using namespace WASimCommander::Utilities;

SimConnectRequestTracker *SimConnectHelper::simRequestTracker()
{
	static SimConnectRequestTracker tracker { /*maxRecords*/ 200 };
	return &tracker;
}

void SimConnectHelper::setMaxTrackedRequests(uint32_t maxRecords)
{
	simRequestTracker()->setMaxRecords(maxRecords);
}

void SimConnectHelper::logSimConnectException(void *pData)
{
	SIMCONNECT_RECV_EXCEPTION *data = reinterpret_cast<SIMCONNECT_RECV_EXCEPTION *>(pData);
	if (ENABLE_SIMCONNECT_REQUEST_TRACKING)
		LOG_WRN << "SimConnect exception: " << simRequestTracker()->getRequestRecord(data->dwSendID, data->dwException, data->dwIndex);
	else
		LOG_WRN << "SimConnect exception: " << SimConnectRequestTracker::exceptionName(data->dwException) << "; SendId: " << data->dwSendID << "; Index: " << data->dwIndex;
}

void SimConnectHelper::logSimVersion(void *pData)
{
	SIMCONNECT_RECV_OPEN *d = reinterpret_cast<SIMCONNECT_RECV_OPEN *>(pData);
	LOG_INF << "Connected to Simulator " << std::quoted(d->szApplicationName)
		<< " v" << d->dwApplicationVersionMajor << '.' << d->dwApplicationVersionMinor << '.' << d->dwApplicationBuildMajor << '.' << d->dwApplicationBuildMinor
		<< ", with SimConnect v" << d->dwSimConnectVersionMajor << '.' << d->dwSimConnectVersionMinor << '.' << d->dwSimConnectBuildMajor << '.' << d->dwSimConnectBuildMinor
		<< " (Server v" << d->dwVersion << ')';
}

HRESULT SimConnectHelper::addClientDataDefinition(HANDLE hSim, DWORD cddId, DWORD szOrType, float epsilon, DWORD offset)
{
	return INVOKE_SIMCONNECT(AddToClientDataDefinition, hSim, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)cddId, offset, szOrType, epsilon, SIMCONNECT_UNUSED);
}

HRESULT SimConnectHelper::removeClientDataDefinition(HANDLE hSim, DWORD cddId)
{
	return INVOKE_SIMCONNECT(ClearClientDataDefinition, hSim, (SIMCONNECT_CLIENT_DATA_DEFINITION_ID)cddId);
}

HRESULT SimConnectHelper::registerDataArea(HANDLE hSim, const std::string & name, DWORD cdaID, DWORD cddId, DWORD szOrType, bool createCDA, bool readonly, float epsilon)
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

HRESULT SimConnectHelper::newClientEvent(HANDLE hSim, DWORD evId, const std::string & evName, DWORD grpId, DWORD priority, bool maskable)
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
