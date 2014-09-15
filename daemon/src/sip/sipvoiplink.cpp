/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "sipvoiplink.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sdp.h"
#include "sipcall.h"
#include "sip_utils.h"

#include "manager.h"
#if HAVE_SDES
#include "sdes_negotiator.h"
#endif

#include "logger.h"
#include "array_size.h"
#include "map_utils.h"
#include "ip_utils.h"

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

#include "audio/audiolayer.h"

#ifdef SFL_VIDEO
#include "video/video_rtp_session.h"
#include "client/videomanager.h"
#endif

#include "client/client.h"
#include "client/callmanager.h"
#include "client/configurationmanager.h"

#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_uri.h>
#include <pjnath.h>

#ifdef SFL_PRESENCE
#include <pjsip-simple/presence.h>
#include <pjsip-simple/publish.h>
#include "pres_sub_server.h"
#endif

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <istream>
#include <utility> // for std::pair
#include <algorithm>

using namespace sfl;

SIPVoIPLink *SIPVoIPLink::instance_ = nullptr;

/** Environment variable used to set pjsip's logging level */
#define SIPLOGLEVEL "SIPLOGLEVEL"

/** A map to retreive SFLphone internal call id
 *  Given a SIP call ID (usefull for transaction sucha as transfer)*/
static std::map<std::string, std::string> transferCallID;

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/**
 * Set audio and video (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 */
static void setCallMediaLocal(SIPCall* call, const pj_sockaddr& localIP);

static pj_caching_pool pool_cache, *cp_ = &pool_cache;
static pj_pool_t *pool_;
static pjsip_endpoint *endpt_;
static pjsip_module mod_ua_;

static void sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status);
static void sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer);
static void sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer);
static void invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *e);
static void outgoing_request_forked_cb(pjsip_inv_session *inv, pjsip_event *e);
static void transaction_state_changed_cb(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);
static void registration_cb(pjsip_regc_cbparam *param);

static void transfer_client_cb(pjsip_evsub *sub, pjsip_event *event);

/**
 * Helper function to process refer function on call transfer
 */
static void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata);

static void
handleIncomingOptions(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;

    if (pjsip_endpt_create_response(endpt_, rdata, PJSIP_SC_OK, NULL, &tdata) != PJ_SUCCESS)
        return;

#define ADD_HDR(hdr) do { \
    const pjsip_hdr *cap_hdr = hdr; \
    if (cap_hdr) \
    pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr)); \
} while (0)
#define ADD_CAP(cap) ADD_HDR(pjsip_endpt_get_capability(endpt_, cap, NULL));

    ADD_CAP(PJSIP_H_ALLOW);
    ADD_CAP(PJSIP_H_ACCEPT);
    ADD_CAP(PJSIP_H_SUPPORTED);
    ADD_HDR(pjsip_evsub_get_allow_events_hdr(NULL));

    pjsip_response_addr res_addr;
    pjsip_get_response_addr(tdata->pool, rdata, &res_addr);

    if (pjsip_endpt_send_response(endpt_, &res_addr, tdata, NULL, NULL) != PJ_SUCCESS)
        pjsip_tx_data_dec_ref(tdata);
}

// return PJ_FALSE so that eventuall other modules will handle these requests
// TODO: move Voicemail to separate module
static pj_bool_t
transaction_response_cb(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);

    if (!dlg)
        return PJ_FALSE;

    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);

    if (!tsx or tsx->method.id != PJSIP_INVITE_METHOD)
        return PJ_FALSE;

    if (tsx->status_code / 100 == 2) {
        /**
         * Send an ACK message inside a transaction. PJSIP send automatically, non-2xx ACK response.
         * ACK for a 2xx response must be send using this method.
         */
        pjsip_tx_data *tdata;

        if (rdata->msg_info.cseq) {
            pjsip_dlg_create_request(dlg, &pjsip_ack_method, rdata->msg_info.cseq->cseq, &tdata);
            pjsip_dlg_send_request(dlg, tdata, -1, NULL);
        }
    }

    return PJ_FALSE;
}

static void
updateSDPFromSTUN(SIPCall &call, SIPAccount &account, const SipTransport &transport)
{
    std::vector<long> socketDescriptors(call.getAudioRtp().getSocketDescriptors());

    try {
        std::vector<pj_sockaddr> stunPorts(transport.getSTUNAddresses(account, socketDescriptors));

        // FIXME: get video sockets
        //stunPorts.resize(4);

        account.setPublishedAddress(stunPorts[0]);
        // published IP MUST be updated first, since RTCP depends on it
        call.getLocalSDP()->setPublishedIP(account.getPublishedAddress());
        call.getLocalSDP()->updatePorts(stunPorts);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }
}

static pj_status_t
try_respond_stateless(pjsip_endpoint *endpt, pjsip_rx_data *rdata, int st_code,
                      const pj_str_t *st_text, const pjsip_hdr *hdr_list,
                      const pjsip_msg_body *body)
{
    /* Check that no UAS transaction has been created for this request.
     * If UAS transaction has been created for this request, application
     * MUST send the response statefully using that transaction.
     */
    if (!pjsip_rdata_get_tsx(rdata))
        return pjsip_endpt_respond_stateless(endpt, rdata, st_code, st_text, hdr_list, body);
    else
        ERROR("Transaction has been created for this request, send response "
              "statefully instead");

    return !PJ_SUCCESS;
}

static pj_bool_t
transaction_request_cb(pjsip_rx_data *rdata)
{
    if (!rdata or !rdata->msg_info.msg) {
        ERROR("rx_data is NULL");
        return PJ_FALSE;
    }

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

    if (!method) {
        ERROR("method is NULL");
        return PJ_FALSE;
    }

    if (method->id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg(rdata))
        return PJ_FALSE;

    if (!rdata->msg_info.to or !rdata->msg_info.from or !rdata->msg_info.via) {
        ERROR("Missing From, To or Via fields");
        return PJ_FALSE;
    }
    const pjsip_sip_uri *sip_to_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.to->uri);
    const pjsip_sip_uri *sip_from_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.from->uri);
    const pjsip_host_port& sip_via = rdata->msg_info.via->sent_by;

    if (!sip_to_uri or !sip_from_uri or !sip_via.host.ptr) {
        ERROR("NULL uri");
        return PJ_FALSE;
    }
    std::string toUsername(sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string viaHostname(sip_via.host.ptr, sip_via.host.slen);
    std::string account_id(SIPVoIPLink::instance().guessAccountIdFromNameAndServer(toUsername, viaHostname));

    std::string displayName(sip_utils::parseDisplayName(rdata->msg_info.msg_buf));

    pjsip_msg_body *body = rdata->msg_info.msg->body;

    if (method->id == PJSIP_OTHER_METHOD) {
        pj_str_t *str = &method->name;
        std::string request(str->ptr, str->slen);

        if (request.find("NOTIFY") != std::string::npos) {
            if (body and body->data) {
                int voicemail = 0;
                int ret = sscanf((const char*) body->data, "Voice-Message: %d/", &voicemail);

                if (ret == 1 and voicemail != 0)
                    Manager::instance().startVoiceMessageNotification(account_id, voicemail);
            }
        }

        try_respond_stateless(endpt_, rdata, PJSIP_SC_OK, NULL, NULL, NULL);

        return PJ_FALSE;
    } else if (method->id == PJSIP_OPTIONS_METHOD) {
        handleIncomingOptions(rdata);
        return PJ_FALSE;
    } else if (method->id != PJSIP_INVITE_METHOD && method->id != PJSIP_ACK_METHOD) {
        try_respond_stateless(endpt_, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, NULL, NULL, NULL);
        return PJ_FALSE;
    }

    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (!account) {
        ERROR("Could not find account %s", account_id.c_str());
        return PJ_FALSE;
    }

    pjmedia_sdp_session *r_sdp;

    if (!body || pjmedia_sdp_parse(rdata->tp_info.pool, (char*) body->data, body->len, &r_sdp) != PJ_SUCCESS)
        r_sdp = NULL;

    if (account->getActiveAudioCodecs().empty()) {
        try_respond_stateless(endpt_, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, NULL, NULL);

        return PJ_FALSE;
    }

    // Verify that we can handle the request
    unsigned options = 0;

    if (pjsip_inv_verify_request(rdata, &options, NULL, NULL, endpt_, NULL) != PJ_SUCCESS) {
        try_respond_stateless(endpt_, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, NULL, NULL, NULL);
        return PJ_FALSE;
    }

    Manager::instance().hookPreference.runHook(rdata->msg_info.msg);

    auto call = std::make_shared<SIPCall>(Manager::instance().getNewCallID(), Call::INCOMING, cp_, account_id);

    // FIXME : for now, use the same address family as the SIP tranport
    auto family = pjsip_transport_type_get_af(account->getTransportType());
    IpAddr addrToUse = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);

    // May use the published address as well
    IpAddr addrSdp = account->isStunEnabled() or (not account->getPublishedSameasLocal())
                    ? account->getPublishedIpAddress() : addrToUse;

    pjsip_tpselector tp_sel  = account->getTransportSelector();

    char tmp[PJSIP_MAX_URL_SIZE];
    size_t length = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, sip_from_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber(tmp, std::min(length, sizeof tmp));
    sip_utils::stripSipUriPrefix(peerNumber);

    const std::string remote_user(sip_from_uri->user.ptr, sip_from_uri->user.slen);
    const std::string remote_hostname(sip_from_uri->host.ptr, sip_from_uri->host.slen);

    if (not remote_user.empty() and not remote_hostname.empty())
        peerNumber = remote_user + "@" + remote_hostname;

    //DEBUG("transaction_request_cb viaHostname %s toUsername %s addrToUse %s addrSdp %s peerNumber: %s" ,
    //viaHostname.c_str(), toUsername.c_str(), addrToUse.toString().c_str(), addrSdp.toString().c_str(), peerNumber.c_str());

    call->setConnectionState(Call::PROGRESSING);
    call->setPeerNumber(peerNumber);
    call->setDisplayName(displayName);
    call->initRecFilename(peerNumber);

    setCallMediaLocal(call.get(), addrToUse);

    call->getLocalSDP()->setPublishedIP(addrSdp);

    call->getAudioRtp().initConfig();

    try {
        call->getAudioRtp().initSession();
    } catch (const ost::Socket::Error &err) {
        ERROR("AudioRtp socket error");
        return PJ_FALSE;
    }

    if (account->isStunEnabled())
        updateSDPFromSTUN(*call, *account, *SIPVoIPLink::instance().sipTransport);

    if (body and body->len > 0 and call->getAudioRtp().isSdesEnabled()) {
        std::string sdpOffer(static_cast<const char*>(body->data), body->len);
        size_t start = sdpOffer.find("a=crypto:");

        // Found crypto header in SDP
        if (start != std::string::npos) {
            CryptoOffer crypto_offer;
            crypto_offer.push_back(std::string(sdpOffer.substr(start, (sdpOffer.size() - start) - 1)));

            const size_t size = ARRAYSIZE(sfl::CryptoSuites);
            std::vector<sfl::CryptoSuiteDefinition> localCapabilities(size);

            std::copy(sfl::CryptoSuites, sfl::CryptoSuites + size,
                      localCapabilities.begin());

#if HAVE_SDES
            sfl::SdesNegotiator sdesnego(localCapabilities, crypto_offer);

            if (sdesnego.negotiate()) {
                try {
                    call->getAudioRtp().setRemoteCryptoInfo(sdesnego);
                    call->getAudioRtp().initLocalCryptoInfo();
                } catch (const AudioRtpFactoryException &e) {
                    ERROR("%s", e.what());
                    return PJ_FALSE;
                }
            }

#endif
        }
    }

    call->getLocalSDP()->receiveOffer(r_sdp, account->getActiveAudioCodecs(), account->getActiveVideoCodecs());

    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    if (!ac) {
        ERROR("Could not instantiate codec");
        return PJ_FALSE;
    }

    std::vector<sfl::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);
    call->getAudioRtp().start(audioCodecs);

    pjsip_dialog *dialog = 0;

    if (pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, nullptr, &dialog) != PJ_SUCCESS) {
        call.reset();
        try_respond_stateless(endpt_, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, nullptr, nullptr, nullptr);
        return PJ_FALSE;
    }

    pjsip_inv_create_uas(dialog, rdata, call->getLocalSDP()->getLocalSdpSession(), 0, &call->inv);

    if (!dialog or pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        ERROR("Could not set transport for dialog");
        return PJ_FALSE;
    }

    if (!call->inv) {
        ERROR("Call invite is not initialized");
        return PJ_FALSE;
    }

    call->inv->mod_data[mod_ua_.id] = call.get();

    // Check whether Replaces header is present in the request and process accordingly.
    pjsip_dialog *replaced_dlg;
    pjsip_tx_data *response;

    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
        ERROR("Something wrong with Replaces request.");
        call.reset();

        // Something wrong with the Replaces header.
        if (response) {
            pjsip_response_addr res_addr;
            pjsip_get_response_addr(response->pool, rdata, &res_addr);
            pjsip_endpt_send_response(endpt_, &res_addr, response,
                                      NULL, NULL);
        } else {
            try_respond_stateless(endpt_, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL, NULL);
        }

        return PJ_FALSE;
    }

    // Check if call has been transfered
    pjsip_tx_data *tdata = 0;

    // If Replace header present
    if (replaced_dlg) {
        // Always answer the new INVITE with 200 if the replaced call is in early or confirmed state.
        if (pjsip_inv_answer(call->inv, PJSIP_SC_OK, NULL, NULL, &response) == PJ_SUCCESS)
            pjsip_inv_send_msg(call->inv, response);

        // Get the INVITE session associated with the replaced dialog.
        pjsip_inv_session *replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

        // Disconnect the "replaced" INVITE session.
        if (pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS && tdata)
            pjsip_inv_send_msg(replaced_inv, tdata);
    } else { // Proceed with normal call flow
        if (pjsip_inv_initial_answer(call->inv, rdata, PJSIP_SC_TRYING, NULL, NULL, &tdata) != PJ_SUCCESS) {
            ERROR("Could not answer invite");
            return PJ_FALSE;
        }

        if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS) {
            ERROR("Could not send msg for invite");
            return PJ_FALSE;
        }

        call->setConnectionState(Call::TRYING);

        if (pjsip_inv_answer(call->inv, PJSIP_SC_RINGING, NULL, NULL, &tdata) != PJ_SUCCESS) {
            ERROR("Could not answer invite");
            return PJ_FALSE;
        }

        // contactStr must stay in scope as long as tdata
        const pj_str_t contactStr(account->getContactHeader());
        sip_utils::addContactHeader(&contactStr, tdata);

        if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS) {
            ERROR("Could not send msg for invite");
            return PJ_FALSE;
        }

        call->setConnectionState(Call::RINGING);

        SIPVoIPLink::instance().addSipCall(call);
        Manager::instance().incomingCall(*call, account_id);
    }

    return PJ_FALSE;
}

/*************************************************************************************************/
pjsip_endpoint * SIPVoIPLink::getEndpoint()
{
    return endpt_;
}

pjsip_module * SIPVoIPLink::getMod()
{
    return &mod_ua_;
}

SIPVoIPLink::SIPVoIPLink() : sipTransport(), sipAccountMap_(),
    sipCallMapMutex_(), sipCallMap_()
#ifdef SFL_VIDEO
    , keyframeRequestsMutex_()
    , keyframeRequests_()
#endif
{

#define TRY(ret) do { \
    if (ret != PJ_SUCCESS) \
    throw VoipLinkException(#ret " failed"); \
} while (0)

    srand(time(NULL)); // to get random number for RANDOM_PORT

    TRY(pj_init());

    TRY(pjlib_util_init());

    setSipLogLevel();
    TRY(pjnath_init());

    pj_caching_pool_init(cp_, &pj_pool_factory_default_policy, 0);
    pool_ = pj_pool_create(&cp_->factory, PACKAGE, 4096, 4096, nullptr);

    if (!pool_)
        throw VoipLinkException("UserAgent: Could not initialize memory pool");

    TRY(pjsip_endpt_create(&cp_->factory, pj_gethostname()->ptr, &endpt_));

    sipTransport.reset(new SipTransport(endpt_, *cp_, *pool_));

    if (!ip_utils::getLocalAddr())
        throw VoipLinkException("UserAgent: Unable to determine network capabilities");

    TRY(pjsip_tsx_layer_init_module(endpt_));
    TRY(pjsip_ua_init_module(endpt_, nullptr));
    TRY(pjsip_replaces_init_module(endpt_)); // See the Replaces specification in RFC 3891
    TRY(pjsip_100rel_init_module(endpt_));

    // Initialize and register sflphone module
    mod_ua_.name = pj_str((char*) PACKAGE);
    mod_ua_.id = -1;
    mod_ua_.priority = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_ua_.on_rx_request = &transaction_request_cb;
    mod_ua_.on_rx_response = &transaction_response_cb;
    TRY(pjsip_endpt_register_module(endpt_, &mod_ua_));

    TRY(pjsip_evsub_init_module(endpt_));
    TRY(pjsip_xfer_init_module(endpt_));

#ifdef SFL_PRESENCE
    // presence/publish management
    TRY(pjsip_pres_init_module(endpt_, pjsip_evsub_instance()));
    TRY(pjsip_endpt_register_module(endpt_, &PresSubServer::mod_presence_server));
#endif

    static const pjsip_inv_callback inv_cb = {
        invite_session_state_changed_cb,
        outgoing_request_forked_cb,
        transaction_state_changed_cb,
        sdp_request_offer_cb,
#if PJ_VERSION_NUM > (2 << 24 | 1 << 16)
        nullptr /* on_rx_reinvite */,
#endif
        sdp_create_offer_cb,
        sdp_media_update_cb,
        nullptr /* on_send_ack */,
        nullptr /* on_redirected */,
    };
    TRY(pjsip_inv_usage_init(endpt_, &inv_cb));

    static const pj_str_t allowed[] = {
        CONST_PJ_STR("INFO"),
        CONST_PJ_STR("OPTIONS"),
        CONST_PJ_STR("MESSAGE"),
        CONST_PJ_STR("PUBLISH"),
    };

    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ALLOW, nullptr, PJ_ARRAY_SIZE(allowed), allowed);

    static const pj_str_t text_plain = CONST_PJ_STR("text/plain");
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, nullptr, 1, &text_plain);

    static const pj_str_t accepted = CONST_PJ_STR("application/sdp");
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, nullptr, 1, &accepted);

    DEBUG("pjsip version %s for %s initialized", pj_get_version(), PJ_OS_NAME);

    TRY(pjsip_replaces_init_module(endpt_));
#undef TRY

    handlingEvents_ = true;
}

SIPVoIPLink::~SIPVoIPLink()
{
    const int MAX_TIMEOUT_ON_LEAVING = 5;

    for (int timeout = 0; pjsip_tsx_layer_get_tsx_count() and timeout < MAX_TIMEOUT_ON_LEAVING; timeout++)
        sleep(1);

    handlingEvents_ = false;

    const pj_time_val tv = {0, 10};
    pjsip_endpt_handle_events(endpt_, &tv);

    for (auto & a : sipAccountMap_)
        unloadAccount(a);
    sipAccountMap_.clear();
    clearSipCallMap();

    // destroy SIP transport before endpoint
    sipTransport.reset();

    pjsip_endpt_destroy(endpt_);

    pj_pool_release(pool_);
    pj_caching_pool_destroy(cp_);

    pj_shutdown();
}

SIPVoIPLink& SIPVoIPLink::instance()
{
    if (!instance_) {
        DEBUG("creating SIPVoIPLink instance");
        instance_ = new SIPVoIPLink;
    }

    return *instance_;
}

void SIPVoIPLink::destroy()
{
    delete instance_;
    instance_ = nullptr;
}

std::string
SIPVoIPLink::guessAccountIdFromNameAndServer(const std::string &userName,
        const std::string &server) const
{
    DEBUG("username = %s, server = %s", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    std::string result(SIPAccount::IP2IP_PROFILE);
    MatchRank best = MatchRank::NONE;

    for (const auto & item : sipAccountMap_) {
        SIPAccount *account = static_cast<SIPAccount*>(item.second);

        if (!account)
            continue;

        const MatchRank match(account->matches(userName, server, endpt_, pool_));

        // return right away if this is a full match
        if (match == MatchRank::FULL) {
            return item.first;
        } else if (match > best) {
            best = match;
            result = item.first;
        }
    }

    return result;
}

void SIPVoIPLink::setSipLogLevel()
{
    char *envvar = getenv(SIPLOGLEVEL);
    int level = 0;

    if (envvar != NULL) {
        std::string loglevel = envvar;

        if (!(std::istringstream(loglevel) >> level)) level = 0;

        level = level > 6 ? 6 : level;
        level = level < 0 ? 0 : level;
    }

    // From 0 (min) to 6 (max)
    pj_log_set_level(level);
}

// Called from EventThread::run (not main thread)
bool SIPVoIPLink::handleEvents()
{
    // We have to register the external thread so it could access the pjsip frameworks
    if (!pj_thread_is_registered()) {
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
        static thread_local pj_thread_desc desc;
        static thread_local pj_thread_t *this_thread;
#else
        static __thread pj_thread_desc desc;
        static __thread pj_thread_t *this_thread;
#endif
        DEBUG("Registering thread");
        pj_thread_register(NULL, desc, &this_thread);
    }

    static const pj_time_val timeout = {0, 10};
    pj_status_t ret;

    if ((ret = pjsip_endpt_handle_events(endpt_, &timeout)) != PJ_SUCCESS)
        sip_utils::sip_strerror(ret);

#ifdef SFL_VIDEO
    dequeKeyframeRequests();
#endif
    return handlingEvents_;
}

void
SIPVoIPLink::sendRegister(Account& a)
{
    SIPAccount& account = static_cast<SIPAccount&>(a);
    if (not account.isEnabled()) {
        WARN("Account must be enabled to register, ignoring");
        return;
    }

    try {
        sipTransport->createSipTransport(account);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
        throw VoipLinkException("Could not create or acquire SIP transport");
    }

    account.setRegister(true);
    account.setRegistrationState(RegistrationState::TRYING);

    pjsip_regc *regc = nullptr;
    if (pjsip_regc_create(endpt_, (void *) &account, &registration_cb, &regc) != PJ_SUCCESS)
        throw VoipLinkException("UserAgent: Unable to create regc structure.");

    std::string srvUri(account.getServerUri());
    pj_str_t pjSrv = pj_str((char*) srvUri.c_str());

    // Generate the FROM header
    std::string from(account.getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    // Get the received header
    std::string received(account.getReceivedParameter());

    // Get the contact header
    const pj_str_t pjContact(account.getContactHeader());

    auto transport = account.getTransport();
    if (transport) {
        if (not account.getPublishedSameasLocal() or (not received.empty() and received != account.getPublishedAddress())) {
            pjsip_host_port *via = account.getViaAddr();
            DEBUG("Setting VIA sent-by to %.*s:%d", via->host.slen, via->host.ptr, via->port);

            if (pjsip_regc_set_via_sent_by(regc, via, transport) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        } else if (account.isStunEnabled()) {
            if (pjsip_regc_set_via_sent_by(regc, account.getViaAddr(), transport) != PJ_SUCCESS)
                throw VoipLinkException("Unable to set the \"sent-by\" field");
        }
    }


    pj_status_t status;

    //DEBUG("pjsip_regc_init from:%s, srv:%s, contact:%s", from.c_str(), srvUri.c_str(), std::string(pj_strbuf(&pjContact), pj_strlen(&pjContact)).c_str());
    if ((status = pjsip_regc_init(regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, account.getRegistrationExpire())) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to initialize account registration structure");
    }

    if (account.hasServiceRoute())
        pjsip_regc_set_route_set(regc, sip_utils::createRouteSet(account.getServiceRoute(), pool_));

    pjsip_regc_set_credentials(regc, account.getCredentialCount(), account.getCredInfo());

    pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    std::string useragent(account.getUserAgentName());
    pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
    const pj_str_t STR_USER_AGENT = CONST_PJ_STR("User-Agent");

    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create(pool_, &STR_USER_AGENT, &pJuseragent);
    pj_list_push_back(&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers(regc, &hdr_list);
    pjsip_tx_data *tdata;

    if (pjsip_regc_register(regc, PJ_TRUE, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to initialize transaction data for account registration");

    const pjsip_tpselector tp_sel = SipTransport::getTransportSelector(transport);
    if (pjsip_regc_set_transport(regc, &tp_sel) != PJ_SUCCESS)
        throw VoipLinkException("Unable to set transport");

    // pjsip_regc_send increment the transport ref count by one,
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to send account registration request");
    }

    account.setRegistrationInfo(regc);
    sipTransport->cleanupTransports();
}

void SIPVoIPLink::sendUnregister(Account& a, std::function<void(bool)> released_cb)
{
    SIPAccount& account = static_cast<SIPAccount&>(a);

    // This may occurs if account failed to register and is in state INVALID
    if (!account.isRegistered()) {
        account.setRegistrationState(RegistrationState::UNREGISTERED);
        if (released_cb)
            released_cb(true);
        return;
    }

    // Make sure to cancel any ongoing timers before unregister
    account.stopKeepAliveTimer();

    pjsip_regc *regc = account.getRegistrationInfo();
    if (!regc)
        throw VoipLinkException("Registration structure is NULL");

    pjsip_tx_data *tdata = nullptr;
    if (pjsip_regc_unregister(regc, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to unregister sip account");

    pj_status_t status;
    if ((status = pjsip_regc_send(regc, tdata)) != PJ_SUCCESS) {
        sip_utils::sip_strerror(status);
        throw VoipLinkException("Unable to send request to unregister sip account");
    }

    account.setRegister(false);

    // remove the transport from the account
    pjsip_transport* transport_ = account.getTransport();
    account.setTransport();
    sipTransport->cleanupTransports();
    if (released_cb) {
        sipTransport->waitForReleased(transport_, released_cb);
    }
}

void SIPVoIPLink::registerKeepAliveTimer(pj_timer_entry &timer, pj_time_val &delay)
{
    DEBUG("Register new keep alive timer %d with delay %d", timer.id, delay.sec);

    if (timer.id == -1)
        WARN("Timer already scheduled");

    switch (pjsip_endpt_schedule_timer(endpt_, &timer, &delay)) {
        case PJ_SUCCESS:
            break;

        default:
            ERROR("Could not schedule new timer in pjsip endpoint");

            /* fallthrough */
        case PJ_EINVAL:
            ERROR("Invalid timer or delay entry");
            break;

        case PJ_EINVALIDOP:
            ERROR("Invalid timer entry, maybe already scheduled");
            break;
    }
}

void SIPVoIPLink::cancelKeepAliveTimer(pj_timer_entry& timer)
{
    pjsip_endpt_cancel_timer(endpt_, &timer);
}

std::shared_ptr<Call> SIPVoIPLink::newOutgoingCall(const std::string& id, const std::string& toUrl, const std::string &account_id)
{
    DEBUG("New outgoing call to %s", toUrl.c_str());
    std::string toCpy = toUrl;

    sip_utils::stripSipUriPrefix(toCpy);

    const bool IPToIP = IpAddr::isValid(toCpy);
    Manager::instance().setIPToIPForCall(id, IPToIP);

    if (IPToIP) {
        return SIPNewIpToIpCall(id, toCpy);
    } else {
        return newRegisteredAccountCall(id, toUrl, account_id);
    }
}

std::shared_ptr<Call> SIPVoIPLink::SIPNewIpToIpCall(const std::string& id, const std::string& to_raw)
{
    bool ipv6 = false;
#if HAVE_IPV6
    ipv6 = IpAddr::isIpv6(to_raw);
#endif
    const std::string& to = ipv6 ? IpAddr(to_raw).toString(false, true) : to_raw;
    DEBUG("New %s IP to IP call to %s", ipv6?"IPv6":"IPv4", to.c_str());

    SIPAccount *account = Manager::instance().getIP2IPAccount();

    if (!account)
        throw VoipLinkException("Could not retrieve default account for IP2IP call");

    auto call = std::make_shared<SIPCall>(id, Call::OUTGOING, cp_, SIPAccount::IP2IP_PROFILE);

    call->setIPToIP(true);
    call->initRecFilename(to);

    IpAddr localAddress = ip_utils::getInterfaceAddr(account->getLocalInterface(), ipv6 ? pj_AF_INET6() : pj_AF_INET());

    setCallMediaLocal(call.get(), localAddress);

    std::string toUri = account->getToUri(to);
    call->setPeerNumber(toUri);

    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    if (!ac)
        throw VoipLinkException("Could not instantiate codec");

    std::vector<sfl::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);

    // Audio Rtp Session must be initialized before creating initial offer in SDP session
    // since SDES require crypto attribute.
    call->getAudioRtp().initConfig();
    call->getAudioRtp().initSession();
    call->getAudioRtp().initLocalCryptoInfo();
    call->getAudioRtp().start(audioCodecs);

    // Building the local SDP offer
    Sdp *localSDP = call->getLocalSDP();

    if (account->getPublishedSameasLocal())
        localSDP->setPublishedIP(localAddress);
    else
        localSDP->setPublishedIP(account->getPublishedAddress());

    const bool created = localSDP->createOffer(account->getActiveAudioCodecs(), account->getActiveVideoCodecs());

    if (not created or not SIPStartCall(call))
        throw VoipLinkException("Could not create new call");

    return call;
}

std::shared_ptr<Call> SIPVoIPLink::newRegisteredAccountCall(const std::string& id,
                                                            const std::string& toUrl,
                                                            const std::string &account_id)
{
    DEBUG("UserAgent: New registered account call to %s", toUrl.c_str());

    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (account == nullptr) // TODO: We should investigate how we could get rid of this error and create a IP2IP call instead
        throw VoipLinkException("Could not get account for this call");

    auto call = std::make_shared<SIPCall>(id, Call::OUTGOING, cp_, account->getAccountID());

    // If toUri is not a well formatted sip URI, use account information to process it
    std::string toUri;

    if (toUrl.find("sip:") != std::string::npos or
            toUrl.find("sips:") != std::string::npos)
        toUri = toUrl;
    else
        toUri = account->getToUri(toUrl);

    call->setPeerNumber(toUri);

    // FIXME : for now, use the same address family as the SIP tranport
    auto family = pjsip_transport_type_get_af(account->getTransportType());
    IpAddr localAddr = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);
    setCallMediaLocal(call.get(), localAddr);

    // May use the published address as well
    IpAddr addrSdp = account->isStunEnabled() or (not account->getPublishedSameasLocal()) ?
                          account->getPublishedIpAddress() : localAddr;

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    if (!ac)
        throw VoipLinkException("Could not instantiate codec for early media");

    std::vector<sfl::AudioCodec *> audioCodecs;
    audioCodecs.push_back(ac);

    try {
        call->getAudioRtp().initConfig();
        call->getAudioRtp().initSession();

        if (account->isStunEnabled())
            updateSDPFromSTUN(*call, *account, *SIPVoIPLink::instance().sipTransport);

        call->getAudioRtp().initLocalCryptoInfo();
        call->getAudioRtp().start(audioCodecs);
    } catch (...) {
        throw VoipLinkException("Could not start rtp session for early media");
    }

    call->initRecFilename(toUrl);

    Sdp *localSDP = call->getLocalSDP();
    localSDP->setPublishedIP(addrSdp);
    const bool created = localSDP->createOffer(account->getActiveAudioCodecs(), account->getActiveVideoCodecs());

    if (not created or not SIPStartCall(call))
        throw VoipLinkException("Could not send outgoing INVITE request for new call");

    return call;
}

void
SIPVoIPLink::answer(Call *call)
{
    if (!call)
        return;

    SIPCall *sipCall = static_cast<SIPCall*>(call);
    SIPAccount *account = Manager::instance().getSipAccount(sipCall->getAccountId());

    if (!account) {
        ERROR("Could not find account %s", sipCall->getAccountId().c_str());
        return;
    }

    if (!sipCall->inv->neg) {
        WARN("Negotiator is NULL, we've received an INVITE without an SDP");
        pjmedia_sdp_session *dummy = 0;
        sdp_create_offer_cb(sipCall->inv, &dummy);

        if (account->isStunEnabled())
            updateSDPFromSTUN(*sipCall, *account, *SIPVoIPLink::instance().sipTransport);
    }

    pj_str_t contact(account->getContactHeader());
    sipCall->setContactHeader(&contact);
    sipCall->answer();
}

static void
stopRtpIfCurrent(const std::string &id, SIPCall &call)
{
    if (Manager::instance().isCurrentCall(id)) {
        call.getAudioRtp().stop();
#ifdef SFL_VIDEO
        call.getVideoRtp().stop();
#endif
    }
}

void
SIPVoIPLink::hangup(const std::string& id, int reason)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    std::string account_id(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (account == NULL)
        throw VoipLinkException("Could not find account for this call");

    pjsip_inv_session *inv = call->inv;

    if (inv == NULL)
        throw VoipLinkException("No invite session for this call");

    pjsip_route_hdr *route = inv->dlg->route_set.next;

    while (route and route != &inv->dlg->route_set) {
        char buf[1024];
        int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));

        if (printed >= 0) {
            buf[printed] = '\0';
            DEBUG("Route header %s", buf);
        }

        route = route->next;
    }

    pjsip_tx_data *tdata = NULL;

    const int status = reason ? reason :
                       inv->state <= PJSIP_INV_STATE_EARLY and inv->role != PJSIP_ROLE_UAC ?
                       PJSIP_SC_CALL_TSX_DOES_NOT_EXIST :
                       inv->state >= PJSIP_INV_STATE_DISCONNECTED ? PJSIP_SC_DECLINE :
                       0;

    // User hangup current call. Notify peer
    if (pjsip_inv_end_session(inv, status, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    // contactStr must stay in scope as long as tdata
    const pj_str_t contactStr(account->getContactHeader());
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    inv->mod_data[mod_ua_.id] = NULL;

    stopRtpIfCurrent(id, *call);
    removeSipCall(id);
}

void
SIPVoIPLink::peerHungup(const std::string& id)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    // User hangup current call. Notify peer
    pjsip_tx_data *tdata = NULL;

    if (pjsip_inv_end_session(call->inv, 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    call->inv->mod_data[mod_ua_.id ] = NULL;

    stopRtpIfCurrent(id, *call);
    removeSipCall(id);
}

void
SIPVoIPLink::onhold(const std::string& id)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    call->onhold();
}

void
SIPVoIPLink::offhold(const std::string& id)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    SIPAccount *account = Manager::instance().getSipAccount(call->getAccountId());

    try {
        if (account and account->isStunEnabled())
            call->offhold([&] { updateSDPFromSTUN(*call, *account, *sipTransport); });
        else
            call->offhold([] {});

    } catch (const SdpException &e) {
        ERROR("%s", e.what());
        throw VoipLinkException("SDP issue in offhold");
    } catch (const ost::Socket::Error &e) {
        throw VoipLinkException("Socket problem in offhold");
    } catch (const ost::Socket *) {
        throw VoipLinkException("Socket problem in offhold");
    } catch (const AudioRtpFactoryException &) {
        throw VoipLinkException("Socket problem in offhold");
    }
}

#if HAVE_INSTANT_MESSAGING
void SIPVoIPLink::sendTextMessage(const std::string &callID,
                                  const std::string &message,
                                  const std::string &from)
{
    using namespace sfl::InstantMessaging;
    auto call = getSipCall(callID);
    if (!call)
        return;

    /* Send IM message */
    UriList list;
    UriEntry entry;
    entry[sfl::IM_XML_URI] = std::string("\"" + from + "\"");  // add double quotes for xml formating
    list.push_front(entry);
    send_sip_message(call->inv, callID, appendUriList(message, list));
}
#endif // HAVE_INSTANT_MESSAGING

void
SIPVoIPLink::clearSipCallMap()
{
    std::lock_guard<std::mutex> lock(sipCallMapMutex_);
    sipCallMap_.clear();
}


std::vector<std::string>
SIPVoIPLink::getCallIDs()
{
    std::vector<std::string> v;
    std::lock_guard<std::mutex> lock(sipCallMapMutex_);

    map_utils::vectorFromMapKeys(sipCallMap_, v);
    return v;
}

std::vector<std::shared_ptr<Call> >
SIPVoIPLink::getCalls(const std::string &account_id) const
{
    std::lock_guard<std::mutex> lock(sipCallMapMutex_);

    std::vector<std::shared_ptr<Call> > calls;
    for (const auto & item : sipCallMap_) {
        if (item.second->getAccountId() == account_id)
            calls.push_back(item.second);
    }
    return calls;
}

void SIPVoIPLink::addSipCall(std::shared_ptr<SIPCall>& call)
{
    if (!call)
        return;

    const std::string id(call->getCallId());

    std::lock_guard<std::mutex> lock(sipCallMapMutex_);

    // emplace C++11 method has been implemented in GCC 4.8.0
    // see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44436
#if !defined(__GNUC__) or (__GNUC__ >= 4 && __GNUC_MINOR__ >= 8)
    if (not sipCallMap_.emplace(id, call).second)
#else
    if (sipCallMap_.find(id) == sipCallMap_.end()) {
        sipCallMap_[id] = call;
    } else
#endif
        ERROR("Call %s is already in the call map", id.c_str());

}

void SIPVoIPLink::removeSipCall(const std::string& id)
{
    std::lock_guard<std::mutex> lock(sipCallMapMutex_);

    DEBUG("Removing call %s from list", id.c_str());
    SipCallMap::iterator iter = sipCallMap_.find(id);
    if (iter != sipCallMap_.end()) {
        auto count = iter->second.use_count();
        if (count > 1)
            WARN("removing a used SIPCall (by %d holders)", count);
    }
    sipCallMap_.erase(id);
}

std::shared_ptr<SIPCall>
SIPVoIPLink::getSipCall(const std::string& id)
{
    std::lock_guard<std::mutex> lock(sipCallMapMutex_);

    SipCallMap::iterator iter = sipCallMap_.find(id);

    if (iter != sipCallMap_.end())
        return iter->second;
    else {
        DEBUG("No SIP call with ID %s", id.c_str());
        return nullptr;
    }
}

std::shared_ptr<SIPCall>
SIPVoIPLink::tryGetSIPCall(const std::string& id)
{
    std::shared_ptr<SIPCall> call;

    if (sipCallMapMutex_.try_lock()) {
        SipCallMap::iterator iter = sipCallMap_.find(id);

        if (iter != sipCallMap_.end())
            call = iter->second;

        sipCallMapMutex_.unlock();
    } else
        ERROR("Could not acquire SIPCallMap mutex");

    return call;
}

bool
SIPVoIPLink::transferCommon(SIPCall *call, pj_str_t *dst)
{
    if (!call or !call->inv)
        return false;

    pjsip_evsub_user xfer_cb;
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    pjsip_evsub *sub;

    if (pjsip_xfer_create_uac(call->inv->dlg, &xfer_cb, &sub) != PJ_SUCCESS)
        return false;

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data(sub, mod_ua_.id, this);

    /*
     * Create REFER request.
     */
    pjsip_tx_data *tdata;

    if (pjsip_xfer_initiate(sub, dst, &tdata) != PJ_SUCCESS)
        return false;

    // Put SIP call id in map in order to retrieve call during transfer callback
    std::string callidtransfer(call->inv->dlg->call_id->id.ptr, call->inv->dlg->call_id->id.slen);
    transferCallID[callidtransfer] = call->getCallId();

    /* Send. */
    if (pjsip_xfer_send_request(sub, tdata) != PJ_SUCCESS)
        return false;

    return true;
}

void
SIPVoIPLink::transfer(const std::string& id, const std::string& to)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    call->stopRecording();

    std::string account_id(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (account == NULL)
        throw VoipLinkException("Could not find account");

    std::string toUri;
    pj_str_t dst = { 0, 0 };

    toUri = account->getToUri(to);
    pj_cstr(&dst, toUri.c_str());
    DEBUG("Transferring to %.*s", dst.slen, dst.ptr);

    if (!transferCommon(call.get(), &dst))
        throw VoipLinkException("Couldn't transfer");
}

bool SIPVoIPLink::attendedTransfer(const std::string& id, const std::string& to)
{
    const auto toCall = getSipCall(to);
    if (!toCall)
        return false;

    if (!toCall->inv or !toCall->inv->dlg)
        throw VoipLinkException("Couldn't get invite dialog");

    pjsip_dialog *target_dlg = toCall->inv->dlg;
    pjsip_uri *uri = (pjsip_uri*) pjsip_uri_get_uri(target_dlg->remote.info->uri);

    char str_dest_buf[PJSIP_MAX_URL_SIZE * 2] = { '<' };
    pj_str_t dst = { str_dest_buf, 1 };

    dst.slen += pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri, str_dest_buf + 1, sizeof(str_dest_buf) - 1);
    dst.slen += pj_ansi_snprintf(str_dest_buf + dst.slen,
                                 sizeof(str_dest_buf) - dst.slen,
                                 "?"
                                 "Replaces=%.*s"
                                 "%%3Bto-tag%%3D%.*s"
                                 "%%3Bfrom-tag%%3D%.*s>",
                                 (int)target_dlg->call_id->id.slen,
                                 target_dlg->call_id->id.ptr,
                                 (int)target_dlg->remote.info->tag.slen,
                                 target_dlg->remote.info->tag.ptr,
                                 (int)target_dlg->local.info->tag.slen,
                                 target_dlg->local.info->tag.ptr);

    auto call = getSipCall(id);
    if (!call)
        return false;

    return transferCommon(call.get(), &dst);
}

void
SIPVoIPLink::refuse(const std::string& id)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    if (!call->isIncoming() or call->getConnectionState() == Call::CONNECTED or !call->inv)
        return;

    call->getAudioRtp().stop();

    pjsip_tx_data *tdata;

    if (pjsip_inv_end_session(call->inv, PJSIP_SC_DECLINE, NULL, &tdata) != PJ_SUCCESS)
        return;

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure the pointer is NULL in callbacks
    call->inv->mod_data[mod_ua_.id] = NULL;

    removeSipCall(id);
}

static void
sendSIPInfo(const SIPCall &call, const char *const body, const char *const subtype)
{
    pj_str_t methodName = CONST_PJ_STR("INFO");
    pjsip_method method;
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    pjsip_tx_data *tdata;

    if (pjsip_dlg_create_request(call.inv->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
        ERROR("Could not create dialog");
        return;
    }

    /* Create "application/<subtype>" message body. */
    pj_str_t content;
    pj_cstr(&content, body);
    const pj_str_t type = CONST_PJ_STR("application");
    pj_str_t pj_subtype;
    pj_cstr(&pj_subtype, subtype);
    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &pj_subtype, &content);

    if (tdata->msg->body == NULL)
        pjsip_tx_data_dec_ref(tdata);
    else
        pjsip_dlg_send_request(call.inv->dlg, tdata, mod_ua_.id, NULL);
}

static void
dtmfSend(SIPCall &call, char code, const std::string &dtmf)
{
    if (dtmf == SIPAccount::OVERRTP_STR) {
        call.getAudioRtp().sendDtmfDigit(code);
        return;
    } else if (dtmf != SIPAccount::SIPINFO_STR) {
        WARN("Unknown DTMF type %s, defaulting to %s instead",
             dtmf.c_str(), SIPAccount::SIPINFO_STR);
    } // else : dtmf == SIPINFO

    int duration = Manager::instance().voipPreferences.getPulseLength();
    char dtmf_body[1000];

    const char *normal_str= "Signal=%c\r\nDuration=%d\r\n";
    const char *flash_str = "Signal=%d\r\nDuration=%d\r\n";
    const char *str;

    // handle flash code
    if (code == '!') {
        str = flash_str;
        code = 16;
    } else {
        str = normal_str;
    }

    snprintf(dtmf_body, sizeof dtmf_body - 1, str, code, duration);
    sendSIPInfo(call, dtmf_body, "dtmf-relay");
}

#ifdef SFL_VIDEO
// Called from a video thread
void
SIPVoIPLink::enqueueKeyframeRequest(const std::string &id)
{
    std::lock_guard<std::mutex> lock(instance_->keyframeRequestsMutex_);
    instance_->keyframeRequests_.push(id);
}

// Called from SIP event thread
void
SIPVoIPLink::dequeKeyframeRequests()
{
    int max_requests = 20;

    while (not keyframeRequests_.empty() and max_requests--) {
        std::lock_guard<std::mutex> lock(keyframeRequestsMutex_);
        const std::string &id(keyframeRequests_.front());
        requestKeyframe(id);
        keyframeRequests_.pop();
    }
}

// Called from SIP event thread
void
SIPVoIPLink::requestKeyframe(const std::string &callID)
{
    std::shared_ptr<SIPCall> call;
    const int tries = 10;

    for (int i = 0; !call and i < tries; ++i)
        call = SIPVoIPLink::instance().tryGetSIPCall(callID);

    if (!call)
        return;

    const char * const BODY =
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<media_control><vc_primitive><to_encoder>"
        "<picture_fast_update/>"
        "</to_encoder></vc_primitive></media_control>";

    DEBUG("Sending video keyframe request via SIP INFO");
    sendSIPInfo(*call, BODY, "media_control+xml");
}
#endif

void
SIPVoIPLink::carryingDTMFdigits(const std::string& id, char code)
{
    auto call = getSipCall(id);
    if (!call)
        return;

    const std::string accountID(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(accountID);

    if (!account)
        return;

    dtmfSend(*call, code, account->getDtmfType());
}


bool
SIPVoIPLink::SIPStartCall(std::shared_ptr<SIPCall>& call)
{
    std::string account_id(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (!account) {
        ERROR("Account is NULL in SIPStartCall");
        return false;
    }

    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(account->getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    pj_str_t pjContact(account->getContactHeader());

    const std::string debugContactHeader(pj_strbuf(&pjContact), pj_strlen(&pjContact));
    DEBUG("contact header: %s / %s -> %s",
          debugContactHeader.c_str(), from.c_str(), toUri.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS) {
        ERROR("Unable to create SIP dialogs for user agent client when "
              "calling %s", toUri.c_str());
        return false;
    }

    pj_str_t subj_hdr_name = CONST_PJ_STR("Subject");
    pjsip_hdr* subj_hdr = (pjsip_hdr*) pjsip_parse_hdr(dialog->pool, &subj_hdr_name, (char *) "Phone call", 10, NULL);

    pj_list_push_back(&dialog->inv_hdr, subj_hdr);

    if (pjsip_inv_create_uac(dialog, call->getLocalSDP()->getLocalSdpSession(), 0, &call->inv) != PJ_SUCCESS) {
        ERROR("Unable to create invite session for user agent client");
        return false;
    }

    account->updateDialogViaSentBy(dialog);

    if (account->hasServiceRoute())
        pjsip_dlg_set_route_set(dialog, sip_utils::createRouteSet(account->getServiceRoute(), call->inv->pool));

    if (account->hasCredentials() and pjsip_auth_clt_set_credentials(&dialog->auth_sess, account->getCredentialCount(), account->getCredInfo()) != PJ_SUCCESS) {
        ERROR("Could not initialize credentials for invite session authentication");
        return false;
    }

    call->inv->mod_data[mod_ua_.id] = call.get();

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv, &tdata) != PJ_SUCCESS) {
        ERROR("Could not initialize invite messager for this call");
        return false;
    }

    const pjsip_tpselector tp_sel = account->getTransportSelector();
    if (pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        ERROR("Unable to associate transport for invite session dialog");
        return false;
    }

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS) {
        ERROR("Unable to send invite message for this call");
        return false;
    }

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);
    addSipCall(call);

    return true;
}

void
SIPVoIPLink::SIPCallServerFailure(SIPCall *call)
{
    std::string id(call->getCallId());
    Manager::instance().callFailure(id);
    removeSipCall(id);
}

void
SIPVoIPLink::SIPCallClosed(SIPCall *call)
{
    const std::string id(call->getCallId());

    stopRtpIfCurrent(id, *call);

    Manager::instance().peerHungupCall(id);
    removeSipCall(id);
    Manager::instance().checkAudio();
}

void
SIPVoIPLink::SIPCallAnswered(SIPCall *call, pjsip_rx_data * /*rdata*/)
{
    if (call->getConnectionState() != Call::CONNECTED) {
        call->setConnectionState(Call::CONNECTED);
        call->setState(Call::ACTIVE);
        Manager::instance().peerAnsweredCall(call->getCallId());
    }
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

static void
makeCallRing(SIPCall &call)
{
    call.setConnectionState(Call::RINGING);
    Manager::instance().peerRingingCall(call.getCallId());
}

static void
invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *ev)
{
    if (!inv)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call)
        return;

    if (ev and inv->state != PJSIP_INV_STATE_CONFIRMED) {
        // Update UI with the current status code and description
        pjsip_transaction * tsx = ev->body.tsx_state.tsx;
        int statusCode = tsx ? tsx->status_code : 404;

        if (statusCode) {
            const pj_str_t * description = pjsip_get_status_text(statusCode);
            std::string desc(description->ptr, description->slen);

            CallManager *cm = Manager::instance().getClient()->getCallManager();
            cm->sipCallStateChanged(call->getCallId(), desc, statusCode);
        }
    }

    SIPVoIPLink& link = SIPVoIPLink::instance();

    if (inv->state == PJSIP_INV_STATE_EARLY and ev and ev->body.tsx_state.tsx and
            ev->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
        makeCallRing(*call);
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED and ev) {
        // After we sent or received a ACK - The connection is established
        link.SIPCallAnswered(call, ev->body.tsx_state.src.rdata);
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        std::string accId(call->getAccountId());

        switch (inv->cause) {
                // The call terminates normally - BYE / CANCEL
            case PJSIP_SC_OK:
            case PJSIP_SC_REQUEST_TERMINATED:
                link.SIPCallClosed(call);
                break;

            case PJSIP_SC_DECLINE:
                if (inv->role != PJSIP_ROLE_UAC)
                    break;

            case PJSIP_SC_NOT_FOUND:
            case PJSIP_SC_REQUEST_TIMEOUT:
            case PJSIP_SC_NOT_ACCEPTABLE_HERE:  /* no compatible codecs */
            case PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE:
            case PJSIP_SC_UNSUPPORTED_MEDIA_TYPE:
            case PJSIP_SC_UNAUTHORIZED:
            case PJSIP_SC_FORBIDDEN:
            case PJSIP_SC_REQUEST_PENDING:
            case PJSIP_SC_ADDRESS_INCOMPLETE:
            default:
                link.SIPCallServerFailure(call);
                break;
        }
    }
}

static void
sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
    if (!inv)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call)
        return;

    std::string accId(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(accId);

    if (!account)
        return;

    call->getLocalSDP()->receiveOffer(offer, account->getActiveAudioCodecs(), account->getActiveVideoCodecs());
    call->getLocalSDP()->startNegotiation();

    pjsip_inv_set_sdp_answer(call->inv, call->getLocalSDP()->getLocalSdpSession());
}

static void
sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    if (!inv or !p_offer)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call)
        return;

    std::string accountid(call->getAccountId());

    SIPAccount *account = Manager::instance().getSipAccount(accountid);

    if (!account)
        return;

    // FIXME : for now, use the same address family as the SIP tranport
    auto family = pjsip_transport_type_get_af(account->getTransportType());
    IpAddr address = account->getPublishedSameasLocal()
                    ? IpAddr(ip_utils::getInterfaceAddr(account->getLocalInterface(), family))
                    : account->getPublishedIpAddress();

    setCallMediaLocal(call, address);

    Sdp *localSDP = call->getLocalSDP();
    localSDP->setPublishedIP(address);
    const bool created = localSDP->createOffer(account->getActiveAudioCodecs(), account->getActiveVideoCodecs());

    if (created)
        *p_offer = localSDP->getLocalSdpSession();
}

// This callback is called after SDP offer/answer session has completed.
static void
sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status)
{
    if (!inv)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call) {
        DEBUG("Call declined by peer, SDP negotiation stopped");
        return;
    }

    if (status != PJ_SUCCESS) {
        const int reason = inv->state != PJSIP_INV_STATE_NULL and
                           inv->state != PJSIP_INV_STATE_CONFIRMED ?
                           PJSIP_SC_UNSUPPORTED_MEDIA_TYPE : 0;

        WARN("Could not negotiate offer");
        const std::string callID(call->getCallId());
        SIPVoIPLink::instance().hangup(callID, reason);
        // call is now a dangling pointer after calling hangup
        call = 0;
        Manager::instance().callFailure(callID);
        return;
    }

    if (!inv->neg) {
        WARN("No negotiator for this session");
        return;
    }

    // Retreive SDP session for this call
    Sdp *sdpSession = call->getLocalSDP();

    if (!sdpSession) {
        ERROR("No SDP session");
        return;
    }

    // Get active session sessions
    const pjmedia_sdp_session *remoteSDP = 0;

    if (pjmedia_sdp_neg_get_active_remote(inv->neg, &remoteSDP) != PJ_SUCCESS) {
        ERROR("Active remote not present");
        return;
    }

    if (pjmedia_sdp_validate(remoteSDP) != PJ_SUCCESS) {
        ERROR("Invalid remote SDP session");
        return;
    }

    const pjmedia_sdp_session *local_sdp;
    pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    if (pjmedia_sdp_validate(local_sdp) != PJ_SUCCESS) {
        ERROR("Invalid local SDP session");
        return;
    }

    // Print SDP session
    char buffer[4096];
    memset(buffer, 0, sizeof buffer);

    if (pjmedia_sdp_print(remoteSDP, buffer, sizeof buffer) == -1) {
        ERROR("SDP was too big for buffer");
        return;
    }

    DEBUG("Remote active SDP Session:\n%s", buffer);

    memset(buffer, 0, sizeof buffer);

    if (pjmedia_sdp_print(local_sdp, buffer, sizeof buffer) == -1) {
        ERROR("SDP was too big for buffer");
        return;
    }

    DEBUG("Local active SDP Session:\n%s", buffer);

    // Set active SDP sessions
    sdpSession->setActiveLocalSdpSession(local_sdp);
    sdpSession->setActiveRemoteSdpSession(remoteSDP);

    // Update internal field for
    sdpSession->setMediaTransportInfoFromRemoteSdp();

    try {
        call->getAudioRtp().updateDestinationIpAddress();
    } catch (const AudioRtpFactoryException &e) {
        ERROR("%s", e.what());
    }

    call->getAudioRtp().setDtmfPayloadType(sdpSession->getTelephoneEventType());
#ifdef SFL_VIDEO
    call->getVideoRtp().updateSDP(*call->getLocalSDP());
    call->getVideoRtp().updateDestination(call->getLocalSDP()->getRemoteIP(), call->getLocalSDP()->getRemoteVideoPort());
    auto localPort = call->getLocalSDP()->getLocalVideoPort();
    if (!localPort)
        localPort = call->getLocalSDP()->getRemoteVideoPort();
    call->getVideoRtp().start(localPort);
#endif

    // Get the crypto attribute containing srtp's cryptographic context (keys, cipher)
    CryptoOffer crypto_offer;
    call->getLocalSDP()->getRemoteSdpCryptoFromOffer(remoteSDP, crypto_offer);

#if HAVE_SDES
    bool nego_success = false;

    if (!crypto_offer.empty()) {
        std::vector<sfl::CryptoSuiteDefinition> localCapabilities;

        for (size_t i = 0; i < ARRAYSIZE(sfl::CryptoSuites); ++i)
            localCapabilities.push_back(sfl::CryptoSuites[i]);

        sfl::SdesNegotiator sdesnego(localCapabilities, crypto_offer);

        if (sdesnego.negotiate()) {
            nego_success = true;

            try {
                call->getAudioRtp().setRemoteCryptoInfo(sdesnego);
                Manager::instance().getClient()->getCallManager()->secureSdesOn(call->getCallId());
            } catch (const AudioRtpFactoryException &e) {
                ERROR("%s", e.what());
                Manager::instance().getClient()->getCallManager()->secureSdesOff(call->getCallId());
            }
        } else {
            ERROR("SDES negotiation failure");
            Manager::instance().getClient()->getCallManager()->secureSdesOff(call->getCallId());
        }
    } else {
        DEBUG("No crypto offer available");
    }

    // We did not find any crypto context for this media, RTP fallback
    if (!nego_success && call->getAudioRtp().isSdesEnabled()) {
        ERROR("Negotiation failed but SRTP is enabled, fallback on RTP");
        call->getAudioRtp().stop();
        call->getAudioRtp().setSrtpEnabled(false);

        const std::string accountID = call->getAccountId();

        SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);

        if (sipaccount and sipaccount->getSrtpFallback()) {
            call->getAudioRtp().initSession();

            if (sipaccount->isStunEnabled())
                updateSDPFromSTUN(*call, *sipaccount, *SIPVoIPLink::instance().sipTransport);
        }
    }

#endif // HAVE_SDES

    std::vector<sfl::AudioCodec*> sessionMedia(sdpSession->getSessionAudioMedia());

    if (sessionMedia.empty()) {
        WARN("Session media is empty");
        return;
    }

    try {
        Manager::instance().startAudioDriverStream();

        std::vector<AudioCodec*> audioCodecs;

        for (const auto & i : sessionMedia) {
            if (!i)
                continue;

            const int pl = i->getPayloadType();

            sfl::AudioCodec *ac = Manager::instance().audioCodecFactory.instantiateCodec(pl);

            if (!ac) {
                ERROR("Could not instantiate codec %d", pl);
            } else {
                audioCodecs.push_back(ac);
            }
        }

        if (not audioCodecs.empty())
            call->getAudioRtp().updateSessionMedia(audioCodecs);
    } catch (const SdpException &e) {
        ERROR("%s", e.what());
    } catch (const std::exception &rtpException) {
        ERROR("%s", rtpException.what());
    }
}

static void
outgoing_request_forked_cb(pjsip_inv_session * /*inv*/, pjsip_event * /*e*/)
{}

static bool
handle_media_control(pjsip_inv_session * inv, pjsip_transaction *tsx, pjsip_event *event)
{
    /*
     * Incoming INFO request for media control.
     */
    const pj_str_t STR_APPLICATION = CONST_PJ_STR("application");
    const pj_str_t STR_MEDIA_CONTROL_XML = CONST_PJ_STR("media_control+xml");
    pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
    pjsip_msg_body *body = rdata->msg_info.msg->body;

    if (body and body->len and pj_stricmp(&body->content_type.type, &STR_APPLICATION) == 0 and
            pj_stricmp(&body->content_type.subtype, &STR_MEDIA_CONTROL_XML) == 0) {
        pj_str_t control_st;

        /* Apply and answer the INFO request */
        pj_strset(&control_st, (char *) body->data, body->len);
        const pj_str_t PICT_FAST_UPDATE = CONST_PJ_STR("picture_fast_update");

        if (pj_strstr(&control_st, &PICT_FAST_UPDATE)) {
#ifdef SFL_VIDEO
            DEBUG("handling picture fast update request");
            auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
            if (call)
                call->getVideoRtp().forceKeyFrame();

            pjsip_tx_data *tdata;
            pj_status_t status = pjsip_endpt_create_response(tsx->endpt, rdata,
                                 PJSIP_SC_OK, NULL, &tdata);

            if (status == PJ_SUCCESS) {
                status = pjsip_tsx_send_msg(tsx, tdata);
                return true;
            }

#else
            (void) inv;
            (void) tsx;
#endif
        }
    }

    return false;
}

static void
sendOK(pjsip_dialog *dlg, pjsip_rx_data *r_data, pjsip_transaction *tsx)
{
    pjsip_tx_data* t_data;

    if (pjsip_dlg_create_response(dlg, r_data, PJSIP_SC_OK, NULL, &t_data) == PJ_SUCCESS)
        pjsip_dlg_send_response(dlg, tsx, t_data);
}

static void
transaction_state_changed_cb(pjsip_inv_session * inv, pjsip_transaction *tsx,
                             pjsip_event *event)
{
    if (!tsx or !event or !inv or tsx->role != PJSIP_ROLE_UAS or
            tsx->state != PJSIP_TSX_STATE_TRYING)
        return;

    // Handle the refer method
    if (pjsip_method_cmp(&tsx->method, &pjsip_refer_method) == 0) {
        onCallTransfered(inv, event->body.tsx_state.src.rdata);
        return;
    }

    if (tsx->role == PJSIP_ROLE_UAS and tsx->state == PJSIP_TSX_STATE_TRYING) {
        if (handle_media_control(inv, tsx, event))
            return;
    }

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);

    if (event->body.rx_msg.rdata) {
        pjsip_rx_data *r_data = event->body.rx_msg.rdata;

        if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {
            std::string request(pjsip_rx_data_get_info(r_data));
            DEBUG("%s", request.c_str());

            if (request.find("NOTIFY") == std::string::npos and
                    request.find("INFO") != std::string::npos) {
                sendOK(inv->dlg, r_data, tsx);
                return;
            }

            pjsip_msg_body *body(r_data->msg_info.msg->body);

            if (body and body->len > 0) {
                const std::string msg(static_cast<char *>(body->data), body->len);
                DEBUG("%s", msg.c_str());

                if (msg.find("Not found") != std::string::npos) {
                    ERROR("Received 404 Not found");
                    sendOK(inv->dlg, r_data, tsx);
                    return;
                } else if (msg.find("Ringing") != std::string::npos and call) {
                    if (call)
                        makeCallRing(*call);
                    else
                        WARN("Ringing state on non existing call");
                    sendOK(inv->dlg, r_data, tsx);
                    return;
                } else if (msg.find("Ok") != std::string::npos) {
                    sendOK(inv->dlg, r_data, tsx);
                    return;
                }
            }
        }
    }

#if HAVE_INSTANT_MESSAGING
    if (!call)
        return;

    // Incoming TEXT message
    pjsip_rx_data *r_data = event->body.tsx_state.src.rdata;

    // Get the message inside the transaction
    if (!r_data or !r_data->msg_info.msg->body)
        return;

    const char *formattedMsgPtr = static_cast<const char*>(r_data->msg_info.msg->body->data);

    if (!formattedMsgPtr)
        return;

    std::string formattedMessage(formattedMsgPtr, strlen(formattedMsgPtr));

    using namespace sfl::InstantMessaging;

    try {
        // retreive the recipient-list of this message
        std::string urilist = findTextUriList(formattedMessage);
        UriList list = parseXmlUriList(urilist);

        // If no item present in the list, peer is considered as the sender
        std::string from;

        if (list.empty()) {
            from = call->getPeerNumber();
        } else {
            from = list.front()[IM_XML_URI];

            if (from == "Me")
                from = call->getPeerNumber();
        }

        // strip < and > characters in case of an IP address
        if (from[0] == '<' && from[from.size() - 1] == '>')
            from = from.substr(1, from.size() - 2);

        Manager::instance().incomingMessage(call->getCallId(), from, findTextMessage(formattedMessage));

        // Respond with a 200/OK
        sendOK(inv->dlg, r_data, tsx);

    } catch (const sfl::InstantMessageException &except) {
        ERROR("%s", except.what());
    }
#endif
}

static void
registration_cb(pjsip_regc_cbparam *param)
{
    if (!param) {
        ERROR("registration callback parameter is null");
        return;
    }

    SIPAccount *account = static_cast<SIPAccount *>(param->token);

    if (!account) {
        ERROR("account doesn't exist in registration callback");
        return;
    }

    if (param->regc != account->getRegistrationInfo())
        return;

    const std::string accountID = account->getAccountID();

    if (param->status != PJ_SUCCESS) {
        ERROR("SIP registration error %d", param->status);
        account->destroyRegistrationInfo();
        account->stopKeepAliveTimer();
    } else if (param->code < 0 || param->code >= 300) {
        ERROR("SIP registration failed, status=%d (%.*s)",
              param->code, (int)param->reason.slen, param->reason.ptr);
        account->destroyRegistrationInfo();
        account->stopKeepAliveTimer();
        switch (param->code) {
            case PJSIP_SC_FORBIDDEN:
                account->setRegistrationState(RegistrationState::ERROR_AUTH);
                break;
            case PJSIP_SC_NOT_FOUND:
                account->setRegistrationState(RegistrationState::ERROR_HOST);
                break;
            case PJSIP_SC_REQUEST_TIMEOUT:
                account->setRegistrationState(RegistrationState::ERROR_HOST);
                break;
            case PJSIP_SC_SERVICE_UNAVAILABLE:
                account->setRegistrationState(RegistrationState::ERROR_SERVICE_UNAVAILABLE);
                break;
            default:
                account->setRegistrationState(RegistrationState::ERROR_GENERIC);
        }
    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

        // Update auto registration flag
        account->resetAutoRegistration();

        if (param->expiration < 1) {
            account->destroyRegistrationInfo();
            /* Stop keep-alive timer if any. */
            account->stopKeepAliveTimer();
            DEBUG("Unregistration success");
            account->setRegistrationState(RegistrationState::UNREGISTERED);
        } else {
            /* TODO Check and update SIP outbound status first, since the result
             * will determine if we should update re-registration
             */
            // update_rfc5626_status(acc, param->rdata);

            if (account->checkNATAddress(param, pool_))
                WARN("Contact overwritten");

            /* TODO Check and update Service-Route header */
            if (account->hasServiceRoute())
                pjsip_regc_set_route_set(param->regc, sip_utils::createRouteSet(account->getServiceRoute(), pool_));

            // start the periodic registration request based on Expire header
            // account determines itself if a keep alive is required
            if (account->isKeepAliveEnabled())
                account->startKeepAliveTimer();

            account->setRegistrationState(RegistrationState::REGISTERED);
        }
    }

    /* Check if we need to auto retry registration. Basically, registration
     * failure codes triggering auto-retry are those of temporal failures
     * considered to be recoverable in relatively short term.
     */
    switch (param->code) {
        case PJSIP_SC_REQUEST_TIMEOUT:
        case PJSIP_SC_INTERNAL_SERVER_ERROR:
        case PJSIP_SC_BAD_GATEWAY:
        case PJSIP_SC_SERVICE_UNAVAILABLE:
        case PJSIP_SC_SERVER_TIMEOUT:
            account->scheduleReregistration(endpt_);
            break;

        default:
            /* Global failure */
            if (PJSIP_IS_STATUS_IN_CLASS(param->code, 600))
                account->scheduleReregistration(endpt_);
    }

    const pj_str_t *description = pjsip_get_status_text(param->code);

    if (param->code && description) {
        std::string state(description->ptr, description->slen);

        Manager::instance().getClient()->getConfigurationManager()->sipRegistrationStateChanged(accountID, state, param->code);
        std::pair<int, std::string> details(param->code, state);
        // TODO: there id a race condition for this ressource when closing the application
        account->setRegistrationStateDetailed(details);
        account->setRegistrationExpire(param->expiration);
    }
#undef FAILURE_MESSAGE
}

static void
onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    SIPCall *currentCall = static_cast<SIPCall *>(inv->mod_data[mod_ua_.id]);

    if (currentCall == NULL)
        return;

    static const pj_str_t str_refer_to = CONST_PJ_STR("Refer-To");
    pjsip_generic_string_hdr *refer_to = static_cast<pjsip_generic_string_hdr*>
                                         (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL));

    if (!refer_to) {
        pjsip_dlg_respond(inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    try {
        SIPVoIPLink::instance().newOutgoingCall(Manager::instance().getNewCallID(),
                std::string(refer_to->hvalue.ptr, refer_to->hvalue.slen), currentCall->getAccountId());
        Manager::instance().hangupCall(currentCall->getCallId());
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
    }
}

static void
transfer_client_cb(pjsip_evsub *sub, pjsip_event *event)
{
    switch (pjsip_evsub_get_state(sub)) {
        case PJSIP_EVSUB_STATE_ACCEPTED:
            if (!event)
                return;

            pj_assert(event->type == PJSIP_EVENT_TSX_STATE && event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
            break;

        case PJSIP_EVSUB_STATE_TERMINATED:
            pjsip_evsub_set_mod_data(sub, mod_ua_.id, NULL);
            break;

        case PJSIP_EVSUB_STATE_ACTIVE: {
            SIPVoIPLink *link = static_cast<SIPVoIPLink *>(pjsip_evsub_get_mod_data(sub, mod_ua_.id));

            if (!link or !event)
                return;

            pjsip_rx_data* r_data = event->body.rx_msg.rdata;

            if (!r_data)
                return;

            std::string request(pjsip_rx_data_get_info(r_data));

            pjsip_status_line status_line = { 500, *pjsip_get_status_text(500) };

            if (!r_data->msg_info.msg)
                return;

            if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD and
                    request.find("NOTIFY") != std::string::npos) {
                pjsip_msg_body *body = r_data->msg_info.msg->body;

                if (!body)
                    return;

                if (pj_stricmp2(&body->content_type.type, "message") or
                        pj_stricmp2(&body->content_type.subtype, "sipfrag"))
                    return;

                if (pjsip_parse_status_line((char*) body->data, body->len, &status_line) != PJ_SUCCESS)
                    return;
            }

            if (!r_data->msg_info.cid)
                return;

            std::string transferID(r_data->msg_info.cid->id.ptr, r_data->msg_info.cid->id.slen);
            auto call = SIPVoIPLink::instance().getSipCall(transferCallID[transferID]);
            if (!call)
                return;

            if (status_line.code / 100 == 2) {
                pjsip_tx_data *tdata;

                if (!call->inv)
                    return;

                if (pjsip_inv_end_session(call->inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS)
                    pjsip_inv_send_msg(call->inv, tdata);

                Manager::instance().hangupCall(call->getCallId());
                pjsip_evsub_set_mod_data(sub, mod_ua_.id, NULL);
            }

            break;
        }

        default:
            break;
    }
}

static void
setCallMediaLocal(SIPCall* call, const pj_sockaddr& localIP)
{
    std::string account_id(call->getAccountId());
    SIPAccount *account = Manager::instance().getSipAccount(account_id);

    if (!account)
        return;

    // Reference: http://www.cs.columbia.edu/~hgs/rtp/faq.html#ports
    // We only want to set ports to new values if they haven't been set
    if (call->getLocalAudioPort() == 0) {
        const unsigned callLocalAudioPort = account->generateAudioPort();
        call->setLocalAudioPort(callLocalAudioPort);
        call->getLocalSDP()->setLocalPublishedAudioPort(callLocalAudioPort);
    }

    call->setLocalIp(localIP);

#ifdef SFL_VIDEO

    if (call->getLocalVideoPort() == 0) {
        // https://projects.savoirfairelinux.com/issues/17498
        const unsigned int callLocalVideoPort = account->generateVideoPort();
        // this should already be guaranteed by SIPAccount
        assert(call->getLocalAudioPort() != callLocalVideoPort);

        call->setLocalVideoPort(callLocalVideoPort);
        call->getLocalSDP()->setLocalPublishedVideoPort(callLocalVideoPort);
    }

#endif
}

int SIPVoIPLink::getModId()
{
    return mod_ua_.id;
}

void SIPVoIPLink::loadIP2IPSettings()
{
    try {
        const auto iter = sipAccountMap_.find(SIPAccount::IP2IP_PROFILE);
        // if IP2IP doesn't exist yet, create it
        if (iter == sipAccountMap_.end())
            sipAccountMap_[SIPAccount::IP2IP_PROFILE] = new SIPAccount(SIPAccount::IP2IP_PROFILE, true);

        SIPAccount *ip2ip = static_cast<SIPAccount*>(sipAccountMap_[SIPAccount::IP2IP_PROFILE]);

        ip2ip->registerVoIPLink();
        sipTransport->createSipTransport(*ip2ip);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }
}
