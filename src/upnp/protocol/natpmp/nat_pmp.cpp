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

    initialized_ = false;
    int err = 0;
    auto newIgd = std::make_shared<PMPIGD>();
    auto localGw = getLocalGateway();

    if (not localGw or not *localGw) {
        JAMI_WARN("NAT-PMP: Couldn't find valid gateway on local host");
        err = NATPMP_ERR_CANNOTGETGATEWAY;
    } else {
        JAMI_DBG("NAT-PMP: Try to initialize IGD on gateway %s", localGw->toString().c_str());
        struct in_addr inaddr;
        inet_pton(AF_INET, localGw->toString().c_str(), &inaddr);
        err = initnatpmp(&getHandle(newIgd), 1, inaddr.s_addr);

        // Reject if the address does not match.
        if (not matchLocalGateway(localGw.get(), getHandle(newIgd).gateway)) {
            err = NATPMP_ERR_CANNOTGETGATEWAY;
        }
    }

    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't initialize libnatpmp -> %s", getNatPmpErrorStr(err));
    } else {
        char addrbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &getHandle(newIgd).gateway, addrbuf, sizeof(addrbuf));
        IpAddr igdAddr(addrbuf);
        JAMI_DBG("NAT-PMP: Initialized on gateway %s", igdAddr.toString().c_str());
        initialized_ = true;

        JAMI_DBG("NAT-PMP: Found new IGD %s", igdAddr.toString().c_str());
        // Update.
        newIgd->setPublicIp(igdAddr);
        newIgd->setLocalIp(ip_utils::getLocalAddr(AF_INET));
        newIgd->setValid(true);
        // Add the new IGD.
        addIgd(newIgd);
        // Report to the listener.
        processIgdUpdate(newIgd, UpnpIgdEvent::ADDED);
    }
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
        closenatpmp(&getHandle(igd));
        igd->clearNatPmpHdl();
    }

    igdList_.clear();

    for (auto const& igd : igdBlackList_) {
        assert(igd);
        closenatpmp(&getHandle(igd));
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

    if (initialized_) {
        std::unique_lock<std::mutex> lock(igdListMutex_);
        auto list = igdList_;
        lock.unlock();
        for (auto igd : list) {
            searchForPmpIgd(igd);
        }
    }

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
NatPmp::incrementErrorsCounter(const std::shared_ptr<IGD>& igd)
{
    if (not igd->isValid())
        return;
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
                     igd->getPublicIp().toString().c_str());
        }
        // Log the new top IGD if any
        if (auto topIgd = getIgd()) {
            JAMI_DBG("NAT-PMP: New top IGD %s", topIgd->getPublicIp().toString().c_str());
        } else {
            JAMI_WARN("NAT-PMP: No more valid IGD!");
        }
    }
}

void
NatPmp::requestMappingAdd(const std::shared_ptr<IGD>& igd, const Mapping& mapping)
{
    assert(igd);

    // Process on nat-pmp thread.
    getNatpmpScheduler()->run([this, igd, mapping] {
        JAMI_DBG("NAT-PMP: Request mapping %s on %s",
                 mapping.toString().c_str(),
                 igd->getPublicIp().toString().c_str());

        Mapping map {mapping};
        addPortMapping(igd, map, false);
    });
}

void
NatPmp::requestMappingRenew(const Mapping& mapping)
{
    assert(mapping);
    assert(mapping.getIgd());

    // Process on nat-pmp thread.
    getNatpmpScheduler()->run([this, mapping] {
        JAMI_DBG("NAT-PMP: Renew mapping %s on %s",
                 mapping.toString().c_str(),
                 mapping.getIgd()->getPublicIp().toString().c_str());

        Mapping map {mapping};
        addPortMapping(mapping.getIgd(), map, true);
    });
}

void
NatPmp::addPortMapping(const std::shared_ptr<IGD>& igdIn, Mapping& mapping, bool renew)
{
    assert(igdIn);

    // Convert pointer.
    auto igd = getIgdInstance(igdIn);

    if (not igd) {
        JAMI_ERR("NAT-PMP: IGD on address %s does not have a match in local list",
                 igdIn->getPublicIp().toString().c_str());
        return;
    }

    if (not igd->isValid()) {
        JAMI_WARN("NAT-PMP: IGD on address %s is invalid (black listed)",
                  igdIn->getPublicIp().toString().c_str());
        return;
    }

    Mapping mapToAdd(mapping.getExternalPort(),
                     mapping.getInternalPort(),
                     mapping.getType() == PortType::UDP ? upnp::PortType::UDP : upnp::PortType::TCP);
    mapToAdd.setExternalAddress(igd->getPublicIp().toString());
    mapToAdd.setInternalAddress(igd->getLocalIp().toString());
    mapToAdd.setIgd(igd);

    int err = sendnewportmappingrequest(&getHandle(igd),
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP
                                                                           : NATPMP_PROTOCOL_TCP,
                                        mapping.getInternalPort(),
                                        mapping.getExternalPort(),
                                        MAPPING_ALLOCATION_LIFETIME);
    if (err < 0) {
        JAMI_ERR("NAT-PMP: Can't send open port request -> %s %i", getNatPmpErrorStr(err), errno);

        mapToAdd.setInvalid();

        if (isErrorFatal(err)) {
            // Fatal error, increment the counter.
            incrementErrorsCounter(igd);
        }

        processMappingAdded(igd, std::move(mapToAdd));

    } else {
        unsigned readRetriesCount = 0;

        while (readRetriesCount++ < MAX_READ_RETRIES) {
            natpmpresp_t response;
            err = readnatpmpresponseorretry(&getHandle(igd), &response);

            if (err == NATPMP_TRYAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_BEFORE_READ_RETRY));
                continue;
            }

            if (err < 0) {
                JAMI_WARN("NAT-PMP: read response on IGD %s failed with error %s",
                          igd->getPublicIp().toString().c_str(),
                          getNatPmpErrorStr(err));

                if (isErrorFatal(err)) {
                    // Fatal error, increment the counter.
                    incrementErrorsCounter(igd);
                }
            } else {
                // Success! Set renewal and update.
                mapToAdd.setLifeTime(response.pnu.newportmapping.lifetime);
                if (not renew) {
                    JAMI_DBG("NAT-PMP: Allocated mapping %s on %s",
                             mapToAdd.toString().c_str(),
                             igd->getPublicIp().toString().c_str());
                    // Notify the listener.
                    processMappingAdded(igd, std::move(mapToAdd));
                } else {
                    JAMI_DBG("NAT-PMP: Renewed mapping %s on %s",
                             mapToAdd.toString().c_str(),
                             igd->getPublicIp().toString().c_str());
                }
            }
            // Done.
            break;
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
    auto igd = getIgd();
    if (not igd) {
        JAMI_WARN("NAT-PMP: No IGD available");
        return;
    }

    Mapping mapToRemove(mapping.getExternalPort(),
                        mapping.getInternalPort(),
                        mapping.getType() == PortType::UDP ? upnp::PortType::UDP
                                                           : upnp::PortType::TCP);
    int err = sendnewportmappingrequest(&getHandle(igd),
                                        mapping.getType() == PortType::UDP ? NATPMP_PROTOCOL_UDP
                                                                           : NATPMP_PROTOCOL_TCP,
                                        mapping.getInternalPort(),
                                        mapping.getExternalPort(),
                                        0);
    if (err < 0) {
        JAMI_WARN("NAT-PMP: Can't send close port request -> %s. Ignoring", getNatPmpErrorStr(err));
    } else {
        unsigned readRetriesCount = 0;
        while (readRetriesCount++ < MAX_READ_RETRIES) {
            natpmpresp_t response;
            err = readnatpmpresponseorretry(&getHandle(igd), &response);

            if (err == NATPMP_TRYAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_BEFORE_READ_RETRY));
                continue;
            }
            if (err < 0) {
                JAMI_WARN("NAT-PMP: remove request for %s failed. Unregistered the mapping anyway",
                          mapping.toString().c_str());
            }

            // Update and notify the listener.
            mapToRemove.setInvalid();
            processMappingRemoved(igd, std::move(mapToRemove));
        }
    }
}

void
NatPmp::searchForPmpIgd(const std::shared_ptr<PMPIGD>& igd)
{
    unsigned restartSearchRetry_ = 0;

    while (true) {
        int err = sendpublicaddressrequest(&getHandle(igd));

        if (err < 0) {
            JAMI_ERR("NAT-PMP: send public address request on IGD %s failed with error: %s",
                     igd->getPublicIp().toString().c_str(),
                     getNatPmpErrorStr(err));

            if (isErrorFatal(err)) {
                // Fatal error, increment the counter.
                incrementErrorsCounter(igd);
            }

            if (restartSearchRetry_++ <= MAX_RESTART_SEARCH_RETRIES) {
                // Trigger another search.
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_BEFORE_READ_RETRY));
                continue;
            } else {
                break;
            }
        } else {
            natpmpresp_t response;
            err = readnatpmpresponseorretry(&getHandle(igd), &response);

            if (err == NATPMP_TRYAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_BEFORE_READ_RETRY));

            } else if (err < 0) {
                JAMI_ERR("NAT-PMP: read response on IGD %s failed with error %s",
                         igd->getPublicIp().toString().c_str(),
                         getNatPmpErrorStr(err));
                break;
            } else {
                IpAddr igdAddr(response.pnu.publicaddress.addr);

                if (isNewIgd(igdAddr)) {
                    auto newIgd = std::make_shared<PMPIGD>();
                    newIgd->setPublicIp(igdAddr);

                    auto localGw = getLocalGateway();
                    if (not matchLocalGateway(localGw.get(),
                                              response.pnu.publicaddress.addr.s_addr)) {
                        // The address does not match the local gateway. Blacklist now.
                        addIgd(newIgd, true);
                    } else {
                        // Add the new IGD.
                        addIgd(newIgd);
                        JAMI_ERR("NAT-PMP: found new device with external IP %s",
                                 igdAddr.toString().c_str());
                        // Update.
                        newIgd->setLocalIp(ip_utils::getLocalAddr(AF_INET));
                        newIgd->setValid(true);
                        // Report to the listener.
                        processIgdUpdate(newIgd, UpnpIgdEvent::ADDED);
                    }
                } else {
                    JAMI_DBG("NAT-PMP: IGD with public IP %s is already in the list",
                             igdAddr.toString().c_str());
                }
                break;
            }
        }
    }
}

const char*
NatPmp::getNatPmpErrorStr(int errorCode)
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

std::unique_ptr<IpAddr>
NatPmp::getLocalGateway() const
{
    char localHostBuf[INET_ADDRSTRLEN];
    if (ip_utils::getHostName(localHostBuf, INET_ADDRSTRLEN) < 0) {
        JAMI_WARN("NAT-PMP: Couldn't find local host");
        return nullptr;
    } else {
        return std::make_unique<IpAddr>(
            ip_utils::getGateway(localHostBuf, ip_utils::subnet_mask::prefix_24bit));
    }
}

natpmp_t&
NatPmp::getHandle(const std::shared_ptr<PMPIGD>& igd)
{
    assert(igd);
    return igd->getHandle();
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
NatPmp::isNewIgd(const IpAddr& igdAddr) const
{
    std::lock_guard<std::mutex> lock(igdListMutex_);
    for (auto const& igd : igdList_) {
        if (igdAddr == IpAddr(igd->getPublicIp())) {
            return false;
        }
    }
    for (auto const& igd : igdBlackList_) {
        if (igdAddr == IpAddr(igd->getPublicIp())) {
            return false;
        }
    }
    return true;
}

void
NatPmp::addIgd(std::shared_ptr<PMPIGD> igd, bool blackList)
{
    if (not igd->getPublicIp()) {
        JAMI_ERR("NAT-PMP: Trying to add an IGD with invalid public address: %s",
                 igd->getPublicIp().toString().c_str());
    }

    std::lock_guard<std::mutex> lock(igdListMutex_);
    if (blackList) {
        JAMI_WARN("NAT-PMP: Added IGD %s to blacklist", igd->getPublicIp().toString().c_str());
        igdBlackList_.emplace_back(std::move(igd));
    } else {
        JAMI_DBG("NAT-PMP: Added new IGD %s", igd->getPublicIp().toString().c_str());
        igdList_.emplace_back(std::move(igd));
    }
    int idx = 0;
    for (auto const& igd : igdList_) {
        JAMI_DBG("NAT-PMP: IGD %i: [%s] valid %s",
                 idx,
                 igd->getPublicIp().toString().c_str(),
                 igd->isValid() ? "YES" : "NO");
        idx++;
    }

    JAMI_DBG("NAT-PMP: Current top IGD [%s] valid %s",
             igdList_.front()->getPublicIp().toString().c_str(),
             igdList_.front()->isValid() ? "YES" : "NO");
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
            JAMI_WARN("NAT-PMP: This IGD [%s] is invalid", igd->getPublicIp().toString().c_str());
            return igd;
        }
    }

    return {};
}

bool
NatPmp::matchLocalGateway(IpAddr* localGw, in_addr_t gateway) const
{
    if (not localGw or not *localGw) {
        JAMI_WARN("NAT-PMP: Couldn't find valid gateway on local host");
        return false;
    } else {
        char addrbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &gateway, addrbuf, sizeof(addrbuf));
        IpAddr igdAddr(addrbuf);

        if (*localGw != igdAddr) {
            JAMI_WARN("NAT-PMP: IGD address %s does not match local gateway %s",
                      igdAddr.toString().c_str(),
                      localGw->toString().c_str());
            return false;
        }
    }

    return true;
}

void
NatPmp::processIgdUpdate(const std::shared_ptr<IGD>& igd, UpnpIgdEvent event)
{
    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, event] { obs->onIgdUpdate(igd, event); });
}

void
NatPmp::processMappingAdded(const std::shared_ptr<IGD>& igd, const Mapping& map)
{
    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, map] { obs->onMappingAdded(igd, map); });
}

void
NatPmp::processMappingRemoved(const std::shared_ptr<IGD>& igd, const Mapping& map)
{
    // Process the response on the context thread.
    runOnUpnpContextThread([obs = observer_, igd, map] { obs->onMappingRemoved(igd, map); });
}

} // namespace upnp
} // namespace jami
