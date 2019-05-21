/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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
#define UPNP_STATIC_LIB
#include <windows.h>
#include <wincrypt.h>
#endif
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#endif

#if HAVE_LIBNATPMP
#include <natpmp.h>
#endif

#include "noncopyable.h"
#include "igd/igd.h"
#include "igd/upnp_igd.h"
#include "igd/pmp_igd.h"
#include "mapping/global_mapping.h"

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class UPnPContext {
public:
    constexpr static unsigned SEARCH_TIMEOUT {30};

    int SUBSCRIBE_TIMEOUT {10};

    UPnPContext();
    ~UPnPContext();

    // Check if there is a valid IGD available. 
    bool hasValidIGD(std::chrono::seconds timeout = {});

    // Add IGD listener.
    size_t addIGDListener(IGDFoundCallback&& cb);
    
    // Remove IGD listener.
    void removeIGDListener(size_t token);

    // Tries to add a valid mapping. Will return it if successful. 
    Mapping addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique);

    // Removes a mapping.
    void removeMapping(const Mapping& mapping);

    // Get external Ip of a chosen IGD.
    IpAddr getExternalIP() const;

    // Get our local Ip.
    IpAddr getLocalIP() const;

    // Inform the UPnP context that the network status has changed. This clears the list of known
    void connectivityChanged();

private:
    // Returns first not null IGD in list. Assumes mutex is already locked.
    IGD* chooseIGD_unlocked() const;

    // Checks if you have a valid IGD in the list. Assumes mutex is already locked.
    bool hasValidIGD_unlocked() const;

    // Tries to add mapping. Assumes mutex is already locked.
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error);

    // Returns a random port that is not yet used by the daemon for UPnP.
    uint16_t chooseRandomPort(const IGD& igd, PortType type);

    // Private functions used by libnatpmp
#if HAVE_LIBNATPMP
    void PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp);
    void PMPaddPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, GlobalMapping& mapping, bool remove=false) const;
    void PMPdeleteAllPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, int proto) const;
#endif /* HAVE_LIBNATPMP */

    // Private functions used by libupnp.
#if HAVE_LIBUPNP
    static int ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
    static inline int ctrlPtCallback(Upnp_EventType event_type, void* event, void* user_data) {
	    return ctrlPtCallback(event_type, (const void*)event, user_data);
    };
#endif

    static int subEventCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
    static inline int subEventCallback(Upnp_EventType event_type, void* event, void* user_data) {
	    return subEventCallback(event_type, (const void*)event, user_data);
    };
#endif

    // Callback event handler function for the UPnP client (control point).
    int handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event);

    // Callback function for handing subscription request.
    int handleSubscriptionUPnPEvent(Upnp_EventType event_type, const void* event, std::string udn);

    // Sends out async search for IGD.
    void searchForIGD();

    // Parses the IGD candidate.
    std::unique_ptr<UPnPIGD> parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event);

    // These functions directly create UPnP actions and make synchronous UPnP control point calls. Assumes mutex is already locked.
    bool   actionIsIgdConnected(const UPnPIGD& igd);
    IpAddr actionGetExternalIP(const UPnPIGD& igd);
    void   actionRemoveMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description);
    bool   actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol);
    bool   actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, int* error_code);
#endif /* HAVE_LIBUPNP */

private:
    NON_COPYABLE(UPnPContext);

    std::map<std::string, std::unique_ptr<IGD>> validIGDs_; // Map of valid IGDs with their UDN.
    mutable std::mutex validIGDMutex_;                      // Mutex used to access these lists and IGDs in a thread-safe manner.
    std::condition_variable validIGDCondVar_;               

    std::map<size_t, IGDFoundCallback> igdListeners_;   // Map of valid IGD listeners with their tokens.

    size_t listenerToken_ {0};          // Last provided token for valid IGD listeners (0 is the invalid token).

    // Private variables used by libnatpmp.
#if HAVE_LIBNATPMP
    std::mutex pmpMutex_ {};
    std::condition_variable pmpCv_ {};
    std::shared_ptr<PMPIGD> pmpIGD_ {};
    std::atomic_bool pmpRun_ {true};
    std::thread pmpThread_ {};
#endif  /* HAVE_LIBNATPMP */

    // Private variables used by libupnp.
#if HAVE_LIBUPNP
    std::set<std::string> cpDeviceId_;                  // Control point vector.
    std::mutex            cpDeviceMutex_;               // Control point mutex.
    UpnpClient_Handle     ctrlptHandle_ {-1};           // Control point handle.
    std::atomic_bool      clientRegistered_ {false};    // Indicates of the client is registered.
#if HAVE_LIBNATPMP == 0
    static constexpr bool pmpRun_ {false};              // Set pmpRun_ to false in case we only use libupnp.
#endif

#endif /* HAVE_LIBUPNP */

};

/**
 * This should be used to get a UPnPContext.
 * It only makes sense to have one unless you have separate
 * contexts for multiple internet interfaces, which is not currently
 * supported.
 */
std::shared_ptr<UPnPContext> getUPnPContext();

/* Helper functions for xml parsing. */
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
        /* If there are several nodes which match the tag, we only want the first one */
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
        /* If there are several nodes which match the tag, we only want the first one. */
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
        JAMI_WARN("UPnP: response contains error: %s : %s", errorCode.c_str(), errorDescription.c_str());
        return true;
    }
    return false;
}

}} // namespace jami::upnp
