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

#include "InstantMessaging.h"

#include "expat.h"

namespace sfl
{

static inline char* duplicateString (char dst[], const char src[], size_t len)
{
    memcpy (dst, src, len);
    dst[len] = 0;
    return dst;
}

static void XMLCALL startElementCallback (void *userData, const char *name, const char **atts)
{

    char attribute[100];
    char value[100];

    const char **att;

    // _debug ("InstantMessaging: StartElement Callback: %s", name);

    if (strcmp (name, "entry") == 0) {

        sfl::InstantMessaging::UriList *list = static_cast<sfl::InstantMessaging::UriList *> (userData);
        sfl::InstantMessaging::UriEntry entry = sfl::InstantMessaging::UriEntry();

        for (att = atts; *att; att += 2) {

            const char **val = att+1;

            duplicateString (attribute, *att, strlen (*att));
            duplicateString (value, *val, strlen (*val));

            // _debug ("InstantMessaging: attribute: %s, value: %s", attribute, value);

            entry.insert (std::pair<std::string, std::string> (std::string (attribute), std::string (value)));
        }

        list->push_back (entry);
    }

}

static void XMLCALL endElementCallback (void * /*userData*/, const char * /*name*/)
{
    // std::cout << "endElement " << name << std::endl;
}


InstantMessaging::InstantMessaging()
    : imFiles ()
    , messageMaxSize (MAXIMUM_MESSAGE_LENGTH) {}


InstantMessaging::~InstantMessaging() {}

bool InstantMessaging::init ()
{
    return true;
}

int InstantMessaging::openArchive (std::string& id)
{

    // Create a new file stream
    std::ofstream File (id.c_str (), std::ios::out | std::ios::app);
    imFiles[id] = &File;

    // Attach it to the call ID
    return (int) imFiles.size ();
}

int InstantMessaging::closeArchive (std::string& id)
{

    // Erase it from the map
    imFiles.erase (id);
    return (int) imFiles.size ();
}

bool InstantMessaging::saveMessage (const std::string& message, const std::string& author, std::string& id, int mode)
{

    // We need here to write the text message in the right file.
    // We will use the Call ID

    std::ofstream File;
    std::string filename = "im:";

    filename.append (id);
    File.open (filename.c_str (), (std::_Ios_Openmode) mode);

    if (!File.good () || !File.is_open ())
        return false;

    File << "[" << author << "] " << message << '\n';
    File.close ();

    return true;
}

std::string InstantMessaging::receive (const std::string& message, const std::string& /*author*/, const std::string& /*id*/)
{

    // We just receive a TEXT message. Before sent it to the recipient, we must assure that the message is complete.
    // We should use a queue to push these messages in

    _debug ("New message : %s", message.c_str ());

    // TODO Security check
    // TODO String cleaning

    // Archive the message
    // TODO Deactivate this for the momment, this is an extra feature.
    // this->saveMessage (message, author, id);


    return message;

}

pj_status_t InstantMessaging::notify (const std::string& /*id*/)
{
    // Notify the clients through a D-Bus signal
    return PJ_SUCCESS;
}

pj_status_t InstantMessaging::sip_send (pjsip_inv_session *session, std::string& id, const std::string& text)
{

    pjsip_method msg_method;
    const pj_str_t type =  STR_TEXT;
    const pj_str_t subtype = STR_PLAIN;
    pjsip_tx_data *tdata;
    pj_status_t status;
    pjsip_dialog* dialog;
    pj_str_t message;

    msg_method.id = PJSIP_OTHER_METHOD;
    msg_method.name = METHOD_NAME;

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

    // Create the Require header to handle recipient-list Content-Disposition type
    // pjsip_generic_string_hdr reqhdr;
    // pj_str_t reqhname = pj_str ("Require");
    // pj_str_t reqhvalue = pj_str ("recipient-list");

    // Create the Content-Type header to handle multipart/mixed and boundary MIME types
    // pj_str_t ctype = pj_str ("Content-Type");
    // pj_str_t sctype = pj_str ("ctype"); // small version of the header name
    // ctypehdr = pjsip_msg_find_hdr_by_names (tdata->msg, &ctype, &sctype, NULL);
    // pjsip_generic_string_hdr ctypehdr;
    // pj_str_t ctypehname = pj_str ("Content-Type");
    // pj_str_t ctypehvalue = pj_str ("multipart/mixed;boundary=\"boundary\"");

    // Add headers to the message
    // pjsip_generic_string_hdr_init2 (&reqhdr, &reqhname, &reqhvalue);
    // pj_list_push_back (& (tdata->msg->hdr), (pjsip_hdr*) (&reqhdr));
    // pj_list_push_back (& (tdata->msg->hdr), (pjsip_hdr*) (&ctypehdr));

    // Send the request
    status = pjsip_dlg_send_request (dialog, tdata, -1, NULL);
    // PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Done
    pjsip_dlg_dec_lock (dialog);

    // Archive the message
    this->saveMessage (text, "Me", id);

    return PJ_SUCCESS;
}

pj_status_t InstantMessaging::send_sip_message (pjsip_inv_session *session, std::string& id, const std::string& message)
{

    /* Check the length of the message */
    if (message.length() < getMessageMaximumSize()) {
        /* No problem here */
        sip_send (session, id, message);
    }

    else {
        /* It exceeds the size limit of a SIP MESSAGE (1300 bytes), o plit it and send multiple messages */
        std::vector<std::string> multiple_messages = split_message (message);
        /* Send multiple messages */
        // int size = multiple_messages.size();
        int i = 0;

        // Maximum is above 1500 character
        // TODO: Send every messages
        sip_send (session, id, multiple_messages[i]);
    }

    return PJ_SUCCESS;
}


bool InstantMessaging::iax_send (iax_session* session, const std::string& /*id*/, const std::string& message)
{
    if (iax_send_text (session, message.c_str()) != -1)
        return true;
    else
        return false;


}

bool InstantMessaging::send_iax_message (iax_session* session, const std::string& id, const std::string& message)
{

    bool ret;

    /* Check the length of the message */
    if (message.length() < getMessageMaximumSize()) {
        /* No problem here */
        ret = iax_send (session, id, message);
    }

    else {
        /* It exceeds the size limit of a SIP MESSAGE (1300 bytes), o plit it and send multiple messages */
        std::vector<std::string> multiple_messages = split_message (message);
        /* Send multiple messages */
        // int size = multiple_messages.size();
        int i = 0;

        // Maximum is above 1500 character
        // TODO: Send every messages
        ret = iax_send (session, id, multiple_messages[i]);
    }

    return ret;
}


std::vector<std::string> InstantMessaging::split_message (const std::string& text)
{

    std::vector<std::string> messages;
    std::string text_to_split = text;

    /* Iterate over the message length */
    while (text_to_split.length() > getMessageMaximumSize()) {
        /* The remaining string is still too long */

        /* Compute the substring */
        std::string split_message = text_to_split.substr (0, (size_t) getMessageMaximumSize());
        /* Append our split character \n\n */
        split_message.append (DELIMITER_CHAR);
        /* Append in the vector */
        messages.push_back (split_message);
        /* Use the remaining string to not loop forever */
        text_to_split = text_to_split.substr ( (size_t) getMessageMaximumSize());
    }

    /* Push the last message */
    /* If the message length does not exceed the maximum size of a SIP MESSAGE, we go directly here */
    messages.push_back (text_to_split);

    return messages;
}

std::string InstantMessaging::generateXmlUriList (UriList& list)
{

    std::string xmlbuffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xmlbuffer.append ("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    xmlbuffer.append ("<list>");

    // An iterator over xml attribute
    UriEntry::iterator iterAttr;

    // An iterator over list entries
    UriList::iterator iterEntry = list.begin();

    while (iterEntry != list.end()) {
        xmlbuffer.append ("<entry uri=");
        UriEntry entry = static_cast<UriEntry> (*iterEntry);
        iterAttr = entry.find (sfl::IM_XML_URI);
        xmlbuffer.append (iterAttr->second);
        xmlbuffer.append (" cp:copyControl=\"to\" />");

        iterEntry++;
    }

    xmlbuffer.append ("</list>");
    xmlbuffer.append ("</resource-lists>");

    return xmlbuffer;
}


InstantMessaging::UriList InstantMessaging::parseXmlUriList (std::string& urilist)
{
    InstantMessaging::UriList list;

    XML_Parser parser = XML_ParserCreate (NULL);
    XML_SetUserData (parser, &list);
    XML_SetElementHandler (parser, startElementCallback, endElementCallback);

    if (XML_Parse (parser, urilist.c_str(), urilist.size(), 1) == XML_STATUS_ERROR) {
        std::cout << "Error: " << XML_ErrorString (XML_GetErrorCode (parser))
                  << " at line " << XML_GetCurrentLineNumber (parser) << std::endl;
        throw InstantMessageException ("Error while parsing uri-list xml content");
    }

    return list;
}

std::string InstantMessaging::appendUriList (std::string text, UriList& list)
{

    std::string formatedText = "--boundary Content-Type: text/plain";

    formatedText.append (text);
    formatedText.append ("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append ("Content-Disposition: recipient-list");

    std::string recipientlist = generateXmlUriList (list);

    formatedText.append (recipientlist);

    formatedText.append ("--boundary--");

    return formatedText;
}

std::string InstantMessaging::findTextUriList (std::string& text)
{
    std::string ctype = "Content-Type: application/resource-lists+xml";
    std::string cdispo = "Content-Disposition: recipient-list";
    std::string boundary = ("--boundary--");

    // init position pointer
    size_t pos = 0;
    size_t begin = 0;
    size_t end = 0;

    // find the content type
    if ( (pos = text.find (ctype)) == std::string::npos)
        throw InstantMessageException ("Could not find Content-Type tag while parsing sip message for recipient-list");

    // find the content disposition
    if ( (pos = text.find (cdispo, pos)) == std::string::npos)
        throw InstantMessageException ("Could not find Content-Disposition tag while parsing sip message for recipient-list");

    // xml content start after content disposition tag (plus \n\n)
    begin = pos+cdispo.size();

    // find final boundary
    if ( (end = text.find (boundary, begin)) == std::string::npos)
        throw InstantMessageException ("Could not find final \"boundary\" while parsing sip message for recipient-list");

    return text.substr (begin, end-begin);
}

std::string InstantMessaging::findTextMessage (std::string& text)
{
    std::string ctype = "Content-Type: text/plain";
    std::string boundary = "--boundary";

    size_t pos = 0;
    size_t begin = 0;
    size_t end = 0;

    // find the content type
    if ( (pos = text.find (ctype)) == std::string::npos)
        throw InstantMessageException ("Could not find Content-Type tag while parsing sip message for text");

    // plain text content start after content type tag (plus \n\n)
    begin = pos+ctype.size();

    // retrive end of the text content
    if ( (end = text.find (boundary, begin)) == std::string::npos)
        throw InstantMessageException ("Could not find end of text \"boundary\" while parsing sip message for text");

    return text.substr (begin, end-begin);
}


}
