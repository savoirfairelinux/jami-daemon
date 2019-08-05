/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
{
    clearNatPmpHdl(natpmpHdl_);
    pmpIgd_ = std::make_unique<PMPIGD>();

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
                    JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err).c_str());
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
            pmpCv_.wait_until(lk, pmpIgd_->getRenewalTime(), [&] {
                return not pmpRun_ or
                       pmpIgd_->getRenewalTime() <= clock::now() or
                       not pmpIgd_->mapToRemoveList_.empty() or
                       restart_;
            });

            // Exit thread if pmpRun_ was set to false. Signal program exit.
            if (not pmpRun_) break;

            // Update clock;
            auto now = clock::now();

            // If the restart flag is set, wait for 1 second to have passed by to try and reinitialize natpmp.
            if (restart_ and (now - restartTimer_ >= std::chrono::seconds(1))) {
                {
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
                        JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err).c_str());
                        restartTimer_ = clock::now();
                    } else {
                        char addrbuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &natpmpHdl_.gateway, addrbuf, sizeof(addrbuf));
                        std::string addr(addrbuf);
                        JAMI_DBG("NAT-PMP: Initialized on gateway %s", addr.c_str());
                        restart_ = false;
                    }
                }
            }

            // Check if we need to update IGD.
            if (pmpIgd_->renewal_ < now) {
                lk.unlock();
                searchForPmpIgd();
                lk.lock();
            }

            if (pmpIgd_) {
                if (pmpIgd_->clearAll_) {
                    // Clear all the mappings.
                    deleteAllPortMappings(NATPMP_PROTOCOL_UDP);
                    deleteAllPortMappings(NATPMP_PROTOCOL_TCP);
                    pmpIgd_->mapToRemoveList_.clear();
                    pmpIgd_->clearAll_ = false;
                } else if (not pmpIgd_->mapToRemoveList_.empty()) {
                    // Remove mappings to be removed.
                    decltype(pmpIgd_->mapToRemoveList_) removed = std::move(pmpIgd_->mapToRemoveList_);
                    for (auto& m : removed) {
                        JAMI_DBG("NAT-PMP: Sent request to close port %s", m.toString().c_str());
                        lk.unlock();
                        removePortMapping(m);
                        lk.lock();
                    }
                } else if (not pmpIgd_->mapToAddList_.empty()) {
                    // Add mappings to be added.
                    decltype(pmpIgd_->mapToAddList_) add = std::move(pmpIgd_->mapToAddList_);
                    for (auto& m : add) {
                        JAMI_DBG("NAT-PMP: Sent request to open port %s", m.toString().c_str());
                        lk.unlock();
                        addPortMapping(m, false);
                        lk.lock();
                    }
                }

                // Add mappings who's renewal times are up.
                decltype(pmpIgd_->mapToRenewList_) renew = std::move(pmpIgd_->mapToRenewList_);
                for (auto& m : renew) {
                    if (pmpIgd_->isMapUpForRenewal(Mapping(m.getPortExternal(), m.getPortInternal(), m.getType()), now)) {
                        JAMI_DBG("NAT-PMP: Sent request to renew port %s", m.toString().c_str());
                        lk.unlock();
                        addPortMapping(m, true);
                        lk.lock();
                    }
                }
            }
        }
        closenatpmp(&natpmpHdl_);
    });
}

NatPmp::~NatPmp()
{

    std::lock_guard<std::mutex> lk1(validIgdMutex_);
    {
        pmpIgd_->clearMappings();
        pmpIgd_->clearAll_ = true;
        pmpCv_.notify_all();
    }

    pmpIgd_.reset();
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
    pmpIgd_.reset(new PMPIGD());
    restart_ = true;
    restartTimer_ = clock::now();
}

void
NatPmp::searchForIgd()
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    pmpIgd_->renewal_ = clock::now();
    pmpCv_.notify_all();
}

void
NatPmp::requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    Mapping mapping {port_external, port_internal, type};

    if (pmpIgd_) {
        if (not igd->isMapInUse(mapping)) {
            if (pmpIgd_->publicIp_ == igd->publicIp_) {
                JAMI_DBG("NAT-PMP: Attempting to open port %s", mapping.toString().c_str());
                pmpIgd_->addMapToAdd(std::move(mapping));
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
NatPmp::requestMappingRemove(const Mapping& igdMapping)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    if (pmpIgd_) {
        JAMI_DBG("NAT-PMP: Attempting to close port %s", igdMapping.toString().c_str());
        pmpIgd_->addMapToRemove(Mapping(igdMapping.getPortExternal(), igdMapping.getPortInternal(), igdMapping.getType()));
        pmpCv_.notify_all();
    } else {
        JAMI_WARN("NAT-PMP: no valid IGD available");
    }
}

void
NatPmp::searchForPmpIgd()
{
    std::lock_guard<std::mutex> lk1(natpmpMutex_);
    std::unique_lock<std::mutex> lk2(validIgdMutex_);

    int err = sendpublicaddressrequest(&natpmpHdl_);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send search request -> %s", getNatPmpErrorStr(err).c_str());
        if (restart_) {
            restartSearchRetry_++;
            if (restartSearchRetry_ <= MAX_RESTART_SEARCH_RETRY) {
                // If we're in restart mode and couldn't find an IGD, trigger another
                // search in one second.
                pmpIgd_->renewal_ = clock::now() + std::chrono::seconds(1);
                return;
            }
        }
        // If we're not in restart mode or we've exceeded the number of max retries,
        // trigger another search in one minute (falls back on libupnp).
        pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(1);
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(5);
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            restartSearchRetry_ = 0;
            pmpIgd_->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmpIgd_->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIgd_->publicIp_.toString().c_str());
            {
                // Store public Ip address.
                std::string publicIpStr(std::move(pmpIgd_.get()->publicIp_.toString()));

                // Add the igd to the upnp context class list.
                lk2.unlock();
                if (updateIgdListCb_(this, pmpIgd_.get(), pmpIgd_.get()->publicIp_, true)) {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s was added to the list", publicIpStr.c_str());
                } else {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s is already in the list", publicIpStr.c_str());
                }
                lk2.lock();
            }
            pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(1);
            break;
        }
    }
}

void
NatPmp::addPortMapping(Mapping& mapping, bool renew)
{
    std::lock_guard<std::mutex> lk1(natpmpMutex_);
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
        JAMI_ERR("NAT-PMP: Can't send open port request -> %s", getNatPmpErrorStr(err).c_str());
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIgd_) {
            pmpIgd_->removeMapToAdd(mapping);
            lk2.unlock();
            notifyContextPortOpenCb_(pmpIgd_->publicIp_, std::move(mapToAdd), false);
        }
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't register port mapping %s", mapping.toString().c_str());
            break;
        } else if (r != NATPMP_TRYAGAIN) {
            mapping.renewal_ = clock::now()
                             + std::chrono::seconds(response.pnu.newportmapping.lifetime/2);
            if (pmpIgd_) {
                if (not renew) {
                    JAMI_WARN("NAT-PMP: Opened port %s", mapping.toString().c_str());
                    pmpIgd_->removeMapToAdd(mapping);
                    pmpIgd_->addMapToRenew(std::move(mapping));
                    lk2.unlock();
                    notifyContextPortOpenCb_(pmpIgd_->publicIp_, std::move(mapToAdd), true);
                } else {
                    JAMI_WARN("NAT-PMP: Renewed port %s", mapping.toString().c_str());
                }
            }
            break;
        }
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
                                        REMOVE_MAP_LIFETIME);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send close port request -> %s", getNatPmpErrorStr(err).c_str());
        mapping.renewal_ = clock::now() + std::chrono::minutes(1);
        if (pmpIgd_) {
            pmpIgd_->removeMapToRemove(mapping);
            lk2.unlock();
            notifyContextPortCloseCb_(pmpIgd_->publicIp_, std::move(mapToRemove), false);
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
            if (pmpIgd_) {
                JAMI_WARN("NAT-PMP: Closed port %s", mapping.toString().c_str());
                pmpIgd_->removeMapToRemove(mapping);
                pmpIgd_->removeMapToRenew(mapping);
                lk2.unlock();
                notifyContextPortCloseCb_(pmpIgd_->publicIp_, std::move(mapToRemove), true);
            }
            break;
        }
    }
}

void
NatPmp::removeAllLocalMappings(IGD* /*igd*/)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    pmpIgd_->clearAll_ = true;
    pmpCv_.notify_all();
}

void
NatPmp::deleteAllPortMappings(int proto)
{
    std::lock_guard<std::mutex> lk(natpmpMutex_);

    if (sendnewportmappingrequest(&natpmpHdl_, proto, 0, 0, 0) < 0) {
        JAMI_ERR("NAT-PMP: Can't send all port mapping removal request");
        return;
    }

    while (pmpRun_) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto r = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't remove all port mappings");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            break;
        }
    }
}

void
NatPmp::clearNatPmpHdl(natpmp_t& hdl)
{
    std::lock_guard<std::mutex> lk(natpmpMutex_);

    memset(&hdl.s,                   0, sizeof(hdl.s));
    memset(&hdl.gateway,             0, sizeof(in_addr_t));
    memset(&hdl.has_pending_request, 0, sizeof(hdl.has_pending_request));
    memset(&hdl.pending_request,     0, sizeof(hdl.pending_request));
    memset(&hdl.pending_request_len, 0, sizeof(hdl.pending_request_len));
    memset(&hdl.try_number,          0, sizeof(hdl.try_number));
    memset(&hdl.retry_time.tv_sec,   0, sizeof(hdl.retry_time.tv_sec));
    memset(&hdl.retry_time.tv_usec,  0, sizeof(hdl.retry_time.tv_usec));
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