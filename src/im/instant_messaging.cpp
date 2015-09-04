/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Emmanuel Lepage <elv1313@gmail.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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
#include <pjsip.h>

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

/**
 * the pair<string, string> we receive is expected to be in the format <mime type, payload>
 * the mime type is in the format "type/subtype"
 * in the header it will be presented as "Content-Type: type/subtype"
 * following the RFC spec, this header line can also contain other parameters in the format:
 *     Content-Type: type/subtype; arg=value; arg=value; ...
 * thus we also accept the key of the map to be in such a format:
 *     type/subtype; arg=value; arg=value; ...
 */
static bool
createMessageBody(pj_pool_t *pool,
                  const std::pair<std::string, std::string>& payload,
                  pjsip_msg_body **body)
{
    /* parse the key:
     * 1. split by ';
     * 2. parse the first resulting string by spliting by '/' into a type and subtype
     * 3. parse any following strings intro arg=value by splitting by '='
     */

    // NOTE: we duplicate all the c_str when creating pj_str_t strings because we're potentially
    // working with local vars which might be destroyed before the message is sent and thus the
    // the mem they pointed to could be something else at the time the message is actually sent

    //FIXME: the following assumes that no param name or value contains ';'
    std::string mimeType, parameters;
    auto paramSplit = payload.first.find(';');
    if (paramSplit == std::string::npos) {
        RING_DBG("did not find ';' char in payload key, assuming only mime type is given (no parameters)");
        mimeType = payload.first;
    } else {
        mimeType = payload.first.substr(0, paramSplit);
        parameters = payload.first.substr(paramSplit + 1, std::string::npos);
    }

    RING_DBG("mime type: \"%s\", parameters: \"%s\"", mimeType.c_str(), parameters.c_str());

    /* split to type and subtype */
    auto typeSplit = mimeType.find('/');
    if (typeSplit == std::string::npos) {
        RING_WARN("could not split into type and subtype");
        return false;
    }

    auto type = mimeType.substr(0,typeSplit);
    auto subtype = mimeType.substr(typeSplit + 1, std::string::npos);

    RING_DBG("split into type/subtype: [\"%s\"/\"%s\"]", type.c_str(), subtype.c_str());

    /* create part */
    auto type_pj = pj_strdup3(pool, type.c_str());
    auto subtype_pj = pj_strdup3(pool, subtype.c_str());
    auto message_pj = pj_strdup3(pool, payload.second.c_str()); //FIXME: assumes null terminated string
    *body = pjsip_msg_body_create(pool, &type_pj, &subtype_pj, &message_pj);

    /* now try to add the parameters */
    if (parameters.size() > 0) {
        paramSplit = parameters.find(';');
        auto paramPair = parameters.substr(0, paramSplit);
        while (paramPair.size() > 0) {
            RING_DBG("got parameter pair: \"%s\"", paramPair.c_str());
            /* split into arg and value by '=' */
            auto argSplit = paramPair.find('=');
            if (argSplit != std::string::npos) {
                auto arg = paramPair.substr(0, argSplit);
                auto value = paramPair.substr(argSplit + 1, std::string::npos);
                RING_DBG("split param pair into arg=value: \"%s\"=\"%s\"", arg.c_str(), value.c_str());

                /* add to the body content type */
                auto arg_pj = pj_strdup3(pool, arg.c_str());
                pj_strtrim(&arg_pj);
                auto value_pj = pj_strdup3(pool, value.c_str());
                pj_strtrim(&value_pj);

                pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
                p->name = arg_pj;
                p->value = value_pj;

                pj_list_push_back(&(*body)->content_type.param, p);

            } else {
                RING_WARN("could not split parameter into arg=value");
            }
            /* get the next param pair */
            if (paramSplit != std::string::npos) {
                RING_DBG("more param pairs might be remaining");
                auto startNext = paramSplit + 1;
                auto paramSplitNext = parameters.find(';', startNext);
                auto lenNext = paramSplitNext != std::string::npos ? paramSplitNext - startNext : std::string::npos;
                RING_DBG("next pair len: %lu", lenNext);
                paramPair = parameters.substr(startNext, lenNext);
                paramSplit = paramSplitNext;
            } else {
                RING_DBG("no more param pairs");
                paramPair.clear();
            }
        }
    }

    return true;
}

/**
 * Constructs and sends a SIP message.
 *
 * The expected format of the map key is:
 *     type/subtype[; *[; arg=value]]
 *     eg: "text/plain; id=1234;part=2;of=1001"
 *     note: all whitespace is optional
 *
 * If the map contains more than one pair, then a multipart/mixed message type will be created
 * containing multiple message parts. Note that all of the message parts must be able to fit into
 * one message... they will not be split into multple messages.
 *
 * @param session SIP session
 * @param payloads a map where the mime type and optional parameters are the key
 *                 and the message payload is the value
 *
 * @return boolean indicating whether or not the message was successfully sent
 */
bool
InstantMessaging::sendSipMessage(pjsip_inv_session* session,
                                 const std::map<std::string, std::string>& payloads)
{
    if (payloads.empty()) {
        RING_WARN("the payloads argument is empty; ignoring message");
        return false;
    }

    const pjsip_method msg_method = { PJSIP_OTHER_METHOD, pj_str((char*)"MESSAGE") };
    pjsip_tx_data* tdata;

    auto dialog = session->dlg;
    pjsip_dlg_inc_lock(dialog);

    if (pjsip_dlg_create_request(dialog, &msg_method, -1, &tdata) != PJ_SUCCESS) {
        pjsip_dlg_dec_lock(dialog);
        return false;
    }

    bool bodyCreated = false;

    if (payloads.size() == 1) {
        /* one part message */
        if (createMessageBody(tdata->pool, *payloads.begin(), &tdata->msg->body))
            bodyCreated = true;
        else
            RING_WARN("error creating message body");
    } else {
        /* create multipart body;
         * if ctype is not specified "multipart/mixed" will be used
         * if the boundary is not specified, a random one will be generateAudioPort
         * FIXME: generate boundary and check that none of the message parts contain it before
         *        calling this function; however the probability of this happenings if quite low as
         *        the randomly generated string is fairly long
         */
        tdata->msg->body = pjsip_multipart_create(tdata->pool, nullptr, nullptr);

        for (const auto& pair : payloads) {
            RING_DBG("creating part from: [%s] [%s]", pair.first.c_str(), pair.second.c_str());

            pjsip_msg_body *body;

            if (createMessageBody(tdata->pool, pair, &body)) {
                auto part = pjsip_multipart_create_part(tdata->pool);
                part->body = body;

                auto status = pjsip_multipart_add_part(tdata->pool, tdata->msg->body, part);
                if (status == PJ_SUCCESS) {
                    /* at least one part created, so we have a message */
                    bodyCreated = true;
                } else
                    RING_WARN("error adding part ([%s] [%s]) to multipart msg body: %s",
                        pair.first.c_str(), pair.second.c_str(), sip_utils::sip_strerror(status).c_str());
            } else {
                RING_WARN("error creating message body for multipart");
            }
        }
    }

    auto ret = false;
    if (bodyCreated) {
        auto status = pjsip_dlg_send_request(dialog, tdata, -1, nullptr);

        if (status == PJ_SUCCESS)
            ret = true;
        else
            RING_WARN("sending SIP message failed: %s", sip_utils::sip_strerror(status).c_str());
    }
    pjsip_dlg_dec_lock(dialog);
    return ret;
}

/**
 * Creates std::pair with the Content-Type header contents as the first value and the message
 * payload as the second value.
 *
 * The format of the first value will be:
 *     type/subtype[; *[; arg=value]]
 *     eg: "text/plain;id=1234;part=2;of=1001"
 */
static std::pair<std::string, std::string>
parseMessageBody(const pjsip_msg_body *body)
{
    std::string type {body->content_type.type.ptr, (size_t)body->content_type.type.slen};
    std::string subtype {body->content_type.subtype.ptr, (size_t)body->content_type.subtype.slen};

    RING_DBG("got type/subtype: [\"%s\"/\"%s\"]", type.c_str(), subtype.c_str());

    std::string header = type + "/" + subtype;

    /* iterate the parameters */
    auto param = body->content_type.param.next;
    while (param != &body->content_type.param) {
        std::string arg {param->name.ptr, (size_t)param->name.slen};
        std::string value {param->value.ptr, (size_t)param->value.slen};

        RING_DBG("got arg=value pair: \"%s\"=\"%s\"", arg.c_str(), value.c_str());

        header += ";" + arg + "=" + value;

        param = param->next;
    }

    RING_DBG("parsed header content: \"%s\"", header.c_str());

    /* get the payload, assume we can interpret it as chars */
    std::string payload { (char *)body->data, body->len };

    RING_DBG("parsed msg payload: \"%s\"", payload.c_str());

    return std::make_pair(header, payload);
}

std::map<std::string, std::string>
InstantMessaging::parseSipMessage(pjsip_rx_data* rdata, pjsip_msg* msg)
{
    std::map<std::string, std::string> ret;

    if (!msg->body) {
        RING_WARN("message body is empty");
        return ret;
    }

    /* check if its a multipart message */
    pj_str_t typeMultipart { "multipart", 9 };

    if ( pj_strcmp(&typeMultipart, &msg->body->content_type.type) != 0) {
        /* assume it is not a multipart message */
        ret.insert(parseMessageBody(msg->body));
    } else {
        /* multipart type message, we will treat it as multipart/mixed even if the subtype is
         * something else, eg: related
         */
        auto part = pjsip_multipart_get_first_part(msg->body);
        while (part != NULL) {
            ret.insert(parseMessageBody(part->body));
            part = pjsip_multipart_get_next_part(msg->body, part);
        }
    }

    return ret;
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
