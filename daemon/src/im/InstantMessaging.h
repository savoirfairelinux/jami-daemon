/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef _INSTANT_MESSAGING_H
#define _INSTANT_MESSAGING_H

#include <string>
#include <iostream>
#include <fstream>
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>

#include "call.h"
#include "sip/sipcall.h"

#include <map>
#include <list>
#include <stdexcept>

#include <iax-client.h>

#define EMPTY_MESSAGE   pj_str((char*)"")
#define STR_TEXT        pj_str((char*)"text")
#define STR_PLAIN       pj_str((char*)"plain")
#define METHOD_NAME     pj_str((char*)"MESSAGE")
#define MAXIMUM_MESSAGE_LENGTH		1560			/* PJSIP's sip message limit */
#define DELIMITER_CHAR				"\n\n"

#define MODE_APPEND			std::ios::out || std::ios::app
#define MODE_TEST			std::ios::out

namespace sfl
{

const std::string IM_XML_URI ("uri");
const std::string BOUNDARY ("--boundary");

class InstantMessageException : public std::runtime_error
{
    public:
        InstantMessageException (const std::string& str="") :
            std::runtime_error("InstantMessageException occured: " + str) {}
};

class InstantMessaging
{

    public:

        typedef std::map <std::string, std::string> UriEntry;
        typedef std::list <UriEntry> UriList;

        /*
         * Class constructor
         */
        InstantMessaging();

        /*
         * Class destructor
         */
        ~InstantMessaging();

        /*
         * Register and initialize instant messaging support
         */
        bool init ();

        /**
         * Set maximum size fo this module.
             */
        void setMessageMaximumSize (unsigned int max) {
            messageMaxSize = max;
        }

        /**
         * Return the maximum number if character for a single SIP MESSAGE.
         * Longer messages should be splitted in several smaller messages using split_message
         */
        unsigned int getMessageMaximumSize (void) {
            return messageMaxSize;
        }

        /*
         * Open an existing file if possible or create a new one.			 *
         * @param id	The current call
         * @return int	The number of currently open file stream
         */
        int openArchive (std::string& id);

        /*
         * Close the file corresponding to the specified call
         *
         * @param id	The current call
         * @return int	The number of remaining open file stream
         */
        int closeArchive (std::string& id);

        /*
         * Write the text message to the right file
         * The call ID is associated to a file descriptor, so it is easy then to retrieve the right file
         *
         * @param message	The text message
         * @param id	The current call
         * @return True if the message could have been successfully saved, False otherwise
         */
        bool saveMessage (const std::string& message, const std::string& author, std::string& id, int mode = MODE_APPEND);

        /*
           * Receive a string SIP message, for a specific call
         *
           * @param message	The message contained in the TEXT message
         * @param id		The call recipient of the message
         */
        std::string receive (const std::string& message, const std::string& author, const std::string& id);

        /*
         * Send a SIP string message inside a call
         *
         * @param id	The call ID we will retrieve the invite session from
         * @param message	The string message, as sent by the client
         *
         * @return pj_status_t  0 on success
         *                      1 otherwise
         */
        pj_status_t sip_send (pjsip_inv_session*, std::string& id, const std::string&);

        pj_status_t send_sip_message (pjsip_inv_session*, std::string& id, const std::string&);

        bool iax_send (iax_session* session, const std::string& id, const std::string& message);

        bool send_iax_message (iax_session *session, const std::string& id, const std::string&);

        std::vector<std::string> split_message (const std::string&);


        /**
          * Notify the clients, through D-Bus, that a new message has arrived
         *
            * @param id	The callID to notify (TODO: accountID?)
         */
        pj_status_t notify (const std::string& /*id*/);


        /**
         * Generate Xml participant list for multi recipient based on RFC Draft 5365
         *
        * @param A UriList of UriEntry
        *
        * @return A string containing the full XML formated information to be included in the
        *         sip instant message.
        */
        std::string generateXmlUriList (UriList& list);

        /**
         * Parse the Urilist from a SIP Instant Message provided by a UriList service.
         *
         * @param A XML formated string as obtained from a SIP instant message.
         *
         * @return An UriList of UriEntry containing parsed XML information as a map.
         */
        UriList parseXmlUriList (std::string& urilist);

        /**
         * Format text message according to RFC 5365, append recipient-list to the message
         *
         * @param text to be displayed
         * @param list containing the recipients
         *
         * @return formated text stored into a string to be included in sip MESSAGE
         */
        std::string appendUriList (std::string text, UriList& list);

        /**
             * Retreive the xml formated uri list in formated text data according to RFC 5365
             *
         * @param text The formated text message as retreived in the SIP message
         *
         * @return A string containing the XML content
         */
        std::string findTextUriList (std::string& text);

        /**
             * Retrive the plain text message in formated text data according to RFC 5365
             *
         * @param text The formated text message as retreived in the SIP message
         *
         * @return A string containing the actual message
         */
        std::string findTextMessage (std::string& text);

    private:

        /**
         * A queue to handle messages
         */
        // std::queue<std::string> queuedMessages;

        /**
         * A map to handle opened file descriptors
         * A file descriptor is associated to a call ID
         */
        std::map<std::string, std::ofstream*> imFiles;

        InstantMessaging (const InstantMessaging&); //No Copy Constructor
        InstantMessaging& operator= (const InstantMessaging&); //No Assignment Operator

        /**
          * Maximum size in char of an instant message
         */
        unsigned int messageMaxSize;
};
}
#endif // _INSTANT_MESSAGING_H
