/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#if HAVE_LIBNATPMP

namespace jami {
namespace upnp {

NatPmp::NatPmp()
{
    JAMI_DBG("NAT-PMP: Instance [%p] created", this);
    runOnNatPmpQueue([this] {
        threadId_ = getCurrentThread();
        igd_ = std::make_shared<PMPIGD>();
    });
}

NatPmp::~NatPmp()
{
    JAMI_DBG("NAT-PMP: Instance [%p] destroyed", this);
}

void
NatPmp::initNatPmp()
{
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak()] {
            if (auto pmpThis = w.lock()) {
                pmpThis->initNatPmp();
            }
        });
        return;
    }

    initialized_ = false;

    {
        std::lock_guard<std::mutex> lock(natpmpMutex_);
        hostAddress_ = ip_utils::getLocalAddr(AF_INET);
    }

    // Local address must be valid.
    if (not getHostAddress() or getHostAddress().isLoopback()) {
        JAMI_WARN("NAT-PMP: Does not have a valid local address!");
        return;
    }

    assert(igd_);
    if (igd_->isValid()) {
        igd_->setValid(false);
        processIgdUpdate(UpnpIgdEvent::REMOVED);
    }

    igd_->setLocalIp(IpAddr());
    igd_->setPublicIp(IpAddr());
    igd_->setUID("");

    JAMI_DBG("NAT-PMP: Trying to initialize IGD");

    int err = initnatpmp(&natpmpHdl_, 0, 0);

    if (err < 0) {
        JAMI_WARN("NAT-PMP: Initializing IGD using default gateway failed!");
        const auto& localGw = ip_utils::getLocalGateway();
        if (not localGw) {
            JAMI_WARN("NAT-PMP: Couldn't find valid gateway on local host");
            err = NATPMP_ERR_CANNOTGETGATEWAY;
        } else {
            JAMI_WARN("NAT-PMP: Trying to initialize using detected gateway %s",
                      localGw.toString().c_str());

            struct in_addr inaddr;
            inet_pton(AF_INET, localGw.toString().c_str(), &inaddr);
            err = initnatpmp(&natpmpHdl_, 1, inaddr.s_addr);
        }
    }

    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err));
        return;
    }

    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &natpmpHdl_.gateway, addrbuf, sizeof(addrbuf));
    IpAddr igdAddr(addrbuf);
    JAMI_DBG("NAT-PMP: Initialized on gateway %s", igdAddr.toString().c_str());

    // Set the local (gateway) address.
    igd_->setLocalIp(igdAddr);
    // NAT-PMP protocol does not have UID, but we will set generic
    // one debugging purposes.
    igd_->setUID("NAT-PMP Gateway");

    // Search and set the public address.
    getIgdPublicAddress();

    // Update and notify.
    if (igd_->isValid()) {
        initialized_ = true;
        processIgdUpdate(UpnpIgdEvent::ADDED);
    };
}

void
NatPmp::setObserver(UpnpMappingObserver* obs)
{
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak(), obs] {
            if (auto pmpThis = w.lock()) {
                pmpThis->setObserver(obs);
            }
        });
        return;
    }

    JAMI_DBG("NAT-PMP: Setting observer to %p", obs);

    observer_ = obs;
}

void
NatPmp::terminate(std::condition_variable& cv)
{
    initialized_ = false;
    observer_ = nullptr;

    {
        std::lock_guard<std::mutex> lock(natpmpMutex_);
        shutdownComplete_ = true;
        cv.notify_one();
    }
}

void
NatPmp::terminate()
{
    std::unique_lock<std::mutex> lk(natpmpMutex_);
    std::condition_variable cv {};

    runOnNatPmpQueue([w = weak(), &cv = cv] {
        if (auto pmpThis = w.lock()) {
            pmpThis->terminate(cv);
        }
    });

    if (cv.wait_for(lk, std::chrono::seconds(10), [this] { return shutdownComplete_; })) {
        JAMI_DBG("NAT-PMP: Shutdown completed");
    } else {
        JAMI_ERR("NAT-PMP: Shutdown timed-out");
    }
}

const IpAddr
NatPmp::getHostAddress() const
{
    std::lock_guard<std::mutex> lock(natpmpMutex_);
    return hostAddress_;
}

void
NatPmp::clearIgds()
{
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak()] {
            if (auto pmpThis = w.lock()) {
                pmpThis->clearIgds();
            }
        });
        return;
    }

    bool do_close = false;

    if (igd_) {
        if (igd_->isValid()) {
            do_close = true;
        }
        igd_->setValid(false);
    }

    initialized_ = false;
    if (searchForIgdTimer_)
        searchForIgdTimer_->cancel();

    igdSearchCounter_ = 0;

    if (do_close) {
        closenatpmp(&natpmpHdl_);
        memset(&natpmpHdl_, 0, sizeof(natpmpHdl_));
    }
}

void
NatPmp::searchForIgd()
{
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak()] {
            if (auto pmpThis = w.lock()) {
                pmpThis->searchForIgd();
            }
        });
        return;
    }

    if (not initialized_) {
        initNatPmp();
    }

    // Schedule a retry in case init failed.
    if (not initialized_) {
        if (igdSearchCounter_++ < MAX_RESTART_SEARCH_RETRIES) {
            JAMI_DBG("NAT-PMP: Start search for IGDs. Attempt %i", igdSearchCounter_);

            // Cancel the current timer (if any) and re-schedule.
            if (searchForIgdTimer_)
                searchForIgdTimer_->cancel();

            searchForIgdTimer_ = getNatpmpScheduler()->scheduleIn([this] { searchForIgd(); },
                                                                  NATPMP_SEARCH_RETRY_UNIT
                                                                      * igdSearchCounter_);
        } else {
            JAMI_WARN("NAT-PMP: Setup failed after %u trials. NAT-PMP will be disabled!",
                      MAX_RESTART_SEARCH_RETRIES);
        }
    }
}

std::list<std::shared_ptr<IGD>>
NatPmp::getIgdList() const
{
    std::lock_guard<std::mutex> lock(natpmpMutex_);
    std::list<std::shared_ptr<IGD>> igdList;
    if (igd_->isValid())
        igdList.emplace_back(igd_);
    return igdList;
}

bool
NatPmp::isReady() const
{
    if (observer_ == nullptr) {
        JAMI_ERR("NAT-PMP: the observer is not set!");
        return false;
    }

    // Must at least have a valid local address.
    if (not getHostAddress() or getHostAddress().isLoopback())
        return false;

    return igd_ and igd_->isValid();
}

void
NatPmp::incrementErrorsCounter(const std::shared_ptr<IGD>& igdIn)
{
    if (not validIgdInstance(igdIn)) {
        return;
    }

    if (not igd_->isValid()) {
        // Already invalid. Nothing to do.
        return;
    }

    if (not igd_->incrementErrorsCounter()) {
        // Disable this IGD.
        igd_->setValid(false);
        // Notify the listener.
        JAMI_WARN("NAT-PMP: No more valid IGD!");

        processIgdUpdate(UpnpIgdEvent::INVALID_STATE);
    }
}

void
NatPmp::requestMappingAdd(const Mapping& mapping)
{
    // Process on nat-pmp thread.
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak(), mapping] {
            if (auto pmpThis = w.lock()) {
                pmpThis->requestMappingAdd(mapping);
            }
        });
        return;
    }

    Mapping map(mapping);
    assert(map.getIgd());
    auto err = addPortMapping(map);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Request for mapping %s on %s failed with error %i: %s",
                  map.toString().c_str(),
                  igd_->toString().c_str(),
                  err,
                  getNatPmpErrorStr(err));

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd_);
        }
        // Notify the listener.
        processMappingRequestFailed(std::move(map));
    } else {
        JAMI_DBG("NAT-PMP: Request for mapping %s on %s succeeded",
                 map.toString().c_str(),
                 igd_->toString().c_str());
        // Notify the listener.
        processMappingAdded(std::move(map));
    }
}

void
NatPmp::requestMappingRenew(const Mapping& mapping)
{
    // Process on nat-pmp thread.
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak(), mapping] {
            if (auto pmpThis = w.lock()) {
                pmpThis->requestMappingRenew(mapping);
            }
        });
        return;
    }

    Mapping map(mapping);
    auto err = addPortMapping(map);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Renewal request for mapping %s on %s failed with error %i: %s",
                  map.toString().c_str(),
                  igd_->toString().c_str(),
                  err,
                  getNatPmpErrorStr(err));
        // Notify the listener.
        processMappingRequestFailed(std::move(map));

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd_);
        }
    } else {
        JAMI_DBG("NAT-PMP: Renewal request for mapping %s on %s succeeded",
                 map.toString().c_str(),
                 igd_->toString().c_str());
        // Notify the listener.
        processMappingRenewed(map);
    }
}

int
NatPmp::readResponse(natpmp_t& handle, natpmpresp_t& response)
{
    int err = 0;
    unsigned readRetriesCounter = 0;

    while (true) {
        if (readRetriesCounter++ > MAX_READ_RETRIES) {
            err = NATPMP_ERR_SOCKETERROR;
            break;
        }

        fd_set fds;
        struct timeval timeout;
        FD_ZERO(&fds);
        FD_SET(handle.s, &fds);
        getnatpmprequesttimeout(&handle, &timeout);
        // Wait for data.
        if (select(FD_SETSIZE, &fds, NULL, NULL, &timeout) == -1) {
            err = NATPMP_ERR_SOCKETERROR;
            break;
        }

        // Read the data.
        err = readnatpmpresponseorretry(&handle, &response);

        if (err == NATPMP_TRYAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_BEFORE_READ_RETRY));
        } else {
            break;
        }
    }

    return err;
}

int
NatPmp::sendMappingRequest(const Mapping& mapping, uint32_t& lifetime)
{
    CHECK_VALID_THREAD();

    int err = sendnewportmappingrequest(&natpmpHdl_,
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP
                                                                           : NATPMP_PROTOCOL_TCP,
                                        mapping.getInternalPort(),
                                        mapping.getExternalPort(),
                                        lifetime);

    if (err < 0) {
        JAMI_ERR("NAT-PMP: Send mapping request failed with error %s %i",
                 getNatPmpErrorStr(err),
                 errno);
        return err;
    }

    unsigned readRetriesCounter = 0;

    while (readRetriesCounter++ < MAX_READ_RETRIES) {
        // Read the response
        natpmpresp_t response;
        err = readResponse(natpmpHdl_, response);

        if (err < 0) {
            JAMI_WARN("NAT-PMP: Read response on IGD %s failed with error %s",
                      igd_->toString().c_str(),
                      getNatPmpErrorStr(err));
        } else if (response.type != NATPMP_RESPTYPE_TCPPORTMAPPING
                   and response.type != NATPMP_RESPTYPE_UDPPORTMAPPING) {
            JAMI_ERR("NAT-PMP: Unexpected response type (%i) for mapping %s from IGD %s.",
                     response.type,
                     mapping.toString().c_str(),
                     igd_->toString().c_str());
            // Try to read again.
            continue;
        }

        lifetime = response.pnu.newportmapping.lifetime;
        // Done.
        break;
    }

    return err;
}

int
NatPmp::addPortMapping(Mapping& mapping)
{
    auto const& igdIn = mapping.getIgd();
    assert(igdIn);
    assert(igdIn->getProtocol() == NatProtocolType::NAT_PMP);

    if (not igdIn->isValid() or not validIgdInstance(igdIn)) {
        mapping.setState(MappingState::FAILED);
        return NATPMP_ERR_INVALIDARGS;
    }

    mapping.setInternalAddress(getHostAddress().toString());

    uint32_t lifetime = MAPPING_ALLOCATION_LIFETIME;
    int err = sendMappingRequest(mapping, lifetime);

    if (err < 0) {
        mapping.setState(MappingState::FAILED);
        return err;
    }

    // Set the renewal time and update.
    mapping.setRenewalTime(sys_clock::now() + std::chrono::seconds(lifetime * 4 / 5));
    mapping.setState(MappingState::OPEN);

    return 0;
}

void
NatPmp::requestMappingRemove(const Mapping& mapping)
{
    // Process on nat-pmp thread.
    if (not isValidThread()) {
        runOnNatPmpQueue([w = weak(), mapping] {
            if (auto pmpThis = w.lock()) {
                Mapping map {mapping};
                pmpThis->removePortMapping(map);
            }
        });
        return;
    }
}

void
NatPmp::removePortMapping(Mapping& mapping)
{
    auto igdIn = mapping.getIgd();
    assert(igdIn);
    if (not igdIn->isValid()) {
        return;
    }

    if (not validIgdInstance(igdIn)) {
        return;
    }

    Mapping mapToRemove(mapping);

    uint32_t lifetime = 0;
    int err = sendMappingRequest(mapping, lifetime);

    if (err < 0) {
        // Nothing to do if the request fails, just log the error.
        JAMI_WARN("NAT-PMP: Send remove request failed with error %s. Ignoring",
                  getNatPmpErrorStr(err));
    }

    // Update and notify the listener.
    mapToRemove.setState(MappingState::FAILED);
    processMappingRemoved(std::move(mapToRemove));
}

void
NatPmp::getIgdPublicAddress()
{
    CHECK_VALID_THREAD();

    // Set the public address for this IGD if it does not
    // have one already.
    if (igd_->getPublicIp()) {
        JAMI_WARN("NAT-PMP: IGD %s already have a public address (%s)",
                  igd_->toString().c_str(),
                  igd_->getPublicIp().toString().c_str());
        return;
    }
    assert(igd_->getProtocol() == NatProtocolType::NAT_PMP);

    int err = sendpublicaddressrequest(&natpmpHdl_);

    if (err < 0) {
        JAMI_ERR("NAT-PMP: send public address request on IGD %s failed with error: %s",
                 igd_->toString().c_str(),
                 getNatPmpErrorStr(err));

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd_);
        }
        return;
    }

    natpmpresp_t response;
    err = readResponse(natpmpHdl_, response);

    if (err < 0) {
        JAMI_WARN("NAT-PMP: Read response on IGD %s failed - %s",
                  igd_->toString().c_str(),
                  getNatPmpErrorStr(err));
        return;
    }

    if (response.type != NATPMP_RESPTYPE_PUBLICADDRESS) {
        JAMI_ERR("NAT-PMP: Unexpected response type (%i) for public address request from IGD %s.",
                 response.type,
                 igd_->toString().c_str());
        return;
    }

    IpAddr publicAddr(response.pnu.publicaddress.addr);

    if (not publicAddr) {
        JAMI_ERR("NAT-PMP: IGD %s returned an invalid public address %s",
                 igd_->toString().c_str(),
                 publicAddr.toString().c_str());
    }

    // Update.
    igd_->setPublicIp(publicAddr);
    igd_->setValid(true);

    JAMI_DBG("NAT-PMP: Setting IGD %s public address to %s",
             igd_->toString().c_str(),
             igd_->getPublicIp().toString().c_str());
}

void
NatPmp::removeAllMappings()
{
    CHECK_VALID_THREAD();

    JAMI_WARN("NAT-PMP: Send request to close all existing mappings to IGD %s",
              igd_->toString().c_str());

    int err = sendnewportmappingrequest(&natpmpHdl_, NATPMP_PROTOCOL_TCP, 0, 0, 0);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Send close all TCP mappings request failed with error %s",
                  getNatPmpErrorStr(err));
    }
    err = sendnewportmappingrequest(&natpmpHdl_, NATPMP_PROTOCOL_UDP, 0, 0, 0);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Send close all UDP mappings request failed with error %s",
                  getNatPmpErrorStr(err));
    }
}

const char*
NatPmp::getNatPmpErrorStr(int errorCode) const
{
#ifdef ENABLE_STRNATPMPERR
    return strnatpmperr(errorCode);
#else
    switch (errorCode) {
    case NATPMP_ERR_INVALIDARGS:
        return "INVALIDARGS";
        break;
    case NATPMP_ERR_SOCKETERROR:
        return "SOCKETERROR";
        break;
    case NATPMP_ERR_CANNOTGETGATEWAY:
        return "CANNOTGETGATEWAY";
        break;
    case NATPMP_ERR_CLOSEERR:
        return "CLOSEERR";
        break;
    case NATPMP_ERR_RECVFROM:
        return "RECVFROM";
        break;
    case NATPMP_ERR_NOPENDINGREQ:
        return "NOPENDINGREQ";
        break;
    case NATPMP_ERR_NOGATEWAYSUPPORT:
        return "NOGATEWAYSUPPORT";
        break;
    case NATPMP_ERR_CONNECTERR:
        return "CONNECTERR";
        break;
    case NATPMP_ERR_WRONGPACKETSOURCE:
        return "WRONGPACKETSOURCE";
        break;
    case NATPMP_ERR_SENDERR:
        return "SENDERR";
        break;
    case NATPMP_ERR_FCNTLERROR:
        return "FCNTLERROR";
        break;
    case NATPMP_ERR_GETTIMEOFDAYERR:
        return "GETTIMEOFDAYERR";
        break;
    case NATPMP_ERR_UNSUPPORTEDVERSION:
        return "UNSUPPORTEDVERSION";
        break;
    case NATPMP_ERR_UNSUPPORTEDOPCODE:
        return "UNSUPPORTEDOPCODE";
        break;
    case NATPMP_ERR_UNDEFINEDERROR:
        return "UNDEFINEDERROR";
        break;
    case NATPMP_ERR_NOTAUTHORIZED:
        return "NOTAUTHORIZED";
        break;
    case NATPMP_ERR_NETWORKFAILURE:
        return "NETWORKFAILURE";
        break;
    case NATPMP_ERR_OUTOFRESOURCES:
        return "OUTOFRESOURCES";
        break;
    case NATPMP_TRYAGAIN:
        return "TRYAGAIN";
        break;
    default:
        return "UNKNOWNERR";
        break;
    }
#endif
}

bool
NatPmp::isErrorFatal(int error)
{
    switch (error) {
    case NATPMP_ERR_INVALIDARGS:
    case NATPMP_ERR_SOCKETERROR:
    case NATPMP_ERR_CANNOTGETGATEWAY:
    case NATPMP_ERR_CLOSEERR:
    case NATPMP_ERR_RECVFROM:
    case NATPMP_ERR_NOGATEWAYSUPPORT:
    case NATPMP_ERR_CONNECTERR:
    case NATPMP_ERR_SENDERR:
    case NATPMP_ERR_UNDEFINEDERROR:
    case NATPMP_ERR_UNSUPPORTEDVERSION:
    case NATPMP_ERR_UNSUPPORTEDOPCODE:
    case NATPMP_ERR_NOTAUTHORIZED:
    case NATPMP_ERR_NETWORKFAILURE:
    case NATPMP_ERR_OUTOFRESOURCES:
        return true;
    default:
        return false;
    }
}

bool
NatPmp::validIgdInstance(const std::shared_ptr<IGD>& igdIn)
{
    if (igd_.get() != igdIn.get()) {
        JAMI_ERR("NAT-PMP: IGD (%s) does not match local instance (%s)",
                 igdIn->toString().c_str(),
                 igd_->toString().c_str());
        return false;
    }

    return true;
}

void
NatPmp::processIgdUpdate(UpnpIgdEvent event)
{
    if (igd_->isValid()) {
        // Remove all current mappings if any.
        removeAllMappings();
    }

    if (observer_ == nullptr)
        return;
    // Process the response on the context thread.
    runOnUpnpContextQueue([obs = observer_, igd = igd_, event] { obs->onIgdUpdated(igd, event); });
}

void
NatPmp::processMappingAdded(const Mapping& map)
{
    if (observer_ == nullptr)
        return;

    // Process the response on the context thread.
    runOnUpnpContextQueue([obs = observer_, igd = igd_, map] { obs->onMappingAdded(igd, map); });
}

void
NatPmp::processMappingRequestFailed(const Mapping& map)
{
    if (observer_ == nullptr)
        return;

    // Process the response on the context thread.
    runOnUpnpContextQueue([obs = observer_, igd = igd_, map] { obs->onMappingRequestFailed(map); });
}

void
NatPmp::processMappingRenewed(const Mapping& map)
{
    if (observer_ == nullptr)
        return;

    // Process the response on the context thread.
    runOnUpnpContextQueue([obs = observer_, igd = igd_, map] { obs->onMappingRenewed(igd, map); });
}

void
NatPmp::processMappingRemoved(const Mapping& map)
{
    if (observer_ == nullptr)
        return;

    // Process the response on the context thread.
    runOnUpnpContextQueue([obs = observer_, igd = igd_, map] { obs->onMappingRemoved(igd, map); });
}

} // namespace upnp
} // namespace jami

#endif //-- #if HAVE_LIBNATPMP
