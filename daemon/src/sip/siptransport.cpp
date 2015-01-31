/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "siptransport.h"
#include "sip_utils.h"
#include "ip_utils.h"

#include "ringdht/sip_transport_ice.h"

#include "client/configurationmanager.h"
#include "array_size.h"
#include "intrin.h"
#include "sipvoiplink.h"

#include <pjsip.h>
#include <pjsip/sip_types.h>
#if HAVE_TLS
#include <pjsip/sip_transport_tls.h>
#endif
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <stdexcept>
#include <sstream>
#include <algorithm>

#define RETURN_IF_FAIL(A, VAL, M, ...) if (!(A)) { RING_ERR(M, ##__VA_ARGS__); return (VAL); }

namespace ring {

constexpr const char* TRANSPORT_STATE_STR[] = {
    "CONNECTED", "DISCONNECTED", "SHUTDOWN", "DESTROY", "UNKNOWN STATE"
};
constexpr const size_t TRANSPORT_STATE_SZ = RING_ARRAYSIZE(TRANSPORT_STATE_STR);

std::string
SipTransportDescr::toString() const
{
    std::stringstream ss;
    ss << "{" << pjsip_transport_get_type_desc(type) << " on " << interface << ":" << listenerPort  << "}";
    return ss.str();
}

SipTransport::SipTransport(pjsip_transport* t)
    : transport_(nullptr, pjsip_transport_dec_ref)
{
    if (not t or pjsip_transport_add_ref(t) != PJ_SUCCESS)
        throw std::runtime_error("invalid transport");

    // Set pointer here, right after the successful pjsip_transport_add_ref
    transport_.reset(t);

    RING_DBG("SipTransport@%p {tr=%p {rc=%u}}",
             this, transport_.get(), pj_atomic_get(transport_->ref_cnt));
}

SipTransport::SipTransport(pjsip_transport* t,
                           const std::shared_ptr<TlsListener>& l)
    : SipTransport(t)
{
    tlsListener_ = l;
}

SipTransport::~SipTransport()
{
    RING_DBG("~SipTransport@%p {tr=%p {rc=%u}}",
             this, transport_.get(), pj_atomic_get(transport_->ref_cnt));
}

bool
SipTransport::isAlive(UNUSED const std::shared_ptr<SipTransport>& t,
                      pjsip_transport_state state)
{
    return state != PJSIP_TP_STATE_DISCONNECTED
#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
    && state != PJSIP_TP_STATE_SHUTDOWN
    && state != PJSIP_TP_STATE_DESTROY
#else
    && t && t->get()
    && !t->get()->is_shutdown
    && !t->get()->is_destroying
#endif
    ;
}

const char*
SipTransport::stateToStr(pjsip_transport_state state)
{
    return TRANSPORT_STATE_STR[std::min<size_t>(state, TRANSPORT_STATE_SZ-1)];
}

void
SipTransport::stateCallback(pjsip_transport_state state,
                            const pjsip_transport_state_info *info)
{
    std::vector<SipTransportStateCallback> cbs {};
    {
        std::lock_guard<std::mutex> lock(stateListenersMutex_);
        cbs.reserve(stateListeners.size());
        for (auto& l : stateListeners)
            cbs.push_back(l.second);
    }
    for (auto& cb : cbs)
        cb(state, info);
}

void
SipTransport::addStateListener(uintptr_t lid, SipTransportStateCallback cb)
{
    std::lock_guard<std::mutex> lock(stateListenersMutex_);
    stateListeners[lid] = cb;
}

bool
SipTransport::removeStateListener(uintptr_t lid)
{
    std::lock_guard<std::mutex> lock(stateListenersMutex_);
    auto it = stateListeners.find(lid);
    if (it != stateListeners.end()) {
        stateListeners.erase(it);
        return true;
    }
    return false;
}

SipTransportBroker::SipTransportBroker(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool) :
#if HAVE_DHT
iceTransports_(),
#endif
cp_(cp), pool_(pool), endpt_(endpt)
{
    auto status = pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), SipTransportBroker::tp_state_callback);
    if (status != PJ_SUCCESS) {
        RING_ERR("Can't set transport callback");
        sip_utils::sip_strerror(status);
    }

#if HAVE_DHT
    pjsip_transport_register_type(PJSIP_TRANSPORT_DATAGRAM, "ICE", pjsip_transport_get_default_port_for_type(PJSIP_TRANSPORT_UDP), &ice_pj_transport_type_);
#endif
}

SipTransportBroker::~SipTransportBroker()
{
    RING_DBG("Destroying SipTransportBroker");

    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        udpTransports_.clear();
        transports_.clear();
    }
    pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), nullptr);
    {
        std::unique_lock<std::mutex> lock(iceMutex_);
        if (not iceTransports_.empty())
            RING_WARN("Remaining %u registred ICE transports", iceTransports_.size());
    }
}

/** static method (so C callable) used by PJSIP making interface to C++ */
void
SipTransportBroker::tp_state_callback(pjsip_transport* tp,
                                      pjsip_transport_state state,
                                      const pjsip_transport_state_info* info)
{
    // There is no way (at writing) to link a user data to a PJSIP transport.
    // So we obtain it from the global SIPVoIPLink instance that owns it.
    // Be sure the broker's owner is not deleted during proccess
    if (auto sipLink = getSIPVoIPLink()) {
        if (auto& broker = sipLink->sipTransportBroker)
            broker->transportStateChanged(tp, state, info);
        else
            RING_ERR("SIPVoIPLink with invalid SipTransportBroker");
    }
}

void
SipTransportBroker::transportStateChanged(pjsip_transport* tp, pjsip_transport_state state, const pjsip_transport_state_info* info)
{
    RING_WARN("Transport %s -> %s", tp->info, SipTransport::stateToStr(state));
    {
        std::shared_ptr<SipTransport> transport {};
        {
            // The mutex is unlocked so the callback can lock it.
            std::lock_guard<std::mutex> lock(transportMapMutex_);
            auto t = transports_.find(tp);
            if (t != transports_.end()) {
                transport = t->second.lock();
            }
        }
        // Propagate the event to the appropriate transport.
        if (transport)
            transport->stateCallback(state, info);
    }

#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
    if (state == PJSIP_TP_STATE_DESTROY)
#else
    if (tp->is_destroying)
#endif
     {
        std::lock_guard<std::mutex> lock(transportMapMutex_);

        // Transport map cleanup
        auto t = transports_.find(tp);
        if (t != transports_.end() && t->second.expired())
            transports_.erase(t);

        // If UDP
        const auto type = tp->key.type;
        if (type == PJSIP_TRANSPORT_UDP || type == PJSIP_TRANSPORT_UDP6) {
            auto transport_key = std::find_if(udpTransports_.cbegin(), udpTransports_.cend(), [tp](const std::pair<SipTransportDescr, pjsip_transport*>& i) {
                return i.second == tp;
            });
            if (transport_key != udpTransports_.end()) {
                RING_WARN("UDP transport destroy");
                transports_.erase(transport_key->second);
                udpTransports_.erase(transport_key);
            }
        }
    }
}

std::shared_ptr<SipTransport>
SipTransportBroker::findTransport(pjsip_transport* t)
{
    if (!t)
        return nullptr;
    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        auto i = transports_.find(t);
        if (i == transports_.end()) {
            auto ret = std::make_shared<SipTransport>(t);
            transports_[t] = ret;
            return ret;
        }
        else if (auto spt = i->second.lock())
            return spt;
        else
            return nullptr;
    }
}

void
SipTransportBroker::shutdown()
{
    std::unique_lock<std::mutex> lock(transportMapMutex_);
    for (auto& t : transports_) {
        if (auto transport = t.second.lock()) {
            pjsip_transport_shutdown(transport->get());
        }
    }
}

std::shared_ptr<SipTransport>
SipTransportBroker::getUdpTransport(const SipTransportDescr& descr)
{
    std::lock_guard<std::mutex> lock(transportMapMutex_);
    auto itp = udpTransports_.find(descr);
    if (itp != udpTransports_.end()) {
        auto it = transports_.find(itp->second);
        if (it != transports_.end()) {
            if (auto spt = it->second.lock()) {
                RING_DBG("Reusing transport %s", descr.toString().c_str());
                return spt;
            }
            else {
                // Transport still exists but have not been destroyed yet.
                RING_WARN("Recycling transport %s", descr.toString().c_str());
                auto ret = std::make_shared<SipTransport>(itp->second);
                it->second = ret;
                return ret;
            }
        } else {
            RING_WARN("Cleaning up UDP transport %s", descr.toString().c_str());
            udpTransports_.erase(itp);
        }
    }
    auto ret = createUdpTransport(descr);
    if (ret) {
        udpTransports_[descr] = ret->get();
        transports_[ret->get()] = ret;
    }
    return ret;
}

std::shared_ptr<SipTransport>
SipTransportBroker::createUdpTransport(const SipTransportDescr& d)
{
    RETURN_IF_FAIL(d.listenerPort != 0, nullptr, "Could not determine port for this transport");
    auto family = pjsip_transport_type_get_af(d.type);

    IpAddr listeningAddress = (d.interface == ip_utils::DEFAULT_INTERFACE) ?
        ip_utils::getAnyHostAddr(family) :
        ip_utils::getInterfaceAddr(d.interface, family);
    listeningAddress.setPort(d.listenerPort);

    RETURN_IF_FAIL(listeningAddress, nullptr, "Could not determine IP address for this transport");
    pjsip_transport *transport = nullptr;
    pj_status_t status = listeningAddress.isIpv4()
        ? pjsip_udp_transport_start (endpt_, &static_cast<const pj_sockaddr_in&>(listeningAddress),  nullptr, 1, &transport)
        : pjsip_udp_transport_start6(endpt_, &static_cast<const pj_sockaddr_in6&>(listeningAddress), nullptr, 1, &transport);
    if (status != PJ_SUCCESS) {
        RING_ERR("UDP IPv%s Transport did not start on %s",
            listeningAddress.isIpv4() ? "4" : "6",
            listeningAddress.toString(true).c_str());
        sip_utils::sip_strerror(status);
        return nullptr;
    }

    RING_DBG("Created UDP transport on %s : %s", d.interface.c_str(), listeningAddress.toString(true).c_str());
    auto ret = std::make_shared<SipTransport>(transport);
    // dec ref because the refcount starts at 1 and SipTransport increments it ?
    // pjsip_transport_dec_ref(transport);
    return ret;
}

#if HAVE_TLS
std::shared_ptr<TlsListener>
SipTransportBroker::getTlsListener(const SipTransportDescr& d, const pjsip_tls_setting* settings)
{
    RETURN_IF_FAIL(settings, nullptr, "TLS settings not specified");
    auto family = pjsip_transport_type_get_af(d.type);

    IpAddr listeningAddress = (d.interface == ip_utils::DEFAULT_INTERFACE) ?
        ip_utils::getAnyHostAddr(family) :
        ip_utils::getInterfaceAddr(d.interface, family);
    listeningAddress.setPort(d.listenerPort);

    RETURN_IF_FAIL(listeningAddress, nullptr, "Could not determine IP address for this transport");
    RING_DBG("Creating TLS listener %s on %s...", d.toString().c_str(), listeningAddress.toString(true).c_str());
#if 0
    RING_DBG(" ca_list_file : %s", settings->ca_list_file.ptr);
    RING_DBG(" cert_file    : %s", settings->cert_file.ptr);
    RING_DBG(" ciphers_num    : %d", settings->ciphers_num);
    RING_DBG(" verify server %d client %d client_cert %d", settings->verify_server, settings->verify_client, settings->require_client_cert);
    RING_DBG(" reuse_addr    : %d", settings->reuse_addr);
#endif

    pjsip_tpfactory *listener = nullptr;
    const pj_status_t status = pjsip_tls_transport_start2(endpt_, settings, listeningAddress.pjPtr(), nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
        RING_ERR("TLS listener did not start");
        sip_utils::sip_strerror(status);
        return nullptr;
    }
    return std::make_shared<TlsListener>(listener);
}

std::shared_ptr<SipTransport>
SipTransportBroker::getTlsTransport(const std::shared_ptr<TlsListener>& l, const IpAddr& remote)
{
    if (!l || !remote)
        return nullptr;
    IpAddr remoteAddr {remote};
    if (remoteAddr.getPort() == 0)
        remoteAddr.setPort(pjsip_transport_get_default_port_for_type(l->get()->type));

    RING_DBG("Get new TLS transport to %s", remoteAddr.toString(true).c_str());
    pjsip_tpselector sel {PJSIP_TPSELECTOR_LISTENER, {
        .listener = l->get()
    }};
    pjsip_transport *transport = nullptr;
    pj_status_t status = pjsip_endpt_acquire_transport(
            endpt_,
            l->get()->type,
            remoteAddr.pjPtr(),
            remoteAddr.getLength(),
            &sel,
            &transport);

    if (!transport || status != PJ_SUCCESS) {
        RING_ERR("Could not get new TLS transport");
        sip_utils::sip_strerror(status);
        return nullptr;
    }
    auto ret = std::make_shared<SipTransport>(transport, l);
    pjsip_transport_dec_ref(transport);
    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        transports_[ret->get()] = ret;
    }
    return ret;
}
#endif

#if HAVE_DHT
std::shared_ptr<SipTransport>
SipTransportBroker::getIceTransport(const std::shared_ptr<IceTransport> ice, unsigned comp_id)
{
    std::unique_lock<std::mutex> lock(iceMutex_);
    iceTransports_.emplace_front(endpt_, pool_, ice_pj_transport_type_, ice, comp_id, [=]() -> int {
        std::unique_lock<std::mutex> lock(iceMutex_);
        const auto ice_transport_key = std::find_if(iceTransports_.begin(), iceTransports_.end(), [&](const SipIceTransport& i) {
            return i.getIceTransport() == ice;
        });
        if (ice_transport_key != iceTransports_.end()) {
            iceTransports_.erase(ice_transport_key);
            return PJ_SUCCESS;
        }
        return PJ_ENOTFOUND;
    });
    auto& sip_ice_tr = iceTransports_.front();
    auto ret = std::make_shared<SipTransport>(&sip_ice_tr.base);
    {
        std::unique_lock<std::mutex> lock(transportMapMutex_);
        transports_[ret->get()] = ret;
    }
    return ret;
}
#endif

} // namespace ring
