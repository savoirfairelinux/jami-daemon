/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sipvoiplink.h"

#include "manager.h"

#include "sip/sdp.h"
#include "sipcall.h"
#include "sipaccount.h"
#include "eventthread.h"
#include "sdes_negotiator.h"

#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"

#include "hooks/urlhook.h"
#include "im/instant_messaging.h"

#include "audio/audiolayer.h"

#include "pjsip/sip_endpoint.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_uri.h"
#include <pjnath.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <istream>
#include <utility> // for std::pair

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include <map>

using namespace sfl;

namespace {

static pjsip_transport *localUDPTransport_ = NULL; /** The default transport (5060) */

/** A map to retreive SFLphone internal call id
 *  Given a SIP call ID (usefull for transaction sucha as transfer)*/
static std::map<std::string, std::string> transferCallID;

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/**
 * Set audio (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 */
void setCallMediaLocal(SIPCall* call, const std::string &localIP);

/**
 * Helper function to parser header from incoming sip messages
 */
std::string fetchHeaderValue(pjsip_msg *msg, const std::string &field);

static pj_caching_pool pool_cache, *cp_ = &pool_cache;
static pj_pool_t *pool_;
static pjsip_endpoint *endpt_;
static pjsip_module mod_ua_;
static pj_thread_t *thread;

void sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status UNUSED);
void sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer);
void sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer);
void invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *e);
void outgoing_request_forked_cb(pjsip_inv_session *inv, pjsip_event *e);
void transaction_state_changed_cb(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);
void registration_cb(pjsip_regc_cbparam *param);
pj_bool_t transaction_request_cb(pjsip_rx_data *rdata);
pj_bool_t transaction_response_cb(pjsip_rx_data *rdata UNUSED) ;

void transfer_client_cb(pjsip_evsub *sub, pjsip_event *event);

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 * @param sip call
 */
int SIPSessionReinvite(SIPCall *);

/**
 * Helper function to process refer function on call transfer
 */
void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata);

std::string getSIPLocalIP()
{
    pj_sockaddr ip_addr;

    if (pj_gethostip(pj_AF_INET(), &ip_addr) == PJ_SUCCESS)
        return pj_inet_ntoa(ip_addr.ipv4.sin_addr);
    else  {
        ERROR("SIPVoIPLink: Could not get local IP");
        return "";
    }
}

pjsip_route_hdr *createRouteSet(const std::string &route, pj_pool_t *hdr_pool)
{
    int port = 0;
    std::string host;

    size_t found = route.find(":");

    if (found != std::string::npos) {
        host = route.substr(0, found);
        port = atoi(route.substr(found + 1, route.length()).c_str());
    } else
        host = route;

    pjsip_route_hdr *route_set = pjsip_route_hdr_create(hdr_pool);
    pjsip_route_hdr *routing = pjsip_route_hdr_create(hdr_pool);
    pjsip_sip_uri *url = pjsip_sip_uri_create(hdr_pool, 0);
    routing->name_addr.uri = (pjsip_uri*) url;
    pj_strdup2(hdr_pool, &url->host, host.c_str());
    url->port = port;

    pj_list_push_back(route_set, pjsip_hdr_clone(hdr_pool, routing));

    return route_set;
}

void handleIncomingOptions(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;

    if (pjsip_endpt_create_response(endpt_, rdata, PJSIP_SC_OK, NULL, &tdata) != PJ_SUCCESS)
        return;

#define ADD_HDR(hdr) do { \
    const pjsip_hdr *cap_hdr = hdr; \
    if (cap_hdr) \
    pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr)); \
} while(0)
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

pj_bool_t transaction_response_cb(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);

    if (!dlg)
        return PJ_SUCCESS;

    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);

    if (!tsx or tsx->method.id != PJSIP_INVITE_METHOD)
        return PJ_SUCCESS;

    if (tsx->status_code / 100 == 2) {
        /**
         * Send an ACK message inside a transaction. PJSIP send automatically, non-2xx ACK response.
         * ACK for a 2xx response must be send using this method.
         */
        pjsip_tx_data *tdata;
        pjsip_dlg_create_request(dlg, &pjsip_ack_method, rdata->msg_info.cseq->cseq, &tdata);
        pjsip_dlg_send_request(dlg, tdata, -1, NULL);
    }

    return PJ_SUCCESS;
}

std::string parseDisplayName(const char * buffer)
{
    const char* from_header = strstr(buffer, "From: ");

    if (!from_header)
        return "";

    std::string temp(from_header);
    size_t begin_displayName = temp.find("\"") + 1;
    size_t end_displayName = temp.rfind("\"");
    std::string displayName(temp.substr(begin_displayName, end_displayName - begin_displayName));

    static const size_t MAX_DISPLAY_NAME_SIZE = 25;
    if (displayName.size() > MAX_DISPLAY_NAME_SIZE)
        return "";

    return displayName;
}

void stripSipUriPrefix(std::string& sipUri)
{
    // Remove sip: prefix
    static const char SIP_PREFIX[] = "sip:";
    size_t found = sipUri.find(SIP_PREFIX);

    if (found != std::string::npos)
        sipUri.erase(found, found + (sizeof SIP_PREFIX) - 1);

    found = sipUri.find("@");

    if (found != std::string::npos)
        sipUri.erase(found);
}

pj_bool_t transaction_request_cb(pjsip_rx_data *rdata)
{
    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

    if (method->id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg(rdata))
        return true;

    pjsip_sip_uri *sip_to_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.to->uri);
    pjsip_sip_uri *sip_from_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(rdata->msg_info.from->uri);
    std::string userName(sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string server(sip_from_uri->host.ptr, sip_from_uri->host.slen);
    std::string account_id(Manager::instance().getAccountIdFromNameAndServer(userName, server));

    std::string displayName(parseDisplayName(rdata->msg_info.msg_buf));

    if (method->id == PJSIP_OTHER_METHOD) {
        pj_str_t *str = &method->name;
        std::string request(str->ptr, str->slen);

        if (request.find("NOTIFY") != (size_t)-1) {
            int voicemail;

            if (sscanf((const char*)rdata->msg_info.msg->body->data, "Voice-Message: %d/", &voicemail) == 1 && voicemail != 0)
                Manager::instance().startVoiceMessageNotification(account_id, voicemail);
        }

        pjsip_endpt_respond_stateless(endpt_, rdata, PJSIP_SC_OK, NULL, NULL, NULL);

        return true;
    } else if (method->id == PJSIP_OPTIONS_METHOD) {
        handleIncomingOptions(rdata);
        return true;
    } else if (method->id != PJSIP_INVITE_METHOD && method->id != PJSIP_ACK_METHOD) {
        pjsip_endpt_respond_stateless(endpt_, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, NULL, NULL, NULL);
        return true;
    }

    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(account_id));

    pjmedia_sdp_session *r_sdp;
    pjsip_msg_body *body = rdata->msg_info.msg->body;

    if (!body || pjmedia_sdp_parse(rdata->tp_info.pool, (char*) body->data, body->len, &r_sdp) != PJ_SUCCESS)
        r_sdp = NULL;

    if (account->getActiveCodecs().empty()) {
        pjsip_endpt_respond_stateless(endpt_, rdata,
                                      PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, NULL,
                                      NULL);
        return false;
    }

    // Verify that we can handle the request
    unsigned options = 0;

    if (pjsip_inv_verify_request(rdata, &options, NULL, NULL, endpt_, NULL) != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless(endpt_, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, NULL, NULL, NULL);
        return true;
    }

    if (Manager::instance().hookPreference.getSipEnabled()) {
        std::string header_value(fetchHeaderValue(rdata->msg_info.msg, Manager::instance().hookPreference.getUrlSipField()));
        UrlHook::runAction(Manager::instance().hookPreference.getUrlCommand(), header_value);
    }

    SIPCall* call = new SIPCall(Manager::instance().getNewCallID(), Call::INCOMING, cp_);
    Manager::instance().associateCallToAccount(call->getCallId(), account_id);

    // May use the published address as well
    std::string addrToUse = SIPVoIPLink::instance()->getInterfaceAddrFromName(account->getLocalInterface());
    std::string addrSdp = account->isStunEnabled()
                          ? account->getPublishedAddress()
                          : addrToUse;

    pjsip_tpselector *tp = SIPVoIPLink::instance()->initTransportSelector(account->transport_, call->getMemoryPool());

    if (addrToUse == "0.0.0.0")
        addrToUse = getSIPLocalIP();

    if (addrSdp == "0.0.0.0")
        addrSdp = addrToUse;

    char tmp[PJSIP_MAX_URL_SIZE];
    int length = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, sip_from_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber(tmp, length);
    stripSipUriPrefix(peerNumber);

    call->setConnectionState(Call::PROGRESSING);
    call->setPeerNumber(peerNumber);
    call->setDisplayName(displayName);
    call->initRecFilename(peerNumber);

    setCallMediaLocal(call, addrToUse);

    call->getLocalSDP()->setLocalIP(addrSdp);

    call->getAudioRtp().initAudioRtpConfig();
    call->getAudioRtp().initAudioSymmetricRtpSession();

    if (rdata->msg_info.msg->body) {
        char sdpbuffer[1000];
        int len = rdata->msg_info.msg->body->print_body(rdata->msg_info.msg->body, sdpbuffer, sizeof sdpbuffer);

        if (len == -1) // error
            len = 0;

        std::string sdpoffer(sdpbuffer, len);
        size_t start = sdpoffer.find("a=crypto:");

        // Found crypto header in SDP
        if (start != std::string::npos) {
            CryptoOffer crypto_offer;
            crypto_offer.push_back(std::string(sdpoffer.substr(start, (sdpoffer.size() - start) - 1)));

            std::vector<sfl::CryptoSuiteDefinition>localCapabilities;

            for (int i = 0; i < 3; i++)
                localCapabilities.push_back(sfl::CryptoSuites[i]);

            sfl::SdesNegotiator sdesnego(localCapabilities, crypto_offer);

            if (sdesnego.negotiate()) {
                call->getAudioRtp().setRemoteCryptoInfo(sdesnego);
                call->getAudioRtp().initLocalCryptoInfo();
            }
        }
    }

    call->getLocalSDP()->receiveOffer(r_sdp, account->getActiveCodecs());

    sfl::Codec* audiocodec = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);
    call->getAudioRtp().start(static_cast<sfl::AudioCodec *>(audiocodec));

    pjsip_dialog* dialog;

    if (pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, NULL, &dialog) != PJ_SUCCESS) {
        delete call;
        pjsip_endpt_respond_stateless(endpt_, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL, NULL);
        return false;
    }

    pjsip_inv_create_uas(dialog, rdata, call->getLocalSDP()->getLocalSdpSession(), 0, &call->inv);

    PJ_ASSERT_RETURN(pjsip_dlg_set_transport(dialog, tp) == PJ_SUCCESS, 1);

    call->inv->mod_data[mod_ua_.id] = call;

    // Check whether Replaces header is present in the request and process accordingly.
    pjsip_dialog *replaced_dlg;
    pjsip_tx_data *response;

    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
        ERROR("Something wrong with Replaces request.");
        pjsip_endpt_respond_stateless(endpt_, rdata, 500 /* internal server error */, NULL, NULL, NULL);
    }

    // Check if call has been transfered
    pjsip_tx_data *tdata;

    if (replaced_dlg) { // If Replace header present
        // Always answer the new INVITE with 200, regardless whether
        // the replaced call is in early or confirmed state.
        if (pjsip_inv_answer(call->inv, 200, NULL, NULL, &response) == PJ_SUCCESS)
            pjsip_inv_send_msg(call->inv, response);

        // Get the INVITE session associated with the replaced dialog.
        pjsip_inv_session *replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

        // Disconnect the "replaced" INVITE session.
        if (pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS && tdata)
            pjsip_inv_send_msg(replaced_inv, tdata);
    } else { // Prooceed with normal call flow
        PJ_ASSERT_RETURN(pjsip_inv_initial_answer(call->inv, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata) == PJ_SUCCESS, 1);
        PJ_ASSERT_RETURN(pjsip_inv_send_msg(call->inv, tdata) == PJ_SUCCESS, 1);

        call->setConnectionState(Call::RINGING);

        Manager::instance().incomingCall(call, account_id);
        Manager::instance().getAccountLink(account_id)->addCall(call);
    }

    return true;
}
} // end anonymous namespace

/*************************************************************************************************/

SIPVoIPLink::SIPVoIPLink() : transportMap_(), evThread_(new EventThread(this))
{
#define TRY(ret) do { \
    if (ret != PJ_SUCCESS) \
    throw VoipLinkException(#ret " failed"); \
} while(0)

    srand(time(NULL)); // to get random number for RANDOM_PORT

    TRY(pj_init());
    TRY(pjlib_util_init());
    // From 0 (min) to 6 (max)
    pj_log_set_level(Logger::getDebugMode() ? 6 : 0);
    TRY(pjnath_init());

    pj_caching_pool_init(cp_, &pj_pool_factory_default_policy, 0);
    pool_ = pj_pool_create(&cp_->factory, "sflphone", 4000, 4000, NULL);

    if (!pool_)
        throw VoipLinkException("UserAgent: Could not initialize memory pool");

    TRY(pjsip_endpt_create(&cp_->factory, pj_gethostname()->ptr, &endpt_));

    if (getSIPLocalIP().empty())
        throw VoipLinkException("UserAgent: Unable to determine network capabilities");

    TRY(pjsip_tsx_layer_init_module(endpt_));
    TRY(pjsip_ua_init_module(endpt_, NULL));
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

    static const pjsip_inv_callback inv_cb = {
        invite_session_state_changed_cb,
        outgoing_request_forked_cb,
        transaction_state_changed_cb,
        sdp_request_offer_cb,
        sdp_create_offer_cb,
        sdp_media_update_cb,
        NULL,
        NULL,
    };
    TRY(pjsip_inv_usage_init(endpt_, &inv_cb));

    static const pj_str_t allowed[] = { { (char*) "INFO", 4}, { (char*) "REGISTER", 8}, { (char*) "OPTIONS", 7}, { (char*) "MESSAGE", 7 } };       //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6}
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE(allowed), allowed);

    static const pj_str_t text_plain = { (char*) "text/plain", 10 };
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, NULL, 1, &text_plain);

    static const pj_str_t accepted = { (char*) "application/sdp", 15 };
    pjsip_endpt_add_capability(endpt_, &mod_ua_, PJSIP_H_ACCEPT, NULL, 1, &accepted);

    DEBUG("UserAgent: pjsip version %s for %s initialized", pj_get_version(), PJ_OS_NAME);

    TRY(pjsip_replaces_init_module(endpt_));
#undef TRY

    evThread_->start();
}

SIPVoIPLink::~SIPVoIPLink()
{
    delete evThread_;
    pj_thread_join(thread);
    pj_thread_destroy(thread);

    const pj_time_val tv = {0, 10};
    pjsip_endpt_handle_events(endpt_, &tv);
    pjsip_endpt_destroy(endpt_);

    pj_pool_release(pool_);
    pj_caching_pool_destroy(cp_);

    pj_shutdown();
}

SIPVoIPLink* SIPVoIPLink::instance()
{
    static SIPVoIPLink* instance = NULL;

    if (!instance)
        instance = new SIPVoIPLink;

    return instance;
}

void SIPVoIPLink::init() {}

void SIPVoIPLink::terminate() {}

void
SIPVoIPLink::getEvent()
{
    static pj_thread_desc desc;

    // We have to register the external thread so it could access the pjsip frameworks
    if (!pj_thread_is_registered())
        pj_thread_register(NULL, desc, &thread);

    static const pj_time_val timeout = {0, 10};
    pjsip_endpt_handle_events(endpt_, &timeout);
}

void SIPVoIPLink::sendRegister(Account *a)
{
    SIPAccount *account = dynamic_cast<SIPAccount*>(a);
    createSipTransport(account);

    account->setRegister(true);
    account->setRegistrationState(Trying);

    pjsip_regc *regc = account->getRegistrationInfo();

    if (pjsip_regc_create(endpt_, (void *) account, &registration_cb, &regc) != PJ_SUCCESS)
        throw VoipLinkException("UserAgent: Unable to create regc structure.");

    std::string srvUri(account->getServerUri());

    // std::string address, port;
    // findLocalAddressFromUri(srvUri, account->transport_, address, port);
    pj_str_t pjSrv = pj_str((char*) srvUri.c_str());

    // Generate the FROM header
    std::string from(account->getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    // Get the contact header for this account
    std::string contact(account->getContactHeader());
    pj_str_t pjContact = pj_str((char*) contact.c_str());

    if (pjsip_regc_init(regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, account->getRegistrationExpire()) != PJ_SUCCESS)
        throw VoipLinkException("Unable to initialize account registration structure");

    if (!account->getServiceRoute().empty())
        pjsip_regc_set_route_set(regc, createRouteSet(account->getServiceRoute(), pool_));

    pjsip_regc_set_credentials(regc, account->getCredentialCount(), account->getCredInfo());


    pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    std::string useragent(account->getUserAgentName());
    pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
    const pj_str_t STR_USER_AGENT = { (char*) "User-Agent", 10 };

    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create(pool_, &STR_USER_AGENT, &pJuseragent);
    pj_list_push_back(&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers(regc, &hdr_list);


    pjsip_tx_data *tdata;

    if (pjsip_regc_register(regc, PJ_TRUE, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to initialize transaction data for account registration");

    if (pjsip_regc_set_transport(regc, initTransportSelector(account->transport_, pool_)) != PJ_SUCCESS)
        throw VoipLinkException("Unable to set transport");

    // decrease transport's ref count, counter incrementation is managed when acquiring transport
    pjsip_transport_dec_ref(account->transport_);

    // pjsip_regc_send increment the transport ref count by one,
    if (pjsip_regc_send(regc, tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to send account registration request");

    // Decrease transport's ref count, since coresponding reference counter decrementation
    // is performed in pjsip_regc_destroy. This function is never called in SFLphone as the
    // regc data structure is permanently associated to the account at first registration.
    pjsip_transport_dec_ref(account->transport_);

    account->setRegistrationInfo(regc);

    // start the periodic registration request based on Expire header
    // account determines itself if a keep alive is required
    account->startKeepAliveTimer();
}

void SIPVoIPLink::sendUnregister(Account *a)
{
    SIPAccount *account = dynamic_cast<SIPAccount *>(a);

    // This may occurs if account failed to register and is in state INVALID
    if (!account->isRegistered()) {
        account->setRegistrationState(Unregistered);
        return;
    }

    // Make sure to cancel any ongoing timers before unregister
    account->stopKeepAliveTimer();

    pjsip_regc *regc = account->getRegistrationInfo();

    if (!regc)
        throw VoipLinkException("Registration structure is NULL");

    pjsip_tx_data *tdata = NULL;

    if (pjsip_regc_unregister(regc, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to unregister sip account");

    if (pjsip_regc_send(regc, tdata) != PJ_SUCCESS)
        throw VoipLinkException("Unable to send request to unregister sip account");

    account->setRegister(false);
}

void SIPVoIPLink::registerKeepAliveTimer(pj_timer_entry& timer, pj_time_val& delay)
{
    pj_status_t status;

    status = pjsip_endpt_schedule_timer(endpt_, &timer, &delay);
    if (status != PJ_SUCCESS)
        ERROR("Could not schedule new timer in pjsip endpoint");
}

void SIPVoIPLink::cancelKeepAliveTimer(pj_timer_entry& timer)
{
    pjsip_endpt_cancel_timer(endpt_, &timer);
}

Call *SIPVoIPLink::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(Manager::instance().getAccountFromCall(id)));

    if (account == NULL) // TODO: We should investigate how we could get rid of this error and create a IP2IP call instead
        throw VoipLinkException("Could not get account for this call");

    SIPCall* call = new SIPCall(id, Call::OUTGOING, cp_);

    // If toUri is not a well formated sip URI, use account information to process it
    std::string toUri;

    if (toUrl.find("sip:") != std::string::npos or
        toUrl.find("sips:") != std::string::npos)
        toUri = toUrl;
    else
        toUri = account->getToUri(toUrl);

    call->setPeerNumber(toUri);
    std::string localAddr(getInterfaceAddrFromName(account->getLocalInterface()));

    if (localAddr == "0.0.0.0")
        localAddr = getSIPLocalIP();

    setCallMediaLocal(call, localAddr);

    // May use the published address as well
    std::string addrSdp = account->isStunEnabled() ?
    account->getPublishedAddress() :
    getInterfaceAddrFromName(account->getLocalInterface());

    if (addrSdp == "0.0.0.0")
        addrSdp = getSIPLocalIP();

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::Codec* audiocodec = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    if (audiocodec == NULL) {
        delete call;
        throw VoipLinkException("Could not instantiate codec for early media");
    }

    try {
        call->getAudioRtp().initAudioRtpConfig();
        call->getAudioRtp().initAudioSymmetricRtpSession();
        call->getAudioRtp().initLocalCryptoInfo();
        call->getAudioRtp().start(static_cast<sfl::AudioCodec *>(audiocodec));
    } catch (...) {
        delete call;
        throw VoipLinkException("Could not start rtp session for early media");
    }

    call->initRecFilename(toUrl);

    call->getLocalSDP()->setLocalIP(addrSdp);
    call->getLocalSDP()->createOffer(account->getActiveCodecs());

    if (!SIPStartCall(call)) {
        delete call;
        throw VoipLinkException("Could not send outgoing INVITE request for new call");
    }

    return call;
}

void
SIPVoIPLink::answer(Call *c)
{
    SIPCall *call = dynamic_cast<SIPCall*>(c);

    if (!call)
        return;

    pjsip_tx_data *tdata;

    if (pjsip_inv_answer(call->inv, PJSIP_SC_OK, NULL, NULL, &tdata) != PJ_SUCCESS)
        throw VoipLinkException("Could not init invite request answer (200 OK)");

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        throw VoipLinkException("Could not send invite request answer (200 OK)");

    call->setConnectionState(Call::CONNECTED);
    call->setState(Call::ACTIVE);
}

void
SIPVoIPLink::hangup(const std::string& id)
{
    SIPCall* call = getSIPCall(id);

    std::string account_id(Manager::instance().getAccountFromCall(id));
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(account_id));

    if (account == NULL)
        throw VoipLinkException("Could not find account for this call");

    pjsip_inv_session *inv = call->inv;

    if (inv == NULL)
        throw VoipLinkException("No invite session for this call");

    // Looks for sip routes
    if (not account->getServiceRoute().empty()) {
        pjsip_route_hdr *route_set = createRouteSet(account->getServiceRoute(), inv->pool);
        pjsip_dlg_set_route_set(inv->dlg, route_set);
    }

    pjsip_tx_data *tdata = NULL;

    // User hangup current call. Notify peer
    if (pjsip_inv_end_session(inv, 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    inv->mod_data[mod_ua_.id] = NULL;

    if (Manager::instance().isCurrentCall(id))
        call->getAudioRtp().stop();

    removeCall(id);
}

void
SIPVoIPLink::peerHungup(const std::string& id)
{
    SIPCall* call = getSIPCall(id);

    // User hangup current call. Notify peer
    pjsip_tx_data *tdata = NULL;

    if (pjsip_inv_end_session(call->inv, 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    call->inv->mod_data[mod_ua_.id ] = NULL;

    if (Manager::instance().isCurrentCall(id))
        call->getAudioRtp().stop();

    removeCall(id);
}

void
SIPVoIPLink::onhold(const std::string& id)
{
    SIPCall *call = getSIPCall(id);
    call->setState(Call::HOLD);
    call->getAudioRtp().stop();

    Sdp *sdpSession = call->getLocalSDP();

    if (!sdpSession)
        throw VoipLinkException("Could not find sdp session");

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");
    sdpSession->addAttributeToLocalAudioMedia("sendonly");

    SIPSessionReinvite(call);
}

void
SIPVoIPLink::offhold(const std::string& id)
{
    SIPCall *call = getSIPCall(id);

    Sdp *sdpSession = call->getLocalSDP();

    if (sdpSession == NULL)
        throw VoipLinkException("Could not find sdp session");

    try {
        int pl = PAYLOAD_CODEC_ULAW;
        sfl::Codec *sessionMedia = sdpSession->getSessionMedia();

        if (sessionMedia)
            pl = sessionMedia->getPayloadType();

        // Create a new instance for this codec
        sfl::Codec* audiocodec = Manager::instance().audioCodecFactory.instantiateCodec(pl);

        if (audiocodec == NULL)
            throw VoipLinkException("Could not instantiate codec");

        call->getAudioRtp().initAudioRtpConfig();
        call->getAudioRtp().initAudioSymmetricRtpSession();
        call->getAudioRtp().start(static_cast<sfl::AudioCodec *>(audiocodec));
    } catch (const SdpException &e) {
        ERROR("UserAgent: Exception: %s", e.what());
    } catch (...) {
        throw VoipLinkException("Could not create audio rtp session");
    }

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");
    sdpSession->addAttributeToLocalAudioMedia("sendrecv");

    if (SIPSessionReinvite(call) == PJ_SUCCESS)
        call->setState(Call::ACTIVE);
}

void
SIPVoIPLink::sendTextMessage(sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from)
{
    SIPCall *call;

    try {
        call = getSIPCall(callID);
    } catch (const VoipLinkException &e) {
        return;
    }

    /* Send IM message */
    sfl::InstantMessaging::UriList list;
    sfl::InstantMessaging::UriEntry entry;
    entry[sfl::IM_XML_URI] = std::string("\"" + from + "\"");  // add double quotes for xml formating

    list.push_front(entry);

    module->send_sip_message(call->inv, callID, module->appendUriList(message, list));
}

bool
SIPVoIPLink::transferCommon(SIPCall *call, pj_str_t *dst)
{
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
    SIPCall *call = getSIPCall(id);
    call->stopRecording();

    std::string account_id(Manager::instance().getAccountFromCall(id));
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(account_id));

    if (account == NULL)
        throw VoipLinkException("Could not find account");

    std::string toUri;
    pj_str_t dst = { 0, 0 };

    if (to.find("@") == std::string::npos) {
        toUri = account->getToUri(to);
        pj_cstr(&dst, toUri.c_str());
    }

    if (!transferCommon(getSIPCall(id), &dst))
        throw VoipLinkException("Couldn't transfer");
}

bool SIPVoIPLink::attendedTransfer(const std::string& id, const std::string& to)
{
    pjsip_dialog *target_dlg = getSIPCall(to)->inv->dlg;
    pjsip_uri *uri = (pjsip_uri*) pjsip_uri_get_uri(target_dlg->remote.info->uri);

    char str_dest_buf[PJSIP_MAX_URL_SIZE*2] = { '<' };
    pj_str_t dst = { str_dest_buf, 1 };

    dst.slen += pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri, str_dest_buf+1, sizeof(str_dest_buf)-1);
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

    return transferCommon(getSIPCall(id), &dst);
}

void
SIPVoIPLink::refuse(const std::string& id)
{
    SIPCall *call = getSIPCall(id);

    if (!call->isIncoming() or call->getConnectionState() == Call::CONNECTED)
        return;

    call->getAudioRtp().stop();

    pjsip_tx_data *tdata;

    if (pjsip_inv_end_session(call->inv, PJSIP_SC_DECLINE, NULL, &tdata) != PJ_SUCCESS)
        return;

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure the pointer is NULL in callbacks
    call->inv->mod_data[mod_ua_.id] = NULL;

    removeCall(id);
}

std::string
SIPVoIPLink::getCurrentCodecName(Call *call) const
{
    return dynamic_cast<SIPCall*>(call)->getLocalSDP()->getCodecName();
}

void
SIPVoIPLink::carryingDTMFdigits(const std::string& id, char code)
{
    std::string accountID(Manager::instance().getAccountFromCall(id));
    SIPAccount *account = static_cast<SIPAccount*>(Manager::instance().getAccount(accountID));

    if (account) {
        try {
            dtmfSend(getSIPCall(id), code, account->getDtmfType());
        } catch (const VoipLinkException &e) {
            // don't do anything if call doesn't exist
        }
    }
}

void
SIPVoIPLink::dtmfSend(SIPCall *call, char code, const std::string &dtmf)
{
    if (dtmf == SIPAccount::OVERRTP_STR) {
        call->getAudioRtp().sendDtmfDigit(code - '0');
        return;
    }
    else if (dtmf != SIPAccount::SIPINFO_STR) {
        WARN("SIPVoIPLink: Unknown DTMF type %s, defaulting to %s instead",
             dtmf.c_str(), SIPAccount::SIPINFO_STR);
    }
    // else : dtmf == SIPINFO

    pj_str_t methodName = pj_str((char*) "INFO");
    pjsip_method method;
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    pjsip_tx_data *tdata;

    if (pjsip_dlg_create_request(call->inv->dlg, &method, -1, &tdata) != PJ_SUCCESS)
        return;

    int duration = Manager::instance().voipPreferences.getPulseLength();
    char dtmf_body[1000];
    snprintf(dtmf_body, sizeof dtmf_body - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);

    /* Create "application/dtmf-relay" message body. */
    pj_str_t content = pj_str(dtmf_body);
    pj_str_t type = pj_str((char*) "application");
    pj_str_t subtype = pj_str((char*) "dtmf-relay");
    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &content);

    if (tdata->msg->body == NULL)
        pjsip_tx_data_dec_ref(tdata);
    else
        pjsip_dlg_send_request(call->inv->dlg, tdata, mod_ua_.id, NULL);
}

bool
SIPVoIPLink::SIPStartCall(SIPCall *call)
{
    std::string id(Manager::instance().getAccountFromCall(call->getCallId()));
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(id));

    if (account == NULL)
        return false;

    std::string toUri(call->getPeerNumber()); // expecting a fully well formed sip uri

    // std::string address, port;
    // findLocalAddressFromUri(toUri, account->transport_, address, port);

    pj_str_t pjTo = pj_str((char*) toUri.c_str());

    // Create the from header
    std::string from(account->getFromUri());
    pj_str_t pjFrom = pj_str((char*) from.c_str());

    // Get the contact header
    std::string contact(account->getContactHeader());
    pj_str_t pjContact = pj_str((char*) contact.c_str());

    pjsip_dialog *dialog = NULL;

    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog) != PJ_SUCCESS)
        return false;

    if (pjsip_inv_create_uac(dialog, call->getLocalSDP()->getLocalSdpSession(), 0, &call->inv) != PJ_SUCCESS)
        return false;

    if (not account->getServiceRoute().empty())
        pjsip_dlg_set_route_set(dialog, createRouteSet(account->getServiceRoute(), call->inv->pool));

    pjsip_auth_clt_set_credentials(&dialog->auth_sess, account->getCredentialCount(), account->getCredInfo());

    call->inv->mod_data[mod_ua_.id] = call;

    pjsip_tx_data *tdata;

    if (pjsip_inv_invite(call->inv, &tdata) != PJ_SUCCESS)
        return false;

    pjsip_tpselector *tp = initTransportSelector(account->transport_, call->inv->pool);

    if (pjsip_dlg_set_transport(dialog, tp) != PJ_SUCCESS)
        return false;

    if (pjsip_inv_send_msg(call->inv, tdata) != PJ_SUCCESS)
        return false;

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);
    addCall(call);

    return true;
}

void
SIPVoIPLink::SIPCallServerFailure(SIPCall *call)
{
    std::string id(call->getCallId());
    Manager::instance().callFailure(id);
    removeCall(id);
}

void
SIPVoIPLink::SIPCallClosed(SIPCall *call)
{
    std::string id(call->getCallId());

    if (Manager::instance().isCurrentCall(id))
        call->getAudioRtp().stop();

    Manager::instance().peerHungupCall(id);
    removeCall(id);
}

void
SIPVoIPLink::SIPCallAnswered(SIPCall *call, pjsip_rx_data *rdata UNUSED)
{
    if (call->getConnectionState() != Call::CONNECTED) {
        call->setConnectionState(Call::CONNECTED);
        call->setState(Call::ACTIVE);
        Manager::instance().peerAnsweredCall(call->getCallId());
    }
}


SIPCall*
SIPVoIPLink::getSIPCall(const std::string& id)
{
    SIPCall *result = dynamic_cast<SIPCall*>(getCall(id));

    if (result == NULL)
        throw VoipLinkException("Could not find SIPCall " + id);

    return result;
}

bool SIPVoIPLink::SIPNewIpToIpCall(const std::string& id, const std::string& to)
{
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(IP2IP_PROFILE));

    if (!account)
        return false;

    SIPCall *call = new SIPCall(id, Call::OUTGOING, cp_);
    call->setIPToIP(true);
    call->initRecFilename(to);

    std::string localAddress(getInterfaceAddrFromName(account->getLocalInterface()));

    if (localAddress == "0.0.0.0")
        localAddress = getSIPLocalIP();

    setCallMediaLocal(call, localAddress);

    std::string toUri = account->getToUri(to);
    call->setPeerNumber(toUri);

    sfl::Codec* audiocodec = Manager::instance().audioCodecFactory.instantiateCodec(PAYLOAD_CODEC_ULAW);

    // Audio Rtp Session must be initialized before creating initial offer in SDP session
    // since SDES require crypto attribute.
    call->getAudioRtp().initAudioRtpConfig();
    call->getAudioRtp().initAudioSymmetricRtpSession();
    call->getAudioRtp().initLocalCryptoInfo();
    call->getAudioRtp().start(static_cast<sfl::AudioCodec *>(audiocodec));

    // Building the local SDP offer
    call->getLocalSDP()->setLocalIP(localAddress);
    call->getLocalSDP()->createOffer(account->getActiveCodecs());

    // Init TLS transport if enabled
    if (account->isTlsEnabled()) {
        size_t at = toUri.find("@");
        size_t trns = toUri.find(";transport");
        if (at == std::string::npos or trns == std::string::npos) {
            ERROR("UserAgent: Error \"@\" or \";transport\" not in URI %s", toUri.c_str());
            delete call;
            return false;
        }

        std::string remoteAddr(toUri.substr(at + 1, trns - at - 1));

        if (toUri.find("sips:") != 1) {
            DEBUG("UserAgent: Error \"sips\" scheme required for TLS call");
            delete call;
            return false;
        }

        shutdownSipTransport(account);
        createTlsTransport(account, remoteAddr);

        if (!account->transport_) {
            delete call;
            return false;
        }
    }

    if (!SIPStartCall(call)) {
        delete call;
        return false;
    }

    return true;
}


///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

pj_bool_t stun_sock_on_status_cb(pj_stun_sock *stun_sock UNUSED, pj_stun_sock_op op UNUSED, pj_status_t status)
{
    return status == PJ_SUCCESS;
}

pj_bool_t stun_sock_on_rx_data_cb(pj_stun_sock *stun_sock UNUSED, void *pkt UNUSED, unsigned pkt_len UNUSED, const pj_sockaddr_t *src_addr UNUSED, unsigned addr_len UNUSED)
{
    return PJ_TRUE;
}


pj_status_t SIPVoIPLink::stunServerResolve(SIPAccount *account)
{
    pj_stun_config stunCfg;
    pj_stun_config_init(&stunCfg, &cp_->factory, 0, pjsip_endpt_get_ioqueue(endpt_), pjsip_endpt_get_timer_heap(endpt_));

    static const pj_stun_sock_cb stun_sock_cb = {
        stun_sock_on_rx_data_cb,
        NULL,
        stun_sock_on_status_cb
    };

    pj_stun_sock *stun_sock;
    pj_status_t status = pj_stun_sock_create(&stunCfg, "stunresolve", pj_AF_INET(), &stun_sock_cb, NULL, NULL, &stun_sock);

    pj_str_t stunServer = account->getStunServerName();

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        DEBUG("Error creating STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        return status;
    }

    status = pj_stun_sock_start(stun_sock, &stunServer, account->getStunPort(), NULL);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        DEBUG("Error starting STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        pj_stun_sock_destroy(stun_sock);
    }

    return status;
}


void SIPVoIPLink::createDefaultSipUdpTransport()
{
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(IP2IP_PROFILE));
    createUdpTransport(account);
    assert(account->transport_);

    localUDPTransport_ = account->transport_;
}

void SIPVoIPLink::createTlsListener(SIPAccount *account, pjsip_tpfactory **listener)
{
    pj_sockaddr_in local_addr;
    pj_sockaddr_in_init(&local_addr, 0, 0);
    local_addr.sin_port = pj_htons(account->getTlsListenerPort());

    pj_str_t pjAddress;
    pj_cstr(&pjAddress, PJ_INADDR_ANY);
    pj_sockaddr_in_set_str_addr(&local_addr, &pjAddress);
    std::string localIP(getSIPLocalIP());

    pjsip_host_port a_name = {
        pj_str((char*) localIP.c_str()),
        local_addr.sin_port
    };

    pjsip_tls_transport_start(endpt_, account->getTlsSetting(), &local_addr, &a_name, 1, listener);
}


void SIPVoIPLink::createTlsTransport(SIPAccount *account, std::string remoteAddr)
{
    pj_str_t remote;
    pj_cstr(&remote, remoteAddr.c_str());

    pj_sockaddr_in rem_addr;
    pj_sockaddr_in_init(&rem_addr, &remote, (pj_uint16_t) DEFAULT_SIP_TLS_PORT);

    static pjsip_tpfactory *localTlsListener = NULL; /** The local tls listener */

    if (localTlsListener == NULL)
        createTlsListener(account, &localTlsListener);

    pjsip_endpt_acquire_transport(endpt_, PJSIP_TRANSPORT_TLS, &rem_addr,
                                  sizeof rem_addr, NULL, &account->transport_);
}


void SIPVoIPLink::createSipTransport(SIPAccount *account)
{
    shutdownSipTransport(account);

    if (account->isTlsEnabled()) {
        std::string remoteSipUri(account->getServerUri());
        static const char SIPS_PREFIX[] = "<sips:";
        size_t sips = remoteSipUri.find(SIPS_PREFIX) + (sizeof SIPS_PREFIX) - 1;
        size_t trns = remoteSipUri.find(";transport");
        std::string remoteAddr(remoteSipUri.substr(sips, trns-sips));

        createTlsTransport(account, remoteAddr);
    } else if (account->isStunEnabled())
        createStunTransport(account);
    else
        createUdpTransport(account);

    if (!account->transport_) {
        // Could not create new transport, this transport may already exists
        account->transport_ = transportMap_[account->getLocalPort()];

        if (account->transport_)
            pjsip_transport_add_ref(account->transport_);
        else {
            account->transport_ = localUDPTransport_;
            account->setLocalPort(localUDPTransport_->local_name.port);
        }
    }
}

void SIPVoIPLink::createUdpTransport(SIPAccount *account)
{
    std::string listeningAddress;
    pj_uint16_t listeningPort = account->getLocalPort();

    pj_sockaddr_in bound_addr;
    pj_bzero(&bound_addr, sizeof(bound_addr));
    bound_addr.sin_port = pj_htons(listeningPort);
    bound_addr.sin_family = PJ_AF_INET;

    if (account->getLocalInterface() == "default") {
        listeningAddress = getSIPLocalIP();
        bound_addr.sin_addr.s_addr = pj_htonl(PJ_INADDR_ANY);
    } else {
        listeningAddress = getInterfaceAddrFromName(account->getLocalInterface());
        bound_addr.sin_addr = pj_inet_addr2(listeningAddress.c_str());
    }

    if (!account->getPublishedSameasLocal()) {
        listeningAddress = account->getPublishedAddress();
        listeningPort = account->getPublishedPort();
    }

    // We must specify this here to avoid the IP2IP_PROFILE creating a
    // transport with the name 0.0.0.0 appearing in the via header
    if (account->getAccountID() == IP2IP_PROFILE)
        listeningAddress = getSIPLocalIP();

    if (listeningAddress.empty() or listeningPort == 0)
        return;

    const pjsip_host_port a_name = {
        pj_str((char*) listeningAddress.c_str()),
        listeningPort
    };

    pjsip_udp_transport_start(endpt_, &bound_addr, &a_name, 1, &account->transport_);
    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_)); // dump debug information to stdout

    if (account->transport_)
        transportMap_[account->getLocalPort()] = account->transport_;
}

pjsip_tpselector *SIPVoIPLink::initTransportSelector(pjsip_transport *transport, pj_pool_t *tp_pool) const
{
    assert(transport);
    pjsip_tpselector *tp = (pjsip_tpselector *) pj_pool_zalloc(tp_pool, sizeof(pjsip_tpselector));
    tp->type = PJSIP_TPSELECTOR_TRANSPORT;
    tp->u.transport = transport;
    return tp;
}



void SIPVoIPLink::createStunTransport(SIPAccount *account)
{
    pj_str_t stunServer = account->getStunServerName();
    pj_uint16_t stunPort = account->getStunPort();

    if (stunServerResolve(account) != PJ_SUCCESS) {
        ERROR("Can't resolve STUN server");
        return;
    }

    pj_sock_t sock = PJ_INVALID_SOCKET;

    pj_sockaddr_in boundAddr;

    if (pj_sockaddr_in_init(&boundAddr, &stunServer, 0) != PJ_SUCCESS) {
        ERROR("Can't initialize IPv4 socket on %*s:%i", stunServer.slen, stunServer.ptr, stunPort);
        return;
    }

    if (pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock) != PJ_SUCCESS) {
        ERROR("Can't create or bind socket");
        return;
    }

    // Query the mapped IP address and port on the 'outside' of the NAT
    pj_sockaddr_in pub_addr;

    if (pjstun_get_mapped_addr(&cp_->factory, 1, &sock, &stunServer, stunPort, &stunServer, stunPort, &pub_addr) != PJ_SUCCESS) {
        ERROR("Can't contact STUN server");
        pj_sock_close(sock);
        return;
    }

    pjsip_host_port a_name = {
        pj_str(pj_inet_ntoa(pub_addr.sin_addr)),
        pj_ntohs(pub_addr.sin_port)
    };

    std::string listeningAddress = std::string(a_name.host.ptr, a_name.host.slen);

    account->setPublishedAddress(listeningAddress);
    account->setPublishedPort(a_name.port);

    pjsip_udp_transport_attach2(endpt_, PJSIP_TRANSPORT_UDP, sock, &a_name, 1,
                                &account->transport_);

    pjsip_tpmgr_dump_transports(pjsip_endpt_get_tpmgr(endpt_));
}


void SIPVoIPLink::shutdownSipTransport(SIPAccount *account)
{
    if (account->transport_) {
        pjsip_transport_dec_ref(account->transport_);
        account->transport_ = NULL;
    }
}

void SIPVoIPLink::findLocalAddressFromTransport(pjsip_transport *transport, pjsip_transport_type_e transportType, std::string &addr, std::string &port) const
{
    // Initialize the sip port with the default SIP port
    std::stringstream ss;
    ss << DEFAULT_SIP_PORT;
    port = ss.str();

    // Initialize the sip address with the hostname
    const pj_str_t *pjMachineName = pj_gethostname();
    addr = std::string(pjMachineName->ptr, pjMachineName->slen);

    // Update address and port with active transport
    if (!transport) {
        ERROR("SIPVoIPLink: Transport is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // get the transport manager associated with the SIP enpoint
    pjsip_tpmgr *tpmgr = pjsip_endpt_get_tpmgr(endpt_);
    if (!tpmgr) {
        ERROR("SIPVoIPLink: Transport manager is NULL in findLocalAddress, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // initialize a transport selector
    // TODO Need to determine why we exclude TLS here...
    // if (transportType == PJSIP_TRANSPORT_UDP and transport_)
    pjsip_tpselector *tp_sel = initTransportSelector(transport, pool_);
    if (!tp_sel) {
        ERROR("SIPVoIPLink: Could not initialize transport selector, using local address %s:%s", addr.c_str(), port.c_str());
        return;
    }

    pj_str_t localAddress = {0,0};
    int i_port = 0;

    // Find the local address and port for this transport
    if (pjsip_tpmgr_find_local_addr(tpmgr, pool_, transportType, tp_sel, &localAddress, &i_port) != PJ_SUCCESS) {
        WARN("SIPVoIPLink: Could not retreive local address and port from transport, using %s:%s", addr.c_str(), port.c_str());
        return;
    }

    // Update local address based on the transport type
    addr = std::string(localAddress.ptr, localAddress.slen);

    // Fallback on local ip provided by pj_gethostip()
    if (addr == "0.0.0.0")
        addr = getSIPLocalIP();

    // Determine the local port based on transport information
    ss.str("");
    ss << i_port;
    port = ss.str();
}

namespace {
int SIPSessionReinvite(SIPCall *call)
{
    pjsip_tx_data *tdata;

    pjmedia_sdp_session *local_sdp = call->getLocalSDP()->getLocalSdpSession();

    if (local_sdp && pjsip_inv_reinvite(call->inv, NULL, local_sdp, &tdata) == PJ_SUCCESS)
        return pjsip_inv_send_msg(call->inv, tdata);

    return !PJ_SUCCESS;
}

void invite_session_state_changed_cb(pjsip_inv_session *inv, pjsip_event *e)
{
    SIPCall *call = reinterpret_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);

    if (call == NULL)
        return;

    SIPVoIPLink *link = SIPVoIPLink::instance();

    if (inv->state != PJSIP_INV_STATE_CONFIRMED) {
        // Update UI with the current status code and description
        pjsip_transaction * tsx = e->body.tsx_state.tsx;
        int statusCode = tsx ? tsx->status_code : 404;

        if (statusCode) {
            const pj_str_t * description = pjsip_get_status_text(statusCode);
            Manager::instance().getDbusManager()->getCallManager()->sipCallStateChanged(call->getCallId(), std::string(description->ptr, description->slen), statusCode);
        }
    }

    if (inv->state == PJSIP_INV_STATE_EARLY and e->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
        call->setConnectionState(Call::RINGING);
        Manager::instance().peerRingingCall(call->getCallId());
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
        // After we sent or received a ACK - The connection is established
        link->SIPCallAnswered(call, e->body.tsx_state.src.rdata);
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        std::string accId(Manager::instance().getAccountFromCall(call->getCallId()));

        switch (inv->cause) {
                // The call terminates normally - BYE / CANCEL
            case PJSIP_SC_OK:
            case PJSIP_SC_REQUEST_TERMINATED:
                link->SIPCallClosed(call);
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
                link->SIPCallServerFailure(call);
                break;
        }
    }
}

void sdp_request_offer_cb(pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
    SIPCall *call = (SIPCall*) inv->mod_data[mod_ua_.id ];

    if (!call)
        return;

    std::string accId(Manager::instance().getAccountFromCall(call->getCallId()));
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(accId));

    call->getLocalSDP()->receiveOffer(offer, account->getActiveCodecs());
    call->getLocalSDP()->startNegotiation();

    pjsip_inv_set_sdp_answer(call->inv, call->getLocalSDP()->getLocalSdpSession());
}

void sdp_create_offer_cb(pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    SIPCall *call = reinterpret_cast<SIPCall*>(inv->mod_data[mod_ua_.id]);
    std::string accountid(Manager::instance().getAccountFromCall(call->getCallId()));

    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(accountid));

    std::string localAddress(SIPVoIPLink::instance()->getInterfaceAddrFromName(account->getLocalInterface()));
    std::string addrSdp(localAddress);

    if (localAddress == "0.0.0.0")
        localAddress = getSIPLocalIP();

    if (addrSdp == "0.0.0.0")
        addrSdp = localAddress;

    setCallMediaLocal(call, localAddress);

    call->getLocalSDP()->setLocalIP(addrSdp);
    call->getLocalSDP()->createOffer(account->getActiveCodecs());

    *p_offer = call->getLocalSDP()->getLocalSdpSession();
}

// This callback is called after SDP offer/answer session has completed.
void sdp_media_update_cb(pjsip_inv_session *inv, pj_status_t status)
{
    const pjmedia_sdp_session *remote_sdp;
    const pjmedia_sdp_session *local_sdp;

    SIPCall *call = reinterpret_cast<SIPCall *>(inv->mod_data[mod_ua_.id]);

    if (call == NULL) {
        DEBUG("UserAgent: Call declined by peer, SDP negotiation stopped");
        return;
    }

    if (status != PJ_SUCCESS) {
        WARN("UserAgent: Error: while negotiating the offer");
        SIPVoIPLink::instance()->hangup(call->getCallId());
        Manager::instance().callFailure(call->getCallId());
        return;
    }

    if (!inv->neg) {
        WARN("UserAgent: Error: no negotiator for this session");
        return;
    }

    // Retreive SDP session for this call
    Sdp *sdpSession = call->getLocalSDP();

    // Get active session sessions
    pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    // Print SDP session
    char buffer[1000];
    memset(buffer, 0, sizeof buffer);
    pjmedia_sdp_print(remote_sdp, buffer, 1000);
    DEBUG("SDP: Remote active SDP Session:\n%s", buffer);

    memset(buffer, 0, 1000);
    pjmedia_sdp_print(local_sdp, buffer, 1000);
    DEBUG("SDP: Local active SDP Session:\n%s", buffer);

    // Set active SDP sessions
    sdpSession->setActiveRemoteSdpSession(remote_sdp);
    sdpSession->setActiveLocalSdpSession(local_sdp);

    // Update internal field for
    sdpSession->setMediaTransportInfoFromRemoteSdp();

    call->getAudioRtp().updateDestinationIpAddress();
    call->getAudioRtp().setDtmfPayloadType(sdpSession->getTelephoneEventType());

    // Get the crypto attribute containing srtp's cryptographic context (keys, cipher)
    CryptoOffer crypto_offer;
    call->getLocalSDP()->getRemoteSdpCryptoFromOffer(remote_sdp, crypto_offer);

    bool nego_success = false;

    if (!crypto_offer.empty()) {
        std::vector<sfl::CryptoSuiteDefinition>localCapabilities;

        for (int i = 0; i < 3; i++)
            localCapabilities.push_back(sfl::CryptoSuites[i]);

        sfl::SdesNegotiator sdesnego(localCapabilities, crypto_offer);

        if (sdesnego.negotiate()) {
            DEBUG("UserAgent: SDES negotiation successfull");
            nego_success = true;

            try {
                call->getAudioRtp().setRemoteCryptoInfo(sdesnego);
            } catch (...) {}

            Manager::instance().getDbusManager()->getCallManager()->secureSdesOn(call->getCallId());
        } else {
            Manager::instance().getDbusManager()->getCallManager()->secureSdesOff(call->getCallId());
        }
    }


    // We did not found any crypto context for this media, RTP fallback
    if (!nego_success && call->getAudioRtp().isSdesEnabled()) {
        call->getAudioRtp().stop();
        call->getAudioRtp().setSrtpEnabled(false);

        std::string accountID = Manager::instance().getAccountFromCall(call->getCallId());

        if (((SIPAccount *) Manager::instance().getAccount(accountID))->getSrtpFallback())
            call->getAudioRtp().initAudioSymmetricRtpSession();
    }

    if (!sdpSession)
        return;

    sfl::AudioCodec *sessionMedia = sdpSession->getSessionMedia();

    if (!sessionMedia)
        return;

    try {
        Manager::instance().audioLayerMutexLock();
        Manager::instance().getAudioDriver()->startStream();
        Manager::instance().audioLayerMutexUnlock();

        int pl = sessionMedia->getPayloadType();

        if (pl != call->getAudioRtp().getSessionMedia()) {
            sfl::Codec* audiocodec = Manager::instance().audioCodecFactory.instantiateCodec(pl);
            call->getAudioRtp().updateSessionMedia(static_cast<sfl::AudioCodec *>(audiocodec));
        }
    } catch (const SdpException &e) {
        ERROR("UserAgent: Exception: %s", e.what());
    } catch (const std::exception& rtpException) {
        ERROR("UserAgent: Exception: %s", rtpException.what());
    }

}

void outgoing_request_forked_cb(pjsip_inv_session *inv UNUSED, pjsip_event *e UNUSED)
{
}

void transaction_state_changed_cb(pjsip_inv_session *inv UNUSED, pjsip_transaction *tsx, pjsip_event *e)
{
    assert(tsx);
    assert(e);

    if (tsx->role != PJSIP_ROLE_UAS || tsx->state != PJSIP_TSX_STATE_TRYING)
        return;

    if (pjsip_method_cmp(&tsx->method, &pjsip_refer_method) ==0) {
        onCallTransfered(inv, e->body.tsx_state.src.rdata);          /** Handle the refer method **/
        return;
    }

    pjsip_tx_data* t_data;

    if (e->body.rx_msg.rdata) {
        pjsip_rx_data *r_data = e->body.rx_msg.rdata;

        if (r_data && r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {
            std::string request =  pjsip_rx_data_get_info(r_data);
            DEBUG("UserAgent: %s", request.c_str());

            if (request.find("NOTIFY") == std::string::npos && request.find("INFO") != std::string::npos) {
                pjsip_dlg_create_response(inv->dlg, r_data, PJSIP_SC_OK, NULL, &t_data);
                pjsip_dlg_send_response(inv->dlg, tsx, t_data);
                return;
            }
        }
    }

    if (!e->body.tsx_state.src.rdata)
        return;

    // Incoming TEXT message

    // Get the message inside the transaction
    pjsip_rx_data *r_data = e->body.tsx_state.src.rdata;
    std::string formatedMessage = (char*) r_data->msg_info.msg->body->data;

    // Try to determine who is the recipient of the message
    SIPCall *call = reinterpret_cast<SIPCall *>(inv->mod_data[mod_ua_.id]);

    if (!call)
        return;

    // Respond with a 200/OK
    pjsip_dlg_create_response(inv->dlg, r_data, PJSIP_SC_OK, NULL, &t_data);
    pjsip_dlg_send_response(inv->dlg, tsx, t_data);

    sfl::InstantMessaging *module = Manager::instance().getInstantMessageModule();

    try {
        // retreive the recipient-list of this message
        std::string urilist = module->findTextUriList(formatedMessage);
        sfl::InstantMessaging::UriList list = module->parseXmlUriList(urilist);

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
        if (from[0] == '<' && from[from.size()-1] == '>')
            from = from.substr(1, from.size()-2);

        Manager::instance().incomingMessage(call->getCallId(), from, module->findTextMessage(formatedMessage));

    } catch (const sfl::InstantMessageException &except) {
        ERROR("SipVoipLink: %s", except.what());
    }
}

void update_contact_header(struct pjsip_regc_cbparam *param, SIPAccount *account)
{

    SIPVoIPLink *siplink = dynamic_cast<SIPVoIPLink *>(account->getVoIPLink());
    if(siplink == NULL) {
        ERROR("SIPVoIPLink: Could not find voip link from account");
        return;
    }

    pj_pool_t *pool = pj_pool_create(&cp_->factory, "tmp", 512, 512, NULL);
    if(pool == NULL) {
        ERROR("SIPVoIPLink: Could not create temporary memory pool in transport header");
        return;
    }

    if (param->contact_cnt == 0) {
        WARN("SIPVoIPLink: No contact header in registration callback");
        pj_pool_release(pool);
        return;
    }

    pjsip_contact_hdr *contact_hdr = param->contact[0];

    pjsip_sip_uri *uri = (pjsip_sip_uri*) contact_hdr->uri;
    if (uri == NULL) {
        ERROR("SIPVoIPLink: Could not find uri in contact header");
        pj_pool_release(pool);
        return;
    }

    // TODO: make this based on transport type
    // with pjsip_transport_get_default_port_for_type(tp_type);
    if (uri->port == 0)
        uri->port = DEFAULT_SIP_PORT;

    std::string recvContactHost(uri->host.ptr, uri->host.slen);
    std::stringstream ss;
    ss << uri->port;
    std::string recvContactPort = ss.str();

    std::string currentAddress, currentPort;
    siplink->findLocalAddressFromTransport(account->transport_, PJSIP_TRANSPORT_UDP, currentAddress, currentPort);

    bool updateContact = false;
    std::string currentContactHeader = account->getContactHeader();

    size_t foundHost = currentContactHeader.find(recvContactHost);
    if(foundHost == std::string::npos) {
        updateContact = true;
    }

    size_t foundPort = currentContactHeader.find(recvContactPort);
    if(foundPort == std::string::npos) {
        updateContact = true;
    }

    if(updateContact) {
        DEBUG("SIPVoIPLink: Update contact header: %s:%s\n", recvContactHost.c_str(), recvContactPort.c_str());
        account->setContactHeader(recvContactHost, recvContactPort);
        siplink->sendRegister(account);
    }
    pj_pool_release(pool);
}

void registration_cb(struct pjsip_regc_cbparam *param)
{
    SIPAccount *account = static_cast<SIPAccount *>(param->token);

    if (account == NULL) {
        ERROR("SipVoipLink: account does'nt exist in registration callback");
        return;
    }

    if (param == NULL) {
        ERROR("SipVoipLink: regsitration callback param is NULL");
        return;
    }

    if(account->isContactUpdateEnabled()) {
        update_contact_header(param, account);
    }

    const pj_str_t *description = pjsip_get_status_text(param->code);

    if (param->code && description) {
        std::string state(description->ptr, description->slen);
        Manager::instance().getDbusManager()->getCallManager()->registrationStateChanged(account->getAccountID(), state, param->code);
        std::pair<int, std::string> details(param->code, state);
        // TODO: there id a race condition for this ressource when closing the application
        account->setRegistrationStateDetailed(details);

        account->setRegistrationExpire(param->expiration);
    }

    if (param->status != PJ_SUCCESS) {
        account->setRegistrationState(ErrorAuth);
        account->setRegister(false);

        SIPVoIPLink::instance()->shutdownSipTransport(account);
        return;
    }

    if (param->code < 0 || param->code >= 300) {
        switch (param->code) {
            case 606:
                account->setRegistrationState(ErrorConfStun);
                break;

            case 503:
            case 408:
                account->setRegistrationState(ErrorHost);
                break;

            case 401:
            case 403:
            case 404:
                account->setRegistrationState(ErrorAuth);
                break;

            case 423: { // Expiration Interval Too Brief
                account->doubleRegistrationExpire();
                account->registerVoIPLink();
            }
            break;

            default:
                account->setRegistrationState(Error);
                break;
        }

        account->setRegister(false);

        SIPVoIPLink::instance()->shutdownSipTransport(account);

    } else {
        if (account->isRegistered())
            account->setRegistrationState(Registered);
        else {
            account->setRegistrationState(Unregistered);
            SIPVoIPLink::instance()->shutdownSipTransport(account);
        }
    }
}

void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    SIPCall *currentCall = reinterpret_cast<SIPCall *>(inv->mod_data[mod_ua_.id]);

    if (currentCall == NULL)
        return;

    static const pj_str_t str_refer_to = { (char*) "Refer-To", 8};
    pjsip_generic_string_hdr *refer_to = static_cast<pjsip_generic_string_hdr*>
                                         (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL));

    if (!refer_to) {
        pjsip_dlg_respond(inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    SIPVoIPLink::instance()->newOutgoingCall(Manager::instance().getNewCallID(), std::string(refer_to->hvalue.ptr, refer_to->hvalue.slen));
    Manager::instance().hangupCall(currentCall->getCallId());
}

void transfer_client_cb(pjsip_evsub *sub, pjsip_event *event)
{
    switch (pjsip_evsub_get_state(sub)) {
        case PJSIP_EVSUB_STATE_ACCEPTED:
            pj_assert(event->type == PJSIP_EVENT_TSX_STATE && event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
            break;

        case PJSIP_EVSUB_STATE_TERMINATED:
            pjsip_evsub_set_mod_data(sub, mod_ua_.id, NULL);
            break;

        case PJSIP_EVSUB_STATE_ACTIVE: {

            SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *>(pjsip_evsub_get_mod_data(sub, mod_ua_.id));

            if (!link or !event)
                return;

            pjsip_rx_data* r_data = event->body.rx_msg.rdata;

            if (!r_data)
                return;

            std::string request(pjsip_rx_data_get_info(r_data));

            pjsip_status_line status_line = { 500, *pjsip_get_status_text(500) };

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

            std::string transferID(r_data->msg_info.cid->id.ptr, r_data->msg_info.cid->id.slen);
            SIPCall *call = dynamic_cast<SIPCall *>(link->getCall(transferCallID[transferID]));

            if (!call)
                return;

            if (status_line.code/100 == 2) {
                pjsip_tx_data *tdata;

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

void setCallMediaLocal(SIPCall* call, const std::string &localIP)
{
    std::string account_id(Manager::instance().getAccountFromCall(call->getCallId()));
    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(account_id));

    unsigned int callLocalAudioPort = ((rand() % 27250) + 5250) * 2;

    unsigned int callLocalExternAudioPort = account->isStunEnabled()
                                            ? account->getStunPort()
                                            : callLocalAudioPort;

    call->setLocalIp(localIP);
    call->setLocalAudioPort(callLocalAudioPort);
    call->getLocalSDP()->setLocalPublishedAudioPort(callLocalExternAudioPort);
}

std::string fetchHeaderValue(pjsip_msg *msg, const std::string &field)
{
    pj_str_t name = pj_str((char*) field.c_str());

    pjsip_generic_string_hdr *hdr = (pjsip_generic_string_hdr*) pjsip_msg_find_hdr_by_name(msg, &name, NULL);

    if (!hdr)
        return "";

    std::string value(std::string(hdr->hvalue.ptr, hdr->hvalue.slen));

    size_t pos = value.find("\n");

    if (pos == std::string::npos)
        return "";

    return value.substr(0, pos);
}
} // end anonymous namespace

std::vector<std::string> SIPVoIPLink::getAllIpInterfaceByName()
{
    static ifreq ifreqs[20];
    ifconf ifconf;

    std::vector<std::string> ifaceList;
    ifaceList.push_back("default");

    ifconf.ifc_buf = (char*)(ifreqs);
    ifconf.ifc_len = sizeof(ifreqs);

    int sock = socket(AF_INET,SOCK_STREAM,0);

    if (sock >= 0) {
        if (ioctl(sock, SIOCGIFCONF, &ifconf) >= 0)
            for (unsigned i = 0; i < ifconf.ifc_len/sizeof(struct ifreq); i++)
                ifaceList.push_back(std::string(ifreqs[i].ifr_name));

        close(sock);
    }

    return ifaceList;
}

std::string SIPVoIPLink::getInterfaceAddrFromName(const std::string &ifaceName)
{
    int fd = socket(AF_INET, SOCK_DGRAM,0);

    if (fd < 0) {
        ERROR("UserAgent: Error: could not open socket: %m");
        return "";
    }

    ifreq ifr;
    strcpy(ifr.ifr_name, ifaceName.c_str());
    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    ifr.ifr_addr.sa_family = AF_INET;

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    sockaddr_in *saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    return inet_ntoa(saddr_in->sin_addr);
}

std::vector<std::string> SIPVoIPLink::getAllIpInterface()
{
    pj_sockaddr addrList[16];
    unsigned addrCnt = PJ_ARRAY_SIZE(addrList);

    std::vector<std::string> ifaceList;

    if (pj_enum_ip_interface(pj_AF_INET(), &addrCnt, addrList) == PJ_SUCCESS)
        for (unsigned i = 0; i < addrCnt; i++) {
            char addr[PJ_INET_ADDRSTRLEN];
            pj_sockaddr_print(&addrList[i], addr, sizeof(addr), 0);
            ifaceList.push_back(std::string(addr));
        }

    return ifaceList;
}
