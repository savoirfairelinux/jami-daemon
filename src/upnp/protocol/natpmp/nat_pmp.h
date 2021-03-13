/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../upnp_protocol.h"
#include "../igd.h"
#include "pmp_igd.h"

#include "logger.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"

// uncomment to enable native natpmp error messages
//#define ENABLE_STRNATPMPERR 1
#include <natpmp.h>

#include <atomic>
#include <thread>

namespace jami {
class IpAddr;
}

namespace jami {
namespace upnp {

// Requested lifetime in seconds. The actual lifetime might be different.
constexpr static unsigned int MAPPING_ALLOCATION_LIFETIME {60 * 60};
// Max number of IGD search attempts before failure.
constexpr static unsigned int MAX_RESTART_SEARCH_RETRIES {5};
// Time-out between two successive read response.
constexpr static auto TIMEOUT_BEFORE_READ_RETRY {std::chrono::milliseconds(300)};
// Max number of read attempts before failure.
constexpr static unsigned int MAX_READ_RETRIES {5};
// Time-out between two successive IGD search.
constexpr static auto TIMEOUT_BEFORE_IGD_SEARCH_RETRY {std::chrono::seconds(60)};

class NatPmp : public UPnPProtocol
{
public:
    NatPmp();
    ~NatPmp();

    // Set the observer.
    void setObserver(UpnpMappingObserver* obs) override;

    // Returns the protocol type.
    NatProtocolType getProtocol() const override { return NatProtocolType::NAT_PMP; }

    // Get protocol type as string.
    char const* getProtocolName() const override { return "NAT-PMP"; }

    // Notifies a change in network.
    void clearIgds() override;

    // Renew pmp_igd.
    void searchForIgd() override;

    // Get the IGD list.
    void getIgdList(std::list<std::shared_ptr<IGD>>& igdList) const override;

    // Return true if it has at least one valid IGD.
    bool isReady() const override;

    // Increment errors counter.
    void incrementErrorsCounter(const std::shared_ptr<IGD>& igd) override;

    // Request a new mapping.
    void requestMappingAdd(const std::shared_ptr<IGD>& igd, const Mapping& mapping) override;

    // Renew an allocated mapping.
    void requestMappingRenew(const Mapping& mapping) override;

    // Removes a mapping.
    void requestMappingRemove(const Mapping& igdMapping) override;

    // Terminate. Nothing to do here, the clean-up is done when
    // the IGD is cleared.
    void terminate() override;

private:
    void initNatPmp();
    void getIgdPublicAddress();
    void removeAllMappings();
    int readResponse(natpmp_t& handle, natpmpresp_t& response);
    int sendMappingRequest(const Mapping& mapping, uint32_t& lifetime);

    // Adds a port mapping.
    void addPortMapping(const std::shared_ptr<IGD>& igd, Mapping& mapping, bool renew);
    // Removes a port mapping.
    void removePortMapping(Mapping& mapping);
    // True if the error is fatal.
    bool isErrorFatal(int error);
    // Get local getaway.
    std::unique_ptr<IpAddr> getLocalGateway() const;

    // Helpers to process user callbacks
    void processIgdUpdate(UpnpIgdEvent event);
    void processMappingAdded(const Mapping& map);
    void processMappingRenewed(const Mapping& map);
    void processMappingRemoved(const Mapping& map);

private:
    NON_COPYABLE(NatPmp);

    // Gets NAT-PMP error code string.
    const char* getNatPmpErrorStr(int errorCode) const;

    ScheduledExecutor* getNatpmpScheduler() { return &natpmpScheduler_; }
    ScheduledExecutor* getUpnContextScheduler() { return UpnpThreadUtil::getScheduler(); }
    bool validIgdInstance(const std::shared_ptr<IGD>& igdIn);
    std::atomic_bool initialized_ {false};

    // Data members
    std::shared_ptr<PMPIGD> igd_;
    natpmp_t natpmpHdl_;
    ScheduledExecutor natpmpScheduler_ {};
    std::shared_ptr<Task> searchForIgdTimer_ {};
    unsigned int igdSearchCounter_ {0};
};

} // namespace upnp
} // namespace jami
