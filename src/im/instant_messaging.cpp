/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */
#include "instant_messaging.h"

#include "logger.h"
#include "connectivity/sip_utils.h"

#include <pjsip_ua.h>
#include <pjsip.h>

namespace jami {

using sip_utils::CONST_PJ_STR;

/**
 * the pair<string, string> we receive is expected to be in the format <mime type, payload>
 * the mime type is in the format "type/subtype"
 * in the header it will be presented as "Content-Type: type/subtype"
 * following the RFC spec, this header line can also contain other parameters in the format:
 *     Content-Type: type/subtype; arg=value; arg=value; ...
 * thus we also accept the key of the map to be in such a format:
 *     type/subtype; arg=value; arg=value; ...
 */
static void
createMessageBody(pj_pool_t* pool,
                  const std::pair<std::string, std::string>& payload,
                  pjsip_msg_body** body_p)
{
    /* parse the key:
     * 1. split by ';'
     * 2. parse the first result by spliting by '/' into a type and subtype
     * 3. parse any following strings into arg=value by splitting by '='
     */
    std::string_view mimeType, parameters;
    auto sep = payload.first.find(';');
    if (std::string::npos == sep) {
        mimeType = payload.first;
    } else {
        mimeType = std::string_view(payload.first).substr(0, sep);
        parameters = std::string_view(payload.first).substr(sep + 1);
    }

    // split mime type to type and subtype
    sep = mimeType.find('/');
    if (std::string::npos == sep) {
        JAMI_DBG("bad mime type: '%.*s'", (int) mimeType.size(), mimeType.data());
        throw im::InstantMessageException("invalid mime type");
    }

    auto type = sip_utils::CONST_PJ_STR(mimeType.substr(0, sep));
    auto subtype = sip_utils::CONST_PJ_STR(mimeType.substr(sep + 1));
    auto message = sip_utils::CONST_PJ_STR(payload.second);

    // create part
    *body_p = pjsip_msg_body_create(pool, &type, &subtype, &message);

    if (not parameters.size())
        return;

    // now try to add parameters one by one
    do {
        sep = parameters.find(';');
        auto paramPair = parameters.substr(0, sep);
        if (paramPair.empty())
            break;

        // split paramPair into arg and value by '='
        auto paramSplit = paramPair.find('=');
        if (std::string::npos == paramSplit) {
            JAMI_DBG("bad parameter: '%.*s'", (int) paramPair.size(), paramPair.data());
            throw im::InstantMessageException("invalid parameter");
        }

        auto arg = sip_utils::CONST_PJ_STR(paramPair.substr(0, paramSplit));
        auto value = sip_utils::CONST_PJ_STR(paramPair.substr(paramSplit + 1));
        pj_strtrim(&arg);
        pj_strtrim(&value);
        pj_str_t arg_pj, value_pj;
        pjsip_param* param = PJ_POOL_ALLOC_T(pool, pjsip_param);
        param->name = *pj_strdup(pool, &arg_pj, &arg);
        param->value = *pj_strdup(pool, &value_pj, &value);
        pj_list_push_back(&(*body_p)->content_type.param, param);

        // next parameter?
        if (std::string::npos != sep)
            parameters = parameters.substr(sep + 1);
    } while (std::string::npos != sep);
}

void
im::fillPJSIPMessageBody(pjsip_tx_data& tdata, const std::map<std::string, std::string>& payloads)
{
    // multi-part body?
    if (payloads.size() == 1) {
        createMessageBody(tdata.pool, *payloads.begin(), &tdata.msg->body);
        return;
    }

    /* if ctype is not specified "multipart/mixed" will be used
     * if the boundary is not specified, a random one will be generateAudioPort
     * FIXME: generate boundary and check that none of the message parts contain it before
     *        calling this function; however the probability of this happenings if quite low as
     *        the randomly generated string is fairly long
     */
    tdata.msg->body = pjsip_multipart_create(tdata.pool, nullptr, nullptr);

    for (const auto& pair : payloads) {
        auto part = pjsip_multipart_create_part(tdata.pool);
        if (not part) {
            JAMI_ERR("pjsip_multipart_create_part failed: not enough memory");
            throw InstantMessageException("Internal SIP error");
        }

        createMessageBody(tdata.pool, pair, &part->body);

        auto status = pjsip_multipart_add_part(tdata.pool, tdata.msg->body, part);
        if (status != PJ_SUCCESS) {
            JAMI_ERR("pjsip_multipart_add_part failed: %s", sip_utils::sip_strerror(status).c_str());
            throw InstantMessageException("Internal SIP error");
        }
    }
}

void
im::sendSipMessage(pjsip_inv_session* session, const std::map<std::string, std::string>& payloads)
{
    if (payloads.empty()) {
        JAMI_WARN("the payloads argument is empty; ignoring message");
        return;
    }

    constexpr pjsip_method msg_method = {PJSIP_OTHER_METHOD,
                                         CONST_PJ_STR(sip_utils::SIP_METHODS::MESSAGE)};

    {
        auto dialog = session->dlg;
        sip_utils::PJDialogLock dialog_lock {dialog};

        pjsip_tx_data* tdata = nullptr;
        auto status = pjsip_dlg_create_request(dialog, &msg_method, -1, &tdata);
        if (status != PJ_SUCCESS) {
            JAMI_ERR("pjsip_dlg_create_request failed: %s", sip_utils::sip_strerror(status).c_str());
            throw InstantMessageException("Internal SIP error");
        }

        fillPJSIPMessageBody(*tdata, payloads);

        status = pjsip_dlg_send_request(dialog, tdata, -1, nullptr);
        if (status != PJ_SUCCESS) {
            JAMI_ERR("pjsip_dlg_send_request failed: %s", sip_utils::sip_strerror(status).c_str());
            throw InstantMessageException("Internal SIP error");
        }
    }
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
parseMessageBody(const pjsip_msg_body* body)
{
    std::string header = sip_utils::as_view(body->content_type.type) + "/"
                         + sip_utils::as_view(body->content_type.subtype);

    // iterate over parameters
    auto param = body->content_type.param.next;
    while (param != &body->content_type.param) {
        header += ";" + sip_utils::as_view(param->name) + "=" + sip_utils::as_view(param->value);
        param = param->next;
    }

    // get the payload, assume we can interpret it as chars
    return {std::move(header), std::string(static_cast<char*>(body->data), (size_t) body->len)};
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
im::parseSipMessage(const pjsip_msg* msg)
{
    std::map<std::string, std::string> ret;

    if (!msg->body) {
        JAMI_WARN("message body is empty");
        return ret;
    }

    // check if its a multipart message
    constexpr pj_str_t typeMultipart {CONST_PJ_STR("multipart")};

    if (pj_strcmp(&typeMultipart, &msg->body->content_type.type) != 0) {
        // treat as single content type message
        ret.emplace(parseMessageBody(msg->body));
    } else {
        /* multipart type message, we will treat it as multipart/mixed even if the subtype is
         * something else, eg: related
         */
        auto part = pjsip_multipart_get_first_part(msg->body);
        while (part != nullptr) {
            ret.emplace(parseMessageBody(part->body));
            part = pjsip_multipart_get_next_part(msg->body, part);
        }
    }

    return ret;
}

} // namespace jami
