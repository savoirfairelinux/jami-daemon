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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "connectivity/upnp/protocol/upnp_protocol.h"
#include "connectivity/upnp/protocol/igd.h"
#include "pmp_igd.h"

#include "logger.h"
#include "connectivity/ip_utils.h"
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
constexpr static unsigned int MAX_RESTART_SEARCH_RETRIES {3};
// Time-out between two successive read response.
constexpr static auto TIMEOUT_BEFORE_READ_RETRY {std::chrono::milliseconds(300)};
// Max number of read attempts before failure.
constexpr static unsigned int MAX_READ_RETRIES {3};
// Base unit for the timeout between two successive IGD search.
constexpr static auto NATPMP_SEARCH_RETRY_UNIT {std::chrono::seconds(10)};

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
    std::list<std::shared_ptr<IGD>> getIgdList() const override;

    // Return true if it has at least one valid IGD.
    bool isReady() const override;

    // Request a new mapping.
    void requestMappingAdd(const Mapping& mapping) override;

    // Renew an allocated mapping.
    void requestMappingRenew(const Mapping& mapping) override;

    // Removes a mapping.
    void requestMappingRemove(const Mapping& mapping) override;

    // Get the host (local) address.
    const IpAddr getHostAddress() const override;

    // Terminate. Nothing to do here, the clean-up is done when
    // the IGD is cleared.
    void terminate() override;

private:
    NON_COPYABLE(NatPmp);

    std::weak_ptr<NatPmp> weak() { return std::static_pointer_cast<NatPmp>(shared_from_this()); }

    // Helpers to run tasks on NAT-PMP internal execution queue.
    ScheduledExecutor* getNatpmpScheduler() { return &natpmpScheduler_; }
    template<typename Callback>
    void runOnNatPmpQueue(Callback&& cb)
    {
        natpmpScheduler_.run([cb = std::forward<Callback>(cb)]() mutable { cb(); });
    }

    // Helpers to run tasks on UPNP context execution queue.
    ScheduledExecutor* getUpnContextScheduler() { return UpnpThreadUtil::getScheduler(); }

    void terminate(std::condition_variable& cv);

    void initNatPmp();
    void getIgdPublicAddress();
    void removeAllMappings();
    int readResponse(natpmp_t& handle, natpmpresp_t& response);
    int sendMappingRequest(const Mapping& mapping, uint32_t& lifetime);

    // Adds a port mapping.
    int addPortMapping(Mapping& mapping);
    // Removes a port mapping.
    void removePortMapping(Mapping& mapping);

    // True if the error is fatal.
    bool isErrorFatal(int error);
    // Gets NAT-PMP error code string.
    const char* getNatPmpErrorStr(int errorCode) const;
    // Get local getaway.
    std::unique_ptr<IpAddr> getLocalGateway() const;

    // Helpers to process user's callbacks
    void processIgdUpdate(UpnpIgdEvent event);
    void processMappingAdded(const Mapping& map);
    void processMappingRequestFailed(const Mapping& map);
    void processMappingRenewed(const Mapping& map);
    void processMappingRemoved(const Mapping& map);

    // Check if the IGD has a local match
    bool validIgdInstance(const std::shared_ptr<IGD>& igdIn);

    // Increment errors counter.
    void incrementErrorsCounter(const std::shared_ptr<IGD>& igd);

    std::atomic_bool initialized_ {false};

    // Data members
    std::shared_ptr<PMPIGD> igd_;
    natpmp_t natpmpHdl_;
    ScheduledExecutor natpmpScheduler_ {"natpmp"};
    std::shared_ptr<Task> searchForIgdTimer_ {};
    unsigned int igdSearchCounter_ {0};
    UpnpMappingObserver* observer_ {nullptr};
    IpAddr hostAddress_ {};

    // Calls from other threads that does not need synchronous access are
    // rescheduled on the NatPmp private queue. This will avoid the need to
    // protect most of the data members of this class.
    // For some internal members (such as the igd instance and the host
    // address) that need to be synchronously accessed, are protected by
    // this mutex.
    mutable std::mutex natpmpMutex_;

    // Shutdown synchronization
    bool shutdownComplete_ {false};
};

} // namespace upnp
} // namespace jami
