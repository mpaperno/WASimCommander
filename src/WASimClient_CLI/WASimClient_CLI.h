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

#include <iostream>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>

#include "client/WASimClient.h"
#include "WASimCommander_CLI.h"

/// \file

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace msclr::interop;

/// WASimCommander::CLI::Client namespace.
/// Implementation of the C++ WASimClient as a C++/CLI .NET "wrapper."
namespace WASimCommander::CLI::Client
{
	using namespace WASimCommander::CLI::Enums;
	using namespace WASimCommander::CLI::Structs;
	#define WSE  WASimCommander::Enums

	/// C+/CLI wrapper implementation of `WASimCommander::Client::WASimClient`.
	/// See documentation for the C++ class for most of the details. Only implementation differences are documented here.
	///
	/// This is pretty much a method-for-method version of the C++ version, with parameter types adjusted accordingly,
	/// and using the CLI versions of relevant data structures and enum types.
	///
	/// The main difference is that callbacks from the C++ version are delivered here as managed Events, with defined delegate types to handle them.
	/// And unlike the callback system, the events can have multiple subscribers if needed.
	public ref class WASimClient
	{
	public:

		// Delegates  -----------------------------------

		delegate void ClientEventDelegate(ClientEvent ^);    ///< Event delegate for Client events (`OnClientEvent`)
		delegate void ListResultsDelegate(ListResult ^);     ///< Event delegate for delivering list results, eg. of local variables sent from Server (`OnListResults`).
		delegate void DataDelegate(DataRequestRecord ^);     ///< Event delegate for subscription result data (`OnDataReceived`).
		delegate void LogDelegate(LogRecord ^, LogSource);   ///< Event delegate for log entries (from both Client and Server) (`OnLogRecordReceived`).
		delegate void CommandResultDelegate(Command ^);      ///< Event delegate for command responses returned from server (`OnCommandResult`).
		delegate void ResponseDelegate(Command ^);           ///< Event delegate for all Command structures received from server (`OnResponseReceived`).

		// Events  -----------------------------------

		#define DELEGATE_DECL(D)  { void add(D^); void remove(D^); }
		/// <summary> WASimClient events like connection and disconnection. </summary>
		event ClientEventDelegate ^ OnClientEvent     DELEGATE_DECL(ClientEventDelegate);
		/// <summary> Delivers list results after a successful `WASimClient.list()` command. </summary>
		event ListResultsDelegate ^ OnListResults     DELEGATE_DECL(ListResultsDelegate);
		/// <summary> Delivers data value subscription updates as they arrive from the Server. </summary>
		event DataDelegate ^ OnDataReceived           DELEGATE_DECL(DataDelegate);
		/// <summary>Log records delivered from Client and/or Server. </summary>
		event LogDelegate ^ OnLogRecordReceived       DELEGATE_DECL(LogDelegate);
		/// <summary> This event is triggered whenever any response to a Command is returned from the Server. Responses are Command objects of `Ack` or `Nak` type. </summary>
		event CommandResultDelegate ^ OnCommandResult DELEGATE_DECL(CommandResultDelegate);
		/// <summary>This event is triggered whenever _any_ Command is received from the server. This is typically `Ack` or `Nak` responses but may also include things list results, pings, or disconnection notices.</summary>
		event ResponseDelegate ^ OnResponseReceived   DELEGATE_DECL(ResponseDelegate);
		#undef DELEGATE_DECL

		/// <summary> Construct a new client with the given ID. The ID should be unique among any other possible clients. </summary>
		/// See \refwcc{WASimClient::WASimClient()} for more details.
		explicit WASimClient(UInt32 clientId);
		/// <summary> Construct a new client with the given ID and with initial settings read from the file specified in `configFile` (.ini format, see default file for example). </summary>
		/// The client ID should be unique among any other possible clients. See \refwcc{WASimClient::WASimClient()} for more details.
		explicit WASimClient(UInt32 clientId, String ^configFile);
		/// This class implements a Disposable type object and should be disposed-of appropriately.
		/// Any open network connections are automatically closed upon destruction, though it is better to close them yourself before deleting the client
		~WASimClient();
		!WASimClient();  ///< \private

		// Status  -----------------------------------

		ClientStatus status() { return (ClientStatus)m_client->status(); }

		bool isInitialized() { return m_client->isInitialized(); }
		bool isConnected() { return m_client->isConnected(); }
		uint32_t clientVersion() { return m_client->clientVersion(); }
		uint32_t serverVersion() { return m_client->serverVersion(); }

		// Network actions  -----------------------------------

		/// <summary> Connect to the Simulator engine on a local connection. </summary>
		/// <paramref name='timeout'> (optional) Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value. </paramref>
		/// \return See \refwcc{connectSimulator(uint32_t)}
		HR connectSimulator([Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectSimulator(timeout.HasValue ? timeout.Value : 0); }
		/// <summary> Connect to the Simulator engine using a specific network configuration ID from a SimConnect.cfg file. The file must be in the same folder as the executable running the Client. </summary>
		/// <paramref name='networkConfigId'> network configuration ID from a SimConnect.cfg file. The file must be in the same folder as the executable running the Client. </paramref>\n
		/// <paramref name='timeout'> (optional) Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value. </paramref>
		/// \return See \refwcc{connectSimulator(int,uint32_t)}
		HR connectSimulator(int networkConfigId, [Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectSimulator(networkConfigId, timeout.HasValue ? timeout.Value : 0); }
		void disconnectSimulator() { m_client->disconnectSimulator(); }

		uint32_t pingServer([Optional] Nullable<uint32_t> timeout) { return m_client->pingServer(timeout.HasValue ? timeout.Value : 0); }
		HR connectServer([Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectServer(timeout.HasValue ? timeout.Value : 0); }
		void disconnectServer() { m_client->disconnectServer(); }

		// Settings  -----------------------------------

		uint32_t defaultTimeout() { return m_client->defaultTimeout(); }
		void setDefaultTimeout(uint32_t ms) { m_client->setDefaultTimeout(ms); }
		int networkConfigurationId() { return m_client->networkConfigurationId(); }
		void setNetworkConfigurationId(int configId) { m_client->setNetworkConfigurationId(configId); }

		// Calculator code -----------------------------------

		/// <summary> Execute calculator code without result </summary>
		HR executeCalculatorCode(String^ code) { return (HR)m_client->executeCalculatorCode(marshal_as<std::string>(code)); }
		/// <summary> Execute calculator code with a numeric result type. </summary> \private
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] double %pfResult);
		/// <summary> Execute calculator code with a string result type. </summary> \private
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] String^ %psResult);
		/// <summary> Execute calculator code with both numeric and string results. </summary> \private
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] double %pfResult, [Out] String^ %psResult);

		// Variables accessors ------------------------------

		HR getVariable(VariableRequest ^var, [Out] double %pfResult) {
			pin_ptr<double> pf = &pfResult;
			return  (HR)m_client->getVariable(var, pf);
		}
		HR getLocalVariable(String ^variableName, [Out] double %pfResult) { return getVariable(gcnew VariableRequest(variableName), pfResult); }

		HR setVariable(VariableRequest ^var, const double value) { return (HR)m_client->setVariable(var, value); }
		HR setLocalVariable(String ^variableName, const double value) { return (HR)m_client->setLocalVariable(marshal_as<std::string>(variableName), value); }
		HR setOrCreateLocalVariable(String ^variableName, const double value) { return (HR)m_client->setOrCreateLocalVariable(marshal_as<std::string>(variableName), value); }

		// Data subscriptions -------------------------------

		HR saveDataRequest(DataRequest ^request) { return (HR)m_client->saveDataRequest(request); }
		HR removeDataRequest(const uint32_t requestId) { return (HR)m_client->removeDataRequest(requestId); }
		HR updateDataRequest(uint32_t requestId) { return (HR)m_client->updateDataRequest(requestId); }

		DataRequestRecord ^dataRequest(uint32_t requestId) { return gcnew DataRequestRecord(m_client->dataRequest(requestId)); }
		array<DataRequestRecord ^> ^dataRequests();
		array<UInt32> ^dataRequestIdsList();

		HR setDataRequestsPaused(bool paused) { return (HR)m_client->setDataRequestsPaused(paused); }

		// Custom Event registration --------------------------

		HR registerEvent(RegisteredEvent ^eventData) { return (HR)m_client->registerEvent(eventData); }
		HR removeEvent(uint32_t eventId) { return (HR)m_client->removeEvent(eventId); }
		HR transmitEvent(uint32_t eventId) { return (HR)m_client->transmitEvent(eventId); }

		RegisteredEvent ^registeredEvent(uint32_t eventId) { return gcnew RegisteredEvent(m_client->registeredEvent(eventId)); }
		array<RegisteredEvent ^> ^registeredEvents();

		// Meta data retrieval --------------------------------

		HR list(LookupItemType itemsType) { return (HR)m_client->list((WSE::LookupItemType)itemsType); }
		HR lookup(LookupItemType itemType, String ^itemName, [Out] Int32 %piResult)
		{
			pin_ptr<Int32> pi = &piResult;
			return (HR)m_client->lookup((WSE::LookupItemType)itemType, marshal_as<std::string>(itemName), pi);
		}

		// Low level API    --------------------------------

		HR sendCommand(Command ^command) { return (HR)m_client->sendCommand(command); }
		HR sendCommandWithResponse(Command ^command, [Out] Command^ %response, [Optional] Nullable<uint32_t> timeout)
		{
			WASimCommander::Command resp {};
			const HRESULT hr = m_client->sendCommandWithResponse(command, &resp, (timeout.HasValue ? timeout.Value : 0));
			response = gcnew Command(resp);
			return (HR)hr;
		}

		// Logging settings --------------------------------

		LogLevel logLevel(LogFacility facility, LogSource source) {
			return (LogLevel)m_client->logLevel((WSE::LogFacility)facility, (WASimCommander::Client::LogSource)source);
		}
		void setLogLevel(LogLevel level, LogFacility facility, LogSource source) {
			m_client->setLogLevel((WSE::LogLevel)level, (WSE::LogFacility)facility, (WASimCommander::Client::LogSource)source);
		}


	private:
		WASimCommander::Client::WASimClient *m_client = nullptr;
		ref class Private;
		Private ^d = nullptr;
	};

	#undef WSE

}
