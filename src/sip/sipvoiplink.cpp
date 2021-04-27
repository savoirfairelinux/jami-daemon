/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
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
 */

#include "sipvoiplink.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sdp.h"
#include "sipcall.h"
#include "sipaccount.h"

#include "jamidht/jamiaccount.h"

#include "manager.h"

#include "im/instant_messaging.h"
#include "system_codec_container.h"
#include "audio/audio_rtp_session.h"

#ifdef ENABLE_VIDEO
#include "video/video_rtp_session.h"
#include "client/videomanager.h"
#endif

#include "pres_sub_server.h"

#include "array_size.h"
#include "ip_utils.h"
#include "sip_utils.h"
#include "string_utils.h"
#include "logger.h"

#include <opendht/thread_pool.h>

#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_uri.h>

#include <pjsip-simple/presence.h>
#include <pjsip-simple/publish.h>

// Only PJSIP 2.10 is supported.
#if PJ_VERSION_NUM < (2 << 24 | 10 << 16)
#error "Unsupported PJSIP version (requires version 2.10+)"
#endif

#include <istream>
#include <algorithm>
#include <regex>

namespace jami {

using sip_utils::CONST_PJ_STR;

static constexpr std::chrono::milliseconds SIP_EVENTS_POLLING_PERIOD {300};
static constexpr pj_time_val SIP_EVENTS_POLLING_TIMEOUT {0, 0};

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

static pjsip_endpoint* endpt_;
static pjsip_module mod_ua_;

static void sdp_media_update_cb(pjsip_inv_session* inv, pj_status_t status);
static void sdp_create_offer_cb(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer);
static pj_status_t reinvite_received_cb(pjsip_inv_session* inv,
                                        const pjmedia_sdp_session* offer,
                                        pjsip_rx_data* rdata);
static void invite_session_state_changed_cb(pjsip_inv_session* inv, pjsip_event* e);
static void outgoing_request_forked_cb(pjsip_inv_session* inv, pjsip_event* e);
static void transaction_state_changed_cb(pjsip_inv_session* inv,
                                         pjsip_transaction* tsx,
                                         pjsip_event* e);

#ifdef DEBUG_SIP_REQUEST_MSG
static void processInviteResponseHelper(pjsip_inv_session* inv, pjsip_event* e);
#endif

static std::shared_ptr<SipEventsHandler>
getSipEventsHandler(pjsip_inv_session* inv)
{
    return Manager::instance().sipVoIPLink().getEventsHandler(inv);
}

static void
handleIncomingOptions(pjsip_rx_data* rdata)
{
    pjsip_tx_data* tdata;

    if (pjsip_endpt_create_response(endpt_, rdata, PJSIP_SC_OK, NULL, &tdata) != PJ_SUCCESS)
        return;

#define ADD_HDR(hdr) \
    do { \
        const pjsip_hdr* cap_hdr = hdr; \
        if (cap_hdr) \
            pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr)); \
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

// return PJ_FALSE so that eventually other modules will handle these requests
// TODO: move Voicemail to separate module
static pj_bool_t
transaction_response_cb(pjsip_rx_data* rdata)
{
    pjsip_dialog* dlg = pjsip_rdata_get_dlg(rdata);

    if (!dlg)
        return PJ_FALSE;

    pjsip_transaction* tsx = pjsip_rdata_get_tsx(rdata);

    if (!tsx or tsx->method.id != PJSIP_INVITE_METHOD)
        return PJ_FALSE;

    if (tsx->status_code / 100 == 2) {
        /**
         * Send an ACK message inside a transaction. PJSIP send automatically, non-2xx ACK response.
         * ACK for a 2xx response must be send using this method.
         */
        pjsip_tx_data* tdata;

        if (rdata->msg_info.cseq) {
            pjsip_dlg_create_request(dlg, &pjsip_ack_method, rdata->msg_info.cseq->cseq, &tdata);
            pjsip_dlg_send_request(dlg, tdata, -1, NULL);
        }
    }

    return PJ_FALSE;
}

pj_status_t
try_respond_stateless(pjsip_endpoint* endpt,
                      pjsip_rx_data* rdata,
                      int st_code,
                      const pj_str_t* st_text,
                      const pjsip_hdr* hdr_list,
                      const pjsip_msg_body* body)
{
    /* Check that no UAS transaction has been created for this request.
     * If UAS transaction has been created for this request, application
     * MUST send the response statefully using that transaction.
     */
    if (!pjsip_rdata_get_tsx(rdata))
        return pjsip_endpt_respond_stateless(endpt, rdata, st_code, st_text, hdr_list, body);
    else
        JAMI_ERR("Transaction has been created for this request, send response "
                 "statefully instead");

    return !PJ_SUCCESS;
}

template<typename T>
bool
is_uninitialized(std::weak_ptr<T> const& weak)
{
    using wt = std::weak_ptr<T>;
    return !weak.owner_before(wt {}) && !wt {}.owner_before(weak);
}

static pj_bool_t
transaction_request_cb(pjsip_rx_data* rdata)
{
    if (!rdata or !rdata->msg_info.msg) {
        JAMI_ERR("rx_data is NULL");
        return PJ_FALSE;
    }

    pjsip_method* method = &rdata->msg_info.msg->line.req.method;

    if (!method) {
        JAMI_ERR("method is NULL");
        return PJ_FALSE;
    }

    if (method->id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg(rdata))
        return PJ_FALSE;

    if (!rdata->msg_info.to or !rdata->msg_info.from or !rdata->msg_info.via) {
        JAMI_ERR("Missing From, To or Via fields");
        return PJ_FALSE;
    }

    const auto sip_to_uri = reinterpret_cast<pjsip_sip_uri*>(
        pjsip_uri_get_uri(rdata->msg_info.to->uri));
    const auto sip_from_uri = reinterpret_cast<pjsip_sip_uri*>(
        pjsip_uri_get_uri(rdata->msg_info.from->uri));
    const pjsip_host_port& sip_via = rdata->msg_info.via->sent_by;

    if (!sip_to_uri or !sip_from_uri or !sip_via.host.ptr) {
        JAMI_ERR("NULL uri");
        return PJ_FALSE;
    }

    std::string_view toUsername(sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string_view toHost(sip_to_uri->host.ptr, sip_to_uri->host.slen);
    std::string_view viaHostname(sip_via.host.ptr, sip_via.host.slen);
    const std::string_view remote_user(sip_from_uri->user.ptr, sip_from_uri->user.slen);
    const std::string_view remote_hostname(sip_from_uri->host.ptr, sip_from_uri->host.slen);
    std::string peerNumber;
    if (not remote_user.empty() and not remote_hostname.empty())
        peerNumber = remote_user + "@" + remote_hostname;
    else {
        char tmp[PJSIP_MAX_URL_SIZE];
        size_t length = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
                                        sip_from_uri,
                                        tmp,
                                        PJSIP_MAX_URL_SIZE);
        peerNumber = sip_utils::stripSipUriPrefix(std::string_view(tmp, length));
    }

    auto transport = Manager::instance().sipVoIPLink().sipTransportBroker_->addTransport(
        rdata->tp_info.transport);

    std::shared_ptr<SIPAccountBase> account;
    // If transport account is default-constructed, guessing account is allowed
    const auto& waccount = transport ? transport->getAccount() : std::weak_ptr<SIPAccountBase> {};
    if (is_uninitialized(waccount)) {
        account = Manager::instance().sipVoIPLink().guessAccount(toUsername,
                                                                 viaHostname,
                                                                 remote_hostname);
        if (not account)
            return PJ_FALSE;
        if (not transport and not ::strcmp(account->getAccountType(), SIPAccount::ACCOUNT_TYPE)) {
            if (not(transport = std::static_pointer_cast<SIPAccount>(account)->getTransport())) {
                JAMI_ERR("No suitable transport to answer this call.");
                return PJ_FALSE;
            }
            JAMI_WARN("Using transport from account.");
        }
    } else if (!(account = waccount.lock())) {
        JAMI_ERR("Dropping SIP request: account is expired.");
        return PJ_FALSE;
    }

    pjsip_msg_body* body = rdata->msg_info.msg->body;

    if (method->id == PJSIP_OTHER_METHOD) {
        std::string_view request = sip_utils::as_view(method->name);

        if (request.find("NOTIFY") != std::string_view::npos) {
            if (body and body->data) {
                std::string_view body_view(static_cast<char*>(body->data), body->len);
                auto pos = body_view.find("Voice-Message: ");
                if (pos != std::string_view::npos) {
                    int newCount {0};
                    int oldCount {0};
                    int urgentCount {0};
                    std::string sp(body_view.substr(pos));
                    int ret = sscanf(sp.c_str(),
                                     "Voice-Message: %d/%d (%d/",
                                     &newCount,
                                     &oldCount,
                                     &urgentCount);

                    // According to rfc3842
                    // urgent messages are optional
                    if (ret >= 2)
                        emitSignal<DRing::CallSignal::VoiceMailNotify>(account->getAccountID(),
                                                                       newCount,
                                                                       oldCount,
                                                                       urgentCount);
                }
            }
        } else if (request.find("MESSAGE") != std::string_view::npos) {
            // Reply 200 immediately (RFC 3428, ch. 7)
            try_respond_stateless(endpt_, rdata, PJSIP_SC_OK, nullptr, nullptr, nullptr);
            // Process message content in case of multi-part body
            auto payloads = im::parseSipMessage(rdata->msg_info.msg);
            if (payloads.size() > 0) {
                constexpr pj_str_t STR_MESSAGE_ID = jami::sip_utils::CONST_PJ_STR("Message-ID");
                auto* msgId = (pjsip_generic_string_hdr*)
                    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_MESSAGE_ID, nullptr);
                std::string id = {};
                if (!msgId) {
                    // Supports imdn message format https://tools.ietf.org/html/rfc5438#section-7.1.1.3
                    constexpr pj_str_t STR_IMDN_MESSAGE_ID = jami::sip_utils::CONST_PJ_STR(
                        "imdn.Message-ID");
                    msgId = (pjsip_generic_string_hdr*)
                        pjsip_msg_find_hdr_by_name(rdata->msg_info.msg,
                                                   &STR_IMDN_MESSAGE_ID,
                                                   nullptr);
                }
                if (msgId)
                    id = std::string(msgId->hvalue.ptr, msgId->hvalue.slen);

                if (not id.empty()) {
                    try {
                        // Mark message as treated
                        auto acc = std::dynamic_pointer_cast<JamiAccount>(account);
                        if (acc and acc->isMessageTreated(id))
                            return PJ_FALSE;
                    } catch (...) {
                    }
                }
                account->onTextMessage(id, peerNumber, payloads);
            }
            return PJ_FALSE;
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

    if (method->id == PJSIP_INVITE_METHOD) {
        // Log headers of received INVITE
        JAMI_INFO("Received a SIP INVITE request");
        sip_utils::logMessageHeaders(&rdata->msg_info.msg->hdr);
    }

    pjmedia_sdp_session* r_sdp {nullptr};

    if (body) {
        if (pjmedia_sdp_parse(rdata->tp_info.pool, (char*) body->data, body->len, &r_sdp)
            != PJ_SUCCESS) {
            JAMI_WARN("Failed to parse the SDP in offer");
            r_sdp = nullptr;
        }
    }

    if (not account->hasActiveCodec(MEDIA_AUDIO)) {
        try_respond_stateless(endpt_, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, NULL, NULL);

        return PJ_FALSE;
    }

    // Verify that we can handle the request
    unsigned options = 0;

    if (pjsip_inv_verify_request(rdata, &options, NULL, NULL, endpt_, NULL) != PJ_SUCCESS) {
        try_respond_stateless(endpt_, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, NULL, NULL, NULL);
        return PJ_FALSE;
    }

    auto const& remoteMediaList = Sdp::getMediaAttributeListFromSdp(r_sdp);
    // Use SipEventsHandler instead of SIPCall. We dont want to give
    // full access to SIPCall from this file to prevent misuse.
    std::shared_ptr<SipEventsHandler> call = account->newIncomingCall(std::string(remote_user),
                                                                      remoteMediaList,
                                                                      transport);
    call->onReceivedCall(peerNumber, r_sdp, rdata, transport);

    return PJ_FALSE;
}

static void
tp_state_callback(pjsip_transport* tp,
                  pjsip_transport_state state,
                  const pjsip_transport_state_info* info)
{
    if (auto& broker = Manager::instance().sipVoIPLink().sipTransportBroker_)
        broker->transportStateChanged(tp, state, info);
    else
        JAMI_ERR("SIPVoIPLink with invalid SipTransportBroker");
}

/*************************************************************************************************/

pjsip_endpoint*
SIPVoIPLink::getEndpoint()
{
    return endpt_;
}

pjsip_module*
SIPVoIPLink::getMod()
{
    return &mod_ua_;
}

pj_pool_t*
SIPVoIPLink::getPool() noexcept
{
    return pool_.get();
}

pj_caching_pool*
SIPVoIPLink::getCachingPool() noexcept
{
    return &cp_;
}

SIPVoIPLink::SIPVoIPLink()
    : pool_(nullptr, pj_pool_release)
{
#define TRY(ret) \
    do { \
        if ((ret) != PJ_SUCCESS) \
            throw VoipLinkException(#ret " failed"); \
    } while (0)

    sip_utils::register_thread();
    pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
    pool_.reset(pj_pool_create(&cp_.factory, PACKAGE, 64 * 1024, 4096, nullptr));
    if (!pool_)
        throw VoipLinkException("UserAgent: Could not initialize memory pool");

    TRY(pjsip_endpt_create(&cp_.factory, pj_gethostname()->ptr, &endpt_));

    auto ns = ip_utils::getLocalNameservers();
    if (not ns.empty()) {
        std::vector<pj_str_t> dns_nameservers(ns.size());
        std::vector<pj_uint16_t> dns_ports(ns.size());
        for (unsigned i = 0, n = ns.size(); i < n; i++) {
            char hbuf[NI_MAXHOST];
            if (auto ret = getnameinfo((sockaddr*) &ns[i],
                                       ns[i].getLength(),
                                       hbuf,
                                       sizeof(hbuf),
                                       nullptr,
                                       0,
                                       NI_NUMERICHOST)) {
                JAMI_WARN("Error printing SIP nameserver: %s", gai_strerror(ret));
            } else {
                JAMI_DBG("Using SIP nameserver: %s", hbuf);
                pj_strdup2(pool_.get(), &dns_nameservers[i], hbuf);
                dns_ports[i] = ns[i].getPort();
            }
        }
        pj_dns_resolver* resv;
        if (auto ret = pjsip_endpt_create_resolver(endpt_, &resv)) {
            JAMI_WARN("Error creating SIP DNS resolver: %s", sip_utils::sip_strerror(ret).c_str());
        } else {
            if (auto ret = pj_dns_resolver_set_ns(resv,
                                                  dns_nameservers.size(),
                                                  dns_nameservers.data(),
                                                  dns_ports.data())) {
                JAMI_WARN("Error setting SIP DNS servers: %s", sip_utils::sip_strerror(ret).c_str());
            } else {
                if (auto ret = pjsip_endpt_set_resolver(endpt_, resv)) {
                    JAMI_WARN("Error setting pjsip DNS resolver: %s",
                              sip_utils::sip_strerror(ret).c_str());
                }
            }
        }
    }

    sipTransportBroker_.reset(new SipTransportBroker(endpt_));

    auto status = pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), tp_state_callback);
    if (status != PJ_SUCCESS)
        JAMI_ERR("Can't set transport callback: %s", sip_utils::sip_strerror(status).c_str());

    TRY(pjsip_tsx_layer_init_module(endpt_));
    TRY(pjsip_ua_init_module(endpt_, nullptr));
    TRY(pjsip_replaces_init_module(endpt_)); // See the Replaces specification in RFC 3891
    TRY(pjsip_100rel_init_module(endpt_));

    // Initialize and register ring module
    mod_ua_.name = sip_utils::CONST_PJ_STR(PACKAGE);
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
        nullptr /* on_rx_offer */,
        nullptr /* on_rx_offer2 */,
        reinvite_received_cb,
        sdp_create_offer_cb,
        sdp_media_update_cb,
        nullptr /* on_send_ack */,
        nullptr /* on_redirected */,
    };
    TRY(pjsip_inv_usage_init(endpt_, &inv_cb));

    static constexpr pj_str_t allowed[] = {
        CONST_PJ_STR("INFO"),
        CONST_PJ_STR("OPTIONS"),
        CONST_PJ_STR("MESSAGE"),
        CONST_PJ_STR("PUBLISH"),
    };

    pjsip_endpt_add_capability(endpt_,
                               &mod_ua_,
                               PJSIP_H_ALLOW,
                               nullptr,
                               PJ_ARRAY_SIZE(allowed),
                               allowed);

    static constexpr pj_str_t text_plain = CONST_PJ_STR("text/plain");
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, nullptr, 1, &text_plain);

    static constexpr pj_str_t accepted = CONST_PJ_STR("application/sdp");
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, nullptr, 1, &accepted);

    static constexpr pj_str_t iscomposing = CONST_PJ_STR("application/im-iscomposing+xml");
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, nullptr, 1, &iscomposing);

    TRY(pjsip_replaces_init_module(endpt_));
#undef TRY

    sipScheduler_ = std::make_shared<ScheduledExecutor>();

    sipScheduler_->run([this] {
        if (sip_utils::register_thread() != PJ_SUCCESS) {
            JAMI_WARN("Failed to register SIP execution thread");
        } else {
            JAMI_DBG("SIP thread registered. Start polling SIP events on this thread");
        }
    });

    JAMI_DBG("Start polling SIP events");
    pollTask_ = sipScheduler_->scheduleAtFixedRate(
        [this] {
            handleEvents(SIP_EVENTS_POLLING_TIMEOUT);
            return running_.load();
        },
        std::chrono::milliseconds(SIP_EVENTS_POLLING_PERIOD));

    JAMI_DBG("SIPVoIPLink@%p", this);
}

SIPVoIPLink::~SIPVoIPLink() {}

void
SIPVoIPLink::shutdown()
{
    JAMI_DBG("Shutdown SIPVoIPLink@%p...", this);
    // Remaining calls should not happen as possible upper callbacks
    // may be called and another instance of SIPVoIPLink can be re-created!

    if (not Manager::instance().callFactory.empty(Call::LinkType::SIP))
        JAMI_ERR("%zu SIP calls remains!",
                 Manager::instance().callFactory.callCount(Call::LinkType::SIP));

    sipTransportBroker_->shutdown();
    pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(endpt_), nullptr);
    sipTransportBroker_.reset();

    running_ = false;
    pjsip_endpt_destroy(endpt_);
    pool_.reset();
    pj_caching_pool_destroy(&cp_);

    JAMI_DBG("SIPVoIPLink@%p is shutdown", this);
}

std::shared_ptr<SIPAccountBase>
SIPVoIPLink::guessAccount(std::string_view userName,
                          std::string_view server,
                          std::string_view fromUri) const
{
    JAMI_DBG("username = %.*s, server = %.*s, from = %.*s",
             (int) userName.size(),
             userName.data(),
             (int) server.size(),
             server.data(),
             (int) fromUri.size(),
             fromUri.data());
    // Try to find the account id from username and server name by full match

    std::shared_ptr<SIPAccountBase> result;
    std::shared_ptr<SIPAccountBase> IP2IPAccount;
    MatchRank best = MatchRank::NONE;

    // SIP accounts
    for (const auto& account : Manager::instance().getAllAccounts<SIPAccount>()) {
        const MatchRank match(account->matches(userName, server));

        // return right away if this is a full match
        if (match == MatchRank::FULL) {
            return account;
        } else if (match > best) {
            best = match;
            result = account;
        } else if (!IP2IPAccount && account->isIP2IP()) {
            // Allow IP2IP calls if an account exists for this type of calls
            IP2IPAccount = account;
        }
    }

    return result ? result : IP2IPAccount;
}

// Called from EventThread::run (not main thread)
void
SIPVoIPLink::handleEvents(const pj_time_val timeout)
{
    if (auto ret = pjsip_endpt_handle_events(endpt_, &timeout))
        JAMI_ERR("pjsip_endpt_handle_events failed with error %s",
                 sip_utils::sip_strerror(ret).c_str());
}

void
SIPVoIPLink::registerKeepAliveTimer(pj_timer_entry& timer, pj_time_val& delay)
{
    JAMI_DBG("Register new keep alive timer %d with delay %ld", timer.id, delay.sec);

    if (timer.id == -1)
        JAMI_WARN("Timer already scheduled");

    switch (pjsip_endpt_schedule_timer(endpt_, &timer, &delay)) {
    case PJ_SUCCESS:
        break;

    default:
        JAMI_ERR("Could not schedule new timer in pjsip endpoint");

        /* fallthrough */
    case PJ_EINVAL:
        JAMI_ERR("Invalid timer or delay entry");
        break;

    case PJ_EINVALIDOP:
        JAMI_ERR("Invalid timer entry, maybe already scheduled");
        break;
    }
}

void
SIPVoIPLink::cancelKeepAliveTimer(pj_timer_entry& timer)
{
    pjsip_endpt_cancel_timer(endpt_, &timer);
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

static void
invite_session_state_changed_cb(pjsip_inv_session* inv, pjsip_event* ev)
{
    if (auto call = getSipEventsHandler(inv)) {
        call->onInviteSessionStateChanged(inv, ev);
    }
}

static pj_status_t
reinvite_received_cb(pjsip_inv_session* inv, const pjmedia_sdp_session* offer, pjsip_rx_data* rdata)
{
    if (auto call = getSipEventsHandler(inv)) {
        return call->onReceiveReinvite(offer, rdata);
    }

    // Return success if there is no matching call. The re-invite
    // should be ignored.
    return PJ_SUCCESS;
}

static void
sdp_create_offer_cb(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer)
{
    if (auto call = getSipEventsHandler(inv)) {
        call->onSdpCreateOffer(inv, p_offer);
    }
}

// This callback is called after SDP offer/answer session has completed.
static void
sdp_media_update_cb(pjsip_inv_session* inv, pj_status_t status)
{
    if (auto call = getSipEventsHandler(inv)) {
        call->onSdpMediaUpdate(inv, status);
    }
}

static void
outgoing_request_forked_cb(pjsip_inv_session* /*inv*/, pjsip_event* /*e*/)
{}

static void
transaction_state_changed_cb(pjsip_inv_session* inv, pjsip_transaction* tsx, pjsip_event* event)
{
    auto call = getSipEventsHandler(inv);
    if (not call)
        return;

#ifdef DEBUG_SIP_REQUEST_MSG
    processInviteResponseHelper(inv, event);
#endif

    // We process here only incoming request message
    if (tsx->role != PJSIP_ROLE_UAS or tsx->state != PJSIP_TSX_STATE_TRYING
        or event->body.tsx_state.type != PJSIP_EVENT_RX_MSG) {
        return;
    }

    const auto rdata = event->body.tsx_state.src.rdata;
    if (!rdata) {
        JAMI_ERR("[INVITE:%p] SIP RX request without rx data", inv);
        return;
    }

    const auto msg = rdata->msg_info.msg;
    if (msg == nullptr)
        throw VoipLinkException("Unexpected null pointer");

    if (msg->type != PJSIP_REQUEST_MSG) {
        JAMI_ERR("[INVITE:%p] SIP RX request without msg", inv);
        return;
    }

    // Using method name to dispatch
    auto methodName = sip_utils::as_view(msg->line.req.method.name);
    JAMI_DBG("[INVITE:%p] RX SIP method %d (%.*s)",
             inv,
             msg->line.req.method.id,
             (int) methodName.size(),
             methodName.data());

#ifdef DEBUG_SIP_REQUEST_MSG
    char msgbuf[1000];
    pjsip_msg_print(msg, msgbuf, sizeof msgbuf);
    JAMI_DBG("%s", msgbuf);
#endif // DEBUG_SIP_MESSAGE

    if (methodName == "REFER")
        call->onRequestRefer(inv, rdata, msg);
    else if (methodName == "INFO")
        call->onRequestInfo(rdata, msg);
    else if (methodName == "NOTIFY")
        call->onRequestNotify(inv, rdata, msg);
    else if (methodName == "MESSAGE") {
        if (msg->body)
            call->onReceivedTextMessage(im::parseSipMessage(msg));
    }
}

#ifdef DEBUG_SIP_REQUEST_MSG
static void
processInviteResponseHelper(pjsip_inv_session* inv, pjsip_event* event)
{
    if (event->body.tsx_state.type != PJSIP_EVENT_RX_MSG)
        return;

    const auto rdata = event->body.tsx_state.src.rdata;
    if (rdata == nullptr or rdata->msg_info.msg == nullptr)
        return;

    const auto msg = rdata->msg_info.msg;
    if (msg->type != PJSIP_RESPONSE_MSG)
        return;

    // Only handle the following responses
    switch (msg->line.status.code) {
    case PJSIP_SC_TRYING:
    case PJSIP_SC_RINGING:
    case PJSIP_SC_OK:
        break;
    default:
        return;
    }

    JAMI_INFO("[INVITE:%p] SIP RX response: reason %.*s, status code %i",
              inv,
              (int) msg->line.status.reason.slen,
              msg->line.status.reason.ptr,
              msg->line.status.code);

    sip_utils::logMessageHeaders(&msg->hdr);
}
#endif

int
SIPVoIPLink::getModId()
{
    return mod_ua_.id;
}

void
SIPVoIPLink::createSDPOffer(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer)
{
    assert(inv and p_offer);
    sdp_create_offer_cb(inv, p_offer);
}

// Thread-safe DNS resolver callback mapping
class SafeResolveCallbackMap
{
public:
    using ResolveCallback = std::function<void(pj_status_t, const pjsip_server_addresses*)>;

    void registerCallback(uintptr_t key, ResolveCallback&& cb)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        cbMap_.emplace(key, std::move(cb));
    }

    void process(uintptr_t key, pj_status_t status, const pjsip_server_addresses* addr)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = cbMap_.find(key);
        if (it != cbMap_.end()) {
            it->second(status, addr);
            cbMap_.erase(it);
        }
    }

private:
    std::mutex mutex_;
    std::map<uintptr_t, ResolveCallback> cbMap_;
};

static SafeResolveCallbackMap&
getResolveCallbackMap()
{
    static SafeResolveCallbackMap map;
    return map;
}

static void
resolver_callback(pj_status_t status, void* token, const struct pjsip_server_addresses* addr)
{
    getResolveCallbackMap().process((uintptr_t) token, status, addr);
}

void
SIPVoIPLink::resolveSrvName(const std::string& name,
                            pjsip_transport_type_e type,
                            SrvResolveCallback&& cb)
{
    // PJSIP limits hostname to be longer than PJ_MAX_HOSTNAME.
    // But, resolver prefix the given name by a string like "_sip._udp."
    // causing a check against PJ_MAX_HOSTNAME to be useless.
    // It's not easy to pre-determinate as it's implementation dependent.
    // So we just choose a security marge enough for most cases, preventing a crash later
    // in the call of pjsip_endpt_resolve().
    if (name.length() > (PJ_MAX_HOSTNAME - 12)) {
        JAMI_ERR("Hostname is too long");
        cb({});
        return;
    }

    // extract port if name is in form "server:port"
    int port;
    pj_ssize_t name_size;
    const auto n = name.rfind(':');
    if (n != std::string::npos) {
        port = std::atoi(name.c_str() + n + 1);
        name_size = n;
    } else {
        port = 0;
        name_size = name.size();
    }
    JAMI_DBG("try to resolve '%s' (port: %u)", name.c_str(), port);

    pjsip_host_info host_info {
        /*.flag = */ 0,
        /*.type = */ type,
        /*.addr = */ {{(char*) name.c_str(), name_size}, port},
    };

    const auto token = std::hash<std::string>()(name + std::to_string(type));
    getResolveCallbackMap().registerCallback(
        token, [=, cb = std::move(cb)](pj_status_t s, const pjsip_server_addresses* r) {
            try {
                if (s != PJ_SUCCESS || !r) {
                    JAMI_WARN("Can't resolve \"%s\" using pjsip_endpt_resolve, trying getaddrinfo.",
                              name.c_str());
                    dht::ThreadPool::io().run([=, cb = std::move(cb)]() {
                        auto ips = ip_utils::getAddrList(name.c_str());
                        runOnMainThread(std::bind(cb, std::move(ips)));
                    });
                } else {
                    std::vector<IpAddr> ips;
                    ips.reserve(r->count);
                    for (unsigned i = 0; i < r->count; i++)
                        ips.push_back(r->entry[i].addr);
                    cb(ips);
                }
            } catch (const std::exception& e) {
                JAMI_ERR("Error resolving address: %s", e.what());
                cb({});
            }
        });

    pjsip_endpt_resolve(endpt_, pool_.get(), &host_info, (void*) token, resolver_callback);
}

#define RETURN_IF_NULL(A, ...) \
    if ((A) == NULL) { \
        JAMI_WARN(__VA_ARGS__); \
        return; \
    }

#define RETURN_FALSE_IF_NULL(A, ...) \
    if ((A) == NULL) { \
        JAMI_WARN(__VA_ARGS__); \
        return false; \
    }

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
    addr = sip_utils::as_view(*pj_gethostname());

    // Update address and port with active transport
    RETURN_IF_NULL(transport,
                   "Transport is NULL in findLocalAddress, using local address %s :%d",
                   addr.c_str(),
                   port);

    // get the transport manager associated with the SIP enpoint
    auto tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    RETURN_IF_NULL(tpmgr,
                   "Transport manager is NULL in findLocalAddress, using local address %s :%d",
                   addr.c_str(),
                   port);

    const pj_str_t pjstring(CONST_PJ_STR(host));

    auto tp_sel = getTransportSelector(transport);
    pjsip_tpmgr_fla2_param param
        = {transportType, &tp_sel, pjstring, PJ_FALSE, {nullptr, 0}, 0, nullptr};
    if (pjsip_tpmgr_find_local_addr2(tpmgr, pool_.get(), &param) != PJ_SUCCESS) {
        JAMI_WARN("Could not retrieve local address and port from transport, using %s :%d",
                  addr.c_str(),
                  port);
        return;
    }

    // Update local address based on the transport type
    addr = sip_utils::as_view(param.ret_addr);

    // Determine the local port based on transport information
    port = param.ret_port;
}

bool
SIPVoIPLink::findLocalAddressFromSTUN(pjsip_transport* transport,
                                      pj_str_t* stunServerName,
                                      int stunPort,
                                      std::string& addr,
                                      pj_uint16_t& port) const
{
    // WARN: this code use pjstun_get_mapped_addr2 that works
    // in IPv4 only.
    // WARN: this function is blocking (network request).

    // Initialize the sip port with the default SIP port
    port = sip_utils::DEFAULT_SIP_PORT;

    // Get Local IP address
    auto localIp = ip_utils::getLocalAddr(pj_AF_INET());
    if (not localIp) {
        JAMI_WARN("Failed to find local IP");
        return false;
    }

    addr = localIp.toString();

    // Update address and port with active transport
    RETURN_FALSE_IF_NULL(transport,
                         "Transport is NULL in findLocalAddress, using local address %s:%u",
                         addr.c_str(),
                         port);

    JAMI_DBG("STUN mapping of '%s:%u'", addr.c_str(), port);

    pj_sockaddr_in mapped_addr;
    pj_sock_t sipSocket = pjsip_udp_transport_get_socket(transport);
    const pjstun_setting stunOpt
        = {PJ_TRUE, localIp.getFamily(), *stunServerName, stunPort, *stunServerName, stunPort};
    const pj_status_t stunStatus = pjstun_get_mapped_addr2(&cp_.factory,
                                                           &stunOpt,
                                                           1,
                                                           &sipSocket,
                                                           &mapped_addr);

    switch (stunStatus) {
    case PJLIB_UTIL_ESTUNNOTRESPOND:
        JAMI_ERR("No response from STUN server %.*s",
                 (int) stunServerName->slen,
                 stunServerName->ptr);
        return false;

    case PJLIB_UTIL_ESTUNSYMMETRIC:
        JAMI_ERR("Different mapped addresses are returned by servers.");
        return false;

    case PJ_SUCCESS:
        port = pj_sockaddr_in_get_port(&mapped_addr);
        addr = IpAddr((const pj_sockaddr&) mapped_addr).toString();
        JAMI_DBG("STUN server %.*s replied '%s:%u'",
                 (int) stunServerName->slen,
                 stunServerName->ptr,
                 addr.c_str(),
                 port);
        return true;

    default: // use given address, silent any not handled error
        JAMI_WARN("Error from STUN server %.*s, using source address",
                  (int) stunServerName->slen,
                  stunServerName->ptr);
        return false;
    }
}

std::shared_ptr<SipEventsHandler>
SIPVoIPLink::getEventsHandler(pjsip_inv_session* inv)
{
    if (auto call_ptr = static_cast<SIPCall*>(inv->mod_data[mod_ua_.id]))
        return std::static_pointer_cast<SIPCall>(call_ptr->shared_from_this());
    return nullptr;
}

#undef RETURN_IF_NULL
#undef RETURN_FALSE_IF_NULL
} // namespace jami
