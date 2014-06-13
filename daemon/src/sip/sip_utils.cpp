/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "sip_utils.h"
#include "logger.h"
#include "utf8_utils.h"

#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>
#include <pj/string.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_uri.h>
#include <pj/list.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vector>
#include <algorithm>

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

// FIXME: replace with regex
std::string
sip_utils::parseDisplayName(const char * buffer)
{
    // Start in From: in short and long form
    const char* from_header = strstr(buffer, "\nFrom: ");
    if (!from_header)
        from_header = strstr(buffer, "\nf: ");
    if (!from_header)
        return "";

    std::string temp(from_header);

    // Cut at end of line
    temp = temp.substr(0, temp.find("\n", 1));

    size_t begin_displayName = temp.find("\"");
    size_t end_displayName;
    if (begin_displayName != std::string::npos) {
      // parse between quotes
      end_displayName = temp.find("\"", begin_displayName + 1);
      if (end_displayName == std::string::npos)
          return "";
    } else {
      // parse without quotes
      end_displayName = temp.find("<");
      if (end_displayName != std::string::npos) {
          begin_displayName = temp.find_first_not_of(" ", temp.find(":"));
          if (begin_displayName == std::string::npos)
              return "";

          // omit trailing/leading spaces
          begin_displayName++;
          end_displayName--;
          if (end_displayName == begin_displayName)
              return "";
      } else {
          return "";
      }
    }

    std::string displayName = temp.substr(begin_displayName + 1,
                                          end_displayName - begin_displayName - 1);

    // Filter out invalid UTF-8 characters to avoid getting kicked from D-Bus
    if (not utf8_validate(displayName)) {
        return utf8_make_valid(displayName);
    }

    return displayName;
}

void
sip_utils::stripSipUriPrefix(std::string& sipUri)
{
    // Remove sip: prefix
    static const char SIP_PREFIX[] = "sip:";
    size_t found = sipUri.find(SIP_PREFIX);

    if (found != std::string::npos)
        sipUri.erase(found, (sizeof SIP_PREFIX) - 1);

    // URI may or may not be between brackets
    found = sipUri.find("<");
    if (found != std::string::npos)
        sipUri.erase(found, 1);

    found = sipUri.find("@");

    if (found != std::string::npos)
        sipUri.erase(found);

    found = sipUri.find(">");
    if (found != std::string::npos)
        sipUri.erase(found, 1);
}

std::string
sip_utils::getHostFromUri(const std::string& sipUri)
{
    std::string hostname(sipUri);
    size_t found = hostname.find("@");
    if (found != std::string::npos)
        hostname.erase(0, found+1);

    found = hostname.find(">");
    if (found != std::string::npos)
        hostname.erase(found, 1);

    return hostname;
}

std::vector<std::string>
sip_utils::getIPList(const std::string &name)
{
    std::vector<std::string> ipList;
    if (name.empty())
        return ipList;

    //ERROR("sip_utils::getIPList %s", name.c_str());

    struct addrinfo *result;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_ADDRCONFIG;
    /* resolve the domain name into a list of addresses */
    const int error = getaddrinfo(name.c_str(), nullptr, &hints, &result);
    if (error != 0) {
        DEBUG("getaddrinfo on \"%s\" failed: %s", name.c_str(), gai_strerror(error));
        return ipList;
    }

    for (struct addrinfo *res = result; res != nullptr; res = res->ai_next) {
        void *ptr = 0;
        std::vector<char> addrstr;
        switch (res->ai_family) {
            case AF_INET:
                addrstr.resize(INET_ADDRSTRLEN);
                ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                addrstr.resize(INET6_ADDRSTRLEN);
                ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
                break;
            default:
                ERROR("Unexpected address family type, skipping.");
                continue;
        }
        inet_ntop(res->ai_family, ptr, addrstr.data(), addrstr.size());
        // don't add duplicates, and don't use an std::set because
        // we want this order preserved.
        const std::string tmp(addrstr.begin(), addrstr.end());
        if (std::find(ipList.begin(), ipList.end(), tmp) == ipList.end()) {
            ipList.push_back(tmp);
        }
    }

    freeaddrinfo(result);
    return ipList;
}

void
sip_utils::addContactHeader(const pj_str_t *contact_str, pjsip_tx_data *tdata)
{
    pjsip_contact_hdr *contact = pjsip_contact_hdr_create(tdata->pool);
    contact->uri = pjsip_parse_uri(tdata->pool, contact_str->ptr,
                                   contact_str->slen, PJSIP_PARSE_URI_AS_NAMEADDR);
    // remove old contact header (if present)
    pjsip_msg_find_remove_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) contact);
}


void
sip_utils::sip_strerror(pj_status_t code)
{
    char err_msg[PJ_ERR_MSG_SIZE];
    pj_strerror(code, err_msg, sizeof err_msg);
    ERROR("%d: %s", code, err_msg);
}
