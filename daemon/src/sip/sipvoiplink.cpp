/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include "sipaccount.h"
#include "sip_utils.h"

#if HAVE_DHT
#include "ring/ringaccount.h"
#endif

#include "call_factory.h"

#include "manager.h"
#if HAVE_SDES
#include "sdes_negotiator.h"
#endif

#include "logger.h"
#include "array_size.h"
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

#include <istream>
#include <algorithm>

using namespace sfl;

/** Environment variable used to set pjsip's logging level */
#define SIPLOGLEVEL "SIPLOGLEVEL"

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/**
 * Set audio and video (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 */
static pj_caching_pool pool_cache;
static pj_pool_t *pool_;
static pjsip_endpoint *endpt_;
static pjsip_module mod_ua_;

static void sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status);
static void sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer);
static void sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer);
static void invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *e);
static void outgoing_request_forked_cb(pjsip_inv_session *inv, pjsip_event *e);
static void transaction_state_changed_cb(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);

pj_caching_pool* SIPVoIPLink::cp_ = &pool_cache;

decltype(getGlobalInstance<SIPVoIPLink>)& getSIPVoIPLink = getGlobalInstance<SIPVoIPLink>;

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
        SFL_ERR("Transaction has been created for this request, send response "
              "statefully instead");

    return !PJ_SUCCESS;
}

static pj_bool_t
transaction_request_cb(pjsip_rx_data *rdata)
{
    if (!rdata or !rdata->msg_info.msg) {
        SFL_ERR("rx_data is NULL");
        return PJ_FALSE;
    }

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

    if (!method) {
        SFL_ERR("method is NULL");
        return PJ_FALSE;
    }

    if (method->id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg(rdata))
        return PJ_FALSE;

    if (!rdata->msg_info.to or !rdata->msg_info.from or !rdata->msg_info.via) {
        SFL_ERR("Missing From, To or Via fields");
        return PJ_FALSE;
    }
    const pjsip_sip_uri *sip_to_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.to->uri);
    const pjsip_sip_uri *sip_from_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.from->uri);
    const pjsip_host_port& sip_via = rdata->msg_info.via->sent_by;

    if (!sip_to_uri or !sip_from_uri or !sip_via.host.ptr) {
        SFL_ERR("NULL uri");
        return PJ_FALSE;
    }
    std::string toUsername(sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string toHost(sip_to_uri->host.ptr, sip_to_uri->host.slen);
    std::string viaHostname(sip_via.host.ptr, sip_via.host.slen);
    const std::string remote_user(sip_from_uri->user.ptr, sip_from_uri->user.slen);
    const std::string remote_hostname(sip_from_uri->host.ptr, sip_from_uri->host.slen);

    auto account(getSIPVoIPLink()->guessAccount(toUsername, viaHostname, remote_hostname));
    if (!account) {
        SFL_ERR("NULL account");
        return PJ_FALSE;
    }

    const auto& account_id = account->getAccountID();
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

    auto call = account->newIncomingCall(Manager::instance().getNewCallID());

    // FIXME : for now, use the same address family as the SIP transport
    auto family = pjsip_transport_type_get_af(account->getTransportType());
    IpAddr addrToUse = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);

    // May use the published address as well
    IpAddr addrSdp = account->isStunEnabled() or (not account->getPublishedSameasLocal())
                    ? account->getPublishedIpAddress() : addrToUse;

    char tmp[PJSIP_MAX_URL_SIZE];
    size_t length = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, sip_from_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber(tmp, length);
    sip_utils::stripSipUriPrefix(peerNumber);

    if (not remote_user.empty() and not remote_hostname.empty())
        peerNumber = remote_user + "@" + remote_hostname;

    // SFL_DBG("transaction_request_cb viaHostname %s toUsername %s addrToUse %s addrSdp %s peerNumber: %s" ,
    // viaHostname.c_str(), toUsername.c_str(), addrToUse.toString().c_str(), addrSdp.toString().c_str(), peerNumber.c_str());

    auto transport = getSIPVoIPLink()->sipTransport->findTransport(rdata->tp_info.transport);
    if (!transport) {
        transport = account->getTransport();
        if (!transport) {
            SFL_ERR("No suitable transport to answer this call.");
            return PJ_FALSE;
        } else {
            SFL_WARN("Using transport from account.");
        }
    }

    call->setConnectionState(Call::PROGRESSING);
    call->setPeerNumber(peerNumber);
    call->setDisplayName(displayName);
    call->initRecFilename(peerNumber);
    call->setCallMediaLocal(addrToUse);
    call->getLocalSDP().setPublishedIP(addrSdp);
    call->getAudioRtp().initConfig();
    call->setTransport(transport);

    try {
        call->getAudioRtp().initSession();
    } catch (const ost::Socket::Error &err) {
        SFL_ERR("AudioRtp socket error");
        return PJ_FALSE;
    }

    if (account->isStunEnabled())
        call->updateSDPFromSTUN();

    if (body and body->len > 0 and call->getAudioRtp().isSdesEnabled()) {
        std::string sdpOffer(static_cast<const char*>(body->data), body->len);
        size_t start = sdpOffer.find("a=crypto:");

        // Found crypto header in SDP
        if (start != std::string::npos) {
            CryptoOffer crypto_offer;
            crypto_offer.push_back(std::string(sdpOffer.substr(start, (sdpOffer.size() - start) - 1)));

            const size_t size = SFL_ARRAYSIZE(sfl::CryptoSuites);
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
                    SFL_ERR("%s", e.what());
                    return PJ_FALSE;
                }
            }

#endif
        }
    }

    call->getLocalSDP().receiveOffer(r_sdp, account->getActiveAudioCodecs(), account->getActiveVideoCodecs());

    sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    if (!ac) {
        SFL_ERR("Could not instantiate codec");
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

    pjsip_tpselector tp_sel  = SipTransportBroker::getTransportSelector(transport->get());
    if (!dialog or pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        SFL_ERR("Could not set transport for dialog");
        return PJ_FALSE;
    }

    pjsip_inv_session* inv = nullptr;
    pjsip_inv_create_uas(dialog, rdata, call->getLocalSDP().getLocalSdpSession(), 0, &inv);

    if (!inv) {
        SFL_ERR("Call invite is not initialized");
        return PJ_FALSE;
    }

    inv->mod_data[mod_ua_.id] = call.get();
    call->inv.reset(inv);

    // Check whether Replaces header is present in the request and process accordingly.
    pjsip_dialog *replaced_dlg;
    pjsip_tx_data *response;

    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
        SFL_ERR("Something wrong with Replaces request.");
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
        if (pjsip_inv_answer(call->inv.get(), PJSIP_SC_OK, NULL, NULL, &response) == PJ_SUCCESS) {
            if (pjsip_inv_send_msg(call->inv.get(), response) != PJ_SUCCESS)
                call->inv.reset();
        }

        // Get the INVITE session associated with the replaced dialog.
        pjsip_inv_session *replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

        // Disconnect the "replaced" INVITE session.
        if (pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS && tdata) {
            if (pjsip_inv_send_msg(replaced_inv, tdata))
                call->inv.reset();
        }
    } else { // Proceed with normal call flow
        if (pjsip_inv_initial_answer(call->inv.get(), rdata, PJSIP_SC_TRYING, NULL, NULL, &tdata) != PJ_SUCCESS) {
            SFL_ERR("Could not answer invite");
            return PJ_FALSE;
        }

        if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
            SFL_ERR("Could not send msg for invite");
            call->inv.reset();
            return PJ_FALSE;
        }

        call->setConnectionState(Call::TRYING);

        if (pjsip_inv_answer(call->inv.get(), PJSIP_SC_RINGING, NULL, NULL, &tdata) != PJ_SUCCESS) {
            SFL_ERR("Could not answer invite");
            return PJ_FALSE;
        }

        // contactStr must stay in scope as long as tdata
        const pj_str_t contactStr(account->getContactHeader(transport->get()));
        sip_utils::addContactHeader(&contactStr, tdata);

        if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
            SFL_ERR("Could not send msg for invite");
            call->inv.reset();
            return PJ_FALSE;
        }

        call->setConnectionState(Call::RINGING);

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

pj_pool_t* SIPVoIPLink::getPool() const
{
    return pool_;
}

SIPVoIPLink::SIPVoIPLink()
{
    SFL_DBG("creating SIPVoIPLink instance");

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

    auto ns = ip_utils::getLocalNameservers();
    if (not ns.empty()) {
        std::vector<pj_str_t> dns_nameservers(ns.size());
        for (unsigned i=0, n=ns.size(); i<n; i++) {
            char hbuf[NI_MAXHOST];
            getnameinfo((sockaddr*)&ns[i], ns[i].getLength(), hbuf, sizeof(hbuf), nullptr, 0, NI_NUMERICHOST);
            SFL_DBG("Using SIP nameserver: %s", hbuf);
            pj_strdup2(pool_, &dns_nameservers[i], hbuf);
        }
        pj_dns_resolver* resv;
        TRY(pjsip_endpt_create_resolver(endpt_, &resv));
        TRY(pj_dns_resolver_set_ns(resv, ns.size(), dns_nameservers.data(), nullptr));
        TRY(pjsip_endpt_set_resolver(endpt_, resv));
    }

    sipTransport.reset(new SipTransportBroker(endpt_, *cp_, *pool_));

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

    SFL_DBG("pjsip version %s for %s initialized", pj_get_version(), PJ_OS_NAME);

    TRY(pjsip_replaces_init_module(endpt_));
#undef TRY

    // ready to handle events
    // Implementation note: we don't use std::bind(xxx, this) here
    // as handleEvents needs a valid instance to be called.
    Manager::instance().registerEventHandler((uintptr_t)this, []() { getSIPVoIPLink()->handleEvents(); });
}

SIPVoIPLink::~SIPVoIPLink()
{
    SFL_DBG("destroying SIPVoIPLink instance");

    const int MAX_TIMEOUT_ON_LEAVING = 5;

    for (int timeout = 0; pjsip_tsx_layer_get_tsx_count() and timeout < MAX_TIMEOUT_ON_LEAVING; timeout++)
        sleep(1);

    Manager::instance().unregisterEventHandler((uintptr_t)this);

    const pj_time_val tv = {0, 10};
    pjsip_endpt_handle_events(endpt_, &tv);

    if (!Manager::instance().callFactory.empty<SIPCall>())
        SFL_ERR("%d SIP calls remains!",
              Manager::instance().callFactory.callCount<SIPCall>());

    // destroy SIP transport before endpoint
    sipTransport.reset();

    pjsip_endpt_destroy(endpt_);

    pj_pool_release(pool_);
    pj_caching_pool_destroy(cp_);

    pj_shutdown();
}

std::shared_ptr<SIPAccountBase>
SIPVoIPLink::guessAccount(const std::string& userName,
                           const std::string& server,
                           const std::string& fromUri) const
{
    SFL_DBG("username = %s, server = %s, from = %s", userName.c_str(), server.c_str(), fromUri.c_str());
    // Try to find the account id from username and server name by full match

    auto result = std::static_pointer_cast<SIPAccountBase>(Manager::instance().getIP2IPAccount()); // default result
    MatchRank best = MatchRank::NONE;

#if HAVE_DHT
    // DHT accounts
    for (const auto& account : Manager::instance().getAllAccounts<RingAccount>()) {
        if (!account)
            continue;
        const MatchRank match(account->matches(userName, server));

        // return right away if this is a full match
        if (match == MatchRank::FULL) {
            return account;
        } else if (match > best) {
            best = match;
            result = account;
        }
    }
#endif

    // SIP accounts
    for (const auto& account : Manager::instance().getAllAccounts<SIPAccount>()) {
        if (!account)
            continue;
        const MatchRank match(account->matches(userName, server, endpt_, pool_));

        // return right away if this is a full match
        if (match == MatchRank::FULL) {
            return account;
        } else if (match > best) {
            best = match;
            result = account;
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
void
SIPVoIPLink::handleEvents()
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
        SFL_DBG("Registering thread");
        pj_thread_register(NULL, desc, &this_thread);
    }

    static const pj_time_val timeout = {0, 10};
    pj_status_t ret;

    if ((ret = pjsip_endpt_handle_events(endpt_, &timeout)) != PJ_SUCCESS)
        sip_utils::sip_strerror(ret);

#ifdef SFL_VIDEO
    dequeKeyframeRequests();
#endif
}

void SIPVoIPLink::registerKeepAliveTimer(pj_timer_entry &timer, pj_time_val &delay)
{
    SFL_DBG("Register new keep alive timer %d with delay %d", timer.id, delay.sec);

    if (timer.id == -1)
        SFL_WARN("Timer already scheduled");

    switch (pjsip_endpt_schedule_timer(endpt_, &timer, &delay)) {
        case PJ_SUCCESS:
            break;

        default:
            SFL_ERR("Could not schedule new timer in pjsip endpoint");

            /* fallthrough */
        case PJ_EINVAL:
            SFL_ERR("Invalid timer or delay entry");
            break;

        case PJ_EINVALIDOP:
            SFL_ERR("Invalid timer entry, maybe already scheduled");
            break;
    }
}

void SIPVoIPLink::cancelKeepAliveTimer(pj_timer_entry& timer)
{
    pjsip_endpt_cancel_timer(endpt_, &timer);
}

#ifdef SFL_VIDEO
// Called from a video thread
void
SIPVoIPLink::enqueueKeyframeRequest(const std::string &id)
{
    auto link = getSIPVoIPLink();
    std::lock_guard<std::mutex> lock(link->keyframeRequestsMutex_);
    link->keyframeRequests_.push(id);
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
        call = Manager::instance().callFactory.getCall<SIPCall>(callID); // fixme: need a try version

    if (!call)
        return;

    const char * const BODY =
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<media_control><vc_primitive><to_encoder>"
        "<picture_fast_update/>"
        "</to_encoder></vc_primitive></media_control>";

    SFL_DBG("Sending video keyframe request via SIP INFO");
    call->sendSIPInfo(BODY, "media_control+xml");
}
#endif

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

    if (inv->state == PJSIP_INV_STATE_EARLY and ev and ev->body.tsx_state.tsx and
            ev->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
        makeCallRing(*call);
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED and ev) {
        // After we sent or received a ACK - The connection is established
        call->onAnswered();
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        //std::string accId(call->getAccountId());

        switch (inv->cause) {
                // The call terminates normally - BYE / CANCEL
            case PJSIP_SC_OK:
            case PJSIP_SC_REQUEST_TERMINATED:
                call->onClosed();
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
                SFL_WARN("PJSIP_INV_STATE_DISCONNECTED: %d %d",
                         inv->cause, ev ? ev->type : -1);
                call->onServerFailure();
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

    const auto& account = call->getSIPAccount();
    auto& localSDP = call->getLocalSDP();

    localSDP.receiveOffer(offer, account.getActiveAudioCodecs(), account.getActiveVideoCodecs());
    localSDP.startNegotiation();

    pjsip_inv_set_sdp_answer(inv, localSDP.getLocalSdpSession());
}

static void
sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    if (!inv or !p_offer)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call)
        return;

    const auto& account = call->getSIPAccount();

    // FIXME : for now, use the same address family as the SIP transport
    auto family = pjsip_transport_type_get_af(account.getTransportType());
    IpAddr address = account.getPublishedSameasLocal()
                    ? IpAddr(ip_utils::getInterfaceAddr(account.getLocalInterface(), family))
                    : account.getPublishedIpAddress();

    call->setCallMediaLocal(address);

    auto& localSDP = call->getLocalSDP();
    localSDP.setPublishedIP(address);
    const bool created = localSDP.createOffer(account.getActiveAudioCodecs(), account.getActiveVideoCodecs());

    if (created)
        *p_offer = localSDP.getLocalSdpSession();
}

// This callback is called after SDP offer/answer session has completed.
static void
sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status)
{
    if (!inv)
        return;

    auto call = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call) {
        SFL_DBG("Call declined by peer, SDP negotiation stopped");
        return;
    }

    if (status != PJ_SUCCESS) {
        const int reason = inv->state != PJSIP_INV_STATE_NULL and
                           inv->state != PJSIP_INV_STATE_CONFIRMED ?
                           PJSIP_SC_UNSUPPORTED_MEDIA_TYPE : 0;

        SFL_WARN("Could not negotiate offer");
        const std::string callID(call->getCallId());
        call->hangup(reason);
        Manager::instance().callFailure(callID);
        return;
    }

    if (!inv->neg) {
        SFL_WARN("No negotiator for this session");
        return;
    }

    // Retreive SDP session for this call
    auto& sdpSession = call->getLocalSDP();

    // Get active session sessions
    const pjmedia_sdp_session *remoteSDP = 0;

    if (pjmedia_sdp_neg_get_active_remote(inv->neg, &remoteSDP) != PJ_SUCCESS) {
        SFL_ERR("Active remote not present");
        return;
    }

    if (pjmedia_sdp_validate(remoteSDP) != PJ_SUCCESS) {
        SFL_ERR("Invalid remote SDP session");
        return;
    }

    const pjmedia_sdp_session *local_sdp;
    pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    if (pjmedia_sdp_validate(local_sdp) != PJ_SUCCESS) {
        SFL_ERR("Invalid local SDP session");
        return;
    }

    // Print SDP session
    char buffer[4096];
    memset(buffer, 0, sizeof buffer);

    if (pjmedia_sdp_print(remoteSDP, buffer, sizeof buffer) == -1) {
        SFL_ERR("SDP was too big for buffer");
        return;
    }

    SFL_DBG("Remote active SDP Session:\n%s", buffer);

    memset(buffer, 0, sizeof buffer);

    if (pjmedia_sdp_print(local_sdp, buffer, sizeof buffer) == -1) {
        SFL_ERR("SDP was too big for buffer");
        return;
    }

    SFL_DBG("Local active SDP Session:\n%s", buffer);

    // Set active SDP sessions
    sdpSession.setActiveLocalSdpSession(local_sdp);
    sdpSession.setActiveRemoteSdpSession(remoteSDP);

    // Update internal field for
    sdpSession.setMediaTransportInfoFromRemoteSdp();

    try {
        call->getAudioRtp().updateDestinationIpAddress();
    } catch (const AudioRtpFactoryException &e) {
        SFL_ERR("%s", e.what());
    }

    call->getAudioRtp().setDtmfPayloadType(sdpSession.getTelephoneEventType());
#ifdef SFL_VIDEO
    call->getVideoRtp().updateSDP(sdpSession);
    call->getVideoRtp().updateDestination(call->getLocalSDP().getRemoteIP(), sdpSession.getRemoteVideoPort());
    auto localPort = sdpSession.getLocalVideoPort();
    if (!localPort)
        localPort = sdpSession.getRemoteVideoPort();
    call->getVideoRtp().start(localPort);
#endif

    // Get the crypto attribute containing srtp's cryptographic context (keys, cipher)
    CryptoOffer crypto_offer;
    call->getLocalSDP().getRemoteSdpCryptoFromOffer(remoteSDP, crypto_offer);

#if HAVE_SDES
    bool nego_success = false;

    if (!crypto_offer.empty()) {
        std::vector<sfl::CryptoSuiteDefinition> localCapabilities;

        for (size_t i = 0; i < SFL_ARRAYSIZE(sfl::CryptoSuites); ++i)
            localCapabilities.push_back(sfl::CryptoSuites[i]);

        sfl::SdesNegotiator sdesnego(localCapabilities, crypto_offer);

        if (sdesnego.negotiate()) {
            nego_success = true;

            try {
                call->getAudioRtp().setRemoteCryptoInfo(sdesnego);
                Manager::instance().getClient()->getCallManager()->secureSdesOn(call->getCallId());
            } catch (const AudioRtpFactoryException &e) {
                SFL_ERR("%s", e.what());
                Manager::instance().getClient()->getCallManager()->secureSdesOff(call->getCallId());
            }
        } else {
            SFL_ERR("SDES negotiation failure");
            Manager::instance().getClient()->getCallManager()->secureSdesOff(call->getCallId());
        }
    } else {
        SFL_DBG("No crypto offer available");
    }

    // We did not find any crypto context for this media, RTP fallback
    if (!nego_success && call->getAudioRtp().isSdesEnabled()) {
        SFL_ERR("Negotiation failed but SRTP is enabled, fallback on RTP");
        call->getAudioRtp().stop();
        call->getAudioRtp().setSrtpEnabled(false);

        const auto& account = call->getSIPAccount();
        if (account.getSrtpFallback()) {
            call->getAudioRtp().initSession();

            if (account.isStunEnabled())
                call->updateSDPFromSTUN();
        }
    }

#endif // HAVE_SDES

    std::vector<sfl::AudioCodec*> sessionMedia(sdpSession.getSessionAudioMedia());

    if (sessionMedia.empty()) {
        SFL_WARN("Session media is empty");
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
                SFL_ERR("Could not instantiate codec %d", pl);
            } else {
                audioCodecs.push_back(ac);
            }
        }

        if (not audioCodecs.empty())
            call->getAudioRtp().updateSessionMedia(audioCodecs);
    } catch (const SdpException &e) {
        SFL_ERR("%s", e.what());
    } catch (const std::exception &rtpException) {
        SFL_ERR("%s", rtpException.what());
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
            SFL_DBG("handling picture fast update request");
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
            SFL_DBG("%s", request.c_str());

            if (request.find("NOTIFY") == std::string::npos and
                    request.find("INFO") != std::string::npos) {
                sendOK(inv->dlg, r_data, tsx);
                return;
            }

            pjsip_msg_body *body(r_data->msg_info.msg->body);

            if (body and body->len > 0) {
                const std::string msg(static_cast<char *>(body->data), body->len);
                SFL_DBG("%s", msg.c_str());

                if (msg.find("Not found") != std::string::npos) {
                    SFL_ERR("Received 404 Not found");
                    sendOK(inv->dlg, r_data, tsx);
                    return;
                } else if (msg.find("Ringing") != std::string::npos and call) {
                    if (call)
                        makeCallRing(*call);
                    else
                        SFL_WARN("Ringing state on non existing call");
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
        SFL_ERR("%s", except.what());
    }
#endif
}

static void
onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    auto currentCall = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!currentCall)
        return;

    static const pj_str_t str_refer_to = CONST_PJ_STR("Refer-To");
    pjsip_generic_string_hdr *refer_to = static_cast<pjsip_generic_string_hdr*>
                                         (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL));

    if (!refer_to) {
        pjsip_dlg_respond(inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    try {
        Manager::instance().newOutgoingCall(Manager::instance().getNewCallID(),
                                            std::string(refer_to->hvalue.ptr,
                                                        refer_to->hvalue.slen),
                                            currentCall->getAccountId());
        Manager::instance().hangupCall(currentCall->getCallId());
    } catch (const VoipLinkException &e) {
        SFL_ERR("%s", e.what());
    }
}

int SIPVoIPLink::getModId()
{
    return mod_ua_.id;
}

void SIPVoIPLink::createSDPOffer(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{ sdp_create_offer_cb(inv, p_offer); }

void
SIPVoIPLink::resolveSrvName(const std::string &name, pjsip_transport_type_e type, SrvResolveCallback cb)
{
    if (name.length() >= PJ_MAX_HOSTNAME) {
        SFL_ERR("Hostname is too long");
        cb({});
        return;
    }

    pjsip_host_info host_info {
        0, type, {{(char*)name.data(), (pj_ssize_t)name.size()}, 0},
    };

    auto token = std::hash<std::string>()(name + std::to_string(type));
    {
        std::lock_guard<std::mutex> lock(resolveMutex_);
        resolveCallbacks_[token] = [cb](pj_status_t s, const pjsip_server_addresses* r) {
            try {
                if (s != PJ_SUCCESS || !r) {
                    sip_utils::sip_strerror(s);
                    throw std::runtime_error("Can't resolve address");
                } else {
                    std::vector<IpAddr> ips;
                    ips.reserve(r->count);
                    for (unsigned i=0; i < r->count; i++)
                        ips.push_back(r->entry[i].addr);
                    cb(ips);
                }
            } catch (const std::exception& e) {
                SFL_ERR("Error resolving address: %s", e.what());
                cb({});
            }
        };
    }

    pjsip_endpt_resolve(endpt_, pool_, &host_info, (void*)token, resolver_callback);
}

void
SIPVoIPLink::resolver_callback(pj_status_t status, void *token, const struct pjsip_server_addresses *addr)
{
    auto sthis_ = getSIPVoIPLink();
    auto& this_ = *sthis_;
    {
        std::lock_guard<std::mutex> lock(this_.resolveMutex_);
        auto it = this_.resolveCallbacks_.find((uintptr_t)token);
        if (it != this_.resolveCallbacks_.end()) {
            it->second(status, addr);
            this_.resolveCallbacks_.erase(it);
        }
    }
}
