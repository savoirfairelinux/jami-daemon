/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "sipaccount.h"
#include "sip_utils.h"

#include "manager.h"
#include "client/configurationmanager.h"
#include "map_utils.h"
#include "ip_utils.h"
#include "array_size.h"

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

#define RETURN_IF_FAIL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return (VAL); }

// FIXME: remove this when pjsip_tp_state_callback gives us enough info
SipTransportBroker* instance = nullptr;

constexpr const char* TRANSPORT_STATE_STR[] = {
    "CONNECTED", "DISCONNECTED", "SHUTDOWN", "DESTROY", "UNKNOWN STATE"
};
constexpr const size_t TRANSPORT_STATE_SZ = ARRAYSIZE(TRANSPORT_STATE_STR);

std::string
SipTransportDescr::toString() const
{
    std::stringstream ss;
    ss << "{" << pjsip_transport_get_type_desc(type) << " on " << interface << ":" << listenerPort  << "}";
    return ss.str();
}

SipTransport::SipTransport(pjsip_transport* t, const std::shared_ptr<TlsListener>& l) :
transport(t), tlsListener(l)
{
    pjsip_transport_add_ref(transport);
}

SipTransport::~SipTransport()
{
    if (transport) {
        pjsip_transport_shutdown(transport);
        pjsip_transport_dec_ref(transport); // ??
        DEBUG("Destroying transport (refcount: %u)",  pj_atomic_get(transport->ref_cnt));
        transport = nullptr;
    }
}

bool
SipTransport::isAlive(const std::shared_ptr<SipTransport>& t, pjsip_transport_state state)
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
    return TRANSPORT_STATE_STR[(size_t)state > TRANSPORT_STATE_SZ-1 ? TRANSPORT_STATE_SZ-1 : (size_t)state];
}

SipTransportBroker::SipTransportBroker(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool) :
cp_(cp), pool_(pool), endpt_(endpt)
{
    instance = this;
    auto status = pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), SipTransportBroker::tp_state_callback);
    if (status != PJ_SUCCESS) {
        ERROR("Can't set transport callback");
        sip_utils::sip_strerror(status);
    }
}

SipTransportBroker::~SipTransportBroker()
{
    instance = nullptr;
    pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), nullptr);
}

/** Static tranport state change callback */
void
SipTransportBroker::tp_state_callback(pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info* info)
{
    if (!instance) {
        ERROR("Can't bubble event: SipTransportBroker instance is null !");
        return;
    }
    instance->transportStateChanged(tp, state, info);
}

void
SipTransportBroker::transportStateChanged(pjsip_transport* tp, pjsip_transport_state state, const pjsip_transport_state_info* info)
{
    WARN("Transport %s -> %s", tp->info, SipTransport::stateToStr(state));
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
    if (state == PJSIP_TP_STATE_DESTROY) {
#else
    if (tp->is_destroying) {
#endif
        std::lock_guard<std::mutex> lock(transportMapMutex_);

        // Transport map cleanup
        auto t = transports_.find(tp);
        if (t != transports_.end() && t->second.expired())
            transports_.erase(t);

        // If UDP
        if (std::strlen(tp->type_name) >= 3 && std::strncmp(tp->type_name, "UDP", 3ul) == 0) {
            auto transport_key = std::find_if(udpTransports_.cbegin(), udpTransports_.cend(), [tp](const std::pair<SipTransportDescr, pjsip_transport*>& i) {
                return i.second == tp;
            });
            if (transport_key != udpTransports_.end()) {
                transports_.erase(transport_key->second);
                udpTransports_.erase(transport_key);
                transportDestroyedCv_.notify_all();
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
SipTransportBroker::waitForReleased(const SipTransportDescr& tp, std::function<void(bool)> released_cb)
{
    std::vector<std::pair<SipTransportDescr, pjsip_transport*>> to_destroy_all;
    bool destroyed = false;
    {
        std::unique_lock<std::mutex> lock(transportMapMutex_);
        auto check_destroyed = [&](){
            return udpTransports_.find(tp) == udpTransports_.end();
        };
        destroyed = transportDestroyedCv_.wait_for(lock, std::chrono::seconds(10), check_destroyed);
        if (!destroyed)
            destroyed = check_destroyed();
    }

    if (released_cb)
        released_cb(destroyed);
}

std::shared_ptr<SipTransport>
SipTransportBroker::getUpdTransport(const SipTransportDescr& descr)
{
    std::lock_guard<std::mutex> lock(transportMapMutex_);
    auto itp = udpTransports_.find(descr);
    if (itp != udpTransports_.end()) {
        auto it = transports_.find(itp->second);
        if (it != transports_.end()) {
            if (auto spt = it->second.lock()) {
                DEBUG("Reusing transport %s", descr.toString().c_str());
                return spt;
            }
            else {
                // Transport still exists but have not been destroyed yet.
                WARN("Recycling transport %s", descr.toString().c_str());
                auto ret = std::make_shared<SipTransport>(itp->second);
                it->second = ret;
                return ret;
            }
        } else {
            WARN("Cleaning up UDP transport %s", descr.toString().c_str());
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
        ERROR("UDP IPv%s Transport did not start on %s",
            listeningAddress.isIpv4() ? "4" : "6",
            listeningAddress.toString(true).c_str());
        sip_utils::sip_strerror(status);
        return nullptr;
    }

    DEBUG("Created UDP transport on %s : %s", d.interface.c_str(), listeningAddress.toString(true).c_str());
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
    DEBUG("Creating TLS listener %s on %s...", d.toString().c_str(), listeningAddress.toString(true).c_str());
    DEBUG(" ca_list_file : %s", settings->ca_list_file.ptr);
    DEBUG(" cert_file    : %s", settings->cert_file.ptr);

    pjsip_tpfactory *listener = nullptr;
    const pj_status_t status = pjsip_tls_transport_start2(endpt_, settings, listeningAddress.pjPtr(), nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
        ERROR("TLS listener did not start");
        sip_utils::sip_strerror(status);
        return nullptr;
    }
    return std::make_shared<TlsListener>(listener);
}

std::shared_ptr<SipTransport>
SipTransportBroker::getTlsTransport(const std::shared_ptr<TlsListener>& l, const std::string& remoteSipUri)
{
    if (!l) {
        ERROR("Can't create TLS transport without listener.");
        return nullptr;
    }
    static const char SIPS_PREFIX[] = "<sips:";
    size_t sips = remoteSipUri.find(SIPS_PREFIX) + (sizeof SIPS_PREFIX) - 1;
    size_t trns = remoteSipUri.find(";transport");
    IpAddr remoteAddr = {remoteSipUri.substr(sips, trns-sips)};
    if (!remoteAddr)
        return nullptr;
    if (remoteAddr.getPort() == 0)
        remoteAddr.setPort(pjsip_transport_get_default_port_for_type(l->get()->type));

    DEBUG("Get new TLS transport to %s", remoteAddr.toString(true).c_str());
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
        ERROR("Could not get new TLS transport");
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

std::vector<pj_sockaddr>
SipTransportBroker::getSTUNAddresses(const SIPAccountBase &account, std::vector<long> &socketDescriptors) const
{
    const pj_str_t serverName = account.getStunServerName();
    const pj_uint16_t port = account.getStunPort();
    const size_t ip_num = socketDescriptors.size();
    pj_sockaddr_in ipv4[ip_num];

    pj_status_t ret;
    if ((ret = pjstun_get_mapped_addr(&cp_.factory, socketDescriptors.size(), &socketDescriptors[0],
                    &serverName, port, &serverName, port, ipv4)) != PJ_SUCCESS) {
        ERROR("STUN query to server \"%.*s\" failed", serverName.slen, serverName.ptr);
        switch (ret) {
            case PJLIB_UTIL_ESTUNNOTRESPOND:
                ERROR("No response from STUN server(s)");
                break;
            case PJLIB_UTIL_ESTUNSYMMETRIC:
                ERROR("Different mapped addresses are returned by servers.");
                break;
            default:
                break;
        }
        throw std::runtime_error("Can't resolve STUN request");
    }

    std::vector<pj_sockaddr> result(ip_num);
    for(size_t i=0; i<ip_num; i++) {
        result[i].ipv4 = ipv4[i];
        WARN("STUN PORTS: %ld", pj_sockaddr_get_port(&result[i]));
    }
    return result;
}

#define RETURN_IF_NULL(A, M, ...) if ((A) == NULL) { ERROR(M, ##__VA_ARGS__); return; }

void
SipTransportBroker::findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, const std::string &host, std::string &addr, pj_uint16_t &port) const
{
    // Initialize the sip port with the default SIP port
    port = pjsip_transport_get_default_port_for_type(transportType);

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport, "Transport is NULL in findLocalAddress, using local address %s :%d", addr.c_str(), port);

    // get the transport manager associated with the SIP enpoint
    pjsip_tpmgr *tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    RETURN_IF_NULL(tpmgr, "Transport manager is NULL in findLocalAddress, using local address %s :%d", addr.c_str(), port);

    pj_str_t pjstring;
    pj_cstr(&pjstring, host.c_str());
    pjsip_tpselector tp_sel = getTransportSelector(transport);
    pjsip_tpmgr_fla2_param param = {transportType, &tp_sel, pjstring, PJ_FALSE, {nullptr, 0}, 0, nullptr};
    if (pjsip_tpmgr_find_local_addr2(tpmgr, &pool_, &param) != PJ_SUCCESS) {
        WARN("Could not retrieve local address and port from transport, using %s :%d", addr.c_str(), port);
        return;
    }

    // Update local address based on the transport type
    addr = std::string(param.ret_addr.ptr, param.ret_addr.slen);

    // Determine the local port based on transport information
    port = param.ret_port;
}

void
SipTransportBroker::findLocalAddressFromSTUN(pjsip_transport *transport,
                                       pj_str_t *stunServerName,
                                       int stunPort,
                                       std::string &addr, pj_uint16_t &port) const
{
    // Initialize the sip port with the default SIP port
    port = DEFAULT_SIP_PORT;

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport, "Transport is NULL in findLocalAddress, using local address %s:%d", addr.c_str(), port);

    IpAddr mapped_addr;
    pj_sock_t sipSocket = pjsip_udp_transport_get_socket(transport);
    const pjstun_setting stunOpt = {PJ_TRUE, *stunServerName, stunPort, *stunServerName, stunPort};
    const pj_status_t stunStatus = pjstun_get_mapped_addr2(&cp_.factory, &stunOpt, 1, &sipSocket, &static_cast<pj_sockaddr_in&>(mapped_addr));

    switch (stunStatus) {
        case PJLIB_UTIL_ESTUNNOTRESPOND:
           ERROR("No response from STUN server %.*s", stunServerName->slen, stunServerName->ptr);
           return;
        case PJLIB_UTIL_ESTUNSYMMETRIC:
           ERROR("Different mapped addresses are returned by servers.");
           return;
        case PJ_SUCCESS:
            port = mapped_addr.getPort();
            addr = mapped_addr.toString();
        default:
           break;
    }

    WARN("Using address %s provided by STUN server %.*s",
         IpAddr(mapped_addr).toString(true).c_str(), stunServerName->slen, stunServerName->ptr);
}

#undef RETURN_IF_NULL
