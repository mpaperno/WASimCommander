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

#include <string>
#include <type_traits>
#include <utility>

#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>

//#include "Enums_CLI.h"
#include "client/WASimClient.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace msclr::interop;

// We need this to ignore the errors about "unknown" pragmas which actually work to suppress bogus Intellisense errors. Yeah.
#pragma warning(disable:4068)

/// WASimCommander::CLI::Structs namespace.
/// CLI/.NET versions of WASimCommander API and Client data structures.
namespace WASimCommander::CLI::Structs
{
	using namespace WASimCommander::CLI::Enums;

	// A fixed size char array... since "a member of a managed class cannot be a standard array"
	// ... but apparently it's ok to have to make your own "standard array?" Strange and peculiar.
	// This is used so we can memcpy between native and managed versions of the same packed structs.
	// Originally found as "inline_array template written by Mark Hall and improved upon by Shaun Miller"
	// on an archived blog by one of the CLR developers, Bran Bray:
	// https://web.archive.org/web/20160126134209/http://blogs.msdn.com/b/branbray/archive/2005/07/20/441099.aspx
	//
	// Also the enable_if size check fails with a syntax error no matter what I try... but it's correct code.
	// So don't try to make a zero size one, OK?
	template<uint32_t SIZE /*, typename = std::enable_if_t<SIZE, uint32_t>*/ >
	[Runtime::CompilerServices::UnsafeValueType]
	[StructLayout(LayoutKind::Explicit, Size=(sizeof(Byte)*SIZE))]
	public value struct char_array {
		private:
			[FieldOffset(0)]
			Byte _item;

		public:
			char_array<SIZE>(String ^str) { fromManagedStr(str); }
			char_array<SIZE>(const std::string &str) { fromStdStr(str); }
			char_array<SIZE>(const char *str) : char_array<SIZE>(str, std::strlen(str)) {}
			char_array<SIZE>(const char *str, const size_t len) {
				clear();
				const pin_ptr<Byte> ptr = &_item;
				memcpy(ptr, str, min(SIZE, len));
			}

			void fromStdStr(const std::string &str) {
				clear();
				if (str.empty())
					return;
				const pin_ptr<Byte> ptr = &_item;
				const uint32_t len = std::min<uint32_t>(SIZE-1, (uint32_t)str.size());
				memcpy(ptr, str.data(), len);
				//if (len < SIZE)
					//memset(ptr + len, 0, (size_t)SIZE - len);
			}

			void fromManagedStr(String ^str) {
				clear();
				if (String::IsNullOrEmpty(str))
					return;
				const pin_ptr<Byte> ptr = &_item;
				const array<Byte> ^strData = Text::Encoding::UTF8->GetBytes(str);
				const pin_ptr<Byte> src = &strData[0];
				const uint32_t len = std::min<uint32_t>(SIZE-1, strData->Length);
				memcpy(ptr, src, len);
				//if (len < SIZE)
					//memset(ptr + len, 0, (size_t)SIZE - len);
			}

			void clear() {
				const pin_ptr<Byte> ptr = &_item;
				memset(ptr, 0, SIZE);
			}

			String ^ToString() override { return (String^)*this; }

			operator String ^() {
				const pin_ptr<Byte> ptr = &this->_item;
				return gcnew String((const char *)ptr, 0, (int)std::strlen((const char *)ptr), Text::Encoding::UTF8);
				//return gcnew String(*this, 0, (int)std::strlen(*this), Text::Encoding::UTF8);
			}

			operator std::string() {
				const pin_ptr<Byte> ptr = &this->_item;
				return std::string((const char *)ptr);
				//return std::string(*this);
			}

			Byte% operator[](int index) {
				return *((&_item)+index);
			}

			static operator interior_ptr<Byte>(char_array<SIZE>% ia) {
				return &ia._item;
			}

			// These operators work but I'm not sure why... shouldn't pin_ptr go out of scope?
			//operator Byte *() {
			//	pin_ptr<Byte> ptr = &_item;
			//	return  ptr;
			//}
			//
			//operator const char *() {
			//	pin_ptr<Byte> ptr = &_item;
			//	return reinterpret_cast<const char*>(ptr);
			//}


	};

	/// <summary> Command data structure. The member contents depend on the command type as described in each command type of the `CommandId` enum documentation. \sa CommandId enum </summary>
	/// For full documentation of each field see `WASimCommander::Command`.
	[StructLayout(LayoutKind::Sequential, CharSet = CharSet::Ansi, Pack = 1)]
	public ref struct Command sealed
	{
		public:
			UInt32 token;
			UInt32 uData;
			double fData;
			CommandId commandId;
			char_array<STRSZ_CMD> sData;

			Command() {}
			explicit Command(CommandId id) : commandId(id) { }
			explicit Command(CommandId id, uint32_t uData) : uData(uData), commandId(id) { }
			explicit Command(CommandId id, uint32_t uData, double fData) : uData(uData), fData(fData), commandId(id) { }
#pragma diag_suppress 144   // a value of type "System::String ^" cannot be used to initialize an entity of type "unsigned char"  (sData is not a uchar... someone's confused)
			explicit Command(CommandId id, uint32_t uData, String ^sData) : uData(uData), fData(0.0), commandId(id), sData{sData} { }
			explicit Command(CommandId id, uint32_t uData, String ^sData, double fData) : uData(uData), fData(fData), commandId(id), sData{sData} { }
			explicit Command(CommandId id, uint32_t uData, String ^sData, double fData, int32_t token) : token(token), uData(uData), fData(fData), commandId(id), sData{sData} { }
#pragma diag_restore 144

			void setStringData(String ^sData)
			{
				this->sData = char_array<STRSZ_CMD>(sData);
			}

			String ^ToString() override {
				return String::Format("Command {{{0}; token: {1}; uData: {2}; fData: {3:F6}; sData: \"{4}\"}}", commandId, token, uData, fData, sData);
			}

		internal:
			explicit Command(const WASimCommander::Command &from)
			{
				pin_ptr<UInt32> ptr = &this->token;
				memcpy_s(ptr, Marshal::SizeOf(this), &from, sizeof(WASimCommander::Command));
			}

			inline operator WASimCommander::Command() {
				WASimCommander::Command cmd;
				pin_ptr<UInt32> ptr = &this->token;
				memcpy_s(&cmd, sizeof(WASimCommander::Command), ptr, Marshal::SizeOf(this));
				return cmd;
			}
	};


	/// <summary> Log record structure. \sa WASimCommander:CommandId::Log command. </summary>
	/// For full documentation of each field see `WASimCommander::LogRecord`.
	[StructLayout(LayoutKind::Sequential, CharSet = CharSet::Ansi, Pack = 1)]
	public ref struct LogRecord sealed
	{
		public:
			UInt64 timestamp;  // ms since epoch
			LogLevel level;
			char_array<STRSZ_LOG> message;

			String ^ToString() override {
				return String::Format("{0} [{1}] {2}", DateTimeOffset::FromUnixTimeMilliseconds(timestamp).LocalDateTime.ToString("MM/dd HH:mm:ss.fff"), level, (String ^)message);
			}

		internal:
			explicit LogRecord(const WASimCommander::LogRecord &lr)
			{
				pin_ptr<UInt64> ptr = &this->timestamp;
				memcpy_s(ptr, Marshal::SizeOf(this), &lr, sizeof(WASimCommander::LogRecord));
			}
	};


	/// <summary> Structure for value update subscription requests. \sa WASimCommander:CommandId::Subscribe command. </summary>
	/// For full documentation of each field see `WASimCommander::DataRequest`.
	[StructLayout(LayoutKind::Sequential, CharSet = CharSet::Ansi, Pack = 1)]
	public ref struct DataRequest
	{
		public:
			UInt32 requestId {0};
			UInt32 valueSize {0};
			float deltaEpsilon {0.0f};
			UInt32 interval {0};
			UpdatePeriod period {UpdatePeriod::Never};
			RequestType requestType {RequestType::None};
			CalcResultType calcResultType {CalcResultType::None};
			Byte simVarIndex {0};
			SByte varTypePrefix {'L'};
			char_array<STRSZ_REQ> nameOrCode;
			char_array<STRSZ_UNIT> unitName;

			DataRequest() { }
			/// <summary> Constructs a request for a named variable (`requestType = RequestType::Named`) with default update period on every `UpdatePeriod.Tick`. </summary>
			explicit DataRequest(UInt32 requestId, Char variableType, String ^variableName, UInt32 valueSize) :
				requestId(requestId), valueSize(valueSize), period(UpdatePeriod::Tick),
				requestType(RequestType::Named), varTypePrefix((Byte)variableType), nameOrCode(variableName)
			{	}
			/// <summary> Constructs a request for a named variable (`requestType = RequestType::Named`) with given update period, interval, and epsilon values. </summary>
			explicit DataRequest(UInt32 requestId, Char variableType, String ^variableName, UInt32 valueSize, UpdatePeriod period, UInt32 interval, float deltaEpsilon) :
				requestId(requestId), valueSize(valueSize), deltaEpsilon(deltaEpsilon), interval(interval), period(period),
				requestType(RequestType::Named), varTypePrefix((Byte)variableType), nameOrCode(variableName)
			{	}
			/// <summary> Constructs a request for a named Simulator Variable (`requestType = RequestType::Named` and `varTypePrefix = 'A'`) with default update period on every `UpdatePeriod.Tick`. </summary>
			explicit DataRequest(UInt32 requestId, String ^simVarName, String ^unitName, Byte simVarIndex, UInt32 valueSize) :
				requestId(requestId), valueSize(valueSize), period(UpdatePeriod::Tick),
				requestType(RequestType::Named), simVarIndex(simVarIndex), varTypePrefix('A'), nameOrCode(simVarName), unitName(unitName)
			{	}
			/// <summary> Constructs a request for a named Simulator Variable (`requestType = RequestType::Named` and `varTypePrefix = 'A'`) with given update period, interval, and epsilon values. </summary>
			explicit DataRequest(UInt32 requestId, String ^simVarName, String ^unitName, Byte simVarIndex, UInt32 valueSize, UpdatePeriod period, UInt32 interval, float deltaEpsilon) :
				requestId(requestId), valueSize(valueSize), deltaEpsilon(deltaEpsilon), interval(interval), period(period),
				requestType(RequestType::Named), simVarIndex(simVarIndex), varTypePrefix('A'), nameOrCode(simVarName), unitName(unitName)
			{	}
			/// <summary> Constructs a calculator code request (`requestType = RequestType::Calculated`) with default update period on every `UpdatePeriod.Tick`. </summary>
			explicit DataRequest(UInt32 requestId, CalcResultType resultType, String ^calculatorCode, UInt32 valueSize) :
				requestId(requestId), valueSize(valueSize), period(UpdatePeriod::Tick),
				requestType(RequestType::Calculated), calcResultType(resultType), nameOrCode(calculatorCode)
			{	}
			/// <summary> Constructs a calculator code request (`requestType = RequestType::Calculated`) with given update period, interval, and epsilon values.. </summary>
			explicit DataRequest(UInt32 requestId, CalcResultType resultType, String ^calculatorCode, UInt32 valueSize, UpdatePeriod period, UInt32 interval, float deltaEpsilon) :
				requestId(requestId), valueSize(valueSize), deltaEpsilon(deltaEpsilon), interval(interval), period(period),
				requestType(RequestType::Calculated), calcResultType(resultType), nameOrCode(calculatorCode)
			{	}

			/// <summary>Set the `nameOrCode` member using a `string` type value. </summary>
			void setNameOrCode(String ^nameOrCode)
			{
				this->nameOrCode = char_array<STRSZ_REQ>(nameOrCode);
			}

			/// <summary>Set the `unitName` member using a `string` type value. </summary>
			void setUnitName(String ^unitName)
			{
				this->unitName = char_array<STRSZ_UNIT>(unitName);
			}

			/// <summary>Serializes this `DataRequest` to a string for debugging purposes.</summary>
			String ^ToString() override
			{
				String ^str = String::Format(
					"DataRequest {{{0}; size: {1}; period: {2}; interval: {3}; deltaE: {4:F6}", requestId, valueSize, period, interval, deltaEpsilon
				);
				Text::StringBuilder sb(str);
				if (requestType == RequestType::None)
					return sb.Append("; type: None; }")->ToString();
				if (requestType == RequestType::Named)
					return sb.AppendFormat("; type: Named; name: \"{0}\"; unit: \"{1}\"; varType: '{2}'; varIndex: {3}}}", nameOrCode, unitName, varTypePrefix, simVarIndex)->ToString();
				return sb.AppendFormat("; type: Calculated; code: \"{0}\"; resultType: {1}}}", nameOrCode, calcResultType)->ToString();
			}

		internal:
			explicit DataRequest(const WASimCommander::DataRequest &dr)
			{
				pin_ptr<UInt32> ptr = &this->requestId;
				memcpy_s(ptr, Marshal::SizeOf((DataRequest^)this), &dr, sizeof(WASimCommander::DataRequest));
			}

			inline operator WASimCommander::DataRequest()
			{
				WASimCommander::DataRequest dr(requestId);
				pin_ptr<UInt32> ptr = &this->requestId;
				memcpy_s(&dr, sizeof(WASimCommander::DataRequest), ptr, Marshal::SizeOf((DataRequest^)this));
				return dr;
			}

	};  // DataRequest


	/// <summary> `DataRequestRecord` inherits and extends `WASimCommander::CLI::Structs::DataRequest` with data pertinent for use by a data consumer/Client.
	/// In particular, any value data sent from the server is stored here as a byte array in the `data` member (a byte array). </summary>
	///
	/// `DataRequest` subscription results are delivered by `WASimClient` (via `OnDataReceived` event) using this `DataRequestRecord` structure.
	/// WASimClient also holds a list of all data requests which have been added using `WASimClient::saveDataRequest()` method. These
	/// requests are available for reference using `WASimClient::dataRequest()`, `WASimClient::dataRequests()`, and `WASimClient::dataRequestIdsList()` methods.
	/// For full documentation of each field see `WASimCommander::Client::DataRequestRecord`.
	///
	/// For assistance with converting the raw data to actual values, there are a couple `bool tryConvert(T)` templates which try to populate a value reference
	/// (`out` parameter) of the desired type and returns true or false depending on if the conversion was valid. Validity is based only on the data array size vs.
	/// the requested type size, so it will happily populate an Int32 with data representing a float value, since the type sizes match. **The user must know what data
	/// type they are expecting.**
	///
	/// One version of `tryConvert()` handles basic numeric types, and another is a special overload for returning Strings.
	///
	/// There are also implicit conversion operators for all numeric types and String. If conversion fails, these return a default-constructed instance,
	/// typically zero or an empty string.
	///
	/// Also note that the size of the data array may or _may not_ be the same as the inherited `DataRequest::valueSize` member, since that may contain
	/// special values for preset types. Always check the actual data array size (`data.Length`) if you need to know the storage requirements.
	[StructLayout(LayoutKind::Sequential, CharSet = CharSet::Ansi, Pack = 1)]
	public ref struct DataRequestRecord : public DataRequest
	{
		public:
			time_t lastUpdate = 0;  ///< Timestamp of last data update in ms since epoch.
			array<Byte> ^data {};   ///< Value data array.


			/// <summary> Tries to populate a value reference of the desired type `T` and returns true or false
			/// depending on if the conversion was valid (meaning the size of requested type matches the data size). </summary>
			/// If the conversion fails, result is default-initialized.
			/// The requested type (`T`) must be a `value` type (not reference) and be default-constructible, (eg. numerics, chars), or fixed-size arrays of such types.
			generic<typename T>
#if !DOXYGEN
			where T : value class, gcnew()
#endif
			inline bool tryConvert([Out] T %result)
			{
				if (data->Length == (int)sizeof(T)) {
					pin_ptr<T> pr = &result;
					pin_ptr<Byte> pd = &data[0];
					memcpy_s(pr, sizeof(T), pd, data->Length);
					return true;
				}
				result = T();
				return false;
			}

			/// <summary> Overload for String type results. This will simply load whatever bytes are in the `data` array into a String, and only
			/// returns `false` (and an empty String) if the data array is empty or starts with a null character byte (`0x00`). </summary>
			inline bool tryConvert([Out] String ^ %result)
			{
				if (!data->Length || !data[0]) {
					result = String::Empty;
					return false;
				}
				pin_ptr<Byte> pd = &data[0];
				result = gcnew String((const char *)pd);
				return true;
			}

			/// \name Implicit conversion operators for various types.
			/// \{
			inline static operator double(DataRequestRecord ^dr) { return dr->toType<double>(); }
			inline static operator float(DataRequestRecord ^dr) { return dr->toType<float>(); }
			inline static operator int64_t(DataRequestRecord ^dr) { return dr->toType<int64_t>(); }
			inline static operator uint64_t(DataRequestRecord ^dr) { return dr->toType<uint64_t>(); }
			inline static operator int32_t(DataRequestRecord ^dr) { return dr->toType<int32_t>(); }
			inline static operator uint32_t(DataRequestRecord ^dr) { return dr->toType<uint32_t>(); }
			inline static operator int16_t(DataRequestRecord ^dr) { return dr->toType<int16_t>(); }
			inline static operator uint16_t(DataRequestRecord ^dr) { return dr->toType<uint16_t>(); }
			inline static operator int8_t(DataRequestRecord ^dr) { return dr->toType<int8_t>(); }
			inline static operator uint8_t(DataRequestRecord ^dr) { return dr->toType<uint8_t>(); }
			inline static operator String ^(DataRequestRecord ^dr) { return dr->toType<String ^>(); }
			/// \}

			// can't get generic to work
			//generic<typename T> where T : value class, gcnew()
			//inline static operator T(DataRequestRecord  ^dr)
			//{
			//	//return dr->toType<T>();
			//	T ret = T();
			//	dr->tryConvert(ret);
			//	return ret;
			//}

			/// <summary>Serializes this `DataRequestRecord` to string for debugging purposes.</summary>
			/// To return the request's _value_ as a string, see `tryConvert()` or the `String ^()` operator. \sa DataRequest::ToString()
			String ^ToString() override {
				return String::Format(
					"{0}; DataRequestRecord {{Last Update: {1}; Data: {2}}}",
					DataRequest::ToString(),
					DateTimeOffset::FromUnixTimeMilliseconds(lastUpdate).LocalDateTime.ToString("HH:mm:ss.fff"),
					BitConverter::ToString(data)
				);
			}

		internal:
			explicit DataRequestRecord(const WASimCommander::Client::DataRequestRecord &dr) :
				DataRequest((WASimCommander::DataRequest)dr), lastUpdate{dr.lastUpdate}, data(gcnew array<Byte>((int)dr.data.size()))
			{
				Marshal::Copy(IntPtr((void*)dr.data.data()), data, 0, (int)dr.data.size());
			}

			template<typename T>
			inline T toType()
			{
				T ret = T();
				tryConvert(ret);
				return ret;
			}

	};


	/// <summary> Client Event data, delivered via callback. </summary>
	/// For full documentation of each field see `WASimCommander::Client::ClientEvent`.
	public ref struct ClientEvent sealed
	{
		public:
			ClientEventType eventType;
			ClientStatus status;
			String ^ message {};

			String ^ToString() override {
				return String::Format("ClientEvent {{{0}; status: {1}; message: \"{2}\"}}", eventType, status, message);
			}

		internal:
			explicit ClientEvent(const WASimCommander::Client::ClientEvent &n) {
				eventType = (ClientEventType)n.eventType;
				status = (ClientStatus)n.status;
				message = gcnew String(n.message.c_str());
			}
	};


	/// <summary> Structure for delivering list results, eg. of local variables sent from Server. </summary>
	/// For full documentation of each field see `WASimCommander::Client::ListResult`.
	public ref struct ListResult sealed
	{
		public:
			using ListCollectionType = Dictionary<int, String^>;

			LookupItemType listType;
			HRESULT result;
			ListCollectionType ^list;

			String ^ToString() override {
				Text::StringBuilder sb(String::Format("ListResult {{{0}; result: {1}; Values:\n", listType, result));
				ListCollectionType::Enumerator en = list->GetEnumerator();
				while (en.MoveNext())
					sb.AppendFormat("{0,4} = {1}\n", en.Current.Key, en.Current.Value);
				sb.Append("}");
				return sb.ToString();
			}

		internal:
			explicit ListResult(const WASimCommander::Client::ListResult &r) :
				listType{(LookupItemType)r.listType}, result(r.result), list{gcnew ListCollectionType((int)r.list.size()) }
			{
#pragma diag_suppress 2242  // for list[] operator: expression must have pointer-to-object or handle-to-C++/CLI-array type but it has type "ListCollectionType ^"  (um... isn't `list`  a pointer?)
				for (const auto &pr : r.list)
					list[pr.first] = gcnew String(pr.second.c_str());
#pragma diag_default 2242
			}
	};


	/// <summary> Structure to hold data for registered (reusable) calculator events. Used to submit events with `WASimClient::registerEvent()`. </summary>
	/// For full documentation of each field see `WASimCommander::Client::RegisteredEvent`.
	public ref struct RegisteredEvent
	{
		public:
			UInt32 eventId;
			String ^ code { String::Empty };
			String ^ name { String::Empty };

			RegisteredEvent() {}
			explicit RegisteredEvent(UInt32 eventId, String ^code) :
				eventId{eventId}, code{code} { }
			explicit RegisteredEvent(UInt32 eventId, String ^code, String ^name) :
				eventId{eventId}, code{code}, name{name} { }

			String ^ToString() override {
				return String::Format("RegisteredEvent {{{0}; code: {1}; name: {2}}}", eventId, code, name);
			}

		internal:
			explicit RegisteredEvent(const WASimCommander::Client::RegisteredEvent &ev) :
				eventId{ev.eventId}, code{gcnew String(ev.code.c_str())}, name{gcnew String(ev.name.c_str())}  { }

			operator WASimCommander::Client::RegisteredEvent() {
				marshal_context mc;
				return WASimCommander::Client::RegisteredEvent (eventId, mc.marshal_as<std::string>(code), mc.marshal_as<std::string>(name));
			}
	};


	/// <summary> Structure for using with `WASimClient::getVariable()` and `WASimClient::setVariable()` to specify information about the variable to set or get. Variables and Units can be specified by name or by numeric ID. </summary>
	/// Only some variable types have an associated numeric ID ('A', 'L', 'T' types) and only some variable types accept a Unit specifier ('A', 'C', 'E', 'L' types). Using numeric IDs, if already known, is more efficient
	/// on the server side since it saves the lookup step.
	/// For full documentation of each field see `WASimCommander::Client::VariableRequest`.
	public ref struct VariableRequest
	{
		public:
			SByte variableType { 'L' };
			String ^ variableName { String::Empty };
			String ^ unitName { String::Empty };
			int variableId { -1 };
			int unitId { -1 };
			Byte simVarIndex { 0 };
			bool createLVar = false;

			VariableRequest() {}
			/// <summary> Construct a variable request for specified variable type ('A', 'L', etc) and variable name. </summary>
			explicit VariableRequest(Char variableType, String ^variableName) :
				variableType{(SByte)variableType}, variableName{variableName} { }
			/// <summary> Construct a variable request for specified variable type ('A', 'L', etc) and numeric variable ID. </summary>
			explicit VariableRequest(Char variableType, int variableId) :
				variableType{(SByte)variableType}, variableId{variableId} { }
			/// <summary> Construct a variable request for specified variable type ('A', 'L', etc), variable name, and with a named unit specifier. </summary>
			explicit VariableRequest(Char variableType, String ^variableName, String ^unitName) :
				variableType{(SByte)variableType}, variableName{variableName}, unitName{unitName} { }
			/// <summary> Construct a variable request for specified variable type ('A', 'L', etc),  numeric variable ID, and with a numeric unit ID specifier. </summary>
			explicit VariableRequest(Char variableType, int variableId, int unitId) :
				variableType{(SByte)variableType}, variableId{variableId}, unitId{unitId} { }
			/// <summary> Construct a variable request for a Simulator variable ('A') with given name, named unit specifier, and index (use zero for non-indexed variables). </summary>
			explicit VariableRequest(String ^simVariableName, String ^unitName, Byte simVarIndex) :
				variableType{'A'}, variableName{simVariableName}, unitName{unitName} { }
			/// <summary> Construct a variable request for a Simulator variable ('A') with given variable ID, numeric unit specifier, and index (use zero for non-indexed variables). </summary>
			explicit VariableRequest(int simVariableId, int unitId, Byte simVarIndex) :
				variableType{'A'}, variableId{simVariableId}, unitId{unitId} { }
			/// <summary> Construct a variable request a Local variable ('L') with the specified name. </summary>
			explicit VariableRequest(String ^localVariableName) :
				variableType{'L'}, variableName{localVariableName} { }
			/// <summary> Construct a variable request for a Local ('L') variable with the specified name. </summary>
			/// `createVariable` will create the L var on the simulator if it doesn't exist yet (for "Get" as well as "Set" commands). An optional unit name can also be provided.
			explicit VariableRequest(String ^localVariableName, bool createVariable) :
				variableType{'L'}, variableName{localVariableName}, createLVar{createVariable} { }
			/// <summary> Construct a variable request for a Local ('L') variable with the specified name. </summary>
			/// `createVariable` will create the L var on the simulator if it doesn't exist yet (for "Get" as well as "Set" commands). An unit name can also be provided with this overload.
			explicit VariableRequest(String ^localVariableName, bool createVariable, String ^unitName) :
				variableType{'L'}, variableName{localVariableName}, unitName{unitName}, createLVar{createVariable} { }
			/// <summary> Construct a variable request a Local variable ('L') with the specified numeric ID. </summary>
			explicit VariableRequest(int localVariableId) :
				variableType{'L'}, variableId{localVariableId} { }

			String ^ToString() override {
				return String::Format(
					"VariableRequest {{{0}; variableName: \"{1}\"; unitName: \"{2}\"; variableId: {3}; unitId {4}; simVarIndex: {5}}}",
					variableType, variableName, unitName, variableId, unitId, simVarIndex
				);
			}

		internal:
			inline operator WASimCommander::Client::VariableRequest()
			{
				marshal_context mc;
				WASimCommander::Client::VariableRequest r((char)variableType, mc.marshal_as<std::string>(variableName), mc.marshal_as<std::string>(unitName), simVarIndex, createLVar);
				r.variableId = variableId;
				r.unitId = unitId;
				return r;
			}
	};

}
