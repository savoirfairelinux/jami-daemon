/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#if HAVE_LIBUPNP

#ifdef _WIN32
#define UPNP_USE_MSVCPP
#define UPNP_STATIC_LIB
#endif

#include "../upnp_protocol.h"
#include "../global_mapping.h"
#include "../igd.h"
#include "upnp_igd.h"

#include "logger.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif
#include <opendht/rng.h>

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <cstdlib>

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class PUPnP : public UPnPProtocol
{
public:
	PUPnP();
	~PUPnP();

	// Notifies a change in network.
	void connectivityChanged() override;

	// Sends out async search for IGD.
	void searchForIGD() override;

	// Tries to add mapping. Assumes mutex is already locked.
	Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error) override;

	// Removes a mapping.
	void removeMapping(const Mapping& mapping) override;

	// Removes all local mappings of IGD that we're added by the application.
	void removeAllLocalMappings(IGD* igd) override;

private:
	// Control point callback.
	static int ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
	static inline int ctrlPtCallback(Upnp_EventType event_type, void* event, void* user_data) {
		return ctrlPtCallback(event_type, (const void*)event, user_data);
	};
#endif

	// Callback event handler function for the UPnP client (control point).
	int handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event);

	// Subscription event callback.
	static int subEventCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
	static inline int subEventCallback(Upnp_EventType event_type, void* event, void* user_data) {
		return subEventCallback(event_type, (const void*)event, user_data);
	};
#endif

	// Callback subscription event function for handling subscription request.
	int handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event);

	// Parses the IGD candidate.
	std::unique_ptr<UPnPIGD> parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event);

	// These functions directly create UPnP actions and make synchronous UPnP control point calls. Assumes mutex is already locked.
	bool   actionIsIgdConnected(const UPnPIGD& igd);
	IpAddr actionGetExternalIP(const UPnPIGD& igd);
	void   actionRemoveMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description);
	bool   actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol);
	bool   actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, UPnPProtocol::UpnpError& upnp_error);

private:
	NON_COPYABLE(PUPnP);

	constexpr static unsigned SEARCH_TIMEOUT {30};          // Discovery search is 30 seconds.
	int SUBSCRIBE_TIMEOUT {300};                            // Subscribtion timeout is 300 seconds (5 minutes).

	std::map<std::string, std::shared_ptr<IGD>> validIGDs_; // Map of valid IGDs with their UDN (universal Id).

	std::map<std::string, std::string> cpDeviceList_;       // Control point device list containing the device ID and device subscription event url.
	std::mutex cpDeviceMutex_;                              // Control point mutex.

	UpnpClient_Handle ctrlptHandle_ {-1};                   // Control point handle.
	std::atomic_bool clientRegistered_ {false};             // Indicates of the client is registered.
};

// Helper functions for xml parsing.
static std::string
get_element_text(IXML_Node* node)
{
	std::string ret;
	if (node) {
		IXML_Node *textNode = ixmlNode_getFirstChild(node);
		if (textNode) {
			const char* value = ixmlNode_getNodeValue(textNode);
			if (value)
				ret = std::string(value);
		}
	}
	return ret;
}

static std::string
get_first_doc_item(IXML_Document* doc, const char* item)
{
	std::string ret;
	std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
	if (nodeList) {
		// If there are several nodes which match the tag, we only want the first one.
		ret = get_element_text(ixmlNodeList_item(nodeList.get(), 0));
	}
	return ret;
}

static std::string
get_first_element_item(IXML_Element* element, const char* item)
{
	std::string ret;
	std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
	if (nodeList) {
		// If there are several nodes which match the tag, we only want the first one.
		ret = get_element_text(ixmlNodeList_item(nodeList.get(), 0));
	}
	return ret;
}

static bool
error_on_response(IXML_Document* doc)
{
	if (not doc)
		return true;

	std::string errorCode = get_first_doc_item(doc, "errorCode");
	if (not errorCode.empty()) {
		std::string errorDescription = get_first_doc_item(doc, "errorDescription");
		JAMI_WARN("PUPnP: Response contains error: %s : %s", errorCode.c_str(), errorDescription.c_str());
		return true;
	}
	return false;
}

}} // namespace jami::upnp

#endif