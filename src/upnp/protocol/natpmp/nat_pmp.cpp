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
#include "opendht/thread_pool.h"

namespace jami {
namespace upnp {

NatPmp::NatPmp()
{
    getNatpmpScheduler()->run([this] { threadId_ = getCurrentThread(); });
}

NatPmp::~NatPmp()
{
    clearIgds();
}

void
NatPmp::initNatPmp()
{
    if (not isValidThread()) {
        getNatpmpScheduler()->run([this] { initNatPmp(); });
        return;
    }

    userLocalIp_ = ip_utils::getLocalAddr(pj_AF_INET());

    initialized_ = false;
    int err = 0;
    auto newIgd = std::make_shared<PMPIGD>();
    const auto& localGw = ip_utils::getLocalGateway();

    if (not localGw) {
        JAMI_WARN("NAT-PMP: Couldn't find valid gateway on local host");
        err = NATPMP_ERR_CANNOTGETGATEWAY;
        return;
    }

    JAMI_DBG("NAT-PMP: Try to initialize IGD on gateway %s", localGw.toString().c_str());
    struct in_addr inaddr;
    inet_pton(AF_INET, localGw.toString().c_str(), &inaddr);
    err = initnatpmp(&newIgd->getHandle(), 1, inaddr.s_addr);

    // Reject if the address does not match.
    if (not matchLocalGateway(localGw, newIgd->getHandle().gateway)) {
        err = NATPMP_ERR_CANNOTGETGATEWAY;
    }

    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err));
        return;
    }

    initialized_ = true;

    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &newIgd->getHandle().gateway, addrbuf, sizeof(addrbuf));
    IpAddr igdAddr(addrbuf);
    JAMI_DBG("NAT-PMP: Initialized on gateway %s", igdAddr.toString().c_str());

    // Only set the local (gateway) address. Public address will be set when
    // performing IGD search.
    newIgd->setLocalIp(igdAddr);
    // Not valid yet.
    newIgd->setValid(false);
    // Add the new IGD.
    addIgd(newIgd);
}

void
NatPmp::setObserver(UpnpMappingObserver* obs)
{
    JAMI_DBG("NAT-PMP: Setting observer to %p", obs);
    observer_ = obs;
}

void
NatPmp::clearIgds()
{
    if (not isValidThread()) {
        getNatpmpScheduler()->run([this] { clearIgds(); });
        return;
    }

    initialized_ = false;
    searchForIgdTimer_->cancel();
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        assert(igd);
        closenatpmp(&igd->getHandle());
        igd->clearNatPmpHdl();
    }

    igdList_.clear();

    for (auto const& igd : igdBlackList_) {
        assert(igd);
        closenatpmp(&igd->getHandle());
        igd->clearNatPmpHdl();
    }

    igdBlackList_.clear();
}

void
NatPmp::searchForIgd()
{
    if (not isValidThread()) {
        getNatpmpScheduler()->run([this] { searchForIgd(); });
        return;
    }

    if (not initialized_) {
        initNatPmp();
    }
    //
    if (initialized_ and not hasValidIgd()) {
        std::unique_lock<std::mutex> lock(igdListMutex_);
        auto list = igdList_;
        lock.unlock();
        for (auto const& igd : list) {
            searchForPmpIgd(igd);
        }
    }

    int idx = 0;
    for (auto const& igd : igdList_) {
        JAMI_DBG("NAT-PMP: IGD %i: [%s] valid %s",
                 idx,
                 igd->getLocalIp().toString().c_str(),
                 igd->isValid() ? "YES" : "NO");
        idx++;
    }

    JAMI_DBG("NAT-PMP: Current top IGD [%s] valid %s",
             igdList_.front()->getLocalIp().toString().c_str(),
             igdList_.front()->isValid() ? "YES" : "NO");

    // Cancel the current timer (if any) and re-schedule.
    if (searchForIgdTimer_)
        searchForIgdTimer_->cancel();

    searchForIgdTimer_ = getNatpmpScheduler()->scheduleIn([this] { searchForIgd(); },
                                                          IGD_RENEWAL_INTERVAL);
}

void
NatPmp::getIgdList(std::list<std::shared_ptr<IGD>>& igdList) const
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igd->isValid())
            igdList.emplace_back(igd);
    }
}

bool
NatPmp::hasValidIgd() const
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igd->isValid()) {
            return true;
        }
    }
    return false;
}

void
NatPmp::incrementErrorsCounter(const std::shared_ptr<IGD>& igdIn)
{
    auto igd = getIgdInstance(igdIn);
    if (not igd) {
        JAMI_ERR("NAT-PMP: IGD on address %s does not have a match in local list",
                 igdIn->getLocalIp().toString().c_str());
        return;
    }

    if (not igd->isValid()) {
        // Already invalid. Nothing to do.
        return;
    }

    if (not igd->incrementErrorsCounter()) {
        // Disable this IGD.
        igd->setValid(false);
        // Notify the listener.
        processIgdUpdate(igd, UpnpIgdEvent::INVALID_STATE);

        // Move to the blacklist.
        {
            std::lock_guard<std::mutex> lock(igdListMutex_);
            auto it = std::find(igdList_.begin(), igdList_.end(), igd);
            if (it != igdList_.end()) {
                igdList_.splice(igdBlackList_.end(), igdList_, it);
            }
            JAMI_DBG("NAT-PMP: Moved invalid IGD %s to blacklist",
                     igd->getLocalIp().toString().c_str());
        }
        // Log the new top IGD if any
        if (auto topIgd = getIgd()) {
            JAMI_DBG("NAT-PMP: New top IGD %s", topIgd->getLocalIp().toString().c_str());
        } else {
            JAMI_WARN("NAT-PMP: No more valid IGD!");
        }
    }
}

void
NatPmp::requestMappingAdd(const std::shared_ptr<IGD>& igd, const Mapping& mapping)
{
    // Process on nat-pmp thread.
    getNatpmpScheduler()->run([this, igd, mapping] {
        JAMI_DBG("NAT-PMP: Request mapping %s on %s",
                 mapping.toString().c_str(),
                 igd->getLocalIp().toString().c_str());

        Mapping map {mapping};
        addPortMapping(igd, map, false);
    });
}

void
NatPmp::requestMappingRenew(const Mapping& mapping)
{
    assert(mapping.getIgd());

    // Process on nat-pmp thread.
    getNatpmpScheduler()->run([this, mapping] {
        JAMI_DBG("NAT-PMP: Renew mapping %s on %s",
                 mapping.toString().c_str(),
                 mapping.getIgd()->getLocalIp().toString().c_str());

        Mapping map {mapping};
        addPortMapping(mapping.getIgd(), map, true);
    });
}

int
NatPmp::readResponse(natpmp_t& handle, natpmpresp_t& response)
{
    int err = 0;
    unsigned readRetriesCounter = 0;

    while (readRetriesCounter++ < MAX_READ_RETRIES) {
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
NatPmp::sendMappingRequest(const std::shared_ptr<PMPIGD>& igd,
                           const Mapping& mapping,
                           uint32_t& lifetime)
{
    int err = sendnewportmappingrequest(&igd->getHandle(),
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
        natpmpresp_t response;
        // Read the response
        err = readResponse(igd->getHandle(), response);

        if (err < 0) {
            JAMI_WARN("NAT-PMP: Read response on IGD %s failed with error %s",
                      igd->getLocalIp().toString().c_str(),
                      getNatPmpErrorStr(err));
        } else if (response.type != NATPMP_RESPTYPE_TCPPORTMAPPING
                   and response.type != NATPMP_RESPTYPE_UDPPORTMAPPING) {
            JAMI_ERR("NAT-PMP: Unexpected response type (%i) for mapping %s from IGD %s.",
                     response.type,
                     mapping.toString().c_str(),
                     igd->getLocalIp().toString().c_str());
            // Try to read again.
            continue;
        }

        lifetime = response.pnu.newportmapping.lifetime;
        // Done.
        break;
    }

    return err;
}

void
NatPmp::addPortMapping(const std::shared_ptr<IGD>& igdIn, Mapping& mapping, bool renew)
{
    assert(igdIn);
    assert(igdIn->getProtocol() == NatProtocolType::NAT_PMP);

    if (not igdIn->isValid())
        return;

    // Convert pointer.
    auto igd = getIgdInstance(igdIn);

    if (not igd) {
        JAMI_ERR("NAT-PMP: IGD on address %s does not have a match in local list",
                 igdIn->getLocalIp().toString().c_str());
        return;
    }

    if (not igd->isValid()) {
        JAMI_WARN("NAT-PMP: IGD on address %s is invalid (black listed)",
                  igdIn->getLocalIp().toString().c_str());
        return;
    }

    Mapping mapToAdd(mapping.getExternalPort(),
                     mapping.getInternalPort(),
                     mapping.getType() == PortType::UDP ? upnp::PortType::UDP : upnp::PortType::TCP);
    mapToAdd.setExternalAddress(igd->getPublicIp().toString());
    mapToAdd.setInternalAddress(getUserLocalIp().toString());
    mapToAdd.setIgd(igd);

    uint32_t lifetime = MAPPING_ALLOCATION_LIFETIME;
    int err = sendMappingRequest(igd, mapping, lifetime);

    if (err < 0) {
        JAMI_ERR("NAT-PMP: Add mapping request failed with error %s %i",
                 getNatPmpErrorStr(err),
                 errno);

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd);
        }
        // Mark as failed and notify.
        mapToAdd.setState(MappingState::FAILED);
        processMappingAdded(igd, std::move(mapToAdd));
    } else {
        // Success! Set renewal and update.
        mapToAdd.setLifeTime(lifetime);
        mapToAdd.setState(MappingState::OPEN);
        if (not renew) {
            JAMI_DBG("NAT-PMP: Allocated mapping %s on %s",
                     mapToAdd.toString().c_str(),
                     igd->getLocalIp().toString().c_str());
            // Notify the listener.
            processMappingAdded(igd, std::move(mapToAdd));
        } else {
            JAMI_DBG("NAT-PMP: Renewed mapping %s on %s",
                     mapToAdd.toString().c_str(),
                     igd->getLocalIp().toString().c_str());
        }
    }
}

void
NatPmp::requestMappingRemove(const Mapping& mapping)
{
    // Process on nat-pmp thread.
    getNatpmpScheduler()->run([this, mapping] {
        Mapping map {mapping};
        removePortMapping(map);
    });
}

void
NatPmp::removePortMapping(Mapping& mapping)
{
    auto igdIn = mapping.getIgd();
    assert(igdIn);
    if (not igdIn->isValid()) {
        return;
    }
    auto igd = getIgdInstance(igdIn);

    Mapping mapToRemove(mapping.getExternalPort(),
                        mapping.getInternalPort(),
                        mapping.getType() == PortType::UDP ? upnp::PortType::UDP
                                                           : upnp::PortType::TCP);

    uint32_t lifetime = 0;
    int err = sendMappingRequest(igd, mapping, lifetime);

    if (err < 0) {
        JAMI_WARN("NAT-PMP: Send remove request failed with error %s. Ignoring",
                  getNatPmpErrorStr(err));
    }

    // Update and notify the listener.
    mapToRemove.setState(MappingState::FAILED);
    processMappingRemoved(igd, std::move(mapToRemove));
}

void
NatPmp::searchForPmpIgd(const std::shared_ptr<PMPIGD>& igd)
{
    assert(igd);
    assert(igd->getProtocol() == NatProtocolType::NAT_PMP);

    int err = sendpublicaddressrequest(&igd->getHandle());

    if (err < 0) {
        JAMI_ERR("NAT-PMP: send public address request on IGD %s failed with error: %s",
                 igd->getLocalIp().toString().c_str(),
                 getNatPmpErrorStr(err));

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd);
        }
        return;
    }

    natpmpresp_t response;
    err = readResponse(igd->getHandle(), response);

    if (err < 0) {
        JAMI_ERR("NAT-PMP: read response on IGD %s failed with error %s",
                 igd->getLocalIp().toString().c_str(),
                 getNatPmpErrorStr(err));
        return;
    }

    if (response.type != NATPMP_RESPTYPE_PUBLICADDRESS) {
        JAMI_ERR("NAT-PMP: Unexpected response type (%i) for public address request from IGD %s.",
                 response.type,
                 igd->getLocalIp().toString().c_str());
        return;
    }

    IpAddr igdAddr(response.pnu.publicaddress.addr);
    // Set the public address for this IGD if it does not
    // have one already.
    if (not igd->getPublicIp()) {
        igd->setPublicIp(igdAddr);
        assert(igd->getLocalIp());
        // Update.
        igd->setValid(true);
        // Report to the listener.
        processIgdUpdate(igd, UpnpIgdEvent::ADDED);
        return;
    }

    // Check if it's a new IGD.
    auto newIgd = std::make_shared<PMPIGD>(*igd);
    newIgd->setPublicIp(igdAddr);
    assert(newIgd->getLocalIp());

    if (isNewIgd(newIgd)) {
        // Add the new IGD.
        addIgd(newIgd);
        JAMI_ERR("NAT-PMP: Found new device with external IP %s", igdAddr.toString(true).c_str());
        // Update.
        newIgd->setValid(true);
        // Report to the listener.
        processIgdUpdate(newIgd, UpnpIgdEvent::ADDED);
    } else {
        JAMI_DBG("NAT-PMP: IGD with public IP %s is already in the list",
                 igdAddr.toString(true).c_str());
    }
}

void
NatPmp::removeAllMappings(const std::shared_ptr<PMPIGD>& igd) const
{
    JAMI_WARN("NAT-PMP: Send request to close all existing mappings to IGD %s",
              igd->getLocalIp().toString().c_str());

    int err = sendnewportmappingrequest(&igd->getHandle(), NATPMP_PROTOCOL_TCP, 0, 0, 0);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Send close all TCP mappings request failed with error %s",
                  getNatPmpErrorStr(err));
    }
    err = sendnewportmappingrequest(&igd->getHandle(), NATPMP_PROTOCOL_UDP, 0, 0, 0);
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

std::shared_ptr<PMPIGD>
NatPmp::getIgd() const
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    if (igdList_.empty())
        return {};

    auto igd = igdList_.front();
    assert(igd);
    return igd;
}

bool
NatPmp::isNewIgd(std::shared_ptr<PMPIGD>& newIgd) const
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (newIgd->getPublicIp() == igd->getPublicIp()
            and newIgd->getLocalIp() == igd->getLocalIp()) {
            return false;
        }
    }

    for (auto const& igd : igdBlackList_) {
        if (newIgd->getPublicIp() == igd->getPublicIp()
            and newIgd->getLocalIp() == igd->getLocalIp()) {
            return false;
        }
    }

    return true;
}

void
NatPmp::addIgd(std::shared_ptr<PMPIGD> igd)
{
    if (not igd->getLocalIp()) {
        JAMI_ERR("NAT-PMP: Trying to add an IGD with invalid local address: %s",
                 igd->getLocalIp().toString().c_str());
    }

    std::lock_guard<std::mutex> lock(igdListMutex_);
    JAMI_DBG("NAT-PMP: Added new IGD %s", igd->getLocalIp().toString(true).c_str());
    igdList_.emplace_back(std::move(igd));
}

std::shared_ptr<PMPIGD>
NatPmp::getIgdInstance(const std::shared_ptr<IGD>& igdIn)
{
    std::lock_guard<std::mutex> lock(igdListMutex_);

    for (auto const& igd : igdList_) {
        if (igdIn == std::dynamic_pointer_cast<PMPIGD>(igd)) {
            return igd;
        }
    }

    for (auto const& igd : igdBlackList_) {
        if (igdIn == std::dynamic_pointer_cast<PMPIGD>(igd)) {
            JAMI_WARN("NAT-PMP: This IGD [%s] is invalid", igd->getLocalIp().toString().c_str());
            return igd;
        }
    }

    return {};
}

bool
NatPmp::matchLocalGateway(const IpAddr& localGw, in_addr_t gateway) const
{
    assert(localGw);

    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &gateway, addrbuf, sizeof(addrbuf));
    IpAddr igdAddr(addrbuf);

    if (localGw != igdAddr) {
        JAMI_WARN("NAT-PMP: IGD address %s does not match local gateway %s",
                  igdAddr.toString().c_str(),
                  localGw.toString().c_str());
        return false;
    }

    return true;
}

void
NatPmp::processIgdUpdate(const std::shared_ptr<PMPIGD>& igd, UpnpIgdEvent event)
{
    assert(igd->getProtocol() == NatProtocolType::NAT_PMP);

    if (igd->isValid()) {
        // Remove all current mappings if any.
        removeAllMappings(igd);
    }

    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, event] {
        assert(igd->getProtocol() == NatProtocolType::NAT_PMP);
        obs->onIgdUpdate(igd, event);
    });
}

void
NatPmp::processMappingAdded(const std::shared_ptr<PMPIGD>& igd, const Mapping& map)
{
    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, map] { obs->onMappingAdded(igd, map); });
}

void
NatPmp::processMappingRemoved(const std::shared_ptr<PMPIGD>& igd, const Mapping& map)
{
    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, map] { obs->onMappingRemoved(igd, map); });
}

} // namespace upnp
} // namespace jami
