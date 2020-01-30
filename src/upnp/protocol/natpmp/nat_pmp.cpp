/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

constexpr static unsigned int ADD_MAP_LIFETIME {3600};
constexpr static unsigned int MAX_RESTART_SEARCH_RETRY {5};

NatPmp::NatPmp()
{
    clearNatPmpHdl(natpmpHdl_);
    pmpIGD_ = std::make_unique<PMPIGD>();
    pmpThread_ = std::thread([this]() {
        {
            std::lock_guard<std::mutex> lk(natpmpMutex_);
            while (pmpRun_) {
                int err = 0;
                char localHostBuf[INET_ADDRSTRLEN];
                if (ip_utils::getHostName(localHostBuf, INET_ADDRSTRLEN) < 0) {
                    JAMI_WARN("NAT-PMP: Couldn't find local host");
                    JAMI_DBG("NAT-PMP: Attempting to initialize with unknown gateway");
                    err = initnatpmp(&natpmpHdl_, 0, 0);
                } else {
                    std::string gw = ip_utils::getGateway(localHostBuf, ip_utils::subnet_mask::prefix_24bit);
                    struct in_addr inaddr;
                    inet_pton(AF_INET, gw.c_str(), &inaddr);
                    err = initnatpmp(&natpmpHdl_, 1, inaddr.s_addr);
                }
                if (err < 0) {
                    JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err));
                    // Retry to init nat pmp in 10 seconds
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                } else {
                    char addrbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &natpmpHdl_.gateway, addrbuf, sizeof(addrbuf));
                    std::string addr(addrbuf);
                    JAMI_DBG("NAT-PMP: Initialized on gateway %s", addr.c_str());
                    break;
                }
            }
        }

        while (pmpRun_) {
            std::unique_lock<std::mutex> lk(validIgdMutex_);
            pmpCv_.wait_until(lk, pmpIGD_->getRenewalTime(), [&] {
                return not pmpRun_ or not pmpIGD_ or restart_
                    or pmpIGD_->getRenewalTime() <= clock::now();
            });
            if (not pmpRun_ or not pmpIGD_) break;
            if (restart_) {
                std::lock_guard<std::mutex> lkNat(natpmpMutex_);
                closenatpmp(&natpmpHdl_);
                clearNatPmpHdl(natpmpHdl_);
                int err = 0;
                char localHostBuf[INET_ADDRSTRLEN];
                if (ip_utils::getHostName(localHostBuf, INET_ADDRSTRLEN) < 0) {
                    JAMI_WARN("NAT-PMP: Couldn't find local host");
                    JAMI_DBG("NAT-PMP: Attempting to initialize with unknown gateway");
                    err = initnatpmp(&natpmpHdl_, 0, 0);
                } else {
                    std::string gw = ip_utils::getGateway(localHostBuf, ip_utils::subnet_mask::prefix_24bit);
                    struct in_addr inaddr;
                    inet_pton(AF_INET, gw.c_str(), &inaddr);
                    err = initnatpmp(&natpmpHdl_, 1, inaddr.s_addr);
                }
                if (err < 0) {
                    JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err));
                    // Retry to re-init nat pmp in 10 seconds
                    if (pmpRun_)
                        pmpCv_.wait_for(lk, std::chrono::seconds(10));
                    continue;
                } else {
                    char addrbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &natpmpHdl_.gateway, addrbuf, sizeof(addrbuf));
                    std::string addr(addrbuf);
                    JAMI_DBG("NAT-PMP: Initialized on gateway %s", addr.c_str());
                    restart_ = false;
                }
            }

            // Check if we need to update IGD.
            auto now = clock::now();
            if (pmpIGD_->renewal_ < now) {
                lk.unlock();
                searchForPmpIgd();
                lk.lock();
            }

            std::vector<Mapping> toRenew, toAdd, toRemove;
            bool clearAll {false};
            if (pmpIGD_) {
                std::lock_guard<std::mutex> upnpLk(pmpIGD_->mapListMutex_);
                if (pmpIGD_->clearAll_) {
                    clearAll = true;
                    pmpIGD_->clearAll_ = false;
                    pmpIGD_->toRemove_.clear();
                } else if (not pmpIGD_->toRemove_.empty()) {
                    // Remove mappings to be removed.
                    toRemove = std::move(pmpIGD_->toRemove_);
                } else if (not pmpIGD_->toAdd_.empty()) {
                    // Add mappings to be added.
                    toAdd = std::move(pmpIGD_->toAdd_);
                }
                // Add mappings who's renewal times are up.
                for (auto it = pmpIGD_->toRenew_.begin(); it != pmpIGD_->toRenew_.end(); ) {
                    if (it->renewal_ <= now) {
                        toRenew.emplace_back(std::move(*it));
                        it = pmpIGD_->toRenew_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            lk.unlock();
            if (clearAll) {
                deleteAllPortMappings(NATPMP_PROTOCOL_UDP);
                deleteAllPortMappings(NATPMP_PROTOCOL_TCP);
            } else {
                for (auto& m : toRemove) {
                    JAMI_DBG("NAT-PMP: Sending request to close port %s", m.toString().c_str());
                    removePortMapping(m);
                }
                for (auto& m : toRenew) {
                    JAMI_DBG("NAT-PMP: Sending request to renew port %s", m.toString().c_str());
                    addPortMapping(m, true);
                }
            }
            for (auto& m : toAdd) {
                JAMI_DBG("NAT-PMP: Sending request to open port %s", m.toString().c_str());
                addPortMapping(m, false);
            }
        }
        std::lock_guard<std::mutex> lk(natpmpMutex_);
        closenatpmp(&natpmpHdl_);
    });
}

NatPmp::~NatPmp()
{
    {
        std::lock_guard<std::mutex> lk1(validIgdMutex_);
        pmpIGD_->clearMappings();
        pmpIGD_->clearAll_ = true;
    }
    pmpCv_.notify_all();
    {
        std::lock_guard<std::mutex> lk1(validIgdMutex_);
        pmpIGD_.reset();
    }
    pmpRun_ = false;
    pmpCv_.notify_all();
    if (pmpThread_.joinable()) {
        pmpThread_.join();
    }
}

void
NatPmp::clearIgds()
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);
    pmpIGD_.reset(new PMPIGD());
    restart_ = true;
}

void
NatPmp::searchForIgd()
{
    // Lock valid IGD.
    std::lock_guard<std::mutex> lk(validIgdMutex_);
    pmpIGD_->renewal_ = clock::now();
    pmpCv_.notify_all();
}

void
NatPmp::requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type)
{
    std::unique_lock<std::mutex> lk(validIgdMutex_);
    Mapping mapping {port_external, port_internal, type};
    if (pmpIGD_) {
        if (not igd->isMapInUse(mapping)) {
            if (pmpIGD_->publicIp_ == igd->publicIp_) {
                JAMI_DBG("NAT-PMP: Attempting to open port %s", mapping.toString().c_str());
                pmpIGD_->addMapToAdd(std::move(mapping));
                pmpCv_.notify_all();
            }
        } else {
            igd->incrementNbOfUsers(mapping);
        }
    } else {
        JAMI_WARN("NAT-PMP: no valid IGD available");
    }
}

void
NatPmp::addPortMapping(Mapping& mapping, bool renew)
{
    std::unique_lock<std::mutex> lk2(validIgdMutex_);
    Mapping mapToAdd(mapping.getPortExternal(),
                     mapping.getPortInternal(),
                     mapping.getType() == PortType::UDP ?
                     upnp::PortType::UDP : upnp::PortType::TCP);
    int err = sendnewportmappingrequest(&natpmpHdl_,
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                        mapping.getPortInternal(),
                                        mapping.getPortExternal(),
                                        ADD_MAP_LIFETIME);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send open port request -> %s %i", getNatPmpErrorStr(err), errno);
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIGD_) {
            pmpIGD_->removeMapToAdd(mapping);
            lk2.unlock();
            notifyContextPortOpenCb_(pmpIGD_->publicIp_, std::move(mapToAdd), false);
        }
        return;
    }
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r != NATPMP_TRYAGAIN) {
            if (r < 0) {
                JAMI_ERR("NAT-PMP: Can't register port mapping %s: %s", mapping.toString().c_str(), getNatPmpErrorStr(r));
            } else {
                mapping.renewal_ = clock::now()
                                + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
                if (pmpIGD_) {
                    if (not renew) {
                        JAMI_WARN("NAT-PMP: Opened port %s", mapping.toString().c_str());
                        lk2.unlock();
                        notifyContextPortOpenCb_(pmpIGD_->publicIp_, std::move(mapToAdd), true);
                    } else {
                        JAMI_WARN("NAT-PMP: Renewed port %s", mapping.toString().c_str());
                    }
                    pmpIGD_->addMapToRenew(std::move(mapping));
                }
            }
            break;
        }
    }
}

void
NatPmp::requestMappingRemove(const Mapping& igdMapping)
{
    std::unique_lock<std::mutex> lk(validIgdMutex_);
    if (pmpIGD_) {
        JAMI_DBG("NAT-PMP: Attempting to close port %s", igdMapping.toString().c_str());
        pmpIGD_->addMapToRemove(Mapping(igdMapping.getPortExternal(), igdMapping.getPortInternal(), igdMapping.getType()));
        lk.unlock();
        pmpCv_.notify_all();
    } else {
        JAMI_WARN("NAT-PMP: no valid IGD available");
    }
}

void
NatPmp::removePortMapping(Mapping& mapping)
{
    std::lock_guard<std::mutex> lk1(natpmpMutex_);
    std::unique_lock<std::mutex> lk2(validIgdMutex_);
    Mapping mapToRemove(mapping.getPortExternal(),
                        mapping.getPortInternal(),
                        mapping.getType() == PortType::UDP ?
                        upnp::PortType::UDP : upnp::PortType::TCP);
    int err = sendnewportmappingrequest(&natpmpHdl_,
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                                        mapping.getPortInternal(),
                                        mapping.getPortExternal(),
                                        0);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send close port request -> %s", getNatPmpErrorStr(err));
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIGD_) {
            pmpIGD_->removeMapToRemove(mapping);
            lk2.unlock();
            notifyContextPortCloseCb_(pmpIGD_->publicIp_, std::move(mapToRemove), false);
        }
        return;
    }
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't unregister port mapping %s", mapping.toString().c_str());
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            if (pmpIGD_) {
                JAMI_WARN("NAT-PMP: Closed port %s", mapping.toString().c_str());
                pmpIGD_->removeMapToRemove(mapping);
                pmpIGD_->removeMapToRenew(mapping);
                lk2.unlock();
                notifyContextPortCloseCb_(pmpIGD_->publicIp_, std::move(mapToRemove), true);
            }
            return;
        }
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
NatPmp::searchForPmpIgd()
{
    std::lock_guard<std::mutex> lk1(natpmpMutex_);
    int err = sendpublicaddressrequest(&natpmpHdl_);
    if (err < 0) {
        std::lock_guard<std::mutex> lk2(validIgdMutex_);
        JAMI_ERR("NAT-PMP: Can't send search request -> %s", getNatPmpErrorStr(err));
        if (pmpIGD_) {
            updateIgdListCb_(this, pmpIGD_.get(), pmpIGD_.get()->publicIp_, false);
        }
        if (restart_) {
            restartSearchRetry_++;
            if (restartSearchRetry_ <= MAX_RESTART_SEARCH_RETRY) {
                // If we're in restart mode and couldn't find an IGD, trigger another
                // search in one second.
                pmpIGD_->renewal_ = clock::now() + std::chrono::seconds(1);
                return;
            }
        }
        // If we're not in restart mode or we've exceeded the number of max retries,
        // trigger another search in one minute (falls back on libupnp).
        pmpIGD_->renewal_ = clock::now() + std::chrono::minutes(1);
        return;
    }
    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        std::unique_lock<std::mutex> lk2(validIgdMutex_);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            pmpIGD_->renewal_ = clock::now() + std::chrono::minutes(5);
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            restartSearchRetry_ = 0;
            pmpIGD_->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmpIGD_->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIGD_->publicIp_.toString().c_str());
            {
                // Store public Ip address.
                std::string publicIpStr(std::move(pmpIGD_.get()->publicIp_.toString()));
                // Add the igd to the upnp context class list.
                lk2.unlock();
                if (updateIgdListCb_(this, pmpIGD_.get(), pmpIGD_.get()->publicIp_, true)) {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s was added to the list", publicIpStr.c_str());
                } else {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s is already in the list", publicIpStr.c_str());
                }
                lk2.lock();
            }
            pmpIGD_->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
NatPmp::deleteAllPortMappings(int proto)
{
    if (sendnewportmappingrequest(&natpmpHdl_, proto, 0, 0, 0) < 0) {
        JAMI_ERR("NAT-PMP: Can't send all port mapping removal request");
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r != NATPMP_TRYAGAIN) {
            if (r < 0)
                JAMI_ERR("NAT-PMP: Can't remove all port mappings: %s", getNatPmpErrorStr(r));
            break;
        }
    }
}

void
NatPmp::clearNatPmpHdl(natpmp_t& hdl)
{
    bzero(&hdl, sizeof(hdl));
}

const char*
NatPmp::getNatPmpErrorStr(int errorCode)
{
#ifdef ENABLE_STRNATPMPERR
    return strnatpmperr(errorCode);
#else
    switch(errorCode) {
    case NATPMP_ERR_INVALIDARGS: return "INVALIDARGS"; break;
    case NATPMP_ERR_SOCKETERROR: return "SOCKETERROR"; break;
    case NATPMP_ERR_CANNOTGETGATEWAY: return "CANNOTGETGATEWAY"; break;
    case NATPMP_ERR_CLOSEERR: return "CLOSEERR"; break;
    case NATPMP_ERR_RECVFROM: return "RECVFROM"; break;
    case NATPMP_ERR_NOPENDINGREQ: return "NOPENDINGREQ"; break;
    case NATPMP_ERR_NOGATEWAYSUPPORT: return "NOGATEWAYSUPPORT"; break;
    case NATPMP_ERR_CONNECTERR: return "CONNECTERR"; break;
    case NATPMP_ERR_WRONGPACKETSOURCE: return "WRONGPACKETSOURCE"; break;
    case NATPMP_ERR_SENDERR: return "SENDERR"; break;
    case NATPMP_ERR_FCNTLERROR: return "FCNTLERROR"; break;
    case NATPMP_ERR_GETTIMEOFDAYERR: return "GETTIMEOFDAYERR"; break;
    case NATPMP_ERR_UNSUPPORTEDVERSION: return "UNSUPPORTEDVERSION"; break;
    case NATPMP_ERR_UNSUPPORTEDOPCODE: return "UNSUPPORTEDOPCODE"; break;
    case NATPMP_ERR_UNDEFINEDERROR: return "UNDEFINEDERROR"; break;
    case NATPMP_ERR_NOTAUTHORIZED: return "NOTAUTHORIZED"; break;
    case NATPMP_ERR_NETWORKFAILURE: return "NETWORKFAILURE"; break;
    case NATPMP_ERR_OUTOFRESOURCES: return "OUTOFRESOURCES"; break;
    case NATPMP_TRYAGAIN: return "TRYAGAIN"; break;
    default: return "UNKNOWNERR"; break;
    }
#endif
}

}} // namespace jami::upnp