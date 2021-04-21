﻿/*
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

// TODO_MC. Fixme.
#define LOCK_MEDIA() std::lock_guard<std::recursive_mutex> lock(callMutex_);
#define LOCK_CALL()  std::lock_guard<std::recursive_mutex> lock(callMutex_);

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
    execQueue_ = getSipVoIPLink().getExecQueue();
    assert(execQueue_->initialized());

    // Init on the SIP thread.
    runOnExecQueue([this, acc = account] {
        initCall(acc, {});
        JAMI_DBG("[call:%s] Created a new SIP call", getCallId().c_str());
    });
}

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::vector<MediaAttribute>& mediaAttrList)
    : Call(account, callId, type)
    , sdp_(new Sdp(callId))
{
    execQueue_ = getSipVoIPLink().getExecQueue();
    assert(execQueue_->initialized());

    // Init on the SIP thread.
    runOnExecQueue([this, acc = account, mediaList = mediaAttrList] {
        initCall(acc, mediaList);
        JAMI_DBG("[call:%s] Created a new SIP call with %lu medias",
                 getCallId().c_str(),
                 mediaList.size());
    });
}

SIPCall::~SIPCall()
{
    LOCK_CALL();
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        if (tmpMediaTransport_)
            dht::ThreadPool::io().run([ice = std::make_shared<decltype(tmpMediaTransport_)>(
                                           std::move(tmpMediaTransport_))] {});
    }
    setTransport({});
    // Prevents callback usage
    if (inviteSession_ != nullptr) {
        JAMI_ERR("[call:%s] SIP invite session [%p] was not properly terminated",
                 getCallId().c_str(),
                 inviteSession_);
        setInviteSession();
    }
}

void
SIPCall::initCall(const std::shared_ptr<SIPAccountBase>& account,
                  const std::vector<MediaAttribute>& mediaAttrList)
{
    CHECK_VALID_EXEC_THREAD()

    JAMI_DBG("[call:%s] Init SIP call", getCallId().c_str());

    sip_utils::register_thread();

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
    if (not mediaAttrList.empty()) {
        initMediaStreams(mediaAttrList);
    } else {
        // Create a media list from local config if not provided. Needed for
        // backward compatiblity.
        auto mediaList = getSIPAccount()->createDefaultMediaList(getSIPAccount()->isVideoEnabled()
                                                                     and not isAudioOnly(),
                                                                 getState() == CallState::HOLD);
        initMediaStreams(mediaList);
    }
}

size_t
SIPCall::findRtpStreamIndex(const std::string& label) const
{
    LOCK_MEDIA();

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

unsigned
SIPCall::getStreamCount() const
{
    LOCK_MEDIA();
    return static_cast<unsigned>(rtpStreams_.size());
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
            if (auto thisPtr = w.lock())
                thisPtr->requestKeyframe();
        });
        videoRtp->setChangeOrientationCallback([w = weak()](int angle) {
            if (auto thisPtr = w.lock())
                thisPtr->setVideoOrientation(angle);
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
                        this_->callFailed(ECONNRESET);
                    }
                }
            });
    }
}

void
SIPCall::requestReinvite()
{
    JAMI_DBG("[call:%s] Sending a SIP re-invite to request media change", getCallId().c_str());

    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::SwitchInput;
    } else {
        auto mediaList = getMediaAttributeList();
        assert(not mediaList.empty());

        // TODO. We should erase existing streams only after the new
        // ones were successfully negotiated, and make a live switch. But
        // for now, we reset all streams before creating new ones.
        initMediaStreams(mediaList);

        SIPSessionReinvite(mediaList);
    }
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 */
int
SIPCall::SIPSessionReinvite(const std::vector<MediaAttribute>& mediaAttrList)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(),
                        [this, mediaList = mediaAttrList] { SIPSessionReinvite(mediaList); });
        return PJ_SUCCESS;
    }

    assert(not mediaAttrList.empty());

    CHECK_VALID_EXEC_THREAD();

    // Do nothing if no invitation processed yet
    if (not getInviteSession() or getInviteSession()->invite_tsx)
        return PJ_SUCCESS;

    JAMI_DBG("[call:%s] Preparing and sending a re-invite (state=%s)",
             getCallId().c_str(),
             pjsip_inv_state_name(getInviteSession()->state));

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

    if (initIceMediaTransport(true)) {
        addLocalIceAttributes();
        isWaitingForIceAndMedia_ = true;
    }

    pjsip_tx_data* tdata;
    auto local_sdp = sdp_->getLocalSdpSession();
    auto result = pjsip_inv_reinvite(getInviteSession(), nullptr, local_sdp, &tdata);
    if (result == PJ_SUCCESS) {
        if (!tdata)
            return PJ_SUCCESS;

        // Add user-agent header
        sip_utils::addUserAgentHeader(acc->getUserAgentName(), tdata);

        result = pjsip_inv_send_msg(getInviteSession(), tdata);
        if (result == PJ_SUCCESS)
            return PJ_SUCCESS;
        JAMI_ERR("[call:%s] Failed to send REINVITE msg (pjsip: %s)",
                 getCallId().c_str(),
                 sip_utils::sip_strerror(result).c_str());
        // Canceling internals without sending (anyways the send has just failed!)
        pjsip_inv_cancel_reinvite(getInviteSession(), &tdata);
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
SIPCall::sendSIPInfo(std::string body, std::string subtype)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, bd = std::move(body), stype = std::move(subtype)] {
            sendSIPInfo(bd, stype);
        });
        return;
    }

    if (not getInviteSession()) {
        JAMI_ERR("[call:%s] No invite session", getCallId().c_str());
        return;
    }

    if (not getInviteSession()->dlg) {
        JAMI_ERR("[call:%s] Could not get from invite dialog", getCallId().c_str());
        return;
    }

    constexpr pj_str_t methodName = CONST_PJ_STR("INFO");
    constexpr pj_str_t type = CONST_PJ_STR("application");

    pjsip_method method;
    pjsip_method_init_np(&method, (pj_str_t*) &methodName);

    /* Create request message. */
    pjsip_tx_data* tdata;
    if (pjsip_dlg_create_request(getInviteSession()->dlg, &method, -1, &tdata) != PJ_SUCCESS) {
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
        pjsip_dlg_send_request(getInviteSession()->dlg, tdata, getSipVoIPLink().getModId(), NULL);
}

void
SIPCall::updateRecState(bool state)
{
    std::string body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                       "<media_control><vc_primitive><to_encoder>"
                       "<recording_state="
                       + std::to_string(state)
                       + "/>"
                         "</to_encoder></vc_primitive></media_control>";
    // see https://tools.ietf.org/html/rfc5168 for XML Schema for Media Control details

    JAMI_DBG("Sending recording state via SIP INFO");

    try {
        sendSIPInfo(body, "media_control+xml");
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

    std::string body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                       "<media_control><vc_primitive><to_encoder>"
                       "<picture_fast_update/>"
                       "</to_encoder></vc_primitive></media_control>";
    JAMI_DBG("Sending video keyframe request via SIP INFO");
    try {
        sendSIPInfo(body, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending video keyframe request: %s", e.what());
    }
    lastKeyFrameReq_ = now;
}

void
SIPCall::setMute(bool state)
{
    std::string body = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                       "<media_control><vc_primitive><to_encoder>"
                       "<mute_state="
                       + std::to_string(state)
                       + "/>"
                         "</to_encoder></vc_primitive></media_control>";
    // see https://tools.ietf.org/html/rfc5168 for XML Schema for Media Control details

    JAMI_DBG("Sending mute state via SIP INFO");

    try {
        sendSIPInfo(body, "media_control+xml");
    } catch (const std::exception& e) {
        JAMI_ERR("Error sending mute state: %s", e.what());
    }
}

pjsip_inv_session*
SIPCall::getInviteSession()
{
    LOCK_CALL();

    return inviteSession_;
}

void
SIPCall::setInviteSession(pjsip_inv_session* invite)
{
    CHECK_VALID_EXEC_THREAD();

    // Ignore if the same.
    if (invite == inviteSession_)
        return;

    if (invite == nullptr and inviteSession_ != nullptr) {
        JAMI_DBG("[call:%s] Release current invite session [%p]",
                 getCallId().c_str(),
                 inviteSession_);
        getSipVoIPLink().unregisterCall(this, inviteSession_);
        inviteSession_ = nullptr;
    } else if (invite != nullptr) {
        if (inviteSession_ != nullptr) {
            // Release the current instance.
            getSipVoIPLink().unregisterCall(this, inviteSession_);
        }

        JAMI_DBG("[call:%s] Set new invite session [%p]", getCallId().c_str(), invite);
        inviteSession_ = invite;
        getSipVoIPLink().registerCall(this, inviteSession_);
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
    CHECK_VALID_EXEC_THREAD();
    if (getInviteSession() and getInviteSession()->state != PJSIP_INV_STATE_DISCONNECTED) {
        pjsip_tx_data* tdata = nullptr;
        auto ret = pjsip_inv_end_session(getInviteSession(), status, nullptr, &tdata);
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

                ret = pjsip_inv_send_msg(getInviteSession(), tdata);
                if (ret != PJ_SUCCESS)
                    JAMI_ERR("[call:%s] failed to send terminate msg, SIP error (%s)",
                             getCallId().c_str(),
                             sip_utils::sip_strerror(ret).c_str());
            }
        } else
            JAMI_ERR("[call:%s] failed to terminate INVITE@%p, SIP error (%s)",
                     getCallId().c_str(),
                     getInviteSession(),
                     sip_utils::sip_strerror(ret).c_str());
    }

    setInviteSession();
}

void
SIPCall::answer()
{
    CHECK_VALID_EXEC_THREAD();
    auto account = getSIPAccount();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }

    if (not getInviteSession())
        throw VoipLinkException("[call:" + getCallId()
                                + "] answer: no invite session for this call");

    if (!getInviteSession()->neg) {
        JAMI_WARN("[call:%s] Negotiator is NULL, we've received an INVITE without an SDP",
                  getCallId().c_str());
        pjmedia_sdp_session* dummy = 0;
        getSipVoIPLink().createSDPOffer(getInviteSession(), &dummy);

        if (account->isStunEnabled())
            updateSDPFromSTUN();
    }

    pj_str_t contact(account->getContactHeader(transport_ ? transport_->get() : nullptr));
    setContactHeader(&contact);

    pjsip_tx_data* tdata;
    if (!getInviteSession()->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(getInviteSession(),
                         PJSIP_SC_OK,
                         NULL,
                         !getInviteSession()->neg ? sdp_->getLocalSdpSession() : NULL,
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

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::answer(const std::vector<MediaAttribute>& mediaAttrList)
{
    // TODO. Refactor/reuse code shared with answer(void) version.

    CHECK_VALID_EXEC_THREAD();
    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("No account detected");
        return;
    }

    if (mediaAttrList.size() != getStreamCount()) {
        JAMI_ERR("Media list size %lu in answer does not match. Expected %u",
                 mediaAttrList.size(),
                 getStreamCount());
        return;
    }

    {
        LOCK_MEDIA();
        // Apply the media attributes provided by the user.
        for (size_t idx = 0; idx < mediaAttrList.size(); idx++) {
            rtpStreams_[idx].mediaAttribute_ = std::make_shared<MediaAttribute>(mediaAttrList[idx]);
        }
    }

    if (not getInviteSession())
        throw VoipLinkException("[call:" + getCallId()
                                + "] answer: no invite session for this call");

    if (not getInviteSession()->neg) {
        JAMI_WARN("[call:%s] Negotiator is NULL, we've received an INVITE without an SDP",
                  getCallId().c_str());
        pjmedia_sdp_session* dummy = 0;
        getSipVoIPLink().createSDPOffer(getInviteSession(), &dummy);

        if (account->isStunEnabled())
            updateSDPFromSTUN();
    }

    pj_str_t contact(account->getContactHeader(transport_ ? transport_->get() : nullptr));
    setContactHeader(&contact);

    if (!getInviteSession()->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // TODO. We need a test for this scenario.
    // How to Check if this use-case is not broken by the changes.

    // Answer with an SDP offer if the initial invite was empty.
    // SIP protocol allows a UA to send a call invite without SDP.
    // In this case, if the callee wants to accept the call, it must
    // provide an SDP offer in the answer. The caller will then send
    // its SDP answer in the SIP OK (200) message.
    pjsip_tx_data* tdata;
    if (pjsip_inv_answer(getInviteSession(),
                         PJSIP_SC_OK,
                         NULL,
                         not getInviteSession()->neg ? sdp_->getLocalSdpSession() : NULL,
                         &tdata)
        != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    if (contactHeader_.slen) {
        JAMI_DBG("[call:%s] Answering with contact header: %.*s",
                 getCallId().c_str(),
                 (int) contactHeader_.slen,
                 contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::answerMediaChangeRequest(const std::vector<MediaAttribute>& mediaAttrList)
{
    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("[call:%s] No account detected", getCallId().c_str());
        return;
    }

    if (mediaAttrList.empty()) {
        JAMI_DBG("[call:%s] Media list size is empty. Ignoring the media change request",
                 getCallId().c_str());
        return;
    }

    if (not sdp_) {
        JAMI_ERR("[call:%s] No valid SDP session", getCallId().c_str());
        return;
    }

    JAMI_DBG("[call:%s] Current media", getCallId().c_str());
    unsigned idx = 0;
    for (auto const& rtp : rtpStreams_) {
        JAMI_DBG("[call:%s] Media @%u: %s",
                 getCallId().c_str(),
                 idx++,
                 rtp.mediaAttribute_->toString(true).c_str());
    }

    JAMI_DBG("[call:%s] Answering to media change request with new media", getCallId().c_str());
    idx = 0;
    for (auto const& newMediaAttr : mediaAttrList) {
        JAMI_DBG("[call:%s] Media @%u: %s",
                 getCallId().c_str(),
                 idx++,
                 newMediaAttr.toString(true).c_str());
    }

    updateAllMediaStreams(mediaAttrList);

    if (not sdp_->processIncomingOffer(mediaAttrList)) {
        JAMI_WARN("[call:%s] Could not process the new offer, ignoring", getCallId().c_str());
        return;
    }

    if (not sdp_->getRemoteSdpSession()) {
        JAMI_ERR("[call:%s] No valid remote SDP session", getCallId().c_str());
        return;
    }

    setupLocalIce();

    if (sdp_->startNegotiation()) {
        JAMI_ERR("[call:%s] Could not start media negotiation for a re-invite request",
                 getCallId().c_str());
        return;
    }

    if (pjsip_inv_set_sdp_answer(getInviteSession(), sdp_->getLocalSdpSession()) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not start media negotiation for a re-invite request",
                 getCallId().c_str());
        return;
    }

    pjsip_tx_data* tdata;
    if (pjsip_inv_answer(getInviteSession(), PJSIP_SC_OK, NULL, NULL, &tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not init answer to a re-invite request", getCallId().c_str());
        return;
    }

    if (contactHeader_.slen) {
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not send answer to a re-invite request", getCallId().c_str());
        return;
    }

    JAMI_DBG("[call:%s] Successfully answered the media change request", getCallId().c_str());
}

void
SIPCall::hangup(int reason)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, reason] { hangup(reason); });
        return;
    }

    JAMI_DBG("[call:%s] Hanging-up the call", getCallId().c_str());

    if (getInviteSession() and getInviteSession()->dlg) {
        pjsip_route_hdr* route = getInviteSession()->dlg->route_set.next;
        while (route and route != &getInviteSession()->dlg->route_set) {
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
        else if (getInviteSession()->state <= PJSIP_INV_STATE_EARLY
                 and getInviteSession()->role != PJSIP_ROLE_UAC)
            status = PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
        else if (getInviteSession()->state >= PJSIP_INV_STATE_DISCONNECTED)
            status = PJSIP_SC_DECLINE;

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
    if (!isIncoming() or getConnectionState() == ConnectionState::CONNECTED or !getInviteSession())
        return;

    stopAllMedia();

    // Notify the peer
    terminateSipSession(PJSIP_SC_BUSY_HERE);

    setState(Call::ConnectionState::DISCONNECTED, ECONNABORTED);
    removeCall();
}

void
SIPCall::transfer_client_cb(pjsip_evsub* sub, pjsip_event* event)
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
            if (call->getInviteSession())
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
    if (not getInviteSession() or not getInviteSession()->dlg)
        return false;

    pjsip_evsub_user xfer_cb;
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    pjsip_evsub* sub;

    if (pjsip_xfer_create_uac(getInviteSession()->dlg, &xfer_cb, &sub) != PJ_SUCCESS)
        return false;

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data(sub, getSipVoIPLink().getModId(), this);

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

    if (not toCall->getInviteSession() or not toCall->getInviteSession()->dlg)
        return false;

    pjsip_dialog* target_dlg = toCall->getInviteSession()->dlg;
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
        }
    }

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

    {
        LOCK_MEDIA();
        for (auto const& stream : rtpStreams_) {
            auto mediaAttr = stream.mediaAttribute_;
            mediaAttr->sourceUri_ = source;
        }
    }

    // Check if the call is being recorded in order to continue
    // ... the recording after the switch
    bool isRec = Call::isRecording();

    if (isWaitingForIceAndMedia_) {
        remainingRequest_ = Request::SwitchInput;
    } else {
        SIPSessionReinvite();
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

    if (getInviteSession())
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
    CHECK_VALID_EXEC_THREAD();
    // TODO: for now we ignore the "from" (the previous implementation for sending this info was
    //      buggy and verbose), another way to send the original message sender will be implemented
    //      in the future
    if (not subcalls_.empty()) {
        pendingOutMessages_.emplace_back(messages, from);
        for (auto& c : subcalls_)
            c->sendTextMessage(messages, from);
    } else {
        if (getInviteSession()) {
            try {
                im::sendSipMessage(getInviteSession(), messages);
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
    CHECK_VALID_EXEC_THREAD();

#ifdef ENABLE_PLUGIN
    jami::Manager::instance().getJamiPluginManager().getCallServicesManager().clearCallHandlerMaps(
        getCallId());
#endif
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
SIPCall::callFailed(signed cause)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, cause] { callFailed(cause); });
        return;
    }

    if (not setState(CallState::MERROR, ConnectionState::DISCONNECTED, cause))
        return;

    Manager::instance().callFailure(*this);
    removeCall();
}

void
SIPCall::peerBusy()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { peerBusy(); });
        return;
    }

    if (getCallType() == CallType::OUTGOING)
        setState(CallState::PEER_BUSY, ConnectionState::DISCONNECTED);
    else
        setState(CallState::BUSY, ConnectionState::DISCONNECTED);

    Manager::instance().callBusy(*this);
    removeCall();
}

void
SIPCall::closeCall()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { closeCall(); });
        return;
    }

    Manager::instance().peerHungupCall(*this);
    removeCall();
}

void
SIPCall::peerAnswered()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { peerAnswered(); });
        return;
    }

    JAMI_WARN("[call:%s] peerAnswered()", getCallId().c_str());
    if (getConnectionState() != ConnectionState::CONNECTED) {
        setState(CallState::ACTIVE, ConnectionState::CONNECTED);
        if (not isSubcall()) {
            Manager::instance().peerAnsweredCall(*this);
        }
    }
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
SIPCall::peerRinging()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { peerRinging(); });
        return;
    }

    setState(ConnectionState::RINGING);
}

void
SIPCall::addLocalIceAttributes()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { addLocalIceAttributes(); });
        return;
    }

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

    auto streamCount = getStreamCount();
    for (unsigned idx = 0, compId = 1; idx < streamCount; idx++, compId += 2) {
        JAMI_DBG("[call:%s] add ICE local candidates for media @ index %u",
                 getCallId().c_str(),
                 idx);
        // RTP
        sdp_->addIceCandidates(idx, media_tr->getLocalCandidates(compId));
        // RTCP
        sdp_->addIceCandidates(idx, media_tr->getLocalCandidates(compId + 1));
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
    auto streamCount = getStreamCount();
    for (unsigned mediaIdx = 0; mediaIdx < streamCount; mediaIdx++) {
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
    LOCK_MEDIA();
    rtpStreams_.emplace_back(std::move(stream));
}

void
SIPCall::initMediaStreams(const std::vector<MediaAttribute>& mediaAttrList)
{
    LOCK_MEDIA();

    rtpStreams_.clear();

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

    assert(getStreamCount() == mediaAttrList.size());

    JAMI_DBG("[call:%s] Created %u Media streams", getCallId().c_str(), getStreamCount());
}

bool
SIPCall::isAudioMuted() const
{
    std::function<bool(const RtpStream& stream)> mutedCheck = [](const RtpStream& stream) {
        return (stream.mediaAttribute_->type_ == MediaType::MEDIA_AUDIO
                and not stream.mediaAttribute_->muted_);
    };

    const auto iter = std::find_if(rtpStreams_.begin(), rtpStreams_.end(), mutedCheck);

    return iter != rtpStreams_.end();
}

bool
SIPCall::hasVideo() const
{
#ifdef ENABLE_VIDEO
    std::function<bool(const RtpStream& stream)> videoCheck = [](const RtpStream& stream) {
        return stream.mediaAttribute_->type_ == MediaType::MEDIA_VIDEO;
    };

    const auto iter = std::find_if(rtpStreams_.begin(), rtpStreams_.end(), videoCheck);

    return iter != rtpStreams_.end();
#else
    return false;
#endif
}

bool
SIPCall::isVideoMuted() const
{
#ifdef ENABLE_VIDEO
    std::function<bool(const RtpStream& stream)> mutedCheck = [](const RtpStream& stream) {
        return (stream.mediaAttribute_->type_ == MediaType::MEDIA_VIDEO
                and not stream.mediaAttribute_->muted_);
    };
    const auto iter = std::find_if(rtpStreams_.begin(), rtpStreams_.end(), mutedCheck);
    return iter != rtpStreams_.end();
#else
    return true;
#endif
}

video::VideoGenerator*
SIPCall::getVideoReceiver()
{
    if (auto const& videoRtp = getVideoRtp())
        return videoRtp->getVideoReceive().get();

    return nullptr;
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
        callFailed(EPROTONOSUPPORT);
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

        std::shared_ptr<RtpSession> rtpSession;
        std::shared_ptr<MediaAttribute> mediaAttr;
        {
            LOCK_MEDIA();

            rtpSession = rtpStreams_[slotN].rtpSession_;
            mediaAttr = rtpStreams_[slotN].mediaAttribute_;
            if (not rtpSession or not mediaAttr) {
                throw std::runtime_error("Invalid media stream");
            }
        }

        // Configure the media.
        configureRtpSession(rtpSession, mediaAttr, local, remote);

        // Not restarting media loop on hold as it's a huge waste of CPU ressources
        // because of the audio loop
        if (getState() != CallState::HOLD) {
            if (isIceRunning()) {
                rtpSession->start(newIceSocket(ice_comp_id), newIceSocket(ice_comp_id + 1));
                ice_comp_id += 2;
            } else
                rtpSession->start(nullptr, nullptr);
        }

        // Aggregate holding info over all remote streams
        peer_holding &= remote.onHold;
    }

    if (not isSubcall() and peerHolding_ != peer_holding) {
        peerHolding_ = peer_holding;
        emitSignal<DRing::CallSignal::PeerHold>(getCallId(), peerHolding_);
    }

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

void
SIPCall::updateMediaStream(const MediaAttribute& newMediaAttr, size_t streamIdx)
{
    LOCK_MEDIA();

    assert(streamIdx < getStreamCount());

    auto const& rtpStream = rtpStreams_[streamIdx];
    assert(rtpStream.rtpSession_);

    auto const& mediaAttr = rtpStream.mediaAttribute_;
    assert(mediaAttr);

    if (newMediaAttr.muted_ == mediaAttr->muted_) {
        // Nothing to do. Already in the desired state.
        JAMI_DBG("[call:%s] [%s] already %s",
                 getCallId().c_str(),
                 mediaAttr->label_.c_str(),
                 mediaAttr->muted_ ? "muted " : "un-muted ");

        return;
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
        return;
    }

#ifdef ENABLE_VIDEO
    if (mediaAttr->type_ == MediaType::MEDIA_VIDEO) {
        if (not isSubcall())
            emitSignal<DRing::CallSignal::VideoMuted>(getCallId(), mediaAttr->muted_);
    }
#endif
}

void
SIPCall::updateAllMediaStreams(const std::vector<MediaAttribute>& mediaAttrList)
{
    LOCK_MEDIA();

    JAMI_DBG("[call:%s] New local medias", getCallId().c_str());

    unsigned idx = 0;
    for (auto const& newMediaAttr : mediaAttrList) {
        JAMI_DBG("[call:%s] Media @%u: %s",
                 getCallId().c_str(),
                 idx++,
                 newMediaAttr.toString(true).c_str());
    }

    JAMI_DBG("[call:%s] Updating local media streams", getCallId().c_str());

    for (auto const& newAttr : mediaAttrList) {
        auto streamIdx = findRtpStreamIndex(newAttr.label_);

        if (streamIdx == getStreamCount()) {
            // Media does not exist, add a new one.
            addMediaStream(newAttr);
            auto& stream = rtpStreams_.back();
            createRtpSession(stream);
            JAMI_DBG("[call:%s] Added a new media stream [%s] @ index %lu",
                     getCallId().c_str(),
                     stream.mediaAttribute_->label_.c_str(),
                     streamIdx);
        } else {
            updateMediaStream(newAttr, streamIdx);
        }
    }
}

bool
SIPCall::isReinviteRequired(const std::vector<MediaAttribute>& mediaAttrList)
{
    if (mediaAttrList.size() != rtpStreams_.size())
        return true;

    for (auto const& newAttr : mediaAttrList) {
        auto streamIdx = findRtpStreamIndex(newAttr.label_);

        if (streamIdx == getStreamCount()) {
            // Always needs a reinvite when a new media is added.
            return true;
        }

#ifdef ENABLE_VIDEO
        LOCK_MEDIA();
        if (newAttr.type_ == MediaType::MEDIA_VIDEO) {
            assert(rtpStreams_[streamIdx].mediaAttribute_);
            // Changes in video attributes always trigger a re-invite.
            return newAttr.muted_ != rtpStreams_[streamIdx].mediaAttribute_->muted_;
        }
#endif
    }

    return false;
}

bool
SIPCall::requestMediaChange(const std::vector<MediaAttribute>& mediaAttrList)
{
    JAMI_DBG("[call:%s] Requesting media change. List of new media:", getCallId().c_str());

    unsigned idx = 0;
    for (auto const& newMediaAttr : mediaAttrList) {
        JAMI_DBG("[call:%s] Media @%u: %s",
                 getCallId().c_str(),
                 idx++,
                 newMediaAttr.toString(true).c_str());
    }

    auto needReinvite = isReinviteRequired(mediaAttrList);

    updateAllMediaStreams(mediaAttrList);

    if (needReinvite) {
        JAMI_DBG("[call:%s] Media change requires a new negotiation (re-invite)",
                 getCallId().c_str());
        requestReinvite();
    }

    return true;
}

std::vector<MediaAttribute>
SIPCall::getMediaAttributeList() const
{
    LOCK_MEDIA();
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
SIPCall::mediaNegotiationComplete()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { mediaNegotiationComplete(); });
        return;
    }

    JAMI_WARN("[call:%s] Media negotiation complete", getCallId().c_str());

    // Main call (no subcalls) must wait for ICE now, the rest of code needs to access
    // to a negotiated transport.
    emitSignal<DRing::CallSignal::MediaNegotiationStatus>(
        getCallId(), MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS);

    JAMI_WARN("[call:%s] media changed", getCallId().c_str());
    // The call is already ended, so we don't need to restart medias
    if (not getInviteSession() or getInviteSession()->state == PJSIP_INV_STATE_DISCONNECTED
        or not sdp_)
        return;
    // If ICE is not used, start medias now
    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty() or rem_ice_attrs.pwd.empty()) {
        JAMI_WARN("[call:%s] no remote ICE for media", getCallId().c_str());
        stopAllMedia();
        startAllMedia();
        return;
    }
    if (not isSubcall())
        startIceMedia();
}

std::weak_ptr<SipEventsHandler>
SIPCall::weakPtr()
{
    return std::dynamic_pointer_cast<SipEventsHandler>(weak().lock());
}

const std::string&
SIPCall::getSipCallId()
{
    return getCallId();
}

void
SIPCall::onReceivedTextMessage(std::map<std::string, std::string> messages)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, messages] { onReceivedTextMessage(messages); });
        return;
    }

    onTextMessage(std::move(messages));
}

void
SIPCall::onRequestInfo(pjsip_inv_session* inv, pjsip_rx_data* rdata, pjsip_msg* msg)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, inv, rdata, msg] { onRequestInfo(inv, rdata, msg); });
        return;
    }

    if (!msg->body or handleMediaControl(msg->body))
        replyToRequest(getInviteSession(), rdata, PJSIP_SC_OK);
}

void
SIPCall::onRequestRefer(pjsip_inv_session* inv, pjsip_rx_data* rdata, pjsip_msg* msg)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, inv, rdata, msg] { onRequestRefer(inv, rdata, msg); });
        return;
    }

    static constexpr pj_str_t str_refer_to = CONST_PJ_STR("Refer-To");

    if (auto refer_to = static_cast<pjsip_generic_string_hdr*>(
            pjsip_msg_find_hdr_by_name(msg, &str_refer_to, nullptr))) {
        // RFC 3515, 2.4.2: reply bad request if no or too many refer-to header.
        if (static_cast<void*>(refer_to->next) == static_cast<void*>(&msg->hdr)
            or !pjsip_msg_find_hdr_by_name(msg, &str_refer_to, refer_to->next)) {
            replyToRequest(inv, rdata, PJSIP_SC_ACCEPTED);
            transferCall(std::string(refer_to->hvalue.ptr, refer_to->hvalue.slen));

            // RFC 3515, 2.4.4: we MUST handle the processing using NOTIFY msgs
            // But your current design doesn't permit that
            return;
        } else
            JAMI_ERR("[call:%s] REFER: too many Refer-To headers", getCallId().c_str());
    } else
        JAMI_ERR("[call:%s] REFER: no Refer-To header", getCallId().c_str());

    replyToRequest(inv, rdata, PJSIP_SC_BAD_REQUEST);
}

void
SIPCall::onRequestNotify(pjsip_inv_session* /*inv*/, pjsip_rx_data* /*rdata*/, pjsip_msg* msg)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, msg] { onRequestNotify(nullptr, nullptr, msg); });
        return;
    }
    CHECK_VALID_EXEC_THREAD();
    if (!msg->body)
        return;

    const std::string bodyText {static_cast<char*>(msg->body->data), msg->body->len};
    JAMI_DBG("[call:%s] NOTIFY body start - %p\n%s\n[call:%s] NOTIFY body end - %p",
             getCallId().c_str(),
             msg->body,
             bodyText.c_str(),
             getCallId().c_str(),
             msg->body);
}

void
SIPCall::replyToRequest(pjsip_inv_session* inv, pjsip_rx_data* rdata, int status_code)
{
    CHECK_VALID_EXEC_THREAD();
    const auto ret = pjsip_dlg_respond(inv->dlg, rdata, status_code, nullptr, nullptr, nullptr);
    if (ret != PJ_SUCCESS)
        JAMI_WARN("SIP: failed to reply %d to request", status_code);
}

bool
SIPCall::handleMediaControl(pjsip_msg_body* body)
{
    CHECK_VALID_EXEC_THREAD();

    constexpr pj_str_t STR_APPLICATION = CONST_PJ_STR("application");
    constexpr pj_str_t STR_MEDIA_CONTROL_XML = CONST_PJ_STR("media_control+xml");

    if (body->len and pj_stricmp(&body->content_type.type, &STR_APPLICATION) == 0
        and pj_stricmp(&body->content_type.subtype, &STR_MEDIA_CONTROL_XML) == 0) {
        auto body_msg = std::string_view((char*) body->data, (size_t) body->len);

        /* Apply and answer the INFO request */
        static constexpr auto PICT_FAST_UPDATE = "picture_fast_update"sv;
        static constexpr auto DEVICE_ORIENTATION = "device_orientation"sv;
        static constexpr auto RECORDING_STATE = "recording_state"sv;
        static constexpr auto MUTE_STATE = "mute_state"sv;

        if (body_msg.find(PICT_FAST_UPDATE) != std::string_view::npos) {
            sendKeyframe();
            return true;
        } else if (body_msg.find(DEVICE_ORIENTATION) != std::string_view::npos) {
            static const std::regex ORIENTATION_REGEX("device_orientation=([-+]?[0-9]+)");

            std::svmatch matched_pattern;
            std::regex_search(body_msg, matched_pattern, ORIENTATION_REGEX);

            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                try {
                    int rotation = -std::stoi(matched_pattern[1]);
                    while (rotation <= -180)
                        rotation += 360;
                    while (rotation > 180)
                        rotation -= 360;
                    JAMI_WARN("Rotate video %d deg.", rotation);
#ifdef ENABLE_VIDEO
                    auto const& videoRtp = getVideoRtp();
                    if (videoRtp)
                        videoRtp->setRotation(rotation);
#endif
                } catch (const std::exception& e) {
                    JAMI_WARN("Error parsing angle: %s", e.what());
                }
                return true;
            }
        } else if (body_msg.find(RECORDING_STATE) != std::string_view::npos) {
            static const std::regex REC_REGEX("recording_state=([0-1])");
            std::svmatch matched_pattern;
            std::regex_search(body_msg, matched_pattern, REC_REGEX);

            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                try {
                    bool state = std::stoi(matched_pattern[1]);
                    peerRecording(state);
                } catch (const std::exception& e) {
                    JAMI_WARN("Error parsing state remote recording: %s", e.what());
                }
                return true;
            }
        } else if (body_msg.find(MUTE_STATE) != std::string_view::npos) {
            static const std::regex REC_REGEX("mute_state=([0-1])");
            std::svmatch matched_pattern;
            std::regex_search(body_msg, matched_pattern, REC_REGEX);

            if (matched_pattern.ready() && !matched_pattern.empty() && matched_pattern[1].matched) {
                try {
                    bool state = std::stoi(matched_pattern[1]);
                    peerMuted(state);
                } catch (const std::exception& e) {
                    JAMI_WARN("Error parsing state remote mute: %s", e.what());
                }
                return true;
            }
        }
    }

    return false;
}

bool
SIPCall::transferCall(const std::string& refer_to)
{
    CHECK_VALID_EXEC_THREAD();

    const auto& callId = getCallId();
    JAMI_WARN("[call:%s] Trying to transfer to %s", callId.c_str(), refer_to.c_str());
    try {
        Manager::instance().newOutgoingCall(refer_to, getAccountId());
        Manager::instance().hangupCall(callId);
    } catch (const std::exception& e) {
        JAMI_ERR("[call:%s] SIP transfer failed: %s", callId.c_str(), e.what());
        return false;
    }
    return true;
}

void
SIPCall::startIceMedia()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { startIceMedia(); });
        return;
    }

    auto ice = getIceMediaTransport();

    if (not ice or ice->isFailed()) {
        JAMI_ERR("[call:%s] Media ICE init failed", getCallId().c_str());
        callFailed(EIO);
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
        callFailed(EIO);
        return;
    }
    if (not ice->startIce(rem_ice_attrs, std::move(getAllRemoteCandidates()))) {
        JAMI_ERR("[call:%s] Media ICE start failed", getCallId().c_str());
        callFailed(EIO);
    }
}

void
SIPCall::onIceNegoSucceed()
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this] { onIceNegoSucceed(); });
        return;
    }

    // Check if the call is already ended, so we don't need to restart medias
    // This is typically the case in a multi-device context where one device
    // can stop a call. So do not start medias
    if (not getInviteSession() or getInviteSession()->state == PJSIP_INV_STATE_DISCONNECTED
        or not sdp_)
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

void
SIPCall::onReceiveReinvite(const pjmedia_sdp_session* offer, pjsip_rx_data* rdata)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, offer, rdata] { onReceiveReinvite(offer, rdata); });
        return;
    }

    JAMI_DBG("[call:%s] Received a re-invite", getCallId().c_str());

    if (not sdp_) {
        JAMI_ERR("SDP session is invalid");
        return;
    }

    sdp_->clearIce();
    auto acc = getSIPAccount();
    if (not acc) {
        JAMI_ERR("No account detected");
        return;
    }

    Sdp::printSession(offer, "Remote session (media change request)", SdpDirection::REMOTE_OFFER);

    sdp_->setReceivedOffer(offer);

    auto const& mediaAttrList = Sdp::getMediaAttributeListFromSdp(offer);
    if (mediaAttrList.empty()) {
        JAMI_WARN("[call:%s] Media list is empty, ignoring", getCallId().c_str());
        return;
    }

    if (upnp_) {
        openPortsUPnP();
    }

    pjsip_tx_data* tdata = nullptr;
    if (pjsip_inv_initial_answer(getInviteSession(), rdata, PJSIP_SC_TRYING, NULL, NULL, &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer TRYING");
        return;
    }

    // Report the change request.
    auto const& remoteMediaList = MediaAttribute::mediaAttributesToMediaMaps(mediaAttrList);
    // TODO_MC. Validate this assessment.
    // Report re-invites only if the number of media changed, otherwise answer
    // using the current local attributes.
    if (acc->isMultiStreamEnabled() and remoteMediaList.size() != rtpStreams_.size()) {
        Manager::instance().mediaChangeRequested(getCallId(), getAccountId(), remoteMediaList);
    } else {
        auto localMediaList = getMediaAttributeList();
        answerMediaChangeRequest(localMediaList);
    }
}

void
SIPCall::onReceivedCall(const std::string& from,
                        const std::string& peerNumber,
                        pjmedia_sdp_session* remoteSdp,
                        pjsip_rx_data* rdata,
                        const std::shared_ptr<SipTransport>& transport)
{
    if (not isValidThread()) {
        runOnExecQueueW(
            weak(), [this, from = from, number = peerNumber, remoteSdp, rdata, transp = transport] {
                onReceivedCall(from, number, remoteSdp, rdata, transp);
            });
        return;
    }

    if (auto status = receiveCall(from, peerNumber, remoteSdp, rdata, transport)
                      > PJSIP_SC_ACCEPTED) {
        try_respond_stateless(getSipEndpoint(), rdata, status, nullptr, nullptr, nullptr);
    }
}

pjsip_status_code
SIPCall::receiveCall(const std::string& from,
                     const std::string& peerNumber,
                     pjmedia_sdp_session* remoteSdp,
                     pjsip_rx_data* rdata,
                     const std::shared_ptr<SipTransport>& transport)
{
    CHECK_VALID_EXEC_THREAD();

    auto endPoint = getSipEndpoint();

    setTransport(transport);

    // Support only addresses with same family as the SIP transport
    auto family = pjsip_transport_type_get_af(
        pjsip_transport_get_type_from_flag(transport->get()->flag));

    IpAddr addrSdp;
    auto account = getSIPAccount();
    if (not account)
        return PJSIP_SC_NULL;

    if (account->getUPnPActive()) {
        // Use UPnP addr, or published addr if its set
        addrSdp = account->getPublishedSameasLocal() ? account->getUPnPIpAddress()
                                                     : account->getPublishedIpAddress();
    } else {
        addrSdp = account->isStunEnabled() or (not account->getPublishedSameasLocal())
                      ? account->getPublishedIpAddress()
                      : ip_utils::getInterfaceAddr(account->getLocalInterface(), family);
    }

    /* fallback on local address */
    if (not addrSdp)
        addrSdp = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);

    // Try to obtain display name from From: header first, fallback on Contact:
    auto peerDisplayName = sip_utils::parseDisplayName(rdata->msg_info.from);
    if (peerDisplayName.empty()) {
        if (auto hdr = static_cast<const pjsip_contact_hdr*>(
                pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, nullptr))) {
            peerDisplayName = sip_utils::parseDisplayName(hdr);
        }
    }

    setPeerNumber(peerNumber);
    setPeerUri(account->getToUri(peerNumber));
    setPeerDisplayName(peerDisplayName);
    setState(Call::ConnectionState::PROGRESSING);
    getSDP().setPublishedIP(addrSdp);

    if (account->isStunEnabled())
        updateSDPFromSTUN();

    {
        auto const& remoteMediaList = Sdp::getMediaAttributeListFromSdp(remoteSdp);
        auto hasVideo = MediaAttribute::hasMediaType(remoteMediaList, MediaType::MEDIA_VIDEO);
        // TODO.
        // This list should be built using all the medias in the incoming offer.
        // The local media should be set temporarily inactive (and possibly unconfigured) until
        // we receive accept(mediaList) from the client.
        auto mediaList = account->createDefaultMediaList(account->isVideoEnabled() and hasVideo);
        getSDP().setReceivedOffer(remoteSdp);
        getSDP().processIncomingOffer(mediaList);
    }
    if (remoteSdp)
        setupLocalIce();

    pjsip_dialog* dialog = nullptr;
    if (pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata, nullptr, &dialog)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create uas");
        return PJSIP_SC_INTERNAL_SERVER_ERROR;
    }

    pjsip_tpselector tp_sel = SIPVoIPLink::getTransportSelector(transport->get());
    if (!dialog or pjsip_dlg_set_transport(dialog, &tp_sel) != PJ_SUCCESS) {
        JAMI_ERR("Could not set transport for dialog");
        if (dialog)
            pjsip_dlg_dec_lock(dialog);
        return PJSIP_SC_NULL;
    }

    pjsip_inv_session* inv = nullptr;
    pjsip_inv_create_uas(dialog, rdata, getSDP().getLocalSdpSession(), PJSIP_INV_SUPPORT_ICE, &inv);
    if (!inv) {
        JAMI_ERR("Call invite is not initialized");
        pjsip_dlg_dec_lock(dialog);
        return PJSIP_SC_NULL;
    }

    // dialog is now owned by invite
    pjsip_dlg_dec_lock(dialog);

    setInviteSession(inv);

    // Check whether Replaces header is present in the request and process accordingly.
    pjsip_dialog* replaced_dlg;
    pjsip_tx_data* response;

    // TODO_MC. Lock replaced_dlg?
    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
        JAMI_ERR("Something wrong with Replaces request.");

        // Something wrong with the Replaces header.
        if (response) {
            pjsip_response_addr res_addr;
            pjsip_get_response_addr(response->pool, rdata, &res_addr);
            pjsip_endpt_send_response(endPoint, &res_addr, response, NULL, NULL);
        } else {
            return PJSIP_SC_INTERNAL_SERVER_ERROR;
        }

        return PJSIP_SC_NULL;
    }

    // Check if call has been transferred
    pjsip_tx_data* tdata = nullptr;

    if (pjsip_inv_initial_answer(getInviteSession(), rdata, PJSIP_SC_TRYING, NULL, NULL, &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer TRYING");
        return PJSIP_SC_NULL;
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not send msg TRYING");
        return PJSIP_SC_NULL;
    }

    setState(Call::ConnectionState::TRYING);

    if (pjsip_inv_answer(getInviteSession(), PJSIP_SC_RINGING, NULL, NULL, &tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer RINGING");
        return PJSIP_SC_NULL;
    }

    // contactStr must stay in scope as long as tdata
    const pj_str_t contactStr(account->getContactHeader(transport->get()));
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not send msg RINGING");
        return PJSIP_SC_NULL;
    }

    setState(Call::ConnectionState::RINGING);

    Manager::instance().incomingCall(*this, account->getAccountID());

    if (replaced_dlg) {
        // Get the INVITE session associated with the replaced dialog.
        auto replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

        // Disconnect the "replaced" INVITE session.
        if (pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, nullptr, &tdata) == PJ_SUCCESS
            && tdata) {
            pjsip_inv_send_msg(replaced_inv, tdata);
        }

        // Close call at application level
        hangup(PJSIP_SC_OK);
    }
    try_respond_stateless(endPoint, rdata, 0, nullptr, nullptr, nullptr);
    return PJSIP_SC_NULL;
}

void
SIPCall::onSdpMediaUpdate(pjsip_inv_session* inv, pj_status_t status)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, inv, status] { onSdpMediaUpdate(inv, status); });
        return;
    }

    JAMI_DBG("[call:%s] INVITE@%p media update: status %d", getCallId().c_str(), inv, status);

    if (status != PJ_SUCCESS) {
        const int reason = inv->state != PJSIP_INV_STATE_NULL
                                   and inv->state != PJSIP_INV_STATE_CONFIRMED
                               ? PJSIP_SC_UNSUPPORTED_MEDIA_TYPE
                               : 0;

        JAMI_WARN("[call:%s] SDP offer failed, reason %d", getCallId().c_str(), reason);

        hangup(reason);
        return;
    }

    // Fetch SDP data from request
    const auto localSDP = getActiveLocalSdpFromInvite(inv);
    const auto remoteSDP = getActiveRemoteSdpFromInvite(inv);

    // Update our SDP manager
    auto& sdp = getSDP();
    sdp.setActiveLocalSdpSession(localSDP);
    if (localSDP != nullptr) {
        Sdp::printSession(localSDP, "Local active session:", sdp.getSdpDirection());
    }

    sdp.setActiveRemoteSdpSession(remoteSDP);
    if (remoteSDP != nullptr) {
        Sdp::printSession(remoteSDP, "Remote active session:", sdp.getSdpDirection());
    }
    mediaNegotiationComplete();
}

void
SIPCall::onSdpCreateOffer(pjsip_inv_session* inv, pjmedia_sdp_session** p_offer)
{
    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, inv, p_offer] { onSdpCreateOffer(inv, p_offer); });
        return;
    }

    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("No account detected");
        return;
    }
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
    auto ifaceAddr = ip_utils::getInterfaceAddr(account->getLocalInterface(), family);

    IpAddr address;
    if (account->getUPnPActive()) {
        /* use UPnP addr, or published addr if its set */
        address = account->getPublishedSameasLocal() ? account->getUPnPIpAddress()
                                                     : account->getPublishedIpAddress();
    } else {
        address = account->getPublishedSameasLocal() ? ifaceAddr : account->getPublishedIpAddress();
    }

    /* fallback on local address */
    if (not address)
        address = ifaceAddr;

    auto& sdp = getSDP();
    sdp.setPublishedIP(address);

    // This list should be provided by the client. Kept for backward compatibility.
    auto mediaList = account->createDefaultMediaList(account->isVideoEnabled());

    const bool created = sdp.createOffer(mediaList);

    if (created)
        *p_offer = sdp.getLocalSdpSession();
}

void
SIPCall::onInviteSessionStateChanged(pjsip_inv_session* inv, pjsip_event* ev)
{
    if (not isValidThread()) {
        pjsip_event* event = new pjsip_event(*ev);
        pj_memcpy(event, ev, sizeof(pjsip_event));
        runOnExecQueueW(weak(), [this, inv, event] {
            onInviteSessionStateChanged(inv, event);
            delete event;
        });
        return;
    }

    if (ev->type != PJSIP_EVENT_TSX_STATE and ev->type != PJSIP_EVENT_TX_MSG) {
        JAMI_WARN("[call:%s] INVITE@%p state changed to %d (%s): unexpected event type %d",
                  getCallId().c_str(),
                  inv,
                  inv->state,
                  pjsip_inv_state_name(inv->state),
                  ev->type);
        return;
    }

    decltype(pjsip_transaction::status_code) status_code;

    if (ev->type != PJSIP_EVENT_TX_MSG) {
        const auto tsx = ev->body.tsx_state.tsx;
        status_code = tsx ? tsx->status_code : PJSIP_SC_NOT_FOUND;
        const pj_str_t* description = pjsip_get_status_text(status_code);

        JAMI_DBG("[call:%s] INVITE@%p state changed to %d (%s): cause=%d, tsx@%p status %d (%.*s)",
                 getSipCallId().c_str(),
                 inv,
                 inv->state,
                 pjsip_inv_state_name(inv->state),
                 inv->cause,
                 tsx,
                 status_code,
                 (int) description->slen,
                 description->ptr);
    } else {
        status_code = 0;
        JAMI_DBG("[call:%s] INVITE@%p state changed to %d (%s): cause=%d (TX_MSG)",
                 getSipCallId().c_str(),
                 inv,
                 inv->state,
                 pjsip_inv_state_name(inv->state),
                 inv->cause);
    }

    switch (inv->state) {
    case PJSIP_INV_STATE_EARLY:
        if (status_code == PJSIP_SC_RINGING)
            peerRinging();
        break;

    case PJSIP_INV_STATE_CONFIRMED:
        // After we sent or received a ACK - The connection is established
        peerAnswered();
        break;

    case PJSIP_INV_STATE_DISCONNECTED:
        switch (inv->cause) {
        // When a peer's device replies busy
        case PJSIP_SC_BUSY_HERE:
            peerBusy();
            break;
        // When the peer manually refuse the call
        case PJSIP_SC_DECLINE:
        case PJSIP_SC_BUSY_EVERYWHERE:
            if (inv->role != PJSIP_ROLE_UAC)
                break;
            // close call
            closeCall();
            break;
        // The call terminates normally - BYE / CANCEL
        case PJSIP_SC_OK:
        case PJSIP_SC_REQUEST_TERMINATED:
            closeCall();
            break;

        // Error/unhandled conditions
        default:
            callFailed(inv->cause);
            break;
        }
        break;

    default:
        break;
    }
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

    JAMI_DBG("[call:%s] Received a new offer (re-invite)", getCallId().c_str());

    // This list should be provided by the client. Kept for backward compatibility.
    auto mediaList = acc->createDefaultMediaList(acc->isVideoEnabled(),
                                                 getState() == CallState::HOLD);

    if (upnp_) {
        openPortsUPnP();
    }

    sdp_->setReceivedOffer(offer);
    sdp_->processIncomingOffer(mediaList);

    if (offer)
        setupLocalIce();
    sdp_->startNegotiation();

    pjsip_tx_data* tdata = nullptr;

    if (pjsip_inv_initial_answer(getInviteSession(),
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

    if (pjsip_inv_answer(getInviteSession(), PJSIP_SC_OK, NULL, sdp_->getLocalSdpSession(), &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer OK");
        return !PJ_SUCCESS;
    }

    // ContactStr must stay in scope as long as tdata
    const pj_str_t contactStr(getSIPAccount()->getContactHeader(getTransport()->get()));
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(getInviteSession(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("Could not send msg OK");
        return !PJ_SUCCESS;
    }

    if (upnp_) {
        openPortsUPnP();
    }

    return PJ_SUCCESS;
}

void
SIPCall::openPortsUPnP()
{
    CHECK_VALID_EXEC_THREAD();

    if (not sdp_) {
        JAMI_ERR("[call:%s] Current SDP instance is invalid", getCallId().c_str());
        return;
    }

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

    // RTP port.
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

std::map<std::string, std::string>
SIPCall::getDetails() const
{
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return {};
    }

    std::map<std::string, std::string> details;

    details.emplace(DRing::Call::Details::CALL_TYPE, std::to_string((unsigned) type_));
    details.emplace(DRing::Call::Details::PEER_NUMBER, peerNumber_);
    details.emplace(DRing::Call::Details::DISPLAY_NAME, peerDisplayName_);
    details.emplace(DRing::Call::Details::CALL_STATE, getStateStr());
    details.emplace(DRing::Call::Details::CONF_ID, confID_);
    details.emplace(DRing::Call::Details::TIMESTAMP_START, std::to_string(timestamp_start_));
    details.emplace(DRing::Call::Details::ACCOUNTID, getAccountId());
    details.emplace(DRing::Call::Details::AUDIO_MUTED, std::string(bool_to_str(isAudioMuted())));
    details.emplace(DRing::Call::Details::VIDEO_MUTED, std::string(bool_to_str(isVideoMuted())));
    details.emplace(DRing::Call::Details::AUDIO_ONLY, std::string(bool_to_str(not hasVideo())));
    details.emplace(DRing::Call::Details::PEER_HOLDING, peerHolding_ ? TRUE_STR : FALSE_STR);

#ifdef ENABLE_VIDEO
    LOCK_MEDIA();
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
    LOCK_CALL();
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
    LOCK_MEDIA();
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
    LOCK_MEDIA();
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
        if (hasVideo()) {
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
        if (hasVideo()) {
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

bool
SIPCall::initIceMediaTransport(bool master, std::optional<IceTransportOptions> options)
{
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return false;
    }

    if (not isValidThread()) {
        runOnExecQueueW(weak(), [this, master, opt = std::move(options)] {
            initIceMediaTransport(master, opt);
        });
        return true;
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
                    call->callFailed(EIO);
                    return;
                }
                call->onIceNegoSucceed();
            }
        });
    };

    // Each RTP stream requires a pair of ICE components (RTP + RTCP).
    int compCount = static_cast<int>(getStreamCount()) * 2;
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    auto transport = iceTransportFactory.createUTransport(getCallId().c_str(),
                                                          compCount,
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
SIPCall::merge(std::shared_ptr<Call> call)
{
    CHECK_VALID_EXEC_THREAD();

    JAMI_DBG("[call:%s] merge subcall %s", getCallId().c_str(), call->getCallId().c_str());

    // This static cast is safe as this method is private and overload Call::merge
    auto const& subCall = std::dynamic_pointer_cast<SIPCall>(call);

    std::lock(callMutex_, subCall->callMutex_);
    std::lock_guard<std::recursive_mutex> lk1 {callMutex_, std::adopt_lock};
    std::lock_guard<std::recursive_mutex> lk2 {subCall->callMutex_, std::adopt_lock};

    // Update invite session
    setInviteSession(subCall->inviteSession_);
    subCall->setInviteSession();

    setTransport(std::move(subCall->transport_));
    sdp_ = std::move(subCall->sdp_);
    peerHolding_ = subCall->peerHolding_;
    upnp_ = std::move(subCall->upnp_);
    std::copy_n(subCall->contactBuffer_, PJSIP_MAX_URL_SIZE, contactBuffer_);
    pj_strcpy(&contactHeader_, &subCall->contactHeader_);
    localAudioPort_ = subCall->localAudioPort_;
    localVideoPort_ = subCall->localVideoPort_;
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        mediaTransport_ = std::move(subCall->mediaTransport_);
        tmpMediaTransport_ = std::move(subCall->tmpMediaTransport_);
    }

    Call::merge(subCall);
    startIceMedia();
}

bool
SIPCall::remoteHasValidIceAttributes()
{
    if (not sdp_) {
        throw std::runtime_error("Must have a valid SDP Session");
    }

    auto rem_ice_attrs = sdp_->getIceAttributes();
    return not rem_ice_attrs.ufrag.empty() and not rem_ice_attrs.pwd.empty();
}

void
SIPCall::setupLocalIce()
{
    if (not remoteHasValidIceAttributes()) {
        // If ICE attributes are not present, skip the ICE initialization
        // step (most likely ICE is not used).
        JAMI_WARN("[call:%s] no ICE data in remote SDP", getCallId().c_str());
        return;
    }

    if (not initIceMediaTransport(false)) {
        JAMI_ERR("[call:%s] ICE initialization failed", getCallId().c_str());
        // Fatal condition
        // TODO: what's SIP rfc says about that?
        // (same question in startIceMedia)
        callFailed(EIO);
        return;
    }

    // WARNING: This call blocks! (need ice init done)
    addLocalIceAttributes();
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

SIPVoIPLink&
SIPCall::getSipVoIPLink() const
{
    return Manager::instance().sipVoIPLink();
}

pjsip_endpoint*
SIPCall::getSipEndpoint() const
{
    return getSipVoIPLink().getEndpoint();
}

const pjmedia_sdp_session*
SIPCall::getActiveLocalSdpFromInvite(pjsip_inv_session* inv)
{
    const pjmedia_sdp_session* sdp_session {};

    if (pjmedia_sdp_neg_get_active_local(inv->neg, &sdp_session) != PJ_SUCCESS) {
        JAMI_ERR("Active local not present");
        return nullptr;
    }

    if (pjmedia_sdp_validate(sdp_session) != PJ_SUCCESS) {
        JAMI_ERR("Invalid local SDP session");
        return nullptr;
    }

    return sdp_session;
}

const pjmedia_sdp_session*
SIPCall::getActiveRemoteSdpFromInvite(pjsip_inv_session* inv)
{
    const pjmedia_sdp_session* sdp_session {};

    if (pjmedia_sdp_neg_get_active_remote(inv->neg, &sdp_session) != PJ_SUCCESS) {
        JAMI_ERR("Active remote not present");
        return nullptr;
    }

    if (pjmedia_sdp_validate(sdp_session) != PJ_SUCCESS) {
        JAMI_ERR("Invalid remote SDP session");
        return nullptr;
    }

    return sdp_session;
}
} // namespace jami
