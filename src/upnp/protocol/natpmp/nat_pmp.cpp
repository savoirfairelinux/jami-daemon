/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

std::shared_ptr<PMPIGD>
getPmpIgd()
{
    static auto pmpIgd = std::make_shared<PMPIGD>();
    return pmpIgd;
}

NatPmp::NatPmp()
    : pmpThread_([this]() {
        auto pmp_igd = getPmpIgd();
        natpmp_t natpmp;

        // The following while loop get's called only once upon instantiation.
        while (pmpRun_) {
            if (initnatpmp(&natpmp, 0, 0) < 0) {
                JAMI_ERR("NAT-PMP: Can't initialize libnatpmp");
                std::unique_lock<std::mutex> lk(pmpMutex_);
                pmpCv_.wait_for(lk, std::chrono::minutes(1));
            } else {
                JAMI_DBG("NAT-PMP: Initialized");
                break;
            }
        }

        while (pmpRun_) {
            std::unique_lock<std::mutex> lk(pmpMutex_);
            pmpCv_.wait_until(lk, pmp_igd->getRenewalTime(), [&] {
                return not pmpRun_ or pmp_igd->getRenewalTime() <= clock::now() or not pmp_igd->mapToRemoveList_.empty();
            });

            // Exit thread if pmpRun_ was set to false. Signal program exit.
            if (not pmpRun_) break;

            // Update clock;
            auto now = clock::now();

            // Check if we need to update IGD.
            if (pmp_igd->renewal_ < now) {
                searchForIGD(pmp_igd, natpmp);
            }

            if (pmp_igd) {
                if (pmp_igd->clearAll_) {
                    // Clear all the mappings.
                    deleteAllPortMappings(*pmp_igd, natpmp, NATPMP_PROTOCOL_UDP);
                    deleteAllPortMappings(*pmp_igd, natpmp, NATPMP_PROTOCOL_TCP);
                    pmp_igd->mapToRemoveList_.clear();
                    pmp_igd->clearAll_ = false;
                    
                } else if (not pmp_igd->mapToRemoveList_.empty()) {
                    // Remove mappings to be removed.
                    decltype(pmp_igd->mapToRemoveList_) removed = std::move(pmp_igd->mapToRemoveList_);
                    lk.unlock();
                    for (auto& m : removed) {
                        addPortMapping(*pmp_igd, natpmp, m, false, true);
                    }
                    lk.lock();
                } else if (not pmp_igd->mapToAddList_.empty()) {
                    decltype(pmp_igd->mapToAddList_) add = std::move(pmp_igd->mapToAddList_);
                    lk.unlock();
                    for (auto& m : add) {
                        addPortMapping(*pmp_igd, natpmp, m, false);
                    }
                    lk.lock();
                }
                
                // Add mappings who's renewal times are up.
                decltype(pmp_igd->mapToRenewList_) renew = std::move(pmp_igd->mapToRenewList_);
                lk.unlock();
                for (auto& m : renew) {
                    if (pmp_igd->isMapUpForRenewal(Mapping(m.getPortExternal(), m.getPortInternal(), m.getType()), now)) {
                        addPortMapping(*pmp_igd, natpmp, m, true);
                    }
                }
                lk.lock();
            }
        }
        closenatpmp(&natpmp);
    })
{
}

NatPmp::~NatPmp()
{
    {
        std::lock_guard<std::mutex> lock(validIgdMutex_);
        if (auto pmpIGD_ = getPmpIgd()) {
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
    getPmpIgd().reset();
}

void
NatPmp::clearIgds()
{
    // Lock valid IGD.
    std::lock_guard<std::mutex> lock(validIgdMutex_);

    // Clear internal IGD (nat pmp only supports one).
    if (auto pmpIGD_ = getPmpIgd()) {
        std::lock_guard<std::mutex> lk(pmpMutex_);
        pmpIGD_->publicIp_ = {};
        pmpIGD_->localIp_ = {};
        pmpIGD_->clearMappings();
    }
}

void
NatPmp::searchForIgd()
{
    // Lock valid IGD.
    std::lock_guard<std::mutex> lock(validIgdMutex_);
    if (auto pmpIGD_ = getPmpIgd()) {
        std::lock_guard<std::mutex> lk(pmpMutex_);
        pmpIGD_->renewal_ = clock::now();
    }
    pmpCv_.notify_all();
}

void
NatPmp::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type)
{
    Mapping mapping {port_external, port_internal, type};

    std::lock_guard<std::mutex> lk(validIgdMutex_);

    if (not igd->isMapInUse(Mapping(port_external, port_internal, type))) {
        if (auto pmp_igd = dynamic_cast<PMPIGD*>(igd)) {
            JAMI_DBG("NAT-PMP: Attempting to open port %s", mapping.toString().c_str());
            pmp_igd->addMapToAdd(Mapping(port_external, port_internal, type));
            pmpCv_.notify_all();
        }
    } else {
        igd->incrementNbOfUsers(Mapping(port_external, port_internal, type));
    }
}

void
NatPmp::removeMapping(const Mapping& igdMapping)
{
    std::lock_guard<std::mutex> lock(validIgdMutex_);

    if (auto pmpIGD_ = getPmpIgd()) {
        JAMI_DBG("NAT-PMP: Attempting to close port %s", igdMapping.toString().c_str());
        pmpIGD_->addMapToRemove(Mapping(igdMapping.getPortExternal(), igdMapping.getPortInternal(), igdMapping.getType()));
        pmpCv_.notify_all(); 
    } else {
        JAMI_WARN("NAT-PMP: no valid IGD available");
    }
}

void
NatPmp::searchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp)
{
    if (sendpublicaddressrequest(&natpmp) < 0) {
        JAMI_ERR("NAT-PMP: Can't send request");
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
        } else if (r != NATPMP_TRYAGAIN) {
            pmp_igd->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmp_igd->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            JAMI_DBG("NAT-PMP: Found device with external IP %s", pmp_igd->publicIp_.toString().c_str());
            {
                // Store public Ip address.
                std::string publicIpStr(std::move(pmp_igd.get()->publicIp_.toString()));

                // Add the igd to the upnp context class list.
                if (updateIgdListCb_(this, std::move(pmp_igd.get()), std::move(pmp_igd.get()->publicIp_), true)) {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s was added to the list", publicIpStr.c_str());
                } else {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s is already in the list", publicIpStr.c_str());
                }
            }
            pmp_igd->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
NatPmp::addPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, Mapping& mapping, bool renew, bool remove) const
{
    auto pmpIGD_ = getPmpIgd();

    if (sendnewportmappingrequest(&natpmp,
                                  mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                  mapping.getPortInternal(),
                                  mapping.getPortExternal(), remove ? 0 : 3600) < 0) {
        JAMI_ERR("NAT-PMP: Can't send port mapping request");
        Mapping* map = new Mapping(std::move(mapping.getPortExternal()),
                                       std::move(mapping.getPortInternal()),
                                       std::move(mapping.getType() == PortType::UDP ? 
                                       upnp::PortType::UDP : upnp::PortType::TCP));
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIGD_) {
            pmpIGD_->removeMapToRemove(Mapping(mapping.getPortExternal(), mapping.getPortInternal(), mapping.getType()));
            notifyContextPortCloseCb_(pmpIGD_->publicIp_, map, false);
        }
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't %sregister port mapping", remove ? "un" : "");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            Mapping* map = new Mapping(std::move(mapping.getPortExternal()),
                                       std::move(mapping.getPortInternal()),
                                       std::move(mapping.getType() == PortType::UDP ? 
                                       upnp::PortType::UDP : upnp::PortType::TCP));
            if (pmpIGD_) {
                if (remove) {
                JAMI_WARN("NAT-PMP: Closed port %d:%d %s", mapping.getPortInternal(),
                                                            mapping.getPortExternal(),
                                                            mapping.getType() == PortType::UDP ? "UDP" : "TCP");
                    pmpIGD_->removeMapToRemove(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                    pmpIGD_->removeMapToRenew(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                    notifyContextPortCloseCb_(pmpIGD_->publicIp_, map, true);
                } else {
                    if (not renew) {
                        JAMI_WARN("NAT-PMP: Opened port %d:%d %s", mapping.getPortInternal(),
                                                                mapping.getPortExternal(),
                                                                mapping.getType() == PortType::UDP ? "UDP" : "TCP");
                        pmpIGD_->removeMapToAdd(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                        pmpIGD_->addMapToRenew(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                        notifyContextPortOpenCb_(pmpIGD_->publicIp_, map, true);
                    } else {
                        JAMI_WARN("NAT-PMP: Renewed port %d:%d %s", mapping.getPortInternal(),
                                                                    mapping.getPortExternal(),
                                                                    mapping.getType() == PortType::UDP ? "UDP" : "TCP");
                    }
                }
            }
            break;
        }
    }
}

void
NatPmp::removeAllLocalMappings(IGD* /*igd*/)
{
    auto pmpIGD_ = getPmpIgd();
    pmpIGD_->clearAll_ = true;
    pmpCv_.notify_all();
}

void
NatPmp::deleteAllPortMappings(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, int proto) const
{
    if (sendnewportmappingrequest(&natpmp, proto, 0, 0, 0) < 0) {
        JAMI_ERR("NAT-PMP: Can't send all port mapping removal request");
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't remove all port mappings");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            break;
        }
    }
}


}} // namespace jami::upnp