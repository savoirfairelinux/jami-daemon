/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "ice_transport.h"
#include "thread_pool.h"

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#include "video/video_rtp_session.h"
#include "dring/videomanager_interface.h"
#include <chrono>
#endif

#include "errno.h"

#include <opendht/crypto.h>

namespace jami {

using sip_utils::CONST_PJ_STR;

#ifdef ENABLE_VIDEO
static DeviceParams
getVideoSettings()
{
    const auto& videomon = jami::getVideoDeviceMonitor();
    return videomon.getDeviceParams(videomon.getDefaultDevice());
}
#endif

static constexpr int DEFAULT_ICE_INIT_TIMEOUT {35}; // seconds
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

SIPCall::SIPCall(SIPAccountBase& account, const std::string& id, Call::CallType type,
                 const std::map<std::string, std::string>& details)
    : Call(account, id, type, details)
    , avformatrtp_(new AudioRtpSession(id))
#ifdef ENABLE_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(new video::VideoRtpSession(id, getVideoSettings()))
    , mediaInput_(Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice())
#endif
    , sdp_(new Sdp(id))
{
    if (account.getUPnPActive())
        upnp_.reset(new upnp::Controller());

    setCallMediaLocal();
}

SIPCall::~SIPCall()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    setTransport({});
    inv.reset(); // prevents callback usage
}

SIPAccountBase&
SIPCall::getSIPAccount() const
{
    return static_cast<SIPAccountBase&>(getAccount());
}

void
SIPCall::setCallMediaLocal()
{
    if (localAudioPort_ == 0
#ifdef ENABLE_VIDEO
        || localVideoPort_ == 0
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
    if (localAudioPort_ != 0)
        account.releasePort(localAudioPort_);
    localAudioPort_ = callLocalAudioPort;
    sdp_->setLocalPublishedAudioPort(callLocalAudioPort);

#ifdef ENABLE_VIDEO
    // https://projects.savoirfairelinux.com/issues/17498
    const unsigned int callLocalVideoPort = account.generateVideoPort();
    if (localVideoPort_ != 0)
        account.releasePort(localVideoPort_);
    // this should already be guaranteed by SIPAccount
    assert(localAudioPort_ != callLocalVideoPort);
    localVideoPort_ = callLocalVideoPort;
    sdp_->setLocalPublishedVideoPort(callLocalVideoPort);
#endif
}

void SIPCall::setContactHeader(pj_str_t *contact)
{
    pj_strcpy(&contactHeader_, contact);
}

void
SIPCall::setTransport(const std::shared_ptr<SipTransport>& t)
{
    if (isSecure() and t and not t->isSecure()) {
        JAMI_ERR("Can't set unsecure transport to secure call.");
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
                        JAMI_WARN("[call:%s] Ending call because underlying SIP transport was closed",
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
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    // Do nothing if no invitation processed yet
    if (not inv or inv->invite_tsx)
        return PJ_SUCCESS;

    JAMI_DBG("[call:%s] Processing reINVITE (state=%s)", getCallId().c_str(),
             pjsip_inv_state_name(inv->state));

    // Generate new ports to receive the new media stream
    // LibAV doesn't discriminate SSRCs and will be confused about Seq changes on a given port
    generateMediaPorts();
    sdp_->clearIce();
    auto& acc = getSIPAccount();
    if (not sdp_->createOffer(acc.getActiveAccountCodecInfoList(MEDIA_AUDIO),
                              acc.getActiveAccountCodecInfoList(acc.isVideoEnabled() and not isAudioOnly() ? MEDIA_VIDEO
                                                                                                           : MEDIA_NONE),
                              acc.getSrtpKeyExchange(),
                              getState() == CallState::HOLD))
        return !PJ_SUCCESS;

    if (initIceMediaTransport(true))
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
        JAMI_ERR("[call:%s] Failed to send REINVITE msg (pjsip: %s)", getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());
        // Canceling internals without sending (anyways the send has just failed!)
        pjsip_inv_cancel_reinvite(inv.get(), &tdata);
    } else
        JAMI_ERR("[call:%s] Failed to create REINVITE msg (pjsip: %s)", getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());

    return !PJ_SUCCESS;
}

void
SIPCall::sendSIPInfo(const char *const body, const char *const subtype)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (not inv or not inv->dlg)
        throw VoipLinkException("Couldn't get invite dialog");

    pj_str_t methodName = CONST_PJ_STR("INFO");
    constexpr pj_str_t type = CONST_PJ_STR("application");

    pjsip_method method;
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    pjsip_tx_data *tdata;

    if (pjsip_dlg_create_request(inv->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not create dialog", getCallId().c_str());
        return;
    }

    /* Create "application/<subtype>" message body. */
    pj_str_t content;
    pj_cstr(&content, body);
    pj_str_t pj_subtype;
    pj_cstr(&pj_subtype, subtype);
    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &pj_subtype, &content);
    if (tdata->msg->body == NULL)
        pjsip_tx_data_dec_ref(tdata);
    else
        pjsip_dlg_send_request(inv->dlg, tdata, getSIPVoIPLink()->getModId(), NULL);
}

void
SIPCall::requestKeyframe()
{
    constexpr const char * const BODY =
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<media_control><vc_primitive><to_encoder>"
        "<picture_fast_update/>"
        "</to_encoder></vc_primitive></media_control>";

    JAMI_DBG("Sending video keyframe request via SIP INFO");
    try {
        sendSIPInfo(BODY, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending video keyframe request: %s", e.what());
    }
}

void
SIPCall::updateSDPFromSTUN()
{
    JAMI_WARN("[call:%s] SIPCall::updateSDPFromSTUN() not implemented", getCallId().c_str());
}

void
SIPCall::terminateSipSession(int status)
{
    JAMI_DBG("[call:%s] Terminate SIP session", getCallId().c_str());
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (inv and inv->state != PJSIP_INV_STATE_DISCONNECTED) {
        pjsip_tx_data* tdata = nullptr;
        auto ret = pjsip_inv_end_session(inv.get(), status, nullptr, &tdata);
        if (ret == PJ_SUCCESS) {
            if (tdata) {
                auto contact = getSIPAccount().getContactHeader(transport_ ? transport_->get() : nullptr);
                sip_utils::addContactHeader(&contact, tdata);
                ret = pjsip_inv_send_msg(inv.get(), tdata);
                if (ret != PJ_SUCCESS)
                    JAMI_ERR("[call:%s] failed to send terminate msg, SIP error (%s)",
                             getCallId().c_str(), sip_utils::sip_strerror(ret).c_str());
            }
        } else
            JAMI_ERR("[call:%s] failed to terminate INVITE@%p, SIP error (%s)",
                     getCallId().c_str(), inv.get(), sip_utils::sip_strerror(ret).c_str());
    }

    inv.reset();
}

void
SIPCall::answer()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    auto& account = getSIPAccount();

    if (not inv)
        throw VoipLinkException("[call:" + getCallId() + "] answer: no invite session for this call");

    if (!inv->neg) {
        JAMI_WARN("[call:%s] Negotiator is NULL, we've received an INVITE without an SDP",
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
        JAMI_DBG("[call:%s] Answering with contact header: %.*s",
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
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (inv and inv->dlg) {
        pjsip_route_hdr *route = inv->dlg->route_set.next;
        while (route and route != &inv->dlg->route_set) {
            char buf[1024];
            int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));
            if (printed >= 0) {
                buf[printed] = '\0';
                JAMI_DBG("[call:%s] Route header %s", getCallId().c_str(), buf);
            }
            route = route->next;
        }
        const int status = reason ? reason :
                           inv->state <= PJSIP_INV_STATE_EARLY and inv->role != PJSIP_ROLE_UAC ?
                           PJSIP_SC_CALL_TSX_DOES_NOT_EXIST :
                           inv->state >= PJSIP_INV_STATE_DISCONNECTED ? PJSIP_SC_DECLINE : 0;
        // Notify the peer
        terminateSipSession(status);
    }
    // Stop all RTP streams
    stopAllMedia();
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
        JAMI_ERR("no more VoIP link");
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
SIPCall::transferCommon(const pj_str_t *dst)
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

    if (Recordable::isRecording())
        stopRecording();

    std::string toUri = account.getToUri(to);
    const pj_str_t dst(CONST_PJ_STR(toUri));
    JAMI_DBG("[call:%s] Transferring to %.*s", getCallId().c_str(), (int)dst.slen, dst.ptr);

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
    // If ICE is currently negotiating, we must wait before hold the call
    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::HoldingOn;
        return false;
    }
    if (not setState(CallState::HOLD))
        return false;

    stopAllMedia();

    if (getConnectionState() == ConnectionState::CONNECTED) {
        if (SIPSessionReinvite() != PJ_SUCCESS) {
            JAMI_WARN("[call:%s] Reinvite failed", getCallId().c_str());
            return true;
        }
    }

    isWaitingForIceAndMedia_ = true;

    return true;
}

bool
SIPCall::offhold()
{
    bool success = false;
    // If ICE is currently negotiating, we must wait before unhold the call
    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::HoldingOff;
        return false;
    }

    auto& account = getSIPAccount();

    try {
        if (account.isStunEnabled())
            success = internalOffHold([&] { updateSDPFromSTUN(); });
        else
            success = internalOffHold([] {});

    } catch (const SdpException &e) {
        JAMI_ERR("[call:%s] %s", getCallId().c_str(), e.what());
        throw VoipLinkException("SDP issue in offhold");
    }

    if (success) isWaitingForIceAndMedia_ = true;

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
            JAMI_WARN("[call:%s] resuming hold", getCallId().c_str());
            onhold();
            return false;
        }
    }

    return true;
}

void
SIPCall::switchInput(const std::string& resource)
{
    mediaInput_ = resource;
    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::SwitchInput;
    } else {
        if (SIPSessionReinvite() == PJ_SUCCESS) {
            isWaitingForIceAndMedia_ = true;
        }
    }
}

void
SIPCall::peerHungup()
{
    // Stop all RTP streams
    stopAllMedia();

    if (inv)
        terminateSipSession(PJSIP_SC_NOT_FOUND);
    else
        JAMI_ERR("[call:%s] peerHungup: no invite session for this call", getCallId().c_str());

    Call::peerHungup();
}

void
SIPCall::carryingDTMFdigits(char code)
{
    int duration = Manager::instance().voipPreferences.getPulseLength();
    char dtmf_body[1000];

    // handle flash code
    if (code == '!') {
        snprintf(dtmf_body, sizeof dtmf_body - 1, "Signal=16\r\nDuration=%d\r\n", duration);
    } else {
        snprintf(dtmf_body, sizeof dtmf_body - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);
    }

    try {
        sendSIPInfo(dtmf_body, "dtmf-relay");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending DTMF: %s", e.what());
    }
}

void
SIPCall::setVideoOrientation(int rotation)
{
    std::string sip_body =
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<media_control><vc_primitive><to_encoder>"
        "<device_orientation=" + std::to_string(rotation) + "/>"
        "</to_encoder></vc_primitive></media_control>";

    JAMI_DBG("Sending device orientation via SIP INFO");

    sendSIPInfo(sip_body.c_str(), "media_control+xml");
}

void
SIPCall::sendTextMessage(const std::map<std::string, std::string>& messages,
                         const std::string& from)
{
    //TODO: for now we ignore the "from" (the previous implementation for sending this info was
    //      buggy and verbose), another way to send the original message sender will be implemented
    //      in the future
    if (not subcalls_.empty()) {
        pendingOutMessages_.emplace_back(messages, from);
        for (auto& c : subcalls_)
            c->sendTextMessage(messages, from);
    } else {
        if (inv) {
            try {
                im::sendSipMessage(inv.get(), messages);
            } catch (...) {}
        } else {
            pendingOutMessages_.emplace_back(messages, from);
            JAMI_ERR("[call:%s] sendTextMessage: no invite session for this call", getCallId().c_str());
        }
    }
}

void
SIPCall::removeCall()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    JAMI_WARN("[call:%s] removeCall()", getCallId().c_str());
    Call::removeCall();
    mediaTransport_.reset();
    inv.reset();
    setTransport({});
}

void
SIPCall::onFailure(signed cause)
{
    setState(CallState::MERROR, ConnectionState::DISCONNECTED, cause);
    runOnMainThread([w = std::weak_ptr<Call>(shared_from_this())] {
        if (auto shared = w.lock()) {
            auto& call = *shared;
            Manager::instance().callFailure(call);
            call.removeCall();
        }
    });
}

void
SIPCall::onBusyHere()
{
    if (getCallType() == CallType::OUTGOING)
        setState(CallState::PEER_BUSY, ConnectionState::DISCONNECTED);
    else
        setState(CallState::BUSY, ConnectionState::DISCONNECTED);

    runOnMainThread([w = std::weak_ptr<Call>(shared_from_this())] {
        if (auto shared = w.lock()) {
            auto& call = *shared;
            Manager::instance().callBusy(call);
            call.removeCall();
        }
    });
}

void
SIPCall::onClosed()
{
    runOnMainThread([w = std::weak_ptr<Call>(shared_from_this())] {
        if (auto shared = w.lock()) {
            auto& call = *shared;
            Manager::instance().peerHungupCall(call);
            call.removeCall();
            Manager::instance().checkAudio();
        }
    });
}

void
SIPCall::onAnswered()
{
    JAMI_WARN("[call:%s] onAnswered()", getCallId().c_str());
    if (getConnectionState() != ConnectionState::CONNECTED) {
        setState(CallState::ACTIVE, ConnectionState::CONNECTED);
        if (not isSubcall()) {
            runOnMainThread([w = std::weak_ptr<Call>(shared_from_this())] {
                if (auto shared = w.lock()) {
                    Manager::instance().peerAnsweredCall(*shared);
                }
            });
        }
    }
}

void
SIPCall::sendKeyframe()
{
#ifdef ENABLE_VIDEO
    ThreadPool::instance().run([w = weak()] {
        if (auto sthis = w.lock()) {
            JAMI_DBG("handling picture fast update request");
            sthis->getVideoRtp().forceKeyFrame();
        }
    });
#endif
}

void
SIPCall::onPeerRinging()
{
    setState(ConnectionState::RINGING);
}

void
SIPCall::setupLocalSDPFromIce()
{
    auto media_tr = getIceMediaTransport();

    if (not media_tr) {
        JAMI_WARN("[call:%s] no media ICE transport, SDP not changed", getCallId().c_str());
        return;
    }

    // we need an initialized ICE to progress further
    if (media_tr->waitForInitialization(DEFAULT_ICE_INIT_TIMEOUT) <= 0) {
        JAMI_ERR("[call:%s] Medias' ICE init failed", getCallId().c_str());
        return;
    }

    if (const auto& ip = getSIPAccount().getPublishedIpAddress()) {
        for (unsigned compId = 1; compId <= media_tr->getComponentCount(); ++compId)
            media_tr->registerPublicIP(compId, ip);
    }

    JAMI_WARN("[call:%s] fill SDP with ICE transport %p", getCallId().c_str(), media_tr);
    sdp_->addIceAttributes(media_tr->getLocalAttributes());

    // Add video and audio channels
    sdp_->addIceCandidates(SDP_AUDIO_MEDIA_ID, media_tr->getLocalCandidates(ICE_AUDIO_RTP_COMPID));
    sdp_->addIceCandidates(SDP_AUDIO_MEDIA_ID, media_tr->getLocalCandidates(ICE_AUDIO_RTCP_COMPID));
#ifdef ENABLE_VIDEO
    sdp_->addIceCandidates(SDP_VIDEO_MEDIA_ID, media_tr->getLocalCandidates(ICE_VIDEO_RTP_COMPID));
    sdp_->addIceCandidates(SDP_VIDEO_MEDIA_ID, media_tr->getLocalCandidates(ICE_VIDEO_RTCP_COMPID));
#endif
}

std::vector<IceCandidate>
SIPCall::getAllRemoteCandidates()
{
    std::vector<IceCandidate> rem_candidates;
    auto media_tr = getIceMediaTransport();

    auto addSDPCandidates = [&, this](unsigned sdpMediaId,
                                      std::vector<IceCandidate>& out) {
        IceCandidate cand;
        for (auto& line : sdp_->getIceCandidates(sdpMediaId)) {
            if (media_tr->getCandidateFromSDP(line, cand)) {
                JAMI_DBG("[call:%s] add remote ICE candidate: %s", getCallId().c_str(), line.c_str());
                out.emplace_back(cand);
            }
        }
    };

    addSDPCandidates(SDP_AUDIO_MEDIA_ID, rem_candidates);
#ifdef ENABLE_VIDEO
    addSDPCandidates(SDP_VIDEO_MEDIA_ID, rem_candidates);
#endif

    return rem_candidates;
}

bool
SIPCall::useVideoCodec(const AccountVideoCodecInfo* codec) const
{
#ifdef ENABLE_VIDEO
    if (videortp_->isSending())
        return videortp_->useCodec(codec);
#endif
    return false;
}

void
SIPCall::startAllMedia()
{
    JAMI_WARN("[call:%s] startAllMedia()", getCallId().c_str());
    if (isSecure() && not transport_->isSecure()) {
        JAMI_ERR("[call:%s] Can't perform secure call over insecure SIP transport",
                 getCallId().c_str());
        onFailure(EPROTONOSUPPORT);
        return;
    }
    auto slots = sdp_->getMediaSlots();
    unsigned ice_comp_id = 0;
    bool peer_holding {true};
    int slotN = -1;

#ifdef ENABLE_VIDEO
    videortp_->setRequestKeyFrameCallback([wthis = weak()] {
        runOnMainThread([wthis] {
            if (auto this_ = wthis.lock())
                this_->requestKeyframe();
        });
    });
    videortp_->setChangeOrientationCallback([wthis = weak()] (int angle) {
        runOnMainThread([wthis, angle] {
            if (auto this_ = wthis.lock())
                this_->setVideoOrientation(angle);
        });
    });
#endif

    for (const auto& slot : slots) {
        ++slotN;
        const auto& local = slot.first;
        const auto& remote = slot.second;

        if (local.type != remote.type) {
            JAMI_ERR("[call:%s] [SDP:slot#%u] Inconsistent media types between local and remote",
                     getCallId().c_str(), slotN);
            continue;
        }

        RtpSession* rtp = local.type == MEDIA_AUDIO
            ? static_cast<RtpSession*>(avformatrtp_.get())
#ifdef ENABLE_VIDEO
            : static_cast<RtpSession*>(videortp_.get());
#else
            : nullptr;
#endif

        if (not rtp)
            continue;

        if (!local.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing local codec", getCallId().c_str(),
                      slotN);
            continue;
        }
        if (!remote.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing remote codec", getCallId().c_str(),
                      slotN);
            continue;
        }

        peer_holding &= remote.holding;

        if (isSecure() && (not local.crypto || not remote.crypto)) {
            JAMI_ERR("[call:%s] [SDP:slot#%u] Can't perform secure call over insecure RTP transport",
                     getCallId().c_str(), slotN);
            continue;
        }

        auto new_mtu = transport_->getTlsMtu();
        if (local.type & MEDIA_AUDIO)
            avformatrtp_->switchInput(mediaInput_);
        avformatrtp_->setMtu(new_mtu);

#ifdef ENABLE_VIDEO
        if (local.type & MEDIA_VIDEO)
            videortp_->switchInput(mediaInput_);
        videortp_->setMtu(new_mtu);
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
                rtp->start(nullptr, nullptr);
        }

        switch (local.type) {
#ifdef ENABLE_VIDEO
            case MEDIA_VIDEO:
                isVideoMuted_ = mediaInput_.empty();
                break;
#endif
            case MEDIA_AUDIO:
                isAudioMuted_ = not rtp->isSending();
                break;
            default: break;
        }
    }

    if (not isSubcall() and peerHolding_ != peer_holding) {
        peerHolding_ = peer_holding;
        emitSignal<DRing::CallSignal::PeerHold>(getCallId(), peerHolding_);
    }

    // Media is restarted, we can process the last holding request.
    isWaitingForIceAndMedia_ = false;
    if (remainingRequest_ != Request::NoRequest) {
        switch (remainingRequest_) {
        case Request::HoldingOn:
            onhold();
            break;
        case Request::HoldingOff:
            offhold();
            break;
        case Request::SwitchInput:
            SIPSessionReinvite();
            break;
        default:
            break;
        }
        remainingRequest_ = Request::NoRequest;
    }
}

void
SIPCall::restartMediaSender()
{
    JAMI_DBG("[call:%s] restarting TX media streams", getCallId().c_str());
    avformatrtp_->restartSender();
#ifdef ENABLE_VIDEO
    videortp_->restartSender();
#endif
}

void
SIPCall::stopAllMedia()
{
    JAMI_DBG("[call:%s] stopping all medias", getCallId().c_str());
    if (Recordable::isRecording())
        Recordable::stopRecording(); // if call stops, finish recording
    avformatrtp_->stop();
#ifdef ENABLE_VIDEO
    videortp_->stop();
#endif
}

void
SIPCall::muteMedia(const std::string& mediaType, bool mute)
{
    if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_VIDEO) == 0) {
#ifdef ENABLE_VIDEO
        if (mute == isVideoMuted_) return;
        JAMI_WARN("[call:%s] video muting %s", getCallId().c_str(), bool_to_str(mute));
        isVideoMuted_ = mute;
        mediaInput_ = isVideoMuted_ ? "" : Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice();
        DRing::switchInput(getCallId(), mediaInput_);
        if (not isSubcall())
            emitSignal<DRing::CallSignal::VideoMuted>(getCallId(), isVideoMuted_);
#endif
    } else if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_AUDIO) == 0) {
        if (mute == isAudioMuted_) return;
        JAMI_WARN("[call:%s] audio muting %s", getCallId().c_str(), bool_to_str(mute));
        isAudioMuted_ = mute;
        avformatrtp_->setMuted(isAudioMuted_);
        if (not isSubcall())
            emitSignal<DRing::CallSignal::AudioMuted>(getCallId(), isAudioMuted_);
    }
}

/// \brief Prepare media transport and launch media stream based on negotiated SDP
///
/// This method has to be called by link (ie SipVoIpLink) when SDP is negotiated and
/// media streams structures are knows.
/// In case of ICE transport used, the medias streams are launched asynchonously when
/// the transport is negotiated.
void
SIPCall::onMediaUpdate()
{
    JAMI_WARN("[call:%s] medias changed", getCallId().c_str());

    // If ICE is not used, start medias now
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        JAMI_WARN("[call:%s] no remote ICE for medias", getCallId().c_str());
        stopAllMedia();
        startAllMedia();
        return;
    }

    // Main call (no subcalls) must wait for ICE now, the rest of code needs to access
    // to a negotiated transport.
    if (not isSubcall())
        waitForIceAndStartMedia();
}

void
SIPCall::waitForIceAndStartMedia()
{
    // Initialization waiting task
    auto weak_call = std::weak_ptr<SIPCall>(std::static_pointer_cast<SIPCall>(shared_from_this()));
    Manager::instance().addTask([weak_call] {
        // TODO: polling algo, to it by event
        if (auto call = weak_call.lock()) {
            auto ice = call->getIceMediaTransport();

            if (ice->isFailed()) {
                JAMI_ERR("[call:%s] Media ICE init failed", call->getCallId().c_str());
                call->onFailure(EIO);
                return false;
            }

            if (!ice->isInitialized())
                return true;

            // Start transport on SDP data and wait for negotiation
            auto rem_ice_attrs = call->sdp_->getIceAttributes();
            if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
                JAMI_ERR("[call:%s] Media ICE attributes empty", call->getCallId().c_str());
                call->onFailure(EIO);
                return false;
            }
            if (not ice->start(rem_ice_attrs, call->getAllRemoteCandidates())) {
                JAMI_ERR("[call:%s] Media ICE start failed", call->getCallId().c_str());
                call->onFailure(EIO);
                return false;
            }

            // Negotiation waiting task
            Manager::instance().addTask([weak_call] {
                if (auto call = weak_call.lock()) {
                    auto ice = call->getIceMediaTransport();

                    if (ice->isFailed()) {
                        JAMI_ERR("[call:%s] Media ICE negotiation failed", call->getCallId().c_str());
                        call->onFailure(EIO);
                        return false;
                    }

                    if (not ice->isRunning())
                        return true;

                    // Nego succeed: move to the new media transport
                    call->stopAllMedia();
                    if (call->tmpMediaTransport_)
                        call->mediaTransport_ = std::move(call->tmpMediaTransport_);
                    call->startAllMedia();
                    return false;
                }
                return false;
            });

            return false;
        }
        return false;
    });
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
        getState() == CallState::HOLD);
    setRemoteSdp(offer);
    sdp_->startNegotiation();
    pjsip_inv_set_sdp_answer(inv.get(), sdp_->getLocalSdpSession());
    openPortsUPnP();
}

void
SIPCall::openPortsUPnP()
{
    if (upnp_) {
        /**
         * Try to open the desired ports with UPnP,
         * if they are used, use the alternative port and update the SDP session with the newly chosen port(s)
         *
         * TODO: the initial ports were chosen from the list of available ports and were marked as used
         *       the newly selected port should possibly be checked against the list of used ports and marked
         *       as used, the old port should be "released"
         */
        JAMI_DBG("[call:%s] opening ports via UPNP for SDP session", getCallId().c_str());
        uint16_t audio_port_used;
        if (upnp_->addAnyMapping(sdp_->getLocalAudioPort(), upnp::PortType::UDP, true, &audio_port_used)) {
            uint16_t control_port_used;
            if (upnp_->addAnyMapping(sdp_->getLocalAudioControlPort(), upnp::PortType::UDP, true, &control_port_used)) {
                sdp_->setLocalPublishedAudioPorts(audio_port_used, control_port_used);
            }
        }
#ifdef ENABLE_VIDEO
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

#ifdef ENABLE_VIDEO
    // If Video is not enabled return an empty string
    details.emplace(DRing::Call::Details::VIDEO_SOURCE, acc.isVideoEnabled() ? mediaInput_ : "");
#endif

#if HAVE_RINGNS
    if (not peerRegistredName_.empty())
        details.emplace(DRing::Call::Details::REGISTERED_NAME, peerRegistredName_);
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
                            jami::to_string(n));
        } else {
            details.emplace(DRing::TlsTransport::TLS_PEER_CERT, "");
            details.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM, "");
        }
    }
    return details;
}

bool
SIPCall::toggleRecording()
{
    // add streams to recorder before starting the record
    if (not Call::isRecording()) {
        std::stringstream ss;
        ss << "Conversation at %TIMESTAMP between "
            << getSIPAccount().getUserUri() << " and " << peerUri_;
        recorder_->setMetadata(ss.str(), ""); // use default description
        if (avformatrtp_)
            avformatrtp_->initRecorder(recorder_);
#ifdef ENABLE_VIDEO
        if (!isAudioOnly_ && videortp_)
            videortp_->initRecorder(recorder_);
#endif
    } else {
        if (avformatrtp_)
            avformatrtp_->deinitRecorder(recorder_);
#ifdef ENABLE_VIDEO
        if (!isAudioOnly_ && videortp_)
            videortp_->deinitRecorder(recorder_);
#endif
    }
    return Call::toggleRecording();
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
    // JAMI_WARN: this is not thread-safe!
    inv->mod_data[getSIPVoIPLink()->getModId()] = nullptr;
}

bool
SIPCall::initIceMediaTransport(bool master, unsigned channel_num)
{
    JAMI_DBG("[call:%s] create media ICE transport", getCallId().c_str());

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    tmpMediaTransport_ = iceTransportFactory.createTransport(getCallId().c_str(),
                                                             channel_num, master,
                                                             getAccount().getIceOptions());
    return static_cast<bool>(tmpMediaTransport_);
}

void
SIPCall::merge(Call& call)
{
    JAMI_DBG("[sipcall:%s] merge subcall %s", getCallId().c_str(), call.getCallId().c_str());

    // This static cast is safe as this method is private and overload Call::merge
    auto& subcall = static_cast<SIPCall&>(call);

    inv = std::move(subcall.inv);
    inv->mod_data[getSIPVoIPLink()->getModId()] = this;
    setTransport(subcall.transport_);
    sdp_ = std::move(subcall.sdp_);
    peerHolding_ = subcall.peerHolding_;
    upnp_ = std::move(subcall.upnp_);
    std::copy_n(subcall.contactBuffer_, PJSIP_MAX_URL_SIZE, contactBuffer_);
    pj_strcpy(&contactHeader_, &subcall.contactHeader_);
    localAudioPort_ = subcall.localAudioPort_;
    localVideoPort_ = subcall.localVideoPort_;
    mediaTransport_ = std::move(subcall.mediaTransport_);
    tmpMediaTransport_ = std::move(subcall.tmpMediaTransport_);

    Call::merge(subcall);

    waitForIceAndStartMedia();
}

void
SIPCall::setRemoteSdp(const pjmedia_sdp_session* sdp)
{
    if (!sdp)
        return;

    // If ICE is not used, start medias now
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        JAMI_WARN("[call:%s] no ICE data in remote SDP", getCallId().c_str());
        return;
    }

    if (not initIceMediaTransport(false)) {
        // Fatal condition
        // TODO: what's SIP rfc says about that?
        // (same question in waitForIceAndStartMedia)
        onFailure(EIO);
        return;
    }

    // WARNING: This call blocks! (need ice init done)
    setupLocalSDPFromIce();
}

bool
SIPCall::isIceRunning() const
{
    return mediaTransport_ and mediaTransport_->isRunning();
}

std::unique_ptr<IceSocket>
SIPCall::newIceSocket(unsigned compId)
{
    return std::unique_ptr<IceSocket> {new IceSocket(mediaTransport_, compId)};
}

} // namespace jami
