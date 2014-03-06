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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>

#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdexcept>
#include <sstream>

#include "logger.h"
#include "siptransport.h"
#include "manager.h"

#include "sipaccount.h"

#include "pjsip/sip_types.h"
#if HAVE_TLS
#include "pjsip/sip_transport_tls.h"
#endif

#include "client/configurationmanager.h"

static const char * const DEFAULT_INTERFACE = "default";
static const char * const ANY_HOSTS = "0.0.0.0";

#define RETURN_IF_FAIL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return (VAL); }

std::string SipTransport::getSIPLocalIP()
{
    pj_sockaddr ip_addr;

    const pj_status_t status = pj_gethostip(pj_AF_INET(), &ip_addr);
    RETURN_IF_FAIL(status == PJ_SUCCESS, "", "Could not get local IP");
    return pj_inet_ntoa(ip_addr.ipv4.sin_addr);
}

std::vector<std::string> SipTransport::getAllIpInterfaceByName()
{
    static ifreq ifreqs[20];
    ifconf ifconf;

    std::vector<std::string> ifaceList;
    ifaceList.push_back("default");

    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof(ifreqs);

    int sock = socket(AF_INET,SOCK_STREAM,0);

    if (sock >= 0) {
        if (ioctl(sock, SIOCGIFCONF, &ifconf) >= 0)
            for (unsigned i = 0; i < ifconf.ifc_len / sizeof(ifreq); ++i)
                ifaceList.push_back(std::string(ifreqs[i].ifr_name));

        close(sock);
    }

    return ifaceList;
}

std::string SipTransport::getInterfaceAddrFromName(const std::string &ifaceName)
{
    if (ifaceName == DEFAULT_INTERFACE)
        return getSIPLocalIP();

    int fd = socket(AF_INET, SOCK_DGRAM,0);
    RETURN_IF_FAIL(fd >= 0, "", "Could not open socket: %m");

    ifreq ifr;
    strncpy(ifr.ifr_name, ifaceName.c_str(), sizeof ifr.ifr_name);
    // guarantee that ifr_name is NULL-terminated
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = AF_INET;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    sockaddr_in *saddr_in = (sockaddr_in *) &ifr.ifr_addr;
    std::string result(inet_ntoa(saddr_in->sin_addr));
    if (result == ANY_HOSTS)
        result = getSIPLocalIP();
    return result;
}

std::vector<std::string> SipTransport::getAllIpInterface()
{
    pj_sockaddr addrList[16];
    unsigned addrCnt = PJ_ARRAY_SIZE(addrList);

    std::vector<std::string> ifaceList;

    if (pj_enum_ip_interface(pj_AF_INET(), &addrCnt, addrList) == PJ_SUCCESS) {
        for (unsigned i = 0; i < addrCnt; i++) {
            char addr[PJ_INET_ADDRSTRLEN];
            pj_sockaddr_print(&addrList[i], addr, sizeof(addr), 0);
            ifaceList.push_back(std::string(addr));
        }
    }

    return ifaceList;
}

SipTransport::SipTransport(pjsip_endpoint *endpt, pj_caching_pool *cp, pj_pool_t *pool) : transportMap_(), cp_(cp), pool_(pool), endpt_(endpt)
{}

#if HAVE_TLS
pjsip_tpfactory* SipTransport::createTlsListener(SIPAccount &account)
{
    pj_sockaddr_in local_addr;
    pj_sockaddr_in_init(&local_addr, 0, 0);
    local_addr.sin_port = pj_htons(account.getTlsListenerPort());

    RETURN_IF_FAIL(account.getTlsSetting() != NULL, NULL, "TLS settings not specified");

    std::string interface(account.getLocalInterface());
    std::string listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = ANY_HOSTS;
    else
        listeningAddress = getInterfaceAddrFromName(interface);

    if (listeningAddress.empty())
        ERROR("Could not determine IP address for this transport");

    pj_str_t pjAddress;
    pj_cstr(&pjAddress, listeningAddress.c_str());
    pj_sockaddr_in_set_str_addr(&local_addr, &pjAddress);
    pj_sockaddr_in_set_port(&local_addr, account.getTlsListenerPort());

    DEBUG("Creating Listener...");
    DEBUG("CRT file : %s", account.getTlsSetting()->ca_list_file.ptr);
    DEBUG("PEM file : %s", account.getTlsSetting()->cert_file.ptr);

    pjsip_tpfactory *listener = NULL;
    const pj_status_t status = pjsip_tls_transport_start(endpt_, account.getTlsSetting(), &local_addr, NULL, 1, &listener);
    sip_strerror(status);
    RETURN_IF_FAIL(status == PJ_SUCCESS, NULL, "Failed to start TLS listener with code %d", status);
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

    pjsip_transport *transport = NULL;
    pj_status_t status = pjsip_endpt_acquire_transport(endpt_, PJSIP_TRANSPORT_TLS, &rem_addr,
                                  sizeof rem_addr, NULL, &transport);
    RETURN_IF_FAIL(transport != NULL and status == PJ_SUCCESS, NULL,
                   "Could not create new TLS transport");

    //pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    //transportMap_[transportMapKey(account.getLocalInterface(), port)] = transport;
    return transport;
}
#endif

namespace {
std::string transportMapKey(const std::string &interface, int port)
{
    std::ostringstream os;
    os << interface << ":" << port;
    return os.str();
}
}

void SipTransport::createSipTransport(SIPAccount &account)
{
    std::map<std::string, pjsip_transport *>::iterator iter;
#if HAVE_TLS
    if (account.isTlsEnabled()) {
//        std::string key(transportMapKey(account.getLocalInterface(), account.getTlsListenerPort()));
//        iter = transportMap_.find(key);
        if (account.transport_ != nullptr) {
            DEBUG("destroying old tls transport, and recreate it with new params");
            const pj_status_t status = pjsip_transport_shutdown(account.transport_);
            sip_strerror(status);
            sip_strerror(pjsip_transport_dec_ref(account.transport_));
        }

        account.transport_ = createTlsTransport(account);
    } else {
#else
    {
#endif
        // if this transport already exists, reuse it
        std::string key(transportMapKey(account.getLocalInterface(), account.getLocalPort()));
        iter = transportMap_.find(key);

        if (iter != transportMap_.end()) {
            account.transport_ = iter->second;
            pjsip_transport_add_ref(account.transport_);
        } else {
            // FIXME: transport should have its reference count decremented and
            // be removed from the map if it's no longer in use
            if (account.transport_)
                WARN("Leaking old transport");
            account.transport_ = createUdpTransport(account.getLocalInterface(), account.getLocalPort());
        }
    }

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
SipTransport::createUdpTransport(const std::string &interface, unsigned int port)
{
    // init socket to bind this transport to
    pj_uint16_t listeningPort = (pj_uint16_t) port;

    // determine the IP address for this transport
    std::string listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = ANY_HOSTS;
    else
        listeningAddress = getInterfaceAddrFromName(interface);

    RETURN_IF_FAIL(not listeningAddress.empty(), NULL, "Could not determine ip address for this transport");
    RETURN_IF_FAIL(listeningPort != 0, NULL, "Could not determine port for this transport");

    std::ostringstream fullAddress;
    fullAddress << listeningAddress << ":" << listeningPort;
    pj_str_t udpString;
    std::string fullAddressStr(fullAddress.str());
    pj_cstr(&udpString, fullAddressStr.c_str());
    pj_sockaddr boundAddr;
    pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &udpString, &boundAddr);
    pj_status_t status;
    pjsip_transport *transport = NULL;

    if (boundAddr.addr.sa_family == pj_AF_INET()) {
        status = pjsip_udp_transport_start(endpt_, &boundAddr.ipv4, NULL, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV4 Transport did not start");
            sip_strerror(status);
            return NULL;
        }
    } else if (boundAddr.addr.sa_family == pj_AF_INET6()) {
        status = pjsip_udp_transport_start6(endpt_, &boundAddr.ipv6, NULL, 1, &transport);
        if (status != PJ_SUCCESS) {
            ERROR("UDP IPV6 Transport did not start");
            sip_strerror(status);
            return NULL;
        }
    }

    DEBUG("Created UDP transport on %s:%d", interface.c_str(), port);
    DEBUG("Listening address %s", fullAddressStr.c_str());
    // dump debug information to stdout
    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    transportMap_[transportMapKey(interface, port)] = transport;

    return transport;
}


pjsip_tpselector *SipTransport::createTransportSelector(pjsip_transport *transport, pj_pool_t *tp_pool) const
{
    RETURN_IF_FAIL(transport != NULL, NULL, "Transport is not initialized");
    pjsip_tpselector *tp = (pjsip_tpselector *) pj_pool_zalloc(tp_pool, sizeof(pjsip_tpselector));
    tp->type = PJSIP_TPSELECTOR_TRANSPORT;
    tp->u.transport = transport;
    return tp;
}

std::vector<pj_sockaddr_in>
SipTransport::getSTUNAddresses(const SIPAccount &account,
        std::vector<long> &socketDescriptors) const
{
    const pj_str_t serverName = account.getStunServerName();
    const pj_uint16_t port = account.getStunPort();

    std::vector<pj_sockaddr_in> result(socketDescriptors.size());

    pj_status_t ret;
    if ((ret = pjstun_get_mapped_addr(&cp_->factory, socketDescriptors.size(), &socketDescriptors[0],
                    &serverName, port, &serverName, port, &result[0])) != PJ_SUCCESS) {
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


    for (const auto & it : result)
        WARN("STUN PORTS: %ld", pj_ntohs(it.sin_port));

    return result;
}

#define RETURN_IF_NULL(A, M, ...) if ((A) == NULL) { ERROR(M, ##__VA_ARGS__); return; }

void SipTransport::findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, std::string &addr, std::string &port) const
{

    // Initialize the sip port with the default SIP port
    std::stringstream ss;
    ss << DEFAULT_SIP_PORT;
    port = ss.str();

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport, "Transport is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());

    // get the transport manager associated with the SIP enpoint
    pjsip_tpmgr *tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    RETURN_IF_NULL(tpmgr, "Transport manager is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());

    // initialize a transport selector
    // TODO Need to determine why we exclude TLS here...
    // if (transportType == PJSIP_TRANSPORT_UDP and transport_)
    pjsip_tpselector *tp_sel = createTransportSelector(transport, pool_);
    RETURN_IF_NULL(tp_sel, "Could not initialize transport selector, using local address %s:%s", addr.c_str(), port.c_str());

    pjsip_tpmgr_fla2_param param = {transportType, tp_sel, {0,0}, PJ_FALSE, {0,0}, 0, NULL};
    if (pjsip_tpmgr_find_local_addr2(tpmgr, pool_, &param) != PJ_SUCCESS) {
        WARN("Could not retrieve local address and port from transport, using %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // Update local address based on the transport type
    addr = std::string(param.ret_addr.ptr, param.ret_addr.slen);

    // Determine the local port based on transport information
    ss.str("");
    ss << param.ret_port;
    port = ss.str();
}

void
SipTransport::findLocalAddressFromSTUN(pjsip_transport *transport,
                                       pj_str_t *stunServerName,
                                       int stunPort,
                                       std::string &addr, std::string &port) const
{
    // Initialize the sip port with the default SIP port
    std::stringstream ss;
    ss << DEFAULT_SIP_PORT;
    port = ss.str();

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport, "Transport is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());

    pj_sockaddr_in mapped_addr;
    pj_sock_t sipSocket = pjsip_udp_transport_get_socket(transport);
    const pjstun_setting stunOpt = {PJ_TRUE, *stunServerName, stunPort, *stunServerName, stunPort};
    const pj_status_t stunStatus = pjstun_get_mapped_addr2(&cp_->factory,
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

    addr = std::string(pj_inet_ntoa(mapped_addr.sin_addr));
    std::ostringstream os;
    os << pj_ntohs(mapped_addr.sin_port);
    port = os.str();

    WARN("Using address %s:%s provided by STUN server %.*s",
         addr.c_str(), port.c_str(), stunServerName->slen, stunServerName->ptr);
}

#undef RETURN_IF_NULL

void sip_strerror(pj_status_t code)
{
    char err_msg[PJ_ERR_MSG_SIZE];
    pj_strerror(code, err_msg, sizeof err_msg);
    ERROR("%d: %s", code, err_msg);
}
