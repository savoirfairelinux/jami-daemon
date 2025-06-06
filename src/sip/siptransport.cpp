/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sip/siptransport.h"
#include "connectivity/sip_utils.h"

#include "jamidht/abstract_sip_transport.h"
#include "jamidht/channeled_transport.h"

#include "compiler_intrinsics.h"
#include "sip/sipvoiplink.h"

#include <pjsip.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_transport_tls.h>
#include <pj/ssl_sock.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/ip_utils.h>
#include <dhtnet/tls_session.h>

#include <opendht/crypto.h>

#include <stdexcept>
#include <sstream>
#include <algorithm>

#define RETURN_IF_FAIL(A, VAL, ...) \
    if (!(A)) { \
        JAMI_ERROR(__VA_ARGS__); \
        return (VAL); \
    }

namespace jami {

constexpr const char* TRANSPORT_STATE_STR[] = {"CONNECTED",
                                               "DISCONNECTED",
                                               "SHUTDOWN",
                                               "DESTROY",
                                               "UNKNOWN STATE"};
constexpr const size_t TRANSPORT_STATE_SZ = std::size(TRANSPORT_STATE_STR);

void
SipTransport::deleteTransport(pjsip_transport* t)
{
    pjsip_transport_dec_ref(t);
}

SipTransport::SipTransport(pjsip_transport* t)
    : transport_(nullptr, deleteTransport)
{
    if (not t or pjsip_transport_add_ref(t) != PJ_SUCCESS)
        throw std::runtime_error("Invalid transport");

    // Set pointer here, right after the successful pjsip_transport_add_ref
    transport_.reset(t);

    JAMI_DEBUG("SipTransport@{} tr={} rc={:d}",
                 fmt::ptr(this),
                 fmt::ptr(transport_.get()),
                 pj_atomic_get(transport_->ref_cnt));
}

SipTransport::SipTransport(pjsip_transport* t, const std::shared_ptr<TlsListener>& l)
    : SipTransport(t)
{
    tlsListener_ = l;
}

SipTransport::SipTransport(pjsip_transport* t,
                           const std::shared_ptr<dht::crypto::Certificate>& peerCertficate)
    : SipTransport(t)
{
    tlsInfos_.peerCert = peerCertficate;
}

SipTransport::~SipTransport()
{
    JAMI_DEBUG("~SipTransport@{} tr={} rc={:d}",
                 fmt::ptr(this),
                 fmt::ptr(transport_.get()),
                 pj_atomic_get(transport_->ref_cnt));
}

bool
SipTransport::isAlive(pjsip_transport_state state)
{
    return state != PJSIP_TP_STATE_DISCONNECTED && state != PJSIP_TP_STATE_SHUTDOWN
           && state != PJSIP_TP_STATE_DESTROY;
}

const char*
SipTransport::stateToStr(pjsip_transport_state state)
{
    return TRANSPORT_STATE_STR[std::min<size_t>(state, TRANSPORT_STATE_SZ - 1)];
}

void
SipTransport::stateCallback(pjsip_transport_state state, const pjsip_transport_state_info* info)
{
    connected_ = state == PJSIP_TP_STATE_CONNECTED;

    auto extInfo = static_cast<const pjsip_tls_state_info*>(info->ext_info);
    if (isSecure() && extInfo && extInfo->ssl_sock_info && extInfo->ssl_sock_info->established) {
        auto tlsInfo = extInfo->ssl_sock_info;
        tlsInfos_.proto = (pj_ssl_sock_proto) tlsInfo->proto;
        tlsInfos_.cipher = tlsInfo->cipher;
        tlsInfos_.verifyStatus = (pj_ssl_cert_verify_flag_t) tlsInfo->verify_status;
        if (!tlsInfos_.peerCert) {
            const auto& peers = tlsInfo->remote_cert_info->raw_chain;
            std::vector<std::pair<const uint8_t*, const uint8_t*>> bits;
            bits.resize(peers.cnt);
            std::transform(peers.cert_raw,
                           peers.cert_raw + peers.cnt,
                           std::begin(bits),
                           [](const pj_str_t& crt) {
                               return std::make_pair((uint8_t*) crt.ptr,
                                                     (uint8_t*) (crt.ptr + crt.slen));
                           });
            tlsInfos_.peerCert = std::make_shared<dht::crypto::Certificate>(bits);
        }
    } else {
        tlsInfos_ = {};
    }

    std::vector<SipTransportStateCallback> cbs;
    {
        std::lock_guard lock(stateListenersMutex_);
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
    std::lock_guard lock(stateListenersMutex_);
    auto pair = stateListeners_.insert(std::make_pair(lid, cb));
    if (not pair.second)
        pair.first->second = cb;
}

bool
SipTransport::removeStateListener(uintptr_t lid)
{
    std::lock_guard lock(stateListenersMutex_);
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
    return 1232; /* Hardcoded yes (it's the IPv6 value).
                  * This method is broken by definition.
                  * A MTU should not be defined at this layer.
                  * And a correct value should come from the underlying transport itself,
                  * not from a constant…
                  */
}

SipTransportBroker::SipTransportBroker(pjsip_endpoint* endpt)
    : endpt_(endpt)
{}

SipTransportBroker::~SipTransportBroker()
{
    shutdown();

    udpTransports_.clear();
    transports_.clear();

    JAMI_DEBUG("Destroying SipTransportBroker@{}…", fmt::ptr(this));
}

void
SipTransportBroker::transportStateChanged(pjsip_transport* tp,
                                          pjsip_transport_state state,
                                          const pjsip_transport_state_info* info)
{
    JAMI_DEBUG("PJSIP transport@{} {} → {}", fmt::ptr(tp), tp->info, SipTransport::stateToStr(state));

    // First ensure that this transport is handled by us
    // and remove it from any mapping if destroy pending or done.

    std::shared_ptr<SipTransport> sipTransport;
    std::lock_guard lock(transportMapMutex_);
    auto key = transports_.find(tp);
    if (key == transports_.end())
        return;

    sipTransport = key->second.lock();

    if (!isDestroying_ && state == PJSIP_TP_STATE_DESTROY) {
        // maps cleanup
        JAMI_DEBUG("Unmap PJSIP transport@{} {{SipTransport@{}}}", fmt::ptr(tp), fmt::ptr(sipTransport.get()));
      transports_.erase(key);

        // If UDP
        const auto type = tp->key.type;
        if (type == PJSIP_TRANSPORT_UDP or type == PJSIP_TRANSPORT_UDP6) {
            const auto updKey = std::find_if(udpTransports_.cbegin(),
                                             udpTransports_.cend(),
                                             [tp](const std::pair<dhtnet::IpAddr, pjsip_transport*>& pair) {
                                                 return pair.second == tp;
                                             });
            if (updKey != udpTransports_.cend())
                udpTransports_.erase(updKey);
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
        std::lock_guard lock(transportMapMutex_);

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
    std::unique_lock lock(transportMapMutex_);
    isDestroying_ = true;
    for (auto& t : transports_) {
        if (auto transport = t.second.lock()) {
            pjsip_transport_shutdown(transport->get());
        }
    }
}

std::shared_ptr<SipTransport>
SipTransportBroker::getUdpTransport(const dhtnet::IpAddr& ipAddress)
{
    std::lock_guard lock(transportMapMutex_);
    auto itp = udpTransports_.find(ipAddress);
    if (itp != udpTransports_.end()) {
        auto it = transports_.find(itp->second);
        if (it != transports_.end()) {
            if (auto spt = it->second.lock()) {
                JAMI_DEBUG("Reusing transport {}", ipAddress.toString(true));
                return spt;
            } else {
                // Transport still exists but have not been destroyed yet.
              JAMI_WARNING("Recycling transport {}", ipAddress.toString(true));
                auto ret = std::make_shared<SipTransport>(itp->second);
                it->second = ret;
                return ret;
            }
        } else {
          JAMI_WARNING("Cleaning up UDP transport {}", ipAddress.toString(true));
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
SipTransportBroker::createUdpTransport(const dhtnet::IpAddr& ipAddress)
{
    RETURN_IF_FAIL(ipAddress, nullptr, "Unable to determine IP address for this transport");

    pjsip_udp_transport_cfg pj_cfg;
    pjsip_udp_transport_cfg_default(&pj_cfg, ipAddress.getFamily());
    pj_cfg.bind_addr = ipAddress;
    pjsip_transport* transport = nullptr;
    if (pj_status_t status = pjsip_udp_transport_start2(endpt_, &pj_cfg, &transport)) {
        JAMI_ERROR("pjsip_udp_transport_start2 failed with error {:d}: {:s}",
                status, sip_utils::sip_strerror(status));
        JAMI_ERROR("UDP IPv{} Transport did not start on {}",
                ipAddress.isIpv4() ? "4" : "6",
                ipAddress.toString(true));
        return nullptr;
    }

    JAMI_DEBUG("Created UDP transport on address {}", ipAddress.toString(true));
    return std::make_shared<SipTransport>(transport);
}

std::shared_ptr<TlsListener>
SipTransportBroker::getTlsListener(const dhtnet::IpAddr& ipAddress, const pjsip_tls_setting* settings)
{
    RETURN_IF_FAIL(settings, nullptr, "TLS settings not specified");
    RETURN_IF_FAIL(ipAddress, nullptr, "Unable to determine IP address for this transport");
    JAMI_DEBUG("Creating TLS listener on {:s}…", ipAddress.toString(true));

    pjsip_tpfactory* listener = nullptr;
    const pj_status_t status
        = pjsip_tls_transport_start2(endpt_, settings, ipAddress.pjPtr(), nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
      JAMI_ERROR("TLS listener did not start: {}", sip_utils::sip_strerror(status));
        return nullptr;
    }
    return std::make_shared<TlsListener>(listener);
}

std::shared_ptr<SipTransport>
SipTransportBroker::getTlsTransport(const std::shared_ptr<TlsListener>& l,
                                    const dhtnet::IpAddr& remote,
                                    const std::string& remote_name)
{
    if (!l || !remote)
        return nullptr;
    dhtnet::IpAddr remoteAddr {remote};
    if (remoteAddr.getPort() == 0)
        remoteAddr.setPort(pjsip_transport_get_default_port_for_type(l->get()->type));

    JAMI_DEBUG("Get new TLS transport to {}", remoteAddr.toString(true));
    pjsip_tpselector sel;
    sel.type = PJSIP_TPSELECTOR_LISTENER;
    sel.u.listener = l->get();
    sel.disable_connection_reuse = PJ_FALSE;

    pjsip_tx_data tx_data;
    tx_data.dest_info.name = pj_str_t {(char*) remote_name.data(), (pj_ssize_t) remote_name.size()};

    pjsip_transport* transport = nullptr;
    pj_status_t status = pjsip_endpt_acquire_transport2(endpt_,
                                                        l->get()->type,
                                                        remoteAddr.pjPtr(),
                                                        remoteAddr.getLength(),
                                                        &sel,
                                                        remote_name.empty() ? nullptr : &tx_data,
                                                        &transport);

    if (!transport || status != PJ_SUCCESS) {
      JAMI_ERROR("Unable to get new TLS transport: {}", sip_utils::sip_strerror(status));
        return nullptr;
    }
    auto ret = std::make_shared<SipTransport>(transport, l);
    pjsip_transport_dec_ref(transport);
    {
        std::lock_guard lock(transportMapMutex_);
        transports_[ret->get()] = ret;
    }
    return ret;
}

std::shared_ptr<SipTransport>
SipTransportBroker::getChanneledTransport(const std::shared_ptr<SIPAccountBase>& account,
                                          const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                                          onShutdownCb&& cb)
{
    if (!socket)
        return {};
    auto sips_tr = std::make_unique<tls::ChanneledSIPTransport>(endpt_,
                                                                socket,
                                                                std::move(cb));
    auto tr = sips_tr->getTransportBase();
    auto sip_tr = std::make_shared<SipTransport>(tr, socket->peerCertificate());
    sip_tr->setDeviceId(socket->deviceId().toString());
    sip_tr->setAccount(account);

    {
        std::lock_guard lock(transportMapMutex_);
        // we do not check for key existence as we've just created it
        // (member of new SipIceTransport instance)
        transports_.emplace(tr, sip_tr);
    }

    sips_tr->start();
    sips_tr.release(); // managed by PJSIP now
    return sip_tr;
}

} // namespace jami
