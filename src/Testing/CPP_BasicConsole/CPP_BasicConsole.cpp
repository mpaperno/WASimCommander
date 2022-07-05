/*
This file is part of the WASimCommander project.
https://github.com/mpaperno/WASimCommander

COPYRIGHT: (c) Maxim Paperno; All Rights Reserved.

This file may be used under the terms of the GNU General Public License (GPL)
as published by the Free Software Foundation, either version 3 of the Licenses,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU GPL is included with this project
and is also available at <http://www.gnu.org/licenses/>.
*/


#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "client/WASimClient.h"

using namespace std;
using namespace WASimCommander;
using namespace WASimCommander::Enums;
using namespace WASimCommander::Client;

// -----------------------------
// Constants
// -----------------------------

// We use unique IDs to identify data subscription requests. I put the data type in the name just to keep them straight.
enum Requests : char {
	REQUEST_ID_1_FLOAT,
	REQUEST_ID_2_STR
};

// A wait handle is used in this test/demo to keep the program alive while data processes in the background.
static const HANDLE g_dataUpdateEvent = CreateEvent(nullptr, false, false, nullptr);


// -----------------------------
// Logging output helpers
// -----------------------------

// "Log" helper, just prints to stdout.
class Log
{
	public:
		Log(const std::string &prfx = "=:") : prfx_(prfx) { }
		~Log() { cout << prfx_ << ' ' << out_.str() << endl; }
		inline std::ostringstream& operator()() { return out_; }
	private:
		std::ostringstream out_;
		const std::string prfx_;
};

// Safely get enum name from an array of names based on the enum value index (not for flags!)
template <typename Enum, typename Range /*, std::enable_if_t<std::is_enum<Enum>::value, bool> = true*/>
static const auto getEnumName(Enum e, const Range &range) noexcept
{
	using T = typename std::iterator_traits<decltype(std::begin(range))>::value_type;
	const size_t len = std::cend(range) - std::cbegin(range);
	if ((size_t)e < len)
		return range[(size_t)e];
	return T();
}

// -----------------------------
// Client callback handlers
// -----------------------------

// This is an event handler for printing Client and Server log messages.
static void LogHandler(const LogRecord &lr, LogSource src)
{
	Log("@@")() << getEnumName(src, LogSourceNames) << ": " << lr;  // LogRecord has a convenient  stream operator
}

// Handler to print the current Client status (connecting, disconnected, etc).
static void ClientStatusHandler(const ClientEvent &ev)
{
	Log("^^")() << "Client event "  << getEnumName(ev.eventType, ClientEventTypeNames) << " - " << quoted(ev.message) << "; Client status: " << hex << setw(2) << setfill('0') << (uint16_t)ev.status;
}

// Event handler for showing listing results (eg. local vars list)
static void ListResultsHandler(const ListResult &lr)
{
	Log()() << "ListResult{ " << getEnumName(lr.listType, LookupItemTypeNames) << "; status: " << hex << setw(8) << setfill('0') << lr.result << " Values: ";
	for (const auto & [k, v] : lr.list)
		Log("\t")() << k << " = " << v;
	// signal completion
	SetEvent(g_dataUpdateEvent);
}

// Event handler to process data value subscription updates.
static void DataSubscriptionHandler(const DataRequestRecord &dr)
{
	cout << "<< Got Data for request " << dr.requestId << " " << quoted(dr.nameOrCode) << " with Value: ";
	// Convert the received data into a usable value.
	// This could be more efficient in a "real" application, but it's good enough for our tests with only 2 value types.
	float fVal;
	// If this is the request for our string value, then the data is the string.
	if (dr.requestId == REQUEST_ID_2_STR)
		cout << "(string) " << quoted(string((const char *)dr.data.data())) << endl;
	// Otherwise get a float value using DataRequestRecord's tryConvert() method.
	else if (dr.tryConvert(fVal))
		cout << "(float) " << fVal << endl;
	else
		cout << "Could not convert result data to value!" << endl;
	// signal completion
	SetEvent(g_dataUpdateEvent);
}

// -----------------------------
// Main
// -----------------------------

int main()
{
	Log()() << "Initializing WASimClient...";

	// Create
	WASimClient client = WASimClient(0x0C997E57);  // "CPPTEST"  :)

	// Monitor client state changes.
	client.setClientEventCallback(ClientStatusHandler);
	// Subscribe to incoming log record events.
	client.setLogCallback(LogHandler);

	// As a test, set Client's callback logging level to display messages in the console.
	client.setLogLevel(LogLevel::Info, LogFacility::Remote, LogSource::Client);
	// Set client's console log level to None to avoid double logging to our console. (Client also logs to a file by default.)
	client.setLogLevel(LogLevel::None, LogFacility::Console, LogSource::Client);
	// Lets also see some log messages from the server
	client.setLogLevel(LogLevel::Info, LogFacility::Remote, LogSource::Server);

	HRESULT hr;  // store method invocation results for logging

					// Connect to Simulator (SimConnect) using default timeout period and network configuration (local Simulator)
	if ((hr = client.connectSimulator()) != S_OK) {
		Log("XX")() << "Cannot connect to Simulator, quitting. Error Code: " << hr;
		return -1;
	}

	// Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
	const uint32_t version = client.pingServer();
	if (version == 0) {
		Log("XX")() << "Server did not respond to ping, quitting.";
		return -1;
	}
	// Decode version number to dotted format and print it
	Log()() << "Found WASimModule Server v" << (version >> 24) << '.' << ((version >> 16) & 0xFF) << '.' << ((version >> 8) & 0xFF) << '.' << (version & 0xFF);

	// Try to connect to the server, using default timeout value.
	if ((hr = client.connectServer()) != S_OK) {
		Log("XX")() << "Server connection failed, quitting. Error Code: " << hr;
		return -1;
	}

	// set up a Simulator Variable for testing.
	const string simVarName = "CG PERCENT";
	const string simVarUnit = "percent";

	// Execute a calculator string with result. We'll try to read the value of the SimVar defined above.
	const string calcCode = (ostringstream() << "(A:" << simVarName << ',' << simVarUnit).str();
	double fResult = 0.;
	string sResult {};
	if (client.executeCalculatorCode(calcCode, CalcResultType::Double, &fResult, &sResult) == S_OK)
		Log("<<")() << "Calculator code " << quoted(calcCode) << " returned: " << fResult << " and " << quoted(sResult);

	// Get a named Sim Variable value, same one as before, but directly using the Gauge API function aircraft_varget()
	if (client.getVariable(VariableRequest(simVarName, simVarUnit, 0), &fResult) == S_OK)
		Log("<<")() << "Get SimVar '" << simVarName << ',' << simVarUnit << "' returned: " << fResult;

	// Create and/or Set a Local variable to play with (will be created if it doesn't exist yet, will exist if this program has run during the current simulator session).
	const string variableName = "WASIM_TEST_VAR_1";
	if (client.setOrCreateLocalVariable(variableName, 1.0) == S_OK)
		Log()() << "Created/Set local variable " << quoted(variableName);

	// We can check that our new variable exists by looking up its ID.
	int localVarId = 0;
	if (client.lookup(LookupItemType::LocalVariable, variableName, &localVarId) == S_OK)
		Log("<<")() << "Got ID: " << localVarId << " for local variable " << quoted(variableName);
	else
		Log("!!")() << "Local variable " << quoted(variableName) << " was not found, something went wrong!";

	// Assuming our variable was created/exists, lets "subscribe" to notifications about when its value changes.
	// First set up the event handler to get the updates.
	client.setDataCallback(DataSubscriptionHandler);
	// We subscribe to it using a "data request" and set it up to return as a float value, using a predefined value type (we could also use `4` here, for the number of bytes in a float).
	// This should also immediately return the current value, which will be delivered to the DataSubscriptionHandler we assigned earlier.
	if (client.saveDataRequest(DataRequest(REQUEST_ID_1_FLOAT, 'L', variableName.c_str(), DATA_TYPE_FLOAT)) == S_OK)
		Log()() << "Subscribed to value changes for local variable " << quoted(variableName);

	// Now let's change the value of our local variable and watch the updates come in via the subscription.
	for (float i = 1.33f; i < 10.0f; i += 0.89f) {
		if (client.setLocalVariable(variableName, i) == S_OK)
			Log(">>")() << "Set local variable " << quoted(variableName) << " to " << i;
		else
			break;
		// wait for updates to process asynchronously and our event handler to get called, or time out
		if (WaitForSingleObject(g_dataUpdateEvent, 1000) != WAIT_OBJECT_0) {
			Log("!!")() << "Data subscription update timed out!";
			break;
		}
	}

	// Test subscribing to a string type value. We'll use the Sim var "TITLE" (airplane name), which can only be retrieved using calculator code.
	// We allocate 32 Bytes here to hold the result and we request this one with an update period of Once, which will return a result right away
	// but will not be scheduled for regular updates. If we wanted to update this value later, we could call the client's `updateDataRequest(requestId)` method.
	hr = client.saveDataRequest(DataRequest(
		/*requestId*/      REQUEST_ID_2_STR,
		/*resultType*/     CalcResultType::String,
		/*calculatorCode*/ "(A:TITLE, String)",
		/*valueSize*/      32,
		/*period*/         UpdatePeriod::Once)
	);
	if (hr == S_OK) {
		Log(">>")() << "Requested TITLE variable.";
		if (WaitForSingleObject(g_dataUpdateEvent, 1000) != WAIT_OBJECT_0)
			Log("!!")() << "Data subscription update timed out!";
	}

	// Test getting a list of our data requests back from the Client.
	Log("::")() << "Saved Data Requests:";
	const auto &requests = client.dataRequests();
	for (const DataRequestRecord &dr : requests)
		Log()() << dr;  // Another convenient stream operator for logging

	// OK, that was fun... now remove the data subscriptions. We could have done it in the loop above but we're testing things here...
	// The server will also remove all our subscriptions when we disconnect, but it's nice to be polite!
	const vector<uint32_t> &requestIds = client.dataRequestIdsList();
	for (const uint32_t &id :requestIds)
		client.removeDataRequest(id);

	// Get a list of all local variables...
	// Connect to the list results Event
	client.setListResultsCallback(ListResultsHandler);
	// Request the list.
	client.list(LookupItemType::LocalVariable);
	// Results will arrive asynchronously, so we wait a bit.
	if (WaitForSingleObject(g_dataUpdateEvent, 3000) != WAIT_OBJECT_0)
		Log("!!")() << "List results timed out!";

	// Look up a Key Event ID by name;
	const string eventName = "ATC_MENU_OPEN";  // w/out the "KEY_" prefix.
	int keyId = -1;
	if (client.lookup(LookupItemType::KeyEventId, eventName, &keyId) == S_OK)
		Log("<<")() << "Got lookup ID: 0x " << hex << setw(8) << setfill('0') << keyId << " for " << quoted(eventName);

	// Try sending a Command directly to the server and wait for a response (Ack/Nak).
	// In this case we'll use a "SendKey" command to activate a Key Event by the ID which we just looked up
	Command response;
	if (client.sendCommandWithResponse(Command(CommandId::SendKey, keyId), &response) == S_OK)
		Log("<<")() << "Got response for SendKey command: " << response;   // another custom stream op

	// Shutdown (really the destructor will close any/all connections anyway, but this is for example).
	client.disconnectServer();
	client.disconnectSimulator();

	return 0;
}

