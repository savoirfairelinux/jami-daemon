/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#pragma once

#include <string>
#include <vector>

struct pjsip_inv_session;

#include <map>
#include <list>
#include <stdexcept>

#include "config.h"

#if HAVE_IAX
#include <iax/iax-client.h>
#endif

namespace ring { namespace InstantMessaging {

/* PJSIP's sip message limit, PJSIP_MAX_PKT_LEN is the total, but most
   is used for the SIP header

   Ring currently split the messages into smaller ones and send them. Obviously
   it is invalid as some messages wont have the MIME boundary section.

   The number set here is arbitrary, the theoretical limit is around 3000,
   but some messages may fail.
*/
constexpr static unsigned MAXIMUM_MESSAGE_LENGTH = 1800;

constexpr static const char* IM_XML_URI = "uri";

struct InstantMessageException : std::runtime_error
{
    InstantMessageException(const std::string& str="") :
        std::runtime_error("InstantMessageException occured: " + str) {}
};

using UriEntry = std::map<std::string, std::string>;
using UriList = std::list<UriEntry>;

void sendSipMessage(pjsip_inv_session* session, const std::string& id,
                    const std::vector<std::string>& chunks);
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

/**
 * Format text message according to RFC 5365, append recipient-list to the message
 *
 * @param Key/Value MIME pairs to be sent. If a payload doesn't fit, the message will be split
 * @param list containing the recipients
 *
 * @return formated text stored into a string to be included in sip MESSAGE
 */
std::vector<std::string> appendMimePayloads(const std::map<std::string, std::string>& payloads,
                                            const UriList& list = {});

/**
 * Retreive the xml formated uri list in formated text data according to RFC 5365
 *
 * @param text The formated text message as retreived in the SIP message
 *
 * @return A string containing the XML content
 */
std::string findTextUriList(const std::string& text);

/**
 * Retrieve a MIME payload from the SIP container RFC5365
 *
 * @param mime the mime type
 *
 * @param encodedPayloads a MIME encoded set of payloads
 *
 * @return A string containing the actual message
 */
std::string findMimePayload(const std::string& encodedPayloads,
                            const std::string& mime = "text/plain");

/**
 * Retrieve all MIME payloads from encodedPayloads
 *
 * @param encodedPayloads a MIME encoded set of payloads
 */
std::map< std::string, std::string > parsePayloads(const std::string& encodedPayloads);

}} // namespace ring::InstantMessaging
