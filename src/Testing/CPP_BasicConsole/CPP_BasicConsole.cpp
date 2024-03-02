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
	Log("@@")() << getEnumName(src, LogSourceNames) << ": " << lr;  // LogRecord has a convenient stream operator
}

// Handler to print the current Client status (connecting, disconnected, etc).
static void ClientStatusHandler(const ClientEvent &ev)
{
	Log("^^")() << "Client event "  << getEnumName(ev.eventType, ClientEventTypeNames) << " - " << quoted(ev.message)
		<< "; Client status: " << hex << setw(2) << setfill('0') << (uint16_t)ev.status;
}

// Event handler for showing listing results (eg. local vars list)
static void ListResultsHandler(const ListResult &lr)
{
	Log()() << "Got " << lr.list.size() << " results for list type " << getEnumName(lr.listType, LookupItemTypeNames) << ". (Uncomment next line in ListResultsHandler() to print them.)";
	// uncomment below to print list results (may be long)
	// for (const auto & [k, v] : lr.list) Log("\t")() << k << " = " << v;
	// Log()() << "End of list results.";

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

// Helper function to wait for data to arrive or time out with a log message.
static bool AwaitData(DWORD ms = 1000)
{
	if (WaitForSingleObject(g_dataUpdateEvent, ms) == WAIT_OBJECT_0)
		return true;
	Log("!!")() << "Data subscription update timed out!";
	return false;
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
	string calcCode = (ostringstream() << "(A:" << simVarName << ',' << simVarUnit << ')').str();  // "(A:CG PERCENT,percent)"
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

	// We subscribe to it using a "data request" and set it up to return as a float value, using a predefined value type (DATA_TYPE_FLOAT).
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
		if (!AwaitData())
			break;
	}

	// Test subscribing to a string type value. We'll use the Sim var "TITLE" (airplane name), which can only be retrieved using calculator code.
	// We allocate 64 Bytes here to hold the result and we request this one with an update period of Once, which will return a result right away
	// but will not be scheduled for regular updates. If we wanted to update this value later, we could call the client's `updateDataRequest(requestId)` method.
	Log(">>")() << "Requesting TITLE variable...";
	hr = client.saveDataRequest(DataRequest(
		/*requestId*/      REQUEST_ID_2_STR,
		/*resultType*/     CalcResultType::String,
		/*calculatorCode*/ "(A:TITLE, String)",
		/*valueSize*/      64,
		/*period*/         UpdatePeriod::Once)
	);
	if (hr == S_OK)
		AwaitData();
	else
		Log("!!")() << "saveDataRequest() for TITLE var returned error result " << hr;

	// Test getting a list of our data requests back from the Client.
	Log("::")() << "Saved Data Requests:";
	const auto &requests = client.dataRequests();
	for (const DataRequestRecord &dr : requests)
		Log()() << dr;  // Another convenient stream operator for logging

	// Test removing a data subscriptions.
	if ((hr = client.removeDataRequest(REQUEST_ID_2_STR)) != S_OK)
		Log("!!")() << "removeDataRequest() for TITLE var returned error result " << hr;

	// Get a list of all local variables...
	// Connect to the list results Event
	client.setListResultsCallback(ListResultsHandler);
	// Request the list.
	client.list(LookupItemType::LocalVariable);
	// Results will arrive asynchronously, so we wait a bit.
	AwaitData(3000);

	// Look up a Key Event ID by name;
	const string eventName = "ATC_MENU_OPEN";  // w/out the "KEY_" prefix.
	int keyId = -1;
	if (client.lookup(LookupItemType::KeyEventId, eventName, &keyId) == S_OK)
		Log("<<")() << "Got lookup ID: 0x " << hex << setw(8) << setfill('0') << keyId << " for " << quoted(eventName);

	// Trigger the event we just looked up (it won't actually work since all the "ATC_MENU" events are broken in MSFS).
	client.sendKeyEvent(keyId);

	// Try sending a Command directly to the server and wait for a response (Ack/Nak).
	// In this case we'll use a "SendKey" command to activate a Key Event by the ID which we just looked up
	// (which does the same thing we just did above with `sendKeyEvent()`).
	Command response;
	if (client.sendCommandWithResponse(Command(CommandId::SendKey, keyId), &response) == S_OK)
		Log("<<")() << "Got response for SendKey command: " << response;   // another custom stream op

	// Custom calculator event tests.

	// Let's say we want a simple way to toggle a variable value using some RPN code (or repeatedly execute any kind of calculator code).
	// We could use `executeCalculatorCode()` each time, but this isn't very efficient because the code string has to be built, sent,
	// parsed, compiled on the sim side, and then finally executed. Using pre-defined "calculator events" we can skip all the preliminary
	// steps and simply trigger the execution of pre-compiled RPN code via a numeric event ID.
	//
	// For this test we're just going to toggle the test Local variable we created earlier between the values of 0 and 1 based on its current value,
	// using the following basic RPN code:
	// (L:WASIM_TEST_VAR_1) 1 == if{ 0 (>L:WASIM_TEST_VAR_1) } els{ 1 (>L:WASIM_TEST_VAR_1) }
	calcCode = (ostringstream() <<
		"(L:" << variableName << ") 1 == if{ 0 (>L:" << variableName << ") } els{ 1 (>L:" << variableName << ") }"
	).str();

	// We need to register that calculator code with the WASim client/server, and we'll use a unique ID to refer to this event later on.
	// The ID is arbitrary (just like request IDs) -- it could be an const enum value, dynamically generated, or whatever else ensures a unique number per event.
	const uint32_t customEventId = 1;
	// We can, optionally, also provide our own name for this custom event when we register it.
	// This name could be used to trigger this custom event from _any_ standard SimConnect client (by mapping to the name using `SimConnect_MapClientEventToSimEvent()`
	// and then triggering with `SimConnect_TransmitClientEvent[_EX1]()`).
	// We don't really need a custom name for this example, but we can actually make use of this later to test another feature.
	hr = client.registerEvent(RegisteredEvent(customEventId, calcCode, "MyEvents.ToggleTestVariable"));
	if (hr == S_OK) {
		// Assuming that worked, we can now trigger the custom event in various ways. Since we're still "watching" the test local variable for changes,
		// we should see a new value every time we trigger this event.
		if ((hr = client.transmitEvent(customEventId)) == S_OK)
			AwaitData();
		else
			Log("!!")() << "transmitEvent() returned error result " << hr;
	}
	else {
		Log("!!")() << "Registration of custom calculator event ID " << customEventId << " with RPN code " << quoted(calcCode) << " failed with error: " << hr;
	}

	// Custom named event registration and triggering test (new for v1.3.0).
	// This tests setting up a custom-named simulator event, which are typically created by 3rd-party aircraft models, such as the FlyByWire A32NX.
	// Here we're (redundantly) using our own custom-named "calculator event" we set up above, just because we know it exists (vs. relying on some specific model to be installed).

	// First the custom-named event needs to be registered with the client. Note we're using the same event name here as we used in the `registerEvent()` call above.
	// This sets up a SimConnect mapping to a generated unique ID. We want to save this ID and use it later to trigger the event efficiently (w/out needing to use the event name again).
	uint32_t customNamedEventId = 0;
	hr = client.registerCustomKeyEvent("MyEvents.ToggleTestVariable", &customNamedEventId);
	if (hr == S_OK) {
		// A successful registration means we can now use the generated event ID to trigger the custom event,
		// using the same `sendKeyEvent()` method as we'd use for a "standard" Key Event.
		Log()() << "registerCustomKeyEvent() succeeded, returned Event ID " << customNamedEventId;
		// Trigger the event. Should get a variable value change message after this, or a timeout if something went wrong.
		if ((hr = client.sendKeyEvent(customNamedEventId)) == S_OK)
			AwaitData();
		else
			Log("!!")() << "sendKeyEvent() returned error result " << hr;
	}
	else {
		Log("!!")() << "registerCustomKeyEvent() returned error result " << hr;
	}

	// Finished, disconnect.

	// Shutdown (really the destructor will close any/all connections anyway, but this is for example).
	client.disconnectServer();
	client.disconnectSimulator();

	CloseHandle(g_dataUpdateEvent);
	return 0;
}
