/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>
#include <stdexcept>

#include "config.h"

#if HAVE_IAX
#include <iax/iax-client.h>
#endif

struct pjsip_inv_session;
struct pjsip_rx_data;
struct pjsip_msg;

namespace ring { namespace InstantMessaging {

constexpr static const char* IM_XML_URI = "uri";

struct InstantMessageException : std::runtime_error
{
    InstantMessageException(const std::string& str="") :
        std::runtime_error("InstantMessageException occured: " + str) {}
};

using UriEntry = std::map<std::string, std::string>;
using UriList = std::list<UriEntry>;

/**
 * Constructs and sends a SIP message.
 *
 * The expected format of the map key is:
 *     type/subtype[; *[; arg=value]]
 *     eg: "text/plain; id=1234;part=2;of=1001"
 *     note: all whitespace is optional
 *
 * If the map contains more than one pair, then a multipart/mixed message type will be created
 * containing multiple message parts. Note that all of the message parts must be able to fit into
 * one message... they will not be split into multiple messages.
 *
 * @param session SIP session
 * @param payloads a map where the mime type and optional parameters are the key
 *                 and the message payload is the value
 *
 * @return boolean indicating whether or not the message was successfully sent
 */
bool sendSipMessage(pjsip_inv_session* session,
                    const std::map<std::string, std::string>& payloads);

/**
 * Parses given SIP message into a map where the key is the contents of the Content-Type header
 * (along with any parameters) and the value is the message payload.
 *
 * @param msg received SIP message
 *
 * @return map of content types and message payloads
 */
std::map<std::string, std::string> parseSipMessage(pjsip_msg* msg);

#if HAVE_IAX
void sendIaxMessage(iax_session* session, const std::string& id,
                    const std::vector<std::string>& chunks);
#endif

/**
 * Generate Xml participant list for multi recipient based on RFC Draft 5365
 *
 * @param A UriList of UriEntry
 *
 * @return A string containing the full XML formated information to be included in the
 *         sip instant message.
 */
std::string generateXmlUriList(const UriList& list);

/**
 * Parse the Urilist from a SIP Instant Message provided by a UriList service.
 *
 * @param A XML formated string as obtained from a SIP instant message.
 *
 * @return An UriList of UriEntry containing parsed XML information as a map.
 */
UriList parseXmlUriList(const std::string &urilist);

}} // namespace ring::InstantMessaging
