/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>
#include <stdexcept>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
struct pjsip_inv_session;
struct pjsip_rx_data;
struct pjsip_msg;
struct pjsip_tx_data;
}

namespace jami {
namespace im {

struct InstantMessageException : std::runtime_error
{
    InstantMessageException(const std::string& str = "")
        : std::runtime_error("InstantMessageException occurred: " + str)
    {}
};

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
 * Exception: throw InstantMessageException if no message sent
 */
void sendSipMessage(pjsip_inv_session* session, const std::map<std::string, std::string>& payloads);

/**
 * Parses given SIP message into a map where the key is the contents of the Content-Type header
 * (along with any parameters) and the value is the message payload.
 *
 * @param msg received SIP message
 *
 * @return map of content types and message payloads
 */
std::map<std::string, std::string> parseSipMessage(const pjsip_msg* msg);

void fillPJSIPMessageBody(pjsip_tx_data& tdata, const std::map<std::string, std::string>& payloads);

} // namespace im
} // namespace jami
