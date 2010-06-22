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
			 * Attach the instant messaging module to an existing SIP dialog
			 *
			 * @param dlg   A pointer on the current pjsip_dialog structure
			 */
			void set_dialog (pjsip_dialog *dlg) { _current_dlg = dlg; }

			/*
			 * Prepare a string to be sent. This method have to be called
			 * before sending each message
			 *
			 * @param message   The text message
			 */
			void set_text( std::string message );

			std::string get_text_message(void) ; 

			/*
  			 * Receive a string SIP message, for a specific call
			 *
  			 * @param message	The message contained in the TEXT message
			 * @param id		The call recipient of the message
			 */
			pj_status_t receive (std::string message, CallID& id);

			/*
			 * Send a SIP string message inside a call 
			 *
			 * @param id	The call ID we will retrieve the invite session from
			 * @param message	The string message, as sent by the client
			 * 
			 * @return pj_status_t  0 on success
			 *                      1 otherwise
			 */
			pj_status_t send (CallID& id, const std::string message);

			/**
 			 * Notify the clients, through D-Bus, that a new message has arrived
			 *
   			 * @param id	The callID to notify (TODO: accountID?)
			 */
			pj_status_t notify (CallID& id);

			/*
			 * Set the response.
			 *
			 * @param resp    The last string message received
			 */ 
			void set_response( std::string resp );

			/*
			 * Display the response
			 */
			void display (void);

		private:

			/*
			 * The pjsip_dialog instance through which the instant messaging module exists
			 */
			pjsip_dialog *_current_dlg;

			/*
			 * The message to be sent
			 */
			pj_str_t _message;

			/**
			 * A queue to handle messages
			 */
			std::queue<std::string> queuedMessages;

			/*
			 * The last response
			 */
			pj_str_t _response;

			InstantMessaging(const InstantMessaging&); //No Copy Constructor
			InstantMessaging& operator=(const InstantMessaging&); //No Assignment Operator
	};
}
#endif // _INSTANT_MESSAGING_H
