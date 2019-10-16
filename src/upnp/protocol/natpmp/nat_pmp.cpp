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
    clearNatPmpHdl(natpmpHdl_);             // Clear natpmp handle.
    pmpIgd_ = std::make_unique<PMPIGD>();   // Create pmpIgd. NatPmp is the only owner.
    changeState(NatPmpState::PMP_INIT);     // Start off the state queue.

    pmpThread_ = std::thread([this]() {

        std::unique_lock<std::mutex> lkState(queueMutex_);
        while (pmpRun_) {
            pmpCv_.wait_until(lkState, pmpIgd_.get()->getRenewalTime(), [this] {
                return goToNextState();
            });

            switch (pmpState_) {
            case NatPmpState::PMP_IDLE:
                // Do nothing
                break;
            case NatPmpState::PMP_INIT:
            {
                std::lock_guard<std::mutex> lk(natpmpMutex_);
                unsigned int retry = 0;
                bool isInit = true;
                while (not initNatPmp()) {
                    retry++;
                    if (retry > MAX_INIT_RETRY) {
                        isInit = false;
                        changeState(NatPmpState::PMP_ERROR);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (isInit)
                    changeState(NatPmpState::PMP_SEARCH);
                break;
            }
            case NatPmpState::PMP_RESTART:
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::lock_guard<std::mutex> lk(natpmpMutex_);
                clearNatPmpHdl(natpmpHdl_);
                unsigned int retry = 0;
                bool isInit = true;
                while (not initNatPmp()) {
                    retry++;
                    if (retry > MAX_INIT_RETRY) {
                        isInit = false;
                        changeState(NatPmpState::PMP_ERROR);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (isInit)
                    changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_SEARCH:
            {
                std::unique_lock<std::mutex> lk1(validIgdMutex_);
                std::lock_guard<std::mutex> lk2(natpmpMutex_);
                unsigned int retry = 0;
                bool igdFound = true;
                while (not searchForPmpIgd(pmpIgd_.get())) {
                    retry++;
                    if (retry > MAX_SEARCH_RETRY) {
                        igdFound = false;
                        changeState(NatPmpState::PMP_ERROR);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (igdFound) {
                    JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIgd_.get()->publicIp_.toString().c_str());
                    lk1.unlock();
                    updateIgdListCb_(this, pmpIgd_.get(), pmpIgd_.get()->publicIp_, true);
                    lk1.lock();
                    changeState(NatPmpState::PMP_CLOSE_ALL);
                }
                break;
            }
            case NatPmpState::PMP_OPEN_PORT:
            {
                std::vector<Mapping> add;
                std::unique_lock<std::mutex> lk(validIgdMutex_);
                if (pmpIgd_) {
                    if (not pmpIgd_->mapToAddList_.empty()) {
                        for (unsigned int i = 0; i < pmpIgd_->mapToAddList_.size(); i++)
                            add.push_back(pmpIgd_->mapToAddList_[i]);
                    }
                }
                if (not add.empty()) {
                    std::lock_guard<std::mutex> lk2(natpmpMutex_);
                    for (auto& m : add) {
                        JAMI_DBG("NAT-PMP: Sent request to open port %s", m.toString().c_str());
                        if (addPortMapping(m)) {
                            JAMI_WARN("NAT-PMP: Opened port %s", m.toString().c_str());
                            pmpIgd_->removeMapToAdd(m);
                            m.renewal_ = clock::now() + std::chrono::seconds(ADD_MAP_LIFETIME/2);
                            pmpIgd_->addMapToRenew(m);
                            lk.unlock();
                            notifyContextPortOpenCb_(pmpIgd_->publicIp_, m, true);
                            lk.lock();
                        } else {
                            lk.unlock();
                            notifyContextPortOpenCb_(pmpIgd_->publicIp_, m, false);
                            lk.lock();
                        }
                    }
                }
                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_CLOSE_PORT:
            {
                std::vector<Mapping> remove;
                std::unique_lock<std::mutex> lk(validIgdMutex_);
                if (pmpIgd_) {
                    if (not pmpIgd_->mapToRemoveList_.empty()) {
                        for (unsigned int i = 0; i < pmpIgd_->mapToRemoveList_.size(); i++)
                            remove.push_back(pmpIgd_->mapToRemoveList_[i]);
                    }
                }
                if (not remove.empty()) {
                    std::lock_guard<std::mutex> lk2(natpmpMutex_);
                    for (auto& m : remove) {
                        JAMI_DBG("NAT-PMP: Sent request to close port %s", m.toString().c_str());
                        if (removePortMapping(m)) {
                            JAMI_WARN("NAT-PMP: Closed port %s", m.toString().c_str());
                            pmpIgd_->removeMapToRemove(m);
                            pmpIgd_->removeMapToRenew(m);
                            lk.unlock();
                            notifyContextPortCloseCb_(pmpIgd_->publicIp_, m, true);
                            lk.lock();
                        } else {
                            lk.unlock();
                            notifyContextPortCloseCb_(pmpIgd_->publicIp_, m, false);
                            lk.lock();
                        }
                    }
                }
                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_CLOSE_ALL:
            {
                std::lock_guard<std::mutex> lk1(validIgdMutex_);
                std::lock_guard<std::mutex> lk2(natpmpMutex_);

                // Clear all the mappings.
                deleteAllPortMappings(NATPMP_PROTOCOL_UDP);
                deleteAllPortMappings(NATPMP_PROTOCOL_TCP);
                if (pmpIgd_)
                    pmpIgd_->mapToRemoveList_.clear();

                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_RENEW_IGD:
            {
                std::unique_lock<std::mutex> lk1(validIgdMutex_);
                std::lock_guard<std::mutex> lk2(natpmpMutex_);
                unsigned int retry = 0;
                bool igdFound = true;
                while (not searchForPmpIgd(pmpIgd_.get())) {
                    retry++;
                    if (retry > MAX_SEARCH_RETRY) {
                        changeState(NatPmpState::PMP_ERROR);
                        igdFound = false;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (igdFound) {
                    JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIgd_.get()->publicIp_.toString().c_str());
                    lk1.unlock();
                    updateIgdListCb_(this, pmpIgd_.get(), pmpIgd_.get()->publicIp_, true);
                    lk1.lock();
                    changeState(NatPmpState::PMP_IDLE);
                }
                break;
            }
            case NatPmpState::PMP_RENEW_PORT:
            {
                std::vector<Mapping> renew;
                std::unique_lock<std::mutex> lk(validIgdMutex_);
                if (pmpIgd_) {
                    if (not pmpIgd_->mapToRenewList_.empty()) {
                        for (unsigned int i = 0; i < pmpIgd_->mapToRenewList_.size(); i++) {
                            pmpIgd_->mapToRenewList_[i].renewal_ = clock::now() + std::chrono::seconds(ADD_MAP_LIFETIME/2);
                            renew.push_back(pmpIgd_->mapToRenewList_[i]);
                        }
                    }
                }
                if (not renew.empty()) {
                    std::lock_guard<std::mutex> lk2(natpmpMutex_);
                    for (auto& m : renew) {
                        JAMI_DBG("NAT-PMP: Sent request to renew port %s", m.toString().c_str());
                        if (addPortMapping(m))
                            JAMI_WARN("NAT-PMP: Renewed port %s", m.toString().c_str());
                    }
                }
                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_ERROR:
            {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                changeState(NatPmpState::PMP_RESTART, true);
                break;
            }
            case NatPmpState::PMP_EXIT:
            {
                // Empty queue.
                lkState.lock();
                while(!stateQueue_.empty())
                    stateQueue_.pop();
                lkState.unlock();

                std::lock_guard<std::mutex> lk(natpmpMutex_);

                // Close all remaining mappings.
                deleteAllPortMappings(NATPMP_PROTOCOL_UDP);
                deleteAllPortMappings(NATPMP_PROTOCOL_TCP);

                // Close natpmp library handle.
                closenatpmp(&natpmpHdl_);

                // Exit thread.
                pmpRun_ = false;
                break;
            }
            default: break;
            }

            // Update clock;
            auto now = clock::now();

            if (isIgdUpForRenewal(now))
                changeState(NatPmpState::PMP_RENEW_IGD);

            if (isMappingUpForRenewal(now))
                changeState(NatPmpState::PMP_RENEW_PORT);
        }
    });
}

NatPmp::~NatPmp()
{
    {
        std::lock_guard<std::mutex> lk1(validIgdMutex_);
        pmpIgd_.reset();
    }

    if (pmpThread_.joinable()) {
        pmpThread_.join();
    }

    changeState(NatPmpState::PMP_EXIT, true);
    pmpCv_.notify_all();
}

void
NatPmp::clearIgds()
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);
    pmpIgd_.reset(new PMPIGD());
    changeState(NatPmpState::PMP_RESTART, true);
}

void
NatPmp::searchForIgd()
{
    changeState(NatPmpState::PMP_SEARCH);
    pmpCv_.notify_all();
}

void
NatPmp::requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type)
{
    if (not pmpRun_)
        return;

    std::lock_guard<std::mutex> lk(validIgdMutex_);

    Mapping mapping {port_external, port_internal, type};

    if (not pmpIgd_) {
        JAMI_WARN("NAT-PMP: no valid IGD available");
        return;
    }

    if (igd->isMapInUse(mapping)) {
        igd->incrementNbOfUsers(mapping);
        return;
    }

    if (pmpIgd_->publicIp_ == igd->publicIp_) {
        JAMI_DBG("NAT-PMP: Attempting to open port %s", mapping.toString().c_str());
        pmpIgd_->addMapToAdd(std::move(mapping));

        changeState(NatPmpState::PMP_OPEN_PORT);
    }

    pmpCv_.notify_all();
}

void
NatPmp::requestMappingRemove(const Mapping& igdMapping)
{
    if (not pmpRun_)
        return;

    std::lock_guard<std::mutex> lk(validIgdMutex_);

    if (not pmpIgd_) {
        JAMI_WARN("NAT-PMP: no valid IGD available");
        return;
    }

    JAMI_DBG("NAT-PMP: Attempting to close port %s", igdMapping.toString().c_str());
    pmpIgd_->addMapToRemove(igdMapping);

    changeState(NatPmpState::PMP_CLOSE_PORT);
    pmpCv_.notify_all();
}

inline void
NatPmp::changeState(NatPmpState state, bool clearBeforeInsert)
{
    // Clear the state queue if requested.
    if (clearBeforeInsert) {
        while(!stateQueue_.empty())
            stateQueue_.pop();
    }

    stateQueue_.push((NatPmpState)state);
}

inline bool
NatPmp::goToNextState()
{
    if (not stateQueue_.empty()) {
        pmpState_ = stateQueue_.front();
        stateQueue_.pop();
        return true;
    }

    return false;
}

bool
NatPmp::initNatPmp()
{
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
        return false;
    } else {
        char addrbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &natpmpHdl_.gateway, addrbuf, sizeof(addrbuf));
        std::string addr(addrbuf);
        JAMI_DBG("NAT-PMP: Initialized on gateway %s", addr.c_str());
        return true;
    }

    return false;
}

bool
NatPmp::searchForPmpIgd(PMPIGD* pmpIgd)
{
    if (not pmpRun_)
        return false;

    int sendErr = sendpublicaddressrequest(&natpmpHdl_);

    if (sendErr < 0) {
        JAMI_ERR("NAT-PMP: Can't send search request -> %s", getNatPmpErrorStr(sendErr).c_str());
        pmpIgd->renewal_ = clock::now() + std::chrono::minutes(1);
        return false;
    }

    int responseErr = 0;
    unsigned int retry = 0;
    while (retry <= MAX_READ_RESPONSE_RETRY) {
        natpmpresp_t response;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        responseErr = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (responseErr < 0 && responseErr != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't find internet gateway device -> %s",
            getNatPmpErrorStr(responseErr).c_str());
            pmpIgd->renewal_ = clock::now() + std::chrono::minutes(5);
            return false;
        } else if (responseErr != NATPMP_TRYAGAIN) {
            pmpIgd->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmpIgd->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            pmpIgd->renewal_ = clock::now() + std::chrono::minutes(60);
            return true;
        }
        retry++;
    }
    return false;
}

bool
NatPmp::addPortMapping(const Mapping& map)
{
    if (not pmpRun_)
        return false;

    int sendErr = sendnewportmappingrequest(
                  &natpmpHdl_,
                  map.getType() == PortType::UDP ?
                  NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                  map.getPortInternal(),
                  map.getPortExternal(),
                  ADD_MAP_LIFETIME);

    if (sendErr < 0) {
        JAMI_ERR("NAT-PMP: Can't send open port request -> %s", getNatPmpErrorStr(sendErr).c_str());
        return false;
    }

    natpmpresp_t response;
    int responseErr = 0;
    unsigned int retry = 0;
    while (retry <= MAX_READ_RESPONSE_RETRY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        responseErr = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (responseErr < 0 && responseErr != NATPMP_TRYAGAIN)
            JAMI_ERR("NAT-PMP: Can't register port mapping %s -> %s",
            map.toString().c_str(), getNatPmpErrorStr(responseErr).c_str());
        else
            return true;
        retry++;
    }
    return false;
}

bool
NatPmp::removePortMapping(const Mapping& map)
{
    if (not pmpRun_)
        return false;

    int sendErr = sendnewportmappingrequest(
                  &natpmpHdl_,
                  map.getType() == PortType::UDP ?
                  NATPMP_PROTOCOL_UDP : NATPMP_PROTOCOL_TCP,
                  map.getPortInternal(),
                  map.getPortExternal(),
                  REMOVE_MAP_LIFETIME);

    if (sendErr < 0) {
        JAMI_ERR("NAT-PMP: Can't send close port request -> %s", getNatPmpErrorStr(sendErr).c_str());
        return false;
    }

    natpmpresp_t response;
    int responseErr = 0;
    unsigned int retry = 0;
    while (retry <= MAX_READ_RESPONSE_RETRY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        responseErr = readnatpmpresponseorretry(&natpmpHdl_, &response);
        if (responseErr < 0 && responseErr != NATPMP_TRYAGAIN)
            JAMI_ERR("NAT-PMP: Can't unregister port mapping %s -> %s",
            map.toString().c_str(), getNatPmpErrorStr(responseErr).c_str());
        else
            return true;
        retry++;
    }
    return false;
}

void
NatPmp::removeAllLocalMappings(IGD* /*igd*/)
{
    changeState(NatPmpState::PMP_CLOSE_ALL);
    pmpCv_.notify_all();
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
        if (r < 0 && r != NATPMP_TRYAGAIN) {
            JAMI_ERR("NAT-PMP: Can't remove all port mappings");
            break;
        }
        else if (r != NATPMP_TRYAGAIN) {
            break;
        }
    }
}

bool
NatPmp::isMappingUpForRenewal(const time_point& now)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    if (not pmpIgd_)
        return false;

    if (pmpIgd_->mapToRenewList_.empty())
            return false;

    for (auto it = pmpIgd_->mapToRenewList_.cbegin(); it != pmpIgd_->mapToRenewList_.cend(); it++) {
        if (pmpIgd_->isMapUpForRenewal(*it, now))
            return true;
    }

    return false;
}

bool
NatPmp::isIgdUpForRenewal(const time_point& now)
{
    std::lock_guard<std::mutex> lk(validIgdMutex_);

    if (pmpIgd_) {
        if (pmpIgd_->renewal_ < now)
            return true;
    }
    return false;
}

void
NatPmp::clearNatPmpHdl(natpmp_t& hdl)
{
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