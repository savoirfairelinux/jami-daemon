/*
 *  Copyright (C) [2004, 2012] Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "logger.h"
#include "siptransport.h"
#include "manager.h"

#include "sip/sdp.h"
#include "sipcall.h"
#include "sipaccount.h"
#include "eventthread.h"
#include "sdes_negotiator.h"

#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"
#include "dbus/configurationmanager.h"

static const char * const DEFAULT_INTERFACE = "default";

static pjsip_transport *localUDPTransport_ = NULL; /** The default transport (5060) */

std::string SipTransport::getSIPLocalIP()
{
    pj_sockaddr ip_addr;

    if (pj_gethostip(pj_AF_INET(), &ip_addr) == PJ_SUCCESS)
        return pj_inet_ntoa(ip_addr.ipv4.sin_addr);
    else  {
        ERROR("SipTransport: Could not get local IP");
        return "";
    }
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
    int fd = socket(AF_INET, SOCK_DGRAM,0);

    if (fd < 0) {
        ERROR("SipTransport: Error: could not open socket: %m");
        return "";
    }

    ifreq ifr;
    strcpy(ifr.ifr_name, ifaceName.c_str());
    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = AF_INET;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    sockaddr_in *saddr_in = (sockaddr_in *) &ifr.ifr_addr;
    return inet_ntoa(saddr_in->sin_addr);
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

SipTransport::SipTransport(pjsip_endpoint *endpt, pj_caching_pool *cp, pj_pool_t *pool) : transportMap_(), stunSocketMap_(), cp_(cp), pool_(pool), endpt_(endpt)
{
}

SipTransport::~SipTransport()
{
}

pj_bool_t
stun_sock_on_status_cb(pj_stun_sock * /*stun_sock*/, pj_stun_sock_op op,
                       pj_status_t status)
{
    switch (op) {
        case PJ_STUN_SOCK_DNS_OP:
            DEBUG("SipTransport: Stun operation dns resolution");
            break;
        case PJ_STUN_SOCK_BINDING_OP:
            DEBUG("SipTransport: Stun operation binding");
            break;
        case PJ_STUN_SOCK_KEEP_ALIVE_OP:
            DEBUG("SipTransport: Stun operation keep alive");
            break;
        case PJ_STUN_SOCK_MAPPED_ADDR_CHANGE:
            DEBUG("SipTransport: Stun operation address mapping change");
            break;
        default:
            DEBUG("SipTransport: Stun unknown operation");
            break;
    }

    if (status == PJ_SUCCESS) {
        DEBUG("SipTransport: Stun operation success");
    } else {
        ERROR("SipTransport: Stun operation failure");
    }

    // Always return true so the stun transport registration retry even on failure
    return true;
}

static pj_bool_t
stun_sock_on_rx_data_cb(pj_stun_sock * /*stun_sock*/, void * /*pkt*/,
                        unsigned /*pkt_len*/,
                        const pj_sockaddr_t * /*src_addr*/,
                        unsigned /*addr_len*/)
{
    return PJ_TRUE;
}


pj_status_t SipTransport::createStunResolver(pj_str_t serverName, pj_uint16_t port)
{
    pj_stun_config stunCfg;
    pj_stun_config_init(&stunCfg, &cp_->factory, 0, pjsip_endpt_get_ioqueue(endpt_), pjsip_endpt_get_timer_heap(endpt_));

    DEBUG("***************** Create Stun Resolver *********************");

    static const pj_stun_sock_cb stun_sock_cb = {
        stun_sock_on_rx_data_cb,
        NULL,
        stun_sock_on_status_cb
    };

    pj_stun_sock *stun_sock;
    std::string stunResolverName(serverName.ptr, serverName.slen);
    pj_status_t status = pj_stun_sock_create(&stunCfg, stunResolverName.c_str(), pj_AF_INET(), &stun_sock_cb, NULL, NULL, &stun_sock);

    // store socket inside list
    DEBUG("     insert %s resolver in map", stunResolverName.c_str());
    stunSocketMap_.insert(std::pair<std::string, pj_stun_sock *>(stunResolverName, stun_sock));

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        ERROR("SipTransport: Error creating STUN socket for %.*s: %s", (int) serverName.slen, serverName.ptr, errmsg);
        return status;
    }

    status = pj_stun_sock_start(stun_sock, &serverName, port, NULL);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        DEBUG("SipTransport: Error starting STUN socket for %.*s: %s", (int) serverName.slen, serverName.ptr, errmsg);
        pj_stun_sock_destroy(stun_sock);
    }

    return status;
}

pj_status_t SipTransport::destroyStunResolver(const std::string &serverName)
{
    std::map<std::string, pj_stun_sock *>::iterator it;
    it = stunSocketMap_.find(serverName);

    DEBUG("***************** Destroy Stun Resolver *********************");

    std::map<std::string, pj_stun_sock *>::iterator iter;
    for(iter = stunSocketMap_.begin(); iter != stunSocketMap_.end(); iter++)
        DEBUG("stun reslover: %s", iter->first.c_str());

    if (it != stunSocketMap_.end()) {
        DEBUG("SipTransport: Deleting stun resolver %s", it->first.c_str());
        pj_stun_sock_destroy(it->second);
        stunSocketMap_.erase(it);
    }

    return PJ_SUCCESS;
}


void SipTransport::createTlsListener(const std::string &interface, pj_uint16_t tlsListenerPort, pjsip_tls_setting *tlsSetting, pjsip_tpfactory **listener)
{
    pj_status_t status;
    pj_sockaddr_in local_addr;
    pj_sockaddr_in_init(&local_addr, 0, 0);
    local_addr.sin_port = pj_htons(tlsListenerPort);

    DEBUG("SipTransport: Create TLS listener %s:%d", interface.c_str(), tlsListenerPort);

    if (tlsSetting == NULL) {
        ERROR("SipTransport: Error TLS settings not specified");
        return;
    }

    if (listener == NULL) {
        ERROR("SipTransport: Error no pointer to store new TLS listener");
        return;
    }

    std::string listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = getSIPLocalIP();
    else
        listeningAddress = getInterfaceAddrFromName(interface);

    if (listeningAddress.empty()) {
        ERROR("SipTransport: Could not determine ip address for this transport");
    }

    pj_str_t pjAddress;
    pj_cstr(&pjAddress, listeningAddress.c_str());
    pj_sockaddr_in_set_str_addr(&local_addr, &pjAddress);
    pj_sockaddr_in_set_port(&local_addr, tlsListenerPort);

    status = pjsip_tls_transport_start(endpt_, tlsSetting, &local_addr, NULL, 1, listener);
    if(status != PJ_SUCCESS)
        ERROR("SipTransport: Error Failed to start tls listener");
}


pjsip_transport *
SipTransport::createTlsTransport(const std::string &remoteAddr,
                                const std::string &interface,
                                pj_uint16_t tlsListenerPort,
                                pjsip_tls_setting *tlsSettings)
{
    pjsip_transport *transport = NULL;
    std::string ipAddr = "";
    int port = DEFAULT_SIP_TLS_PORT;

    // parse c string
    size_t pos = remoteAddr.find(":");
    if(pos != std::string::npos) {
        ipAddr = remoteAddr.substr(0, pos);
        port = atoi(remoteAddr.substr(pos+1, remoteAddr.length() - pos).c_str());
    }
    else {
        ipAddr = remoteAddr;
    }

    DEBUG("SipTransport: Create sip transport on %s:%d at destination %s:%d", interface.c_str(), tlsListenerPort, ipAddr.c_str(), port);

    pj_str_t remote;
    pj_cstr(&remote, ipAddr.c_str());

    pj_sockaddr_in rem_addr;
    pj_sockaddr_in_init(&rem_addr, &remote, (pj_uint16_t) port);

    // The local tls listener
    static pjsip_tpfactory *localTlsListener = NULL;

    if (localTlsListener == NULL)
        createTlsListener(interface, tlsListenerPort, tlsSettings, &localTlsListener);

    DEBUG("SipTransport: Get new tls transport from transport manager");
    pjsip_endpt_acquire_transport(endpt_, PJSIP_TRANSPORT_TLS, &rem_addr,
                                  sizeof rem_addr, NULL, &transport);

    if (transport == NULL)
        ERROR("SipTransport: Could not create new TLS transport\n");

    return transport;
}

void SipTransport::createSipTransport(SIPAccount *account)
{
    if (account == NULL) {
        ERROR("SipTransport: Account is NULL while creating sip transport");
        return;
    }

    shutdownSipTransport(account);

    if (account->isTlsEnabled()) {
        std::string remoteSipUri(account->getServerUri());
        static const char SIPS_PREFIX[] = "<sips:";
        size_t sips = remoteSipUri.find(SIPS_PREFIX) + (sizeof SIPS_PREFIX) - 1;
        size_t trns = remoteSipUri.find(";transport");
        std::string remoteAddr(remoteSipUri.substr(sips, trns-sips));

        pjsip_transport *transport = createTlsTransport(remoteAddr, account->getLocalInterface(), account->getTlsListenerPort(), account->getTlsSetting());
        account->transport_ = transport;
    } else if (account->isStunEnabled()) {
        pjsip_transport *transport = createStunTransport(account->getStunServerName(), account->getStunPort());
        if(transport == NULL)
            transport = createUdpTransport(account->getLocalInterface(), account->getLocalPort());
        account->transport_ = transport;
    }
    else {
        pjsip_transport *transport = createUdpTransport(account->getLocalInterface(), account->getLocalPort());
        account->transport_ = transport;
    }



    if (!account->transport_) {
        DEBUG("SipTransport: Looking into previously created transport map for %s:%d",
                              account->getLocalInterface().c_str(), account->getLocalPort());
        // Could not create new transport, this transport may already exists
        account->transport_ = transportMap_[account->getLocalPort()];

        if (account->transport_)
            pjsip_transport_add_ref(account->transport_);
        else {
            account->transport_ = localUDPTransport_;
            account->setLocalPort(localUDPTransport_->local_name.port);
        }
    }

    if(account->transport_ == NULL)
        ERROR("SipTransport: Could not create transport on %s:%d",
                              account->getLocalInterface().c_str(), account->getLocalPort());
}

void SipTransport::createDefaultSipUdpTransport()
{
    pj_uint16_t port = 0;
    int counter = 0;

    DEBUG("SipTransport: Create default sip udp transport");

    SIPAccount *account = Manager::instance().getIP2IPAccount();

    pjsip_transport *transport = NULL;
    static const int DEFAULT_TRANSPORT_ATTEMPTS = 5;
    for (; transport == NULL and counter < DEFAULT_TRANSPORT_ATTEMPTS; ++counter) {
        // if default udp transport fails to init on 5060, try other ports
        // with 2 step size increment (i.e. 5062, 5064, ...)
        port = account->getLocalPort() + (counter * 2);
        transport = createUdpTransport(account->getLocalInterface(), port);
    }

    if (transport == NULL) {
        ERROR("SipTransport: Create UDP transport");
        return;
    }

    DEBUG("SipTransport: Created default sip transport on %d", port);

    // set transport for this account
    account->transport_ = transport;

    // set local udp transport
    localUDPTransport_ = account->transport_;
}

pjsip_transport *
SipTransport::createUdpTransport(const std::string &interface, unsigned int port)
{
    // init socket to bind this transport to
    pj_uint16_t listeningPort = (pj_uint16_t) port;

    DEBUG("SipTransport: Create UDP transport on %s:%d", interface.c_str(), port);

    // determine the ip address for this transport
    std::string listeningAddress;
    if (interface == DEFAULT_INTERFACE)
        listeningAddress = getSIPLocalIP();
    else
        listeningAddress = getInterfaceAddrFromName(interface);

    if (listeningAddress.empty()) {
        ERROR("SipTransport: Could not determine ip address for this transport");
        return NULL;
    }

    if (listeningPort == 0) {
        ERROR("SipTransport: Could not determine port for this transport");
        return NULL;
    }

    pj_sockaddr boundAddr;
    pj_str_t udpString;
    pj_cstr(&udpString, listeningAddress.c_str());
    pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &udpString, &boundAddr);
    pj_status_t status;
    pjsip_transport *transport = NULL;
    if (boundAddr.addr.sa_family == pj_AF_INET()) {
        boundAddr.ipv4.sin_port = listeningPort;
        status = pjsip_udp_transport_start(endpt_, &boundAddr.ipv4, NULL, 1, &transport);
        if (status != PJ_SUCCESS) {
            return NULL;
        }
    } else if (boundAddr.addr.sa_family == pj_AF_INET6()) {
        boundAddr.ipv6.sin6_port = listeningPort;
        status = pjsip_udp_transport_start6(endpt_, &boundAddr.ipv6, NULL, 1, &transport);
        if (status != PJ_SUCCESS) {
            return NULL;
        }
    }

    DEBUG("SipTransport: Listening address %s, listening port %d", listeningAddress.c_str(), listeningPort);
    // dump debug information to stdout
    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
    transportMap_[listeningPort] = transport;

    return transport;
}

pjsip_tpselector *SipTransport::initTransportSelector(pjsip_transport *transport, pj_pool_t *tp_pool) const
{
    assert(transport);
    pjsip_tpselector *tp = (pjsip_tpselector *) pj_pool_zalloc(tp_pool, sizeof(pjsip_tpselector));
    tp->type = PJSIP_TPSELECTOR_TRANSPORT;
    tp->u.transport = transport;
    return tp;
}

pjsip_transport *SipTransport::createStunTransport(pj_str_t serverName, pj_uint16_t port)
{
    pjsip_transport *transport;

    DEBUG("SipTransport: Create stun transport  server name: %s, port: %d", serverName, port);// account->getStunPort());
    if (createStunResolver(serverName, port) != PJ_SUCCESS) {
        ERROR("SipTransport: Can't resolve STUN server");
        Manager::instance().getDbusManager()->getConfigurationManager()->stunStatusFailure("");
        return NULL;
    }

    pj_sock_t sock = PJ_INVALID_SOCKET;

    pj_sockaddr_in boundAddr;

    if (pj_sockaddr_in_init(&boundAddr, &serverName, 0) != PJ_SUCCESS) {
        ERROR("SipTransport: Can't initialize IPv4 socket on %*s:%i", serverName.slen, serverName.ptr, port);
        Manager::instance().getDbusManager()->getConfigurationManager()->stunStatusFailure("");
        return NULL;
    }

    if (pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock) != PJ_SUCCESS) {
        ERROR("SipTransport: Can't create or bind socket");
        Manager::instance().getDbusManager()->getConfigurationManager()->stunStatusFailure("");
        return NULL;
    }

    // Query the mapped IP address and port on the 'outside' of the NAT
    pj_sockaddr_in pub_addr;

    if (pjstun_get_mapped_addr(&cp_->factory, 1, &sock, &serverName, port, &serverName, port, &pub_addr) != PJ_SUCCESS) {
        ERROR("SipTransport: Can't contact STUN server");
        pj_sock_close(sock);
        Manager::instance().getDbusManager()->getConfigurationManager()->stunStatusFailure("");
        return NULL;
    }

    pjsip_host_port a_name = {
        pj_str(pj_inet_ntoa(pub_addr.sin_addr)),
        pj_ntohs(pub_addr.sin_port)
    };

    pjsip_udp_transport_attach2(endpt_, PJSIP_TRANSPORT_UDP, sock, &a_name, 1,
                                &transport);

    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));

    return transport;
}

void SipTransport::shutdownSipTransport(SIPAccount *account)
{
    if (account->isStunEnabled()) {
        pj_str_t stunServerName = account->getStunServerName();
        std::string server(stunServerName.ptr, stunServerName.slen);
        destroyStunResolver(server);
    }

    if (account->transport_) {
        pjsip_transport_dec_ref(account->transport_);
        account->transport_ = NULL;
    }
}

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
    if (!transport) {
        ERROR("SipTransport: Transport is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // get the transport manager associated with the SIP enpoint
    pjsip_tpmgr *tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    if (!tpmgr) {
        ERROR("SipTransport: Transport manager is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // initialize a transport selector
    // TODO Need to determine why we exclude TLS here...
    // if (transportType == PJSIP_TRANSPORT_UDP and transport_)
    pjsip_tpselector *tp_sel = initTransportSelector(transport, pool_);
    if (!tp_sel) {
        ERROR("SipTransport: Could not initialize transport selector, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    pj_str_t localAddress = {0,0};
    int i_port = 0;

    // Find the local address and port for this transport
    if (pjsip_tpmgr_find_local_addr(tpmgr, pool_, transportType, tp_sel, &localAddress, &i_port) != PJ_SUCCESS) {
        WARN("SipTransport: Could not retrieve local address and port from transport, using %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // Update local address based on the transport type
    addr = std::string(localAddress.ptr, localAddress.slen);

    // Fallback on local ip provided by pj_gethostip()
    if (addr == "0.0.0.0")
        addr = getSIPLocalIP();

    // Determine the local port based on transport information
    ss.str("");
    ss << i_port;
    port = ss.str();
}
