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

    UPnPContext();
    ~UPnPContext();

    /**
     * Returns 'true' if there is at least one valid (connected) IGD.
     * @param timeout Time to wait until a valid IGD is found.
     * If timeout is not given or 0, the function pool (non-blocking).
     */
    bool hasValidIGD(std::chrono::seconds timeout = {});

    size_t addIGDListener(IGDFoundCallback&& cb);
    void removeIGDListener(size_t token);

    /**
     * tries to add mapping from and to the port_desired
     * if unique == true, makes sure the client is not using this port already
     * if the mapping fails, tries other available ports until success
     *
     * tries to use a random port between 1024 < > 65535 if desired port fails
     *
     * maps port_desired to port_local; if use_same_port == true, makes sure
     * that the external and internal ports are the same
     *
     * returns a valid mapping on success and an invalid mapping on failure
     */
    Mapping addAnyMapping(uint16_t port_desired,
                          uint16_t port_local,
                          PortType type,
                          bool use_same_port,
                          bool unique);

    /**
     * tries to remove the given mapping
     */
    void removeMapping(const Mapping& mapping);

    /**
     * tries to get the external ip of the router
     */
    IpAddr getExternalIP() const;


    /**
     * get our local ip
     */
    IpAddr getLocalIP() const;

    /**
     * Inform the UPnP context that the network status has changed. This clears the list of known
     * IGDs
     */
    void connectivityChanged();

private:
    NON_COPYABLE(UPnPContext);

    std::atomic_bool clientRegistered_ {false};

    /**
     * map of valid IGDs - IGDs which have the correct services and are connected
     * to some external network (have an external IP)
     *
     * the UDN string is used to uniquely identify the IGD
     *
     * the mutex is used to access these lists and IGDs in a thread-safe manner
     */
    std::map<std::string, std::unique_ptr<IGD>> validIGDs_;
    mutable std::mutex validIGDMutex_;
    std::condition_variable validIGDCondVar_;

    /**
     * Map of valid IGD listeners.
     */
    std::map<size_t, IGDFoundCallback> igdListeners_;

    /**
     * Last provided token for valid IGD listeners.
     * 0 is the invalid token.
     */
    size_t listenerToken_ {0};

    /**
     * chooses the IGD to use (currently selects the first one in the map)
     * assumes you already have a lock on igd_mutex_
     */
    IGD* chooseIGD_unlocked() const;
    bool hasValidIGD_unlocked() const;

    /* tries to add mapping, assumes you already have lock on igd_mutex_ */
    Mapping addMapping(IGD* igd,
                       uint16_t port_external,
                       uint16_t port_internal,
                       PortType type,
                       int *upnp_error);

    uint16_t chooseRandomPort(const IGD& igd, PortType type);

#if HAVE_LIBNATPMP
    std::mutex pmpMutex_ {};
    std::condition_variable pmpCv_ {};
    std::shared_ptr<PMPIGD> pmpIGD_ {};
    std::atomic_bool pmpRun_ {true};
    std::thread pmpThread_ {};

    void PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp);
    void PMPaddPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, GlobalMapping& mapping, bool remove=false) const;
    void PMPdeleteAllPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, int proto) const;
#else
    static constexpr bool pmpRun_ {false};
#endif

#if HAVE_LIBUPNP

    /**
     * UPnP devices typically send out several discovery
     * packets at the same time. libupnp creates a separate event
     * for each discovery packet which is processed in the threadpool,
     * even if the multiple discovery packets are received from the
     * same IP at the same time. In order to prevent trying
     * to download and parse the device description from the
     * same location in multiple threads at the same time, we
     * keep track from which URL(s) we are in the process of downloading
     * and parsing the device description in this set.
     *
     * The main purspose of this is to prevent blocking multiple
     * threads when trying to download the description from an
     * unresponsive device (the timeout can be several seconds)
     *
     * The mutex is to access the set in a thread safe manner
     */

    std::set<std::string> cpDeviceId_;
    std::mutex cpDeviceMutex_;

    /**
     * control and device handles;
     * set by the SDK once each is registered
     */
    UpnpClient_Handle ctrlptHandle_ {-1};
    UpnpDevice_Handle deviceHandle_ {-1};

    /**
     * keep track if we've successfully registered a device
     */
    bool deviceRegistered_ {false};

    static int ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data);
#if UPNP_VERSION < 10800
    static inline int ctrlPtCallback(Upnp_EventType event_type, void* event, void* user_data) {
	    return cp_ctrlPtCallback(event_type, (const void*)event, user_data);
    };
#endif

    /**
     * callback function for the UPnP client (control point)
     * all UPnP events received by the client are processed here
     */
    int handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event);

    /* sends out async search for IGD */
    void searchForIGD();

    /**
     * Parses the device description and adds desired devices to
     * relevant lists
     */
    void parseDevice(IXML_Document* doc, const UpnpDiscovery* d_event);

    void parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event);


    /* these functions directly create UPnP actions
     * and make synchronous UPnP control point calls
     * they assume you have a lock on the igd_mutex_ */
    bool isIGDConnected(const UPnPIGD& igd);

    IpAddr getExternalIP(const UPnPIGD& igd);

    void removeMappingsByLocalIPAndDescription(const UPnPIGD& igd,
                                               const std::string& description);

    bool deletePortMapping(const UPnPIGD& igd,
                           const std::string& port_external,
                           const std::string& protocol);

    bool addPortMapping(const UPnPIGD& igd,
                        const Mapping& mapping,
                        int* error_code);
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

static void
checkResponseError(IXML_Document* doc)
{
    if (not doc)
        return;

    std::string errorCode = get_first_doc_item(doc, "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(doc, "errorDescription");
        JAMI_WARN("UPnP: response contains error: %s : %s", errorCode.c_str(), errorDescription.c_str());
    }
}

}} // namespace jami::upnp
