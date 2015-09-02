/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Emmanuel Lepage <elv1313@gmail.com>
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
#include "sip/sip_utils.h"

#include <expat.h>
#include <pjsip_ua.h>

namespace ring {

static void XMLCALL
startElementCallback(void* userData, const char* name, const char** atts)
{
    if (strcmp(name, "entry"))
        return;

    InstantMessaging::UriEntry entry = InstantMessaging::UriEntry();

    for (const char **att = atts; *att; att += 2)
        entry.insert(std::pair<std::string, std::string> (*att, *(att+1)));

    static_cast<InstantMessaging::UriList *>(userData)->push_back(entry);
}

static void XMLCALL
endElementCallback(void * /*userData*/, const char* /*name*/)
{}

void
InstantMessaging::sendSipMessage(pjsip_inv_session* session, const std::string& id,
                                 const std::vector<std::string>& chunks)
{
    for (const auto& text: chunks) {
        const pjsip_method msg_method = { PJSIP_OTHER_METHOD, pj_str((char*)"MESSAGE") };
        pjsip_tx_data* tdata;

        auto dialog = session->dlg;
        pjsip_dlg_inc_lock(dialog);

        if (pjsip_dlg_create_request(dialog, &msg_method, -1, &tdata) != PJ_SUCCESS) {
            pjsip_dlg_dec_lock(dialog);
            return;
        }

        //TODO multipart/mixed and multipart/related need to be handled separately
        //Use the "is_mixed" sendMessage() API
//         const auto type = pj_str((char*) "multipart");
//         const auto subtype = pj_str((char*) "related");
        const auto type = pj_str((char*) "text");
        const auto subtype = pj_str((char*) "plain");
        const auto message = pj_str((char*) text.c_str());

        tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &message);
        auto ret = pjsip_dlg_send_request(dialog, tdata, -1, nullptr);
        if (ret != PJ_SUCCESS)
            RING_WARN("SIP send message failed: %s", sip_utils::sip_strerror(ret).c_str());
        pjsip_dlg_dec_lock(dialog);
    }
}

#if HAVE_IAX
void
InstantMessaging::sendIaxMessage(iax_session* session, const std::string& /* id */,
                                 const std::vector<std::string>& chunks)
{
    for (const auto& msg: chunks)
        iax_send_text(session, msg.c_str());
}
#endif

std::string
InstantMessaging::generateXmlUriList(const UriList& list)
{
    std::string xmlbuffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">"
        "<list>";

    for (const auto& item: list) {
        const auto it = item.find(IM_XML_URI);
        if (it == item.cend())
            continue;
        xmlbuffer += "<entry uri=" + it->second + " cp:copyControl=\"to\" />";
    }
    return xmlbuffer + "</list></resource-lists>";
}


InstantMessaging::UriList
InstantMessaging::parseXmlUriList(const std::string& urilist)
{
    InstantMessaging::UriList list;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &list);
    XML_SetElementHandler(parser, startElementCallback, endElementCallback);

    if (XML_Parse(parser, urilist.c_str(), urilist.size(), 1) == XML_STATUS_ERROR) {
        RING_ERR("%s at line %lu\n", XML_ErrorString(XML_GetErrorCode(parser)),
               XML_GetCurrentLineNumber(parser));
        throw InstantMessageException("Error while parsing uri-list xml content");
    }

    return list;
}

///See rfc2046#section-5.1.4
static std::string
buildMimeMultipartPart(const std::string& type, const std::string& dispo,
                       const std::string& content)
{
    return
    "--boundary\n"
    "Content-Type: " + type + "\n" +
    (!dispo.empty() ? "Content-Disposition: "+ dispo + "\n" : "") +
    "\n" +
    content + "\n";
}

std::vector<std::string>
InstantMessaging::appendMimePayloads(const std::map<std::string, std::string>& payloads,
                                     const UriList& list)
{
    static const std::string footer = "--boundary--";
    std::vector<std::string> ret;
    std::string chunk;

    const auto& urilist = not list.empty() ? buildMimeMultipartPart("application/resource-lists+xml",
                                                                    "recipient-list",
                                                                    generateXmlUriList(list)) : "";

    const size_t max_message_size = MAXIMUM_MESSAGE_LENGTH - urilist.size() - footer.size();

    for (const auto& pair : payloads) {
        const auto& m = buildMimeMultipartPart(pair.first, {}, pair.second);
        if (m.size() > max_message_size) {
            RING_DBG("An %s payload is too large to be sent, the maximum lenght is %zu",
                     m.c_str(), max_message_size);
            continue;
        }
        if (m.size() + chunk.size() > max_message_size) {
            RING_DBG("Some MIME payloads don't fit into the packet, splitting, max size is %zu, the payload would be %zu %zu %zu",
                     max_message_size, m.size() + chunk.size(), m.size() , chunk.size()
            );
            chunk += urilist + footer;
            ret.push_back(chunk);
            chunk = "";
        }
        chunk += m;
    }

    if (chunk.size())
        ret.push_back(chunk);

    return ret;
}

std::string
InstantMessaging::findTextUriList(const std::string& text)
{
    static const std::string ctype("Content-Type: application/resource-lists+xml");
    static const std::string cdispo("Content-Disposition: recipient-list");
    static const std::string boundary("--boundary--");

    // init position pointer
    size_t pos = 0;

    // find the content type
    if ((pos = text.find(ctype)) == std::string::npos)
        throw InstantMessageException("Could not find Content-Type tag while parsing sip message for recipient-list");

    // find the content disposition
    if ((pos = text.find(cdispo, pos)) == std::string::npos)
        throw InstantMessageException("Could not find Content-Disposition tag while parsing sip message for recipient-list");

    // xml content start after content disposition tag
    size_t begin = pos + cdispo.size();

    //Remove arbitrary number of empty lines, otherwise XML_Parse will return XML_STATUS_ERROR
    while (text[begin] == '\n' || text[begin] == '\r')
        begin++;

    // find final boundary
    size_t end;
    if ((end = text.find(boundary, begin)) == std::string::npos)
        throw InstantMessageException("Could not find final \"boundary\" while parsing sip message for recipient-list");

    return text.substr(begin, end - begin);
}

/*
 * From RFC2046:
 *
 * MIME-Version: 1.0
 * Content-Type: multipart/alternative; boundary=boundary42
 *
 * --boundary42
 * Content-Type: text/plain; charset=us-ascii
 *
 *    ... plain text version of message goes here ...
 *
 * --boundary42
 * Content-Type: text/html
 *
 *    ... RFC 1896 text/enriched version of same message
 *       goes here ...
 *
 * --boundary42
 * Content-Type: application/x-whatever
 *
 *    ... fanciest version of same message goes here ...
 *
 * --boundary42--
 */

std::string
InstantMessaging::findMimePayload(const std::string& encodedPayloads, const std::string& mime)
{
    const std::string ctype = "Content-Type: " + mime;
    const size_t pos = encodedPayloads.find(ctype);
    if (pos == std::string::npos)
        return {};
    const size_t begin = pos + ctype.size();

    const size_t end = encodedPayloads.find("--boundary", begin);
    if (end == std::string::npos) {
        RING_DBG("Could not find end of text \"boundary\" while parsing sip message for text");
        return {};
    }

    return encodedPayloads.substr(begin, end - begin);
}

std::map<std::string, std::string>
InstantMessaging::parsePayloads(const std::string& encodedPayloads)
{
    //Constants
    static const std::string boud  = "--boundary"           ;
    static const std::string type  = "Content-Type: "       ;
    static const std::string dispo = "Content-Disposition: ";
    const size_t end = encodedPayloads.find("--boundary--");

    size_t next_start = encodedPayloads.find(boud);

    std::map< std::string, std::string > ret;

    do {
        size_t currentStart = next_start;

        next_start = encodedPayloads.find(boud, currentStart+1);

        //Get the mime type
        size_t context_pos = encodedPayloads.find(type, currentStart+1);
        if (context_pos == std::string::npos)
            break;
        else if (context_pos >= next_start)
            continue;

        context_pos += type.size();

        size_t mimeTypeEnd = encodedPayloads.find('\n', context_pos+1);
        if (encodedPayloads[context_pos-1] == '\r')
            mimeTypeEnd--;

        std::string mimeType = encodedPayloads.substr(context_pos, mimeTypeEnd - context_pos);
        currentStart = mimeTypeEnd+1;

        //Remove the disposition
        const size_t dispoPos = encodedPayloads.find(dispo, currentStart);
        if (dispoPos != std::string::npos && dispoPos < next_start) {
            currentStart = encodedPayloads.find('\n', dispoPos);
            while (encodedPayloads[currentStart] == '\n' || encodedPayloads[currentStart] == '\r')
                currentStart++;
        }

        //Get the payload
        std::string payload = encodedPayloads.substr(currentStart, next_start - currentStart);

        //WARNING assume only one message per payload exist
        ret[mimeType] = payload;

    } while(next_start < end);

    return ret;
}

} // namespace ring
