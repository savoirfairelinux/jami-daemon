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
#include "upnp_igd.h"
#include "compiler_intrinsics.h"

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include <string>
#include <set>
#include <mutex>
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

/*
 * Local prototypes
 */
static std::string get_element_text(IXML_Node*);
static std::string get_first_doc_item(IXML_Document*, const char*);
static std::string get_first_element_item(IXML_Element*, const char*);
static void checkResponseError(IXML_Document*);

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
    unsigned short port = 0;

    /* TODO: allow user to specify interface to be used
     *       by selecting the IP
     */

 #ifdef UPNP_ENABLE_IPV6
     JAMI_DBG("UPnP: IPv6 support enabled, but we will use IPv4");
     /* IPv6 version seems to fail on some systems with:
      * UPNP_E_SOCKET_BIND: An error occurred binding a socket. */
     /* TODO: figure out why ipv6 version doesn't work  */
     // upnp_err = UpnpInit2(0, 0);
 #endif
    upnp_err = UpnpInit(0, 0);
    if ( upnp_err != UPNP_E_SUCCESS ) {
        JAMI_ERR("UPnP: can't initialize libupnp: %s", UpnpGetErrorMessage(upnp_err));
        UpnpFinish();
    } else {
        JAMI_DBG("UPnP: using IPv4");
        ip_address = UpnpGetServerIpAddress(); // do not free, it is freed by UpnpFinish()
        port = UpnpGetServerPort();

        JAMI_DBG("UPnP: initialiazed on %s:%u", ip_address, port);

        // relax the parser to allow malformed XML text
        ixmlRelaxParser( 1 );

        // Register a control point to start looking for devices right away
        upnp_err = UpnpRegisterClient( cp_callback, this, &ctrlptHandle_ );
        if ( upnp_err != UPNP_E_SUCCESS ) {
            JAMI_ERR("UPnP: can't register client: %s", UpnpGetErrorMessage(upnp_err));
            UpnpFinish();
        } else {
            clientRegistered_ = true;
            // start gathering a list of available devices
            searchForIGD();
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
                removeMappingsByLocalIPAndDescription(*igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
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
        UpnpUnRegisterClient( ctrlptHandle_ );

    if (deviceRegistered_)
        UpnpUnRegisterRootDevice( deviceHandle_ );
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
    if (not upnp or addPortMapping(*upnp, mapping, upnp_error))
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
                    deletePortMapping(*upnp,
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

    /* send out search for both types, as some routers may possibly only reply to one */
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_ROOT_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_IGD_DEVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANIP_SERVICE, this);
    UpnpSearchAsync(ctrlptHandle_, SEARCH_TIMEOUT, UPNP_WANPPP_SERVICE, this);
}

/**
 * Parses the device description and adds desired devices to
 * relevant lists
 */
void
UPnPContext::parseDevice(IXML_Document* doc, const UpnpDiscovery* d_event)
{
    if (not doc or not d_event)
        return;

    /* check to see the device type */
    std::string deviceType = get_first_doc_item(doc, "deviceType");
    if (deviceType.empty()) {
        /* JAMI_DBG("UPnP: could not find deviceType in the description document of the device"); */
        return;
    }

    if (deviceType.compare(UPNP_IGD_DEVICE) == 0) {
        parseIGD(doc, d_event);
    }

    /* TODO: check if its a ring device */
}

void
UPnPContext::parseIGD(IXML_Document* doc, const UpnpDiscovery* d_event)
{
    if (not doc or not d_event)
        return;

    /* check the UDN to see if its already in our device list(s)
     * if it is, then update the device advertisement timeout (expiration)
     */
    std::string UDN = get_first_doc_item(doc, "UDN");
    if (UDN.empty()) {
        JAMI_DBG("UPnP: could not find UDN in description document of device");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);
        auto it = validIGDs_.find(UDN);

        if (it != validIGDs_.end()) {
            /* we already have this device in our list */
            /* TODO: update expiration */
            return;
        }
    }

    std::unique_ptr<UPnPIGD> new_igd;
    int upnp_err;

    std::string friendlyName = get_first_doc_item(doc, "friendlyName");
    if (not friendlyName.empty() )
        JAMI_DBG("UPnP: checking new device of type IGD: '%s'",
                 friendlyName.c_str());

    /* determine baseURL */
    std::string baseURL = get_first_doc_item(doc, "URLBase");
    if (baseURL.empty()) {
        /* get it from the discovery event location */
        baseURL = std::string(UpnpDiscovery_get_Location_cstr(d_event));
    }

    /* check if its a valid IGD:
     *      1. check for IGD device... already done if this function is called
     *      2. check for WAN device... skip checking for this and check for the services directly
     *      3. check for WANIPConnection service or WANPPPConnection service
     *      4. check if connected to Internet (if not, no point in port forwarding)
     *      5. check that we can get the external IP
     */

    /* get list of services defined by serviceType */
    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&> serviceList(nullptr, ixmlNodeList_free);
    serviceList.reset(ixmlDocument_getElementsByTagName(doc, "serviceType"));

    /* get list of all 'serviceType' elements */
    bool found_connected_IGD = false;
    unsigned long list_length = ixmlNodeList_length(serviceList.get());

    /* go through the 'serviceType' nodes until we find the first service of type
     * WANIPConnection or WANPPPConnection which is connected to an external network */
    for (unsigned long node_idx = 0; node_idx < list_length and not found_connected_IGD; node_idx++) {
        IXML_Node* serviceType_node = ixmlNodeList_item(serviceList.get(), node_idx);
        std::string serviceType = get_element_text(serviceType_node);

        /* only check serviceType of WANIPConnection or WANPPPConnection */
        if (serviceType.compare(UPNP_WANIP_SERVICE) == 0
            or serviceType.compare(UPNP_WANPPP_SERVICE) == 0) {

            /* we found a correct 'serviceType', now get the parent node because
             * the rest of the service definitions are siblings of 'serviceType' */
            IXML_Node* service_node = ixmlNode_getParentNode(serviceType_node);
            if (service_node) {
                /* perform sanity check; the parent node should be called "service" */
                if( strcmp(ixmlNode_getNodeName(service_node), "service") == 0) {
                    /* get the rest of the service definitions */

                    /* serviceId */
                    IXML_Element* service_element = (IXML_Element*)service_node;
                    std::string serviceId = get_first_element_item(service_element, "serviceId");

                    /* get the relative controlURL and turn it into absolute address using the URLBase */
                    std::string controlURL = get_first_element_item(service_element, "controlURL");
                    if (not controlURL.empty()) {
                        char* absolute_url = nullptr;
                        upnp_err = UpnpResolveURL2(baseURL.c_str(),
                                                   controlURL.c_str(),
                                                   &absolute_url);
                        if (upnp_err == UPNP_E_SUCCESS)
                            controlURL = absolute_url;
                        else
                            JAMI_WARN("UPnP: error resolving absolute controlURL: %s",
                                      UpnpGetErrorMessage(upnp_err));
                        std::free(absolute_url);
                    }

                    /* get the relative eventSubURL and turn it into absolute address using the URLBase */
                    std::string eventSubURL = get_first_element_item(service_element, "eventSubURL");
                    if (not eventSubURL.empty()) {
                        char* absolute_url = nullptr;
                        upnp_err = UpnpResolveURL2(baseURL.c_str(),
                                                   eventSubURL.c_str(),
                                                   &absolute_url);
                        if (upnp_err == UPNP_E_SUCCESS)
                            eventSubURL = absolute_url;
                        else
                            JAMI_WARN("UPnP: error resolving absolute eventSubURL: %s",
                                      UpnpGetErrorMessage(upnp_err));
                        std::free(absolute_url);
                    }

                    /* make sure all of the services are defined
                     * and check if the IGD is connected to an external network */
                    if (not (serviceId.empty() and controlURL.empty() and eventSubURL.empty()) ) {
                        /* JAMI_DBG("UPnP: got service info from device:\n\tserviceType: %s\n\tserviceID: %s\n\tcontrolURL: %s\n\teventSubURL: %s",
                                 serviceType.c_str(), serviceId.c_str(), controlURL.c_str(), eventSubURL.c_str()); */
                        new_igd.reset(new UPnPIGD(
                            std::move(UDN),
                            std::move(baseURL),
                            std::move(friendlyName),
                            std::move(serviceType),
                            std::move(serviceId),
                            std::move(controlURL),
                            std::move(eventSubURL)));
                        if (isIGDConnected(*new_igd)) {
                            new_igd->publicIp = getExternalIP(*new_igd);
                            if (new_igd->publicIp) {
                                JAMI_DBG("UPnP: got external IP: %s", new_igd->publicIp.toString().c_str());
                                new_igd->localIp = ip_utils::getLocalAddr(pj_AF_INET());
                                if (new_igd->localIp)
                                    found_connected_IGD = true;

                            }
                        }
                    }
                    /* TODO: subscribe to the service to get events, eg: when IP changes */
                } else
                     JAMI_WARN("UPnP: IGD \"serviceType\" parent node is not called \"service\"!");
            } else
                JAMI_WARN("UPnP: IGD \"serviceType\" has no parent node!");
        }
    }

    /* if its a valid IGD, add to list of IGDs (ideally there is only one at a time)
     * subscribe to the WANIPConnection or WANPPPConnection service to receive
     * updates about state changes, eg: new external IP
     */
    if (found_connected_IGD) {
        JAMI_DBG("UPnP: found a valid IGD: %s", new_igd->getBaseURL().c_str());

        {
            std::lock_guard<std::mutex> lock(validIGDMutex_);
            /* delete all RING mappings first */
            removeMappingsByLocalIPAndDescription(*new_igd, Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
            validIGDs_.emplace(UDN, std::move(new_igd));
            validIGDCondVar_.notify_all();
            for (const auto& l : igdListeners_)
                l.second();
        }
    }
}

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

    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlDocument_getElementsByTagName(doc, item), ixmlNodeList_free);
    if (nodeList) {
        /* if there are several nodes which match the tag, we only want the first one */
        ret = get_element_text( ixmlNodeList_item(nodeList.get(), 0) );
    }
    return ret;
}

static std::string
get_first_element_item(IXML_Element* element, const char* item)
{
    std::string ret;

    std::unique_ptr<IXML_NodeList, decltype(ixmlNodeList_free)&>
        nodeList(ixmlElement_getElementsByTagName(element, item), ixmlNodeList_free);
    if (nodeList) {
        /* if there are several nodes which match the tag, we only want the first one */
        ret = get_element_text( ixmlNodeList_item(nodeList.get(), 0) );
    }
    return ret;
}

int
UPnPContext::cp_callback(Upnp_EventType event_type, const void* event, void* user_data)
{
    if (auto upnpContext = static_cast<UPnPContext*>(user_data))
        return upnpContext->handleUPnPEvents(event_type, event);

    JAMI_WARN("UPnP callback without UPnPContext");
    return 0;
}

int
UPnPContext::handleUPnPEvents(Upnp_EventType event_type, const void* event)
{
    switch( event_type )
    {
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        /* JAMI_DBG("UPnP: CP received a discovery advertisement"); */
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery* d_event = ( const UpnpDiscovery* )event;
        std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> desc_doc(nullptr, ixmlDocument_free);
        int upnp_err;

        /* if (event_type != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE)
             JAMI_DBG("UPnP: CP received a discovery search result"); */

        /* check if we are already in the process of checking this device */
        std::unique_lock<std::mutex> lock(cpDeviceMutex_);
        auto it = cpDevices_.find(std::string(UpnpDiscovery_get_Location_cstr(d_event)));

        if (it == cpDevices_.end()) {
            cpDevices_.emplace(std::string(UpnpDiscovery_get_Location_cstr(d_event)));
            lock.unlock();

            if (UpnpDiscovery_get_ErrCode(d_event) != UPNP_E_SUCCESS)
                JAMI_WARN("UPnP: Error in discovery event received by the CP: %s",
                          UpnpGetErrorMessage(UpnpDiscovery_get_ErrCode(d_event)));

            /* JAMI_DBG("UPnP: Control Point received discovery event from device:\n\tid: %s\n\ttype: %s\n\tservice: %s\n\tversion: %s\n\tlocation: %s\n\tOS: %s",
                     d_event->DeviceId, d_event->DeviceType, d_event->ServiceType, d_event->ServiceVer, UpnpDiscovery_get_Location_cstr(d_event), d_event->Os);
            */

            /* note: this thing will block until success for the system socket timeout
             *       unless libupnp is compile with '-disable-blocking-tcp-connections'
             *       in which case it will block for the libupnp specified timeout
             */
            IXML_Document* desc_doc_ptr = nullptr;
            upnp_err = UpnpDownloadXmlDoc( UpnpDiscovery_get_Location_cstr(d_event), &desc_doc_ptr);
            desc_doc.reset(desc_doc_ptr);
            if ( upnp_err != UPNP_E_SUCCESS ) {
                /* the download of the xml doc has failed; this probably happened
                 * because the router has UPnP disabled, but is still sending
                 * UPnP discovery packets
                 *
                 * JAMI_WARN("UPnP: Error downloading device description: %s",
                 *         UpnpGetErrorMessage(upnp_err));
                 */
            } else {
                parseDevice(desc_doc.get(), d_event);
            }

            /* finished parsing device; remove it from know devices list,
             * since next time it could be a new device with same URL
             * eg: if we switch routers or if a new device with the same IP appears
             */
            lock.lock();
            cpDevices_.erase(UpnpDiscovery_get_Location_cstr(d_event));
            lock.unlock();
        } else {
            lock.unlock();
            /* JAMI_DBG("UPnP: Control Point is already checking this device"); */
        }
    }
    break;

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery *d_event = (const UpnpDiscovery *)event;

        JAMI_DBG("UPnP: Control Point received ByeBye for device: %s",
		 UpnpDiscovery_get_DeviceID_cstr(d_event));

        if (UpnpDiscovery_get_ErrCode(d_event) != UPNP_E_SUCCESS)
            JAMI_WARN("UPnP: Error in ByeBye received by the CP: %s",
                      UpnpGetErrorMessage(UpnpDiscovery_get_ErrCode(d_event)));

        /* TODO: check if its a device we care about and remove it from the relevant lists */
    }
    break;

    case UPNP_EVENT_RECEIVED:
    {
        /* struct Upnp_Event *e_event UNUSED = (struct Upnp_Event *)event; */

        /* JAMI_DBG("UPnP: Control Point event received"); */

        /* TODO: handle event by updating any changed state variables */

    }
    break;

    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        JAMI_WARN("UPnP: Control Point subscription auto-renewal failed");
    }
    break;

    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
    {
        JAMI_DBG("UPnP: Control Point subscription expired");
    }
    break;

    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        /* JAMI_DBG("UPnP: Control Point async subscription complete"); */

        /* TODO: check if successful */

        break;

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        /* this event will occur whether or not a valid IGD has been found;
         * it just indicates the search timeout has been reached
         *
         * JAMI_DBG("UPnP: Control Point search timeout");
         */
        break;

    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        const UpnpActionComplete *a_event = (const UpnpActionComplete *)event;

        /* JAMI_DBG("UPnP: Control Point async action complete"); */

        if (UpnpActionComplete_get_ErrCode(a_event) != UPNP_E_SUCCESS)
            JAMI_WARN("UPnP: Error in action complete event: %s",
                      UpnpGetErrorMessage(UpnpActionComplete_get_ErrCode(a_event)));

        /* TODO: no need for any processing here, just print out results.
         * Service state table updates are handled by events. */
    }
    break;

    case UPNP_CONTROL_GET_VAR_COMPLETE:
    {
        const UpnpStateVarComplete *sv_event = (const UpnpStateVarComplete *)event;

        /* JAMI_DBG("UPnP: Control Point async get variable complete"); */

        if (UpnpStateVarComplete_get_ErrCode(sv_event) != UPNP_E_SUCCESS)
            JAMI_WARN("UPnP: Error in get variable complete event: %s",
                      UpnpGetErrorMessage(UpnpStateVarComplete_get_ErrCode(sv_event)));

        /* TODO: update state variables */
    }
    break;

    default:
        JAMI_WARN("UPnP: unhandled Control Point event");
        break;
    }

    return UPNP_E_SUCCESS; /* return value currently ignored by SDK */
}

static void
checkResponseError(IXML_Document* doc)
{
    if (not doc)
        return;

    std::string errorCode = get_first_doc_item(doc, "errorCode");
    if (not errorCode.empty()) {
        std::string errorDescription = get_first_doc_item(doc, "errorDescription");
        JAMI_WARN("UPnP: response contains error: %s : %s",
                  errorCode.c_str(), errorDescription.c_str());
    }
}

bool
UPnPContext::isIGDConnected(const UPnPIGD& igd)
{
    bool connected = false;
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    action.reset(UpnpMakeAction("GetStatusInfo", igd.getServiceType().c_str(), 0, nullptr));

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    checkResponseError(response.get());
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        JAMI_WARN("UPnP: Failed to get GetStatusInfo from: %s, %d: %s",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));

        return false;
    }

    /* parse response */
    std::string status = get_first_doc_item(response.get(), "NewConnectionStatus");
    if (status.compare("Connected") == 0)
        connected = true;

    /* response should also contain the following elements, but we don't care for now:
     *  "NewLastConnectionError"
     *  "NewUptime"
     */
    return connected;
}

IpAddr
UPnPContext::getExternalIP(const UPnPIGD& igd)
{
    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> action(nullptr, ixmlDocument_free);
    action.reset(UpnpMakeAction("GetExternalIPAddress", igd.getServiceType().c_str(), 0, nullptr));

    std::unique_ptr<IXML_Document, decltype(ixmlDocument_free)&> response(nullptr, ixmlDocument_free);
    IXML_Document* response_ptr = nullptr;
    int upnp_err = UpnpSendAction(ctrlptHandle_, igd.getControlURL().c_str(),
                                  igd.getServiceType().c_str(), nullptr, action.get(), &response_ptr);
    response.reset(response_ptr);
    checkResponseError(response.get());
    if( upnp_err != UPNP_E_SUCCESS) {
        /* TODO: if failed, should we chck if the igd is disconnected? */
        JAMI_WARN("UPnP: Failed to get GetExternalIPAddress from: %s, %d: %s",
                  igd.getServiceType().c_str(), upnp_err, UpnpGetErrorMessage(upnp_err));
        return {};
    }

    /* parse response */
    return {get_first_doc_item(response.get(), "NewExternalIPAddress")};
}

void
UPnPContext::removeMappingsByLocalIPAndDescription(const UPnPIGD& igd, const std::string& description)
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
                        "NewPortMappingIndex", std::to_string(entry_idx).c_str());
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
                if (not deletePortMapping(igd, port_external, protocol)) {
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
UPnPContext::deletePortMapping(const UPnPIGD& igd, const std::string& port_external, const std::string& protocol)
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
UPnPContext::addPortMapping(const UPnPIGD& igd, const Mapping& mapping, int* error_code)
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
