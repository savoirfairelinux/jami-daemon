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

const std::string IM_XML_URI ("IM_XML_URI");

class InstantMessaging
{

    public:

        typedef std::map <std::string, std::string> UriEntry;
        typedef std::list <UriEntry *> UriList;

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
        int openArchive (CallID& id);

        /*
         * Close the file corresponding to the specified call
         *
         * @param id	The current call
         * @return int	The number of remaining open file stream
         */
        int closeArchive (CallID& id);

        /*
         * Write the text message to the right file
         * The call ID is associated to a file descriptor, so it is easy then to retrieve the right file
         *
         * @param message	The text message
         * @param id	The current call
         * @return True if the message could have been successfully saved, False otherwise
         */
        bool saveMessage (const std::string& message, const std::string& author, CallID& id, int mode = MODE_APPEND);

        /*
           * Receive a string SIP message, for a specific call
         *
           * @param message	The message contained in the TEXT message
         * @param id		The call recipient of the message
         */
        std::string receive (const std::string& message, const std::string& author, CallID& id);

        /*
         * Send a SIP string message inside a call
         *
         * @param id	The call ID we will retrieve the invite session from
         * @param message	The string message, as sent by the client
         *
         * @return pj_status_t  0 on success
         *                      1 otherwise
         */
        pj_status_t send (pjsip_inv_session*, CallID& id, const std::string&);

        pj_status_t send_message (pjsip_inv_session*, CallID& id, const std::string&);

        std::vector<std::string> split_message (const std::string&);


        /**
          * Notify the clients, through D-Bus, that a new message has arrived
         *
            * @param id	The callID to notify (TODO: accountID?)
         */
        pj_status_t notify (CallID& id);


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
