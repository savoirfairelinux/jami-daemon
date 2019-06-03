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

#include "nat_pmp.h"

namespace jami { namespace upnp {

NatPmp::NatPmp()
    : pmpThread_([this]() {
        auto pmp_igd = std::make_shared<PMPIGD>();
        natpmp_t natpmp;

        while (pmpRun_) {
            if (initnatpmp(&natpmp, 0, 0) < 0) {
                JAMI_ERR("NAT-PMP: Can't initialize libnatpmp.");
                std::unique_lock<std::mutex> lk(pmpMutex_);
                pmpCv_.wait_for(lk, std::chrono::minutes(1));
            } else {
                JAMI_DBG("NAT-PMP: Initialized.");
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
    })
{

}

NatPmp::~NatPmp()
{
    {
        std::lock_guard<std::mutex> lock(validIGDMutex_);

        if (pmpIGD_) {
            {
                std::lock_guard<std::mutex> lk(pmpMutex_);
                pmpIGD_->clearMappings();
            }
            pmpCv_.notify_all();
        }
    }
    
    pmpRun_ = false;
    pmpCv_.notify_all();
    if (pmpThread_.joinable()) {
        pmpThread_.join();
    }
    pmpIGD_.reset();
}

void
NatPmp::connectivityChanged()
{
    {
        // Lock valid IGD.
        std::lock_guard<std::mutex> lock(validIGDMutex_);

        // Clear internal IGD (nat pmp only supports one).
        if (pmpIGD_) {
            std::lock_guard<std::mutex> lk(pmpMutex_);
            pmpIGD_->clear();
        }

        // Notify.
        pmpCv_.notify_all();
        validIGDCondVar_.notify_all();
    }

}

void 
NatPmp::searchForIGD()
{
    pmpRun_ = true;
    
    // Clear internal IGD (nat pmp only supports one).
    if (pmpIGD_) {
        std::lock_guard<std::mutex> lk(pmpMutex_);
        pmpIGD_->renewal_ = clock::now();
        pmpIGD_.reset();
    }
    pmpCv_.notify_all();
}

Mapping
NatPmp::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error)
{
    upnp_error = UPnPProtocol::UpnpError::INVALID_ERR;

    Mapping mapping {port_external, port_internal, type};

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
            upnp_error = UPnPProtocol::UpnpError::ERROR_OK;
            ++(mapping_ptr->users);
            JAMI_DBG("NAT-PMP: Mapping already exists, incrementing number of users: %d.",
                     iter->second.users);
            return mapping;
        } else {
            /* this port is already used by a different mapping */
            JAMI_WARN("NAT-PMP: Cannot add a mapping with an external port which is already used by another:\n\tcurrent: %s\n\ttrying to add: %s",
                      mapping_ptr->toString().c_str(), mapping.toString().c_str());
            upnp_error = UPnPProtocol::UpnpError::CONFLICT_IN_MAPPING;
            return {};
        }
    }

    {
        /* success; add it to global list */
        globalMappings->emplace(port_external, GlobalMapping{mapping});

        pmpCv_.notify_all();
        return mapping;
    }
    return {};
}

void
NatPmp::removeAllLocalMappings(IGD* igd)
{

}

void
NatPmp::removeMapping(const Mapping& mapping)
{

    std::lock_guard<std::mutex> lock(validIGDMutex_);

    if (not pmpIGD_) {
        JAMI_WARN("NAT-PMP: no valid IGD available");
        return;
    }

    /* first make sure the mapping exists in the global list of the igd */
    auto globalMappings = mapping.getType() == PortType::UDP ?
                          &pmpIGD_->udpMappings : &pmpIGD_->tcpMappings;

    auto iter = globalMappings->find(mapping.getPortExternal());
    if (iter != globalMappings->end()) {
        /* make sure its the same mapping */
        GlobalMapping& global_mapping = iter->second;
        if (mapping == global_mapping ) {
            /* now check the users */
            if (global_mapping.users > 1) {
                /* more than one user, simply decrement the number */
                --(global_mapping.users);
                JAMI_DBG("NAT-PMP: Decrementing users of mapping: %s, %d users remaining.",
                         mapping.toString().c_str(), global_mapping.users);
            } else {
                {
                    std::lock_guard<std::mutex> lk(pmpMutex_);
                    pmpIGD_->toRemove_.emplace_back(std::move(global_mapping));
                }
                pmpCv_.notify_all();
                globalMappings->erase(iter);
            }
        } else {
            JAMI_WARN("NAT-PMP: Cannot remove mapping which doesn't match the existing one in the IGD list.");
        }
    } else {
        JAMI_WARN("NAT-PMP: Cannot remove mapping which is not in the list of existing mappings of the IGD.");
    }
}

void
NatPmp::PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp)
{
    if (sendpublicaddressrequest(&natpmp) < 0) {
        JAMI_ERR("NAT-PMP: Can't send request.");
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
            pmp_igd->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmp_igd->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            if (not pmpIGD_) {
                JAMI_DBG("NAT-PMP: Found device with external IP %s.", pmp_igd->publicIp_.toString().c_str());
                {
                    // Store public Ip address.
                    std::string publicIpStr(std::move(pmp_igd.get()->publicIp_.toString()));

                    // Add the igd to the upnp context class list.
                    if (updateIgdListCb_(this, std::move(pmp_igd.get()), std::move(pmp_igd.get()->publicIp_), true)) {
                        JAMI_WARN("NAT-PMP: IGD with public IP %s was added to the list.", publicIpStr.c_str());
                    } else {
                        JAMI_WARN("NAT-PMP: IGD with public IP %s is already in the list.", publicIpStr.c_str());
                    }

                    // Keep IGD internally.
                    std::lock_guard<std::mutex> lock(validIGDMutex_);
                    pmpIGD_ = pmp_igd;

                    // Notify.
                    validIGDCondVar_.notify_all();
                }
            }
            pmp_igd->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
NatPmp::PMPaddPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, GlobalMapping& mapping, bool remove) const
{
    if (sendnewportmappingrequest(&natpmp,
                                  mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                  mapping.getPortInternal(),
                                  mapping.getPortExternal(), remove ? 0 : 3600) < 0) {
        JAMI_ERR("NAT-PMP: Can't send port mapping request.");
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't %sregister port mapping.", remove ? "un" : "");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            if (remove) {
            JAMI_WARN("NAT-PMP: Closed port %d:%d %s.", mapping.getPortInternal(),
                                                         mapping.getPortExternal(),
                                                         mapping.getType() == PortType::UDP ? "UDP" : "TCP");
            } else {
                JAMI_WARN("NAT-PMP: Opened port %d:%d %s.", mapping.getPortInternal(),
                                                            mapping.getPortExternal(),
                                                            mapping.getType() == PortType::UDP ? "UDP" : "TCP");
            }
            break;
        }
    }
}

void
NatPmp::PMPdeleteAllPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, int proto) const
{
    if (sendnewportmappingrequest(&natpmp, proto, 0, 0, 0) < 0) {
        JAMI_ERR("NAT-PMP: Can't send all port mapping removal request.");
        return;
    }
    
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't remove all port mappings.");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            break;
        }
    }
}


}} // namespace jami::upnp