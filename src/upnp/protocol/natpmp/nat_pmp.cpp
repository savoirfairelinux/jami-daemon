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

NatPmp::NatPmp()
    : pmpThread_([this]() {
        auto pmp_igd = std::make_shared<PMPIGD>();
        natpmp_t natpmp;

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
                return not pmpRun_ or pmp_igd->getRenewalTime() <= clock::now();
            });
            if (not pmpRun_) break;

            auto now = clock::now();

            if (pmp_igd->renewal_ < now) {
                searchForIGD(pmp_igd, natpmp);
            }
            if (pmpIGD_) {
                if (pmp_igd->clearAll_) {
                    deleteAllPortMappings(*pmp_igd, natpmp, NATPMP_PROTOCOL_UDP);
                    deleteAllPortMappings(*pmp_igd, natpmp, NATPMP_PROTOCOL_TCP);
                    pmp_igd->clearAll_ = false;
                    pmp_igd->toRemove_.clear();
                } else if (not pmp_igd->toRemove_.empty()) {
                    decltype(pmp_igd->toRemove_) removed = std::move(pmp_igd->toRemove_);
                    pmp_igd->toRemove_.clear();
                    lk.unlock();
                    for (auto& m : removed) {
                        addPortMapping(*pmp_igd, natpmp, m, true);
                    }
                    lk.lock();
                }
                auto mapping = pmp_igd->getNextMappingToRenew();
                if (mapping and mapping->renewal_ < now)
                    addPortMapping(*pmp_igd, natpmp, *mapping);
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
NatPmp::clearIgds()
{
    // Lock valid IGD.
    std::lock_guard<std::mutex> lock(validIgdMutex_);

    // Clear internal IGD (nat pmp only supports one).
    if (pmpIGD_) {
        std::lock_guard<std::mutex> lk(pmpMutex_);
        pmpIGD_->renewal_ = clock::now();
        pmpIGD_.reset();
    }

    pmpCv_.notify_all();
}

void
NatPmp::searchForIgd()
{
    // Lock valid IGD.
    std::lock_guard<std::mutex> lock(validIgdMutex_);
    if (pmpIGD_) {
        // Clear internal IGD (nat pmp only supports one).
        std::lock_guard<std::mutex> lk(pmpMutex_);
        pmpIGD_->renewal_ = clock::now();
    }
    pmpCv_.notify_all();
}

void
NatPmp::addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error)
{
    upnp_error = UPnPProtocol::UpnpError::INVALID_ERR;

    Mapping mapping {port_external, port_internal, type};
    auto globalMappings = type == PortType::UDP ? &igd->udpMappings_ : &igd->tcpMappings_;
    {
        // Add mapping to list and notify thread.
        JAMI_DBG("NAT-PMP: Attempting to open port %s", mapping.toString().c_str());
        globalMappings->emplace(port_external, GlobalMapping{mapping});
        pmpCv_.notify_all();
    }
}

void
NatPmp::removeAllLocalMappings(IGD* /*igd*/)
{
    if (pmpIGD_) {
        pmpIGD_->clearAll_ = true;
        pmpCv_.notify_all();
    }
}

void
NatPmp::removeMapping(const Mapping& igdMapping)
{
    std::lock_guard<std::mutex> lock(validIgdMutex_);

    if (not pmpIGD_) {
        JAMI_WARN("NAT-PMP: no valid IGD available");
        return;
    }

    // First make sure the mapping exists in the global list of the igd.
    auto globalMappings = igdMapping.getType() == PortType::UDP ?
                          &pmpIGD_->udpMappings_ : &pmpIGD_->tcpMappings_;

    auto iter = globalMappings->find(igdMapping.getPortExternal());
    if (iter != globalMappings->end()) {
        // Make sure it's the same mapping.
        GlobalMapping& global_mapping = iter->second;
        if (igdMapping == global_mapping) {
            // Place the mapping in the toRemove list and notify the thread.
            {
                std::lock_guard<std::mutex> lk(pmpMutex_);
                JAMI_DBG("NAT-PMP: Attempting to close port %s", global_mapping.toString().c_str());
                pmpIGD_->toRemove_.emplace_back(std::move(global_mapping));
            }
            pmpCv_.notify_all();
            globalMappings->erase(iter);
        } else {
            JAMI_WARN("NAT-PMP: Cannot remove mapping which doesn't match the existing one in the IGD list");
        }
    } else {
        JAMI_WARN("NAT-PMP: Cannot remove mapping which is not in the list of existing mappings of the IGD");
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
            if (not pmpIGD_) {
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

                    // Keep IGD internally.
                    std::lock_guard<std::mutex> lock(validIgdMutex_);
                    pmpIGD_ = pmp_igd;
                }
            }
            pmp_igd->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
NatPmp::addPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, GlobalMapping& mapping, bool remove) const
{
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
        notifyContextPortCloseCb_(map, false);
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
            if (remove) {
            JAMI_WARN("NAT-PMP: Closed port %d:%d %s", mapping.getPortInternal(),
                                                         mapping.getPortExternal(),
                                                         mapping.getType() == PortType::UDP ? "UDP" : "TCP");
                notifyContextPortCloseCb_(map, true);
            } else {
                JAMI_WARN("NAT-PMP: Opened port %d:%d %s", mapping.getPortInternal(),
                                                           mapping.getPortExternal(),
                                                           mapping.getType() == PortType::UDP ? "UDP" : "TCP");
                
                notifyContextPortOpenCb_(map, true);
            }
            break;
        }
    }
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