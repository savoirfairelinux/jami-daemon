/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
 */

#include "siptransport.h"
#include "sip_utils.h"
#include "ip_utils.h"
#include "ice_transport.h"
#include "security/tls_session.h"

#include "jamidht/sips_transport_ice.h"

#include "array_size.h"
#include "compiler_intrinsics.h"
#include "sipvoiplink.h"

#include <pjsip.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_transport_tls.h>
#include <pj/ssl_sock.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <opendht/crypto.h>

#include <stdexcept>
#include <sstream>
#include <algorithm>

#define RETURN_IF_FAIL(A, VAL, ...) if (!(A)) { JAMI_ERR(__VA_ARGS__); return (VAL); }

namespace jami {

constexpr const char* TRANSPORT_STATE_STR[] = {
    "CONNECTED", "DISCONNECTED", "SHUTDOWN", "DESTROY", "UNKNOWN STATE"
};
constexpr const size_t TRANSPORT_STATE_SZ = arraySize(TRANSPORT_STATE_STR);

void
SipTransport::deleteTransport(pjsip_transport* t)
{
    pjsip_transport_dec_ref(t);
}

SipTransport::SipTransport(pjsip_transport* t)
    : transport_(nullptr, deleteTransport)
{
    if (not t or pjsip_transport_add_ref(t) != PJ_SUCCESS)
        throw std::runtime_error("invalid transport");

    // Set pointer here, right after the successful pjsip_transport_add_ref
    transport_.reset(t);

    JAMI_DBG("SipTransport@%p {tr=%p {rc=%ld}}",
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
    JAMI_DBG("~SipTransport@%p {tr=%p {rc=%ld}}",
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
    connected_ = state == PJSIP_TP_STATE_CONNECTED;

    auto extInfo = static_cast<const pjsip_tls_state_info*>(info->ext_info);
    if (isSecure() && extInfo && extInfo->ssl_sock_info && extInfo->ssl_sock_info->established) {
        auto tlsInfo = extInfo->ssl_sock_info;
        tlsInfos_.proto = (pj_ssl_sock_proto)tlsInfo->proto;
        tlsInfos_.cipher = tlsInfo->cipher;
        tlsInfos_.verifyStatus = (pj_ssl_cert_verify_flag_t)tlsInfo->verify_status;
        const auto& peers = tlsInfo->remote_cert_info->raw_chain;
        std::vector<std::pair<const uint8_t*, const uint8_t*>> bits;
        bits.resize(peers.cnt);
        std::transform(peers.cert_raw, peers.cert_raw+peers.cnt, std::begin(bits),
                       [](const pj_str_t& crt){
                           return std::make_pair((uint8_t*)crt.ptr,
                                                 (uint8_t*)(crt.ptr+crt.slen));
                       });
        tlsInfos_.peerCert = std::make_shared<dht::crypto::Certificate>(bits);
    } else {
        tlsInfos_ = {};
    }

    std::vector<SipTransportStateCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(stateListenersMutex_);
        cbs.reserve(stateListeners_.size());
        for (auto& l : stateListeners_)
            cbs.push_back(l.second);
    }
    for (auto& cb : cbs)
        cb(state, info);
}

void
SipTransport::addStateListener(uintptr_t lid, SipTransportStateCallback cb)
{
    std::lock_guard<std::mutex> lock(stateListenersMutex_);
    auto pair = stateListeners_.insert(std::make_pair(lid, cb));
    if (not pair.second)
        pair.first->second = cb;
}

bool
SipTransport::removeStateListener(uintptr_t lid)
{
    std::lock_guard<std::mutex> lock(stateListenersMutex_);
    auto it = stateListeners_.find(lid);
    if (it != stateListeners_.end()) {
        stateListeners_.erase(it);
        return true;
    }
    return false;
}

uint16_t
SipTransport::getTlsMtu()
{
    if (isIceTransport_ && isSecure()) {
        auto tls_tr = reinterpret_cast<tls::SipsIceTransport::TransportData*>(transport_.get())->self;
        return tls_tr->getTlsSessionMtu();
    }
    return 1232; /* Hardcoded yes (it's the IPv6 value).
                  * This method is broken by definition.
                  * A MTU should not be defined at this layer.
                  * And a correct value should come from the underlying transport itself,
                  * not from a constant...
                  */
}

SipTransportBroker::SipTransportBroker(pjsip_endpoint *endpt,
                                       pj_caching_pool& cp, pj_pool_t& pool) :
cp_(cp), pool_(pool), endpt_(endpt)
{
/*
    pjsip_transport_register_type(PJSIP_TRANSPORT_DATAGRAM, "ICE",
                                  pjsip_transport_get_default_port_for_type(PJSIP_TRANSPORT_UDP),
                                  &ice_pj_transport_type_);
*/
    JAMI_DBG("SipTransportBroker@%p", this);
}

SipTransportBroker::~SipTransportBroker()
{
    JAMI_DBG("~SipTransportBroker@%p", this);

    shutdown();

    udpTransports_.clear();
    transports_.clear();

    JAMI_DBG("destroying SipTransportBroker@%p", this);
}

void
SipTransportBroker::transportStateChanged(pjsip_transport* tp,
                                          pjsip_transport_state state,
                                          const pjsip_transport_state_info* info)
{
    JAMI_DBG("pjsip transport@%p %s -> %s",
             tp, tp->info, SipTransport::stateToStr(state));

    // First make sure that this transport is handled by us
    // and remove it from any mapping if destroy pending or done.

    std::shared_ptr<SipTransport> sipTransport;

    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        auto key = transports_.find(tp);
        if (key == transports_.end()) {
            JAMI_WARN("spurious pjsip transport state change");
            return;
        }

        sipTransport = key->second.lock();

#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
        bool destroyed = state == PJSIP_TP_STATE_DESTROY;
#else
        bool destroyed = tp->is_destroying;
#endif

        // maps cleanup
        if (destroyed) {
            JAMI_DBG("unmap pjsip transport@%p {SipTransport@%p}",
                     tp, sipTransport.get());
            transports_.erase(key);

            // If UDP
            const auto type = tp->key.type;
            if (type == PJSIP_TRANSPORT_UDP or type == PJSIP_TRANSPORT_UDP6) {
                const auto updKey = std::find_if(
                    udpTransports_.cbegin(), udpTransports_.cend(),
                    [tp](const std::pair<IpAddr, pjsip_transport*>& pair) {
                        return pair.second == tp;
                    });
                if (updKey != udpTransports_.cend())
                    udpTransports_.erase(updKey);
            }
        }
    }

    // Propagate the event to the appropriate transport
    // Note the SipTransport may not be in our mappings if marked as dead
    if (sipTransport)
        sipTransport->stateCallback(state, info);
}

std::shared_ptr<SipTransport>
SipTransportBroker::addTransport(pjsip_transport* t)
{
    if (t) {
        std::lock_guard<std::mutex> lock(transportMapMutex_);

        auto key = transports_.find(t);
        if (key != transports_.end()) {
            if (auto sipTr = key->second.lock())
                return sipTr;
        }

        auto sipTr = std::make_shared<SipTransport>(t);
        if (key != transports_.end())
            key->second = sipTr;
        else
            transports_.emplace(std::make_pair(t, sipTr));
        return sipTr;
    }

    return nullptr;
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
SipTransportBroker::getUdpTransport(const IpAddr& ipAddress)
{
    std::lock_guard<std::mutex> lock(transportMapMutex_);
    auto itp = udpTransports_.find(ipAddress);
    if (itp != udpTransports_.end()) {
        auto it = transports_.find(itp->second);
        if (it != transports_.end()) {
            if (auto spt = it->second.lock()) {
                JAMI_DBG("Reusing transport %s", ipAddress.toString(true).c_str());
                return spt;
            }
            else {
                // Transport still exists but have not been destroyed yet.
                JAMI_WARN("Recycling transport %s", ipAddress.toString().c_str());
                auto ret = std::make_shared<SipTransport>(itp->second);
                it->second = ret;
                return ret;
            }
        } else {
            JAMI_WARN("Cleaning up UDP transport %s", ipAddress.toString().c_str());
            udpTransports_.erase(itp);
        }
    }
    auto ret = createUdpTransport(ipAddress);
    if (ret) {
        udpTransports_[ipAddress] = ret->get();
        transports_[ret->get()] = ret;
    }
    return ret;
}

std::shared_ptr<SipTransport>
SipTransportBroker::createUdpTransport(const IpAddr& ipAddress)
{
    RETURN_IF_FAIL(ipAddress, nullptr, "Could not determine IP address for this transport");

    pjsip_udp_transport_cfg pj_cfg;
    pjsip_udp_transport_cfg_default(&pj_cfg, ipAddress.getFamily());
    pj_cfg.bind_addr = ipAddress;
    pjsip_transport *transport = nullptr;
    if (pj_status_t status = pjsip_udp_transport_start2(endpt_, &pj_cfg, &transport)) {
        JAMI_ERR("pjsip_udp_transport_start2 failed with error %d: %s", status,
                 sip_utils::sip_strerror(status).c_str());
        JAMI_ERR("UDP IPv%s Transport did not start on %s",
            ipAddress.isIpv4() ? "4" : "6",
            ipAddress.toString(true).c_str());
        return nullptr;
    }

    JAMI_DBG("Created UDP transport on address %s", ipAddress.toString(true).c_str());
    return std::make_shared<SipTransport>(transport);
}

std::shared_ptr<TlsListener>
SipTransportBroker::getTlsListener(const IpAddr& ipAddress, const pjsip_tls_setting* settings)
{
    RETURN_IF_FAIL(settings, nullptr, "TLS settings not specified");
    RETURN_IF_FAIL(ipAddress, nullptr, "Could not determine IP address for this transport");
    JAMI_DBG("Creating TLS listener on %s...", ipAddress.toString(true).c_str());
#if 0
    JAMI_DBG(" ca_list_file : %s", settings->ca_list_file.ptr);
    JAMI_DBG(" cert_file    : %s", settings->cert_file.ptr);
    JAMI_DBG(" ciphers_num    : %d", settings->ciphers_num);
    JAMI_DBG(" verify server %d client %d client_cert %d", settings->verify_server, settings->verify_client, settings->require_client_cert);
    JAMI_DBG(" reuse_addr    : %d", settings->reuse_addr);
#endif

    pjsip_tpfactory *listener = nullptr;
    const pj_status_t status = pjsip_tls_transport_start2(endpt_, settings, ipAddress.pjPtr(), nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
        JAMI_ERR("TLS listener did not start: %s", sip_utils::sip_strerror(status).c_str());
        return nullptr;
    }
    return std::make_shared<TlsListener>(listener);
}

std::shared_ptr<SipTransport>
SipTransportBroker::getTlsTransport(const std::shared_ptr<TlsListener>& l, const IpAddr& remote, const std::string& remote_name)
{
    if (!l || !remote)
        return nullptr;
    IpAddr remoteAddr {remote};
    if (remoteAddr.getPort() == 0)
        remoteAddr.setPort(pjsip_transport_get_default_port_for_type(l->get()->type));

    JAMI_DBG("Get new TLS transport to %s", remoteAddr.toString(true).c_str());
    pjsip_tpselector sel;
    sel.type = PJSIP_TPSELECTOR_LISTENER;
    sel.u.listener = l->get();
    sel.disable_connection_reuse = PJ_FALSE;

    pjsip_tx_data tx_data;
    tx_data.dest_info.name = pj_str_t{(char*)remote_name.data(), (pj_ssize_t)remote_name.size()};

    pjsip_transport *transport = nullptr;
    pj_status_t status = pjsip_endpt_acquire_transport2(
            endpt_,
            l->get()->type,
            remoteAddr.pjPtr(),
            remoteAddr.getLength(),
            &sel,
            remote_name.empty() ? nullptr : &tx_data,
            &transport);

    if (!transport || status != PJ_SUCCESS) {
        JAMI_ERR("Could not get new TLS transport: %s", sip_utils::sip_strerror(status).c_str());
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

std::shared_ptr<SipTransport>
SipTransportBroker::getTlsIceTransport(const std::shared_ptr<jami::IceTransport>& ice,
                                       unsigned comp_id,
                                       const tls::TlsParams& params)
{
    auto ipv6 = ice->getLocalAddress(comp_id).isIpv6();
    auto type = ipv6 ? PJSIP_TRANSPORT_DTLS6 : PJSIP_TRANSPORT_DTLS;
    if (ice->isTCPEnabled()) {
        type = ipv6 ? PJSIP_TRANSPORT_TLS6 : PJSIP_TRANSPORT_TLS;
    }
    auto sip_ice_tr = std::unique_ptr<tls::SipsIceTransport>(
        new tls::SipsIceTransport(endpt_, type, params, ice, comp_id));
    auto tr = sip_ice_tr->getTransportBase();
    auto sip_tr = std::make_shared<SipTransport>(tr);
    sip_tr->setIsIceTransport();
    sip_ice_tr.release(); // managed by PJSIP now

    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        // we do not check for key existence as we've just created it
        // (member of new SipIceTransport instance)
        transports_.emplace(std::make_pair(tr, sip_tr));
    }
    return sip_tr;
}

} // namespace jami
