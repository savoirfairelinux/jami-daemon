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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_LIBNATPMP
#include <natpmp.h>
#endif

#include "logger.h"
#include "ip_utils.h"
#include "igd/upnp_igd.h"
#include "mapping/global_mapping.h"
#include "compiler_intrinsics.h"

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <string>
#include <set>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cstdlib> // for std::free

#include "upnp_context.h"

namespace jami { namespace upnp {

/**
 * This should be used to get a UPnPContext.
 * It only makes sense to have one unless you have separate
 * contexts for multiple internet interfaces, which is not currently
 * supported.
 */
std::shared_ptr<UPnPContext>
getUPnPContext()
{
    static auto context = std::make_shared<UPnPContext>();
    return context;
}

/* UPnP error codes */
constexpr static int INVALID_ARGS = 402;
constexpr static int ARRAY_IDX_INVALID = 713;
constexpr static int CONFLICT_IN_MAPPING = 718;

/* max number of times to retry mapping if it fails due to conflict;
 * there isn't much logic in picking this number... ideally not many ports should
 * be mapped in a system, so a few number of random port retries should work;
 * a high number of retries would indicate there might be some kind of bug or else
 * incompatibility with the router; we use it to prevent an infinite loop of
 * retrying to map the entry
 */
constexpr static unsigned MAX_RETRIES = 20;

#if HAVE_LIBUPNP

/* UPnP IGD definitions */
constexpr static const char * UPNP_ROOT_DEVICE = "upnp:rootdevice";
constexpr static const char * UPNP_IGD_DEVICE = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
constexpr static const char * UPNP_WAN_DEVICE = "urn:schemas-upnp-org:device:WANDevice:1";
constexpr static const char * UPNP_WANCON_DEVICE = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
constexpr static const char * UPNP_WANIP_SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1";
constexpr static const char * UPNP_WANPPP_SERVICE = "urn:schemas-upnp-org:service:WANPPPConnection:1";

constexpr static const char * INVALID_ARGS_STR = "402";
constexpr static const char * ARRAY_IDX_INVALID_STR = "713";
constexpr static const char * CONFLICT_IN_MAPPING_STR = "718";

#else

constexpr static int UPNP_E_SUCCESS = 0;

#endif // HAVE_LIBUPNP

UPnPContext::UPnPContext()
#if HAVE_LIBNATPMP
    : pmpThread_([this]() {
        auto pmp_igd = std::make_shared<PMPIGD>();
        natpmp_t natpmp;

        while (pmpRun_) {
            if (initnatpmp(&natpmp, 0, 0) < 0) {
                JAMI_ERR("NAT-PMP: can't initialize libnatpmp");
                std::unique_lock<std::mutex> lk(pmpMutex_);
                pmpCv_.wait_for(lk, std::chrono::minutes(1));
            } else {
                JAMI_DBG("NAT-PMP: initialized");
                break;
            }
        }

        while (pmpRun_) {
            std::unique_lock<std::mutex> lk(pmpMutex_);
            pmpCv_.wait_until(lk, pmp_igd->getRenewalTime(), [&] {
                return not pmpRun_ or pmp_igd->getRenewalTime() <= clock::now();
            });
            if (not pmpRun_) break;

            auto now = clock::now();

            if (pmp_igd->renewal_ < now) {
                PMPsearchForIGD(pmp_igd, natpmp);
            }
            if (pmpIGD_) {
                if (pmp_igd->clearAll_) {
                    PMPdeleteAllPortMapping(*pmp_igd, natpmp, NATPMP_PROTOCOL_UDP);
                    PMPdeleteAllPortMapping(*pmp_igd, natpmp, NATPMP_PROTOCOL_TCP);
                    pmp_igd->clearAll_ = false;
                    pmp_igd->toRemove_.clear();
                } else if (not pmp_igd->toRemove_.empty()) {
                    decltype(pmp_igd->toRemove_) removed = std::move(pmp_igd->toRemove_);
                    pmp_igd->toRemove_.clear();
                    lk.unlock();
                    for (auto& m : removed) {
                        PMPaddPortMapping(*pmp_igd, natpmp, m, true);
                    }
                    lk.lock();
                }
                auto mapping = pmp_igd->getNextMappingToRenew();
                if (mapping and mapping->renewal_ < now)
                    PMPaddPortMapping(*pmp_igd, natpmp, *mapping);
            }
        }
        closenatpmp(&natpmp);
        JAMI_DBG("NAT-PMP: ended");
    })
#endif
{
#if HAVE_LIBUPNP
    int upnp_err;
    char* ip_address = nullptr;
    char* ip_address6 = nullptr;
    unsigned short port = 0;
    unsigned short port6 = 0;

    /* TODO: allow user to specify interface to be used
     *       by selecting the IP
     */

#ifdef UPNP_ENABLE_IPV6
    /* IPv6 version seems to fail on some systems with message
     * UPNP_E_SOCKET_BIND: An error occurred binding a socket. 
     * TODO: figure out why ipv6 version doesn't work.  
     */
    JAMI_DBG("Upnp: Initializing with UpnpInit2.");
    upnp_err = UpnpInit2(0, 0);
    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_WARN("Upnp: UpnpInit2 Failed to initialize.");
        JAMI_DBG("Upnp: Initializing with UpnpInit (deprecated).");
        upnp_err = UpnpInit(0, 0);      // Deprecated function but fall back on it if UpnpInit2 fails. 
    } 
#else
    JAMI_DBG("Upnp: Initializing with UpnpInit (deprecated).");
    upnp_err = UpnpInit(0, 0);           // Deprecated function but fall back on it if IPv6 not enabled.
#endif

    if (upnp_err != UPNP_E_SUCCESS) {
        JAMI_ERR("Upnp: Can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
    } else {
        JAMI_DBG("Upnp: Initialization successful.");
        ip_address = UpnpGetServerIpAddress();      
        port = UpnpGetServerPort();
        ip_address6 = UpnpGetServerIp6Address();    
        port6 = UpnpGetServerPort6();
        JAMI_DBG("UPnP: Initialiazed on %s:%u | %s:%u", ip_address, port, ip_address6, port6);

        // Relax the parser to allow malformed XML text.
        ixmlRelaxParser(1);

        // Register Upnp control point.
        upnp_err = UpnpRegisterClient(ctrlPtCallback, this, &ctrlptHandle_);
        if (upnp_err != UPNP_E_SUCCESS) {
            JAMI_ERR("UPnP: Can't register client: %s", UpnpGetErrorMessage(upnp_err));
            UpnpFinish();
        } else {
            JAMI_DBG("UPnP: Control point registration successful.");
            clientRegistered_ = true;
        }

        if (clientRegistered_){
            searchForIGD();                 // Start gathering a list of available devices.
        }
    }
#endif
}

UPnPContext::~UPnPContext()
{
    /* make sure everything is unregistered, freed, and UpnpFinish() is called */
    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        for( auto const &it : validIGDs_) {
#if HAVE_LIBUPNP
            if (auto igd = dynamic_cast<UPnPIGD*>(it.second.get()))
                actionRemoveMappingsByLocalIPAndDescription(*igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
#endif
        }
#if HAVE_LIBNATPMP
        if (pmpIGD_) {
            {
                std::lock_guard<std::mutex> lk(pmpMutex_);
                pmpIGD_->clearMappings();
            }
            pmpCv_.notify_all();
        }
#endif
    }

#if HAVE_LIBNATPMP
    pmpRun_ = false;
    pmpCv_.notify_all();
    if (pmpThread_.joinable())
        pmpThread_.join();
    pmpIGD_.reset();
#endif

#if HAVE_LIBUPNP
    if (clientRegistered_)
        UpnpUnRegisterClient(ctrlptHandle_);

// FIXME : on windows thread have already been destroyed at this point resulting in a deadlock
#ifndef _WIN32
    UpnpFinish();
#endif
#endif
}

void
UPnPContext::connectivityChanged()
{
    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);

        /* when the network changes, we're likely no longer connected to the same IGD, or if we are
         * we might now have a different IP, thus we clear the list of IGDs and notify the listeners
         * so that they can attempt to re-do the port mappings once we detect an IGD
         */
        validIGDs_.clear();
#if HAVE_LIBNATPMP
        if (pmpIGD_) {
            std::lock_guard<std::mutex> lk(pmpMutex_);
            pmpIGD_->clear();
            pmpIGD_->renewal_ = clock::now();
            pmpIGD_.reset();
        }
        pmpCv_.notify_all();
#endif
        validIGDCondVar_.notify_all();
        for (const auto& l : igdListeners_)
            l.second();
    }

#if HAVE_LIBUPNP
    // send out a new search request
    searchForIGD();
#endif
}

bool
UPnPContext::hasValidIGD(std::chrono::seconds timeout)
{
    if (not clientRegistered_ and not pmpRun_) {
        JAMI_WARN("UPnP: Control Point not registered");
        return false;
    }

    std::unique_lock<std::mutex> lock(validIGDMutex_);
    if (!validIGDCondVar_.wait_for(lock, timeout,
                                   [this]{return hasValidIGD_unlocked();})) {
        JAMI_WARN("UPnP: check for valid IGD timeout");
        return false;
    }

    return hasValidIGD_unlocked();
}

size_t
UPnPContext::addIGDListener(IGDFoundCallback&& cb)
{
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto token = ++listenerToken_;
    igdListeners_.emplace(token, std::move(cb));
    return token;
}

void
UPnPContext::removeIGDListener(size_t token)
{
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto it = igdListeners_.find(token);
    if (it != igdListeners_.end())
        igdListeners_.erase(it);
}

bool
UPnPContext::hasValidIGD_unlocked() const
{
    return
#if HAVE_LIBNATPMP
    pmpIGD_ or
#endif
    not validIGDs_.empty();
}

/**
 * chooses the IGD to use,
 * assumes you already have a lock on validIGDMutex_
 */
IGD*
UPnPContext::chooseIGD_unlocked() const
{
#if HAVE_LIBNATPMP
    if (pmpIGD_)
        return pmpIGD_.get();
#endif
    if (validIGDs_.empty())
        return nullptr;
    return validIGDs_.begin()->second.get();
}

/**
 * tries to add mapping
 */
Mapping
UPnPContext::addMapping(IGD* igd,
                        uint16_t port_external,
                        uint16_t port_internal,
                        PortType type,
                        int *upnp_error)
{
    *upnp_error = -1;

    Mapping mapping{port_external, port_internal, type};

    /* check if this mapping already exists
     * if the mapping is the same, then we just need to increment the number of users globally
     * if the mapping is not the same, then we have to return fail, as the external port is used
     * for something else
     * if the mapping doesn't exist, then try to add it
     */
    auto globalMappings = type == PortType::UDP ? &igd->udpMappings : &igd->tcpMappings;
    auto iter = globalMappings->find(port_external);
    if (iter != globalMappings->end()) {
        /* mapping exists with same external port */
        GlobalMapping* mapping_ptr = &iter->second;
        if (*mapping_ptr == mapping) {
            /* the same mapping, so nothing needs to be done */
            *upnp_error = UPNP_E_SUCCESS;
            ++(mapping_ptr->users);
            JAMI_DBG("UPnp : mapping already exists, incrementing number of users: %d",
                     iter->second.users);
            return mapping;
        } else {
            /* this port is already used by a different mapping */
            JAMI_WARN("UPnP: cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            *upnp_error = CONFLICT_IN_MAPPING;
            return {};
        }
    }

    /* mapping doesn't exist, so try to add it */
    JAMI_DBG("adding port mapping : %s", mapping.toString().c_str());

#if HAVE_LIBUPNP
    auto upnp = dynamic_cast<const UPnPIGD*>(igd);
    if (not upnp or actionAddPortMapping(*upnp, mapping, upnp_error))
#endif
    {
        /* success; add it to global list */
        globalMappings->emplace(port_external, GlobalMapping{mapping});
#if HAVE_LIBNATPMP
#if HAVE_LIBUPNP
        if (not upnp)
#endif
            pmpCv_.notify_all();
#endif
        return mapping;
    }
    return {};
}

static uint16_t
generateRandomPort()
{
    /* obtain a random number from hardware */
    static random_device rd;
    /* seed the generator */
    static std::mt19937 gen(rd());
    /* define the range */
    static std::uniform_int_distribution<uint16_t> dist(Mapping::UPNP_PORT_MIN, Mapping::UPNP_PORT_MAX);

    return dist(gen);
}

/**
 * chooses a random port that is not yet used by the daemon for UPnP
 */
uint16_t
UPnPContext::chooseRandomPort(const IGD& igd, PortType type)
{
    auto globalMappings = type == PortType::UDP ?
                          &igd.udpMappings : &igd.tcpMappings;

    uint16_t port = generateRandomPort();

    /* keep generating random ports until we find one which is not used */
    while(globalMappings->find(port) != globalMappings->end()) {
        port = generateRandomPort();
    }

    JAMI_DBG("UPnP: chose random port %u", port);

    return port;
}

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
Mapping
UPnPContext::addAnyMapping(uint16_t port_desired,
                           uint16_t port_local,
                           PortType type,
                           bool use_same_port,
                           bool unique)
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        JAMI_WARN("UPnP: no valid IGD available");
        return {};
    }

    auto globalMappings = type == PortType::UDP ?
                          &igd->udpMappings : &igd->tcpMappings;
    if (unique) {
        /* check that port is not already used by the client */
        auto iter = globalMappings->find(port_desired);
        if (iter != globalMappings->end()) {
            /* port already used, we need a unique port */
            port_desired = chooseRandomPort(*igd, type);
        }
    }

    if (use_same_port)
        port_local = port_desired;

    int upnp_error;
    Mapping mapping = addMapping(igd, port_desired, port_local, type, &upnp_error);
    /* keep trying to add the mapping as long as the upnp error is 718 == conflicting mapping
     * if adding the mapping fails for any other reason, give up
     * don't try more than MAX_RETRIES to prevent infinite loops
     */
    unsigned numberRetries = 0;

    while ( not mapping
            and (upnp_error == CONFLICT_IN_MAPPING or upnp_error == INVALID_ARGS)
            and numberRetries < MAX_RETRIES ) {
        /* acceptable errors to keep trying:
         * 718 : conflictin mapping
         * 402 : invalid args (due to router implementation)
         */
        JAMI_DBG("UPnP: mapping failed (conflicting entry? err = %d), trying with a different port.",
                 upnp_error);
        /* TODO: make sure we don't try sellecting the same random port twice if it fails ? */
        port_desired = chooseRandomPort(*igd, type);
        if (use_same_port)
            port_local = port_desired;
        mapping = addMapping(igd, port_desired, port_local, type, &upnp_error);
        ++numberRetries;
    }

    if (not mapping and numberRetries == MAX_RETRIES)
        JAMI_DBG("UPnP: could not add mapping after %u retries, giving up", MAX_RETRIES);

    return mapping;
}

/**
 * tries to remove the given mapping
 */
void
UPnPContext::removeMapping(const Mapping& mapping)
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        JAMI_WARN("UPnP: no valid IGD available");
        return;
    }

    /* first make sure the mapping exists in the global list of the igd */
    auto globalMappings = mapping.getType() == PortType::UDP ?
                          &igd->udpMappings : &igd->tcpMappings;

    auto iter = globalMappings->find(mapping.getPortExternal());
    if ( iter != globalMappings->end() ) {
        /* make sure its the same mapping */
        GlobalMapping& global_mapping = iter->second;
        if (mapping == global_mapping ) {
            /* now check the users */
            if (global_mapping.users > 1) {
                /* more than one user, simply decrement the number */
                --(global_mapping.users);
                JAMI_DBG("UPnP: decrementing users of mapping: %s, %d users remaining",
                         mapping.toString().c_str(), global_mapping.users);
            } else {
                /* no other users, can delete */
#if HAVE_LIBUPNP
                if (auto upnp = dynamic_cast<UPnPIGD*>(igd)) {
                    JAMI_DBG("UPnP: removing port mapping : %s",
                             mapping.toString().c_str());
                    actionDeletePortMapping(*upnp,
                                      mapping.getPortExternalStr(),
                                      mapping.getTypeStr());
                }
#endif
#if HAVE_LIBNATPMP
                if (auto pmp = dynamic_cast<PMPIGD*>(igd)) {
                    {
                        std::lock_guard<std::mutex> lk(pmpMutex_);
                        pmp->toRemove_.emplace_back(std::move(global_mapping));
                    }
                    pmpCv_.notify_all();
                }
#endif
                globalMappings->erase(iter);
            }
        } else {
            JAMI_WARN("UPnP: cannot remove mapping which doesn't match the existing one in the IGD list");
        }
    } else {
        JAMI_WARN("UPnP: cannot remove mapping which is not in the list of existing mappings of the IGD");
    }
}

IpAddr
UPnPContext::getLocalIP() const
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);

    /* if its a valid igd, we must have already gotten the local ip */
    if (auto igd = chooseIGD_unlocked())
        return igd->localIp;

    JAMI_WARN("UPnP: no valid IGD available");
    return {};
}

IpAddr
UPnPContext::getExternalIP() const
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);

    /* if its a valid igd, we must have already gotten the external ip */
    if (auto igd = chooseIGD_unlocked())
        return igd->publicIp;

    JAMI_WARN("UPnP: no valid IGD available");
    return {};
}

#if HAVE_LIBNATPMP

void
UPnPContext::PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp)
{
    if (sendpublicaddressrequest(&natpmp) < 0) {
        JAMI_ERR("NAT-PMP: can't send request");
        pmp_igd->renewal_ = clock::now() + std::chrono::minutes(1);
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            pmp_igd->renewal_ = clock::now() + std::chrono::minutes(5);
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            pmp_igd->localIp = ip_utils::getLocalAddr(AF_INET);
            pmp_igd->publicIp = IpAddr(response.pnu.publicaddress.addr);
            if (not pmpIGD_) {
                JAMI_DBG("NAT-PMP: found new device");
                JAMI_DBG("NAT-PMP: got external IP: %s", pmp_igd->publicIp.toString().c_str());
                {
                    std::lock_guard<std::mutex> lock(validIGDMutex_);
                    pmpIGD_ = pmp_igd;
                    validIGDCondVar_.notify_all();
                    for (const auto& l : igdListeners_)
                        l.second();
                }
            }
            pmp_igd->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
UPnPContext::PMPaddPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, GlobalMapping& mapping, bool remove) const
{
    if (sendnewportmappingrequest(&natpmp,
                                  mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                  mapping.getPortInternal(),
                                  mapping.getPortExternal(), remove ? 0 : 3600) < 0) {
        JAMI_ERR("NAT-PMP: can't send port mapping request");
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        return;
    }
    JAMI_DBG("NAT-PMP: sent port mapping %srequest", remove ? "removal " : "");
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: can't %sregister port mapping", remove ? "un" : "");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            break;
        }
    }
}

void
UPnPContext::PMPdeleteAllPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, int proto) const
{
    if (sendnewportmappingrequest(&natpmp, proto, 0, 0, 0) < 0) {
        JAMI_ERR("NAT-PMP: can't send all port mapping removal request");
        return;
    }
    JAMI_DBG("NAT-PMP: sent all port mapping removal request");
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: can't remove all port mappings");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            break;
        }
    }
}

#endif /* HAVE_LIBNATPMP */


#if HAVE_LIBUPNP

void
UPnPContext::searchForIGD()
{
    if (not clientRegistered_) {
        JAMI_WARN("UPnP: Control Point not registered");
        return;
    }

    /* Send out search for multiple types of devices, as some routers may possibly only reply to one. */
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
}

void
UPnPContext::parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event)
{
    if (not doc or not d_event)
        return;

    /*
     * Check the UDN to see if its already in our device list. If it
     * is, then update the device advertisement timeout (expiration).
     */
    std::string UDN = get_first_doc_item(doc, "UDN");
    if (UDN.empty()) {
        JAMI_DBG("UPnP: could not find UDN in description document of device");
        return;
    } else {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        auto it = validIGDs_.find(UDN);
        if (it != validIGDs_.end()) {
            /* we already have this device in our list */
            /* TODO: update expiration */
            return;
        }
    }

    std::unique_ptr<UPnPIGD> new_igd;
    bool found_connected_IGD = false;
    int upnp_err;

    // Get friendly name.
    std::string friendlyName = get_first_doc_item(doc, "friendlyName");
    
    // Get base URL.
    std::string baseURL = get_first_doc_item(doc, "URLBase");
    if (baseURL.empty()) {
        baseURL = std::string(UpnpDiscovery_get_Location_cstr(d_event));
    }

    // Get list of services defined by serviceType.
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr, ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));
    unsigned long list_length = ixmlNodeList_length(serviceList.get());

    /* 
     * Go through the 'serviceType' nodes until we find the first service of type
     * WANIPConnection or WANPPPConnection which is connected to an external network. 
     */
    for (unsigned long node_idx = 0; node_idx < list_length and not found_connected_IGD; node_idx++) {
        
        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = get_element_text(serviceType_node);

        // Only check serviceType of WANIPConnection or WANPPPConnection.
        if (not serviceType.compare(UPNP_WANIP_SERVICE) == 0 and not serviceType.compare(UPNP_WANPPP_SERVICE) == 0) {
            JAMI_WARN("UPnP: IGD is not WANIP or WANPPP service. Going to next node.");
            continue;
        }
        
        /* 
        * Found a correct 'serviceType'. Now get the parent node because
        * the rest of the service definitions are siblings of 'serviceType'. 
        */
        IXML_Node* service_node = ixmlNode_getParentNode(serviceType_node);
        if (not service_node) {
            JAMI_WARN("UPnP: IGD 'serviceType' has no parent node. Going to next node.");
            continue;
        }

        // Perform sanity check. The parent node should be called "service".
        if(strcmp(ixmlNode_getNodeName(service_node), "service") != 0) {
            JAMI_WARN("UPnP: IGD 'serviceType' parent node is not called 'service'. Going to next node.");
            continue;
        }

        // Get serviceId.
        IXML_Element* service_element = (IXML_Element*)service_node;
        std::string serviceId = get_first_element_item(service_element, "serviceId");
        if (serviceId.empty()){
            JAMI_WARN("UPnP: IGD serviceId is empty. Going to next node.");
            continue;
        }

        // Get the relative controlURL and turn it into absolute address using the URLBase.
        std::string controlURL = get_first_element_item(service_element, "controlURL");
        if (controlURL.empty()) {
            JAMI_WARN("UPnP: IGD control URL is empty. Going to next node.");
            continue;
        }
        char* absolute_control_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), controlURL.c_str(), &absolute_control_url);
        if (upnp_err == UPNP_E_SUCCESS) {
            controlURL = absolute_control_url;
        } else {
            JAMI_WARN("UPnP: Error resolving absolute controlURL -> %s", UpnpGetErrorMessage(upnp_err));
        }
        std::free(absolute_control_url);

        // Get the relative eventSubURL and turn it into absolute address using the URLBase.
        std::string eventSubURL = get_first_element_item(service_element, "eventSubURL");
        if (eventSubURL.empty()) {
            JAMI_WARN("UPnP: IGD event sub URL is empty. Going to next node.");
            continue;
        }
        char* absolute_event_sub_url = nullptr;
        upnp_err = UpnpResolveURL2(baseURL.c_str(), eventSubURL.c_str(), &absolute_event_sub_url);
        if (upnp_err == UPNP_E_SUCCESS) {
            eventSubURL = absolute_event_sub_url;
        } else {
            JAMI_WARN("UPnP: Error resolving absolute eventSubURL -> %s", UpnpGetErrorMessage(upnp_err));
        }
        std::free(absolute_event_sub_url);

        JAMI_DBG("UPnP: Adding IGD.\n\tUDN: %s\n\tBase URL: %s\n\tName: %s\n\tserviceType: %s\n\tserviceID: %s\n\tcontrolURL: %s\n\teventSubURL: %s",
                    UDN.c_str(), baseURL.c_str(), friendlyName.c_str(), serviceType.c_str(), serviceId.c_str(), controlURL.c_str(), eventSubURL.c_str()); 
        new_igd.reset(new UPnPIGD(std::move(UDN),
                                  std::move(baseURL),
                                  std::move(friendlyName),
                                  std::move(serviceType),
                                  std::move(serviceId),
                                  std::move(controlURL),
                                  std::move(eventSubURL)));
        
        if (actionIsIgdConnected(*new_igd)) {
            new_igd->publicIp = actionGetExternalIP(*new_igd);
            if (new_igd->publicIp) {
                JAMI_DBG("UPnP: IGD external IP -> %s", new_igd->publicIp.toString().c_str());
                new_igd->localIp = ip_utils::getLocalAddr(pj_AF_INET());
                if (new_igd->localIp) {
                    JAMI_DBG("UPnP: IGD local IP -> %s", new_igd->localIp.toString().c_str());
                    found_connected_IGD = true;
                }
            }
        }
    }

    // Add IGD to validIGDs list.
    if (not found_connected_IGD) {
        return;
    }

    // Add IGD to validIGDs list.
    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        validIGDs_.emplace(UDN, std::move(new_igd));
        validIGDCondVar_.notify_all();
        for (const auto& l : igdListeners_) {
            l.second();
        }
    }


}

int
UPnPContext::ctrlPtCallback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto upnpContext = static_cast<UPnPContext*>(user_data))
        return upnpContext->handleCtrlPtUPnPEvents(event_type, event);

    JAMI_WARN("UPnP control point callback without UPnPContext");
    return 0;
}

int
UPnPContext::handleCtrlPtUPnPEvents(Upnp_EventType event_type, const void* event)
{
    switch(event_type)
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE: 
    {   
        cpDeviceMutex_.lock();
        const UpnpDiscovery *d_event = (const UpnpDiscovery *)event;
        JAMI_DBG("UPnP: UPNP_DISCOVERY_ADVERTISEMENT_ALIVE from %s", UpnpDiscovery_get_DeviceID_cstr(d_event));
        cpDeviceMutex_.unlock();
        // Fall through. Treat advertisements like discovery search results.   
    }
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        // Lock mutex to prevent handling other discovery search results (or advertisements) simultaneously.
        cpDeviceMutex_.lock();
        const UpnpDiscovery* d_event = (const UpnpDiscovery*)event;
        JAMI_DBG("UPnP: UPNP_DISCOVERY_SEARCH_RESULT from %s", UpnpDiscovery_get_DeviceID_cstr(d_event));

        int upnp_err;

        // First check the error code. 
        if (UpnpDiscovery_get_ErrCode(d_event) != UPNP_E_SUCCESS) {
            JAMI_WARN("UPnP: Error in discovery event received by the CP -> %s", UpnpGetErrorMessage(UpnpDiscovery_get_ErrCode(d_event)));
            cpDeviceMutex_.unlock();
            break; 
        }

        /*
         * Check if this device ID is already in the list. If we reach the past-the-end
         * iterator of the list, it means we haven't discovered it. So we add it.
         */
        auto it = cpDeviceId_.find(std::string(UpnpDiscovery_get_DeviceID_cstr(d_event)));
        if (it == cpDeviceId_.end()) {
            JAMI_DBG("Upnp: New device ID found -> %s", UpnpDiscovery_get_DeviceID_cstr(d_event));
            cpDeviceId_.emplace(std::string(UpnpDiscovery_get_DeviceID_cstr(d_event)));
        } else {
            JAMI_DBG("Upnp: Device %s already processed.", UpnpDiscovery_get_DeviceID_cstr(d_event));
            cpDeviceMutex_.unlock();
            break;
        }

        /*
         * NOTE: This thing will block until success for the system socket timeout
         * unless libupnp is compile with '-disable-blocking-tcp-connections', in
         * which case it will block for the libupnp specified timeout.
         */
        IXML_Document* doc_container_ptr = nullptr;
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> doc_desc_ptr(nullptr, ixmlDocument_free);
        upnp_err = UpnpDownloadXmlDoc(UpnpDiscovery_get_Location_cstr(d_event), &doc_container_ptr);
        doc_desc_ptr.reset(doc_container_ptr);
        if (upnp_err != UPNP_E_SUCCESS or not doc_desc_ptr) {
            JAMI_WARN("UPnP: Error downloading device XML document -> %s", UpnpGetErrorMessage(upnp_err));
            cpDeviceMutex_.unlock();
            break;
        } 

        // Check device type.
        std::string deviceType = get_first_doc_item(doc_desc_ptr.get(), "deviceType");
        if (not deviceType.empty() and deviceType.compare(UPNP_IGD_DEVICE) == 0) {
            JAMI_DBG("UPnP: PARSING IGD");
            parseIGD(doc_desc_ptr.get(), d_event);
        }

        cpDeviceMutex_.unlock();
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        cpDeviceMutex_.lock();
        const UpnpDiscovery *d_event = (const UpnpDiscovery *)event;
        JAMI_DBG("UPnP: UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE from %s", UpnpDiscovery_get_DeviceID_cstr(d_event));
        cpDeviceMutex_.unlock();

        // TODO: Check if its a device we care about and remove it from the relevant lists.
        break;
    }
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        cpDeviceMutex_.lock();
        JAMI_DBG("UPnP: UPNP_DISCOVERY_SEARCH_TIMEOUT.");
        cpDeviceMutex_.unlock();
        break;
    }
    case UPNP_EVENT_RECEIVED:
    {
        cpDeviceMutex_.lock();
        const UpnpEvent *e_event = (const UpnpEvent *)event;
        JAMI_DBG("UPnP: UPNP_EVENT_RECEIVED");
        
        char *xmlbuff = NULL;

		xmlbuff = ixmlPrintNode((IXML_Node *)UpnpEvent_get_ChangedVariables(e_event));
		JAMI_DBG("\tSID: %s\n\tEventKey: %d\n\tChangedVars: %s", 
            UpnpString_get_String(UpnpEvent_get_SID(e_event)), 
            UpnpEvent_get_EventKey(e_event), 
            xmlbuff);
        
		ixmlFreeDOMString(xmlbuff);
        cpDeviceMutex_.unlock();

        // TODO: Handle event by updating any changed state variables */
        break;
    }
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        cpDeviceMutex_.lock();
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("UPnP: UPNP_EVENT_AUTORENEWAL_FAILED");

		JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut %d",
			UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
			UpnpEventSubscribe_get_ErrCode(es_event),
			UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
			UpnpEventSubscribe_get_TimeOut(es_event));
		cpDeviceMutex_.unlock();
        break;
    }
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
    {
        cpDeviceMutex_.lock();
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("UPnP: UPNP_EVENT_SUBSCRIPTION_EXPIRED");

		JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut %d",
			UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
			UpnpEventSubscribe_get_ErrCode(es_event),
			UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
			UpnpEventSubscribe_get_TimeOut(es_event));
		cpDeviceMutex_.unlock();
        break;
    }
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    {
        cpDeviceMutex_.lock();
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("UPnP: UPNP_EVENT_SUBSCRIBE_COMPLETE");
		
        JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut: %d\n",
			UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
			UpnpEventSubscribe_get_ErrCode(es_event),
			UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
			UpnpEventSubscribe_get_TimeOut(es_event));
		cpDeviceMutex_.unlock();
        break;
    }
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: 
    {
        cpDeviceMutex_.lock();
        const UpnpEventSubscribe *es_event = (const UpnpEventSubscribe *)event;
        JAMI_DBG("UPnP: UPNP_EVENT_UNSUBSCRIBE_COMPLETE");
		
        JAMI_DBG("\tSID: %s\n\tErrCode: %d\n\tPublisherURL: %s\n\tTimeOut: %d\n",
			UpnpString_get_String(UpnpEventSubscribe_get_SID(es_event)),
			UpnpEventSubscribe_get_ErrCode(es_event),
			UpnpString_get_String(UpnpEventSubscribe_get_PublisherUrl(es_event)),
			UpnpEventSubscribe_get_TimeOut(es_event));
		cpDeviceMutex_.unlock();
        break;
	}
    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        cpDeviceMutex_.lock();
        const UpnpActionComplete *a_event = (const UpnpActionComplete *)event;
        JAMI_DBG("UPnP: UPNP_CONTROL_ACTION_COMPLETE.");

		char *xmlbuff = NULL;
		int errCode = UpnpActionComplete_get_ErrCode(a_event);
		const char *ctrlURL = UpnpString_get_String(UpnpActionComplete_get_CtrlUrl(a_event));
		IXML_Document *actionRequest = UpnpActionComplete_get_ActionRequest(a_event);
		IXML_Document *actionResult = UpnpActionComplete_get_ActionResult(a_event);

		JAMI_DBG("\tErrCode: %d\n\tCtrlUrl: %s", errCode, ctrlURL);
		
        if (actionRequest) {
			xmlbuff = ixmlPrintNode((IXML_Node *)actionRequest);
			if (xmlbuff) {
				JAMI_DBG("\tActRequest: %s\n", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			JAMI_DBG("\tActRequest: (null)");
		}
		if (actionResult) {
			xmlbuff = ixmlPrintNode((IXML_Node *)actionResult);
			if (xmlbuff) {
				JAMI_DBG("\tActResult: %s", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
			}
			xmlbuff = NULL;
		} else {
			JAMI_DBG("\tActResult: (null)");
		}
        cpDeviceMutex_.unlock();
        /* TODO: no need for any processing here, just print out results.
         * Service state table updates are handled by events. */
        break;
    }
    default:
        JAMI_WARN("UPnP: unhandled Control Point event");
        break;
    }

    return UPNP_E_SUCCESS; /* return value currently ignored by SDK */
}

bool
UPnPContext::actionIsIgdConnected(const UPnPIGD& igd)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    action_container_ptr = UpnpMakeAction("GetStatusInfo", igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("UPnP: Failed to make GetStatusInfo action.");
        return false;
    }
    action.reset(action_container_ptr);
    
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS){
        JAMI_WARN("UPnP: Failed to send GetStatusInfo action -> %s", UpnpGetErrorMessage(upnp_err));
        return false;
    }
    response.reset(response_container_ptr);
    
    if(error_on_response(response.get())) {
        JAMI_WARN("UPnP: Failed to get GetStatusInfo from %s -> %d: %s",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }

    // Parse response.
    std::string status = get_first_doc_item(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") != 0) {
        return false;
    }

    return true;
}

IpAddr
UPnPContext::actionGetExternalIP(const UPnPIGD& igd)
{
    int upnp_err;

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);    // Action pointer.
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);  // Response pointer.
    IXML_Document* action_container_ptr = nullptr;
    IXML_Document* response_container_ptr = nullptr;

    action_container_ptr = UpnpMakeAction("GetExternalIPAddress", igd.getServiceType().c_str(), 0, nullptr);
    if (not action_container_ptr) {
        JAMI_WARN("UPnP: Failed to make GetExternalIPAddress action.");
        return {};
    }
    action.reset(action_container_ptr);
    
    upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(), igd.getServiceType().c_str(), nullptr, action.get(), &response_container_ptr);
    if (upnp_err != UPNP_E_SUCCESS){
        JAMI_WARN("UPnP: Failed to send GetExternalIPAddress action -> %s", UpnpGetErrorMessage(upnp_err));
        return {};
    }
    response.reset(response_container_ptr);

    if(error_on_response(response.get())) {
        JAMI_WARN("UPnP: Failed to get GetExternalIPAddress from %s -> %d: %s",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return {};
    }

    return {get_first_doc_item(response.get(), "NewExternalIPAddress")};
    return {};
}

void
UPnPContext::actionRemoveMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description)
{
    if (!igd.localIp) {
        JAMI_DBG("UPnP: cannot determine local IP in function removeMappingsByLocalIPAndDescription()");
        return;
    }

    JAMI_DBG("UPnP: removing all port mappings with description: \"%s\" and local ip: %s",
             description.c_str(), igd.localIp.toString().c_str());

    int entry_idx = 0;
    bool done = false;

    do {
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
        IXML_Document* action_ptr = nullptr;
        UpnpAddToAction(&action_ptr, "GetGenericPortMappingEntry", igd.getServiceType().c_str(),
                        "NewPortMappingIndex", jami::to_string(entry_idx).c_str());
        action.reset(action_ptr);

        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
        IXML_Document* response_ptr = nullptr;
        int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                      igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
        response.reset(response_ptr);
        if( not response and upnp_err != UPNP_E_SUCCESS) {
            /* TODO: if failed, should we chck if the igd is disconnected? */
            JAMI_WARN("UPnP: Failed to get GetGenericPortMappingEntry from: %s, %d: %s",
                      igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
            return;
        }

        /* check if there is an error code */
        std::string errorCode = get_first_doc_item(response.get(), "errorCode");

        if (errorCode.empty()) {
            /* no error, prase the rest of the response */
            std::string desc_actual = get_first_doc_item(response.get(), "NewPortMappingDescription");
            std::string client_ip = get_first_doc_item(response.get(), "NewInternalClient");

            /* check if same IP and description */
            if (IpAddr(client_ip) == igd.localIp and desc_actual.compare(description) == 0) {
                /* get the rest of the needed parameters */
                std::string port_internal = get_first_doc_item(response.get(), "NewInternalPort");
                std::string port_external = get_first_doc_item(response.get(), "NewExternalPort");
                std::string protocol = get_first_doc_item(response.get(), "NewProtocol");

                JAMI_DBG("UPnP: deleting entry with matching desciption and ip:\n\t%s %5s->%s:%-5s '%s'",
                         protocol.c_str(), port_external.c_str(), client_ip.c_str(), port_internal.c_str(), desc_actual.c_str());

                /* delete entry */
                if (not actionDeletePortMapping(igd, port_external, protocol)) {
                    /* failed to delete entry, skip it and try the next one */
                    ++entry_idx;
                }
                /* note: in the case that the entry deletion is successful, we do not increment the entry
                 *       idx as the number of entries has decreased by one */
            } else
                ++entry_idx;

        } else if (errorCode.compare(ARRAY_IDX_INVALID_STR) == 0
                   or errorCode.compare(INVALID_ARGS_STR) == 0) {
            /* 713 means there are no more entires, but some routers will return 402 instead */
            done = true;
        } else {
            std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
            JAMI_WARN("UPnP: GetGenericPortMappingEntry returned with error: %s: %s",
                      errorCode.c_str(), errorDescription.c_str());
            done = true;
        }
    } while(not done);
}

bool
UPnPContext::actionDeletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol)
{
    std::string action_name{"DeletePortMapping"};
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    IXML_Document* action_ptr = nullptr;
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewRemoteHost", "");
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewExternalPort", port_external.c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewProtocol", protocol.c_str());
    action.reset(action_ptr);

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we check if the igd is disconnected? */
        JAMI_WARN("UPnP: Failed to get %s from: %s, %d: %s", action_name.c_str(),
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return false;
    }
    /* check if there is an error code */
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        JAMI_WARN("UPnP: %s returned with error: %s: %s",
                  action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        return false;
    }
    return true;
}

bool
UPnPContext::actionAddPortMapping(const UPnPIGD& igd, const Mapping& mapping, int* error_code)
{
    *error_code = UPNP_E_SUCCESS;

    std::string action_name{"AddPortMapping"};
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    IXML_Document* action_ptr = nullptr;
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewRemoteHost", "");
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewExternalPort", mapping.getPortExternalStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewProtocol", mapping.getTypeStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewInternalPort", mapping.getPortInternalStr().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewInternalClient", igd.localIp.toString().c_str());
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewEnabled", "1");
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewPortMappingDescription", mapping.getDescription().c_str());
    /* for now assume lease duration is always infinite */
    UpnpAddToAction(&action_ptr, action_name.c_str(), igd.getServiceType().c_str(),
                    "NewLeaseDuration", "0");
    action.reset(action_ptr);

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    if( not response and upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        JAMI_WARN("UPnP: Failed to %s from: %s, %d: %s", action_name.c_str(),
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        *error_code = -1; /* make sure to -1 since we didn't get a response */
        return false;
    }

    /* check if there is an error code */
    std::string errorCode = get_first_doc_item(response.get(), "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(response.get(), "errorDescription");
        JAMI_WARN("UPnP: %s returned with error: %s: %s",
                  action_name.c_str(), errorCode.c_str(), errorDescription.c_str());
        *error_code = jami::stoi(errorCode);
        return false;
    }
    return true;
}

#endif /* HAVE_LIBUPNP */

}} // namespace jami::upnp
