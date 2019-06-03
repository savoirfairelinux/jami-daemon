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

#include "upnp_protocol.h"
#include "../igd/igd.h"
#include "../igd/upnp_igd.h"
#include "../mapping/global_mapping.h"

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

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <string>
#include <chrono>
#include <random>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <opendht/rng.h>

using random_device = dht::crypto::random_device;

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

// UPnP device descriptions.
constexpr static const char * UPNP_ROOT_DEVICE    = "upnp:rootdevice";
constexpr static const char * UPNP_IGD_DEVICE     = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static const char * UPNP_WAN_DEVICE     = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static const char * UPNP_WANCON_DEVICE  = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static const char * UPNP_WANIP_SERVICE  = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static const char * UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

constexpr static int PUPNP_INVALID_ARGS        = 402;
constexpr static int PUPNP_ARRAY_IDX_INVALID   = 713;
constexpr static int PUPNP_CONFLICT_IN_MAPPING = 718;

constexpr static const char * INVALID_ARGS_STR        = "402";
constexpr static const char * ARRAY_IDX_INVALID_STR   = "713";
constexpr static const char * CONFLICT_IN_MAPPING_STR = "718";

constexpr static unsigned PUPNP_PMP_MAX_RETRIES = 20;
constexpr static unsigned PUPNP_E_SUCCESS = UPNP_E_SUCCESS;

typedef std::function<bool(UPnPProtocol*, IGD*)> callbackAddIgd;
typedef std::function<bool(IGD*)> callbackRemoveIgd;
typedef std::function<bool(IpAddr)> callbackRemoveIgdByIp;

class PUPnP : public UPnPProtocol
{
public:
    // Template for adding IGD callback.
    template<class T> void setAddIgdCallback(T* const object, bool(T::* const mf)(UPnPProtocol*, IGD*)) {
        using namespace std::placeholders;
        addIgdCb_ = std::move(std::bind(mf, object, _1, _2));
    }

    // Template for removing IGD callback.
    template<class T> void setRemoveIgdCallback(T* const object, bool(T::* const mf)(IGD*)) {
        using namespace std::placeholders;
        removeIgdCb_ = std::move(std::bind(mf, object, _1));
    }

    // Template for removing IGD by public Ip callback.
    template<class T> void setRemoveIgdByIpCallback(T* const object, bool(T::* const mf)(IpAddr)) {
        using namespace std::placeholders;
        removeIgdByIpCb_ = std::move(std::bind(mf, object, _1));
    }

    PUPnP();
    ~PUPnP();

    // Notifies a change in network.
    void connectivityChanged();

    // Add IGD listener.
    size_t addIGDListener(IGDFoundCallback&& cb);

    // Remove IGD listener.
    void removeIGDListener(size_t token);

    // Sends out async search for IGD.
    void searchForIGD();

    // Tries to add mapping. Assumes mutex is already locked.
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error);

    // Removes a mapping.
    void removeMapping(const Mapping& mapping);

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd);

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
    int handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event, const std::string& udn);

    // Parses the IGD candidate.
    std::unique_ptr<UPnPIGD> parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event);

    // These functions directly create UPnP actions and make synchronous UPnP control point calls. Assumes mutex is already locked.
    bool   actionIsIgdConnected(const UPnPIGD& igd);
    IpAddr actionGetExternalIP(const UPnPIGD& igd);
    void   actionRemoveMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description);
    bool   actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol);
    bool   actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, int* error_code);

private:
    NON_COPYABLE(PUPnP);

    constexpr static unsigned SEARCH_TIMEOUT {30};          // Discovery search is 30 seconds.
    int SUBSCRIBE_TIMEOUT {300};                            // Subscribtion timeout is 300 seconds (5 minutes).

    std::map<std::string, std::shared_ptr<IGD>> validIGDs_; // Map of valid IGDs with their UDN (universal Id).
    mutable std::mutex validIGDMutex_;                      // Mutex used to access these lists and IGDs in a thread-safe manner.
    std::condition_variable validIGDCondVar_;

    std::map<size_t, IGDFoundCallback> igdListeners_;       // Map of valid IGD listeners with their tokens.
    size_t listenerToken_ {0};                              // Last provided token for valid IGD listeners (0 is the invalid token).

    std::map<std::string, std::string> cpDeviceList_;       // Control point device list containing the device ID and device subscription event url.
    std::mutex cpDeviceMutex_;                              // Control point mutex.

    UpnpClient_Handle ctrlptHandle_ {-1};                   // Control point handle.
    std::atomic_bool clientRegistered_ {false};             // Indicates of the client is registered.

    callbackAddIgd addIgdCb_;                               // Add IGD callback to UPnPContext class.
    callbackRemoveIgd removeIgdCb_;                         // Remove IGD callback to UPnPContext class.
    callbackRemoveIgdByIp removeIgdByIpCb_;                 // Remove IGD by Ip address callback to UPnPContext class.
};

std::shared_ptr<PUPnP> getPUPnP();

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