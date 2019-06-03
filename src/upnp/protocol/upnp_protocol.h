/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../igd/igd.h"
#include "../mapping.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <condition_variable>

namespace jami { namespace upnp {

// UPnP device descriptions.
constexpr static const char * UPNP_ROOT_DEVICE	  = "upnp:rootdevice";
constexpr static const char * UPNP_IGD_DEVICE	  = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static const char * UPNP_WAN_DEVICE	  = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static const char * UPNP_WANCON_DEVICE  = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static const char * UPNP_WANIP_SERVICE  = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static const char * UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

// Pure virtual interface class that UPnPContext uses to call protocol functions.
class UPnPProtocol
{
public:
	enum class UpnpError : int {
		INTERNAL_ERROR		   = -911,
		NOT_FOUND			   = -507,
		OUTOF_BOUNDS		   = -506,
		NO_WEB_SERVER		   = -505,
		EXT_NOT_XML			   = -504,
		FILE_READ_ERROR		   = -503,
		FILE_NOT_FOUND		   = -502,
		INVALID_ARGUMENT	   = -501,
		NOTIFY_UNACCEPTED	   = -303,
		UNSUBSCRIBE_UNACCEPTED = -302,
		SUBSCRIBE_UNACCEPTED   = -301,
		EVENT_PROTOCOL		   = -300,
		CANCELED			   = -210,
		FILE_WRITE_ERROR	   = -209,
		SOCK_ERROR			   = -208,
		TIMEDOUT			   = -207,
		LISTEN				   = -206,
		OUTOF_SOCKET		   = -205,
		SOCKET_CONNECT		   = -204,
		SOCKET_BIND			   = -203,
		SOCKET_READ			   = -202,
		SOCKET_WRITE		   = -201,
		NETWORK_ERROR		   = -200,
		INVALID_INTERFACE	   = -121,
		ALREADY_REGISTERED     = -120,
		BAD_HTTPMSG			   = -119,
		URL_TOO_BIG			   = -118,
		INIT_FAILED			   = -117,
		FINISH				   = -116,
		INVALID_ACTION		   = -115,
		BAD_REQUEST			   = -114,
		BAD_RESPONSE		   = -113,
		INVALID_SERVICE		   = -111,
		INVALID_DEVICE		   = -110,
		INVALID_SID			   = -109,
		INVALID_URL			   = -108,
		INVALID_DESC		   = -107,
		BUFFER_TOO_SMALL	   = -106,
		INIT				   = -105,
		OUTOF_MEMORY		   = -104,
		OUTOF_CONTEXT		   = -103,
		OUTOF_HANDLE		   = -102,
		INVALID_PARAM		   = -101,
		INVALID_HANDLE		   = -100,
		INVALID_ERR			   = -1,
		ERROR_OK			   = 0,
		SOAP_INVALID_ACTION    = 401,
		SOAP_INVALID_ARGS      = 402,
		SOAP_OUT_OF_SYNC	   = 403,
		SOAP_INVALID_VAR	   = 404,
		SOAP_ACTION_FAILED     = 501,
		ARRAY_IDX_INVALID	   = 713,
		CONFLICT_IN_MAPPING    = 718
	};

	using IgdListChangedCallback = std::function<bool(UPnPProtocol*, IGD*, IpAddr, bool)>;

	UPnPProtocol(){};
	virtual ~UPnPProtocol(){};

	// Signals a change in the network.
	virtual void connectivityChanged() = 0;

	// Search for IGD.
	virtual void searchForIGD() = 0;

	// Tries to add mapping. Assumes mutex is already locked.
	virtual Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error) = 0;

	// Removes a mapping.
	virtual void removeMapping(const Mapping& mapping) = 0;

	// Removes all local mappings of IGD that we're added by the application.
	virtual void removeAllLocalMappings(IGD* igd) = 0;

	// Set the IGD list callback handler.
	virtual void setOnIgdChanged(IgdListChangedCallback&& cb) = 0;

	// Add IGD listener.
	virtual size_t addIGDListener(IgdFoundCallback&& cb) = 0;

	// Remove IGD listener.
	virtual void removeIGDListener(size_t token) = 0;

protected:
	mutable std::mutex validIGDMutex_;                      // Mutex used to access these lists and IGDs in a thread-safe manner.
	std::condition_variable validIGDCondVar_;				// Variable to notify changes in IGDs.

	std::map<size_t, IgdFoundCallback> igdListeners_;       // Map of valid IGD listeners with their tokens.
	size_t listenerToken_{ 0 };                             // Last provided token for valid IGD listeners (0 is the invalid token).
};

}} // namespace jami::upnp