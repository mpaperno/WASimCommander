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
	///
	/// \note Events are delivered asyncronously from a separtely running thread. The event handlers should be reentrant since they could be callled at any time. \n
	/// Typically, interactions with GUI components will not be possible directly from inside the event handlers -- use a `Dispatcher` to marshal GUI interactions
	/// back to the main thread.
	public ref class WASimClient
	{
	public:

		// Delegates  -----------------------------------

		/// \name Event handler delegate types
		/// \{
		delegate void ClientEventDelegate(ClientEvent ^);    ///< Event delegate for Client events (`OnClientEvent`)
		delegate void ListResultsDelegate(ListResult ^);     ///< Event delegate for delivering list results, eg. of local variables sent from Server (`OnListResults`).
		delegate void DataDelegate(DataRequestRecord ^);     ///< Event delegate for subscription result data (`OnDataReceived`).
		delegate void LogDelegate(LogRecord ^, LogSource);   ///< Event delegate for log entries (from both Client and Server) (`OnLogRecordReceived`).
		delegate void CommandResultDelegate(Command ^);      ///< Event delegate for command responses returned from server (`OnCommandResult`).
		delegate void ResponseDelegate(Command ^);           ///< Event delegate for all Command structures received from server (`OnResponseReceived`).
		/// \}

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

		/// <summary> Construct a new client with the given ID. The ID must be unique among any other possible clients and cannot be zero. </summary>
		/// See \refwccc{WASimClient()} for more details.
		explicit WASimClient(UInt32 clientId);
		/// <summary> Construct a new client with the given ID and with initial settings read from the file specified in `configFile` (.ini format, see default file for example). </summary>
		/// The client ID must be unique among any other possible clients and cannot be zero. See \refwccc{WASimClient()} for more details.
		explicit WASimClient(UInt32 clientId, String ^configFile);
#if DOXYGEN
		/// <summary> This class implements a Disposable type object and should be disposed-of appropriately by calling `client.Dispose()` when the instance is no longer needed.
		/// Any open network connections are automatically closed upon destruction, though it is better to close them yourself before deleting the client. </summary>
		void Dispose();
#else
		~WASimClient();
		!WASimClient();
#endif

		// Status  -----------------------------------

		/// \name Network actions, status, and settings
		/// \{

		/// <summary> Get current connection status of this client. \sa WASimCommander::Client::ClientStatus </summary>
		ClientStatus status() { return (ClientStatus)m_client->status(); }
		/// <summary> Returns true if connected to the Simulator (SimConnect). </summary>
		bool isInitialized() { return m_client->isInitialized(); }
		/// <summary> Returns true if connected to WASimModule server. </summary>
		bool isConnected() { return m_client->isConnected(); }
		/// <summary> Returns version number of the WASimClient. </summary>
		uint32_t clientVersion() { return m_client->clientVersion(); }
		/// <summary> Returns version number of the WASimModule server, if known. The version is populated after a successful Ping command or server connection. </summary>
		uint32_t serverVersion() { return m_client->serverVersion(); }

		// Network actions  -----------------------------------

		/// <summary> Connect to the Simulator engine on a local connection. </summary>
		/// <paramref name='timeout'> (optional) Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value. </paramref>
		/// \return See \refwccc{connectSimulator(uint32_t)}
		HR connectSimulator([Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectSimulator(timeout.HasValue ? timeout.Value : 0); }
		/// <summary> Connect to the Simulator engine using a specific network configuration ID from a SimConnect.cfg file. The file must be in the same folder as the executable running the Client. </summary>
		/// <paramref name='networkConfigId'> network configuration ID from a SimConnect.cfg file. The file must be in the same folder as the executable running the Client. </paramref>\n
		/// <paramref name='timeout'> (optional) Maximum time to wait for response, in milliseconds. Zero (default) means to use the `defaultTimeout()` value. </paramref>
		/// \return See \refwccc{connectSimulator(int,uint32_t)}
		HR connectSimulator(int networkConfigId, [Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectSimulator(networkConfigId, timeout.HasValue ? timeout.Value : 0); }
		/// See \refwccc{disconnectSimulator()}
		void disconnectSimulator() { m_client->disconnectSimulator(); }

		/// See \refwccc{pingServer()}
		uint32_t pingServer([Optional] Nullable<uint32_t> timeout) { return m_client->pingServer(timeout.HasValue ? timeout.Value : 0); }
		/// See \refwccc{connectServer()}
		HR connectServer([Optional] Nullable<uint32_t> timeout) { return (HR)m_client->connectServer(timeout.HasValue ? timeout.Value : 0); }
		void disconnectServer() { m_client->disconnectServer(); }  ///< See \refwccc{disconnectServer()}

		// Settings  -----------------------------------

		uint32_t defaultTimeout() { return m_client->defaultTimeout(); }  ///< See \refwccc{defaultTimeout()}
		void setDefaultTimeout(uint32_t ms) { m_client->setDefaultTimeout(ms); }  ///< See \refwccc{setDefaultTimeout()}
		int networkConfigurationId() { return m_client->networkConfigurationId(); }  ///< See \refwccc{networkConfigurationId()}
		void setNetworkConfigurationId(int configId) { m_client->setNetworkConfigurationId(configId); }  ///< See \refwccc{setNetworkConfigurationId()}

		/// \}
		/// \name High level API
		/// \{

		// Calculator code -----------------------------------

		/// <summary> Execute calculator code without result </summary> \sa \refwccc{executeCalculatorCode()}
		HR executeCalculatorCode(String^ code) { return (HR)m_client->executeCalculatorCode(marshal_as<std::string>(code)); }
		/// <summary> Execute calculator code with a numeric result type. </summary>  \sa \refwccc{executeCalculatorCode()}
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] double %pfResult);
		/// <summary> Execute calculator code with a string result type. </summary> \sa \refwccc{executeCalculatorCode()}
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] String^ %psResult);
		/// <summary> Execute calculator code with both numeric and string results. </summary>  \sa \refwccc{executeCalculatorCode()}
		HR executeCalculatorCode(String^ code, CalcResultType resultType, [Out] double %pfResult, [Out] String^ %psResult);

		// Variables accessors ------------------------------

		/// <summary> Get the value of a variable with a numeric result type. This is the most typical use case since most variable types are numeric. </summary> \sa \refwccc{getVariable()}
		HR getVariable(VariableRequest ^var, [Out] double %pfResult);
		/// <summary> Get the value of a variable with a string result type. The request is executed as calculator code since that is the only way to get string results. </summary>
		/// Note that only some 'A', 'C', and 'T' type variables can have a string value type in the first place. \sa \refwccc{getVariable()}
		HR getVariable(VariableRequest ^var, [Out] String^ %psResult);
		/// <summary> Get the value of a variable with _either_ a numeric or string result based on the unit type of the requested variable. </summary>
		/// The request is executed as calculator code since that is the only way to get string results. Unlike `executeCalculatorCode()`, this method will not return a string representation of a numeric value.
		/// Note that only some 'A', 'C', and 'T' type variables can have a string value type in the first place. \sa \refwccc{getVariable()}
		HR getVariable(VariableRequest ^var, [Out] double %pfResult, [Out] String^ %psResult);  ///< See \refwccc{getVariable()}

		/// See \refwccc{getLocalVariable()}
		HR getLocalVariable(String ^variableName, [Out] double %pfResult) { return getVariable(gcnew VariableRequest(variableName), pfResult); }
		/// See \refwccc{getLocalVariable()}
		HR getLocalVariable(String ^variableName, String ^unitName, [Out] double %pfResult) { return getVariable(gcnew VariableRequest(variableName, false, unitName), pfResult); }
		/// \sa \refwccc{getOrCreateLocalVariable()}
		HR getOrCreateLocalVariable(String ^variableName, double defaultValue, [Out] double %pfResult);
		/// \sa \refwccc{getOrCreateLocalVariable()}
		HR getOrCreateLocalVariable(String ^variableName, String ^unitName, double defaultValue, [Out] double %pfResult);

		/// See \refwccc{setVariable()}
		HR setVariable(VariableRequest ^var, const double value) { return (HR)m_client->setVariable(var, value); }
		/// See \refwccc{setLocalVariable()}
		HR setLocalVariable(String ^variableName, const double value) { return (HR)m_client->setLocalVariable(marshal_as<std::string>(variableName), value); }
		HR setLocalVariable(String ^variableName, String ^unitName, const double value) {
			return (HR)m_client->setLocalVariable(marshal_as<std::string>(variableName), value, marshal_as<std::string>(unitName));
		}
		/// See \refwccc{setOrCreateLocalVariable()}
		HR setOrCreateLocalVariable(String ^variableName, const double value) { return (HR)m_client->setOrCreateLocalVariable(marshal_as<std::string>(variableName), value); }
		/// See \refwccc{setOrCreateLocalVariable()}
		HR setOrCreateLocalVariable(String ^variableName, String ^unitName, const double value) {
			return (HR)m_client->setOrCreateLocalVariable(marshal_as<std::string>(variableName), value, marshal_as<std::string>(unitName));
		}

		// Data subscriptions -------------------------------

		HR saveDataRequest(DataRequest ^request) { return (HR)m_client->saveDataRequest(request); }  ///< See \refwccc{saveDataRequest()} as used with `async = false`
		HR saveDataRequestAsync(DataRequest ^request) { return (HR)m_client->saveDataRequest(request, true); }  ///< See \refwccc{saveDataRequest()} as used with `async = true`
		HR removeDataRequest(const uint32_t requestId) { return (HR)m_client->removeDataRequest(requestId); }  ///< See \refwccc{removeDataRequest()}
		HR updateDataRequest(uint32_t requestId) { return (HR)m_client->updateDataRequest(requestId); }  ///< See \refwccc{updateDataRequest()}

		DataRequestRecord ^dataRequest(uint32_t requestId) { return gcnew DataRequestRecord(m_client->dataRequest(requestId)); }  ///< See \refwccc{dataRequest()}
		array<DataRequestRecord ^> ^dataRequests();  ///< See \refwccc{dataRequests()}
		array<UInt32> ^dataRequestIdsList();  ///< See \refwccc{dataRequestIdsList()}

		HR setDataRequestsPaused(bool paused) { return (HR)m_client->setDataRequestsPaused(paused); }  ///< See \refwccc{setDataRequestsPaused()}

		// Calculator Custom Event registration --------------------------

		HR registerEvent(RegisteredEvent ^eventData) { return (HR)m_client->registerEvent(eventData); }  ///< See \refwccc{registerEvent()}
		HR removeEvent(uint32_t eventId) { return (HR)m_client->removeEvent(eventId); }  ///< See \refwccc{removeEvent()}
		HR transmitEvent(uint32_t eventId) { return (HR)m_client->transmitEvent(eventId); }  ///< See \refwccc{transmitEvent()}

		RegisteredEvent ^registeredEvent(uint32_t eventId) { return gcnew RegisteredEvent(m_client->registeredEvent(eventId)); }  ///< See \refwccc{registeredEvent()}
		array<RegisteredEvent ^> ^registeredEvents();  ///< See \refwccc{registeredEvents()}

		// SimConnect Custom Events registration ------------------------------

		/// See \refwccc{registerCustomEvent(const std::string&, uint32_t)}
		HR registerCustomEvent(String ^customEventName, [Out] uint32_t% puiCustomEventId) {
			pin_ptr<UInt32> pui = &puiCustomEventId;
			return (HR)m_client->registerCustomEvent(marshal_as<std::string>(customEventName), pui);
		}

		/// See \refwccc{registerCustomEvent(const std::string&)}
		HR registerCustomEvent(String^ customEventName) {
			return (HR)m_client->registerCustomEvent(marshal_as<std::string>(customEventName));
		}

		// Simulator Key Events ------------------------------

		/// See \refwccc{sendKeyEvent(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) const}
		HR sendKeyEvent(uint32_t keyEventId, [Optional] Nullable<uint32_t> v1, [Optional] Nullable<uint32_t> v2, [Optional] Nullable<uint32_t> v3, [Optional] Nullable<uint32_t> v4, [Optional] Nullable<uint32_t> v5) {
			return (HR)m_client->sendKeyEvent(keyEventId, v1.GetValueOrDefault(0), v2.GetValueOrDefault(0), v3.GetValueOrDefault(0), v4.GetValueOrDefault(0), v5.GetValueOrDefault(0));
		}

		/// See \refwccc{sendKeyEvent(const std::string&, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)}
		HR sendKeyEvent(String ^keyEventName, [Optional] Nullable<uint32_t> v1, [Optional] Nullable<uint32_t> v2, [Optional] Nullable<uint32_t> v3, [Optional] Nullable<uint32_t> v4, [Optional] Nullable<uint32_t> v5) {
			return (HR)m_client->sendKeyEvent(marshal_as<std::string>(keyEventName), v1.GetValueOrDefault(0), v2.GetValueOrDefault(0), v3.GetValueOrDefault(0), v4.GetValueOrDefault(0), v5.GetValueOrDefault(0));
		}

		// Meta data retrieval --------------------------------

		/// See \refwccc{list()}
		HR list(LookupItemType itemsType) { return (HR)m_client->list((WSE::LookupItemType)itemsType); }
		/// See \refwccc{lookup()}
		HR lookup(LookupItemType itemType, String ^itemName, [Out] Int32 %piResult)
		{
			pin_ptr<Int32> pi = &piResult;
			return (HR)m_client->lookup((WSE::LookupItemType)itemType, marshal_as<std::string>(itemName), pi);
		}

		/// \}
		/// \name Low level API
		/// \{

		/// See \refwccc{sendCommand()}
		HR sendCommand(Command ^command) { return (HR)m_client->sendCommand(command); }
		/// See \refwccc{sendCommandWithResponse()}
		HR sendCommandWithResponse(Command ^command, [Out] Command^ %response, [Optional] Nullable<uint32_t> timeout)
		{
			WASimCommander::Command resp {};
			const HRESULT hr = m_client->sendCommandWithResponse(command, &resp, (timeout.HasValue ? timeout.Value : 0));
			response = gcnew Command(resp);
			return (HR)hr;
		}

		/// \}
		/// \name  Logging settings
		/// \{

		/// See \refwccc{logLevel()}
		LogLevel logLevel(LogFacility facility, LogSource source) {
			return (LogLevel)m_client->logLevel((WSE::LogFacility)facility, (WASimCommander::Client::LogSource)source);
		}
		/// See \refwccc{setLogLevel()}
		void setLogLevel(LogLevel level, LogFacility facility, LogSource source) {
			m_client->setLogLevel((WSE::LogLevel)level, (WSE::LogFacility)facility, (WASimCommander::Client::LogSource)source);
		}

		/// \}


	private:
		WASimCommander::Client::WASimClient *m_client = nullptr;
		ref class Private;
		Private ^d = nullptr;
	};

	#undef WSE

}
