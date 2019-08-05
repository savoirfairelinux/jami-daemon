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

    // Tries to add mapping. Assumes mutex is already locked.
    void addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type) override;

    // Removes a mapping.
    void removeMapping(const Mapping& igdMapping) override;

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd) override;

private:
    // Searches for an IGD.
    void searchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp);

    // Adds (or deletes) a port mapping.
    void addPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, Mapping& mapping, bool renew, bool remove=false) const;

    // Deletes all port mappings.
    void deleteAllPortMappings(const PMPIGD& pmp_igd, natpmp_t& natpmp, int proto) const;

private:
    NON_COPYABLE(NatPmp);

    std::mutex pmpMutex_ {};                // NatPmp mutex.
    std::condition_variable pmpCv_ {};      // Condition variable for thread-safe signaling.
    std::atomic_bool pmpRun_ { true };      // Variable to allow the thread to run.
    std::thread pmpThread_ {};              // NatPmp thread.
};

std::shared_ptr<PMPIGD> getPmpIgd();

}} // namespace jami::upnp
