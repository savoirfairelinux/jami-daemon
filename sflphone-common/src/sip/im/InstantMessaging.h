#ifndef _INSTANT_MESSAGING_H
#define _INSTANT_MESSAGING_H

#include <string>
#include <queue>
#include <iostream>
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>

#include "call.h"
#include "sip/sipcall.h"

#define EMPTY_MESSAGE   pj_str((char*)"")
#define STR_TEXT        pj_str((char*)"text")
#define STR_PLAIN       pj_str((char*)"plain")
#define METHOD_NAME     pj_str((char*)"MESSAGE")

namespace sfl  {

	class InstantMessaging
	{
		public:
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
			pj_status_t init ();

			/*
  			 * Receive a string SIP message, for a specific call
			 *
  			 * @param message	The message contained in the TEXT message
			 * @param id		The call recipient of the message
			 */
			std::string receive (std::string message, CallID& id);

			/*
			 * Send a SIP string message inside a call 
			 *
			 * @param id	The call ID we will retrieve the invite session from
			 * @param message	The string message, as sent by the client
			 * 
			 * @return pj_status_t  0 on success
			 *                      1 otherwise
			 */
			pj_status_t send (pjsip_inv_session*, const std::string&);

			/**
 			 * Notify the clients, through D-Bus, that a new message has arrived
			 *
   			 * @param id	The callID to notify (TODO: accountID?)
			 */
			pj_status_t notify (CallID& id);

		private:

			/**
			 * A queue to handle messages
			 */
			std::queue<std::string> queuedMessages;

			InstantMessaging(const InstantMessaging&); //No Copy Constructor
			InstantMessaging& operator=(const InstantMessaging&); //No Assignment Operator
	};
}
#endif // _INSTANT_MESSAGING_H
