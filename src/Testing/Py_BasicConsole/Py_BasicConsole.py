"""
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
"""

import sys
from os import path as os_path

# first we need to specify a .NET runtime config, which is stored in a file
# this is in case the default isn't .NET 5, which is what the WASimClient assembly uses
from clr_loader import get_coreclr
from pythonnet import set_runtime
set_runtime(get_coreclr("app.runtime.json"))
# now we can import clr from PythonNet
import clr

# this needs to be used  if DLL is not in the same folder or otherwise in the system path
#assembly_path = "./"
#sys.path.insert(0, assembly_path)

# this ensures this script's location (and the DLL) is in the search path in case it's  being run from a different location
sys.path.insert(0, os_path.dirname(os_path.realpath(__file__)))

# now we can finally load our assembly...
clr.AddReference("WASimCommander.WASimClient")
# ... and import what we need
from WASimCommander.CLI.Client import WASimClient
from WASimCommander.CLI import ValueTypes
from WASimCommander.CLI.Enums import (HR, LogLevel, LogFacility, LogSource, CalcResultType, LookupItemType, UpdatePeriod, CommandId)
from WASimCommander.CLI.Structs import (VariableRequest, DataRequest, Command)

from System import Single
from System.Threading import AutoResetEvent

# We use unique IDs to identify data subscription requests. I put the data type in the name just to keep them straight.
REQUEST_ID_1_FLOAT = 0
REQUEST_ID_2_STR = 1

# An event wait handle is used in this test/demo to keep the program alive while data processes in the background.
# (We could use a Python threading.Event() but those need to be reset manually, so the .NET one is simpler to use.)
g_dataUpdateEvent = AutoResetEvent(False)

def main():
	Log("Initializing WASimClient...")

	client = WASimClient(0x317E57E9)

	# Monitor client state changes.
	client.OnClientEvent += ClientStatusHandler
	# Subscribe to incoming log record events.
	client.OnLogRecordReceived += LogHandler

	# As a test, set Client's callback logging level to display messages in the console.
	client.setLogLevel(LogLevel.Info, LogFacility.Remote, LogSource.Client)
	# Set client's console log level to None to avoid double logging to our console. (Client also logs to a file by default.)
	client.setLogLevel(LogLevel.Critical, LogFacility.Console, LogSource.Client)
	# Lets also see some log messages from the server
	client.setLogLevel(LogLevel.Info, LogFacility.Remote, LogSource.Server)

	hr = HR.OK;  # store method invocation results for logging

	# Connect to Simulator (SimConnect) using default timeout period and network configuration (local Simulator)
	if ((hr := client.connectSimulator()) != HR.OK):
		Log("Cannot connect to Simulator, quitting. Error: " + hr.ToString(), "XX")
		client.Dispose()
		return -2

	# Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
	version = client.pingServer()
	if (version == 0):
		Log("Server did not respond to ping, quitting.", "XX")
		client.Dispose()
		return -3

	# Decode version number to dotted format and print it
	Log(f"Found WASimModule Server v{version >> 24}.{(version >> 16) & 0xFF}.{(version >> 8) & 0xFF}.{version & 0xFF}");

	# Try to connect to the server, using default timeout value.
	if ((hr := client.connectServer()) != HR.OK):
		Log("Server connection failed, quitting. Error: " + hr.ToString(), "XX")
		client.Dispose()
		return -4

	# set up a Simulator Variable for testing.
	simVarName = "CG PERCENT"
	simVarUnit = "percent"

	# Execute a calculator string with result. We'll try to read the value of the SimVar defined above.
	calcCode = f"(A:{simVarName},{simVarUnit})"
	# In C# executeCalculatorCode() takes an 'out' type parameters and returns an HR, but in Py everything is returned as a tuple.
	(hr, fResult) = client.executeCalculatorCode(calcCode, CalcResultType.Double)
	if hr == HR.OK:
		Log(f"Calculator code '{calcCode}' returned: {fResult}", "<<")

	# Get a named Sim Variable value, same one as before, but directly using the Gauge API function aircraft_varget()
	(hr, varResult) = client.getVariable(VariableRequest(simVarName, simVarUnit, 0))
	if (hr == HR.OK):
		Log(f"Get SimVar '{simVarName}, {simVarUnit}' returned: {varResult}", "<<")

	# Create and/or Set a Local variable to play with (will be created if it doesn't exist yet, will exist if this program has run during the current simulator session).
	variableName = "WASIM_CS_TEST_VAR_1"
	if client.setOrCreateLocalVariable(variableName, 1.0) == HR.OK:
		Log(f"Created/Set local variable {variableName}")

	# We can check that our new variable exists by looking up its ID.
	(hr, localVarId) = client.lookup(LookupItemType.LocalVariable, variableName)
	if hr == HR.OK:
		Log(f"Got ID: 0x{localVarId:X} for local variable {variableName}", "<<")
	else:
		Log(f"Got local variable {variableName} was not found, something went wrong!", "!!")

	# Assuming our variable was created/exists, lets "subscribe" to notifications about when its value changes.
	# First set up the event handler to get the updates.
	client.OnDataReceived += DataSubscriptionHandler
	# We subscribe to it using a "data request" and set it up to return as a float value, using a predefined value type (we could also use `4` here, for the number of bytes in a float).
	# This should also immediately return the current value, which will be delivered to the DataSubscriptionHandler we assigned earlier.
	if client.saveDataRequest(DataRequest(REQUEST_ID_1_FLOAT, 'L', variableName, ValueTypes.DATA_TYPE_FLOAT)) == HR.OK:
		Log(f"Subscribed to value changes for local variable {variableName}.")
		# We should get a value back right away... wait for it
		if not g_dataUpdateEvent.WaitOne(1000):
			Log("Data subscription update timed out!", "!!")

	# Now let's change the value of our local variable and watch the updates come in via the subscription.
	i = 1.33
	while i < 10.0:
		if client.setLocalVariable(variableName, i) == HR.OK:
			Log(f"Set local variable {variableName} to {i:.2f}", ">>")
		else:
			break
		i += 0.89
		# wait for updates to process asynchronously and our event handler to get called, or time out
		if not g_dataUpdateEvent.WaitOne(1000):
			Log("Data subscription update timed out!", "!!")
			break

	# Test subscribing to a string type value. We'll use the Sim var "TITLE" (airplane name), which can only be retrieved using calculator code.
	# We allocate 32 Bytes here to hold the result and we request this one with an update period of Once, which will return a result right away
	# but will not be scheduled for regular updates. If we wanted to update this value later, we could call the client's `updateDataRequest(requestId)` method.
	hr = client.saveDataRequest(DataRequest(
		requestId = REQUEST_ID_2_STR,
		resultType = CalcResultType.String,
		calculatorCode = "(A:TITLE, String)",
		valueSize = 32,
		period = UpdatePeriod.Once,
		interval = 0,
		deltaEpsilon = 0.0)
	)
	if (hr == HR.OK):
		Log(f"Requested TITLE variable.", ">>")
		if not g_dataUpdateEvent.WaitOne(1000):
			Log("Data subscription update timed out!", "!!")

	# Test getting a list of our data requests back from the Client.
	Log("Saved Data Requests:", "::")
	requests = client.dataRequests()
	for dr in requests:
		Log(dr.ToString())  # Another convenient ToString() override for logging

	# OK, that was fun... now remove the data subscriptions. We could have done it in the loop above but we're testing things here...
	# The server will also remove all our subscriptions when we disconnect, but it's nice to be polite!
	requestIds = client.dataRequestIdsList()
	for reqId in requestIds:
		client.removeDataRequest(reqId)

	# Get a list of all local variables...
	# Connect to the list results Event
	client.OnListResults += ListResultsHandler
	# Request the list.
	client.list(LookupItemType.LocalVariable);
	# Results will arrive asynchronously, so we wait a bit.
	if not g_dataUpdateEvent.WaitOne(3000):
		Log("List results timed out!", "!!")

	# Look up a Key Event ID by name;
	eventName = "ATC_MENU_OPEN";  # w/out the "KEY_" prefix.
	(hr, varId) = client.lookup(LookupItemType.KeyEventId, eventName)
	if hr == HR.OK:
		Log(f"Got lookup ID: 0x{varId:X} for {eventName}", "<<")

	# Try sending a Command directly to the server and wait for a response (Ack/Nak).
	# In this case we'll use a "SendKey" command to activate a Key Event by the ID which we just looked up
	command = Command()
	command.commandId = CommandId.SendKey
	command.uData = varId
	(hr, cmdResp) = client.sendCommandWithResponse(command)
	if hr == HR.OK:
		Log(f"Got response for SendKey command: {cmdResp}", "<<")

	# Shutdown (really just the Dispose() will close any/all connections anyway, but this is for example).
	client.disconnectServer()
	client.disconnectSimulator()
	# delete the client
	client.Dispose()
	return 0


# This is an event handler for printing Client and Server log messages
def LogHandler(logRecord, logSource):
	Log(f"{logSource} Log: {logRecord}", "@@");  # LogRecord has a convenience ToString() override


# Event handler to print the current Client status.
def ClientStatusHandler(clientEvent):
	Log(f"Client event {clientEvent.eventType} - \"{clientEvent.message}\"; Client status: {clientEvent.status}", "^^");


# Event handler for showing listing results (eg. local vars list)
def ListResultsHandler(listResult):
	Log(listResult.ToString());  # just use the ToString() override
	# signal completion
	g_dataUpdateEvent.Set();


# Event handler to process data value subscription updates.
def DataSubscriptionHandler(dataRequestRecord):
	print(f"<< Got Data for request {dataRequestRecord.requestId} \"{dataRequestRecord.nameOrCode}\" with Value: ", end='');
	# Convert the received data into a value using DataRequestRecord's tryConvert() methods.
	# This could be more efficient in a "real" application, but it's good enough for our tests with only 2 value types.
	# Note that in Py we have to explicitly specify the generic parameter type, and use `System.Single` instead of a Py `float` type.
	(ok, fVal) = dataRequestRecord.tryConvert[Single]()
	if (ok):
		print(f"(float) {fVal:.2f}")
	else:
		# Our other value type is a string, so this tryConvert() should attempt to use the System.String overload
		(ok, sVal) = dataRequestRecord.tryConvert()
		if (ok):
			print(f"(string) \"{sVal}\"")
		else:
			print("Could not convert result data to value!");
	# signal completion
	g_dataUpdateEvent.Set()


# Just a simple abstraction of print() with a default prefix
def Log(msg, prfx = ":="):
	print(prfx + ' ' + msg);


if __name__ == "__main__":
	try:
		sys.exit(main())
	except KeyboardInterrupt:
		sys.exit(0)
	except Exception as e:
		from traceback import format_exc
		print(format_exc())
		sys.exit(-1)
