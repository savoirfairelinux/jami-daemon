#include "InstantMessaging.h"

namespace sfl {

InstantMessaging::InstantMessaging()
        : imFiles () {}


InstantMessaging::~InstantMessaging() {}

bool InstantMessaging::init () {
    return true;
}

int InstantMessaging::openArchive (CallID& id) {

    // Create a new file stream
    std::ofstream File (id.c_str (), std::ios::out | std::ios::app);
    imFiles[id] = &File;

    // Attach it to the call ID
    return (int) imFiles.size ();
}

int InstantMessaging::closeArchive (CallID& id) {

    // Erase it from the map
    imFiles.erase (id);
    return (int) imFiles.size ();
}

bool InstantMessaging::saveMessage (const std::string& message, const std::string& author, CallID& id, int mode) {

    // We need here to write the text message in the right file.
    // We will use the Call ID

    std::ofstream File;
    std::string filename = "sip:";

    filename.append (id);
    File.open (filename.c_str (), (std::_Ios_Openmode) mode);

    if (!File.good () || !File.is_open ())
        return false;

    File << "[" << author << "] " << message << '\n';
    File.close ();

    return true;
}

std::string InstantMessaging::receive (const std::string& message, const std::string& author, CallID& id) {

    // We just receive a TEXT message. Before sent it to the recipient, we must assure that the message is complete.
    // We should use a queue to push these messages in

    _debug ("New message : %s", message.c_str ());

    // TODO Security check
    // TODO String cleaning

    // Archive the message
    this->saveMessage (message, author, id);


    return message;

}

pj_status_t InstantMessaging::notify (CallID& id) {

    // Notify the clients through a D-Bus signal
    return PJ_SUCCESS;

}

pj_status_t InstantMessaging::send (pjsip_inv_session *session, CallID& id, const std::string& text) {


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
    message = pj_str ( (char*) text.c_str ());

    // Must lock dialog
    pjsip_dlg_inc_lock (dialog);

    // Create the message request
    status = pjsip_dlg_create_request (dialog, &msg_method, -1, &tdata);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Attach "text/plain" body
    tdata->msg->body = pjsip_msg_body_create (tdata->pool, &type, &subtype, &message);

    // Send the request
    status = pjsip_dlg_send_request (dialog, tdata, -1, NULL);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Done
    pjsip_dlg_dec_lock (dialog);

    // Archive the message
    this->saveMessage (text, "Me", id);

    printf ("SIPVoIPLink::sendTextMessage %s %s\n", id.c_str(), text.c_str());

    return PJ_SUCCESS;
}

}
