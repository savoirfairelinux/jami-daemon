#include "InstantMessaging.h"

namespace sfl {

	InstantMessaging::InstantMessaging()
		 {}


	InstantMessaging::~InstantMessaging(){
	}

	pj_status_t InstantMessaging::init () {
		return PJ_SUCCESS;
	}

	pj_status_t InstantMessaging::receive (std::string message, CallID& id) {

		// We just receive a TEXT message. Before sent it to the recipient, we must assure that the message is complete.
		// We should use a queue to push these messages in

	}


	pj_status_t InstantMessaging::send (pjsip_inv_session *session, const std::string& text) {


		pjsip_method msg_method;
		const pj_str_t type =  STR_TEXT;
		const pj_str_t subtype = STR_PLAIN;
		pjsip_tx_data *tdata;
		pj_status_t status;
		pjsip_dialog* dialog;
		pj_str_t message;

		msg_method.id = PJSIP_OTHER_METHOD;
		msg_method.name = METHOD_NAME ;

		// Get the dialog associated to the call
		dialog = session->dlg;
		// Convert the text into a format readable by pjsip
		message = pj_str ((char*) text.c_str ());

		// Must lock dialog
		pjsip_dlg_inc_lock( dialog );

		// Create the message request
		status = pjsip_dlg_create_request( dialog, &msg_method, -1, &tdata );
		PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

		// Attach "text/plain" body 
		tdata->msg->body = pjsip_msg_body_create( tdata->pool, &type, &subtype, &message );

		// Send the request
		status = pjsip_dlg_send_request( dialog, tdata, -1, NULL);
		PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

		// Done
		pjsip_dlg_dec_lock( dialog );

		return PJ_SUCCESS;
	}

}
