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

#include "instant_messaging.h"
#include "logger.h"
#include <expat.h>

static void XMLCALL
startElementCallback(void *userData, const char *name, const char **atts)
{
    if (strcmp(name, "entry"))
        return;

    sfl::InstantMessaging::UriEntry entry = sfl::InstantMessaging::UriEntry();

    for (const char **att = atts; *att; att += 2)
        entry.insert(std::pair<std::string, std::string> (*att, *(att+1)));

    static_cast<sfl::InstantMessaging::UriList *>(userData)->push_back(entry);
}

static void XMLCALL
endElementCallback(void * /*userData*/, const char * /*name*/)
{}

namespace sfl {
bool InstantMessaging::saveMessage(const std::string &message, const std::string &author, const std::string &id, int mode)
{
    std::ofstream File;
    std::string filename = "im:" + id;
    File.open(filename.c_str(), static_cast<std::ios_base::openmode>(mode));

    if (!File.good() || !File.is_open())
        return false;

    File << "[" << author << "] " << message << '\n';
    File.close();

    return true;
}

void InstantMessaging::sip_send(pjsip_inv_session *session, const std::string& id, const std::string& text)
{
    pjsip_tx_data *tdata;

    pjsip_dialog* dialog = session->dlg;

    pjsip_dlg_inc_lock(dialog);

    pjsip_method msg_method = { PJSIP_OTHER_METHOD, pj_str((char*)"MESSAGE") };

    if (pjsip_dlg_create_request(dialog, &msg_method, -1, &tdata) != PJ_SUCCESS) {
        pjsip_dlg_dec_lock(dialog);
        return;
    }

    const pj_str_t type =  pj_str((char*) "text");
    const pj_str_t subtype = pj_str((char*) "plain");

    pj_str_t message = pj_str((char*) text.c_str());

    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &message);

    pjsip_dlg_send_request(dialog, tdata, -1, NULL);
    pjsip_dlg_dec_lock(dialog);

    saveMessage(text, "Me", id);
}

void InstantMessaging::send_sip_message(pjsip_inv_session *session, const std::string &id, const std::string &message)
{
    std::vector<std::string> msgs(split_message(message));
    for (const auto &item : msgs)
        sip_send(session, id, item);
}

#if HAVE_IAX
void InstantMessaging::send_iax_message(iax_session *session, const std::string &/* id */, const std::string &message)
{
    std::vector<std::string> msgs(split_message(message));

    for (const auto &item : msgs)
        iax_send_text(session, item.c_str());
}
#endif


std::vector<std::string> InstantMessaging::split_message(std::string text)
{
    std::vector<std::string> messages;
    size_t len = MAXIMUM_MESSAGE_LENGTH;

    while (text.length() > len - 2) {
        messages.push_back(text.substr(len - 2) + "\n\n");
        text = text.substr(len - 2);
    }

    messages.push_back(text);

    return messages;
}

std::string InstantMessaging::generateXmlUriList(UriList &list)
{
    std::string xmlbuffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                  "<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">"
                                  "<list>";

    for (auto &item : list)
        xmlbuffer += "<entry uri=" + item[sfl::IM_XML_URI] + " cp:copyControl=\"to\" />";

    return xmlbuffer + "</list></resource-lists>";
}


InstantMessaging::UriList
InstantMessaging::parseXmlUriList(const std::string &urilist)
{
    InstantMessaging::UriList list;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &list);
    XML_SetElementHandler(parser, startElementCallback, endElementCallback);

    if (XML_Parse(parser, urilist.c_str(), urilist.size(), 1) == XML_STATUS_ERROR) {
        ERROR("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(parser)),
               XML_GetCurrentLineNumber(parser));
        throw InstantMessageException("Error while parsing uri-list xml content");
    }

    return list;
}

std::string InstantMessaging::appendUriList(const std::string &text, UriList& list)
{
    return "--boundary Content-Type: text/plain" + text +
           "--boundary Content-Type: application/resource-lists+xml" +
           "Content-Disposition: recipient-list" + generateXmlUriList(list) +
           "--boundary--";
}

std::string InstantMessaging::findTextUriList(const std::string &text)
{
    const std::string ctype("Content-Type: application/resource-lists+xml");
    const std::string cdispo("Content-Disposition: recipient-list");
    const std::string boundary("--boundary--");

    // init position pointer
    size_t pos = 0;

    // find the content type
    if ((pos = text.find(ctype)) == std::string::npos)
        throw InstantMessageException("Could not find Content-Type tag while parsing sip message for recipient-list");

    // find the content disposition
    if ((pos = text.find(cdispo, pos)) == std::string::npos)
        throw InstantMessageException("Could not find Content-Disposition tag while parsing sip message for recipient-list");

    // xml content start after content disposition tag (plus \n\n)
    const size_t begin = pos + cdispo.size();

    // find final boundary
    size_t end;
    if ((end = text.find(boundary, begin)) == std::string::npos)
        throw InstantMessageException("Could not find final \"boundary\" while parsing sip message for recipient-list");

    return text.substr(begin, end - begin);
}

std::string InstantMessaging::findTextMessage(const std::string &text)
{
    std::string ctype = "Content-Type: text/plain";
    const size_t pos = text.find(ctype);
    if (pos == std::string::npos)
        throw InstantMessageException("Could not find Content-Type tag while parsing sip message for text");

    const size_t begin = pos + ctype.size();

    const size_t end = text.find("--boundary", begin);
    if (end == std::string::npos)
        throw InstantMessageException("Could not find end of text \"boundary\" while parsing sip message for text");

    return text.substr(begin, end - begin);
}


}
