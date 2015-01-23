/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "call_factory.h"
#include "sipcall.h"
#include "sipaccount.h" // for SIPAccount::ACCOUNT_TYPE
#include "sipaccountbase.h"
#include "sipvoiplink.h"
#include "sip_utils.h"
#include "logger.h" // for _debug
#include "sdp.h"
#include "manager.h"
#include "array_size.h"

using namespace ring;


#include "audio/audiortp/avformat_rtp_session.h"
#include "client/callmanager.h"

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

#ifdef RING_VIDEO
#include "video/video_rtp_session.h"
#include "client/videomanager.h"

#include <chrono>

static video::VideoSettings
getSettings()
{
    const auto videoman = Manager::instance().getClient()->getVideoManager();
    return videoman->getSettings(videoman->getDefaultDevice());
}
#endif

static constexpr int DEFAULT_ICE_INIT_TIMEOUT {10}; // seconds
static constexpr int DEFAULT_ICE_NEGO_TIMEOUT {60}; // seconds

// SDP media Ids
static constexpr int SDP_AUDIO_MEDIA_ID {0};
static constexpr int SDP_VIDEO_MEDIA_ID {1};

// ICE components Id used on SIP
static constexpr int ICE_AUDIO_RTP_COMPID {0};
static constexpr int ICE_AUDIO_RTCP_COMPID {1};
static constexpr int ICE_VIDEO_RTP_COMPID {2};
static constexpr int ICE_VIDEO_RTCP_COMPID {3};


/** A map to retreive SFLphone internal call id
 *  Given a SIP call ID (usefull for transaction sucha as transfer)*/
static std::map<std::string, std::string> transferCallID;

const char* const SIPCall::LINK_TYPE = SIPAccount::ACCOUNT_TYPE;

static void
dtmfSend(SIPCall &call, char code, const std::string &dtmf)
{
    if (dtmf == SIPAccount::OVERRTP_STR) {
#if USE_CCRTP
        call.getAudioRtp().sendDtmfDigit(code);
#endif
        return;
    } else if (dtmf != SIPAccount::SIPINFO_STR) {
        RING_WARN("Unknown DTMF type %s, defaulting to %s instead",
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
    call.sendSIPInfo(dtmf_body, "dtmf-relay");
}

SIPCall::SIPCall(SIPAccountBase& account, const std::string& id, Call::CallType type)
    : Call(account, id, type)
    //, avformatrtp_(new AVFormatRtpSession(id, /* FIXME: These are video! */ getSettings()))
    , avformatrtp_(new AVFormatRtpSession(id, *new std::map<std::string, std::string>))
#ifdef RING_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(id, getSettings())
#endif
    , sdp_(new Sdp(id))
{}

SIPCall::~SIPCall()
{
    const auto mod_ua_id = getSIPVoIPLink()->getModId();

    // prevent this from getting accessed in callbacks
    // RING_WARN: this is not thread-safe!
    if (inv && inv->mod_data[mod_ua_id]) {
        RING_WARN("Call was not properly removed from invite callbacks");
        inv->mod_data[mod_ua_id] = nullptr;
    }
}

SIPAccountBase&
SIPCall::getSIPAccount() const
{
    return static_cast<SIPAccountBase&>(getAccount());
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
        sdp_->setLocalPublishedAudioPort(callLocalAudioPort);
    }

    setLocalIp(localIP);

#ifdef RING_VIDEO
    if (getLocalVideoPort() == 0) {
        // https://projects.savoirfairelinux.com/issues/17498
        const unsigned int callLocalVideoPort = account.generateVideoPort();
        // this should already be guaranteed by SIPAccount
        assert(getLocalAudioPort() != callLocalVideoPort);

        setLocalVideoPort(callLocalVideoPort);
        sdp_->setLocalPublishedVideoPort(callLocalVideoPort);
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
    using ring::HistoryItem;

    std::map<std::string, std::string> entry(Call::createHistoryEntry());
    return entry;
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 */

int
SIPCall::SIPSessionReinvite()
{
    pjmedia_sdp_session *local_sdp = sdp_->getLocalSdpSession();
    pjsip_tx_data *tdata;

    if (local_sdp and inv and inv->pool_prov
        and pjsip_inv_reinvite(inv.get(), NULL, local_sdp, &tdata) == PJ_SUCCESS) {
        if (pjsip_inv_send_msg(inv.get(), tdata) == PJ_SUCCESS)
            return PJ_SUCCESS;
        else
            inv.reset();
    }

    return !PJ_SUCCESS;
}

void
SIPCall::sendSIPInfo(const char *const body, const char *const subtype)
{
    if (not inv or not inv->dlg)
        throw VoipLinkException("Couldn't get invite dialog");

    pj_str_t methodName = CONST_PJ_STR("INFO");
    pjsip_method method;
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    pjsip_tx_data *tdata;

    if (pjsip_dlg_create_request(inv->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
        RING_ERR("Could not create dialog");
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
        pjsip_dlg_send_request(inv->dlg, tdata, getSIPVoIPLink()->getModId(), NULL);
}

void
SIPCall::updateSDPFromSTUN()
{
#if USE_CCRTP
    auto& account = getSIPAccount();
    std::vector<long> socketDescriptors(getAudioRtp().getSocketDescriptors());

    try {
        std::vector<pj_sockaddr> stunPorts(getSIPVoIPLink()->sipTransport->getSTUNAddresses(account.getStunServerName(), account.getStunPort(), socketDescriptors));

        // FIXME: get video sockets
        //stunPorts.resize(4);

        account.setPublishedAddress(stunPorts[0]);
        // published IP MUST be updated first, since RTCP depends on it
        sdp_->setPublishedIP(account.getPublishedAddress());
        sdp_->updatePorts(stunPorts);
    } catch (const std::runtime_error &e) {
        RING_ERR("%s", e.what());
    }
#endif
}

void SIPCall::answer()
{
    auto& account = getSIPAccount();

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    if (!inv->neg) {
        RING_WARN("Negotiator is NULL, we've received an INVITE without an SDP");
        pjmedia_sdp_session *dummy = 0;
        getSIPVoIPLink()->createSDPOffer(inv.get(), &dummy);

        if (account.isStunEnabled())
            updateSDPFromSTUN();
    }

    pj_str_t contact(account.getContactHeader(transport_ ? transport_->get() : nullptr));
    setContactHeader(&contact);

    pjsip_tx_data *tdata;
    if (!inv->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(inv.get(), PJSIP_SC_OK, NULL, !inv->neg ? sdp_->getLocalSdpSession() : NULL, &tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    // contactStr must stay in scope as long as tdata
    if (contactHeader_.slen) {
        RING_DBG("Answering with contact header: %.*s", contactHeader_.slen, contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    if (pjsip_inv_send_msg(inv.get(), tdata) != PJ_SUCCESS) {
        inv.reset();
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    if (iceTransport_->isStarted())
        waitForIceNegotiation(DEFAULT_ICE_NEGO_TIMEOUT);
    startAllMedia();

    setConnectionState(CONNECTED);
    setState(ACTIVE);
}

void
SIPCall::hangup(int reason)
{
    // Stop all RTP streams
    stopAllMedias();

    if (not inv or not inv->dlg)
        throw VoipLinkException("No invite session for this call");

    auto& account = getSIPAccount();

    pjsip_route_hdr *route = inv->dlg->route_set.next;
    while (route and route != &inv->dlg->route_set) {
        char buf[1024];
        int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));

        if (printed >= 0) {
            buf[printed] = '\0';
            RING_DBG("Route header %s", buf);
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
    if (pjsip_inv_end_session(inv.get(), status, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    // contactStr must stay in scope as long as tdata
    const pj_str_t contactStr(account.getContactHeader(transport_ ? transport_->get() : nullptr));
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(inv.get(), tdata) != PJ_SUCCESS) {
        RING_ERR("Error sending hangup message");
        inv.reset();
        return;
    }

    // Make sure user data is NULL in callbacks
    inv->mod_data[getSIPVoIPLink()->getModId()] = NULL;

    removeCall();
}

void
SIPCall::refuse()
{
    if (!isIncoming() or getConnectionState() == Call::CONNECTED or !inv)
        return;

    avformatrtp_->stop();

    pjsip_tx_data *tdata;

    if (pjsip_inv_end_session(inv.get(), PJSIP_SC_DECLINE, NULL, &tdata) != PJ_SUCCESS)
        return;

    if (pjsip_inv_send_msg(inv.get(), tdata) != PJ_SUCCESS) {
        inv.reset();
        return;
    }

    // Make sure the pointer is NULL in callbacks
    inv->mod_data[getSIPVoIPLink()->getModId()] = NULL;

    removeCall();
}

static void
transfer_client_cb(pjsip_evsub *sub, pjsip_event *event)
{
    auto mod_ua_id = getSIPVoIPLink()->getModId();

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

                if (pjsip_inv_end_session(call->inv.get(), PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS) {
                    if (pjsip_inv_send_msg(call->inv.get(), tdata) != PJ_SUCCESS)
                        call->inv.reset();
                }

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
    if (not inv or not inv->dlg)
        return false;

    pjsip_evsub_user xfer_cb;
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    pjsip_evsub *sub;

    if (pjsip_xfer_create_uac(inv->dlg, &xfer_cb, &sub) != PJ_SUCCESS)
        return false;

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data(sub, getSIPVoIPLink()->getModId(), this);

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
    RING_DBG("Transferring to %.*s", dst.slen, dst.ptr);

    if (!transferCommon(&dst))
        throw VoipLinkException("Couldn't transfer");
}

bool
SIPCall::attendedTransfer(const std::string& to)
{
    const auto toCall = Manager::instance().callFactory.getCall<SIPCall>(to);
    if (!toCall)
        return false;

    if (not toCall->inv or not toCall->inv->dlg)
        return false;

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

    return transferCommon(&dst);
}

void
SIPCall::onhold()
{
    if (not setState(Call::HOLD))
        return;

    avformatrtp_->stop();
#ifdef RING_VIDEO
    videortp_.stop();
#endif

    sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    sdp_->addAttributeToLocalAudioMedia("sendonly");

#ifdef RING_VIDEO
    sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    sdp_->removeAttributeFromLocalVideoMedia("inactive");
    sdp_->addAttributeToLocalVideoMedia("inactive");
#endif

    if (SIPSessionReinvite() != PJ_SUCCESS)
        RING_WARN("Reinvite failed");
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
        RING_ERR("%s", e.what());
        throw VoipLinkException("SDP issue in offhold");
    }
#if USE_CCRTP
    catch (const ost::Socket *) {
        throw VoipLinkException("Socket problem in offhold");
    } catch (const AudioRtpFactoryException &) {
        throw VoipLinkException("Socket problem in offhold");
    }
#endif
}

void
SIPCall::internalOffHold(const std::function<void()> &SDPUpdateFunc)
{
    if (not setState(Call::ACTIVE))
        return;

    std::vector<AudioCodec*> sessionMedia(sdp_->getSessionAudioMedia());

    if (sessionMedia.empty()) {
        RING_WARN("Session media is empty");
        return;
    }

    std::vector<AudioCodec*> audioCodecs;

    for (auto & i : sessionMedia) {

        if (!i)
            continue;

        // Create a new instance for this codec
        AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(i->getPayloadType());

        if (ac == NULL) {
            RING_ERR("Could not instantiate codec %d", i->getPayloadType());
            throw std::runtime_error("Could not instantiate codec");
        }

        audioCodecs.push_back(ac);
    }

    if (audioCodecs.empty()) {
        throw std::runtime_error("Could not instantiate any codecs");
    }

#if USE_CCRTP
    audiortp_.initConfig();
    audiortp_.initSession();

    // Invoke closure
    SDPUpdateFunc();

    audiortp_.restoreLocalContext();
    audiortp_.initLocalCryptoInfoOnOffHold();
    audiortp_.start(audioCodecs);
#endif

    sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    sdp_->addAttributeToLocalAudioMedia("sendrecv");

#ifdef RING_VIDEO
    sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    sdp_->removeAttributeFromLocalVideoMedia("sendonly");
    sdp_->removeAttributeFromLocalVideoMedia("inactive");
    sdp_->addAttributeToLocalVideoMedia("sendrecv");
#endif

    if (SIPSessionReinvite() != PJ_SUCCESS) {
        RING_WARN("Reinvite failed, resuming hold");
        onhold();
    }
}

void
SIPCall::peerHungup()
{
    // Stop all RTP streams
    stopAllMedias();

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    // User hangup current call. Notify peer
    pjsip_tx_data *tdata = NULL;

    if (pjsip_inv_end_session(inv.get(), 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (auto ret = pjsip_inv_send_msg(inv.get(), tdata) == PJ_SUCCESS) {
        // Make sure user data is NULL in callbacks
        inv->mod_data[getSIPVoIPLink()->getModId()] = NULL;
    } else {
        inv.reset();
        sip_utils::sip_strerror(ret);
    }
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
    using namespace InstantMessaging;

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    /* Send IM message */
    UriList list;
    UriEntry entry;
    entry[IM_XML_URI] = std::string("\"" + from + "\"");  // add double quotes for xml formating
    list.push_front(entry);
    send_sip_message(inv.get(), getCallId(), appendUriList(message, list));
}
#endif // HAVE_INSTANT_MESSAGING

void
SIPCall::onServerFailure()
{
    Manager::instance().callFailure(*this);
    removeCall();
}

void
SIPCall::onClosed()
{
    Manager::instance().peerHungupCall(*this);
    removeCall();
    Manager::instance().checkAudio();
}

void
SIPCall::onAnswered()
{
    if (getConnectionState() != Call::CONNECTED) {
        if (iceTransport_->isStarted())
            waitForIceNegotiation(DEFAULT_ICE_NEGO_TIMEOUT);
        startAllMedia();
        setConnectionState(Call::CONNECTED);
        setState(Call::ACTIVE);
        Manager::instance().peerAnsweredCall(*this);
    }
}

void
SIPCall::setupLocalSDPFromIce()
{
    if (waitForIceInitialization(DEFAULT_ICE_INIT_TIMEOUT) <= 0) {
        RING_ERR("ICE init failed, ICE will not be used for medias");
        return;
    }

    sdp_->addIceAttributes(iceTransport_->getLocalAttributes());

    // Add video and audio channels
    sdp_->addIceCandidates(SDP_AUDIO_MEDIA_ID, iceTransport_->getLocalCandidates(ICE_AUDIO_RTP_COMPID));
    sdp_->addIceCandidates(SDP_AUDIO_MEDIA_ID, iceTransport_->getLocalCandidates(ICE_AUDIO_RTCP_COMPID));
#ifdef RING_VIDEO
    sdp_->addIceCandidates(SDP_VIDEO_MEDIA_ID, iceTransport_->getLocalCandidates(ICE_VIDEO_RTP_COMPID));
    sdp_->addIceCandidates(SDP_VIDEO_MEDIA_ID, iceTransport_->getLocalCandidates(ICE_VIDEO_RTCP_COMPID));
#endif
}

std::vector<IceCandidate>
SIPCall::getAllRemoteCandidates()
{
    std::vector<IceCandidate> rem_candidates;

    auto addSDPCandidates = [this](unsigned sdpMediaId,
                                   std::vector<IceCandidate>& out) {
        IceCandidate cand;
        for (auto& line : sdp_->getIceCandidates(sdpMediaId)) {
            if (iceTransport_->getCandidateFromSDP(line, cand))
                out.emplace_back(cand);
        }
    };

    addSDPCandidates(SDP_AUDIO_MEDIA_ID, rem_candidates);
#ifdef RING_VIDEO
    addSDPCandidates(SDP_VIDEO_MEDIA_ID, rem_candidates);
#endif

    return rem_candidates;
}

bool
SIPCall::startIce()
{
    if (iceTransport_->isStarted() || iceTransport_->isCompleted())
        return true;
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        RING_ERR("ICE empty attributes");
        return false;
    }
    return iceTransport_->start(rem_ice_attrs, getAllRemoteCandidates());
}

void
SIPCall::startAllMedia()
{
    auto& remoteIP = sdp_->getRemoteIP();
    avformatrtp_->updateSDP(*sdp_);
    avformatrtp_->updateDestination(remoteIP, sdp_->getRemoteAudioPort());
    if (isIceRunning()) {
        std::unique_ptr<IceSocket> sockRTP(newIceSocket(0));
        std::unique_ptr<IceSocket> sockRTCP(newIceSocket(1));
        avformatrtp_->start(std::move(sockRTP), std::move(sockRTCP));
    } else {
        const auto localAudioPort = sdp_->getLocalAudioPort();
        avformatrtp_->start(localAudioPort ? localAudioPort : sdp_->getRemoteAudioPort());
    }

#ifdef RING_VIDEO
    auto remoteVideoPort = sdp_->getRemoteVideoPort();
    videortp_.updateSDP(*sdp_);
    videortp_.updateDestination(remoteIP, remoteVideoPort);
    if (isIceRunning()) {
        std::unique_ptr<IceSocket> sockRTP(newIceSocket(2));
        std::unique_ptr<IceSocket> sockRTCP(newIceSocket(3));
        try {
            videortp_.start(std::move(sockRTP), std::move(sockRTCP));
        } catch (const std::runtime_error &e) {
            RING_ERR("videortp_.start() with ICE failed, %s", e.what());
        }
    } else {
        const auto localVideoPort = sdp_->getLocalVideoPort();
        try {
            videortp_.start(localVideoPort ? localVideoPort : remoteVideoPort);
        } catch (const std::runtime_error &e) {
            RING_ERR("videortp_.start() failed, %s", e.what());
        }
    }
#endif

    // Get the crypto attribute containing srtp's cryptographic context (keys, cipher)
    CryptoOffer crypto_offer;
    getSDP().getRemoteSdpCryptoFromOffer(sdp_->getActiveRemoteSdpSession(), crypto_offer);

#if USE_CCRTP && HAVE_SDES
    bool nego_success = false;

    if (!crypto_offer.empty()) {
        std::vector<CryptoSuiteDefinition> localCapabilities;

        for (size_t i = 0; i < RING_ARRAYSIZE(CryptoSuites); ++i)
            localCapabilities.push_back(CryptoSuites[i]);

        SdesNegotiator sdesnego(localCapabilities, crypto_offer);
        auto callMgr = Manager::instance().getClient()->getCallManager();

        if (sdesnego.negotiate()) {
            nego_success = true;

            try {
                audiortp_.setRemoteCryptoInfo(sdesnego);
                callMgr->secureSdesOn(getCallId());
            } catch (const AudioRtpFactoryException &e) {
                RING_ERR("%s", e.what());
                callMgr->secureSdesOff(getCallId());
            }
        } else {
            RING_ERR("SDES negotiation failure");
            callMgr->secureSdesOff(getCallId());
        }
    } else {
        RING_DBG("No crypto offer available");
    }

    // We did not find any crypto context for this media, RTP fallback
    if (!nego_success && audiortp_.isSdesEnabled()) {
        RING_ERR("Negotiation failed but SRTP is enabled, fallback on RTP");
        audiortp_.stop();
        audiortp_.setSrtpEnabled(false);

        const auto& account = getSIPAccount();
        if (account.getSrtpFallback()) {
            audiortp_.initSession();

            if (account.isStunEnabled())
                updateSDPFromSTUN();
        }
    }
#endif // USE_CCRTP && HAVE_SDES

    std::vector<AudioCodec*> sessionMedia(sdp_->getSessionAudioMedia());

    if (sessionMedia.empty()) {
        RING_WARN("Session media is empty");
        return;
    }

    try {
        Manager::instance().startAudioDriverStream();

        std::vector<AudioCodec*> audioCodecs;

        for (const auto & i : sessionMedia) {
            if (!i)
                continue;

            const int pl = i->getPayloadType();

            AudioCodec *ac = Manager::instance().audioCodecFactory.instantiateCodec(pl);

            if (!ac) {
                RING_ERR("Could not instantiate codec %d", pl);
            } else {
                audioCodecs.push_back(ac);
            }
        }

#if USE_CCRTP
        if (not audioCodecs.empty())
            getAudioRtp().updateSessionMedia(audioCodecs);
#endif
    } catch (const SdpException &e) {
        RING_ERR("%s", e.what());
    } catch (const std::exception &rtpException) {
        RING_ERR("%s", rtpException.what());
    }
}

void
SIPCall::stopAllMedias()
{
    RING_DBG("SIPCall %s: stopping all medias", getCallId().c_str());
    avformatrtp_->stop();
#ifdef RING_VIDEO
    videortp_.stop();
#endif
}
