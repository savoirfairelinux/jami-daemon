/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

#include <stdexcept>
#include <sstream>

static const char * const DEFAULT_INTERFACE = "default";

#define RETURN_IF_FAIL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return (VAL); }

pj_sockaddr SipTransport::getSIPLocalIP(pj_uint16_t family)
{
    if (family == pj_AF_UNSPEC()) family = pj_AF_INET();
    pj_sockaddr ip_addr;
    pj_status_t status = pj_gethostip(family, &ip_addr);
    if (status == PJ_SUCCESS) return ip_addr;
    WARN("Could not get preferred IP version (%s)", (family == pj_AF_INET6()) ? "IPv6" : "IPv4");
    family = (family == pj_AF_INET()) ? pj_AF_INET6() : pj_AF_INET();
    status = pj_gethostip(family, &ip_addr);
    if (status == PJ_SUCCESS) return ip_addr;
    ERROR("Could not get local IP");
    ip_addr.addr.sa_family = pj_AF_UNSPEC();
    return ip_addr;
}

std::vector<std::string> SipTransport::getAllIpInterfaceByName()
{
    static ifreq ifreqs[20];
    ifconf ifconf;

    std::vector<std::string> ifaceList;
    ifaceList.push_back("default");

    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof(ifreqs);

    int sock = socket(AF_INET6, SOCK_STREAM, 0);

    if (sock >= 0) {
        if (ioctl(sock, SIOCGIFCONF, &ifconf) >= 0)
            for (unsigned i = 0; i < ifconf.ifc_len / sizeof(ifreq); ++i)
                ifaceList.push_back(std::string(ifreqs[i].ifr_name));

        close(sock);
    }

    return ifaceList;
}

pj_sockaddr SipTransport::getInterfaceAddr(const std::string &ifaceName, bool forceIPv6)
{
    if (ifaceName == DEFAULT_INTERFACE)
        return getSIPLocalIP(forceIPv6 ? pj_AF_INET6() : pj_AF_INET());
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if(!forceIPv6) {
        int no = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no));
    }
    pj_sockaddr saddr;
    if(fd < 0) {
        ERROR("Could not open socket: %m", fd);
        saddr.addr.sa_family = pj_AF_UNSPEC();
        return saddr;
    }
    ifreq ifr;
    strncpy(ifr.ifr_name, ifaceName.c_str(), sizeof ifr.ifr_name);
    // guarantee that ifr_name is NULL-terminated
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = AF_INET6;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    sockaddr* unix_addr = &ifr.ifr_addr;
    memcpy(&saddr, &ifr.ifr_addr, sizeof(pj_sockaddr));
    if ((ifr.ifr_addr.sa_family == AF_INET  &&  IN_IS_ADDR_UNSPECIFIED(&((sockaddr_in *)unix_addr)->sin_addr ))
    || (ifr.ifr_addr.sa_family == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(&((sockaddr_in6*)unix_addr)->sin6_addr))) {
        return getSIPLocalIP(saddr.addr.sa_family);
    }
    return saddr;
}

std::string SipTransport::getInterfaceAddrFromName(const std::string &ifaceName, bool forceIPv6)
{
    return sip_utils::addrToStr(getInterfaceAddr(ifaceName, forceIPv6));
}

std::vector<std::string> SipTransport::getAllIpInterface()
{
    pj_sockaddr addrList[16];
    unsigned addrCnt = PJ_ARRAY_SIZE(addrList);

    std::vector<std::string> ifaceList;

    if (pj_enum_ip_interface(pj_AF_UNSPEC(), &addrCnt, addrList) == PJ_SUCCESS) {
        for (unsigned i = 0; i < addrCnt; i++) {
            char addr[PJ_INET6_ADDRSTRLEN];
            pj_sockaddr_print(&addrList[i], addr, sizeof(addr), 0);
            ifaceList.push_back(std::string(addr));
        }
    }

    return ifaceList;
}

SipTransport::SipTransport(pjsip_endpoint *endpt, pj_caching_pool& cp, pj_pool_t& pool) : transportMap_(), cp_(cp), pool_(pool), endpt_(endpt)
{}

namespace {
std::string transportMapKey(const std::string &interface, int port)
{
    std::ostringstream os;
    os << interface << ":" << port;
    return os.str();
}
}

void SipTransport::createSipTransport(SIPAccount &account, pj_uint16_t family)
{
    if (account.transport_) {
        pjsip_transport_dec_ref(account.transport_);
        //DEBUG("Transport %s has count %d", account.transport_->info, pj_atomic_get(account.transport_->ref_cnt));
        account.transport_ = nullptr;
    }
    auto interface = account.getLocalInterface();
    if (family == pj_AF_UNSPEC()) family = account.getPublishedIpAddress().addr.sa_family;

#if HAVE_TLS
    if (account.isTlsEnabled()) {
        std::string key(transportMapKey(interface, account.getTlsListenerPort()));
        auto iter = transportMap_.find(key);

        // if this transport already exists, reuse it
        if (iter != transportMap_.end()) {
            account.transport_ = iter->second;
            auto status = pjsip_transport_add_ref(account.transport_);
            if (status != PJ_SUCCESS)
                account.transport_ = nullptr;
        }
        if (!account.transport_) {
            account.transport_ = createTlsTransport(account);
            transportMap_[key] = account.transport_;
        }
    } else {
#else
    {
#endif
        auto port = account.getLocalPort();
        std::string key = transportMapKey(interface, port);
        auto iter = transportMap_.find(key);

        // if this transport already exists, reuse it
        if (iter != transportMap_.end()) {
            account.transport_ = iter->second;
            auto status = pjsip_transport_add_ref(account.transport_);
            if (status != PJ_SUCCESS)
                account.transport_ = nullptr;
        }
        if (!account.transport_) {
            account.transport_ = createUdpTransport(interface, port);
            transportMap_[key] = account.transport_;
        }
    }

    cleanupTransports();

    if (!account.transport_) {
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
    pj_sockaddr listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = ip_utils::getAnyHostAddr(family);
    else
        listeningAddress = getInterfaceAddr(interface, family == pj_AF_INET6());

    RETURN_IF_FAIL(not listeningAddress.addr.sa_family == pj_AF_UNSPEC(), nullptr, "Could not determine ip address for this transport");
    RETURN_IF_FAIL(port != 0, nullptr, "Could not determine port for this transport");

    pj_sockaddr_set_port(&listeningAddress, port);
    pj_status_t status;
    pjsip_transport *transport = nullptr;

    if (listeningAddress.addr.sa_family == pj_AF_INET()) {
        status = pjsip_udp_transport_start(endpt_, &listeningAddress.ipv4, nullptr, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV4 Transport did not start");
            sip_utils::sip_strerror(status);
            return nullptr;
        }
    } else if (listeningAddress.addr.sa_family == pj_AF_INET6()) {
        status = pjsip_udp_transport_start6(endpt_, &listeningAddress.ipv6, nullptr, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV6 Transport did not start");
            sip_utils::sip_strerror(status);
            return nullptr;
        }
    }

    DEBUG("Created UDP transport on %s : %s", interface.c_str(), ip_utils::addrToStr(listeningAddress, true, true).c_str());
    // dump debug information to stdout
    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    return transport;
}

#if HAVE_TLS
pjsip_tpfactory*
SipTransport::createTlsListener(SIPAccount &account, pj_uint16_t family)
{
    RETURN_IF_FAIL(account.getTlsSetting() != nullptr, nullptr, "TLS settings not specified");

    std::string interface(account.getLocalInterface());
    pj_sockaddr listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = ip_utils::getAnyHostAddr(family);
    else
        listeningAddress = getInterfaceAddr(interface, family==pj_AF_INET6());

    pj_sockaddr_set_port(&listeningAddress, account.getTlsListenerPort());

    RETURN_IF_FAIL(not listeningAddress.addr.sa_family == pj_AF_UNSPEC(), nullptr, "Could not determine IP address for this transport");

    DEBUG("Creating Listener on %s...", ip_utils::addrToStr(listeningAddress).c_str());
    DEBUG("CRT file : %s", account.getTlsSetting()->ca_list_file.ptr);
    DEBUG("PEM file : %s", account.getTlsSetting()->cert_file.ptr);

    pjsip_tpfactory *listener = nullptr;
    const pj_status_t status = pjsip_tls_transport_start2(endpt_, account.getTlsSetting(), &listeningAddress, nullptr, 1, &listener);
    if (status != PJ_SUCCESS) {
        ERROR("TLS Transport did not start");
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
    std::string remoteAddr(remoteSipUri.substr(sips, trns-sips));
    std::string ipAddr = "";
    int port = DEFAULT_SIP_TLS_PORT;

    // parse c string
    size_t pos = remoteAddr.find(":");
    if (pos != std::string::npos) {
        ipAddr = remoteAddr.substr(0, pos);
        port = atoi(remoteAddr.substr(pos + 1, remoteAddr.length() - pos).c_str());
    } else
        ipAddr = remoteAddr;

    pj_str_t remote;
    pj_cstr(&remote, ipAddr.c_str());

    pj_sockaddr_in rem_addr;
    pj_sockaddr_in_init(&rem_addr, &remote, (pj_uint16_t) port);

    DEBUG("Get new tls transport/listener from transport manager");
    // The local tls listener
    // FIXME: called only once as it is static -> that's why parameters are not saved
    pjsip_tpfactory *localTlsListener = createTlsListener(account);

    pjsip_transport *transport = nullptr;
    pj_status_t status = pjsip_endpt_acquire_transport(endpt_, PJSIP_TRANSPORT_TLS, &rem_addr,
                                  sizeof rem_addr, NULL, &transport);
    RETURN_IF_FAIL(transport != nullptr and status == PJ_SUCCESS, nullptr,
                   "Could not create new TLS transport");

    //pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    return transport;
}
#endif

void
SipTransport::cleanupTransports()
{
    for (auto it = transportMap_.cbegin(); it != transportMap_.cend();) {
        pjsip_transport* t = (*it).second;
        pj_lock_acquire(t->lock);
        if (pj_atomic_get(t->ref_cnt) == 0 || t->is_shutdown || t->is_destroying) {
            DEBUG("Removing transport for %s (%s)", (*it).first.c_str(), pj_atomic_get(t->ref_cnt) );
            transportMap_.erase(it++);
        } else {
            ++it;
        }
        pj_lock_release(t->lock);
    }
}

pjsip_tpselector *SipTransport::createTransportSelector(pjsip_transport *transport, pj_pool_t *tp_pool) const
{
    RETURN_IF_FAIL(transport != NULL, NULL, "Transport is not initialized");
    pjsip_tpselector *tp = (pjsip_tpselector *) pj_pool_zalloc(tp_pool, sizeof(pjsip_tpselector));
    tp->type = PJSIP_TPSELECTOR_TRANSPORT;
    tp->u.transport = transport;
    return tp;
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
        result[i].addr.sa_family = pj_AF_INET();
        result[i].ipv4 = ipv4[i];
        WARN("STUN PORTS: %ld", pj_ntohs(ipv4[i].sin_port));
    }
    return result;
}

#define RETURN_IF_NULL(A, M, ...) if ((A) == NULL) { ERROR(M, ##__VA_ARGS__); return; }

void SipTransport::findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, std::string &addr, pj_uint16_t &port) const
{
    // Initialize the sip port with the default SIP port
    port = DEFAULT_SIP_PORT;

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport, "Transport is NULL in findLocalAddress, using local address %s :%d", addr.c_str(), port);

    // get the transport manager associated with the SIP enpoint
    pjsip_tpmgr *tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    RETURN_IF_NULL(tpmgr, "Transport manager is NULL in findLocalAddress, using local address %s :%d", addr.c_str(), port);

    // initialize a transport selector
    // TODO Need to determine why we exclude TLS here...
    // if (transportType == PJSIP_TRANSPORT_UDP and transport_)
    pjsip_tpselector *tp_sel = createTransportSelector(transport, &pool_);
    RETURN_IF_NULL(tp_sel, "Could not initialize transport selector, using local address %s :%d", addr.c_str(), port);

    pjsip_tpmgr_fla2_param param = {transportType, tp_sel, {nullptr,0}, PJ_FALSE, {nullptr,0}, 0, nullptr};
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

    pj_sockaddr_in mapped_addr;
    pj_sock_t sipSocket = pjsip_udp_transport_get_socket(transport);
    const pjstun_setting stunOpt = {PJ_TRUE, *stunServerName, stunPort, *stunServerName, stunPort};
    const pj_status_t stunStatus = pjstun_get_mapped_addr2(&cp_.factory,
            &stunOpt, 1, &sipSocket, &mapped_addr);

    switch (stunStatus) {
        case PJLIB_UTIL_ESTUNNOTRESPOND:
           ERROR("No response from STUN server %.*s", stunServerName->slen, stunServerName->ptr);
           return;
        case PJLIB_UTIL_ESTUNSYMMETRIC:
           ERROR("Different mapped addresses are returned by servers.");
           return;
        default:
           break;
    }

    port = pj_sockaddr_get_port(&mapped_addr);

    WARN("Using address %s:%d provided by STUN server %.*s",
         addr.c_str(), port, stunServerName->slen, stunServerName->ptr);
}

#undef RETURN_IF_NULL
