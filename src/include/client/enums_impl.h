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

// DO NOT USE THIS FILE DIRECTLY  -- NO INCLUDE GUARD
// Use client/enums.h

#include <cstdint>
#include <vector>

#ifndef WSMCMND_ENUM_EXPORT
	#define WSMCMND_ENUM_EXPORT
	#define WSMCMND_ENUM_NAMESPACE WASimCommander::Client
#endif

namespace WSMCMND_ENUM_NAMESPACE
{

/// Client status flags. \sa WASimClient::status()
WSMCMND_ENUM_EXPORT enum class ClientStatus : uint8_t
	{
		Idle         = 0x00,  ///< no active SimConnect or WASim server connection
		Initializing = 0x01,  ///< trying to connect to SimConnect
		SimConnected = 0x02,  ///< have SimConnect link
		Connecting   = 0x04,  ///< attempting connection to WASim server
		Connected    = 0x08,  ///< connected to WASim server
		ShuttingDown = 0x10,  ///< closing SimConnect link, before being back in Idle status mode
		AllConnected = SimConnected | Connected,
	};
#if defined(DEFINE_ENUM_FLAG_OPERATORS) && !defined(_MANAGED)
	DEFINE_ENUM_FLAG_OPERATORS(ClientStatus)
#endif
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> ClientStatusNames = { "Idle", "Initializing", "SimConnected", "Connecting", "Connected", "ShuttingDown" };  ///< \refwcc{ClientStatus} enum names.
	/// \}

	/// Client event type enumeration. \sa ClientEvent, ClientStatus, WASimClient::setClientEventCallback()
	WSMCMND_ENUM_EXPORT enum class ClientEventType : uint8_t
	{
		None = 0,
		SimConnecting,        ///< Initializing status
		SimConnected,         ///< SimConnected status
		SimDisconnecting,     ///< ShuttingDown status
		SimDisconnected,      ///< From SimConnected to Initializing status change
		ServerConnecting,     ///< Connecting status
		ServerConnected,      ///< Connected status
		ServerDisconnected,   ///< From Connected to SimConnected status change
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> ClientEventTypeNames = { "None",
		"SimConnecting", "SimConnected", "SimDisconnecting", "SimDisconnected",
		"ServerConnecting", "ServerConnected", "ServerDisconnected" };  ///< \refwcc{ClientEventType} enum names.
	/// \}

	/// Log entry source, Client or Server. \sa WASimClient::logCallback_t
	WSMCMND_ENUM_EXPORT enum class LogSource : uint8_t
	{
		Client,  ///< Log record from WASimClient
		Server   ///< Log record from WASimModule (Server)
	};
	/// \name Enumeration name strings
	/// \{
	static const std::vector<const char *> LogSourceNames = { "Client", "Server" };  ///< \refwcc{LogSource} enum names.
	/// \}

};
