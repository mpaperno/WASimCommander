/*
SimConnectRequestTracker
https://github.com/mpaperno

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

#ifndef SCRT_THREADSAFE
	#ifdef _LIBCPP_HAS_NO_THREADS
		#define SCRT_THREADSAFE 0
	#else
		#define SCRT_THREADSAFE 1
	#endif
#endif

#include <algorithm>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#if SCRT_THREADSAFE
#include <atomic>
#endif

#if defined(_MSFS_WASM)
#  if !defined(MAX_PATH)
#    include <MSFS/MSFS_WindowsTypes.h>
#  endif
#elif !defined(_WINDOWS_)
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

#include <SimConnect.h>

// grrr...
#ifdef min
#undef min
#undef max
#endif

namespace WASimCommander {
	namespace Utilities
	{

/**
`SimConnectRequestTracker` provides methods for recording and retrieving data associated with SimConnect function calls.
When SimConnect sends an exception message (`SIMCONNECT_RECV_ID_EXCEPTION`), it only provides a "send ID" with
which to identify what caused the exception in the first place. Since requests are asynchronous, there needs to
be some way to record what the original function call was in order to find out what the exception is referring to.

Two primary methods are provided to achieve this goal: `addRequestRecord()` and `getRequestRecord()` which
should be fairly self-explanatory. The former is called right after a SimConnect function is invoked and gets the unique
"send ID" from SimConnect and saved all data in a cache.\n
Use the latter method to find which SimConnect function call caused an exception in the `SIMCONNECT_RECV_ID_EXCEPTION`
handler of your message dispatcher. See the respective documentation for further details on each method.

Request data is available as a `RequestData` structure, which contains all relevant information about the original function call
(method name and arguments list), and also provides convenient output methods like an `ostream <<` operator and a `RequestData::toString()`. Both
will show as much data as possible, including the exception name and at which argument the error occurred (the `dwIndex`).

Request data is stored in a "circular buffer" type cache with a configurable maximum number of records stored. After the maximum is reached, records start to be overwritten,
starting at the oldest. The amount of memory used for the requests cache can be controlled either in the constructor `maxRecords` argument or using the `setMaxRecords()` method.
This memory is pre-allocated on the stack, and each record takes 440B of stack space. Additional heap (de)allocations will happen at runtime by the
`string` and `stringstream` type members of `RequestData` as they are created/destroyed dynamically.
See docs on `SimConnectRequestTracker()` c'tor and `setMaxRecords()` for some more details.

A few convenience methods are also provided for when a request is not tracked (or the record of it wasn't found in cache) or for other logging purposes.
A SimConnect exception name can be looked up based on the `dwException` enum value from the exception message struct using the static `exceptionName()` method.\n
A variadic template is also provided for logging any number of argument values to a stream or string, for example to log a returned error right after a SimConnect function call.

All methods in this class are thread-safe if the `SCRT_THREADSAFE` macro == 1, which is the default unless `_LIBCPP_HAS_NO_THREADS` is defined.

## Examples

Assuming that:
\code
SimConnectRequestTracker g_requestTracker {};
HANDLE hSim;  // handle to an open SimConnect
// some data fields used in the example SimConnect function calls
SIMCONNECT_CLIENT_DATA_DEFINITION_ID cddId;
DWORD szOrType;
float epsilon;
DWORD offset;

// Signature of SimConnect function used in the examples, for reference:
// SimConnect_AddToClientDataDefinition(HANDLE hSimConnect,
//    SIMCONNECT_CLIENT_DATA_DEFINITION_ID DefineID,
//    DWORD dwOffset,
//    DWORD dwSizeOrType,
//    float fEpsilon = 0,
//    DWORD DatumID = SIMCONNECT_UNUSED
// )
\endcode

Basic usage:
\code
HRESULT hr = SimConnect_AddToClientDataDefinition(hSim, cddId, offset, szOrType, epsilon);
if FAILED(hr)
	std::cerr << "SimConnect_AddToClientDataDefinition("  << cddId << ',' << szOrType << ',' << offset << ',' << epsilon << ") failed with " << hr << std::endl;
else
	g_requestTracker.addRequestRecord(hSim, "SimConnect_AddToClientDataDefinition", cddId, szOrType, offset, epsilon);
\endcode

This next example shows how to set up a "proxy" template which will call any SimConnect function passed to it, using any number of arguments,
log any immediate error or add a tracking record, and return the `HRESULT`.

\code
// Main SimConnect function invoker template which does the actual call, logs any error or creates a request record, and returns the HRESULT from SimConnect.
// The first argument, `fname`, is the function name that is being called, as a string, for logging purposes. It can be any string actually, perhaps a full function signature.
// The other arguments are a pointer to the function, a handle to SimConnect, and any further arguments passed along to SimConnect in the function call.
template<typename... Args>
static HRESULT simConnectProxy(const char *fname, std::function<HRESULT(HANDLE, Args...)> f, HANDLE hSim, Args... args) {
	const HRESULT hr = std::bind(f, std::forward<HANDLE>(hSim), std::forward<Args>(args)...)();
	if FAILED(hr)
		std::cerr << "Error: " << fname << '(' << SimConnectRequestTracker::printArgs(args...) << ") failed with " << hr << std::endl;
	else
		simRequestTracker().addRequestRecord(hSim, fname, args...);
	return hr;
}

// Because we really want type deduction, we can invoke simConnectProxy() via this template w/out specifying each argument type for the `simConnectProxy<>()` call.
template<typename... Args>
static HRESULT invokeSimConnect(const char *fname, HRESULT(*f)(HANDLE, Args...), HANDLE hSim, Args... args) {
	return simConnectProxy(fname, std::function<HRESULT(HANDLE, Args...)>(f), hSim, args...);
}

// The macro allows us to only pass the function name once to get both the name string and the actual callable. Optional.
#define INVOKE_SIMCONNECT(F, ...)  invokeSimConnect(#F, &F, __VA_ARGS__)

// ... then use the macro to make the call, or if you don't like macros, or want to use a specific function name (perhaps with signature), specify the name of the function yourself.
// **Note** that unless you're using c++20 (or higher), for the template function type deduction to work, the arguments list and types need to match the SimConnect function signature _exactly_.
// Including any optional arguments (the last 2 in this example).  C++20 apparently improves the deduction rules but I haven't personally tested it.
INVOKE_SIMCONNECT(SimConnect_AddToClientDataDefinition, hSim, cddId, offset, szOrType, epsilon, SIMCONNECT_UNUSED);
// invokeSimConnect("AddToClientDataDefinition(DefineID, dwOffset, dwSizeOrType, fEpsilon, DatumID)", &SimConnect_AddToClientDataDefinition, hSim, cddId, offset, szOrType, epsilon, SIMCONNECT_UNUSED);
\endcode
*/
	class SimConnectRequestTracker
	{
	public:

		// RequestData struct ---------------------------------------------------

		/// SimConnect request (method invocation) tracking record for storing which request caused a simulator error (such as unknown variable/event name, etc).
		struct RequestData
		{
			std::string sMethod;                ///< Name of the function/method which was invoked.
			std::stringstream ssArguments;      ///< Parameter argument values passed in the invoker method. Stored inan I/O stream for potential retrieval as individual values.
			uint32_t dwSendId;                  ///< The "dwSendId" from SimConnect_GetLastSentPacketID() and referenced in the `SIMCONNECT_RECV_EXCEPTION.dwSendId` struct member.
			SIMCONNECT_EXCEPTION eException = SIMCONNECT_EXCEPTION_NONE;    ///< Associated exception, if any, from `SIMCONNECT_RECV_EXCEPTION.dwException` member.
			uint32_t dwExceptionIndex = 0;      ///< The index number of the first parameter that caused an error, if any, from `SIMCONNECT_RECV_EXCEPTION.dwIndex`.
			                                    /// 0 if unknown and index starts at 1, which is the first argument after the HANDLE pointer.
			                                    /// Note that `SIMCONNECT_RECV_EXCEPTION.dwIndex` may sometimes be -1 which means none of the arguments specifically caused the error (usually due to a previous error).
			uint8_t argsCount = 0;              ///< Number of arguments in arguments stream.

			/// Returns formated information about the method invocation which triggered the request, and the SimConnect error, if any. Uses the `ostream <<` operator to generate the string.
			std::string ToString() {
				return (std::ostringstream() << *this).str();
			}

			/// Streams formated information about the method invocation which triggered the request, if any is found, and the SimConnect error, if any.
			friend inline std::ostream& operator<<(std::ostream& os, const RequestData &r) {
				if (r.eException != SIMCONNECT_EXCEPTION_NONE)
					os << "SimConnect exception in packet ID " << r.dwSendId << ": " << exceptionName(r.eException) << " for request: ";
				if (r.sMethod.empty())
					os << "Request record not found.";
				else
					os << r.sMethod << '(' << r.ssArguments.str() << ')';
				if (r.dwExceptionIndex && r.dwExceptionIndex <= r.argsCount)
					os << " (error @ arg# " << r.dwExceptionIndex << ')';
				else if ((int)r.dwExceptionIndex == -1)
					os << " (not due to args)";
				return os;
			}

			// default constructor for a "null" instance, required for container storage (or at least things are a lot less verbose this way).
			explicit RequestData(int sendId = -1) : sMethod{std::string()}, ssArguments{std::stringstream()}, dwSendId(sendId) { }  ///< \private no docs please

		private:
			explicit RequestData(const uint32_t sendId, std::string &&method, std::stringstream &&args, uint8_t argsCount = 0) :
				sMethod{std::move(method)}, ssArguments{std::move(args)}, dwSendId(sendId), argsCount{argsCount} { }
			friend class SimConnectRequestTracker;

		};  // RequestData


		// SimConnectRequestTracker methods ---------------------------------------------------

		/// Construct an instance of the tracker. Typically only one global/static instance should track all requests, but this is not required
		/// (each tracker instance keeps its own log of requests, they are not shared between instances).
		/// \param maxRecords The maximum number of request records to store in the cache. If many requests are made in fast succession (like at the startup
		/// of a SimConnect client requesting a lot of data value subscriptions), some records may get lost (overwritten) by the time SimConnect manages to send
		/// an exception message. On the other hand for low volume usage, memory can be saved by reducing the number of stored records. This property can also
		/// be changed during runtime using `setMaxRecords()` method.
		explicit SimConnectRequestTracker(uint32_t maxRecords = 250)
		{
			setMaxRecords(maxRecords);
		}

		/// Sets the maximum number of request records stored in the cache. See description of the `maxRecords` argument in `SimConnectRequestTracker()` constructor for details.
		/// If this value is reduced from the original (starting) value, all records stored at indexes past the new maximum will be deleted, which may include the most recent records
		/// (they are stored in a round-robin fashion so the current "write slot" may by anywhere in the cache at any given moment).
		void setMaxRecords(uint32_t maxRecords)
		{
			if (maxRecords != m_maxRecords) {
				m_dataResizing = true;
				m_data.resize(maxRecords /*, RequestData(-1)*/);
				if (maxRecords < m_maxRecords) {
					m_data.shrink_to_fit();
					m_dataIndex = maxRecords ? std::min<int>(m_dataIndex, maxRecords - 1) : 0;
				}
				m_maxRecords = maxRecords;
				m_dataResizing = false;
			}
		}

		/// Makes a record of a SimConnect request. Needs a handle to SimConnect to get the last sent ID, which it saves along with the passed
		/// SimConnect function name and any number of arguments which were originally passed to whatever SimConnect function was called.
		/// If needed, the record can later be checked using the `dwSendId` from SimConnect's exception message and the original call which caused
		/// the exception can be logged.
		template <typename... Args>
		void addRequestRecord(HANDLE hSim, std::string &&methodInfo, Args... args) {
			if (m_dataResizing || !m_maxRecords)
				return;
			DWORD dwSendId = 0;
			if (SimConnect_GetLastSentPacketID(hSim, &dwSendId) == 0) {
				std::stringstream ss;
				streamArgs(ss, args...);
				m_dataIndex = (m_dataIndex + 1) % m_maxRecords;  // atomic (if SCRT_THREADSAFE), guaranteed to be accessing a unique vector index.
				m_data[m_dataIndex] = RequestData(dwSendId, std::move(methodInfo), std::move(ss), (uint8_t)sizeof...(args));
			}
		}

		/// Try to find and return a request record for the given dwSendId. If no record is found or the cache is disabled entirely, then it returns
		/// a reference to a static instance (which has no method or argument details) populated with the given `dwSendID`, `ex`, and `idx` parameters.
		/// \param dwSendId The `dwSendId` to look up, typically from the `SIMCONNECT_RECV_EXCEPTION.dwSendId` struct member.
		/// \param ex SimeConnect exception ID, typically from the `SIMCONNECT_RECV_EXCEPTION.dwException` member. This is stored in the returned RequestRecord,
		///        and is resolved to a string name (with `exceptionName()`) for display with the `RequestData.toString()` or stream operator methods.
		/// \param idx SimeConnect exception parameter index, typically from the `SIMCONNECT_RECV_EXCEPTION.dwIndex` member.
		///        This is stored in the returned RequestRecord and is displayed in the `RequestData.toString()` or stream operator method outputs.
		/// \note The returned reference should stay in scope unless the cache is shrunk (and that index gets deleted). However the data could change
		///       at any point if the cache storage slot is reused for a new request. Or, in the cases where a reference to a static instance is returned,
		///       the next `getRequestRecord()` call will overwrite the static data from the previous call.
		///       All this to say: **do not store the reference.**
		RequestData const & getRequestRecord(uint32_t dwSendId, /*SIMCONNECT_EXCEPTION*/ uint32_t ex = SIMCONNECT_EXCEPTION_NONE, uint32_t idx = 0)
		{
			static RequestData nullReq{ -1 };
			RequestData *d = nullptr;
			if (!m_dataResizing && m_maxRecords)  {
				// start at 10 records back from current index since it is more likely that the sendId error relates to a recent request than an old one
				int i = (m_maxRecords + m_dataIndex - std::min<uint32_t>(10UL, m_maxRecords)) % m_maxRecords,
					e = i;
				for (bool first = true; i != e || first; i = (i + 1) % m_maxRecords, first = false) {
					if (m_data[i].dwSendId == dwSendId) {
						d = &m_data.at(i);
						break;
					}
				}
			}
			if (!d) {
				d = &nullReq;
				d->dwSendId = dwSendId;
			}
			d->eException = (SIMCONNECT_EXCEPTION)ex;
			d->dwExceptionIndex = idx;
			return *d;
		}

		// Static functions ---------------------------------------------------

		template <typename T> static bool isNullptr(T* ptr) { return !ptr; }
		template <typename T> static bool isNullptr(T) { return false; }

		/// Recursively output all arguments to a stream, with comma separator between each (no spaces). Args types must have stream operators, obviously.
		template <typename T, typename... Args>
		static void streamArgs(std::ostream &os, T var1, Args... var2)
		{
			if (!isNullptr(var1))
				os << var1;
			else
				os << "NULL";
			if (sizeof...(var2) > 0) {
				os << ',';
				streamArgs(os, var2...);
			}
		}
		// final deduction for last call with no variadic args.
		static void streamArgs(std::ostream &) { /* doesn't actually get called but is required for compilation */ }  ///< \private

		/// Outputs all arguments to a string with comma separators.  Args types must have stream operators (uses `streamArgs()` to create a string).
		template <typename... Args>
		static std::string printArgs(Args... args)
		{
			std::ostringstream ss;
			streamArgs(ss, args...);
			return ss.str();
		}

		/// Get SimConnect exception name (enum as string) from ID enum. Omits the "SIMCONNECT_EXCEPTION" part.
		static const char * const exceptionName(uint32_t id) {
			static const std::map<uint8_t, const char *> names {
				{ SIMCONNECT_EXCEPTION_NONE,                              "NONE" },
				{ SIMCONNECT_EXCEPTION_ERROR,                             "ERROR" },
				{ SIMCONNECT_EXCEPTION_SIZE_MISMATCH,                     "SIZE_MISMATCH" },
				{ SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID,                   "UNRECOGNIZED_ID" },
				{ SIMCONNECT_EXCEPTION_UNOPENED,                          "UNOPENED" },
				{ SIMCONNECT_EXCEPTION_VERSION_MISMATCH,                  "VERSION_MISMATCH" },
				{ SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS,                   "TOO_MANY_GROUPS" },
				{ SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED,                 "NAME_UNRECOGNIZED" },
				{ SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES,              "TOO_MANY_EVENT_NAMES" },
				{ SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE,                "EVENT_ID_DUPLICATE" },
				{ SIMCONNECT_EXCEPTION_TOO_MANY_MAPS,                     "TOO_MANY_MAPS" },
				{ SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS,                  "TOO_MANY_OBJECTS" },
				{ SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS,                 "TOO_MANY_REQUESTS" },
				{ SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT,              "WEATHER_INVALID_PORT" },
				{ SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR,             "WEATHER_INVALID_METAR" },
				{ SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION, "WEATHER_UNABLE_TO_GET_OBSERVATION" },
				{ SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION,  "WEATHER_UNABLE_TO_CREATE_STATION" },
				{ SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION,  "WEATHER_UNABLE_TO_REMOVE_STATION" },
				{ SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE,                 "INVALID_DATA_TYPE" },
				{ SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE,                 "INVALID_DATA_SIZE" },
				{ SIMCONNECT_EXCEPTION_DATA_ERROR,                        "DATA_ERROR" },
				{ SIMCONNECT_EXCEPTION_INVALID_ARRAY,                     "INVALID_ARRAY" },
				{ SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED,              "CREATE_OBJECT_FAILED" },
				{ SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED,            "LOAD_FLIGHTPLAN_FAILED" },
				{ SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE, "OPERATION_INVALID_FOR_OBJECT_TYPE" },
				{ SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION,                 "ILLEGAL_OPERATION" },
				{ SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED,                "ALREADY_SUBSCRIBED" },
				{ SIMCONNECT_EXCEPTION_INVALID_ENUM,                      "INVALID_ENUM" },
				{ SIMCONNECT_EXCEPTION_DEFINITION_ERROR,                  "DEFINITION_ERROR" },
				{ SIMCONNECT_EXCEPTION_DUPLICATE_ID,                      "DUPLICATE_ID" },
				{ SIMCONNECT_EXCEPTION_DATUM_ID,                          "DATUM_ID" },
				{ SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS,                     "OUT_OF_BOUNDS" },
				{ SIMCONNECT_EXCEPTION_ALREADY_CREATED,                   "ALREADY_CREATED" },
				{ SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE,     "OBJECT_OUTSIDE_REALITY_BUBBLE" },
				{ SIMCONNECT_EXCEPTION_OBJECT_CONTAINER,                  "OBJECT_CONTAINER" },
				{ SIMCONNECT_EXCEPTION_OBJECT_AI,                         "OBJECT_AI" },
				{ SIMCONNECT_EXCEPTION_OBJECT_ATC,                        "OBJECT_ATC" },
				{ SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE,                   "OBJECT_SCHEDULE" }
			};
			std::map<uint8_t, const char *>::const_iterator f = names.find((uint8_t)id);
			return f == names.cend() ? "Unknown Exception" : f->second;
		}

	private:
#if SCRT_THREADSAFE
		std::atomic_int m_dataIndex = -1;  // increment before insert, first one yields 0
		std::atomic_bool m_dataResizing = false;
#else
		int m_dataIndex = -1;
		bool m_dataResizing = false;
#endif
		uint32_t m_maxRecords = 0;
		std::vector<RequestData> m_data {};

	};  // SimConnectRequestTracker

	}  // Utilities
}  // WASimCommander
