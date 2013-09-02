/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __INSTANT_MESSAGING_H__
#define __INSTANT_MESSAGING_H__

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>

#include <map>
#include <list>
#include <stdexcept>

#include "config.h"

#if HAVE_IAX
#include <iax-client.h>
#endif

#define EMPTY_MESSAGE   pj_str((char*)"")
#define MAXIMUM_MESSAGE_LENGTH		1560			/* PJSIP's sip message limit */

#define MODE_APPEND			std::ios::out || std::ios::app
#define MODE_TEST			std::ios::out

namespace sfl {

const std::string IM_XML_URI("uri");
const std::string BOUNDARY("--boundary");

class InstantMessageException : public std::runtime_error {
    public:
        InstantMessageException(const std::string& str="") :
            std::runtime_error("InstantMessageException occured: " + str) {}
};

namespace InstantMessaging {
        typedef std::map<std::string, std::string> UriEntry;
        typedef std::list<UriEntry> UriList;

        /*
         * Write the text message to the right file
         * The call ID is associated to a file descriptor, so it is easy then to retrieve the right file
         *
         * @param message	The text message
         * @param id	The current call
         * @return True if the message could have been successfully saved, False otherwise
         */
        bool saveMessage(const std::string& message, const std::string& author, const std::string& id, int mode = MODE_APPEND);

        /*
         * Send a SIP string message inside a call
         *
         * @param id	The call ID we will retrieve the invite session from
         * @param message	The string message, as sent by the client
         */
        void sip_send(pjsip_inv_session*, const std::string& id, const std::string&);

        void send_sip_message(pjsip_inv_session*, const std::string& id, const std::string&);
#if HAVE_IAX
        void send_iax_message(iax_session *session, const std::string& id, const std::string&);
#endif

        std::vector<std::string> split_message(std::string);

        /**
         * Generate Xml participant list for multi recipient based on RFC Draft 5365
         *
        * @param A UriList of UriEntry
        *
        * @return A string containing the full XML formated information to be included in the
        *         sip instant message.
        */
        std::string generateXmlUriList(UriList &list);

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
         * @param text to be displayed
         * @param list containing the recipients
         *
         * @return formated text stored into a string to be included in sip MESSAGE
         */
        std::string appendUriList(const std::string &text, UriList &list);

        /**
             * Retreive the xml formated uri list in formated text data according to RFC 5365
             *
         * @param text The formated text message as retreived in the SIP message
         *
         * @return A string containing the XML content
         */
        std::string findTextUriList(const std::string &text);

        /**
             * Retrive the plain text message in formated text data according to RFC 5365
             *
         * @param text The formated text message as retreived in the SIP message
         *
         * @return A string containing the actual message
         */
        std::string findTextMessage(const std::string &text);
} // end namespace InstantMessaging
}
#endif // __INSTANT_MESSAGING_H_
