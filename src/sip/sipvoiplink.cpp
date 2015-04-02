/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#if HAVE_DHT
#include "ringdht/ringaccount.h"
#endif

#include "manager.h"
#if HAVE_SDES
#include "sdes_negotiator.h"
#endif

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

#include "system_codec_container.h"
#include "audio/audio_rtp_session.h"

#ifdef RING_VIDEO
#include "video/video_rtp_session.h"
#include "client/videomanager.h"
#endif

#include "client/ring_signal.h"

#include "pres_sub_server.h"

#include "array_size.h"
#include "ip_utils.h"
#include "sip_utils.h"
#include "string_utils.h"
#include "logger.h"

#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_uri.h>

#include <pjsip-simple/presence.h>
#include <pjsip-simple/publish.h>

#include <istream>
#include <algorithm>

namespace ring {

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

decltype(getGlobalInstance<SIPVoIPLink>)& getSIPVoIPLink = getGlobalInstance<SIPVoIPLink, 1>;

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
        RING_ERR("Transaction has been created for this request, send response "
              "statefully instead");

    return !PJ_SUCCESS;
}

static pj_bool_t
transaction_request_cb(pjsip_rx_data *rdata)
{
    if (!rdata or !rdata->msg_info.msg) {
        RING_ERR("rx_data is NULL");
        return PJ_FALSE;
    }

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

    if (!method) {
        RING_ERR("method is NULL");
        return PJ_FALSE;
    }

    if (method->id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg(rdata))
        return PJ_FALSE;

    if (!rdata->msg_info.to or !rdata->msg_info.from or !rdata->msg_info.via) {
        RING_ERR("Missing From, To or Via fields");
        return PJ_FALSE;
    }
    const pjsip_sip_uri *sip_to_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.to->uri);
    const pjsip_sip_uri *sip_from_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.from->uri);
    const pjsip_host_port& sip_via = rdata->msg_info.via->sent_by;

    if (!sip_to_uri or !sip_from_uri or !sip_via.host.ptr) {
        RING_ERR("NULL uri");
        return PJ_FALSE;
    }
    std::string toUsername(sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string toHost(sip_to_uri->host.ptr, sip_to_uri->host.slen);
    std::string viaHostname(sip_via.host.ptr, sip_via.host.slen);
    const std::string remote_user(sip_from_uri->user.ptr, sip_from_uri->user.slen);
    const std::string remote_hostname(sip_from_uri->host.ptr, sip_from_uri->host.slen);

    auto link = getSIPVoIPLink();
    if (not link) {
        RING_ERR("no more VoIP link");
        return PJ_FALSE;
    }

    auto account(link->guessAccount(toUsername, viaHostname, remote_hostname));
    if (!account) {
        RING_ERR("NULL account");
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

    if (account->getActiveAccountCodecInfoIdList(MEDIA_AUDIO).empty()) {
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

    auto call = account->newIncomingCall(remote_user);
    if (!call) {
        return PJ_FALSE;
    }

    char tmp[PJSIP_MAX_URL_SIZE];
    size_t length = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, sip_from_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber(tmp, length);
    sip_utils::stripSipUriPrefix(peerNumber);

    if (not remote_user.empty() and not remote_hostname.empty())
        peerNumber = remote_user + "@" + remote_hostname;

    // RING_DBG("transaction_request_cb viaHostname %s toUsername %s addrToUse %s addrSdp %s peerNumber: %s" ,
    // viaHostname.c_str(), toUsername.c_str(), addrToUse.toString().c_str(), addrSdp.toString().c_str(), peerNumber.c_str());

    // Append PJSIP transport to the broker's SipTransport list
    auto transport = link->sipTransportBroker->addTransport(rdata->tp_info.transport);
    if (!transport) {
        if (account->getAccountType() == SIPAccount::ACCOUNT_TYPE) {
            RING_WARN("Using transport from account.");
            transport = std::static_pointer_cast<SIPAccount>(account)->getTransport();
        }
        if (!transport) {
            RING_ERR("No suitable transport to answer this call.");
            return PJ_FALSE;
        }
    }
    call->setTransport(transport);

    // FIXME : for now, use the same address family as the SIP transport
    auto family = pjsip_transport_type_get_af(pjsip_transport_get_type_from_flag(transport->get()->flag));
    IpAddr addrToUse = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);

    IpAddr addrSdp;
    if (account->getUPnPActive()) {
        /* use UPnP addr, or published addr if its set */
        addrSdp = account->getPublishedSameasLocal() ?
            account->getUPnPIpAddress() : account->getPublishedIpAddress();
    } else {
        addrSdp = account->isStunEnabled() or (not account->getPublishedSameasLocal())
                    ? account->getPublishedIpAddress() : addrToUse;
    }

    /* fallback on local address */
    if (not addrSdp) addrSdp = addrToUse;

    call->setConnectionState(Call::PROGRESSING);
    call->setPeerNumber(peerNumber);
    call->setDisplayName(displayName);
    call->initRecFilename(peerNumber);
    call->setCallMediaLocal(addrToUse);
    call->getSDP().setPublishedIP(addrSdp);

    if (account->isStunEnabled())
        call->updateSDPFromSTUN();

    call->getSDP().receiveOffer(r_sdp,
        account->getActiveAccountCodecInfoList(MEDIA_AUDIO),
        account->getActiveAccountCodecInfoList(MEDIA_VIDEO),
        account->getSrtpKeyExchange()
    );
    auto ice_attrs = Sdp::getIceAttributes(r_sdp);
    if (not ice_attrs.ufrag.empty() and not ice_attrs.pwd.empty()) {
        if (not call->getIceTransport()) {
            RING_DBG("Initializing ICE transport");
            call->initIceTransport(false);
        }
        call->setupLocalSDPFromIce();
    }

    pjsip_dialog *dialog = nullptr;
    if (pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, nullptr, &dialog) != PJ_SUCCESS) {
        RING_ERR("Could not create uas");
        call.reset();
        try_respond_stateless(endpt_, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, nullptr, nullptr, nullptr);
        return PJ_FALSE;
    }

    pjsip_tpselector tp_sel  = SIPVoIPLink::getTransportSelector(transport->get());
    if (!dialog or pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        RING_ERR("Could not set transport for dialog");
        return PJ_FALSE;
    }

    pjsip_inv_session* inv = nullptr;
    pjsip_inv_create_uas(dialog, rdata, call->getSDP().getLocalSdpSession(), PJSIP_INV_SUPPORT_ICE, &inv);

    if (!inv) {
        RING_ERR("Call invite is not initialized");
        return PJ_FALSE;
    }

    inv->mod_data[mod_ua_.id] = call.get();
    call->inv.reset(inv);

    // Check whether Replaces header is present in the request and process accordingly.
    pjsip_dialog *replaced_dlg;
    pjsip_tx_data *response;

    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
        RING_ERR("Something wrong with Replaces request.");
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
            RING_ERR("Could not answer invite");
            return PJ_FALSE;
        }

        if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
            RING_ERR("Could not send msg for invite");
            call->inv.reset();
            return PJ_FALSE;
        }

        call->setConnectionState(Call::TRYING);

        if (pjsip_inv_answer(call->inv.get(), PJSIP_SC_RINGING, NULL, NULL, &tdata) != PJ_SUCCESS) {
            RING_ERR("Could not answer invite");
            return PJ_FALSE;
        }

        // contactStr must stay in scope as long as tdata
        const pj_str_t contactStr(account->getContactHeader(transport->get()));
        sip_utils::addContactHeader(&contactStr, tdata);

        if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS) {
            RING_ERR("Could not send msg for invite");
            call->inv.reset();
            return PJ_FALSE;
        }

        call->setConnectionState(Call::RINGING);

        Manager::instance().incomingCall(*call, account_id);
    }

    return PJ_FALSE;
}

static void
tp_state_callback(pjsip_transport* tp, pjsip_transport_state state,
                  const pjsip_transport_state_info* info)
{
    // There is no way (at writing) to link a user data to a PJSIP transport.
    // So we obtain it from the global SIPVoIPLink instance that owns it.
    // Be sure the broker's owner is not deleted during proccess
    if (auto sipLink = getSIPVoIPLink()) {
        if (auto& broker = sipLink->sipTransportBroker)
            broker->transportStateChanged(tp, state, info);
        else
            RING_ERR("SIPVoIPLink with invalid SipTransportBroker");
    } else
        RING_ERR("no more VoIP link");
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
#define TRY(ret) do { \
    if (ret != PJ_SUCCESS) \
    throw VoipLinkException(#ret " failed"); \
} while (0)

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
            RING_DBG("Using SIP nameserver: %s", hbuf);
            pj_strdup2(pool_, &dns_nameservers[i], hbuf);
        }
        pj_dns_resolver* resv;
        TRY(pjsip_endpt_create_resolver(endpt_, &resv));
        TRY(pj_dns_resolver_set_ns(resv, ns.size(), dns_nameservers.data(), nullptr));
        TRY(pjsip_endpt_set_resolver(endpt_, resv));
    }

    sipTransportBroker.reset(new SipTransportBroker(endpt_, *cp_, *pool_));

    auto status = pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_),
                                           tp_state_callback);
    if (status != PJ_SUCCESS) {
        RING_ERR("Can't set transport callback");
        sip_utils::sip_strerror(status);
    }

    if (!ip_utils::getLocalAddr())
        throw VoipLinkException("UserAgent: Unable to determine network capabilities");

    TRY(pjsip_tsx_layer_init_module(endpt_));
    TRY(pjsip_ua_init_module(endpt_, nullptr));
    TRY(pjsip_replaces_init_module(endpt_)); // See the Replaces specification in RFC 3891
    TRY(pjsip_100rel_init_module(endpt_));

    // Initialize and register ring module
    mod_ua_.name = pj_str((char*) PACKAGE);
    mod_ua_.id = -1;
    mod_ua_.priority = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_ua_.on_rx_request = &transaction_request_cb;
    mod_ua_.on_rx_response = &transaction_response_cb;
    TRY(pjsip_endpt_register_module(endpt_, &mod_ua_));

    TRY(pjsip_evsub_init_module(endpt_));
    TRY(pjsip_xfer_init_module(endpt_));

    // presence/publish management
    TRY(pjsip_pres_init_module(endpt_, pjsip_evsub_instance()));
    TRY(pjsip_endpt_register_module(endpt_, &PresSubServer::mod_presence_server));

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

    TRY(pjsip_replaces_init_module(endpt_));
#undef TRY

    // ready to handle events
    // Implementation note: we don't use std::bind(xxx, this) here
    // as handleEvents needs a valid instance to be called.
    Manager::instance().registerEventHandler((uintptr_t)this,
                                             [this]{ handleEvents(); });

    RING_DBG("SIPVoIPLink@%p", this);
}

SIPVoIPLink::~SIPVoIPLink()
{
    RING_DBG("~SIPVoIPLink@%p", this);

    // Remaining calls should not happen as possible upper callbacks
    // may be called and another instance of SIPVoIPLink can be re-created!

    if (not Manager::instance().callFactory.empty<SIPCall>())
        RING_ERR("%d SIP calls remains!",
                 Manager::instance().callFactory.callCount<SIPCall>());

    sipTransportBroker->shutdown();

    const int MAX_TIMEOUT_ON_LEAVING = 5;
    for (int timeout = 0;
         pjsip_tsx_layer_get_tsx_count() and timeout < MAX_TIMEOUT_ON_LEAVING;
         timeout++)
        sleep(1);

    pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), nullptr);
    Manager::instance().unregisterEventHandler((uintptr_t)this);
    handleEvents();

    sipTransportBroker.reset();

    pjsip_endpt_destroy(endpt_);
    pj_pool_release(pool_);
    pj_caching_pool_destroy(cp_);

    RING_DBG("destroying SIPVoIPLink@%p", this);
}

std::shared_ptr<SIPAccountBase>
SIPVoIPLink::guessAccount(const std::string& userName,
                           const std::string& server,
                           const std::string& fromUri) const
{
    RING_DBG("username = %s, server = %s, from = %s", userName.c_str(), server.c_str(), fromUri.c_str());
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
        RING_DBG("Registering thread");
        pj_thread_register(NULL, desc, &this_thread);
    }

    static const pj_time_val timeout = {0, 0}; // polling
    auto ret = pjsip_endpt_handle_events(endpt_, &timeout);
    if (ret != PJ_SUCCESS)
        sip_utils::sip_strerror(ret);

#ifdef RING_VIDEO
    dequeKeyframeRequests();
#endif
}

void SIPVoIPLink::registerKeepAliveTimer(pj_timer_entry &timer, pj_time_val &delay)
{
    RING_DBG("Register new keep alive timer %d with delay %d", timer.id, delay.sec);

    if (timer.id == -1)
        RING_WARN("Timer already scheduled");

    switch (pjsip_endpt_schedule_timer(endpt_, &timer, &delay)) {
        case PJ_SUCCESS:
            break;

        default:
            RING_ERR("Could not schedule new timer in pjsip endpoint");

            /* fallthrough */
        case PJ_EINVAL:
            RING_ERR("Invalid timer or delay entry");
            break;

        case PJ_EINVALIDOP:
            RING_ERR("Invalid timer entry, maybe already scheduled");
            break;
    }
}

void SIPVoIPLink::cancelKeepAliveTimer(pj_timer_entry& timer)
{
    pjsip_endpt_cancel_timer(endpt_, &timer);
}

#ifdef RING_VIDEO
// Called from a video thread
void
SIPVoIPLink::enqueueKeyframeRequest(const std::string &id)
{
    if (auto link = getSIPVoIPLink()) {
        std::lock_guard<std::mutex> lock(link->keyframeRequestsMutex_);
        link->keyframeRequests_.push(id);
    } else
        RING_ERR("no more VoIP link");
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

    RING_DBG("Sending video keyframe request via SIP INFO");
    call->sendSIPInfo(BODY, "media_control+xml");
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

static void
invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *ev)
{
    if (!inv)
        return;

    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr) {
        RING_WARN("invite_session_state_changed_cb: can't find related call");
        return;
    }
    auto call = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());

    if (ev and inv->state != PJSIP_INV_STATE_CONFIRMED) {
        const auto tsx = ev->body.tsx_state.tsx;
        if (auto status_code = tsx ? tsx->status_code : 404) {
            const pj_str_t* description = pjsip_get_status_text(status_code);
            RING_DBG("SIP invite session state change: %d %.*s", status_code, description->slen, description->ptr);
        }
    }

    if (inv->state == PJSIP_INV_STATE_EARLY and ev and ev->body.tsx_state.tsx and
            ev->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
        call->onPeerRinging();
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED and ev) {
        // After we sent or received a ACK - The connection is established
        call->onAnswered();
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
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
                RING_WARN("PJSIP_INV_STATE_DISCONNECTED: %d %d",
                         inv->cause, ev ? ev->type : -1);
                call->onServerFailure(inv->cause);
                break;
        }
    }
}

static void
sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
    if (!inv)
        return;

    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr)
        return;
    auto call = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());
    call->onReceiveOffer(offer);
}

static void
sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    if (!inv or !p_offer)
        return;

    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr)
        return;
    auto call = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());

    const auto& account = call->getSIPAccount();
    auto family = pj_AF_INET();
    // FIXME : for now, use the same address family as the SIP transport
    if (auto dlg = inv->dlg) {
        if (dlg->tp_sel.type == PJSIP_TPSELECTOR_TRANSPORT) {
            if (auto tr = dlg->tp_sel.u.transport)
                family = tr->local_addr.addr.sa_family;
        } else if (dlg->tp_sel.type == PJSIP_TPSELECTOR_TRANSPORT) {
            if (auto tr = dlg->tp_sel.u.listener)
                family = tr->local_addr.addr.sa_family;
        }
    }
    auto ifaceAddr = ip_utils::getInterfaceAddr(account.getLocalInterface(), family);

    IpAddr address;
    if (account.getUPnPActive()) {
        /* use UPnP addr, or published addr if its set */
        address = account.getPublishedSameasLocal() ?
            account.getUPnPIpAddress() : account.getPublishedIpAddress();
    } else {
        address = account.getPublishedSameasLocal() ?
            ifaceAddr : account.getPublishedIpAddress();
    }

    /* fallback on local address */
    if (not address) address = ifaceAddr;

    call->setCallMediaLocal(address);

    auto& localSDP = call->getSDP();
    localSDP.setPublishedIP(address);
    const bool created = localSDP.createOffer(
        account.getActiveAccountCodecInfoList(MEDIA_AUDIO),
        account.getActiveAccountCodecInfoList(account.isVideoEnabled() ? MEDIA_VIDEO : MEDIA_NONE),
        account.getSrtpKeyExchange()
    );

    if (created)
        *p_offer = localSDP.getLocalSdpSession();
}

static void
dump_sdp_session(const pjmedia_sdp_session* sdp_session, const char* header)
{
    char buffer[4096] {};

    if (pjmedia_sdp_print(sdp_session, buffer, sizeof buffer) == -1) {
        RING_ERR("%sSDP too big for dump", header);
        return;
    }

    RING_DBG("%s%s", header, buffer);
}

static const pjmedia_sdp_session*
get_active_remote_sdp(pjsip_inv_session *inv)
{
    const pjmedia_sdp_session* sdp_session {};

    if (pjmedia_sdp_neg_get_active_remote(inv->neg, &sdp_session) != PJ_SUCCESS) {
        RING_ERR("Active remote not present");
        return nullptr;
    }

    if (pjmedia_sdp_validate(sdp_session) != PJ_SUCCESS) {
        RING_ERR("Invalid remote SDP session");
        return nullptr;
    }

    dump_sdp_session(sdp_session, "Remote active SDP Session:\n");
    return sdp_session;
}

static const pjmedia_sdp_session*
get_active_local_sdp(pjsip_inv_session *inv)
{
    const pjmedia_sdp_session* sdp_session {};

    if (pjmedia_sdp_neg_get_active_local(inv->neg, &sdp_session) != PJ_SUCCESS) {
        RING_ERR("Active local not present");
        return nullptr;
    }

    if (pjmedia_sdp_validate(sdp_session) != PJ_SUCCESS) {
        RING_ERR("Invalid local SDP session");
        return nullptr;
    }

    dump_sdp_session(sdp_session, "Local active SDP Session:\n");
    return sdp_session;
}

// This callback is called after SDP offer/answer session has completed.
static void
sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status)
{
    if (!inv)
        return;

    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr) {
        RING_DBG("Call declined by peer, SDP negotiation stopped");
        return;
    }
    auto call = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());

    if (status != PJ_SUCCESS) {
        const int reason = inv->state != PJSIP_INV_STATE_NULL and
                           inv->state != PJSIP_INV_STATE_CONFIRMED ?
                           PJSIP_SC_UNSUPPORTED_MEDIA_TYPE : 0;

        RING_WARN("Could not negotiate offer");
        call->hangup(reason);
        Manager::instance().callFailure(*call);
        return;
    }

    if (!inv->neg) {
        RING_WARN("No negotiator for this session");
        return;
    }

    const auto localSDP = get_active_local_sdp(inv);
    const auto remoteSDP = get_active_remote_sdp(inv);

    // Update our sdp manager
    auto& sdp = call->getSDP();

    // Set active SDP sessions
    sdp.setActiveLocalSdpSession(localSDP);
    sdp.setActiveRemoteSdpSession(remoteSDP);

    call->onMediaUpdate();
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
#ifdef RING_VIDEO
            RING_DBG("handling picture fast update request");
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

    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr)
        return;
    auto call = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());

    if (event->body.rx_msg.rdata) {
        pjsip_rx_data *r_data = event->body.rx_msg.rdata;

        if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {
            std::string request(pjsip_rx_data_get_info(r_data));
            RING_DBG("%s", request.c_str());

            if (request.find("NOTIFY") == std::string::npos and
                    request.find("INFO") != std::string::npos) {
                sendOK(inv->dlg, r_data, tsx);
                return;
            }

            pjsip_msg_body *body(r_data->msg_info.msg->body);

            if (body and body->len > 0) {
                const std::string msg(static_cast<char *>(body->data), body->len);
                RING_DBG("%s", msg.c_str());

                if (msg.find("Not found") != std::string::npos) {
                    RING_ERR("Received 404 Not found");
                    sendOK(inv->dlg, r_data, tsx);
                    return;
                } else if (msg.find("Ringing") != std::string::npos and call) {
                    if (call)
                        call->onPeerRinging();
                    else
                        RING_WARN("Ringing state on non existing call");
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

    try {
        // retreive the recipient-list of this message
        std::string urilist = InstantMessaging::findTextUriList(formattedMessage);
        auto list = InstantMessaging::parseXmlUriList(urilist);

        // If no item present in the list, peer is considered as the sender
        std::string from;

        if (list.empty()) {
            from = call->getPeerNumber();
        } else {
            from = list.front()[InstantMessaging::IM_XML_URI];

            if (from == "Me")
                from = call->getPeerNumber();
        }

        // strip < and > characters in case of an IP address
        if (from[0] == '<' && from[from.size() - 1] == '>')
            from = from.substr(1, from.size() - 2);

        Manager::instance().incomingMessage(call->getCallId(), from,
                                            InstantMessaging::findTextMessage(formattedMessage));

        // Respond with a 200/OK
        sendOK(inv->dlg, r_data, tsx);

    } catch (const InstantMessaging::InstantMessageException &except) {
        RING_ERR("%s", except.what());
    }
#endif
}

static void
onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    if (!call_ptr)
        return;
    auto currentCall = std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());

    static const pj_str_t str_refer_to = CONST_PJ_STR("Refer-To");
    pjsip_generic_string_hdr *refer_to = static_cast<pjsip_generic_string_hdr*>
                                         (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL));

    if (!refer_to) {
        pjsip_dlg_respond(inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    try {
        Manager::instance().newOutgoingCall(std::string(refer_to->hvalue.ptr,
                                                        refer_to->hvalue.slen),
                                            currentCall->getAccountId());
        Manager::instance().hangupCall(currentCall->getCallId());
    } catch (const VoipLinkException &e) {
        RING_ERR("%s", e.what());
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
        RING_ERR("Hostname is too long");
        cb({});
        return;
    }

    pjsip_host_info host_info {
        0, type, {{(char*)name.data(), (pj_ssize_t)name.size()}, 0},
    };

    auto token = std::hash<std::string>()(name + to_string(type));
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
                RING_ERR("Error resolving address: %s", e.what());
                cb({});
            }
        };
    }

    pjsip_endpt_resolve(endpt_, pool_, &host_info, (void*)token, resolver_callback);
}

void
SIPVoIPLink::resolver_callback(pj_status_t status, void *token, const struct pjsip_server_addresses *addr)
{
    if (auto link = getSIPVoIPLink()) {
        std::lock_guard<std::mutex> lock(link->resolveMutex_);
        auto it = link->resolveCallbacks_.find((uintptr_t)token);
        if (it != link->resolveCallbacks_.end()) {
            it->second(status, addr);
            link->resolveCallbacks_.erase(it);
        }
    } else
        RING_ERR("no more VoIP link");
}

#define RETURN_IF_NULL(A, M, ...) \
    if ((A) == NULL) { RING_WARN(M, ##__VA_ARGS__); return; }

void
SIPVoIPLink::findLocalAddressFromTransport(pjsip_transport* transport,
                                           pjsip_transport_type_e transportType,
                                           const std::string& host,
                                           std::string& addr,
                                           pj_uint16_t& port) const
{
    // Initialize the sip port with the default SIP port
    port = pjsip_transport_get_default_port_for_type(transportType);

    // Initialize the sip address with the hostname
    const auto pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport,
                   "Transport is NULL in findLocalAddress, using local address %s :%d",
                   addr.c_str(), port);

    // get the transport manager associated with the SIP enpoint
    auto tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    RETURN_IF_NULL(tpmgr,
                   "Transport manager is NULL in findLocalAddress, using local address %s :%d",
                   addr.c_str(), port);

    pj_str_t pjstring;
    pj_cstr(&pjstring, host.c_str());

    auto tp_sel = getTransportSelector(transport);
    pjsip_tpmgr_fla2_param param = { transportType, &tp_sel, pjstring, PJ_FALSE,
                                     {nullptr, 0}, 0, nullptr };
    if (pjsip_tpmgr_find_local_addr2(tpmgr, pool_, &param) != PJ_SUCCESS) {
        RING_WARN("Could not retrieve local address and port from transport, using %s :%d",
                  addr.c_str(), port);
        return;
    }

    // Update local address based on the transport type
    addr = std::string(param.ret_addr.ptr, param.ret_addr.slen);

    // Determine the local port based on transport information
    port = param.ret_port;
}

void
SIPVoIPLink::findLocalAddressFromSTUN(pjsip_transport* transport,
                                      pj_str_t* stunServerName,
                                      int stunPort,
                                      std::string& addr,
                                      pj_uint16_t& port) const
{
    // Initialize the sip port with the default SIP port
    port = sip_utils::DEFAULT_SIP_PORT;

    // Initialize the sip address with the hostname
    const pj_str_t* pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    RETURN_IF_NULL(transport,
                   "Transport is NULL in findLocalAddress, using local address %s:%d",
                   addr.c_str(), port);

    IpAddr mapped_addr;
    pj_sock_t sipSocket = pjsip_udp_transport_get_socket(transport);
    const pjstun_setting stunOpt = {PJ_TRUE, *stunServerName, stunPort,
                                    *stunServerName, stunPort};
    const pj_status_t stunStatus = pjstun_get_mapped_addr2(&cp_->factory,
                                                           &stunOpt, 1,
                                                           &sipSocket,
                                                           &static_cast<pj_sockaddr_in&>(mapped_addr));

    switch (stunStatus) {
        case PJLIB_UTIL_ESTUNNOTRESPOND:
           RING_ERR("No response from STUN server %.*s",
                    stunServerName->slen, stunServerName->ptr);
           return;
        case PJLIB_UTIL_ESTUNSYMMETRIC:
           RING_ERR("Different mapped addresses are returned by servers.");
           return;
        case PJ_SUCCESS:
            port = mapped_addr.getPort();
            addr = mapped_addr.toString();
        default:
           break;
    }

    RING_WARN("Using address %s provided by STUN server %.*s",
              IpAddr(mapped_addr).toString(true).c_str(), stunServerName->slen,
              stunServerName->ptr);
}
#undef RETURN_IF_NULL
} // namespace ring
