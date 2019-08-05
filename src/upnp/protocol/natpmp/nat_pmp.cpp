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
                        JAMI_DBG("NAT-PMP: Sent request to close port %s", m.toString().c_str());
                        removePortMapping(*pmp_igd, natpmp, m);
                    }
                    lk.lock();
                } else if (not pmp_igd->mapToAddList_.empty()) {
                    decltype(pmp_igd->mapToAddList_) add = std::move(pmp_igd->mapToAddList_);
                    lk.unlock();
                    for (auto& m : add) {
                        JAMI_DBG("NAT-PMP: Sent request to open port %s", m.toString().c_str());
                        addPortMapping(*pmp_igd, natpmp, m, false);
                    }
                    lk.lock();
                }
                
                // Add mappings who's renewal times are up.
                decltype(pmp_igd->mapToRenewList_) renew = std::move(pmp_igd->mapToRenewList_);
                lk.unlock();
                for (auto& m : renew) {
                    if (pmp_igd->isMapUpForRenewal(Mapping(m.getPortExternal(), m.getPortInternal(), m.getType()), now)) {
                        JAMI_DBG("NAT-PMP: Sent request to renew port %s", m.toString().c_str());
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
        getPmpIgd().reset();
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
NatPmp::requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);
    
    Mapping mapping {port_external, port_internal, type};

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
NatPmp::requestMappingRemove(const Mapping& igdMapping)
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
    int err = sendpublicaddressrequest(&natpmp);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send search request -> %s", getNatPmpErrorStr(err).c_str());
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
NatPmp::addPortMapping(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, Mapping& mapping, bool renew)
{
    auto pmpIGD_ = getPmpIgd();
    int err = sendnewportmappingrequest(&natpmp,
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                        mapping.getPortInternal(),
                                        mapping.getPortExternal(),
                                        3600);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send open port request -> %s", getNatPmpErrorStr(err).c_str());
        Mapping* map = new Mapping(std::move(mapping.getPortExternal()),
                                       std::move(mapping.getPortInternal()),
                                       std::move(mapping.getType() == PortType::UDP ? 
                                       upnp::PortType::UDP : upnp::PortType::TCP));
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIGD_) {
            pmpIGD_->removeMapToAdd(Mapping(mapping.getPortExternal(), mapping.getPortInternal(), mapping.getType()));
            notifyContextPortOpenCb_(pmpIGD_->publicIp_, map, false);
        }
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmp, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't register port mapping %s", mapping.toString().c_str());
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            Mapping* map = new Mapping(std::move(mapping.getPortExternal()),
                                       std::move(mapping.getPortInternal()),
                                       std::move(mapping.getType() == PortType::UDP ? 
                                       upnp::PortType::UDP : upnp::PortType::TCP));
            if (pmpIGD_) {
                if (not renew) {
                    JAMI_WARN("NAT-PMP: Opened port %s", mapping.toString().c_str());
                    pmpIGD_->removeMapToAdd(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                    pmpIGD_->addMapToRenew(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                    notifyContextPortOpenCb_(pmpIGD_->publicIp_, map, true);
                } else {
                    JAMI_WARN("NAT-PMP: Renewed port %s", mapping.toString().c_str());
                }
            }
            break;
        }
    }
}

void
NatPmp::removePortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, Mapping& mapping)
{
    auto pmpIGD_ = getPmpIgd();
    int err = sendnewportmappingrequest(&natpmp,
                                  mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                  mapping.getPortInternal(),
                                  mapping.getPortExternal(), 
                                  0); 
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send close port request -> %s", getNatPmpErrorStr(err).c_str());
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
            JAMI_ERR("NAT-PMP: Can't unregister port mapping %s", mapping.toString().c_str());
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            Mapping* map = new Mapping(std::move(mapping.getPortExternal()),
                                       std::move(mapping.getPortInternal()),
                                       std::move(mapping.getType() == PortType::UDP ? 
                                       upnp::PortType::UDP : upnp::PortType::TCP));
            if (pmpIGD_) {
                JAMI_WARN("NAT-PMP: Closed port %s", mapping.toString().c_str());
                pmpIGD_->removeMapToRemove(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                pmpIGD_->removeMapToRenew(Mapping(map->getPortExternal(), map->getPortInternal(), map->getType()));
                notifyContextPortCloseCb_(pmpIGD_->publicIp_, map, true);
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
NatPmp::deleteAllPortMappings(const PMPIGD& /*pmp_igd*/, natpmp_t& natpmp, int proto)
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

std::string
NatPmp::getNatPmpErrorStr(int errorCode)
{
    std::string errorStr {};
    
    switch(errorCode) {
    case NATPMP_ERR_INVALIDARGS: errorStr = "INVALIDARGS"; break;
    case NATPMP_ERR_SOCKETERROR: errorStr = "SOCKETERROR"; break;
    case NATPMP_ERR_CANNOTGETGATEWAY: errorStr = "CANNOTGETGATEWAY"; break;
    case NATPMP_ERR_CLOSEERR: errorStr = "CLOSEERR"; break;
    case NATPMP_ERR_RECVFROM: errorStr = "RECVFROM"; break;
    case NATPMP_ERR_NOPENDINGREQ: errorStr = "NOPENDINGREQ"; break;
    case NATPMP_ERR_NOGATEWAYSUPPORT: errorStr = "NOGATEWAYSUPPORT"; break;
    case NATPMP_ERR_CONNECTERR: errorStr = "CONNECTERR"; break;
    case NATPMP_ERR_WRONGPACKETSOURCE: errorStr = "WRONGPACKETSOURCE"; break;
    case NATPMP_ERR_SENDERR: errorStr = "SENDERR"; break;
    case NATPMP_ERR_FCNTLERROR: errorStr = "FCNTLERROR"; break;
    case NATPMP_ERR_GETTIMEOFDAYERR: errorStr = "GETTIMEOFDAYERR"; break;
    case NATPMP_ERR_UNSUPPORTEDVERSION: errorStr = "UNSUPPORTEDVERSION"; break;
    case NATPMP_ERR_UNSUPPORTEDOPCODE: errorStr = "UNSUPPORTEDOPCODE"; break;
    case NATPMP_ERR_UNDEFINEDERROR: errorStr = "UNDEFINEDERROR"; break;
    case NATPMP_ERR_NOTAUTHORIZED: errorStr = "NOTAUTHORIZED"; break;
    case NATPMP_ERR_NETWORKFAILURE: errorStr = "NETWORKFAILURE"; break;
    case NATPMP_ERR_OUTOFRESOURCES: errorStr = "OUTOFRESOURCES"; break;
    case NATPMP_TRYAGAIN: errorStr = "TRYAGAIN"; break;
    default: errorStr = "UNKNOWNERR"; break;
    }

    return errorStr;
}

}} // namespace jami::upnp