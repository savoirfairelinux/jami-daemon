/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
#include "pjsip-ua/sip_inv.h"

#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#endif

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#include "video/video_rtp_session.h"
#include "dring/videomanager_interface.h"
#include <chrono>
#include <libavutil/display.h>
#endif
#include "jamidht/channeled_transport.h"

#include "errno.h"

#include <opendht/crypto.h>
#include <opendht/thread_pool.h>

namespace jami {

using sip_utils::CONST_PJ_STR;
using namespace DRing::Call;

#ifdef ENABLE_VIDEO
static DeviceParams
getVideoSettings()
{
    const auto& videomon = jami::getVideoDeviceMonitor();
    return videomon.getDeviceParams(videomon.getDefaultDevice());
}
#endif

static constexpr std::chrono::seconds DEFAULT_ICE_INIT_TIMEOUT {35}; // seconds
static constexpr std::chrono::seconds DEFAULT_ICE_NEGO_TIMEOUT {60}; // seconds
static constexpr std::chrono::milliseconds MS_BETWEEN_2_KEYFRAME_REQUEST {1000};

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::map<std::string, std::string>& details)
    : Call(account, callId, type, details)
    , sdp_(new Sdp(callId))
{
    if (account->getUPnPActive())
        upnp_.reset(new upnp::Controller());

    setCallMediaLocal();

    // Set the media caps.
    sdp_->setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                    account->getActiveAccountCodecInfoList(MEDIA_AUDIO));
#ifdef ENABLE_VIDEO
    sdp_->setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                    account->getActiveAccountCodecInfoList(MEDIA_VIDEO));
#endif
    auto mediaAttrList = getSIPAccount()->createDefaultMediaList(getSIPAccount()->isVideoEnabled()
                                                                     and not isAudioOnly(),
                                                                 getState() == CallState::HOLD);
    initMediaStreams(mediaAttrList);
}

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::vector<MediaAttribute>& mediaAttrList)
    : Call(account, callId, type)
    , sdp_(new Sdp(callId))
{
    if (account->getUPnPActive())
        upnp_.reset(new upnp::Controller());

    setCallMediaLocal();

    // Set the media caps.
    sdp_->setLocalMediaCapabilities(MediaType::MEDIA_AUDIO,
                                    account->getActiveAccountCodecInfoList(MEDIA_AUDIO));
#ifdef ENABLE_VIDEO
    sdp_->setLocalMediaCapabilities(MediaType::MEDIA_VIDEO,
                                    account->getActiveAccountCodecInfoList(MEDIA_VIDEO));
#endif

    initMediaStreams(mediaAttrList);
}

SIPCall::~SIPCall()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        if (tmpMediaTransport_)
            dht::ThreadPool::io().run([ice = std::make_shared<decltype(tmpMediaTransport_)>(
                                           std::move(tmpMediaTransport_))] {});
    }
    setTransport({});
    setInviteSession(); // prevents callback usage
}

size_t
SIPCall::findRtpStreamIndex(const std::string& label) const
{
    const auto iter = std::find_if(rtpStreams_.begin(),
                                   rtpStreams_.end(),
                                   [&label](const RtpStream& rtp) {
                                       return (label.compare(rtp.mediaAttribute_->label_) == 0);
                                   });

    // Return the index if there is a match.
    if (iter != rtpStreams_.end())
        return std::distance(rtpStreams_.begin(), iter);

    // No match found.
    return rtpStreams_.size();
}

void
SIPCall::createRtpSession(RtpStream& stream)
{
    if (not stream.mediaAttribute_)
        throw std::runtime_error("Missing media attribute");

    if (stream.mediaAttribute_->type_ == MediaType::MEDIA_AUDIO) {
        stream.rtpSession_ = std::make_shared<AudioRtpSession>(id_);
    }
#ifdef ENABLE_VIDEO
    else if (stream.mediaAttribute_->type_ == MediaType::MEDIA_VIDEO) {
        stream.rtpSession_ = std::make_shared<video::VideoRtpSession>(id_, getVideoSettings());
    }
#endif
    else {
        throw std::runtime_error("Unsupported media type");
    }

    // Must be valid at this point.
    if (not stream.rtpSession_)
        throw std::runtime_error("Failed to create RTP Session");
    ;
}

void
SIPCall::configureRtpSession(const std::shared_ptr<RtpSession>& rtpSession,
                             const std::shared_ptr<MediaAttribute>& mediaAttr,
                             const MediaDescription& localMedia,
                             const MediaDescription& remoteMedia)
{
    if (not rtpSession)
        throw std::runtime_error("Must have a valid Audio RTP Session");

    // Configure the media stream
    auto new_mtu = transport_->getTlsMtu();
    rtpSession->setMtu(new_mtu);
    rtpSession->updateMedia(remoteMedia, localMedia);

    // Mute/un-mute media
    if (mediaAttr->muted_) {
        rtpSession->setMuted(true);
        // TODO. Setting mute to true should be enough to mute.
        // Kept for backward compatiblity.
        rtpSession->setMediaSource("");
    } else {
        rtpSession->setMuted(false);
        rtpSession->setMediaSource(mediaAttr->sourceUri_);
    }

    rtpSession->setSuccessfulSetupCb([w = weak()](MediaType type, bool isRemote) {
        if (auto thisPtr = w.lock())
            thisPtr->rtpSetupSuccess(type, isRemote);
    });

#ifdef ENABLE_VIDEO
    if (localMedia.type == MediaType::MEDIA_VIDEO) {
        auto videoRtp = std::dynamic_pointer_cast<video::VideoRtpSession>(rtpSession);
        assert(videoRtp);
        videoRtp->setRequestKeyFrameCallback([w = weak()] {
            runOnMainThread([w] {
                if (auto thisPtr = w.lock())
                    thisPtr->requestKeyframe();
            });
        });
        videoRtp->setChangeOrientationCallback([w = weak()](int angle) {
            runOnMainThread([w, angle] {
                if (auto thisPtr = w.lock())
                    thisPtr->setVideoOrientation(angle);
            });
        });
    }
#endif
}

std::shared_ptr<SIPAccountBase>
SIPCall::getSIPAccount() const
{
    return std::static_pointer_cast<SIPAccountBase>(getAccount().lock());
}

#ifdef ENABLE_PLUGIN
void
SIPCall::createCallAVStreams()
{
    if (hasVideo()) {
        auto videoRtp = getVideoRtp();
        if (not videoRtp)
            return;
        if (videoRtp->hasConference()) {
            clearCallAVStreams();
            return;
        }
    }

    auto baseId = getCallId();
    /**
     *   Map: maps the AudioFrame to an AVFrame
     **/
    auto audioMap = [](const std::shared_ptr<jami::MediaFrame>& m) -> AVFrame* {
        return std::static_pointer_cast<AudioFrame>(m)->pointer();
    };

    auto const& audioRtp = getAudioRtp();
    if (not audioRtp) {
        throw std::runtime_error("Must have a valid Audio RTP Session");
    }

    // Preview
    if (auto& localAudio = audioRtp->getAudioLocal()) {
        auto previewSubject = std::make_shared<MediaStreamSubject>(audioMap);
        StreamData microStreamData {baseId, false, StreamType::audio, getPeerNumber()};
        createCallAVStream(microStreamData, *localAudio, previewSubject);
    }

    // Receive
    if (auto& audioReceive = audioRtp->getAudioReceive()) {
        auto receiveSubject = std::make_shared<MediaStreamSubject>(audioMap);
        StreamData phoneStreamData {baseId, true, StreamType::audio, getPeerNumber()};
        createCallAVStream(phoneStreamData, (AVMediaStream&) *audioReceive, receiveSubject);
    }
#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        auto videoRtp = getVideoRtp();
        if (not videoRtp)
            return;

        // Map: maps the VideoFrame to an AVFrame
        auto map = [](const std::shared_ptr<jami::MediaFrame> m) -> AVFrame* {
            return std::static_pointer_cast<VideoFrame>(m)->pointer();
        };
        // Preview
        if (auto& videoPreview = videoRtp->getVideoLocal()) {
            auto previewSubject = std::make_shared<MediaStreamSubject>(map);
            StreamData previewStreamData {getCallId(), false, StreamType::video, getPeerNumber()};
            createCallAVStream(previewStreamData, *videoPreview, previewSubject);
        }

        // Receive
        auto& videoReceive = videoRtp->getVideoReceive();

        if (videoReceive) {
            auto receiveSubject = std::make_shared<MediaStreamSubject>(map);
            StreamData receiveStreamData {getCallId(), true, StreamType::video, getPeerNumber()};
            createCallAVStream(receiveStreamData, *videoReceive, receiveSubject);
        }
    }
#endif
}

void
SIPCall::createCallAVStream(const StreamData& StreamData,
                            AVMediaStream& streamSource,
                            const std::shared_ptr<MediaStreamSubject>& mediaStreamSubject)
{
    const std::string AVStreamId = StreamData.id + std::to_string(static_cast<int>(StreamData.type))
                                   + std::to_string(StreamData.direction);
    std::lock_guard<std::mutex> lk(avStreamsMtx_);
    auto it = callAVStreams.find(AVStreamId);
    if (it != callAVStreams.end())
        return;
    it = callAVStreams.insert(it, {AVStreamId, mediaStreamSubject});
    streamSource.attachPriorityObserver(it->second);
    jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .createAVSubject(StreamData, it->second);
}

void
SIPCall::clearCallAVStreams()
{
    std::lock_guard<std::mutex> lk(avStreamsMtx_);
    callAVStreams.clear();
}
#endif // ENABLE_PLUGIN

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
    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }

    // Reference: http://www.cs.columbia.edu/~hgs/rtp/faq.html#ports
    // We only want to set ports to new values if they haven't been set
    const unsigned callLocalAudioPort = account->generateAudioPort();
    if (localAudioPort_ != 0)
        account->releasePort(localAudioPort_);
    localAudioPort_ = callLocalAudioPort;
    sdp_->setLocalPublishedAudioPort(callLocalAudioPort);

#ifdef ENABLE_VIDEO
    // https://projects.savoirfairelinux.com/issues/17498
    const unsigned int callLocalVideoPort = account->generateVideoPort();
    if (localVideoPort_ != 0)
        account->releasePort(localVideoPort_);
    // this should already be guaranteed by SIPAccount
    assert(localAudioPort_ != callLocalVideoPort);
    localVideoPort_ = callLocalVideoPort;
    sdp_->setLocalPublishedVideoPort(callLocalVideoPort);
#endif
}

void
SIPCall::setContactHeader(pj_str_t* contact)
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

        // listen for transport destruction
        transport_->addStateListener(
            list_id,
            [wthis_ = weak()](pjsip_transport_state state, const pjsip_transport_state_info*) {
                if (auto this_ = wthis_.lock()) {
                    // end the call if the SIP transport is shut down
                    auto isAlive = SipTransport::isAlive(state);
                    if (not isAlive
                        and this_->getConnectionState() != ConnectionState::DISCONNECTED) {
                        JAMI_WARN(
                            "[call:%s] Ending call because underlying SIP transport was closed",
                            this_->getCallId().c_str());
                        this_->stopAllMedia();
                        this_->onFailure(ECONNRESET);
                    }
                }
            });
    }
}

void
SIPCall::requestReinvite()
{
    JAMI_DBG("[call:%s] Requesting a SIP re-invite", getCallId().c_str());
    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::SwitchInput;
    } else {
        auto mediaList = getMediaAttributeList();
        assert(not mediaList.empty());

        // TODO.
        // We should erase existing streams only after the new ones were
        // successfully negotiated, and make a live switch. But for now,
        // we reset all stream before creating new ones.
        rtpStreams_.clear();
        initMediaStreams(mediaList);
        if (SIPSessionReinvite(mediaList) == PJ_SUCCESS) {
            isWaitingForIceAndMedia_ = true;
        }
    }
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 */
int
SIPCall::SIPSessionReinvite(const std::vector<MediaAttribute>& mediaAttrList)
{
    assert(not mediaAttrList.empty());

    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    // Do nothing if no invitation processed yet
    if (not inviteSession_ or inviteSession_->invite_tsx)
        return PJ_SUCCESS;

    JAMI_DBG("[call:%s] Processing reINVITE (state=%s)",
             getCallId().c_str(),
             pjsip_inv_state_name(inviteSession_->state));

    // Generate new ports to receive the new media stream
    // LibAV doesn't discriminate SSRCs and will be confused about Seq changes on a given port
    generateMediaPorts();
    sdp_->clearIce();
    auto acc = getSIPAccount();
    if (not acc) {
        JAMI_ERR("No account detected");
        return !PJ_SUCCESS;
    }

    if (not sdp_->createOffer(mediaAttrList))
        return !PJ_SUCCESS;

    if (initIceMediaTransport(true))
        setupLocalSDPFromIce();

    pjsip_tx_data* tdata;
    auto local_sdp = sdp_->getLocalSdpSession();
    auto result = pjsip_inv_reinvite(inviteSession_.get(), nullptr, local_sdp, &tdata);
    if (result == PJ_SUCCESS) {
        if (!tdata)
            return PJ_SUCCESS;

        // Add user-agent header
        sip_utils::addUserAgentHeader(acc->getUserAgentName(), tdata);

        result = pjsip_inv_send_msg(inviteSession_.get(), tdata);
        if (result == PJ_SUCCESS)
            return PJ_SUCCESS;
        JAMI_ERR("[call:%s] Failed to send REINVITE msg (pjsip: %s)",
                 getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());
        // Canceling internals without sending (anyways the send has just failed!)
        pjsip_inv_cancel_reinvite(inviteSession_.get(), &tdata);
    } else
        JAMI_ERR("[call:%s] Failed to create REINVITE msg (pjsip: %s)",
                 getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());

    return !PJ_SUCCESS;
}

int
SIPCall::SIPSessionReinvite()
{
    // This version is kept for backward compatibility.
    auto const& acc = getSIPAccount();
    auto mediaList = acc->createDefaultMediaList(acc->isVideoEnabled() and not isAudioOnly(),
                                                 getState() == CallState::HOLD);
    return SIPSessionReinvite(mediaList);
}

void
SIPCall::sendSIPInfo(std::string_view body, std::string_view subtype)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (not inviteSession_ or not inviteSession_->dlg)
        throw VoipLinkException("Couldn't get invite dialog");

    constexpr pj_str_t methodName = CONST_PJ_STR("INFO");
    constexpr pj_str_t type = CONST_PJ_STR("application");

    pjsip_method method;
    pjsip_method_init_np(&method, (pj_str_t*) &methodName);

    /* Create request message. */
    pjsip_tx_data* tdata;
    if (pjsip_dlg_create_request(inviteSession_->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not create dialog", getCallId().c_str());
        return;
    }

    /* Create "application/<subtype>" message body. */
    pj_str_t content = CONST_PJ_STR(body);
    pj_str_t pj_subtype = CONST_PJ_STR(subtype);
    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &pj_subtype, &content);
    if (tdata->msg->body == NULL)
        pjsip_tx_data_dec_ref(tdata);
    else
        pjsip_dlg_send_request(inviteSession_->dlg,
                               tdata,
                               Manager::instance().sipVoIPLink().getModId(),
                               NULL);
}

void
SIPCall::updateRecState(bool state)
{
    std::string BODY = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                       "<media_control><vc_primitive><to_encoder>"
                       "<recording_state="
                       + std::to_string(state)
                       + "/>"
                         "</to_encoder></vc_primitive></media_control>";
    // see https://tools.ietf.org/html/rfc5168 for XML Schema for Media Control details

    JAMI_DBG("Sending recording state via SIP INFO");

    try {
        sendSIPInfo(BODY, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending recording state: %s", e.what());
    }
}

void
SIPCall::requestKeyframe()
{
    auto now = clock::now();
    if ((now - lastKeyFrameReq_) < MS_BETWEEN_2_KEYFRAME_REQUEST
        and lastKeyFrameReq_ != time_point::min())
        return;

    constexpr auto BODY = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                          "<media_control><vc_primitive><to_encoder>"
                          "<picture_fast_update/>"
                          "</to_encoder></vc_primitive></media_control>"sv;
    JAMI_DBG("Sending video keyframe request via SIP INFO");
    try {
        sendSIPInfo(BODY, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending video keyframe request: %s", e.what());
    }
    lastKeyFrameReq_ = now;
}

void
SIPCall::setMute(bool state)
{
    std::string BODY = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                       "<media_control><vc_primitive><to_encoder>"
                       "<mute_state="
                       + std::to_string(state)
                       + "/>"
                         "</to_encoder></vc_primitive></media_control>";
    // see https://tools.ietf.org/html/rfc5168 for XML Schema for Media Control details

    JAMI_DBG("Sending mute state via SIP INFO");

    try {
        sendSIPInfo(BODY, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending mute state: %s", e.what());
    }
}

void
SIPCall::setInviteSession(pjsip_inv_session* inviteSession)
{
    if (inviteSession == nullptr and inviteSession_) {
        JAMI_DBG("[call:%s] Delete current invite session", getCallId().c_str());
    } else if (inviteSession != nullptr) {
        JAMI_DBG("[call:%s] Set new invite session [%p]", getCallId().c_str(), inviteSession);
    } else {
        // Nothing to do.
        return;
    }

    inviteSession_.reset(inviteSession);
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
    if (inviteSession_ and inviteSession_->state != PJSIP_INV_STATE_DISCONNECTED) {
        pjsip_tx_data* tdata = nullptr;
        auto ret = pjsip_inv_end_session(inviteSession_.get(), status, nullptr, &tdata);
        if (ret == PJ_SUCCESS) {
            if (tdata) {
                auto account = getSIPAccount();
                if (account) {
                    auto contact = account->getContactHeader(transport_ ? transport_->get()
                                                                        : nullptr);
                    sip_utils::addContactHeader(&contact, tdata);
                } else {
                    JAMI_ERR("No account detected");
                }

                // Add user-agent header
                sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

                ret = pjsip_inv_send_msg(inviteSession_.get(), tdata);
                if (ret != PJ_SUCCESS)
                    JAMI_ERR("[call:%s] failed to send terminate msg, SIP error (%s)",
                             getCallId().c_str(),
                             sip_utils::sip_strerror(ret).c_str());
            }
        } else
            JAMI_ERR("[call:%s] failed to terminate INVITE@%p, SIP error (%s)",
                     getCallId().c_str(),
                     inviteSession_.get(),
                     sip_utils::sip_strerror(ret).c_str());
    }
    setInviteSession();
}

void
SIPCall::answer()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }

    if (not inviteSession_)
        throw VoipLinkException("[call:" + getCallId()
                                + "] answer: no invite session for this call");

    if (!inviteSession_->neg) {
        JAMI_WARN("[call:%s] Negotiator is NULL, we've received an INVITE without an SDP",
                  getCallId().c_str());
        pjmedia_sdp_session* dummy = 0;
        Manager::instance().sipVoIPLink().createSDPOffer(inviteSession_.get(), &dummy);

        if (account->isStunEnabled())
            updateSDPFromSTUN();
    }

    pj_str_t contact(account->getContactHeader(transport_ ? transport_->get() : nullptr));
    setContactHeader(&contact);

    pjsip_tx_data* tdata;
    if (!inviteSession_->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(inviteSession_.get(),
                         PJSIP_SC_OK,
                         NULL,
                         !inviteSession_->neg ? sdp_->getLocalSdpSession() : NULL,
                         &tdata)
        != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    // contactStr must stay in scope as long as tdata
    if (contactHeader_.slen) {
        JAMI_DBG("[call:%s] Answering with contact header: %.*s",
                 getCallId().c_str(),
                 (int) contactHeader_.slen,
                 contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        setInviteSession();
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::hangup(int reason)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (inviteSession_ and inviteSession_->dlg) {
        pjsip_route_hdr* route = inviteSession_->dlg->route_set.next;
        while (route and route != &inviteSession_->dlg->route_set) {
            char buf[1024];
            int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));
            if (printed >= 0) {
                buf[printed] = '\0';
                JAMI_DBG("[call:%s] Route header %s", getCallId().c_str(), buf);
            }
            route = route->next;
        }

        int status = PJSIP_SC_OK;
        if (reason)
            status = reason;
        else if (inviteSession_->state <= PJSIP_INV_STATE_EARLY
                 and inviteSession_->role != PJSIP_ROLE_UAC)
            status = PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
        else if (inviteSession_->state >= PJSIP_INV_STATE_DISCONNECTED)
            status = PJSIP_SC_DECLINE;

        // Notify the peer
        terminateSipSession(status);
    }

    // Stop all RTP streams
    stopAllMedia();
    setState(Call::ConnectionState::DISCONNECTED, reason);
    dht::ThreadPool::io().run([w = weak()] {
        if (auto shared = w.lock())
            shared->removeCall();
    });
}

void
SIPCall::refuse()
{
    if (!isIncoming() or getConnectionState() == ConnectionState::CONNECTED or !inviteSession_)
        return;

    stopAllMedia();

    // Notify the peer
    terminateSipSession(PJSIP_SC_BUSY_HERE);

    setState(Call::ConnectionState::DISCONNECTED, ECONNABORTED);
    removeCall();
}

static void
transfer_client_cb(pjsip_evsub* sub, pjsip_event* event)
{
    auto mod_ua_id = Manager::instance().sipVoIPLink().getModId();

    switch (pjsip_evsub_get_state(sub)) {
    case PJSIP_EVSUB_STATE_ACCEPTED:
        if (!event)
            return;

        pj_assert(event->type == PJSIP_EVENT_TSX_STATE
                  && event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
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

        pjsip_status_line status_line = {500, *pjsip_get_status_text(500)};

        if (!r_data->msg_info.msg)
            return;

        if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD
            and request.find("NOTIFY") != std::string::npos) {
            pjsip_msg_body* body = r_data->msg_info.msg->body;

            if (!body)
                return;

            if (pj_stricmp2(&body->content_type.type, "message")
                or pj_stricmp2(&body->content_type.subtype, "sipfrag"))
                return;

            if (pjsip_parse_status_line((char*) body->data, body->len, &status_line) != PJ_SUCCESS)
                return;
        }

        if (!r_data->msg_info.cid)
            return;

        auto call = static_cast<SIPCall*>(pjsip_evsub_get_mod_data(sub, mod_ua_id));
        if (!call)
            return;

        if (status_line.code / 100 == 2) {
            if (call->inviteSession_)
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
SIPCall::transferCommon(const pj_str_t* dst)
{
    if (not inviteSession_ or not inviteSession_->dlg)
        return false;

    pjsip_evsub_user xfer_cb;
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    pjsip_evsub* sub;

    if (pjsip_xfer_create_uac(inviteSession_->dlg, &xfer_cb, &sub) != PJ_SUCCESS)
        return false;

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data(sub, Manager::instance().sipVoIPLink().getModId(), this);

    /*
     * Create REFER request.
     */
    pjsip_tx_data* tdata;

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
    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }

    if (Recordable::isRecording()) {
        deinitRecorder();
        stopRecording();
    }

    std::string toUri = account->getToUri(to);
    const pj_str_t dst(CONST_PJ_STR(toUri));
    JAMI_DBG("[call:%s] Transferring to %.*s", getCallId().c_str(), (int) dst.slen, dst.ptr);

    if (!transferCommon(&dst))
        throw VoipLinkException("Couldn't transfer");
}

bool
SIPCall::attendedTransfer(const std::string& to)
{
    auto toCall = Manager::instance().callFactory.getCall<SIPCall>(to);
    if (!toCall)
        return false;

    if (not toCall->inviteSession_ or not toCall->inviteSession_->dlg)
        return false;

    pjsip_dialog* target_dlg = toCall->inviteSession_->dlg;
    pjsip_uri* uri = (pjsip_uri*) pjsip_uri_get_uri(target_dlg->remote.info->uri);

    char str_dest_buf[PJSIP_MAX_URL_SIZE * 2] = {'<'};
    pj_str_t dst = {str_dest_buf, 1};

    dst.slen += pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
                                uri,
                                str_dest_buf + 1,
                                sizeof(str_dest_buf) - 1);
    dst.slen += pj_ansi_snprintf(str_dest_buf + dst.slen,
                                 sizeof(str_dest_buf) - dst.slen,
                                 "?"
                                 "Replaces=%.*s"
                                 "%%3Bto-tag%%3D%.*s"
                                 "%%3Bfrom-tag%%3D%.*s>",
                                 (int) target_dlg->call_id->id.slen,
                                 target_dlg->call_id->id.ptr,
                                 (int) target_dlg->remote.info->tag.slen,
                                 target_dlg->remote.info->tag.ptr,
                                 (int) target_dlg->local.info->tag.slen,
                                 target_dlg->local.info->tag.ptr);

    return transferCommon(&dst);
}

bool
SIPCall::onhold(OnReadyCb&& cb)
{
    // If ICE is currently negotiating, we must wait before hold the call
    if (isWaitingForIceAndMedia_) {
        holdCb_ = std::move(cb);
        remainingRequest_ = Request::HoldingOn;
        return false;
    }

    auto result = hold();

    if (cb)
        cb(result);

    return result;
}

bool
SIPCall::hold()
{
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
SIPCall::offhold(OnReadyCb&& cb)
{
    // If ICE is currently negotiating, we must wait before unhold the call
    if (isWaitingForIceAndMedia_) {
        offHoldCb_ = std::move(cb);
        remainingRequest_ = Request::HoldingOff;
        return false;
    }

    auto result = unhold();

    if (cb)
        cb(result);

    return result;
}

bool
SIPCall::unhold()
{
    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return false;
    }

    bool success = false;
    try {
        if (account->isStunEnabled())
            success = internalOffHold([&] { updateSDPFromSTUN(); });
        else
            success = internalOffHold([] {});

    } catch (const SdpException& e) {
        JAMI_ERR("[call:%s] %s", getCallId().c_str(), e.what());
        throw VoipLinkException("SDP issue in offhold");
    }

    if (success)
        isWaitingForIceAndMedia_ = true;

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
            if (isWaitingForIceAndMedia_) {
                remainingRequest_ = Request::HoldingOn;
            } else {
                hold();
            }
            return false;
        }
    }

    return true;
}

void
SIPCall::switchInput(const std::string& source)
{
    JAMI_DBG("[call:%s] Set selected source to %s", getCallId().c_str(), source.c_str());

    for (auto const& stream : rtpStreams_) {
        auto mediaAttr = stream.mediaAttribute_;
        mediaAttr->sourceUri_ = source;
    }

    // Check if the call is being recorded in order to continue
    // ... the recording after the switch
    bool isRec = Call::isRecording();

    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::SwitchInput;
    } else {
        if (SIPSessionReinvite() == PJ_SUCCESS) {
            isWaitingForIceAndMedia_ = true;
        }
    }
    if (isRec) {
        readyToRecord_ = false;
        resetMediaReady();
        pendingRecord_ = true;
    }
}

void
SIPCall::peerHungup()
{
    // Stop all RTP streams
    stopAllMedia();

    if (inviteSession_)
        terminateSipSession(PJSIP_SC_NOT_FOUND);

    Call::peerHungup();
}

void
SIPCall::carryingDTMFdigits(char code)
{
    int duration = Manager::instance().voipPreferences.getPulseLength();
    char dtmf_body[1000];
    int ret;

    // handle flash code
    if (code == '!') {
        ret = snprintf(dtmf_body, sizeof dtmf_body - 1, "Signal=16\r\nDuration=%d\r\n", duration);
    } else {
        ret = snprintf(dtmf_body,
                       sizeof dtmf_body - 1,
                       "Signal=%c\r\nDuration=%d\r\n",
                       code,
                       duration);
    }

    try {
        sendSIPInfo({dtmf_body, (size_t) ret}, "dtmf-relay");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending DTMF: %s", e.what());
    }
}

void
SIPCall::setVideoOrientation(int rotation)
{
    std::string sip_body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                           "<media_control><vc_primitive><to_encoder>"
                           "<device_orientation="
                           + std::to_string(-rotation)
                           + "/>"
                             "</to_encoder></vc_primitive></media_control>";

    JAMI_DBG("Sending device orientation via SIP INFO");

    sendSIPInfo(sip_body, "media_control+xml");
}

void
SIPCall::sendTextMessage(const std::map<std::string, std::string>& messages, const std::string& from)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    // TODO: for now we ignore the "from" (the previous implementation for sending this info was
    //      buggy and verbose), another way to send the original message sender will be implemented
    //      in the future
    if (not subcalls_.empty()) {
        pendingOutMessages_.emplace_back(messages, from);
        for (auto& c : subcalls_)
            c->sendTextMessage(messages, from);
    } else {
        if (inviteSession_) {
            try {
                im::sendSipMessage(inviteSession_.get(), messages);
            } catch (...) {
            }
        } else {
            pendingOutMessages_.emplace_back(messages, from);
            JAMI_ERR("[call:%s] sendTextMessage: no invite session for this call",
                     getCallId().c_str());
        }
    }
}

void
SIPCall::removeCall()
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance().getJamiPluginManager().getCallServicesManager().clearCallHandlerMaps(
        getCallId());
#endif
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    JAMI_WARN("[call:%s] removeCall()", getCallId().c_str());
    if (sdp_) {
        sdp_->setActiveLocalSdpSession(nullptr);
        sdp_->setActiveRemoteSdpSession(nullptr);
    }
    Call::removeCall();
    if (mediaTransport_)
        dht::ThreadPool::io().run([ice = std::move(mediaTransport_)] {});
    if (tmpMediaTransport_)
        dht::ThreadPool::io().run([ice = std::make_shared<decltype(tmpMediaTransport_)>(
                                       std::move(tmpMediaTransport_))] {});
    setInviteSession();
    setTransport({});
}

void
SIPCall::onFailure(signed cause)
{
    if (setState(CallState::MERROR, ConnectionState::DISCONNECTED, cause)) {
        runOnMainThread([w = weak()] {
            if (auto shared = w.lock()) {
                auto& call = *shared;
                Manager::instance().callFailure(call);
                call.removeCall();
            }
        });
    }
}

void
SIPCall::onBusyHere()
{
    if (getCallType() == CallType::OUTGOING)
        setState(CallState::PEER_BUSY, ConnectionState::DISCONNECTED);
    else
        setState(CallState::BUSY, ConnectionState::DISCONNECTED);

    runOnMainThread([w = weak()] {
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
    runOnMainThread([w = weak()] {
        if (auto shared = w.lock()) {
            auto& call = *shared;
            Manager::instance().peerHungupCall(call);
            call.removeCall();
        }
    });
}

void
SIPCall::onAnswered()
{
    JAMI_WARN("[call:%s] onAnswered()", getCallId().c_str());
    runOnMainThread([w = weak()] {
        if (auto shared = w.lock()) {
            if (shared->getConnectionState() != ConnectionState::CONNECTED) {
                shared->setState(CallState::ACTIVE, ConnectionState::CONNECTED);
                if (not shared->isSubcall()) {
                    Manager::instance().peerAnsweredCall(*shared);
                }
            }
        }
    });
}

void
SIPCall::sendKeyframe()
{
#ifdef ENABLE_VIDEO
    dht::ThreadPool::computation().run([w = weak()] {
        if (auto sthis = w.lock()) {
            JAMI_DBG("handling picture fast update request");
            if (auto const& videoRtp = sthis->getVideoRtp()) {
                videoRtp->forceKeyFrame();
            }
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

    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }
    if (!sdp_) {
        JAMI_ERR("No sdp detected");
        return;
    }

    JAMI_DBG("[call:%s] fill SDP with ICE transport %p", getCallId().c_str(), media_tr);
    sdp_->addIceAttributes(media_tr->getLocalAttributes());

    unsigned idx = 0;
    unsigned compId = 1;
    for (auto const& stream : rtpStreams_) {
        JAMI_DBG("[call:%s] add ICE local candidates for media [%s] at index %u",
                 getCallId().c_str(),
                 stream.mediaAttribute_->label_.c_str(),
                 idx);
        // RTP
        sdp_->addIceCandidates(idx, media_tr->getLocalCandidates(compId));
        // RTCP
        sdp_->addIceCandidates(idx, media_tr->getLocalCandidates(compId + 1));

        idx++;
        compId += 2;
    }
}

std::vector<IceCandidate>
SIPCall::getAllRemoteCandidates()
{
    auto media_tr = getIceMediaTransport();

    if (not media_tr) {
        JAMI_WARN("[call:%s] no media ICE transport", getCallId().c_str());
        return {};
    }

    std::vector<IceCandidate> rem_candidates;
    for (unsigned mediaIdx = 0; mediaIdx < static_cast<unsigned>(rtpStreams_.size()); mediaIdx++) {
        IceCandidate cand;
        for (auto& line : sdp_->getIceCandidates(mediaIdx)) {
            if (media_tr->getCandidateFromSDP(line, cand)) {
                JAMI_DBG("[call:%s] add remote ICE candidate: %s",
                         getCallId().c_str(),
                         line.c_str());
                rem_candidates.emplace_back(cand);
            }
        }
    }
    return rem_candidates;
}

std::shared_ptr<AccountCodecInfo>
SIPCall::getVideoCodec() const
{
#ifdef ENABLE_VIDEO
    if (auto const& videoRtp = getVideoRtp())
        return videoRtp->getCodec();
#endif
    return {};
}

std::shared_ptr<AccountCodecInfo>
SIPCall::getAudioCodec() const
{
    if (auto const& audioRtp = getAudioRtp())
        return audioRtp->getCodec();
    return {};
}

void
SIPCall::addMediaStream(const MediaAttribute& mediaAttr)
{
    // Create and add the media stream with the provided attribute.
    // Do not create the RTP sessions yet.
    RtpStream stream;
    stream.mediaAttribute_ = std::make_shared<MediaAttribute>(mediaAttr);

    // Set default media source if empty. Kept for backward compatibility.
#ifdef ENABLE_VIDEO
    if (stream.mediaAttribute_->sourceUri_.empty()) {
        stream.mediaAttribute_->sourceUri_
            = Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice();
    }
#endif

    rtpStreams_.emplace_back(std::move(stream));
}

size_t
SIPCall::initMediaStreams(const std::vector<MediaAttribute>& mediaAttrList)
{
    for (size_t idx = 0; idx < mediaAttrList.size(); idx++) {
        auto const& mediaAttr = mediaAttrList.at(idx);
        if (mediaAttr.type_ != MEDIA_AUDIO && mediaAttr.type_ != MEDIA_VIDEO) {
            JAMI_ERR("[call:%s] Unexpected media type %u", getCallId().c_str(), mediaAttr.type_);
            assert(false);
        }

        addMediaStream(mediaAttr);
        auto& stream = rtpStreams_.back();
        createRtpSession(stream);

        JAMI_DBG("[call:%s] Added media @%lu: %s",
                 getCallId().c_str(),
                 idx,
                 stream.mediaAttribute_->toString(true).c_str());
    }

    assert(rtpStreams_.size() == mediaAttrList.size());

    JAMI_DBG("[call:%s] Created %lu Media streams", getCallId().c_str(), rtpStreams_.size());

    return rtpStreams_.size();
}

void
SIPCall::startAllMedia()
{
    if (!transport_ or !sdp_)
        return;
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

    // reset
    readyToRecord_ = false;
    resetMediaReady();

    for (const auto& slot : slots) {
        ++slotN;
        const auto& local = slot.first;
        const auto& remote = slot.second;

        if (local.type != MEDIA_AUDIO && local.type != MEDIA_VIDEO) {
            JAMI_ERR("[call:%s] Unexpected media type %u", getCallId().c_str(), local.type);
            throw std::runtime_error("Invalid media attribute");
        }

        if (local.type != remote.type) {
            JAMI_ERR("[call:%s] [SDP:slot#%u] Inconsistent media type between local and remote",
                     getCallId().c_str(),
                     slotN);
            continue;
        }

        if (!local.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing local codec", getCallId().c_str(), slotN);
            continue;
        }
        if (!remote.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing remote codec", getCallId().c_str(), slotN);
            continue;
        }

        if (isSecure() && (not local.crypto || not remote.crypto)) {
            JAMI_WARN(
                "[call:%s] [SDP:slot#%u] Secure mode but no crypto attributes. Ignoring the media",
                getCallId().c_str(),
                slotN);
            continue;
        }

        auto const& rtpStream = rtpStreams_[slotN];
        if (not rtpStream.mediaAttribute_) {
            throw std::runtime_error("Missing media attribute");
        }

        // Configure the media.
        configureRtpSession(rtpStream.rtpSession_, rtpStream.mediaAttribute_, local, remote);

        // Not restarting media loop on hold as it's a huge waste of CPU ressources
        // because of the audio loop
        if (getState() != CallState::HOLD) {
            if (isIceRunning()) {
                rtpStream.rtpSession_->start(newIceSocket(ice_comp_id),
                                             newIceSocket(ice_comp_id + 1));
                ice_comp_id += 2;
            } else
                rtpStream.rtpSession_->start(nullptr, nullptr);
        }

        // Aggregate holding info over all remote streams
        peer_holding &= remote.onHold;
    }

    if (not isSubcall() and peerHolding_ != peer_holding) {
        peerHolding_ = peer_holding;
        emitSignal<DRing::CallSignal::PeerHold>(getCallId(), peerHolding_);
    }

#ifdef ENABLE_VIDEO
    // Check if there is an un-muted video stream
    auto const& iter = std::find_if(rtpStreams_.begin(),
                                    rtpStreams_.end(),
                                    [](const RtpStream& stream) {
                                        return stream.mediaAttribute_->type_
                                                   == MediaType::MEDIA_VIDEO
                                               and not stream.mediaAttribute_->muted_;
                                    });

    // Video is muted if all video streams are muted.
    isVideoMuted_ = iter == rtpStreams_.end();
#endif

    // Media is restarted, we can process the last holding request.
    isWaitingForIceAndMedia_ = false;
    if (remainingRequest_ != Request::NoRequest) {
        bool result = true;
        switch (remainingRequest_) {
        case Request::HoldingOn:
            result = hold();
            if (holdCb_) {
                holdCb_(result);
                holdCb_ = nullptr;
            }
            break;
        case Request::HoldingOff:
            result = unhold();
            if (offHoldCb_) {
                offHoldCb_(result);
                offHoldCb_ = nullptr;
            }
            break;
        case Request::SwitchInput:
            SIPSessionReinvite();
            break;
        default:
            break;
        }
        remainingRequest_ = Request::NoRequest;
    }

#ifdef ENABLE_PLUGIN
    // Create AVStreams associated with the call
    createCallAVStreams();
#endif
}

void
SIPCall::restartMediaSender()
{
    JAMI_DBG("[call:%s] restarting TX media streams", getCallId().c_str());
    auto const& audioRtp = getAudioRtp();
    if (audioRtp)
        audioRtp->restartSender();

#ifdef ENABLE_VIDEO
    if (hasVideo()) {
        auto const& videoRtp = getVideoRtp();
        if (videoRtp)
            videoRtp->restartSender();
    }
#endif
}

void
SIPCall::stopAllMedia()
{
    JAMI_DBG("[call:%s] stopping all medias", getCallId().c_str());
    if (Recordable::isRecording()) {
        deinitRecorder();
        stopRecording(); // if call stops, finish recording
    }
    auto const& audioRtp = getAudioRtp();
    if (audioRtp)
        audioRtp->stop();
#ifdef ENABLE_VIDEO
    auto const& videoRtp = getVideoRtp();
    if (videoRtp)
        videoRtp->stop();
#endif

#ifdef ENABLE_PLUGIN
    {
        clearCallAVStreams();
        std::lock_guard<std::mutex> lk(avStreamsMtx_);
        Manager::instance().getJamiPluginManager().getCallServicesManager().clearAVSubject(
            getCallId());
    }
#endif
}

void
SIPCall::muteMedia(const std::string& mediaType, bool mute)
{
    auto type = MediaAttribute::stringToMediaType(mediaType);

    if (type == MediaType::MEDIA_AUDIO) {
        JAMI_WARN("[call:%s] %s all audio medias",
                  getCallId().c_str(),
                  mute ? "muting " : "un-muting ");

    } else if (type == MediaType::MEDIA_VIDEO) {
        JAMI_WARN("[call:%s] %s all video medias",
                  getCallId().c_str(),
                  mute ? "muting" : "un-muting");
    } else {
        JAMI_ERR("[call:%s] invalid media type %s", getCallId().c_str(), mediaType.c_str());
        assert(false);
    }

    // Get the current media attributes.
    auto mediaList = getMediaAttributeList();

    // Mute/Un-mute all medias with matching type.
    for (auto& mediaAttr : mediaList) {
        if (mediaAttr.type_ == type) {
            mediaAttr.muted_ = mute;
        }
    }

    // Apply
    requestMediaChange(mediaList);
}

bool
SIPCall::updateMediaStreamInternal(const MediaAttribute& newMediaAttr, size_t streamIdx)
{
    assert(streamIdx < rtpStreams_.size());

    auto const& rtpStream = rtpStreams_[streamIdx];
    assert(rtpStream.rtpSession_);

    auto const& mediaAttr = rtpStream.mediaAttribute_;
    assert(mediaAttr);

    bool requireReinvite = false;

    if (newMediaAttr.muted_ == mediaAttr->muted_) {
        // Nothing to do. Already in the desired state.
        JAMI_DBG("[call:%s] [%s] already %s",
                 getCallId().c_str(),
                 mediaAttr->label_.c_str(),
                 mediaAttr->muted_ ? "muted " : "un-muted ");

        return requireReinvite;
    }

    // Update
    mediaAttr->muted_ = newMediaAttr.muted_;
    mediaAttr->sourceUri_ = newMediaAttr.sourceUri_;

    JAMI_DBG("[call:%s] %s [%s]",
             getCallId().c_str(),
             mediaAttr->muted_ ? "muting" : "un-muting",
             mediaAttr->label_.c_str());

    if (mediaAttr->type_ == MediaType::MEDIA_AUDIO) {
        rtpStream.rtpSession_->setMuted(mediaAttr->muted_);
        if (not isSubcall())
            emitSignal<DRing::CallSignal::AudioMuted>(getCallId(), mediaAttr->muted_);
        setMute(mediaAttr->muted_);
        return requireReinvite;
    }

#ifdef ENABLE_VIDEO
    if (mediaAttr->type_ == MediaType::MEDIA_VIDEO) {
        if (not isSubcall())
            emitSignal<DRing::CallSignal::VideoMuted>(getCallId(), mediaAttr->muted_);

        // Changes in video attributes always trigger a re-invite.
        requireReinvite = true;
    }
#endif

    return requireReinvite;
}

bool
SIPCall::requestMediaChange(const std::vector<MediaAttribute>& mediaAttrList)
{
    JAMI_DBG("[call:%s] New local medias", getCallId().c_str());

    unsigned idx = 0;
    for (auto const& newMediaAttr : mediaAttrList) {
        JAMI_DBG("[call:%s] Media @%u: %s",
                 getCallId().c_str(),
                 idx,
                 newMediaAttr.toString(true).c_str());
        idx++;
    }

    bool reinviteRequired = false;

    for (auto const& newMediaAttr : mediaAttrList) {
        auto streamIdx = findRtpStreamIndex(newMediaAttr.label_);
        if (streamIdx == rtpStreams_.size()) {
            // Media does not exist, add a new one.
            addMediaStream(newMediaAttr);
            auto& stream = rtpStreams_.back();
            createRtpSession(stream);
            JAMI_DBG(
                "[call:%s] Added a new media stream - type: %s - @ index %lu. Require a re-invite",
                getCallId().c_str(),
                stream.mediaAttribute_->type_ == MediaType::MEDIA_AUDIO ? "AUDIO" : "VIDEO",
                streamIdx);
            // Needs a new SDP and reinvite.
            reinviteRequired = true;
        } else {
            reinviteRequired |= updateMediaStreamInternal(newMediaAttr, streamIdx);
        }
    }

    if (reinviteRequired)
        requestReinvite();

    return true;
}

std::vector<MediaAttribute>
SIPCall::getMediaAttributeList() const
{
    std::vector<MediaAttribute> mediaList;
    for (auto const& stream : rtpStreams_) {
        mediaList.emplace_back(*stream.mediaAttribute_);
    }
    return mediaList;
}

/// \brief Prepare media transport and launch media stream based on negotiated SDP
///
/// This method has to be called by link (ie SipVoIpLink) when SDP is negotiated and
/// media streams structures are knows.
/// In case of ICE transport used, the medias streams are launched asynchronously when
/// the transport is negotiated.
void
SIPCall::onMediaUpdate()
{
    // Main call (no subcalls) must wait for ICE now, the rest of code needs to access
    // to a negotiated transport.
    runOnMainThread([w = weak()] {
        if (auto this_ = w.lock()) {
            std::lock_guard<std::recursive_mutex> lk {this_->callMutex_};
            JAMI_WARN("[call:%s] medias changed", this_->getCallId().c_str());
            // The call is already ended, so we don't need to restart medias
            if (not this_->inviteSession_
                or this_->inviteSession_->state == PJSIP_INV_STATE_DISCONNECTED or not this_->sdp_)
                return;
            // If ICE is not used, start medias now
            auto rem_ice_attrs = this_->sdp_->getIceAttributes();
            if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
                JAMI_WARN("[call:%s] no remote ICE for medias", this_->getCallId().c_str());
                this_->stopAllMedia();
                this_->startAllMedia();
                return;
            }
            if (not this_->isSubcall())
                this_->startIceMedia();
        }
    });
}

void
SIPCall::startIceMedia()
{
    auto ice = getIceMediaTransport();

    if (not ice or ice->isFailed()) {
        JAMI_ERR("[call:%s] Media ICE init failed", getCallId().c_str());
        onFailure(EIO);
        return;
    }

    if (ice->isStarted()) {
        // NOTE: for incoming calls, the ice is already there and running
        if (ice->isRunning())
            onIceNegoSucceed();
        return;
    }

    if (!ice->isInitialized()) {
        // In this case, onInitDone will occurs after the startIceMedia
        waitForIceInit_ = true;
        return;
    }

    // Start transport on SDP data and wait for negotiation
    if (!sdp_)
        return;
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        JAMI_ERR("[call:%s] Media ICE attributes empty", getCallId().c_str());
        onFailure(EIO);
        return;
    }
    if (not ice->startIce(rem_ice_attrs, std::move(getAllRemoteCandidates()))) {
        JAMI_ERR("[call:%s] Media ICE start failed", getCallId().c_str());
        onFailure(EIO);
    }
}

void
SIPCall::onIceNegoSucceed()
{
    // Check if the call is already ended, so we don't need to restart medias
    // This is typically the case in a multi-device context where one device
    // can stop a call. So do not start medias
    if (not inviteSession_ or inviteSession_->state == PJSIP_INV_STATE_DISCONNECTED or not sdp_)
        return;
    // Nego succeed: move to the new media transport
    stopAllMedia();
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        if (tmpMediaTransport_) {
            // Destroy the ICE media transport on another thread. This can take
            // quite some time.
            if (mediaTransport_)
                dht::ThreadPool::io().run([ice = std::move(mediaTransport_)] {});
            mediaTransport_ = std::move(tmpMediaTransport_);
        }
    }
    startAllMedia();
}

int
SIPCall::onReceiveOffer(const pjmedia_sdp_session* offer, const pjsip_rx_data* rdata)
{
    if (!sdp_)
        return !PJ_SUCCESS;
    sdp_->clearIce();
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return !PJ_SUCCESS;
    }

    // TODO. WARNING. If this is an ongoing audio-only call, and the peer added
    // video in a subsequent offer, the local user MUST first accept the new media(s).
    // This list should be provided by the client. Kept for backward compatibility.
    auto mediaList = acc->createDefaultMediaList(acc->isVideoEnabled(),
                                                 getState() == CallState::HOLD);
    if (offer) {
        sdp_->receiveOffer(offer, mediaList);

        setRemoteSdp(offer);
        sdp_->startNegotiation();
    }

    pjsip_tx_data* tdata = nullptr;

    if (pjsip_inv_initial_answer(inviteSession_.get(),
                                 const_cast<pjsip_rx_data*>(rdata),
                                 PJSIP_SC_OK,
                                 NULL,
                                 NULL,
                                 &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create initial answer OK");
        return !PJ_SUCCESS;
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(getSIPAccount()->getUserAgentName(), tdata);

    if (pjsip_inv_answer(inviteSession_.get(), PJSIP_SC_OK, NULL, sdp_->getLocalSdpSession(), &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer OK");
        return !PJ_SUCCESS;
    }

    // ContactStr must stay in scope as long as tdata
    const pj_str_t contactStr(getSIPAccount()->getContactHeader(getTransport()->get()));
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not send msg OK");
        return !PJ_SUCCESS;
    }

    openPortsUPnP();

    return PJ_SUCCESS;
}

void
SIPCall::openPortsUPnP()
{
    if (upnp_ and sdp_) {
        /**
         * Try to open the desired ports with UPnP,
         * if they are used, use the alternative port and update the SDP session with the newly
         * chosen port(s)
         *
         * TODO:
         * No need to request mappings for specfic port numbers. Set the port to '0' to
         * request the first available port (faster and more likely to succeed).
         */
        JAMI_DBG("[call:%s] opening ports via UPNP for SDP session", getCallId().c_str());

        upnp_->reserveMapping(sdp_->getLocalAudioPort(), upnp::PortType::UDP);
        // RTCP port.
        upnp_->reserveMapping(sdp_->getLocalAudioControlPort(), upnp::PortType::UDP);

#ifdef ENABLE_VIDEO
        // RTP port.
        upnp_->reserveMapping(sdp_->getLocalVideoPort(), upnp::PortType::UDP);
        // RTCP port.
        upnp_->reserveMapping(sdp_->getLocalVideoControlPort(), upnp::PortType::UDP);
#endif
    }
}

std::map<std::string, std::string>
SIPCall::getDetails() const
{
    auto details = Call::getDetails();
    details.emplace(DRing::Call::Details::PEER_HOLDING, peerHolding_ ? TRUE_STR : FALSE_STR);

    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return {};
    }

#ifdef ENABLE_VIDEO
    for (auto const& stream : rtpStreams_) {
        if (stream.mediaAttribute_->type_ != MediaType::MEDIA_VIDEO)
            continue;
        details.emplace(DRing::Call::Details::VIDEO_SOURCE, stream.mediaAttribute_->sourceUri_);
        if (auto const& rtpSession = stream.rtpSession_) {
            if (auto codec = rtpSession->getCodec())
                details.emplace(DRing::Call::Details::VIDEO_CODEC, codec->systemCodecInfo.name);
            else
                details.emplace(DRing::Call::Details::VIDEO_CODEC, "");
        }
    }
#endif

#if HAVE_RINGNS
    if (not peerRegisteredName_.empty())
        details.emplace(DRing::Call::Details::REGISTERED_NAME, peerRegisteredName_);
#endif

#ifdef ENABLE_CLIENT_CERT
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    if (transport_ and transport_->isSecure()) {
        const auto& tlsInfos = transport_->getTlsInfos();
        if (tlsInfos.cipher != PJ_TLS_UNKNOWN_CIPHER) {
            const auto& cipher = pj_ssl_cipher_name(tlsInfos.cipher);
            details.emplace(DRing::TlsTransport::TLS_CIPHER, cipher ? cipher : "");
        } else {
            details.emplace(DRing::TlsTransport::TLS_CIPHER, "");
        }
        if (tlsInfos.peerCert) {
            details.emplace(DRing::TlsTransport::TLS_PEER_CERT, tlsInfos.peerCert->toString());
            auto ca = tlsInfos.peerCert->issuer;
            unsigned n = 0;
            while (ca) {
                std::ostringstream name_str;
                name_str << DRing::TlsTransport::TLS_PEER_CA_ << n++;
                details.emplace(name_str.str(), ca->toString());
                ca = ca->issuer;
            }
            details.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM, std::to_string(n));
        } else {
            details.emplace(DRing::TlsTransport::TLS_PEER_CERT, "");
            details.emplace(DRing::TlsTransport::TLS_PEER_CA_NUM, "");
        }
    }
#endif
    return details;
}

void
SIPCall::enterConference(const std::string& confId)
{
#ifdef ENABLE_VIDEO
    auto conf = Manager::instance().getConferenceFromID(confId);
    if (conf == nullptr) {
        JAMI_ERR("Unknown conference [%s]", confId.c_str());
        return;
    }
    auto const& videoRtp = getVideoRtp();
    if (videoRtp)
        videoRtp->enterConference(conf.get());
#endif
#ifdef ENABLE_PLUGIN
    clearCallAVStreams();
#endif
}

void
SIPCall::exitConference()
{
#ifdef ENABLE_VIDEO
    auto const& videoRtp = getVideoRtp();
    if (videoRtp)
        videoRtp->exitConference();
#endif
#ifdef ENABLE_PLUGIN
    createCallAVStreams();
#endif
}

std::shared_ptr<AudioRtpSession>
SIPCall::getAudioRtp() const
{
    // For the moment, we support only one audio stream.

    for (auto const& stream : rtpStreams_) {
        auto rtp = stream.rtpSession_;
        if (rtp->getMediaType() == MediaType::MEDIA_AUDIO) {
            return std::dynamic_pointer_cast<AudioRtpSession>(rtp);
        }
    }

    return nullptr;
}

#ifdef ENABLE_VIDEO
std::shared_ptr<video::VideoRtpSession>
SIPCall::getVideoRtp() const
{
    for (auto const& stream : rtpStreams_) {
        auto rtp = stream.rtpSession_;
        if (rtp->getMediaType() == MediaType::MEDIA_VIDEO) {
            return std::dynamic_pointer_cast<video::VideoRtpSession>(rtp);
        }
    }
    return nullptr;
}
#endif

void
SIPCall::monitor() const
{
    if (isSubcall())
        return;
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return;
    }
    JAMI_DBG("- Call %s with %s:", getCallId().c_str(), getPeerNumber().c_str());
    // TODO move in getCallDuration
    auto duration = duration_start_ == time_point::min()
                        ? 0
                        : std::chrono::duration_cast<std::chrono::milliseconds>(clock::now()
                                                                                - duration_start_)
                              .count();
    JAMI_DBG("\t- Duration: %lu", duration);
    for (auto& mediaAttr : getMediaAttributeList())
        JAMI_DBG("\t- Media: %s", mediaAttr.toString(true).c_str());
#ifdef ENABLE_VIDEO
    if (auto codec = getVideoCodec())
        JAMI_DBG("\t- Video codec: %s", codec->systemCodecInfo.name.c_str());
#endif
    auto media_tr = getIceMediaTransport();
    if (media_tr) {
        JAMI_DBG("\t- Medias: %s", media_tr->link().c_str());
    }
}

bool
SIPCall::toggleRecording()
{
    pendingRecord_ = true;
    if (not readyToRecord_)
        return true;

    // add streams to recorder before starting the record
    if (not Call::isRecording()) {
        updateRecState(true);
        auto account = getSIPAccount();
        if (!account) {
            JAMI_ERR("No account detected");
            return false;
        }
        auto title = fmt::format("Conversation at %TIMESTAMP between {} and {}",
                                 account->getUserUri(),
                                 peerUri_);
        recorder_->setMetadata(title, ""); // use default description
        auto const& audioRtp = getAudioRtp();
        if (audioRtp)
            audioRtp->initRecorder(recorder_);
#ifdef ENABLE_VIDEO
        if (not isAudioOnly_) {
            auto const& videoRtp = getVideoRtp();
            if (videoRtp)
                videoRtp->initRecorder(recorder_);
        }
#endif
    } else {
        updateRecState(false);
        deinitRecorder();
    }
    pendingRecord_ = false;
    return Call::toggleRecording();
}

void
SIPCall::deinitRecorder()
{
    if (Call::isRecording()) {
        auto const& audioRtp = getAudioRtp();
        if (audioRtp)
            audioRtp->deinitRecorder(recorder_);
#ifdef ENABLE_VIDEO
        if (not isAudioOnly_) {
            auto const& videoRtp = getVideoRtp();
            if (videoRtp)
                videoRtp->deinitRecorder(recorder_);
        }
#endif
    }
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
SIPCall::InvSessionDeleter::operator()(pjsip_inv_session* inv) const noexcept
{
    // prevent this from getting accessed in callbacks
    // JAMI_WARN: this is not thread-safe!
    if (!inv)
        return;
    inv->mod_data[Manager::instance().sipVoIPLink().getModId()] = nullptr;
    // NOTE: the counter is incremented by sipvoiplink (transaction_request_cb)
    pjsip_inv_dec_ref(inv);
}

bool
SIPCall::initIceMediaTransport(bool master,
                               std::optional<IceTransportOptions> options,
                               unsigned channel_num)
{
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return false;
    }
    JAMI_DBG("[call:%s] create media ICE transport", getCallId().c_str());
    auto iceOptions = options == std::nullopt ? acc->getIceOptions() : *options;

    auto optOnInitDone = std::move(iceOptions.onInitDone);
    auto optOnNegoDone = std::move(iceOptions.onNegoDone);
    iceOptions.onInitDone = [w = weak(), cb = std::move(optOnInitDone)](bool ok) {
        runOnMainThread([w = std::move(w), cb = std::move(cb), ok] {
            auto call = w.lock();
            if (cb)
                cb(ok);
            if (!ok or !call or !call->waitForIceInit_.exchange(false))
                return;

            std::lock_guard<std::recursive_mutex> lk {call->callMutex_};
            auto rem_ice_attrs = call->sdp_->getIceAttributes();
            // Init done but no remote_ice_attributes, the ice->start will be triggered later
            if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty())
                return;
            call->startIceMedia();
        });
    };
    iceOptions.onNegoDone = [w = weak(), cb = std::move(optOnNegoDone)](bool ok) {
        runOnMainThread([w = std::move(w), cb = std::move(cb), ok] {
            if (cb)
                cb(ok);
            if (auto call = w.lock()) {
                // The ICE is related to subcalls, but medias are handled by parent call
                std::lock_guard<std::recursive_mutex> lk {call->callMutex_};
                call = call->isSubcall() ? std::dynamic_pointer_cast<SIPCall>(call->parent_) : call;
                if (!ok) {
                    JAMI_ERR("[call:%s] Media ICE negotiation failed", call->getCallId().c_str());
                    call->onFailure(EIO);
                    return;
                }
                call->onIceNegoSucceed();
            }
        });
    };

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    auto transport = iceTransportFactory.createUTransport(getCallId().c_str(),
                                                          channel_num,
                                                          master,
                                                          iceOptions);
    std::lock_guard<std::mutex> lk(transportMtx_);
    // Destroy old ice on a separate io pool
    if (tmpMediaTransport_)
        dht::ThreadPool::io().run([ice = std::make_shared<decltype(tmpMediaTransport_)>(
                                       std::move(tmpMediaTransport_))] {});
    tmpMediaTransport_ = std::move(transport);
    if (tmpMediaTransport_) {
        JAMI_DBG("[call:%s] Successfully created media ICE transport", getCallId().c_str());
    } else {
        JAMI_ERR("[call:%s] Failed to create media ICE transport", getCallId().c_str());
    }
    return static_cast<bool>(tmpMediaTransport_);
}

void
SIPCall::merge(Call& call)
{
    JAMI_DBG("[sipcall:%s] merge subcall %s", getCallId().c_str(), call.getCallId().c_str());

    // This static cast is safe as this method is private and overload Call::merge
    auto& subcall = static_cast<SIPCall&>(call);

    std::lock(callMutex_, subcall.callMutex_);
    std::lock_guard<std::recursive_mutex> lk1 {callMutex_, std::adopt_lock};
    std::lock_guard<std::recursive_mutex> lk2 {subcall.callMutex_, std::adopt_lock};
    inviteSession_ = std::move(subcall.inviteSession_);
    inviteSession_->mod_data[Manager::instance().sipVoIPLink().getModId()] = this;
    setTransport(std::move(subcall.transport_));
    sdp_ = std::move(subcall.sdp_);
    peerHolding_ = subcall.peerHolding_;
    upnp_ = std::move(subcall.upnp_);
    std::copy_n(subcall.contactBuffer_, PJSIP_MAX_URL_SIZE, contactBuffer_);
    pj_strcpy(&contactHeader_, &subcall.contactHeader_);
    localAudioPort_ = subcall.localAudioPort_;
    localVideoPort_ = subcall.localVideoPort_;
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        mediaTransport_ = std::move(subcall.mediaTransport_);
        tmpMediaTransport_ = std::move(subcall.tmpMediaTransport_);
    }

    Call::merge(subcall);
    startIceMedia();
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
        // (same question in startIceMedia)
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

void
SIPCall::rtpSetupSuccess(MediaType type, bool isRemote)
{
    std::lock_guard<std::mutex> lk {setupSuccessMutex_};
    if (type == MEDIA_AUDIO) {
        if (isRemote)
            mediaReady_.at("a:remote") = true;
        else
            mediaReady_.at("a:local") = true;
    } else {
        if (isRemote)
            mediaReady_.at("v:remote") = true;
        else
            mediaReady_.at("v:local") = true;
    }

    if (mediaReady_.at("a:local") and mediaReady_.at("a:remote") and mediaReady_.at("v:remote")) {
        if (Manager::instance().videoPreferences.getRecordPreview() or mediaReady_.at("v:local"))
            readyToRecord_ = true;
    }

    if (pendingRecord_ && readyToRecord_)
        toggleRecording();
}

void
SIPCall::peerRecording(bool state)
{
    const std::string& id = getConfId().empty() ? getCallId() : getConfId();
    if (state) {
        JAMI_WARN("Peer is recording");
        emitSignal<DRing::CallSignal::RemoteRecordingChanged>(id, getPeerNumber(), true);
    } else {
        JAMI_WARN("Peer stopped recording");
        emitSignal<DRing::CallSignal::RemoteRecordingChanged>(id, getPeerNumber(), false);
    }
    peerRecording_ = state;
}

void
SIPCall::peerMuted(bool muted)
{
    if (muted) {
        JAMI_WARN("Peer muted");
    } else {
        JAMI_WARN("Peer un-muted");
    }
    peerMuted_ = muted;
    if (auto conf = Manager::instance().getConferenceFromID(getConfId())) {
        conf->updateMuted();
    }
}

void
SIPCall::resetMediaReady()
{
    for (auto& m : mediaReady_)
        m.second = false;
}

} // namespace jami
