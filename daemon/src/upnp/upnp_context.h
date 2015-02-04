/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef UPNP_CONTEXT_H_
#define UPNP_CONTEXT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>

#if HAVE_LIBUPNP
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#endif

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"
#include "upnp_igd.h"

namespace ring { namespace upnp {

class UPnPContext {

friend int cp_callback(Upnp_EventType, void*, void*);

public:

#if HAVE_LIBUPNP

    /* search timeout in seconds */
    const int SEARCH_TIMEOUT_SEC = 10;

    UPnPContext();
    ~UPnPContext();

    /**
     * Move constructor and operator not implemented.
     * Its probably best to avoid moving this object
     * as it contains mutexes and containers used by mutltiple
     * threads.
     */

    /**
     * returns 'true' if there is at least one valid (connected) IGD
     * note: this function will until an IGD has been found or SEARCH_TIMEOUT_SEC
     *       have expired; the timeout starts when the context is created as we start
     *       searchign for IGDs immediately
     */
    bool hasValidIGD();

    /**
     * tries to add mapping from and to the port_desired
     * if unique == true, makes sure the client is not using this port already
     * if the mapping fails, tries other available ports until success
     *
     * tries to use a random port between 1024 < > 65535 if desired port fails
     *
     * maps port_desired to port_local; if use_same_port == true, makes sure that
     * that the external and internal ports are the same
     *
     * returns a valid mapping on success and an invalid mapping on failure
     */
    Mapping addAnyMapping(uint16_t port_desired, uint16_t port_local, PortType type, bool use_same_port, bool unique);

    /**
     * addAnyMapping with the local port being the same as the external port
     *
     * returns a valid mapping on success and an invalid mapping on failure
     */
    Mapping addAnyMapping(uint16_t port_desired, PortType type, bool unique);

    /**
     * tries to remove the given mapping
     */
    void removeMapping(const Mapping& mapping);

#else
    /* use default constructor and destructor */
    UPnPContext() = default;
    ~UPnPContext() = default;;
#endif

    bool isInitialized() const { return initialized_; };

private:
    NON_COPYABLE(UPnPContext);

    bool initialized_ {false};

#if HAVE_LIBUPNP

    /**
     * This is used to maintain of devices whose description
     * we are in the process of downloading so that we don't try to
     * download the same description more than one at a time.
     * This is mainly to prevent several threads from blocking
     * at the same time when a connection cannot be established.
     *
     * The mutex is to access the set in a thread safe manner
     */
    std::set<std::string> cp_checking_devices_;
    std::mutex cp_mutex_;

    /**
     * control and device handles;
     * set by the SDK once each is registered
     */
    UpnpClient_Handle ctrlpt_handle_ {-1};
    UpnpDevice_Handle device_handle_ {-1};

    /**
     * keep track if we've successfully registered
     * a client and/ore device
     */
    bool client_registered_ {false};
    bool device_registered_ {false};

    /**
     * map of valid IGDs - IGDs which have the correct services
     * map of connected IGDs - which are valid IGDs which are connected to the internet
     *                         and which have an external IP
     *
     * the UDN string is used to uniquely identify the IGD
     *
     * the mutex is used to access these lists and IGDs in a thread-safe manner
     */
    std::map<std::string, std::unique_ptr<IGD>> valid_igds_;
    // std::map<std::string, std::shared_ptr<IGD>> connected_igds_;
    std::mutex igd_mutex_;
    /**
     * chooses the IGD to use,
     * assumes you already have a lock on igd_mutex_
     */
    IGD* chooseIGD_unlocked();

    /* sends out async search for IGD */
    void searchForIGD();
    /* vars to sync search timeout */
    unsigned pending_igd_search_requests_ {0};
    std::mutex igd_search_mutex_;
    std::condition_variable igd_search_condition_;

    /**
     * Parses the device description and adds desired devices to
     * relevant lists
     */
    void parseDevice(IXML_Document* doc, const Upnp_Discovery* d_event);
    void parseIGD(IXML_Document* doc, const Upnp_Discovery* d_event);

    /* tries to add mapping, must have lock on igd_mutex */
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error);
    uint16_t chooseRandomPort(const IGD* igd, PortType type);

    /* these functions directly create UPnP actions and make synchronous UPnP control point calls */
    bool isIGDConnected(const IGD* igd);
    IpAddr getExternalIP(const IGD* igd);
    void removeMappingsByLocalIPAndDescription(const IGD* igd, const std::string& description);
    bool deletePortMapping(const IGD* igd, const std::string& port_external, const std::string& protocol);
    bool addPortMapping(const IGD* igd, const Mapping& mapping, int* error_code);

#endif /* HAVE_LIBUPNP */

};

std::shared_ptr<UPnPContext> getUPnPContext();

}} // namespace ring::upnp

#endif /* UPNP_CONTEXT_H_ */