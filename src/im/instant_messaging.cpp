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
        // RING_DBG("did not find ';' char in payload key, assuming only mime type is given (no parameters)");
        mimeType = payload.first;
    } else {
        mimeType = payload.first.substr(0, paramSplit);
        parameters = payload.first.substr(paramSplit + 1, std::string::npos);
    }

    // RING_DBG("mime type: \"%s\", parameters: \"%s\"", mimeType.c_str(), parameters.c_str());

    /* split to type and subtype */
    auto typeSplit = mimeType.find('/');
    if (typeSplit == std::string::npos) {
        RING_WARN("could not split into type and subtype");
        return false;
    }

    auto type = mimeType.substr(0,typeSplit);
    auto subtype = mimeType.substr(typeSplit + 1, std::string::npos);

    // RING_DBG("split into type/subtype: [\"%s\"/\"%s\"]", type.c_str(), subtype.c_str());

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
            // RING_DBG("got parameter pair: \"%s\"", paramPair.c_str());
            /* split into arg and value by '=' */
            auto argSplit = paramPair.find('=');
            if (argSplit != std::string::npos) {
                auto arg = paramPair.substr(0, argSplit);
                auto value = paramPair.substr(argSplit + 1, std::string::npos);
                // RING_DBG("split param pair into arg=value: \"%s\"=\"%s\"", arg.c_str(), value.c_str());

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
                auto startNext = paramSplit + 1;
                auto paramSplitNext = parameters.find(';', startNext);
                auto lenNext = paramSplitNext != std::string::npos ? paramSplitNext - startNext : std::string::npos;
                paramPair = parameters.substr(startNext, lenNext);
                paramSplit = paramSplitNext;
            } else {
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

    std::string header = type + "/" + subtype;

    /* iterate the parameters */
    auto param = body->content_type.param.next;
    while (param != &body->content_type.param) {
        std::string arg {param->name.ptr, (size_t)param->name.slen};
        std::string value {param->value.ptr, (size_t)param->value.slen};

        // RING_DBG("got arg=value pair: \"%s\"=\"%s\"", arg.c_str(), value.c_str());

        header += ";" + arg + "=" + value;

        param = param->next;
    }

    /* get the payload, assume we can interpret it as chars */
    std::string payload { (char *)body->data, body->len };

    return std::make_pair(header, payload);
}

/**
 * Parses given SIP message into a map where the key is the contents of the Content-Type header
 * (along with any parameters) and the value is the message payload.
 *
 * @param msg received SIP message
 *
 * @return map of content types and message payloads
 */
std::map<std::string, std::string>
InstantMessaging::parseSipMessage(pjsip_msg* msg)
{
    std::map<std::string, std::string> ret;

    if (!msg->body) {
        RING_WARN("message body is empty");
        return ret;
    }

    /* check if its a multipart message */
    pj_str_t typeMultipart { "multipart", 9 }; //FIXME: deprecated conversion from string constant to ‘char*’

    if ( pj_strcmp(&typeMultipart, &msg->body->content_type.type) != 0) {
        /* treat as single content type message */
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
    //TODO: implement multipart message creation for IAX via the pjsip api and then convert
    //      into string for sending
    for (const auto& msg: chunks)
        iax_send_text(session, msg.c_str());
}
#endif


/*
 * The following functions are for creating and parsing XML URI lists which are appended to
 * instant messages in order to be able to send a message to multiple recipients as defined in
 * RFC 5365
 *
 * These functions are not currently used, but are left for now, along with the Expat library
 * dependance, in case this functionality is implemented later. Note that it may be possible to
 * replace the Expat XML parser library by using the pjsip xml functions.
 */

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

} // namespace ring
