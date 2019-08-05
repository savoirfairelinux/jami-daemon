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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#define NATPMP_STATICLIB
#endif

#include "../upnp_protocol.h"
#include "../global_mapping.h"
#include "../igd.h"
#include "pmp_igd.h"

#include "logger.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "compiler_intrinsics.h"

#include <natpmp.h>

#include <atomic>
#include <thread>
#include <queue>

#ifndef _WIN32
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define NATPMP_MAX_INTERFACES     (256)
#define NATPMP_DEFAULT_INTERFACE    (1)
#define NATPMP_INVALID_SOCKET      (-1)

#ifndef _WIN32
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define NATPMP_MAX_INTERFACES     (256)
#define NATPMP_DEFAULT_INTERFACE    (1)
#define NATPMP_INVALID_SOCKET      (-1)

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

constexpr static unsigned int ADD_MAP_LIFETIME {3600};
constexpr static unsigned int REMOVE_MAP_LIFETIME {0};
constexpr static unsigned int MAX_SEARCH_RETRY {5};
constexpr static unsigned int MAX_INIT_RETRY {5};

class NatPmp : public UPnPProtocol
{
public:
    enum class NatPmpState {
        IDLE = 0,
        INIT,
        RESTART,
        SEARCH,
        OPEN_PORT,
        CLOSE_PORT,
        CLOSE_ALL,
        RENEW,
        ERROR,
        EXIT
    };

    NatPmp();
    ~NatPmp();

    // Returns the protocol type.
    Type getType() const override { return Type::NAT_PMP; }

    // Notifies a change in network.
    void clearIgds() override;

    // Renew IGD.
    void searchForIgd() override;

    // Tries to add mapping.
    void requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type) override;

    // Removes a mapping.
    void requestMappingRemove(const Mapping& igdMapping) override;

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd) override;

private:
    // Updates the state.
    void updateState(NatPmpState state, bool clearBeforeInsert = false);

    // Clears natpmp handle and initializes library.
    bool initNatPmp();

    // Searches for an IGD discoverable by natpmp.
    bool searchForPmpIgd();

    // Adds a port mapping.
    void addPortMapping(Mapping& mapping, bool renew);

    // Removes a port mapping.
    void removePortMapping(Mapping& mapping);

    // Deletes all port mappings.
    void deleteAllPortMappings(int proto);

    // Checks if there is a mapping that needs to be renewed.
    bool isMappingUpForRenewal(const time_point& now);

    // Checks if the IGD needs to be renewed.
    bool isIgdUpForRenewal(const time_point& now);

    // Clears the natpmp struct.
    void clearNatPmpHdl(natpmp_t& hdl);

    // Returns gateway based on the local host address.
    std::string getGateway(char* localHost);

    // Gets NAT-PMP error code string.
    std::string getNatPmpErrorStr(int errorCode);

private:
    NON_COPYABLE(NatPmp);

    std::atomic_bool pmpRun_ {true};            // Variable to allow the thread to run.
    std::thread pmpThread_ {};                  // NatPmp thread.
    std::condition_variable pmpCv_;

    std::mutex natpmpMutex_;                    // NatPmp handle mutex.
    natpmp_t natpmpHdl_;                        // NatPmp handle.

    std::unique_ptr<PMPIGD> pmpIgd_;            // IGD for NatPmp.

    std::atomic_bool isInit_ {false};           // Variable that indicates if we are currently initialized.
    std::atomic_bool isRestart_ {false};        // Variable that indicates we need to restart natpmp after a connectivity change.

    unsigned int initRetry_ {0};                // Keeps track of number of times we try to initialize.
    unsigned int searchRetry_ {0};              // Keeps track of number of times we try to find an IGD.

    time_point restartTimer_ {clock::now()};    // Keeps track of time elapsed since restart was triggered.
    time_point errorTimer_ {clock::now()};      // Keeps track of time elapsed since error was triggered.

    std::mutex queueMutex_;                     // State queue mutex.
    std::queue<NatPmpState> stateQueue_ {};     // Queue for storing the state changes.
    NatPmpState pmpState_ {NatPmpState::INIT};  // State variable.
};

}} // namespace jami::upnp
