/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>

#include <opendht/rng.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_LIBNATPMP

#include <natpmp.h>

#include "noncopyable.h"
#include "upnp_protocol.h"
#include "../igd/igd.h"
#include "../igd/pmp_igd.h"
#include "../mapping/global_mapping.h"

using random_device = dht::crypto::random_device;

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

constexpr static int NAT_PMP_INVALID_ARGS = 402;
constexpr static int NAT_PMP_ARRAY_IDX_INVALID = 713;
constexpr static int NAT_PMP_CONFLICT_IN_MAPPING = 718;

constexpr static int NAT_PMP_UPNP_E_SUCCESS = 0;

constexpr static unsigned NAT_PMP_MAX_RETRIES = 20;

typedef std::function<bool(UPnPProtocol*, IGD*)> callbackAddIgd;
typedef std::function<bool(IGD*)> callbackRemoveIgd;
typedef std::function<bool(IpAddr)> callbackRemoveIgdByIp;

class NatPmp : public UPnPProtocol
{
public:
    // Template for adding IGD callback.
    template<class T> void setAddIgdCallback(T* const object, bool(T::* const mf)(UPnPProtocol*, IGD*)) {
        using namespace std::placeholders; 
        addIgdCb_ = std::move(std::bind(mf, object, _1, _2));
    }

    // Template for removing IGD callback.
    template<class T> void setRemoveIgdCallback(T* const object, bool(T::* const mf)(IGD*)) {
        using namespace std::placeholders; 
        removeIgdCb_ = std::move(std::bind(mf, object, _1));
    }

    // Template for removing IGD by public Ip callback.
    template<class T> void setRemoveIgdByIpCallback(T* const object, bool(T::* const mf)(IpAddr)) {
        using namespace std::placeholders; 
        removeIgdByIpCb_ = std::move(std::bind(mf, object, _1));
    }

    NatPmp();
    ~NatPmp();
    
    void connectivityChanged();

    // Renew pmp_igd.
    void searchForIGD();

    // Add IGD listener.
    size_t addIGDListener(IGDFoundCallback&& cb);
    
    // Remove IGD listener.
    void removeIGDListener(size_t token);

    // Tries to add mapping. Assumes mutex is already locked.
    Mapping addMapping(IGD* igd, uint16_t port_external, uint16_t port_internal, PortType type, int *upnp_error);

    // Removes a mapping.
    void removeMapping(const Mapping& mapping);

    // Removes all local mappings of IGD that we're added by the application.
    void removeAllLocalMappings(IGD* igd);

public:
    void PMPsearchForIGD(const std::shared_ptr<PMPIGD>& pmp_igd, natpmp_t& natpmp);
    void PMPaddPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, GlobalMapping& mapping, bool remove=false) const;
    void PMPdeleteAllPortMapping(const PMPIGD& pmp_igd, natpmp_t& natpmp, int proto) const;

public:
    mutable std::mutex validIGDMutex_;                  // Mutex used to access these lists and IGDs in a thread-safe manner.
    std::condition_variable validIGDCondVar_;               

    std::map<size_t, IGDFoundCallback> igdListeners_;   // Map of valid IGD listeners with their tokens.
    size_t listenerToken_ {0};                          // Last provided token for valid IGD listeners (0 is the invalid token).

    std::mutex pmpMutex_ {};
    std::condition_variable pmpCv_ {};
    std::shared_ptr<PMPIGD> pmpIGD_ {};
    std::atomic_bool pmpRun_ {true};
    std::thread pmpThread_ {};
    

private:
    NON_COPYABLE(NatPmp);

    callbackAddIgd addIgdCb_;
    callbackRemoveIgd removeIgdCb_;
    callbackRemoveIgdByIp removeIgdByIpCb_;
};

std::shared_ptr<NatPmp> getNatPmp();

}} // namespace jami::upnp

#endif /* HAVE_LIBNATPMP */
