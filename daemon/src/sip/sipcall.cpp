/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "sipcall.h"
#include "sip_utils.h"
#include "logger.h" // for _debug
#include "sdp.h"
#include "manager.h"
#include "array_size.h"

#include "audio/audiortp/audio_rtp_factory.h" // for AudioRtpFactoryException

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

#ifdef SFL_VIDEO
#include "client/videomanager.h"

static sfl_video::VideoSettings
getSettings()
{
    const auto videoman = Manager::instance().getClient()->getVideoManager();
    return videoman->getSettings(videoman->getDefaultDevice());
}
#endif
#include "sipvoiplink.h"

static const int INITIAL_SIZE = 16384;
static const int INCREMENT_SIZE = INITIAL_SIZE;

/** A map to retreive SFLphone internal call id
 *  Given a SIP call ID (usefull for transaction sucha as transfer)*/
static std::map<std::string, std::string> transferCallID;

static void
stopRtpIfCurrent(SIPCall& call)
{
    if (Manager::instance().isCurrentCall(call.getCallId())) {
        call.getAudioRtp().stop();
#ifdef SFL_VIDEO
        call.getVideoRtp().stop();
#endif
    }
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

SIPCall::SIPCall(const std::string& id, Call::CallType type,
                 SIPAccount& account) :
    Call(id, type, account)
    , inv(NULL)
    , audiortp_(this)
#ifdef SFL_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(id, getSettings())
#endif
    , pool_(pj_pool_create(&SIPVoIPLink::instance().getCachingPool()->factory,
                           id.c_str(), INITIAL_SIZE, INCREMENT_SIZE, NULL))
    , local_sdp_(new Sdp(pool_))
    , contactBuffer_()
    , contactHeader_{contactBuffer_, 0}
{}

SIPCall::~SIPCall()
{
    delete local_sdp_;
    pj_pool_release(pool_);
}

SIPAccount&
SIPCall::getSIPAccount() const
{
    return static_cast<SIPAccount&>(getAccount());
}

void
SIPCall::setCallMediaLocal(const pj_sockaddr& localIP)
{
    auto& account = getSIPAccount();

    // Reference: http://www.cs.columbia.edu/~hgs/rtp/faq.html#ports
    // We only want to set ports to new values if they haven't been set
    if (getLocalAudioPort() == 0) {
        const unsigned callLocalAudioPort = account.generateAudioPort();
        setLocalAudioPort(callLocalAudioPort);
        getLocalSDP()->setLocalPublishedAudioPort(callLocalAudioPort);
    }

    setLocalIp(localIP);

#ifdef SFL_VIDEO
    if (getLocalVideoPort() == 0) {
        // https://projects.savoirfairelinux.com/issues/17498
        const unsigned int callLocalVideoPort = account.generateVideoPort();
        // this should already be guaranteed by SIPAccount
        assert(getLocalAudioPort() != callLocalVideoPort);

        setLocalVideoPort(callLocalVideoPort);
        getLocalSDP()->setLocalPublishedVideoPort(callLocalVideoPort);
    }
#endif
}

void SIPCall::setContactHeader(pj_str_t *contact)
{
    pj_strcpy(&contactHeader_, contact);
}

std::map<std::string, std::string>
SIPCall::createHistoryEntry() const
{
    using sfl::HistoryItem;

    std::map<std::string, std::string> entry(Call::createHistoryEntry());
    return entry;
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 * @param sip call
 */

static int
SIPSessionReinvite(SIPCall *call)
{
    pjmedia_sdp_session *local_sdp = call->getLocalSDP()->getLocalSdpSession();
    pjsip_tx_data *tdata;

    if (local_sdp and call->inv and call->inv->pool_prov and
            pjsip_inv_reinvite(call->inv, NULL, local_sdp, &tdata) == PJ_SUCCESS)
        return pjsip_inv_send_msg(call->inv, tdata);

    return !PJ_SUCCESS;
}

VoIPLink*
SIPCall::getVoIPLink() const
{ return &SIPVoIPLink::instance(); }

void
SIPCall::sendSIPInfo(const char *const body, const char *const subtype)
{
    pj_str_t methodName = CONST_PJ_STR("INFO");
    pjsip_method method;
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    pjsip_tx_data *tdata;

    if (pjsip_dlg_create_request(inv->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
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
        pjsip_dlg_send_request(inv->dlg, tdata, SIPVoIPLink::instance().getMod()->id, NULL);
}

void
SIPCall::updateSDPFromSTUN()
{
    auto& account = getSIPAccount();
    std::vector<long> socketDescriptors(getAudioRtp().getSocketDescriptors());

    try {
        std::vector<pj_sockaddr> stunPorts(SIPVoIPLink::instance().sipTransport->getSTUNAddresses(account, socketDescriptors));

        // FIXME: get video sockets
        //stunPorts.resize(4);

        account.setPublishedAddress(stunPorts[0]);
        // published IP MUST be updated first, since RTCP depends on it
        getLocalSDP()->setPublishedIP(account.getPublishedAddress());
        getLocalSDP()->updatePorts(stunPorts);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }
}

void SIPCall::answer()
{
    auto& account = getSIPAccount();

    if (!inv->neg) {
        SIPVoIPLink& siplink = SIPVoIPLink::instance();

        WARN("Negotiator is NULL, we've received an INVITE without an SDP");
        pjmedia_sdp_session *dummy = 0;
        siplink.createSDPOffer(inv, &dummy);

        if (account.isStunEnabled())
            updateSDPFromSTUN();
    }

    pj_str_t contact(account.getContactHeader());
    setContactHeader(&contact);

    pjsip_tx_data *tdata;
    if (!inv->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(inv, PJSIP_SC_OK, NULL, !inv->neg ? local_sdp_->getLocalSdpSession() : NULL, &tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    // contactStr must stay in scope as long as tdata
    if (contactHeader_.slen) {
        DEBUG("Answering with contact header: %.*s", contactHeader_.slen, contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not send invite request answer (200 OK)");

    setConnectionState(CONNECTED);
    setState(ACTIVE);
}

void
SIPCall::hangup(int reason)
{
    auto& account = getSIPAccount();

    if (not inv)
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
    const pj_str_t contactStr(account.getContactHeader());
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    auto& siplink = SIPVoIPLink::instance();

    // Make sure user data is NULL in callbacks
    inv->mod_data[siplink.getMod()->id] = NULL;

    // Stop all RTP streams
    stopRtpIfCurrent(*this);

    siplink.removeSipCall(getCallId());
}

void
SIPCall::refuse()
{
    if (!isIncoming() or getConnectionState() == Call::CONNECTED or !inv)
        return;

    getAudioRtp().stop();

    pjsip_tx_data *tdata;

    if (pjsip_inv_end_session(inv, PJSIP_SC_DECLINE, NULL, &tdata) != PJ_SUCCESS)
        return;

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    auto& siplink = SIPVoIPLink::instance();

    // Make sure the pointer is NULL in callbacks
    inv->mod_data[siplink.getMod()->id] = NULL;

    siplink.removeSipCall(getCallId());
}

static void
transfer_client_cb(pjsip_evsub *sub, pjsip_event *event)
{
    auto mod_ua_id = SIPVoIPLink::instance().getMod()->id;

    switch (pjsip_evsub_get_state(sub)) {
        case PJSIP_EVSUB_STATE_ACCEPTED:
            if (!event)
                return;

            pj_assert(event->type == PJSIP_EVENT_TSX_STATE && event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
            break;

        case PJSIP_EVSUB_STATE_TERMINATED:
            pjsip_evsub_set_mod_data(sub, mod_ua_id, NULL);
            break;

        case PJSIP_EVSUB_STATE_ACTIVE: {
            if (!event)
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

            auto call = static_cast<SIPCall *>(pjsip_evsub_get_mod_data(sub, mod_ua_id));
            if (!call)
                return;

            if (status_line.code / 100 == 2) {
                pjsip_tx_data *tdata;

                if (!call->inv)
                    return;

                if (pjsip_inv_end_session(call->inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS)
                    pjsip_inv_send_msg(call->inv, tdata);

                Manager::instance().hangupCall(call->getCallId());
                pjsip_evsub_set_mod_data(sub, mod_ua_id, NULL);
            }

            break;
        }

        default:
            break;
    }
}

bool
SIPCall::transferCommon(pj_str_t *dst)
{
    if (!inv)
        return false;

    pjsip_evsub_user xfer_cb;
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    pjsip_evsub *sub;

    if (pjsip_xfer_create_uac(inv->dlg, &xfer_cb, &sub) != PJ_SUCCESS)
        return false;

    auto& siplink = SIPVoIPLink::instance();

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data(sub, siplink.getMod()->id, this);

    /*
     * Create REFER request.
     */
    pjsip_tx_data *tdata;

    if (pjsip_xfer_initiate(sub, dst, &tdata) != PJ_SUCCESS)
        return false;

    // Put SIP call id in map in order to retrieve call during transfer callback
    std::string callidtransfer(inv->dlg->call_id->id.ptr, inv->dlg->call_id->id.slen);
    transferCallID[callidtransfer] = getCallId();

    /* Send. */
    if (pjsip_xfer_send_request(sub, tdata) != PJ_SUCCESS)
        return false;

    return true;
}

void
SIPCall::transfer(const std::string& to)
{
    auto& account = getSIPAccount();

    stopRecording();

    std::string toUri;
    pj_str_t dst = { 0, 0 };

    toUri = account.getToUri(to);
    pj_cstr(&dst, toUri.c_str());
    DEBUG("Transferring to %.*s", dst.slen, dst.ptr);

    if (!transferCommon(&dst))
        throw VoipLinkException("Couldn't transfer");
}

bool
SIPCall::attendedTransfer(const std::string& /*to*/)
{
    if (!inv or !inv->dlg)
        throw VoipLinkException("Couldn't get invite dialog");

    pjsip_dialog *target_dlg = inv->dlg;
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

    return transferCommon(&dst);
}

void
SIPCall::onhold()
{
    if (not setState(Call::HOLD))
        return;

    audiortp_.saveLocalContext();
    audiortp_.stop();
#ifdef SFL_VIDEO
    videortp_.stop();
#endif

    if (!local_sdp_)
        throw SdpException("Could not find sdp session");

    local_sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    local_sdp_->addAttributeToLocalAudioMedia("sendonly");

#ifdef SFL_VIDEO
    local_sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalVideoMedia("inactive");
    local_sdp_->addAttributeToLocalVideoMedia("inactive");
#endif

    if (SIPSessionReinvite(this) != PJ_SUCCESS)
        WARN("Reinvite failed");
}

void
SIPCall::offhold()
{
    auto& account = getSIPAccount();

    try {
        if (account.isStunEnabled())
            internalOffHold([&] { updateSDPFromSTUN(); });
        else
            internalOffHold([] {});

    } catch (const SdpException &e) {
        ERROR("%s", e.what());
        throw VoipLinkException("SDP issue in offhold");
    } catch (const ost::Socket::Error &e) {
        throw VoipLinkException("Socket problem in offhold");
    } catch (const ost::Socket *) {
        throw VoipLinkException("Socket problem in offhold");
    } catch (const sfl::AudioRtpFactoryException &) {
        throw VoipLinkException("Socket problem in offhold");
    }
}

void
SIPCall::internalOffHold(const std::function<void()> &SDPUpdateFunc)
{
    if (not setState(Call::ACTIVE))
        return;

    if (local_sdp_ == NULL)
        throw SdpException("Could not find sdp session");

    std::vector<sfl::AudioCodec*> sessionMedia(local_sdp_->getSessionAudioMedia());

    if (sessionMedia.empty()) {
        WARN("Session media is empty");
        return;
    }

    std::vector<sfl::AudioCodec*> audioCodecs;

    for (auto & i : sessionMedia) {

        if (!i)
            continue;

        // Create a new instance for this codec
        sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(i->getPayloadType());

        if (ac == NULL) {
            ERROR("Could not instantiate codec %d", i->getPayloadType());
            throw std::runtime_error("Could not instantiate codec");
        }

        audioCodecs.push_back(ac);
    }

    if (audioCodecs.empty()) {
        throw std::runtime_error("Could not instantiate any codecs");
    }

    audiortp_.initConfig();
    audiortp_.initSession();

    // Invoke closure
    SDPUpdateFunc();

    audiortp_.restoreLocalContext();
    audiortp_.initLocalCryptoInfoOnOffHold();
    audiortp_.start(audioCodecs);

    local_sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    local_sdp_->addAttributeToLocalAudioMedia("sendrecv");

#ifdef SFL_VIDEO
    local_sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalVideoMedia("sendonly");
    local_sdp_->removeAttributeFromLocalVideoMedia("inactive");
    local_sdp_->addAttributeToLocalVideoMedia("sendrecv");
#endif

    if (SIPSessionReinvite(this) != PJ_SUCCESS) {
        WARN("Reinvite failed, resuming hold");
        onhold();
    }
}

void
SIPCall::peerHungup()
{
    // User hangup current call. Notify peer
    pjsip_tx_data *tdata = NULL;

    if (pjsip_inv_end_session(inv, 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    auto& siplink = SIPVoIPLink::instance();

    // Make sure user data is NULL in callbacks
    inv->mod_data[siplink.getMod()->id ] = NULL;

    // Stop all RTP streams
    stopRtpIfCurrent(*this);

    siplink.removeSipCall(getCallId());
}

void
SIPCall::carryingDTMFdigits(char code)
{
    dtmfSend(*this, code, getSIPAccount().getDtmfType());
}

#if HAVE_INSTANT_MESSAGING
void
SIPCall::sendTextMessage(const std::string &message, const std::string &from)
{
    using namespace sfl::InstantMessaging;

    /* Send IM message */
    UriList list;
    UriEntry entry;
    entry[sfl::IM_XML_URI] = std::string("\"" + from + "\"");  // add double quotes for xml formating
    list.push_front(entry);
    send_sip_message(inv, getCallId(), appendUriList(message, list));
}
#endif // HAVE_INSTANT_MESSAGING
