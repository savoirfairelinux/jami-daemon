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
#include "sipvoiplink.h"
#include "sip_utils.h"

#include "manager.h"
#include "client/configurationmanager.h"
#include "map_utils.h"
#include "ip_utils.h"

#include <pjsip.h>
#include <pjsip_ua.h>
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

#define RETURN_IF_FAIL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return (VAL); }

SipTransport::SipTransport(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool) :
transportMap_(), transportMapMutex_(), transportDestroyedCv_(), cp_(cp), pool_(pool), endpt_(endpt)
{
    auto status = pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), SipTransport::tp_state_callback);
    if (status != PJ_SUCCESS) {
        ERROR("Can't set transport callback");
        sip_utils::sip_strerror(status);
    }
}

SipTransport::~SipTransport()
{
    pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), nullptr);
}

static std::string
transportMapKey(const std::string &interface, int port, pjsip_transport_type_e type)
{
    std::ostringstream os;
    auto family = pjsip_transport_type_get_af(type);
    char af_ver_num = (family == pj_AF_INET6()) ? '6' : '4';
    if (type == PJSIP_TRANSPORT_START_OTHER) // STUN
        type = (family == pj_AF_INET6()) ?  PJSIP_TRANSPORT_UDP6 : PJSIP_TRANSPORT_UDP;
    os << interface << ':' << port << ':' << pjsip_transport_get_type_name(type) << af_ver_num;
    return os.str();
}

/** Static tranport state change callback */
void
SipTransport::tp_state_callback(pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info* /* info */)
{
    SipTransport& this_ = *SIPVoIPLink::instance().sipTransport;
    this_.transportStateChanged(tp, state);
}

void
SipTransport::transportStateChanged(pjsip_transport* tp, pjsip_transport_state state)
{
    std::lock_guard<std::mutex> lock(transportMapMutex_);
    auto transport_key = map_utils::findByValue(transportMap_, tp);
    if (transport_key == transportMap_.cend())
        return;
#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
    if (state == PJSIP_TP_STATE_SHUTDOWN || state == PJSIP_TP_STATE_DESTROY) {
#else
    if (tp->is_shutdown || tp->is_destroying) {
#endif
        WARN("Transport was destroyed: {%s}", tp->info);
        transportMap_.erase(transport_key++);
        transportDestroyedCv_.notify_all();
    }
}

void
SipTransport::waitForReleased(pjsip_transport* tp, std::function<void(bool)> released_cb)
{
    if (!released_cb)
        return;
    if (!tp) {
        released_cb(true);
        return;
    }
    std::vector<pjsip_transport*> to_destroy_all;
    bool destroyed = false;
    {
        std::unique_lock<std::mutex> lock(transportMapMutex_);
        auto check_destroyed = [&](){
            std::vector<pjsip_transport*> to_destroy = _cleanupTransports();
            bool _destr = false;
            for (auto t : to_destroy) {
                if (t == tp) {
                    _destr = true;
                    break;
                }
            }
            to_destroy_all.insert(to_destroy_all.end(), to_destroy.begin(), to_destroy.end());
            return _destr;
        };
        destroyed = transportDestroyedCv_.wait_for(lock, std::chrono::milliseconds(50), check_destroyed);
        if (!destroyed)
            destroyed = check_destroyed();
    }
    for (auto t : to_destroy_all) {
        pj_lock_release(t->lock);
        pjsip_transport_destroy(t);
    }
    released_cb(destroyed);
}

void
SipTransport::createSipTransport(SIPAccount &account)
{
    // Remove any existing transport from the account
    account.setTransport();
    cleanupTransports();

    auto type = account.getTransportType();
    auto family = pjsip_transport_type_get_af(type);
    auto interface = account.getLocalInterface();
    pjsip_transport* new_transport = nullptr;

    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        std::string key;
#if HAVE_TLS
        if (account.isTlsEnabled()) {
            key = transportMapKey(interface, account.getTlsListenerPort(), type);
            if (transportMap_.find(key) != transportMap_.end()) {
                throw std::runtime_error("TLS transport already exists");
            }
            createTlsTransport(account);
        } else {
#else
        {
#endif
            auto port = account.getLocalPort();
            key = transportMapKey(interface, port, type);
            // if this transport already exists, reuse it
            auto iter = transportMap_.find(key);
            if (iter != transportMap_.end()) {
                auto status = pjsip_transport_add_ref(iter->second);
                if (status == PJ_SUCCESS)
                    account.setTransport(iter->second);
            }
            if (!account.getTransport()) {
                account.setTransport(createUdpTransport(interface, port, family));
            }
        }

        new_transport = account.getTransport();
        if (new_transport)
            transportMap_[key] = new_transport;
    }
    cleanupTransports();

    if (!new_transport) {
#if HAVE_TLS
        if (account.isTlsEnabled())
            throw std::runtime_error("Could not create TLS connection");
        else
#endif
            throw std::runtime_error("Could not create new UDP transport");
    }
}

pjsip_transport *
SipTransport::createUdpTransport(const std::string &interface, pj_uint16_t port, pj_uint16_t family)
{
    IpAddr listeningAddress;
    if (interface == ip_utils::DEFAULT_INTERFACE)
        listeningAddress = ip_utils::getAnyHostAddr(family);
    else
        listeningAddress = ip_utils::getInterfaceAddr(interface, family);

    RETURN_IF_FAIL(listeningAddress, nullptr, "Could not determine ip address for this transport");
    RETURN_IF_FAIL(port != 0, nullptr, "Could not determine port for this transport");

    listeningAddress.setPort(port);
    pj_status_t status;
    pjsip_transport *transport = nullptr;

    if (listeningAddress.isIpv4()) {
        status = pjsip_udp_transport_start(endpt_, &static_cast<const pj_sockaddr_in&>(listeningAddress), nullptr, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV4 Transport did not start");
            sip_utils::sip_strerror(status);
            return nullptr;
        }
    } else if (listeningAddress.isIpv6()) {
        status = pjsip_udp_transport_start6(endpt_, &static_cast<const pj_sockaddr_in6&>(listeningAddress), nullptr, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV6 Transport did not start");
            sip_utils::sip_strerror(status);
            return nullptr;
        }
    }

    DEBUG("Created UDP transport on %s : %s", interface.c_str(), listeningAddress.toString(true).c_str());
    // dump debug information to stdout
    //pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    return transport;
}

#if HAVE_TLS
pjsip_tpfactory*
SipTransport::createTlsListener(SIPAccount &account, pj_uint16_t family)
{
    RETURN_IF_FAIL(account.getTlsSetting() != nullptr, nullptr, "TLS settings not specified");

    std::string interface(account.getLocalInterface());
    IpAddr listeningAddress;
    if (interface == ip_utils::DEFAULT_INTERFACE)
        listeningAddress = ip_utils::getAnyHostAddr(family);
    else
        listeningAddress = ip_utils::getInterfaceAddr(interface, family);

    listeningAddress.setPort(account.getTlsListenerPort());

    RETURN_IF_FAIL(not listeningAddress, nullptr, "Could not determine IP address for this transport");

    DEBUG("Creating Listener on %s...", listeningAddress.toString(true).c_str());
    DEBUG("CRT file : %s", account.getTlsSetting()->ca_list_file.ptr);
    DEBUG("PEM file : %s", account.getTlsSetting()->cert_file.ptr);

    pjsip_tpfactory *listener = nullptr;
    const pj_status_t status = pjsip_tls_transport_start2(endpt_, account.getTlsSetting(), listeningAddress.pjPtr(), nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
        ERROR("TLS listener did not start");
        sip_utils::sip_strerror(status);
        return nullptr;
    }
    return listener;
}

pjsip_transport *
SipTransport::createTlsTransport(SIPAccount &account)
{
    std::string remoteSipUri(account.getServerUri());
    static const char SIPS_PREFIX[] = "<sips:";
    size_t sips = remoteSipUri.find(SIPS_PREFIX) + (sizeof SIPS_PREFIX) - 1;
    size_t trns = remoteSipUri.find(";transport");
    IpAddr remoteAddr = {remoteSipUri.substr(sips, trns-sips)};
    if (!remoteAddr)
        return nullptr;

    const pjsip_transport_type_e transportType =
#if HAVE_IPV6
        remoteAddr.isIpv6() ? PJSIP_TRANSPORT_TLS6 :
#endif
        PJSIP_TRANSPORT_TLS;

    int port = pjsip_transport_get_default_port_for_type(transportType);
    if (remoteAddr.getPort() == 0)
        remoteAddr.setPort(port);

    DEBUG("Get new tls transport/listener from transport manager to %s", remoteAddr.toString(true).c_str());

    // create listener
    pjsip_tpfactory *localTlsListener = createTlsListener(account, remoteAddr.getFamily());

    // create transport
    pjsip_transport *transport = nullptr;
    pj_status_t status = pjsip_endpt_acquire_transport(
            endpt_,
            transportType,
            remoteAddr.pjPtr(),
            pj_sockaddr_get_len(remoteAddr.pjPtr()),
            nullptr,
            &transport);

    if (!transport || status != PJ_SUCCESS) {
        if (localTlsListener)
            localTlsListener->destroy(localTlsListener);
        ERROR("Could not create new TLS transport");
        sip_utils::sip_strerror(status);
        return nullptr;
    }

    account.setTransport(transport, localTlsListener);
    return transport;
}
#endif

void
SipTransport::cleanupTransports()
{
    std::vector<pjsip_transport*> to_destroy;
    {
        std::lock_guard<std::mutex> lock(transportMapMutex_);
        to_destroy = _cleanupTransports();
    }
    for (auto t : to_destroy) {
        pj_lock_release(t->lock);
        pjsip_transport_destroy(t);
    }
}

std::vector<pjsip_transport*>
SipTransport::_cleanupTransports()
{
    std::vector<pjsip_transport*> to_destroy;
    for (auto it = transportMap_.cbegin(); it != transportMap_.cend();) {
        pjsip_transport* t = (*it).second;
        if (!t) {
            transportMap_.erase(it++);
            continue;
        }
        pj_lock_acquire(t->lock);
        auto ref_cnt = pj_atomic_get(t->ref_cnt);
        if (ref_cnt == 0 || t->is_shutdown || t->is_destroying) {
            DEBUG("Removing transport for %s", t->info );
            bool is_shutdown = t->is_shutdown || t->is_destroying;
            transportMap_.erase(it++);
            if (!is_shutdown)
                to_destroy.push_back(t);
            else
                pj_lock_release(t->lock);
        } else {
            ++it;
            pj_lock_release(t->lock);
        }
    }
    return to_destroy;
}

std::vector<pj_sockaddr>
SipTransport::getSTUNAddresses(const SIPAccount &account,
        std::vector<long> &socketDescriptors) const
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

void SipTransport::findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, const std::string &host, std::string &addr, pj_uint16_t &port) const
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
SipTransport::findLocalAddressFromSTUN(pjsip_transport *transport,
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
