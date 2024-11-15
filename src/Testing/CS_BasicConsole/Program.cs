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

using System;
using System.Threading;
using WASimCommander.CLI;
using WASimCommander.CLI.Enums;
using WASimCommander.CLI.Structs;
using WASimCommander.CLI.Client;

namespace CS_BasicConsole
{
	internal class Program
	{
		// We use unique IDs to identify data subscription requests. I put the data type in the name just to keep them straight.
		private enum Requests : uint {
			REQUEST_ID_1_FLOAT,
			REQUEST_ID_2_STR
		}
		// A wait handle is used in this test/demo to keep the program alive while data processes in the background.
		private static readonly AutoResetEvent dataUpdateEvent = new AutoResetEvent(false);

		static void Main(string[] _)
		{
			Log("Initializing WASimClient...");

			// Create
			WASimClient client = new WASimClient(0xC57E57E9);  // "CSTESTER"  :)

			// Monitor client state changes.
			client.OnClientEvent += ClientStatusHandler;
			// Subscribe to incoming log record events.
			client.OnLogRecordReceived += LogHandler;

			// As a test, set Client's callback logging level to display messages in the console.
			client.setLogLevel(LogLevel.Info, LogFacility.Remote, LogSource.Client);
			// Set client's console log level to None to avoid double logging to our console. (Client also logs to a file by default.)
			client.setLogLevel(LogLevel.None, LogFacility.Console, LogSource.Client);
			// Lets also see some log messages from the server
			client.setLogLevel(LogLevel.Info, LogFacility.Remote, LogSource.Server);

			HR hr;  // store method invocation results for logging

			// Connect to Simulator (SimConnect) using default timeout period and network configuration (local Simulator)
			if ((hr = client.connectSimulator()) != HR.OK) {
				Log("Cannot connect to Simulator, quitting. Error: " + hr.ToString(), "XX");
				client.Dispose();
				return;
			}

			// Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
			UInt32 version = client.pingServer();
			if (version == 0) {
				Log("Server did not respond to ping, quitting.", "XX");
				client.Dispose();
				return;
			}
			// Decode version number to dotted format and print it
			Log($"Found WASimModule Server v{version >> 24}.{(version >> 16) & 0xFF}.{(version >> 8) & 0xFF}.{version & 0xFF}");

			// Try to connect to the server, using default timeout value.
			if ((hr = client.connectServer()) != HR.OK) {
				Log("Server connection failed, quitting. Error: " + hr.ToString(), "XX");
				client.Dispose();
				return;
			}

			// set up a Simulator Variable for testing.
			string simVarName = "COCKPIT CAMERA ZOOM";
			string simVarUnit = "percent";
			// containers for result values
			double fResult = 0;

			// Get a named Sim Variable value directly using the Gauge API function aircraft_varget()
			if (client.getVariable(new VariableRequest(simVarName, simVarUnit, 0), out fResult) == HR.OK)
				Log($"Get SimVar '{simVarName}, {simVarUnit}' returned: {fResult}", "<<");

			// Set the Sim Var to a new value (increase the percentage if it is <= 50%, decrease otherwise).
			double fNewValue = fResult <= 50.0 ? Math.Max(fResult, 1.0) * 1.05 : fResult * 0.95;
			if (client.setVariable(new VariableRequest(simVarName, simVarUnit, 0), fNewValue) == HR.OK)
				Log($"Set SimVar '{simVarName}, {simVarUnit}' to: {fNewValue}", "<<");

			// Execute a calculator string with a result.
			// We'll try to read the same value of the SimVar defined above, which should now have changed.
			string calcCode = $"(A:{simVarName},{simVarUnit})";
			if (client.executeCalculatorCode(calcCode, CalcResultType.Double, out fResult) == HR.OK)
				Log($"Calculator code '{calcCode}' returned: {fResult}", "<<");

			// Create and/or Set a Local variable to play with (will be created if it doesn't exist yet, will exist if this program has run during the current simulator session).
			const string variableName = "WASIM_CS_TEST_VAR_1";
			if (client.setOrCreateLocalVariable(variableName, 1.0) == HR.OK)
				Log($"Created/Set local variable {variableName}");

			// We can check that our new variable exists by looking up its ID.
			if (client.lookup(LookupItemType.LocalVariable, variableName, out var localVarId) == HR.OK)
				Log($"Got ID: 0x{localVarId:X} for local variable {variableName}", "<<");
			else
				Log($"Got local variable {variableName} was not found, something went wrong!", "!!");

			// Assuming our variable was created/exists, lets "subscribe" to notifications about when its value changes.
			// First set up the event handler to get the updates.
			client.OnDataReceived += DataSubscriptionHandler;
			// We subscribe to it using a "data request" and set it up to return as a float value, using a predefined value type (we could also use `4` here, for the number of bytes in a float).
			// This should also immediately return the current value, which will be delivered to the DataSubscriptionHandler we assigned earlier.
			hr = client.saveDataRequest(
				new DataRequest(
					requestId:    (uint)Requests.REQUEST_ID_1_FLOAT,
					variableType: 'L',
					variableName: variableName,
					valueSize:    ValueTypes.DATA_TYPE_FLOAT
				)
			);
			if (hr == HR.OK)
				Log($"Subscribed to value changes for local variable {variableName}.");

			// Now let's change the value of our local variable and watch the updates come in via the subscription.
			for (float i = 1.33f; i < 10.0f; i += 0.89f) {
				if (client.setLocalVariable(variableName, i) == HR.OK)
					Log($"Set local variable {variableName} to {i}", ">>");
				else
					break;
				// wait for updates to process asynchronously and our event handler to get called, or time out
				if (!AwaitData())
					break;
			}

			// Test subscribing to a string type value. We'll use the Sim var "ATC AIRLINE" (airplane name), which can only be retrieved using calculator code.
			// We allocate 32 Bytes here to hold the result and we request this one with an update period of Once, which will return a result right away
			// but will not be scheduled for regular updates. If we wanted to update this value later, we could call the client's `updateDataRequest(requestId)` method.
			// A negative `deltaEpsilon` value disables equality checks at the server, so we'll get a value back when we request it again later, even if it hasn't changed.
			// Also we can use the "async" version which doesn't wait for the server to respond before returning. We're going to wait for a result anyway after submitting the request.
			simVarName = "ATC AIRLINE";
			simVarUnit = "string";
			Log($"Requesting {simVarName} variable....", ">>");
			hr = client.saveDataRequestAsync(
				new DataRequest(
					requestId:      (uint)Requests.REQUEST_ID_2_STR,
					resultType:     CalcResultType.String,
					calculatorCode: $"(A:{simVarName}, {simVarUnit})",
					valueSize:      32,
					period:         UpdatePeriod.Once,
					interval:       0,
					deltaEpsilon:   -1.0f
				)
			);
			if (hr == HR.OK)
				AwaitData();
			else
				Log($"saveDataRequestAsync() for '{simVarName}' returned error result {hr}", "!!");

			// Test setting the variable. This test the `setVariable()` overload which takes a string value, though it would be a little more efficient to use `executeCalculatorCode()` directly.
			// I'm adding a semi-random string here, otherwise this test will only seem to work the first time it runs.
			var rand = new Random();
			char[] chars = { (char)('A' + rand.Next(26)), (char)('A' + rand.Next(26)) };
			string stringValue = "Blue Heron Airlines " + new string(chars);
			Log($"Setting variable {simVarName} to string value \"{stringValue}\"...", ">>");
			// `setSimVarVariable()` is just a convenience version of `setVariable()` SimVars, especially string types since we don't need to specify a unit type here.
			hr = client.setSimVarVariable(simVarName, stringValue);
			if (hr == HR.OK) {
				// Since we registered the variable request with "Once" period, we should request an update now.
				client.updateDataRequest((uint)Requests.REQUEST_ID_2_STR);
				// And wait for the update to come in.
				AwaitData();
			}
			else {
				Log($"setSimVarVariable(\"{simVarName}\", \"{stringValue}\") returned error result {hr}", "!!");
			}

			// Test getting a list of our data requests back from the Client.
			Log("Saved Data Requests:", "::");
			var requests = client.dataRequests();
			foreach (DataRequestRecord dr in requests)
				Log(dr.ToString());  // Another convenient ToString() override for logging

			// Test removing a data subscription.
			hr = client.removeDataRequest((uint)Requests.REQUEST_ID_2_STR);
			if (hr != HR.OK)
				Log($"removeDataRequest() for {simVarName} var returned error result {hr}", "!!");

			// Get a list of all local variables...
			// Connect to the list results Event
			client.OnListResults += ListResultsHandler;
			// Request the list.
			client.list(LookupItemType.LocalVariable);
			// Results will arrive asynchronously, so we wait a bit.
			AwaitData(3000);

			// Look up a Key Event ID by name;
			const string eventName = "ATC_MENU_OPEN";  // w/out the "KEY_" prefix.
			if (client.lookup(LookupItemType.KeyEventId, eventName, out var keyId) == HR.OK)
				Log($"Got lookup ID: 0x{keyId:X} for {eventName}", "<<");

			// Trigger the event we just looked up (it won't actually work since all the "ATC_MENU" events are broken in MSFS).
			client.sendKeyEvent((uint)keyId);

			// Try sending a Command directly to the server and wait for a response (Ack/Nak).
			// In this case we'll use a "SendKey" command to activate a Key Event by the ID which we just looked up
			// (which does the same thing we just did above with `sendKeyEvent()`).
			if (client.sendCommandWithResponse(new Command() { commandId = CommandId.SendKey, uData = (uint)keyId }, out var cmdResp) == HR.OK)
				Log($"Got response for SendKey command: {cmdResp}", "<<");

			// Custom calculator event tests.

			// Let's say we want a simple way to toggle a variable value using some RPN code (or repeatedly execute any kind of calculator code).
			// We could use `executeCalculatorCode()` each time, but this isn't very efficient because the code string has to be built, sent,
			// parsed, compiled on the sim side, and then finally executed. Using pre-defined "calculator events" we can skip all the preliminary
			// steps and simply trigger the execution of pre-compiled RPN code via a numeric event ID.
			//
			// For this test we're just going to toggle the test Local variable we created earlier between the values of 0 and 1 based on its current value,
			// using the following basic RPN code:
			// (L:WASIM_TEST_VAR_1) 1 == if{ 0 (>L:WASIM_TEST_VAR_1) } els{ 1 (>L:WASIM_TEST_VAR_1) }
			calcCode = "(L:" + variableName + ") 1 == if{ 0 (>L:" + variableName + ") } els{ 1 (>L:" + variableName + ") }";

			// We need to register that calculator code with the WASim client/server, and we'll use a unique ID to refer to this event later on.
			// The ID is arbitrary (just like request IDs) -- it could be an const enum value, dynamically generated, or whatever else ensures a unique number per event.
			const uint customEventId = 1;
			// We can, optionally, also provide our own name for this custom event when we register it.
			// This name could be used to trigger this custom event from _any_ standard SimConnect client (by mapping to the name using `SimConnect_MapClientEventToSimEvent()`
			// and then triggering with `SimConnect_TransmitClientEvent[_EX1]()`).
			// We don't really need a custom name for this example, but we can actually make use of this later to test another feature.
			hr = client.registerEvent(new RegisteredEvent(customEventId, calcCode, "MyEvents.ToggleTestVariable"));
			if (hr == HR.OK) {
				// Assuming that worked, we can now trigger the custom event in various ways. Since we're still "watching" the test local variable for changes,
				// we should see a new value every time we trigger this event.
				if ((hr = client.transmitEvent(customEventId)) == HR.OK)
					AwaitData();
				else
					Log($"transmitEvent() returned error result 0x{hr:X}", "!!");
			}
			else {
				Log($"Registration of custom calculator event ID {customEventId} with RPN code '{calcCode}' failed with error 0x{hr:X}", "!!");
			}

			// Custom named event registration and triggering test (new for v1.3.0).
			// This tests setting up a custom-named simulator event, which are typically created by 3rd-party aircraft models, such as the FlyByWire A32NX.
			// Here we're (redundantly) using our own custom-named "calculator event" we set up above, just because we know it exists (vs. relying on some specific model to be installed).

			// First the custom-named event needs to be registered with the client. Note we're using the same event name here as we used in the `registerEvent()` call above.
			// This sets up a SimConnect mapping to a generated unique ID. We want to save this ID and use it later to trigger the event efficiently (w/out needing to use the event name again).
			hr = client.registerCustomKeyEvent("MyEvents.ToggleTestVariable", out uint customNamedEventId);
			if (hr == HR.OK) {
				// A successful registration means we can now use the generated event ID to trigger the custom event,
				// using the same `sendKeyEvent()` method as we'd use for a "standard" Key Event.
				Log($"registerCustomKeyEvent() succeeded, returned Event ID {customNamedEventId}");
				// Trigger the event. Should get a variable value change message after this, or a timeout if something went wrong.
				if ((hr = client.sendKeyEvent(customNamedEventId)) == HR.OK)
					AwaitData();
				else
					Log($"sendKeyEvent() returned error result 0x{hr:X}", "!!");
			}
			else {
				Log($"registerCustomKeyEvent() returned error result 0x{hr:X}", "!!");
			}

			// Finished, disconnect.

			// Shutdown (really just the Dispose() will close any/all connections anyway, but this is for example).
			client.disconnectServer();
			client.disconnectSimulator();
			// delete the client
			client.Dispose();
		}


		// This is an event handler for printing Client and Server log messages
		static void LogHandler(LogRecord lr, LogSource src)
		{
			Log($"{src} Log: {lr}", "@@");  // LogRecord has a convenience ToString() override
		}

		// Event handler to print the current Client status.
		static void ClientStatusHandler(ClientEvent ev)
		{
			Log($"Client event {ev.eventType} - \"{ev.message}\"; Client status: {ev.status}", "^^");
		}

		// Event handler for showing listing results (eg. local vars list)
		static void ListResultsHandler(ListResult lr)
		{
			Log($"Got {lr.list.Count} results for list type {lr.listType}. (Uncomment next line in ListResultsHandler() to print them.)");
			//Log(lr.ToString());  // To print all the items just use the ToString() override.
			//Log("End of list results.");

			// signal completion
			dataUpdateEvent.Set();
		}

		// Event handler to process data value subscription updates.
		static void DataSubscriptionHandler(DataRequestRecord dr)
		{
			Console.Write($"[{DateTime.Now.ToString("mm:ss.fff")}] << Got Data for request {(Requests)dr.requestId} \"{dr.nameOrCode}\" with Value: ");
			// Convert the received data into a value using DataRequestRecord's tryConvert() methods.
			// This could be more efficient in a "real" application, but it's good enough for our tests with only 2 value types.
			if (dr.tryConvert(out float fVal))
				Console.WriteLine($"(float) {fVal}");
			else if (dr.tryConvert(out string sVal)) {
				Console.WriteLine($"(string) \"{sVal}\"");
			}
			else
				Log("Could not convert result data to value!", "!!");
			// signal completion
			dataUpdateEvent.Set();
		}

		// Helper function to wait for data to arrive or time out with a log message.
		static bool AwaitData(int ms = 1000)
		{
			if (dataUpdateEvent.WaitOne(ms))
				return true;
			Log("Data update timed out!", "!!");
			return false;
		}

		static void Log(string msg, string prfx = "=:")
		{
			Console.WriteLine("[{0}] {1} {2}", DateTime.Now.ToString("mm:ss.fff"), prfx, msg);
		}

	}

}
