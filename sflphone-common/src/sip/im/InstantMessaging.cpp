#include "InstantMessaging.h"

namespace sfl {

	InstantMessaging::InstantMessaging()
		: _current_dlg( NULL ), _message(EMPTY_MESSAGE), _response(EMPTY_MESSAGE){}


	InstantMessaging::~InstantMessaging(){
		delete _current_dlg; _current_dlg = 0;
	}

	pj_status_t InstantMessaging::init () {

		
	
	}


	void InstantMessaging::set_text( std::string message ){
		_message = pj_str((char*)message.c_str());
	}

	void InstantMessaging::set_response( std::string resp ){
		_response = pj_str((char*)resp.c_str());
	}

	std::string InstantMessaging::get_text_message( void ){
		std::string text;

		text = _response.ptr;
		return text;
	}


	pj_status_t InstantMessaging::receive (std::string message, CallID& id) {

		// We just receive a TEXT message. Before sent it to the recipient, we must assure that the message is complete.
		// We should use a queue to push these messages in

	}


	pj_status_t InstantMessaging::send (CallID& id, std::string message) {

		/*
		   pjsip_method msg_method;
		   const pj_str_t type =  STR_TEXT;
		   const pj_str_t subtype = STR_PLAIN;
		   pjsip_tx_data *tdata;
		   pj_status_t status;

		   msg_method.id = PJSIP_OTHER_METHOD;
		   msg_method.name = METHOD_NAME ;

		// Must lock dialog
		pjsip_dlg_inc_lock( _current_dlg );

		// Create the message request
		status = pjsip_dlg_create_request( _current_dlg, &msg_method, -1, &tdata );
		PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

		// Attach "text/plain" body 
		tdata->msg->body = pjsip_msg_body_create( tdata->pool, &type, &subtype, &_message );

		// Send the request
		status = pjsip_dlg_send_request( _current_dlg, tdata, -1, NULL);
		PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

		// Done
		pjsip_dlg_dec_lock( _current_dlg );
		 */
		return PJ_SUCCESS;
	}


	void InstantMessaging::display (void){
		std::cout << "<IM> " << _response.ptr << std::endl;
	}

}
