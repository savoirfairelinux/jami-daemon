/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "upnp_context.h"

#include <string>
#include <set>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cstdlib> // for std::free

#include "logger.h"
#include "ip_utils.h"
#include "upnp_igd.h"
#include "intrin.h"

#if HAVE_DHT
#include <opendht/rng.h>
using random_device = dht::crypto::random_device;
#else
using random_device = std::random_device;
#endif

namespace ring { namespace upnp {

/**
 * This should be used to get a UPnPContext.
 * It only makes sense to have one unless you have separate
 * contexts for multiple internet interfaces, which is not currently
 * supported.
 */
std::shared_ptr<NATPMPContext>
getNATPMPContext()
{
    static auto context = std::make_shared<NATPMPContext>();
    return context;
}

#if HAVE_LIBUPNP

/* max number of times to retry mapping if it fails due to conflict;
 * there isn't much logic in picking this number... ideally not many ports should
 * be mapped in a system, so a few number of random port retries should work;
 * a high number of retries would indicate there might be some kind of bug or else
 * incompatibility with the router; we use it to prevent an infinite loop of
 * retrying to map the entry
 */
constexpr static unsigned MAX_RETRIES = 20;

NATPMPContext::NATPMPContext()
{
    if (initnatpmp(&natpmp, 0, 0) < 0)
        throw std::runtime_exception("can't initialize natpmp");
    t_ = [](){
        std::chrono::seconds timeout;
        std::unique_lock<std::mutex> lock(stateMutex_);
        stateCV_.wait_for(lock, timeout);
        if (natpmpstate == Sstop)
            return;
        if (natpmpstate == Srecvpub) {
            r = readnatpmpresponseorretry(&natpmp, &response);
            if (r < 0 && r != NATPMP_TRYAGAIN)
                natpmpstate = Serror;
            else if (r != NATPMP_TRYAGAIN) {
                //t3 = high_resolution_clock::now();
                //memcpy(&public_addr, &response.pnu.publicaddress.addr, sizeof(public_addr));
                public_addr = response.pnu.publicaddress.addr;
                //inet_ntop(AF_INET, &public_addr, addr_str, INET_ADDRSTRLEN);
                printf("Ssendmap, got public address %s\n", public_addr.toString().c_str());
                clientRegistered_ = true;
                {
                    std::lock_guard<std::mutex> lock(validIGDMutex_);
                    /* delete all RING mappings first */
                    //removeMappingsByLocalIPAndDescription(new_igd.get(), Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION);
                    //validIGDs_.emplace(UDN, std::move(new_igd));
                    validIGDCondVar_.notify_all();
                    for (const auto& l : igdListeners_)
                        l.second();
                }

                natpmpstate = Sidle;
            } else {
                timeout = std::chrono::milliseconds(2);
            }
            break;
        } else if (natpmpstate == Srecvmap) {
            r = readnatpmpresponseorretry(&natpmp, &response);
            if (r<0 && r!=NATPMP_TRYAGAIN)
                natpmpstate = Serror;
            else if (r != NATPMP_TRYAGAIN) {
                //t4 = high_resolution_clock::now();
                pub_port = response.pnu.newportmapping.mappedpublicport;
                priv_port = response.pnu.newportmapping.privateport;
                natpmpstate = Sidle;
            } else {
                timeout = std::chrono::milliseconds(2);
            }
        } else if (natpmpstate == Sidle) {
            timeout = std::chrono::milliseconds(10000);
        }
    };
    searchForIGD();
}

NATPMPContext::~NATPMPContext()
{
    natpmpstate = Sstop;
    stateCV_.notify_all();
    t_.join();
    closenatpmp(&natpmp);
}

void
NATPMPContext::searchForIGD()
{
    /*if (clientRegistered_) {
        RING_WARN("UPnP: Control Point not registered");
        return;
    }*/

    if (natpmpstate == Srecvpub || natpmpstate == Srecvmap)
        return;

    if (sendpublicaddressrequest(&natpmp) < 0)
        throw std::runtime_exception("can't send natpmp request");

    natpmpstate = Srecvpub;
}

bool
NATPMPContext::hasValidIGD(std::chrono::seconds timeout)
{
    if (clientRegistered_) {
        return true;
    }

    std::unique_lock<std::mutex> lock(validIGDMutex_);
    if (!validIGDCondVar_.wait_for(lock, timeout,
                                   [this]{return clientRegistered_;})) {
        RING_WARN("UPnP: check for valid IGD timeout");
        return false;
    }

    return clientRegistered_;
}

size_t
NATPMPContext::addIGDListener(IGDFoundCallback&& cb)
{
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto token = ++listenerToken_;
    igdListeners_.emplace(token, std::move(cb));
    return token;
}

void
NATPMPContext::removeIGDListener(size_t token)
{
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    auto it = igdListeners_.find(token);
    if (it != igdListeners_.end())
        igdListeners_.erase(it);
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

    return dist(gen);;
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
NATPMPContext::addAnyMapping(uint16_t port_desired,
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
        RING_WARN("UPnP: no valid IGD available");
        return {};
    }

    auto globalMappings = type == PortType::UDP ?
                          &igd->udpMappings : &igd->tcpMappings;
    if (unique) {
        /* check that port is not already used by the client */
        auto iter = globalMappings->find(port_desired);
        if (iter != globalMappings->end()) {
            /* port already used, we need a unique port */
            port_desired = chooseRandomPort(igd, type);
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
        RING_DBG("UPnP: mapping failed (conflicting entry? err = %d), trying with a different port.",
                 upnp_error);
        /* TODO: make sure we don't try sellecting the same random port twice if it fails ? */
        port_desired = chooseRandomPort(igd, type);
        if (use_same_port)
            port_local = port_desired;
        mapping = addMapping(igd, port_desired, port_local, type, &upnp_error);
        ++numberRetries;
    }

    if (not mapping and numberRetries == MAX_RETRIES)
        RING_DBG("UPnP: could not add mapping after %u retries, giving up", MAX_RETRIES);

    return mapping;
}

/**
 * tries to remove the given mapping
 */
void
NATPMPContext::removeMapping(const Mapping& mapping)
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);
    IGD* igd = chooseIGD_unlocked();
    if (not igd) {
        RING_WARN("UPnP: no valid IGD available");
        return;
    }

    /* first make sure the mapping exists in the global list of the igd */
    auto globalMappings = mapping.getType() == PortType::UDP ?
                          &igd->udpMappings : &igd->tcpMappings;

    auto iter = globalMappings->find(mapping.getPortExternal());
    if ( iter != globalMappings->end() ) {
        /* make sure its the same mapping */
        GlobalMapping *global_mapping = &iter->second;
        if (mapping == *global_mapping ) {
            /* now check the users */
            if (global_mapping->users > 1) {
                /* more than one user, simply decrement the number */
                --(global_mapping->users);
                RING_DBG("UPnP: decrementing users of mapping: %s, %d users remaining",
                         mapping.toString().c_str(), global_mapping->users);
            } else {
                /* no other users, can delete */
                RING_DBG("UPnP: removing port mapping : %s",
                         mapping.toString().c_str());
                deletePortMapping(igd,
                                  mapping.getPortExternalStr(),
                                  mapping.getTypeStr());
                globalMappings->erase(iter);
            }
        } else {
            RING_WARN("UPnP: cannot remove mapping which doesn't match the existing one in the IGD list");
        }
    } else {
        RING_WARN("UPnP: cannot remove mapping which is not in the list of existing mappings of the IGD");
    }
}

IpAddr
NATPMPContext::getLocalIP() const
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);

    /* if its a valid igd, we must have already gotten the local ip */
    /*if (auto igd = chooseIGD_unlocked())
        return igd->localIp;*/
    RING_WARN("UPnP: no valid IGD available");
    return {};
}

IpAddr
UPnPContext::getExternalIP() const
{
    /* get a lock on the igd list because we don't want the igd to be modified
     * or removed from the list while using it */
    std::lock_guard<std::mutex> lock(validIGDMutex_);

    /* if its a valid igd, we must have already gotten the external ip */

    if (clientRegistered_)
        return public_addr;


    RING_WARN("UPnP: no valid IGD available");
    return {};
}

#endif /* HAVE_LIBUPNP */

}} // namespace ring::upnp
