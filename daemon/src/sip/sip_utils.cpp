/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <pj/string.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_uri.h>
#include <pj/list.h>
#include "sip_utils.h"

// for resolveDns
#include <vector>
#include <set>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logger.h"

std::string
sip_utils::fetchHeaderValue(pjsip_msg *msg, const std::string &field)
{
    pj_str_t name = pj_str((char*) field.c_str());
    pjsip_generic_string_hdr *hdr = static_cast<pjsip_generic_string_hdr*>(pjsip_msg_find_hdr_by_name(msg, &name, NULL));

    if (!hdr)
        return "";

    std::string value(hdr->hvalue.ptr, hdr->hvalue.slen);

    size_t pos = value.find("\n");

    if (pos != std::string::npos)
        return value.substr(0, pos);
    else
        return "";
}

pjsip_route_hdr *
sip_utils::createRouteSet(const std::string &route, pj_pool_t *hdr_pool)
{
    pjsip_route_hdr *route_set = pjsip_route_hdr_create(hdr_pool);

    std::string host;
    int port = 0;
    size_t found = route.find(":");
    if (found != std::string::npos) {
        host = route.substr(0, found);
        port = atoi(route.substr(found + 1, route.length() - found).c_str());
    } else
        host = route;

    pjsip_route_hdr *routing = pjsip_route_hdr_create(hdr_pool);
    pjsip_sip_uri *url = pjsip_sip_uri_create(hdr_pool, 0);
    url->lr_param = 1;
    routing->name_addr.uri = (pjsip_uri*) url;
    pj_strdup2(hdr_pool, &url->host, host.c_str());
    url->port = port;

    DEBUG("Adding route %s", host.c_str());
    pj_list_push_back(route_set, pjsip_hdr_clone(hdr_pool, routing));

    return route_set;
}

pjsip_route_hdr *
sip_utils::createRouteSetList(const std::string &route, pj_pool_t *hdr_pool)
{
    pjsip_route_hdr *route_set = pjsip_route_hdr_create(hdr_pool);

    std::string host;
    int port = 0;
    size_t found = route.find(":");
    if (found != std::string::npos) {
        host = route.substr(0, found);
        port = atoi(route.substr(found + 1, route.length() - found).c_str());
    } else
        host = route;

    std::set<std::string> ipList(getIPList(host));
    for (std::set<std::string>::const_iterator iter = ipList.begin(); iter != ipList.end(); ++iter) {

        pjsip_route_hdr *routing = pjsip_route_hdr_create(hdr_pool);
        pjsip_sip_uri *url = pjsip_sip_uri_create(hdr_pool, 0);
        url->lr_param = 1;
        routing->name_addr.uri = (pjsip_uri*) url;
        pj_strdup2(hdr_pool, &url->host, iter->c_str());
        url->port = port;

        DEBUG("Adding route %s", iter->c_str());
        pj_list_push_back(route_set, pjsip_hdr_clone(hdr_pool, routing));
    }

    return route_set;
}

std::string
sip_utils::parseDisplayName(const char * buffer)
{
    const char* from_header = strstr(buffer, "From: ");

    if (!from_header)
        return "";

    std::string temp(from_header);
    size_t begin_displayName = temp.find("\"") + 1;
    size_t end_displayName = temp.rfind("\"");
    std::string displayName(temp.substr(begin_displayName, end_displayName - begin_displayName));

    static const size_t MAX_DISPLAY_NAME_SIZE = 25;
    if (displayName.size() > MAX_DISPLAY_NAME_SIZE)
        return "";

    return displayName;
}

void
sip_utils::stripSipUriPrefix(std::string& sipUri)
{
    // Remove sip: prefix
    static const char SIP_PREFIX[] = "sip:";
    size_t found = sipUri.find(SIP_PREFIX);

    if (found != std::string::npos)
        sipUri.erase(found, found + (sizeof SIP_PREFIX) - 1);

    found = sipUri.find("@");

    if (found != std::string::npos)
        sipUri.erase(found);
}

/**
 * This function looks for '@' and replaces the second part with the corresponding ip address (when possible)
 */
std::string
sip_utils::resolveDns(const std::string &url, pjsip_endpoint * /*endpt*/, pj_pool_t * /*pool*/)
{
   size_t pos;
   if ((pos = url.find("@")) == std::string::npos)
      return url;

   const std::string hostname = url.substr(pos + 1);

   std::set<std::string> ipList(getIPList(hostname));
   if (not ipList.empty() and ipList.begin()->size() > 7)
       return url.substr(0, pos + 1) + *ipList.begin();
   else
       return hostname;
}

#if 0
static const int UNRESOLVED = 0x12345678;
struct ResolveResult {
    ResolveResult() : status(UNRESOLVED), servers() {}
    pj_status_t status;
    pjsip_server_addresses servers;
};

static void resolve_cb(pj_status_t status, void *token,
                       const struct pjsip_server_addresses *addr)
{
    ResolveResult *result = static_cast<ResolveResult*>(token);

    result->status = status;
    if (status == PJ_SUCCESS)
        pj_memcpy(&result->servers, addr, sizeof(*addr));
    else
        ERROR("Could not resolve");
}

std::vector<std::string>
sip_utils::resolve(pjsip_endpoint *endpt, pj_pool_t *pool, const std::string &name)
{
    pjsip_host_info dest;
    ResolveResult result;

    dest.type = PJSIP_TRANSPORT_UDP;
    dest.flag = pjsip_transport_get_flag_from_type(dest.type);
    dest.addr.host = pj_str((char *) (name.c_str()));
    dest.addr.port = 0;

    pjsip_endpt_resolve(endpt, pool, &dest, &result, &resolve_cb);

    // We'll try for up to 5 seconds to resolve the address
    int tries = 5;
    while (result.status == UNRESOLVED and --tries) {
        pj_time_val timeout = { 1, 0 };
        pjsip_endpt_handle_events(endpt, &timeout);
    }

    if (!tries) {
        ERROR("Could not resolve address");
        return std::vector<std::string>();
    }

    std::vector<std::string> ipList;
    for (size_t i = 0; i < result.servers.count; ++i) {
        // get each address
        pj_sockaddr_in *rb = (pj_sockaddr_in *) &result.servers.entry[i].addr;
        std::string addr;
        addr.reserve(pj_sockaddr_get_len(rb));
        pj_sockaddr_print(rb, &*addr.begin(), addr.size(), 0);
        DEBUG("Added address %s of size %u", addr.c_str(), addr.size());
        ipList.push_back(addr);
    }
    return ipList;
}
#endif


std::set<std::string>
sip_utils::getIPList(const std::string &name)
{
    std::set<std::string> ipList;
    struct addrinfo *result;
    /* resolve the domain name into a list of addresses */
    const int error = getaddrinfo(name.c_str(), NULL, NULL, &result);
    if (error != 0) {
        DEBUG("getaddrinfo %s failed: %s", name.c_str(), gai_strerror(error));
        return ipList;
    }

    const int IP_LENGTH = 45;
    for (struct addrinfo *res = result; res != NULL; res = res->ai_next) {
        char addrstr[IP_LENGTH];
        inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, IP_LENGTH);

        void *ptr = 0;
        switch (res->ai_family) {
            case AF_INET:
                ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
                break;
        }
        inet_ntop(res->ai_family, ptr, addrstr, IP_LENGTH);
        ipList.insert(addrstr);
    }

    freeaddrinfo(result);
    return ipList;
}
