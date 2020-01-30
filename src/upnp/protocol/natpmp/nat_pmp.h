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

// uncomment to enable native natpmp error messages
//#define ENABLE_STRNATPMPERR 1
#include <natpmp.h>

#include <atomic>
#include <thread>

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class NatPmp : public UPnPProtocol
{
public:
    NatPmp();
    ~NatPmp();

    // Returns the protocol type.
    Type getType() const override { return Type::NAT_PMP; }

    // Notifies a change in network.
    void clearIgds() override;

    // Renew pmp_igd.
    void searchForIgd() override;

    // Tries to add mapping.
    void requestMappingAdd(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type) override;

    // Removes a mapping.
    void requestMappingRemove(const Mapping& igdMapping) override;

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd) override;

private:
    void searchForPmpIgd();

    // Adds a port mapping.
    void addPortMapping(Mapping& mapping, bool renew);
    // Removes a port mapping.
    void removePortMapping(Mapping& mapping);
    //void addPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, GlobalMapping& mapping, bool remove=false) const;

    // Deletes all port mappings.
    void deleteAllPortMappings(int proto);

private:
    NON_COPYABLE(NatPmp);

    std::mutex pmpMutex_ {};                            // NatPmp mutex.
    std::condition_variable pmpCv_ {};                  // Condition variable for thread-safe signaling.
    std::atomic_bool pmpRun_ { true };                  // Variable to allow the thread to run.
    std::thread pmpThread_ {};                          // NatPmp thread.

    std::atomic_bool restart_ {false};          // Variable to indicate we need to restart natpmp after a connectivity change.
    unsigned int restartSearchRetry_ {0};       // Keeps track of number of times we try to find an IGD after a connectivity change.

    std::shared_ptr<PMPIGD> pmpIGD_ {};                 // IGD discovered by NatPmp.
    std::mutex natpmpMutex_;
    natpmp_t natpmpHdl_;                                // NatPmp handle.
    // Clears the natpmp struct.
    void clearNatPmpHdl(natpmp_t& hdl);
    // Gets NAT-PMP error code string.
    const char* getNatPmpErrorStr(int errorCode);
};

}} // namespace jami::upnp
