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

static int getLocalHostName(char *out, size_t out_len)
{
    char tempstr[INET_ADDRSTRLEN];
    const char *p = NULL;

#ifdef _WIN32

    struct hostent* h = NULL;
    struct sockaddr_in localAddr;

    memset(&localAddr, 0, sizeof(localAddr));

    gethostname(out, out_len);
    h = gethostbyname(out);
    if (h != NULL) {
        memcpy(&localAddr.sin_addr, h->h_addr_list[0], 4);
        p = inet_ntop(AF_INET, &localAddr.sin_addr, tempstr, sizeof(tempstr));
        if (p)
            strncpy(out, p, out_len);
        else
            return -1;
    } else {
        return -1;
    }

#elif (defined(BSD) && BSD >= 199306) || defined(__FreeBSD_kernel__)

    int retVal = 0;
    struct ifaddrs* ifap;
    struct ifaddrs* ifa;

    if (getifaddrs(&ifap) != 0)
        return -1;

    // Cycle through available interfaces.
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {

        // Skip loopback, point-to-point and down interfaces.
        // except don't skip down interfaces if we're trying to get
        // a list of configurable interfaces.
        if ((ifa->ifa_flags & IFF_LOOPBACK) || (!( ifa->ifa_flags & IFF_UP)))
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                // We don't want the loopback interface. Go to next one.
                continue;
            }

            p = inet_ntop(AF_INET, &((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr, tempstr, sizeof(tempstr));
            if (p)
                strncpy(out, p, out_len);
            else
                retVal = -1;
            }
            break;
        }
    }
    freeifaddrs(ifap);

    retVal = ifa ? 0 : -1;
    return retVal;

#else

    struct ifconf ifConf;
    struct ifreq ifReq;
    struct sockaddr_in localAddr;

    char szBuffer[NATPMP_MAX_INTERFACES * sizeof (struct ifreq)];
    int nResult;
    int localSock;

    memset(&ifConf,  0, sizeof(ifConf));
    memset(&ifReq,   0, sizeof(ifReq));
    memset(szBuffer, 0, sizeof(szBuffer));
    memset(&localAddr, 0, sizeof(localAddr));

    // Create an unbound datagram socket to do the SIOCGIFADDR ioctl on.
    localSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (localSock == NATPMP_INVALID_SOCKET)
        return -1;

    /* Get the interface configuration information... */
    ifConf.ifc_len = (int)sizeof szBuffer;
    ifConf.ifc_ifcu.ifcu_buf = (caddr_t) szBuffer;
    nResult = ioctl(localSock, SIOCGIFCONF, &ifConf);
    if (nResult < 0) {
        close(localSock);
        return -1;
    }

    long unsigned int i;
    int j = 0;

    // Cycle through the list of interfaces looking for IP addresses.
    for (i = 0lu; i < (long unsigned int)ifConf.ifc_len && j < NATPMP_DEFAULT_INTERFACE; ) {

        struct ifreq *pifReq = (struct ifreq *)((caddr_t)ifConf.ifc_req + i);
        i += sizeof *pifReq;

        // See if this is the sort of interface we want to deal with.
        memset(ifReq.ifr_name, 0, sizeof(ifReq.ifr_name));
        strncpy(ifReq.ifr_name, pifReq->ifr_name, sizeof(ifReq.ifr_name) - 1);
        ioctl(localSock, SIOCGIFFLAGS, &ifReq);

        // Skip loopback, point-to-point and down interfaces.
        // except don't skip down interfaces if we're trying to get
        // a list of configurable interfaces.
        if ((ifReq.ifr_flags & IFF_LOOPBACK) || (!(ifReq.ifr_flags & IFF_UP)))
            continue;

        if (pifReq->ifr_addr.sa_family == AF_INET) {
            memcpy(&localAddr, &pifReq->ifr_addr, sizeof pifReq->ifr_addr);
            if (localAddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                // We don't want the loopback interface. Go to the next one.
                continue;
            }
        }
        j++;    // Increment j if we found an address which is not loopback and is up.
    }
    close(localSock);

    p = inet_ntop(AF_INET, &localAddr.sin_addr, tempstr, sizeof(tempstr));
    if (p)
        strncpy(out, p, out_len);
    else
        return -1;

#endif

    return 0;
}

NatPmp::NatPmp()
{
    clearNatPmpHdl(natpmpHdl_);             // Clear natpmp handle.
    pmpIgd_ = std::make_unique<PMPIGD>();   // Create pmpIgd. NatPmp is the only owner.
    changeState(NatPmpState::PMP_INIT);     // Start off the state queue.

    pmpThread_ = std::thread([this]() {

        std::unique_lock<std::mutex> lkState(queueMutex_);
        while (pmpRun_) {
            pmpCv_.wait_for(lkState, std::chrono::milliseconds(750), [this] {
                return goToNextState();
            });

            // Update clock;
            auto now = clock::now();

            switch (pmpState_) {
            case NatPmpState::PMP_IDLE:
                // Do nothing
                break;
            case NatPmpState::PMP_INIT:
            {
                std::lock_guard<std::mutex> lk(natpmpMutex_);

                if (not initNatPmp()) {
                    initRetry_++;
                    if (initRetry_ > MAX_INIT_RETRY) {
                        initRetry_ = 0;
                        changeState(NatPmpState::PMP_ERROR);
                        errorTimer_ = clock::now();
                        break;
                    }
                    // If init failed, try again after a delay.
                    restartTimer_ = clock::now();
                    changeState(NatPmpState::PMP_RESTART);
                    break;
                }
                isInit_ = true;
                initRetry_ = 0;
                changeState(NatPmpState::PMP_SEARCH);
                break;
            }
            case NatPmpState::PMP_RESTART:
            {
                isInit_ = false;
                if (now - restartTimer_ >= std::chrono::seconds(1)) {
                    std::lock_guard<std::mutex> lk(natpmpMutex_);
                    clearNatPmpHdl(natpmpHdl_);
                    changeState(NatPmpState::PMP_INIT);
                }
                break;
            }
            case NatPmpState::PMP_SEARCH:
            {
                std::unique_lock<std::mutex> lk1(validIgdMutex_);
                std::lock_guard<std::mutex> lk2(natpmpMutex_);

                if (not searchForPmpIgd(pmpIgd_.get())) {
                    searchRetry_++;
                    if (searchRetry_ > MAX_SEARCH_RETRY) {
                        searchRetry_ = 0;
                        changeState(NatPmpState::PMP_ERROR);
                        errorTimer_ = clock::now();
                    }
                    break;
                }

                JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIgd_.get()->publicIp_.toString().c_str());

                // Add the igd to the upnp context class list.
                lk1.unlock();
                updateIgdListCb_(this, pmpIgd_.get(), pmpIgd_.get()->publicIp_, true);
                lk1.lock();
                pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(1);

                searchRetry_ = 0;
                if (isRestart_)
                     isRestart_ = false;
                changeState(NatPmpState::PMP_CLOSE_ALL);
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

                if (not searchForPmpIgd(pmpIgd_.get())) {
                    searchRetry_++;
                    if (searchRetry_ > MAX_SEARCH_RETRY) {
                        searchRetry_ = 0;
                        changeState(NatPmpState::PMP_ERROR);
                        errorTimer_ = clock::now();
                    }
                    break;
                }

                JAMI_DBG("NAT-PMP: Found device with external IP %s", pmpIgd_.get()->publicIp_.toString().c_str());

                // Add the igd to the upnp context class list.
                lk1.unlock();
                updateIgdListCb_(this, pmpIgd_.get(), pmpIgd_.get()->publicIp_, true);
                lk1.lock();
                pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(1);

                searchRetry_ = 0;

                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_RENEW_PORT:
            {
                std::vector<Mapping> renew;
                std::unique_lock<std::mutex> lk(validIgdMutex_);
                if (pmpIgd_) {
                    if (not pmpIgd_->mapToRenewList_.empty()) {
                        for (unsigned int i = 0; i < pmpIgd_->mapToAddList_.size(); i++)
                            renew.push_back(pmpIgd_->mapToRenewList_[i]);
                    }
                }

                if (not renew.empty()) {
                    std::lock_guard<std::mutex> lk2(natpmpMutex_);
                    for (auto& m : renew) {
                        JAMI_DBG("NAT-PMP: Sent request to renew port %s", m.toString().c_str());
                        if (addPortMapping(m)) {
                            JAMI_WARN("NAT-PMP: Renewed port %s", m.toString().c_str());
                            auto mapToRenew = pmpIgd_->getMapping(m.getPortExternal(), m.getType());
                            mapToRenew.renewal_ = clock::now() + std::chrono::seconds(ADD_MAP_LIFETIME/2);
                        }
                    }
                }

                changeState(NatPmpState::PMP_IDLE);
                break;
            }
            case NatPmpState::PMP_ERROR:
            {
                // If we're in error for more than 30 seconds try and restart.
                if (now - errorTimer_ >= std::chrono::seconds(30)) {
                    changeState(NatPmpState::PMP_RESTART, true);
                    errorTimer_ = clock::now();
                }

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

            // Only check for renewal if:
            // - Not in error.
            // - Is initialized.
            // - Not in restart mode.
            // - Still in thread run mode.
            {
                if (pmpState_ != NatPmpState::PMP_ERROR and
                    isInit_ and !isRestart_ and pmpRun_) {

                    std::lock_guard<std::mutex> lk(validIgdMutex_);

                    if (isIgdUpForRenewal(clock::now())) {
                        changeState(NatPmpState::PMP_RENEW_IGD);
                    } else {
                        if (isMappingUpForRenewal(clock::now())) {
                            changeState(NatPmpState::PMP_RENEW_PORT);
                        }
                    }
                }
            }
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
    isRestart_ = true;
    restartTimer_ = clock::now();

    changeState(NatPmpState::PMP_RESTART, true);
}

void
NatPmp::searchForIgd()
{
    if (not isRestart_ and isInit_) {
        changeState(NatPmpState::PMP_SEARCH);
    }

    std::lock_guard<std::mutex> lk(validIgdMutex_);
    pmpIgd_->renewal_ = clock::now();

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
    if (getLocalHostName(localHostBuf, INET_ADDRSTRLEN) < 0) {
        JAMI_WARN("NAT-PMP: Couldn't find local host");
        JAMI_DBG("NAT-PMP: Attempting to initialize with unknown gateway");
        err = initnatpmp(&natpmpHdl_, 0, 0);
    } else {
        std::string gw = getGateway(localHostBuf);
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
        pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(1);
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
            pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(5);
            return false;
        } else if (responseErr != NATPMP_TRYAGAIN) {
            pmpIgd_->localIp_ = ip_utils::getLocalAddr(AF_INET);
            pmpIgd_->publicIp_ = IpAddr(response.pnu.publicaddress.addr);
            return true;
        }
        retry++;
    }
    pmpIgd_->renewal_ = clock::now() + std::chrono::minutes(5);
    return false;
}

bool
NatPmp::addPortMapping(Mapping map)
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
NatPmp::removePortMapping(Mapping map)
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
    if (pmpIgd_) {
        decltype(pmpIgd_->mapToRenewList_) renew = std::move(pmpIgd_->mapToRenewList_);
        for (auto& m : renew) {
            if (pmpIgd_->isMapUpForRenewal(m, now)) {
                return true;
            }
        }
    }
    return false;
}

bool
NatPmp::isIgdUpForRenewal(const time_point& now)
{
    if (pmpIgd_) {
        if (pmpIgd_->renewal_ < now)
            return true;
    }
    return false;
}

void
NatPmp::clearNatPmpHdl(natpmp_t& hdl)
{
    memset(&natpmpHdl_.s,                   0, sizeof(natpmpHdl_.s));
    memset(&natpmpHdl_.gateway,             0, sizeof(in_addr_t));
    memset(&natpmpHdl_.has_pending_request, 0, sizeof(natpmpHdl_.has_pending_request));
    memset(&natpmpHdl_.pending_request,     0, sizeof(natpmpHdl_.pending_request));
    memset(&natpmpHdl_.pending_request_len, 0, sizeof(natpmpHdl_.pending_request_len));
    memset(&natpmpHdl_.try_number,          0, sizeof(natpmpHdl_.try_number));
    memset(&natpmpHdl_.retry_time.tv_sec,   0, sizeof(natpmpHdl_.retry_time.tv_sec));
    memset(&natpmpHdl_.retry_time.tv_usec,  0, sizeof(natpmpHdl_.retry_time.tv_usec));
}

std::string
NatPmp::getGateway(char* localHost)
{
    std::string defaultGw {};

    std::string localHostStr(localHost);
    std::istringstream iss(localHostStr);

    // Make a vector of each individual number in the ip address.
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(iss, token, '.')) {
        if (!token.empty())
            tokens.push_back(token);
    }

    // Build a gateway address from the individual ip components.
    for (unsigned int i = 0; i <=2; i++)
        defaultGw += tokens[i] + ".";
    defaultGw += "1";     // x.x.x.1

    return defaultGw;
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