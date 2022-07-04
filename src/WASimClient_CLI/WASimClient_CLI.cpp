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

#include "WASimClient_CLI.h"

using namespace WASimCommander::CLI;
using namespace WASimCommander::CLI::Client;

ref class WASimClient::Private
{
public:
	// apparently we have to redeclare these w/out the std::function<> type  :-/
	using clientEventCallback_t = void(__stdcall *)(const WASimCommander::Client::ClientEvent &);
	using listResultsCallback_t = void(__stdcall *)(const WASimCommander::Client::ListResult &);
	using dataCallback_t = void(__stdcall *)(const WASimCommander::Client::DataRequestRecord &);
	using logCallback_t = void(__stdcall *)(const WASimCommander::LogRecord &, WASimCommander::Client::LogSource);
	using commandCallback_t = void(__stdcall *)(const WASimCommander::Command &);

	WASimClient ^q = nullptr;
	WASimCommander::Client::WASimClient *client = nullptr;
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void ClientEventCallback(const WASimCommander::Client::ClientEvent &);
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void ListResultsCallback(const WASimCommander::Client::ListResult &);
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void DataCallback(const WASimCommander::Client::DataRequestRecord &);
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void LogCallback(const WASimCommander::LogRecord &, WASimCommander::Client::LogSource);
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void CommandResultCallback(const WASimCommander::Command &);
	[UnmanagedFunctionPointer(CallingConvention::StdCall)]
	delegate void ResponseCallback(const WASimCommander::Command &);

	[MarshalAs(UnmanagedType::FunctionPtr)]
	ClientEventCallback ^clientEventCb;
	[MarshalAs(UnmanagedType::FunctionPtr)]
	ListResultsCallback ^listResultsCb;
	[MarshalAs(UnmanagedType::FunctionPtr)]
	DataCallback ^dataCb;
	[MarshalAs(UnmanagedType::FunctionPtr)]
	LogCallback ^logCb;
	[MarshalAs(UnmanagedType::FunctionPtr)]
	CommandResultCallback ^resultCb;
	[MarshalAs(UnmanagedType::FunctionPtr)]
	ResponseCallback ^responseCb;

	ClientEventDelegate ^clientEventHandler;
	ListResultsDelegate ^listResultsHandler;
	DataDelegate ^dataHandler;
	LogDelegate ^logHandler;
	CommandResultDelegate ^resultHandler;
	ResponseDelegate ^responseHandler;

	Private(WASimClient ^q, WASimCommander::Client::WASimClient *c) :
		q(q), client(c)
	{
		clientEventCb = gcnew ClientEventCallback(this, &Private::onClientEvent);
		IntPtr pCb = Marshal::GetFunctionPointerForDelegate(clientEventCb);
		client->setClientEventCallback(static_cast<clientEventCallback_t>(pCb.ToPointer()));

		listResultsCb = gcnew ListResultsCallback(this, &Private::onListResult);
		pCb = Marshal::GetFunctionPointerForDelegate(listResultsCb);
		client->setListResultsCallback(static_cast<listResultsCallback_t>(pCb.ToPointer()));

		dataCb = gcnew DataCallback(this, &Private::onDataResult);
		pCb = Marshal::GetFunctionPointerForDelegate(dataCb);
		client->setDataCallback(static_cast<dataCallback_t>(pCb.ToPointer()));

		logCb = gcnew LogCallback(this, &Private::onLogRecord);
		pCb = Marshal::GetFunctionPointerForDelegate(logCb);
		client->setLogCallback(static_cast<logCallback_t>(pCb.ToPointer()));

		resultCb = gcnew CommandResultCallback(this, &Private::onCommandResult);
		pCb = Marshal::GetFunctionPointerForDelegate(resultCb);
		client->setCommandResultCallback(static_cast<commandCallback_t>(pCb.ToPointer()));

		responseCb = gcnew ResponseCallback(this, &Private::onServerResponse);
		pCb = Marshal::GetFunctionPointerForDelegate(responseCb);
		client->setResponseCallback(static_cast<commandCallback_t>(pCb.ToPointer()));
	}

	void onClientEvent(const WASimCommander::Client::ClientEvent &ev)
	{
		if (clientEventHandler)
			clientEventHandler->Invoke(gcnew ClientEvent(ev));
	}

	void onListResult(const WASimCommander::Client::ListResult &lr)
	{
		if (listResultsHandler)
			listResultsHandler->Invoke(gcnew ListResult(lr));
	}

	void onDataResult(const WASimCommander::Client::DataRequestRecord &ev)
	{
		if (dataHandler)
			dataHandler->Invoke(gcnew DataRequestRecord(ev));
	}

	void onLogRecord(const WASimCommander::LogRecord &lr, WASimCommander::Client::LogSource s)
	{
		if (logHandler)
			logHandler->Invoke(gcnew LogRecord(lr), (LogSource)s);
	}

	void onCommandResult(const WASimCommander::Command &cmd)
	{
		if (resultHandler)
			resultHandler->Invoke(gcnew Command(cmd));
	}

	void onServerResponse(const WASimCommander::Command &cmd)
	{
		if (responseHandler)
			responseHandler->Invoke(gcnew Command(cmd));
	}

	inline HR executeCalculatorCode(String ^ code, CalcResultType resultType, interior_ptr<double> pfResult, interior_ptr<String ^> psResult)
	{
		pin_ptr<double> pf = pfResult;
		std::string s { };
		const HRESULT hr = client->executeCalculatorCode(
			marshal_as<std::string>(code),
			(WASimCommander::Enums::CalcResultType)resultType,
			pf, psResult ? &s : nullptr
		);
		if (psResult)
			*psResult = marshal_as<String ^>(s);
		return (HR)hr;
	}

	~Private()
	{
		if (client) {
			client->setListResultsCallback(nullptr);
			client->setDataCallback(nullptr);
			client->setCommandResultCallback(nullptr);
			client->setResponseCallback(nullptr);
			client->setClientEventCallback(nullptr);
			client->setLogCallback(nullptr);
		}

		delete clientEventHandler;
		delete listResultsHandler;
		delete dataHandler;
		delete logHandler;
		delete resultHandler;
		delete responseHandler;

		delete clientEventCb;
		delete listResultsCb;
		delete dataCb;
		delete logCb;
		delete resultCb;
		delete responseCb;
	}

};


WASimClient::WASimClient(UInt32 clientId, String ^configFile) :
	m_client( new WASimCommander::Client::WASimClient(clientId, marshal_as<std::string>(configFile))),
	d { gcnew Private(this, m_client) }
{}

WASimClient::WASimClient(UInt32 clientId) : WASimClient(clientId, String::Empty)
{ }

WASimClient::~WASimClient()
{
	delete d;
	d = nullptr;
	this->!WASimClient();
}

WASimClient::!WASimClient()
{
	if (m_client)
		m_client->disconnectSimulator();
	delete m_client;
	m_client = nullptr;
}

inline HR WASimClient::executeCalculatorCode(String ^ code, CalcResultType resultType, [Out] double % pfResult) {
	return d->executeCalculatorCode(code, resultType, &pfResult, nullptr);
}

inline HR WASimClient::executeCalculatorCode(String ^ code, CalcResultType resultType, [Out] String ^% psResult) {
	return d->executeCalculatorCode(code, resultType, nullptr, &psResult);
}

inline HR WASimClient::executeCalculatorCode(String ^ code, CalcResultType resultType, [Out] double % pfResult, [Out] String ^% psResult) {
	return d->executeCalculatorCode(code, resultType, &pfResult, &psResult);
}

inline array<DataRequestRecord^>^ WASimClient::dataRequests()
{
	const std::vector<WASimCommander::Client::DataRequestRecord> &res = m_client->dataRequests();
	array<DataRequestRecord ^> ^ret = gcnew array<DataRequestRecord ^>((int)res.size());
	int i = 0;
	for (const auto &dr : res)
		ret[i++] = gcnew DataRequestRecord(dr);
	return ret;
}

inline array<UInt32>^ WASimClient::dataRequestIdsList()
{
	const std::vector<uint32_t> &res = m_client->dataRequestIdsList();
	array<UInt32> ^ret = gcnew array<UInt32>((int)res.size());
	if (res.size()) {
		pin_ptr<UInt32> pp = &ret[0];
		memcpy(pp, res.data(), res.size() * 4);
	}
	return ret;
}

inline array<RegisteredEvent^>^ WASimClient::registeredEvents()
{
	const std::vector<WASimCommander::Client::RegisteredEvent> &res = m_client->registeredEvents();
	array<RegisteredEvent ^> ^ret = gcnew array<RegisteredEvent ^>((int)res.size());
	int i = 0;
	for (const auto &ev : res)
		ret[i++] = gcnew RegisteredEvent(ev);
	return ret;
}

#define DELEGATE_HANDLER(E, D, H)      \
		void WASimClient::E::add(D ^ h) { H += h; }     \
		void WASimClient::E::remove(D ^ h) { H -= h; }

DELEGATE_HANDLER(OnClientEvent, ClientEventDelegate, d->clientEventHandler)
DELEGATE_HANDLER(OnListResults, ListResultsDelegate, d->listResultsHandler)
DELEGATE_HANDLER(OnDataReceived, DataDelegate, d->dataHandler)
DELEGATE_HANDLER(OnLogRecordReceived, LogDelegate, d->logHandler)
DELEGATE_HANDLER(OnCommandResult, CommandResultDelegate, d->resultHandler)
DELEGATE_HANDLER(OnResponseReceived, ResponseDelegate, d->responseHandler)

#undef DELEGATE_HANDLER
