/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "call_factory.h"
#include "sipcall.h"
#include "sipaccount.h" // for SIPAccount::ACCOUNT_TYPE
#include "sipaccountbase.h"
#include "sipvoiplink.h"
#include "sdes_negotiator.h"
#include "logger.h" // for _debug
#include "sdp.h"
#include "manager.h"
#include "string_utils.h"
#include "upnp/upnp_control.h"
#include "sip_utils.h"
#include "audio/audio_rtp_session.h"
#include "system_codec_container.h"
#include "im/instant_messaging.h"
#include "dring/call_const.h"
#include "dring/media_const.h"
#include "client/ring_signal.h"

#ifdef RING_VIDEO
#include "client/videomanager.h"
#include "video/video_rtp_session.h"
#include "dring/videomanager_interface.h"
#include <chrono>
#endif

#include "errno.h"

namespace ring {

using sip_utils::CONST_PJ_STR;

#ifdef RING_VIDEO
static DeviceParams
getVideoSettings()
{
    const auto& videomon = ring::getVideoDeviceMonitor();
    return videomon.getDeviceParams(videomon.getDefaultDevice());
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

const char* const SIPCall::LINK_TYPE = SIPAccount::ACCOUNT_TYPE;

static void
dtmfSend(SIPCall &call, char code, const std::string &dtmf)
{
    if (dtmf == SIPAccount::OVERRTP_STR) {
        RING_WARN("[call:%s] DTMF over RTP not supported yet", call.getCallId().c_str());
        return;
    } else if (dtmf != SIPAccount::SIPINFO_STR) {
        RING_WARN("[call:%s] Unknown DTMF type %s, defaulting to %s instead",
                  call.getCallId().c_str(), dtmf.c_str(), SIPAccount::SIPINFO_STR);
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
    , avformatrtp_(new AudioRtpSession(id))
#ifdef RING_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(id, getVideoSettings())
    , videoInput_(Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice())
#endif
    , sdp_(new Sdp(id))
{
    if (account.getUPnPActive())
        upnp_.reset(new upnp::Controller());
}

SIPCall::~SIPCall()
{
    setTransport({});
    inv.reset(); // prevents callback usage
}

SIPAccountBase&
SIPCall::getSIPAccount() const
{
    return static_cast<SIPAccountBase&>(getAccount());
}

void
SIPCall::setCallMediaLocal(const pj_sockaddr& localIP)
{
    setLocalIp(localIP);

    if (getLocalAudioPort() == 0
#ifdef RING_VIDEO
        || getLocalVideoPort() == 0
#endif
        )
        generateMediaPorts();
}

void
SIPCall::generateMediaPorts()
{
    auto& account = getSIPAccount();

    // Reference: http://www.cs.columbia.edu/~hgs/rtp/faq.html#ports
    // We only want to set ports to new values if they haven't been set
    const unsigned callLocalAudioPort = account.generateAudioPort();
    if (getLocalAudioPort() != 0)
        account.releasePort(getLocalAudioPort());
    setLocalAudioPort(callLocalAudioPort);
    sdp_->setLocalPublishedAudioPort(callLocalAudioPort);

#ifdef RING_VIDEO
    // https://projects.savoirfairelinux.com/issues/17498
    const unsigned int callLocalVideoPort = account.generateVideoPort();
    if (getLocalVideoPort() != 0)
        account.releasePort(getLocalVideoPort());
    // this should already be guaranteed by SIPAccount
    assert(getLocalAudioPort() != callLocalVideoPort);
    setLocalVideoPort(callLocalVideoPort);
    sdp_->setLocalPublishedVideoPort(callLocalVideoPort);
#endif
}

void SIPCall::setContactHeader(pj_str_t *contact)
{
    pj_strcpy(&contactHeader_, contact);
}

void
SIPCall::setTransport(std::shared_ptr<SipTransport> t)
{
    if (isSecure() and t and not t->isSecure()) {
        RING_ERR("Can't set unsecure transport to secure call.");
        return;
    }

    const auto list_id = reinterpret_cast<uintptr_t>(this);
    if (transport_)
        transport_->removeStateListener(list_id);
    transport_ = t;

    if (transport_) {
        setSecure(transport_->isSecure());
        std::weak_ptr<SIPCall> wthis_ = std::static_pointer_cast<SIPCall>(shared_from_this());

        // listen for transport destruction
        transport_->addStateListener(list_id,
            [wthis_] (pjsip_transport_state state, const pjsip_transport_state_info*) {
                if (auto this_ = wthis_.lock()) {
                    // end the call if the SIP transport is shut down
                    if (not SipTransport::isAlive(this_->transport_, state) and this_->getConnectionState() != ConnectionState::DISCONNECTED) {
                        RING_WARN("[call:%s] Ending call because underlying SIP transport was closed",
                                  this_->getCallId().c_str());
                        this_->onFailure(ECONNRESET);
                    }
                }
            });
    }
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 */

int
SIPCall::SIPSessionReinvite()
{
    // Do nothing if no invitation processed yet
    if (not inv or inv->invite_tsx)
        return PJ_SUCCESS;

    RING_DBG("[call:%s] Processing reINVITE (state=%s)", getCallId().c_str(),
             pjsip_inv_state_name(inv->state));

    // Generate new ports to receive the new media stream
    // LibAV doesn't discriminate SSRCs and will be confused about Seq changes on a given port
    generateMediaPorts();
    sdp_->clearIce();
    auto& acc = getSIPAccount();
    if (not sdp_->createOffer(acc.getActiveAccountCodecInfoList(MEDIA_AUDIO),
                              acc.getActiveAccountCodecInfoList(acc.isVideoEnabled() ? MEDIA_VIDEO : MEDIA_NONE),
                              acc.getSrtpKeyExchange(),
                              getState() == CallState::HOLD))
        return !PJ_SUCCESS;

    if (initIceTransport(true))
        setupLocalSDPFromIce();

    pjsip_tx_data* tdata;
    auto local_sdp = sdp_->getLocalSdpSession();
    auto result = pjsip_inv_reinvite(inv.get(), nullptr, local_sdp, &tdata);
    if (result == PJ_SUCCESS) {
        if (!tdata)
            return PJ_SUCCESS;
        result = pjsip_inv_send_msg(inv.get(), tdata);
        if (result == PJ_SUCCESS)
            return PJ_SUCCESS;
        RING_ERR("[call:%s] Failed to send REINVITE msg (pjsip: %s)", getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());
        // Canceling internals without sending (anyways the send has just failed!)
        pjsip_inv_cancel_reinvite(inv.get(), &tdata);
    } else
        RING_ERR("[call:%s] Failed to create REINVITE msg (pjsip: %s)", getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());

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
        RING_ERR("[call:%s] Could not create dialog", getCallId().c_str());
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
    RING_WARN("[call:%s] SIPCall::updateSDPFromSTUN() not implemented", getCallId().c_str());
}

void
SIPCall::terminateSipSession(int status)
{
    if (inv and inv->state != PJSIP_INV_STATE_DISCONNECTED) {
        RING_DBG("[call:%s] Terminate SIP session", getCallId().c_str());

        pjsip_tx_data* tdata = nullptr;
        auto ret = pjsip_inv_end_session(inv.get(), status, nullptr, &tdata);
        if (ret == PJ_SUCCESS) {
            if (tdata) {
                auto contact = getSIPAccount().getContactHeader(transport_ ? transport_->get() : nullptr);
                sip_utils::addContactHeader(&contact, tdata);
                ret = pjsip_inv_send_msg(inv.get(), tdata);
                if (ret != PJ_SUCCESS)
                    RING_ERR("[call:%s] failed to send terminate msg, SIP error (%s)",
                             getCallId().c_str(), sip_utils::sip_strerror(ret).c_str());
            }
        } else
            RING_ERR("[call:%s] failed to terminate INVITE@%p, SIP error (%s)",
                     getCallId().c_str(), inv.get(), sip_utils::sip_strerror(ret).c_str());
    }

    inv.reset();
}

void
SIPCall::answer()
{
    auto& account = getSIPAccount();

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    if (!inv->neg) {
        RING_WARN("[call:%s] Negotiator is NULL, we've received an INVITE without an SDP",
                  getCallId().c_str());
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
        RING_DBG("[call:%s] Answering with contact header: %.*s",
                 getCallId().c_str(), (int)contactHeader_.slen, contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    if (pjsip_inv_send_msg(inv.get(), tdata) != PJ_SUCCESS) {
        inv.reset();
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::hangup(int reason)
{
    // Stop all RTP streams
    stopAllMedia();

    if (not inv or not inv->dlg) {
        removeCall();
        throw VoipLinkException("No invite session for this call");
    }

    pjsip_route_hdr *route = inv->dlg->route_set.next;
    while (route and route != &inv->dlg->route_set) {
        char buf[1024];
        int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));

        if (printed >= 0) {
            buf[printed] = '\0';
            RING_DBG("[call:%s] Route header %s", getCallId().c_str(), buf);
        }

        route = route->next;
    }

    const int status = reason ? reason :
                       inv->state <= PJSIP_INV_STATE_EARLY and inv->role != PJSIP_ROLE_UAC ?
                       PJSIP_SC_CALL_TSX_DOES_NOT_EXIST :
                       inv->state >= PJSIP_INV_STATE_DISCONNECTED ? PJSIP_SC_DECLINE : 0;

    // Notify the peer
    terminateSipSession(status);

    setState(Call::ConnectionState::DISCONNECTED, reason);
    removeCall();
}

void
SIPCall::refuse()
{
    if (!isIncoming() or getConnectionState() == ConnectionState::CONNECTED or !inv)
        return;

    stopAllMedia();

    // Notify the peer
    terminateSipSession(PJSIP_SC_DECLINE);

    setState(Call::ConnectionState::DISCONNECTED, ECONNABORTED);
    removeCall();
}

static void
transfer_client_cb(pjsip_evsub *sub, pjsip_event *event)
{
    auto link = getSIPVoIPLink();
    if (not link) {
        RING_ERR("no more VoIP link");
        return;
    }

    auto mod_ua_id = link->getModId();

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
                if (call->inv)
                    call->terminateSipSession(PJSIP_SC_GONE);
                Manager::instance().hangupCall(call->getCallId());
                pjsip_evsub_set_mod_data(sub, mod_ua_id, NULL);
            }

            break;
        }

        case PJSIP_EVSUB_STATE_NULL:
        case PJSIP_EVSUB_STATE_SENT:
        case PJSIP_EVSUB_STATE_PENDING:
        case PJSIP_EVSUB_STATE_UNKNOWN:
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
    RING_DBG("[call:%s] Transferring to %.*s", getCallId().c_str(), (int)dst.slen, dst.ptr);

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

bool
SIPCall::onhold()
{
    if (not setState(CallState::HOLD))
        return false;

    stopAllMedia();

    if (getConnectionState() == ConnectionState::CONNECTED) {
        if (SIPSessionReinvite() != PJ_SUCCESS)
            RING_WARN("[call:%s] Reinvite failed", getCallId().c_str());
    }

    return true;
}

bool
SIPCall::offhold()
{
    bool success = false;
    auto& account = getSIPAccount();

    try {
        if (account.isStunEnabled())
            success = internalOffHold([&] { updateSDPFromSTUN(); });
        else
            success = internalOffHold([] {});

    } catch (const SdpException &e) {
        RING_ERR("[call:%s] %s", getCallId().c_str(), e.what());
        throw VoipLinkException("SDP issue in offhold");
    }

    return success;
}

bool
SIPCall::internalOffHold(const std::function<void()>& sdp_cb)
{
    if (not setState(CallState::ACTIVE))
        return false;

    sdp_cb();

    if (getConnectionState() == ConnectionState::CONNECTED) {
        if (SIPSessionReinvite() != PJ_SUCCESS) {
            RING_WARN("[call:%s] resuming hold", getCallId().c_str());
            onhold();
            return false;
        }
    }

    return true;
}

void
SIPCall::switchInput(const std::string& resource)
{
#ifdef RING_VIDEO
    videoInput_ = resource;
    SIPSessionReinvite();
#endif
}

void
SIPCall::peerHungup()
{
    // Stop all RTP streams
    stopAllMedia();

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    terminateSipSession(PJSIP_SC_NOT_FOUND);
    Call::peerHungup();
}

void
SIPCall::carryingDTMFdigits(char code)
{
    dtmfSend(*this, code, getSIPAccount().getDtmfType());
}

void
SIPCall::sendTextMessage(const std::map<std::string, std::string>& messages,
                         const std::string& /* from */)
{
    if (not inv)
        throw VoipLinkException("No invite session for this call");

    //TODO: for now we ignore the "from" (the previous implementation for sending this info was
    //      buggy and verbose), another way to send the original message sender will be implemented
    //      in the future

    im::sendSipMessage(inv.get(), messages);
}

void
SIPCall::onFailure(signed cause)
{
    setState(CallState::MERROR, ConnectionState::DISCONNECTED, cause);
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
    if (getConnectionState() != ConnectionState::CONNECTED) {
        setState(CallState::ACTIVE, ConnectionState::CONNECTED);
        Manager::instance().peerAnsweredCall(*this);
    }
}

void
SIPCall::onPeerRinging()
{
    setState(ConnectionState::RINGING);
    Manager::instance().peerRingingCall(*this);
}

void
SIPCall::setupLocalSDPFromIce()
{
    if (not iceTransport_) {
        RING_WARN("[call:%s] null icetransport, no attributes added to SDP", getCallId().c_str());
        return;
    }

    if (waitForIceInitialization(DEFAULT_ICE_INIT_TIMEOUT) <= 0) {
        RING_ERR("[call:%s] Local ICE init failed", getCallId().c_str());
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
            if (iceTransport_->getCandidateFromSDP(line, cand)) {
                RING_DBG("[call:%s] add remote ICE candidate: %s", getCallId().c_str(), line.c_str());
                out.emplace_back(cand);
            }
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
    if (not iceTransport_ or iceTransport_->isFailed())
        return false;
    if (iceTransport_->isStarted()) {
        RING_DBG("[call:%s] ICE already started", getCallId().c_str());
        return true;
    }
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        RING_ERR("[call:%s] ICE empty attributes", getCallId().c_str());
        return false;
    }
    return iceTransport_->start(rem_ice_attrs, getAllRemoteCandidates());
}

bool
SIPCall::useVideoCodec(const AccountVideoCodecInfo* codec) const
{
#ifdef RING_VIDEO
    if (videortp_.isSending())
        return videortp_.useCodec(codec);
#endif
    return false;
}

void
SIPCall::startAllMedia()
{
    if (isSecure() && not transport_->isSecure()) {
        RING_ERR("[call:%s] Can't perform secure call over insecure SIP transport",
                 getCallId().c_str());
        onFailure(EPROTONOSUPPORT);
        return;
    }
    auto slots = sdp_->getMediaSlots();
    unsigned ice_comp_id = 0;
    bool peer_holding {true};
    int slotN = -1;

    for (const auto& slot : slots) {
        ++slotN;
        const auto& local = slot.first;
        const auto& remote = slot.second;

        if (local.type != remote.type) {
            RING_ERR("[call:%s] [SDP:slot#%u] Inconsistent media types between local and remote",
                     getCallId().c_str(), slotN);
            continue;
        }

        RtpSession* rtp = local.type == MEDIA_AUDIO
            ? static_cast<RtpSession*>(avformatrtp_.get())
#ifdef RING_VIDEO
            : static_cast<RtpSession*>(&videortp_);
#else
            : nullptr;
#endif

        if (not rtp)
            continue;

        if (!local.codec) {
            RING_WARN("[call:%s] [SDP:slot#%u] Missing local codec", getCallId().c_str(),
                      slotN);
            continue;
        }
        if (!remote.codec) {
            RING_WARN("[call:%s] [SDP:slot#%u] Missing remote codec", getCallId().c_str(),
                      slotN);
            continue;
        }

        peer_holding &= remote.holding;

        if (isSecure() && (not local.crypto || not remote.crypto)) {
            RING_ERR("[call:%s] [SDP:slot#%u] Can't perform secure call over insecure RTP transport",
                     getCallId().c_str(), slotN);
            continue;
        }

#ifdef RING_VIDEO
        if (local.type == MEDIA_VIDEO)
            videortp_.switchInput(videoInput_);
#endif

        rtp->updateMedia(remote, local);

        // Not restarting media loop on hold as it's a huge waste of CPU ressources
        // because of the audio loop
        if (getState() != CallState::HOLD) {
            if (isIceRunning()) {
                rtp->start(newIceSocket(ice_comp_id + 0),
                           newIceSocket(ice_comp_id + 1));
                ice_comp_id += 2;
            } else
                rtp->start();
        }

        switch (local.type) {
#ifdef RING_VIDEO
            case MEDIA_VIDEO:
                isVideoMuted_ = videoInput_.empty();
                break;
#endif
            case MEDIA_AUDIO:
                isAudioMuted_ = not rtp->isSending();
                break;
            default: break;
        }
    }

    if (peerHolding_ != peer_holding) {
        peerHolding_ = peer_holding;
        emitSignal<DRing::CallSignal::PeerHold>(getCallId(), peerHolding_);
    }
}

void
SIPCall::restartMediaSender()
{
    RING_DBG("[call:%s] restarting TX media streams", getCallId().c_str());
    avformatrtp_->restartSender();
#ifdef RING_VIDEO
    videortp_.restartSender();
#endif
}

void
SIPCall::stopAllMedia()
{
    RING_DBG("[call:%s] stopping all medias", getCallId().c_str());
    avformatrtp_->stop();
#ifdef RING_VIDEO
    videortp_.stop();
#endif
}

void
SIPCall::muteMedia(const std::string& mediaType, bool mute)
{
    if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_VIDEO) == 0) {
#ifdef RING_VIDEO
        if (mute == isVideoMuted_) return;
        RING_WARN("[call:%s] video muting %s", getCallId().c_str(), bool_to_str(mute));
        isVideoMuted_ = mute;
        videoInput_ = isVideoMuted_ ? "" : Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice();
        DRing::switchInput(getCallId(), videoInput_);
        emitSignal<DRing::CallSignal::VideoMuted>(getCallId(), isVideoMuted_);
#endif
    } else if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_AUDIO) == 0) {
        if (mute == isAudioMuted_) return;
        RING_WARN("[call:%s] audio muting %s", getCallId().c_str(), bool_to_str(mute));
        isAudioMuted_ = mute;
        avformatrtp_->setMuted(isAudioMuted_);
        emitSignal<DRing::CallSignal::AudioMuted>(getCallId(), isAudioMuted_);
    }
}

void
SIPCall::onMediaUpdate()
{
    stopAllMedia();
    openPortsUPnP();

    if (startIce()) {
        auto this_ = std::static_pointer_cast<SIPCall>(shared_from_this());
        auto ice = iceTransport_;
        auto iceTimeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        Manager::instance().addTask([=] {
            if (ice != this_->iceTransport_) {
                RING_WARN("[call:%s] ICE transport replaced", getCallId().c_str());
                return false;
            }
            /* First step: wait for an ICE transport for SIP channel */
            if (this_->iceTransport_->isFailed() or std::chrono::steady_clock::now() >= iceTimeout) {
                RING_DBG("[call:%s] ICE init failed (or timeout)", getCallId().c_str());
                this_->onFailure(ETIMEDOUT);
                return false;
            }
            if (not this_->iceTransport_->isRunning())
                return true;
            startAllMedia();
            return false;
        });
    } else {
        RING_WARN("[call:%s] ICE not used for media", getCallId().c_str());
        startAllMedia();
    }
}

void
SIPCall::onReceiveOffer(const pjmedia_sdp_session* offer)
{
    sdp_->clearIce();
    auto& acc = getSIPAccount();
    sdp_->receiveOffer(offer,
        acc.getActiveAccountCodecInfoList(MEDIA_AUDIO),
        acc.getActiveAccountCodecInfoList(acc.isVideoEnabled() ? MEDIA_VIDEO : MEDIA_NONE),
        acc.getSrtpKeyExchange(),
        getState() == CallState::HOLD
    );
    auto ice_attrs = Sdp::getIceAttributes(offer);
    if (not ice_attrs.ufrag.empty() and not ice_attrs.pwd.empty()) {
        if (initIceTransport(false))
            setupLocalSDPFromIce();
    }
    sdp_->startNegotiation();
    pjsip_inv_set_sdp_answer(inv.get(), sdp_->getLocalSdpSession());
}

void
SIPCall::openPortsUPnP()
{
    if (upnp_) {
        /**
         * Try to open the desired ports with UPnP,
         * if they are used, use the alternative port and update the SDP session with the newly chosen port(s)
         *
         * TODO: the inital ports were chosen from the list of available ports and were marked as used
         *       the newly selected port should possibly be checked against the list of used ports and marked
         *       as used, the old port should be "released"
         */
        RING_DBG("[call:%s] opening ports via UPNP for SDP session", getCallId().c_str());
        uint16_t audio_port_used;
        if (upnp_->addAnyMapping(sdp_->getLocalAudioPort(), upnp::PortType::UDP, true, &audio_port_used)) {
            uint16_t control_port_used;
            if (upnp_->addAnyMapping(sdp_->getLocalAudioControlPort(), upnp::PortType::UDP, true, &control_port_used)) {
                sdp_->setLocalPublishedAudioPorts(audio_port_used, control_port_used);
            }
        }
#ifdef RING_VIDEO
        uint16_t video_port_used;
        if (upnp_->addAnyMapping(sdp_->getLocalVideoPort(), upnp::PortType::UDP, true, &video_port_used)) {
            uint16_t control_port_used;
            if (upnp_->addAnyMapping(sdp_->getLocalVideoControlPort(), upnp::PortType::UDP, true, &control_port_used)) {
                sdp_->setLocalPublishedVideoPorts(video_port_used, control_port_used);
            }
        }
#endif
    }
}

std::map<std::string, std::string>
SIPCall::getDetails() const
{
    auto details = Call::getDetails();
    details.emplace(DRing::Call::Details::PEER_HOLDING,
                    peerHolding_ ? TRUE_STR : FALSE_STR);

    auto& acc = getSIPAccount();

#ifdef RING_VIDEO
    // If Video is not enabled return an empty string
    details.emplace(DRing::Call::Details::VIDEO_SOURCE, acc.isVideoEnabled() ? videoInput_ : "");
#endif

    if (transport_ and transport_->isSecure()) {
        const auto& tlsInfos = transport_->getTlsInfos();
        const auto& cipher = pj_ssl_cipher_name(tlsInfos.cipher);
        details.emplace(DRing::TlsTransport::TLS_CIPHER, cipher ? cipher : "");
        if (tlsInfos.peerCert) {
            details.emplace(DRing::TlsTransport::TLS_PEER_CERT,
                            tlsInfos.peerCert->toString());
            auto ca = tlsInfos.peerCert->issuer;
            unsigned n = 0;
            while (ca) {
                std::ostringstream name_str;
                name_str << DRing::TlsTransport::TLS_PEER_CA_ << n++;
                details.emplace(name_str.str(), ca->toString());
                ca = ca->issuer;
            }
            details.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM,
                            ring::to_string(n));
        } else {
            details.emplace(DRing::TlsTransport::TLS_PEER_CERT, "");
            details.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM, "");
        }
    }
    return details;
}

void
SIPCall::setSecure(bool sec)
{
    if (srtpEnabled_)
        return;
    if (sec && getConnectionState() != ConnectionState::DISCONNECTED) {
        throw std::runtime_error("Can't enable security since call is already connected");
    }
    srtpEnabled_ = sec;
}

void
SIPCall::InvSessionDeleter::operator ()(pjsip_inv_session* inv) const noexcept
{
    // prevent this from getting accessed in callbacks
    // RING_WARN: this is not thread-safe!
    inv->mod_data[getSIPVoIPLink()->getModId()] = nullptr;
    pjsip_dlg_dec_lock(inv->dlg);
}

bool
SIPCall::initIceTransport(bool master, unsigned channel_num)
{
    auto result = Call::initIceTransport(master, channel_num);
    if (result) {
        if (const auto& publicIP = getSIPAccount().getPublishedIpAddress()) {
            for (unsigned compId = 1; compId <= iceTransport_->getComponentCount(); ++compId)
                iceTransport_->registerPublicIP(compId, publicIP);
        }
    }
    return result;
}

} // namespace ring
