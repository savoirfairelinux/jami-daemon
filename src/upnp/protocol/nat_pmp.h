/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *	Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#if HAVE_LIBNATPMP

#include "upnp_protocol.h"
#include "../igd/igd.h"
#include "../igd/pmp_igd.h"
#include "../global_mapping.h"

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"
#include "compiler_intrinsics.h"

#include <natpmp.h>
#include <opendht/rng.h>

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <random>
#include <cstdlib>
#include <condition_variable>

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

class NatPmp : public UPnPProtocol
{
public:
	NatPmp();
	~NatPmp();

	// Notifies a change in network.
	void connectivityChanged() override;

	// Renew pmp_igd.
	void searchForIGD() override;

	// Set the IGD list callback handler.
	void setOnIgdChanged(IgdListChangedCallback&& cb) override;

	// Add IGD listener.
	size_t addIGDListener(IgdFoundCallback&& cb) override;

	// Remove IGD listener.
	void removeIGDListener(size_t token) override;

	// Tries to add mapping. Assumes mutex is already locked.
	Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, UPnPProtocol::UpnpError& upnp_error) override;

	// Removes a mapping.
	void removeMapping(const Mapping& mapping) override;

	// Removes all local mappings of IGD that we're added by the application.
	void removeAllLocalMappings(IGD* igd) override;

private:
	// Searches for an IGD.
	void PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp);

	// Adds (or deletes) a port mapping.
	void PMPaddPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, GlobalMapping& mapping, bool remove=false) const;

	// Deletes all port mappings.
	void PMPdeleteAllPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, int proto) const;

private:
	NON_COPYABLE(NatPmp);

	std::mutex pmpMutex_ {};                            // NatPmp mutex.
	std::condition_variable pmpCv_ {};
	std::shared_ptr<PMPIGD> pmpIGD_ {};                 // IGD discovered by NatPmp.
	std::atomic_bool pmpRun_ {true};
	std::thread pmpThread_ {};                          // NatPmp thread.

	IgdListChangedCallback updateIgdListCb_;			// Callback for when the IGD list changes.
};

std::shared_ptr<NatPmp> getNatPmp();

}} // namespace jami::upnp

#endif /* HAVE_LIBNATPMP */
