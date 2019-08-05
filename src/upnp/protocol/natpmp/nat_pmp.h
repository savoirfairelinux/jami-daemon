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

class NatPmp : public UPnPProtocol
{
public:
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
    // Searches for an IGD discoverable by natpmp.
    void searchForPmpIgd();

    // Adds a port mapping.
    void addPortMapping(Mapping& mapping, bool renew);

    // Removes a port mapping.
    void removePortMapping(Mapping& mapping);

    // Deletes all port mappings.
    void deleteAllPortMappings(int proto);

    // Clears the natpmp struct.
    void clearNatPmpHdl(natpmp_t& hdl);

    // Returns gateway based on the local host address.
    std::string getGateway(char* localHost);

    // Gets NAT-PMP error code string.
    std::string getNatPmpErrorStr(int errorCode);

private:
    NON_COPYABLE(NatPmp);

    std::mutex pmpThreadMutex_ {};          // Thread mutex.
    std::condition_variable pmpCv_ {};      // Condition variable for thread-safe signaling.
    std::atomic_bool pmpRun_ { true };      // Variable to allow the thread to run.
    std::thread pmpThread_ {};              // NatPmp thread.

    std::mutex natpmpMutex_;                // NatPmp handle mutex.
    natpmp_t natpmpHdl_;                    // NatPmp handle.

    std::unique_ptr<PMPIGD> pmpIgd_;        // IGD for NatPmp.
};

}} // namespace jami::upnp
