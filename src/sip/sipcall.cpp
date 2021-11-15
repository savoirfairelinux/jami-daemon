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
#include "sipaccount.h"
#include "sipaccountbase.h"
#include "sipvoiplink.h"
#include "logger.h"
#include "sdp.h"
#include "manager.h"
#include "string_utils.h"
#include "upnp/upnp_control.h"
#include "sip_utils.h"
#include "audio/audio_rtp_session.h"
#include "system_codec_container.h"
#include "im/instant_messaging.h"
#include "jami/call_const.h"
#include "jami/media_const.h"
#include "client/ring_signal.h"
#include "ice_transport.h"
#include "pjsip-ua/sip_inv.h"

#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#endif

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#include "video/video_rtp_session.h"
#include "jami/videomanager_interface.h"
#include <chrono>
#include <libavutil/display.h>
#include <video/sinkclient.h>
#endif
#include "audio/ringbufferpool.h"
#include "jamidht/channeled_transport.h"

#include "errno.h"

#include <opendht/crypto.h>
#include <opendht/thread_pool.h>
#include <fmt/ranges.h>

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
static constexpr std::chrono::milliseconds EXPECTED_ICE_INIT_MAX_TIME {5000};
static constexpr std::chrono::seconds DEFAULT_ICE_NEGO_TIMEOUT {60}; // seconds
static constexpr std::chrono::milliseconds MS_BETWEEN_2_KEYFRAME_REQUEST {1000};
static constexpr int ICE_COMP_ID_RTP {1};
static constexpr int ICE_COMP_COUNT_PER_STREAM {2};
static constexpr auto MULTISTREAM_REQUIRED_VERSION_STR = "10.0.2"sv;
static const std::vector<unsigned> MULTISTREAM_REQUIRED_VERSION
    = split_string_to_unsigned(MULTISTREAM_REQUIRED_VERSION_STR, '.');

constexpr auto DUMMY_VIDEO_STR = "dummy video session";

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::map<std::string, std::string>& details)
    : Call(account, callId, type, details)
    , sdp_(new Sdp(callId))
    , enableIce_(account->isIceForMediaEnabled())
    , srtpEnabled_(account->isSrtpEnabled())
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
    JAMI_DBG("[call:%s] Create a new [%s] SIP call with %lu media",
             getCallId().c_str(),
             type == Call::CallType::INCOMING
                 ? "INCOMING"
                 : (type == Call::CallType::OUTGOING ? "OUTGOING" : "MISSED"),
             mediaAttrList.size());

    initMediaStreams(mediaAttrList);
}

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::vector<DRing::MediaMap>& mediaList)
    : Call(account, callId, type)
    , peerSupportMultiStream_(false)
    , sdp_(new Sdp(callId))
    , enableIce_(account->isIceForMediaEnabled())
    , srtpEnabled_(account->isSrtpEnabled())
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

    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, isSrtpEnabled());

    if (mediaAttrList.size() == 0) {
        if (type_ == Call::CallType::INCOMING) {
            // Handle incoming call without media offer.
            JAMI_WARN(
                "[call:%s] No media offered in the incoming invite. An offer will be provided in "
                "the answer",
                getCallId().c_str());
            mediaAttrList = getSIPAccount()->createDefaultMediaList(false,
                                                                    getState() == CallState::HOLD);
        } else {
            JAMI_WARN("[call:%s] Creating an outgoing call with empty offer", getCallId().c_str());
        }
    }

    JAMI_DBG("[call:%s] Create a new [%s] SIP call with %lu media",
             getCallId().c_str(),
             type == Call::CallType::INCOMING
                 ? "INCOMING"
                 : (type == Call::CallType::OUTGOING ? "OUTGOING" : "MISSED"),
             mediaList.size());

    initMediaStreams(mediaAttrList);
}

SIPCall::~SIPCall()
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    setSipTransport({});
    setInviteSession(); // prevents callback usage
}

size_t
SIPCall::findRtpStreamIndex(const std::string& label) const
{
    const auto iter = std::find_if(rtpStreams_.begin(),
                                   rtpStreams_.end(),
                                   [&label](const RtpStream& rtp) {
                                       return label == rtp.mediaAttribute_->label_;
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
    JAMI_DBG("[call:%s] Configuring [%s] rtp session",
             getCallId().c_str(),
             MediaAttribute::mediaTypeToString(mediaAttr->type_));

    if (not rtpSession)
        throw std::runtime_error("Must have a valid RTP Session");

    // Configure the media stream
    auto new_mtu = sipTransport_->getTlsMtu();
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
        if (auto& videoReceive = videoRtp->getVideoReceive()) {
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

    // TODO. Setting specfic range for RTP ports is obsolete, in
    // particular in the context of ICE.

    // Reference: http://www.cs.columbia.edu/~hgs/rtp/faq.html#ports
    // We only want to set ports to new values if they haven't been set
    const unsigned callLocalAudioPort = account->generateAudioPort();
    if (localAudioPort_ != 0)
        account->releasePort(localAudioPort_);
    localAudioPort_ = callLocalAudioPort;
    sdp_->setLocalPublishedAudioPorts(callLocalAudioPort,
                                      rtcpMuxEnabled_ ? 0 : callLocalAudioPort + 1);

#ifdef ENABLE_VIDEO
    // https://projects.savoirfairelinux.com/issues/17498
    const unsigned int callLocalVideoPort = account->generateVideoPort();
    if (localVideoPort_ != 0)
        account->releasePort(localVideoPort_);
    // this should already be guaranteed by SIPAccount
    assert(localAudioPort_ != callLocalVideoPort);
    localVideoPort_ = callLocalVideoPort;
    sdp_->setLocalPublishedVideoPorts(callLocalVideoPort,
                                      rtcpMuxEnabled_ ? 0 : callLocalVideoPort + 1);
#endif
}

const std::string&
SIPCall::getContactHeader() const
{
    return contactHeader_;
}

void
SIPCall::setSipTransport(const std::shared_ptr<SipTransport>& transport,
                         const std::string& contactHdr)
{
    if (transport != sipTransport_) {
        JAMI_DBG("[call:%s] Setting tranport to [%p]", getCallId().c_str(), transport.get());
    }

    sipTransport_ = transport;
    contactHeader_ = contactHdr;

    if (not transport) {
        // Done.
        return;
    }

    if (contactHeader_.empty()) {
        JAMI_WARN("[call:%s] Contact header is empty, the call will likely fail",
                  getCallId().c_str());
    }

    if (isSrtpEnabled() and not sipTransport_->isSecure()) {
        JAMI_WARN("[call:%s] Crypto (SRTP) is negotiated over an un-encrypted signaling channel",
                  getCallId().c_str());
    }

    if (not isSrtpEnabled() and sipTransport_->isSecure()) {
        JAMI_WARN("[call:%s] The signaling channel is encrypted but the media is not encrypted",
                  getCallId().c_str());
    }

    const auto list_id = reinterpret_cast<uintptr_t>(this);
    sipTransport_->removeStateListener(list_id);

    // listen for transport destruction
    sipTransport_->addStateListener(
        list_id, [wthis_ = weak()](pjsip_transport_state state, const pjsip_transport_state_info*) {
            if (auto this_ = wthis_.lock()) {
                JAMI_DBG("[call:%s] SIP transport state [%i] - connection state [%u]",
                         this_->getCallId().c_str(),
                         state,
                         static_cast<unsigned>(this_->getConnectionState()));

                // End the call if the SIP transport was shut down
                auto isAlive = SipTransport::isAlive(state);
                if (not isAlive and this_->getConnectionState() != ConnectionState::DISCONNECTED) {
                    JAMI_WARN("[call:%s] Ending call because underlying SIP transport was closed",
                              this_->getCallId().c_str());
                    this_->stopAllMedia();
                    this_->onFailure(ECONNRESET);
                }
            }
        });
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

    JAMI_DBG("[call:%s] Preparing and sending a re-invite (state=%s)",
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

    if (isIceEnabled()) {
        createIceMediaTransport();
        if (initIceMediaTransport(true))
            addLocalIceAttributes();
    }

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
    auto mediaList = getMediaAttributeList();
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
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    if (inviteSession == nullptr and inviteSession_) {
        JAMI_DBG("[call:%s] Delete current invite session", getCallId().c_str());
    } else if (inviteSession != nullptr) {
        // NOTE: The first reference of the invite session is owned by pjsip. If
        // that counter goes down to zero the invite will be destroyed, and the
        // unique_ptr will point freed datas.  To avoid this, we increment the
        // ref counter and let our unique_ptr share the ownership of the session
        // with pjsip.
        if (PJ_SUCCESS != pjsip_inv_add_ref(inviteSession)) {
            JAMI_WARN("[call:%s] trying to set invalid invite session [%p]",
                      getCallId().c_str(),
                      inviteSession);
            inviteSession_.reset(nullptr);
            return;
        }
        JAMI_DBG("[call:%s] Set new invite session [%p]", getCallId().c_str(), inviteSession);
    } else {
        // Nothing to do.
        return;
    }

    inviteSession_.reset(inviteSession);
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
                    sip_utils::addContactHeader(contactHeader_, tdata);
                    // Add user-agent header
                    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);
                } else {
                    JAMI_ERR("No account detected");
                    std::ostringstream msg;
                    msg << "[call:" << getCallId().c_str() << "] "
                        << "The account owning this call is invalid";
                    throw std::runtime_error(msg.str());
                }

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

        Manager::instance().sipVoIPLink().createSDPOffer(inviteSession_.get());
    }

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

    if (contactHeader_.empty()) {
        throw std::runtime_error("Cant answer with an invalid contact header");
    }

    JAMI_DBG("[call:%s] Answering with contact header: %s",
             getCallId().c_str(),
             contactHeader_.c_str());

    sip_utils::addContactHeader(contactHeader_, tdata);

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        setInviteSession();
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::answer(const std::vector<DRing::MediaMap>& mediaList)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("No account detected");
        return;
    }

    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, isSrtpEnabled());

    if (mediaAttrList.empty()) {
        JAMI_DBG("[call:%s] Media list must not be empty!", getCallId().c_str());
        return;
    }

    if (not inviteSession_)
        JAMI_DBG("[call:%s] No invite session for this call", getCallId().c_str());

    JAMI_DBG("[call:%s] Answering incoming call with %lu media:",
             getCallId().c_str(),
             mediaAttrList.size());

    if (mediaAttrList.size() != rtpStreams_.size()) {
        JAMI_ERR("[call:%s] Media list size %lu in answer does not match. Expected %lu",
                 getCallId().c_str(),
                 mediaAttrList.size(),
                 rtpStreams_.size());
        return;
    }

    for (size_t idx = 0; idx < mediaAttrList.size(); idx++) {
        auto const& mediaAttr = mediaAttrList.at(idx);
        JAMI_DBG("[call:%s] Media @%lu: %s",
                 getCallId().c_str(),
                 idx,
                 mediaAttr.toString(true).c_str());
    }

    // Apply the media attributes provided by the user.
    for (size_t idx = 0; idx < mediaAttrList.size(); idx++) {
        updateMediaStream(mediaAttrList[idx], idx);
    }

    if (not inviteSession_)
        throw VoipLinkException("[call:" + getCallId()
                                + "] answer: no invite session for this call");

    if (not inviteSession_->neg) {
        // We are answering to an INVITE that did not include a media offer (SDP).
        // The SIP specification (RFCs 3261/6337) requires that if a UA wishes to
        // proceed with the call, it must provide a media offer (SDP) if the initial
        // INVITE did not offer one. In this case, the SDP offer will be included in
        // the SIP OK (200) answer. The peer UA will then include its SDP answer in
        // the SIP ACK message.

        // TODO. This code should be unified with the code used by accounts to create
        // SDP offers.

        JAMI_WARN("[call:%s] No negotiator session, peer sent an empty INVITE (without SDP)",
                  getCallId().c_str());

        Manager::instance().sipVoIPLink().createSDPOffer(inviteSession_.get());

        generateMediaPorts();

        // Setup and create ICE offer
        if (isIceEnabled()) {
            sdp_->clearIce();

            auto opts = account->getIceOptions();

            auto publicAddr = account->getPublishedIpAddress();

            if (publicAddr) {
                opts.accountPublicAddr = publicAddr;
                if (auto interfaceAddr = ip_utils::getInterfaceAddr(account->getLocalInterface(),
                                                                    publicAddr.getFamily())) {
                    opts.accountLocalAddr = interfaceAddr;
                    createIceMediaTransport();
                    if (initIceMediaTransport(true, std::move(opts)))
                        addLocalIceAttributes();
                } else {
                    JAMI_WARN("[call:%s] Cant init ICE transport, missing local address",
                              getCallId().c_str());
                }
            } else {
                JAMI_WARN("[call:%s] Cant init ICE transport, missing public address",
                          getCallId().c_str());
            }
        }
    }

    if (!inviteSession_->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // Answer with an SDP offer if the initial invite was empty,
    // otherwise, set the local_sdp session to null to use the
    // current SDP session.
    pjsip_tx_data* tdata;
    if (pjsip_inv_answer(inviteSession_.get(),
                         PJSIP_SC_OK,
                         NULL,
                         not inviteSession_->neg ? sdp_->getLocalSdpSession() : NULL,
                         &tdata)
        != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    if (contactHeader_.empty()) {
        throw std::runtime_error("Cant answer with an invalid contact header");
    }

    JAMI_DBG("[call:%s] Answering with contact header: %s",
             getCallId().c_str(),
             contactHeader_.c_str());

    sip_utils::addContactHeader(contactHeader_, tdata);

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        setInviteSession();
        throw std::runtime_error("Could not send invite request answer (200 OK)");
    }

    setState(CallState::ACTIVE, ConnectionState::CONNECTED);
}

void
SIPCall::answerMediaChangeRequest(const std::vector<DRing::MediaMap>& mediaList)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};

    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("[call:%s] No account detected", getCallId().c_str());
        return;
    }

    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, isSrtpEnabled());

    // TODO. is the right place?
    // Disable video if disabled in the account.
    if (not account->isVideoEnabled()) {
        for (auto& mediaAttr : mediaAttrList) {
            if (mediaAttr.type_ == MediaType::MEDIA_VIDEO) {
                mediaAttr.enabled_ = false;
            }
        }
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

    if (isIceEnabled())
        setupIceResponse();

    if (not sdp_->startNegotiation()) {
        JAMI_ERR("[call:%s] Could not start media negotiation for a re-invite request",
                 getCallId().c_str());
        return;
    }

    if (pjsip_inv_set_sdp_answer(inviteSession_.get(), sdp_->getLocalSdpSession()) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not start media negotiation for a re-invite request",
                 getCallId().c_str());
        return;
    }

    pjsip_tx_data* tdata;
    if (pjsip_inv_answer(inviteSession_.get(), PJSIP_SC_OK, NULL, NULL, &tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not init answer to a re-invite request", getCallId().c_str());
        return;
    }

    if (not contactHeader_.empty()) {
        sip_utils::addContactHeader(contactHeader_, tdata);
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(account->getUserAgentName(), tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not send answer to a re-invite request", getCallId().c_str());
        setInviteSession();
        return;
    }

    JAMI_DBG("[call:%s] Successfully answered the media change request", getCallId().c_str());
}

void
SIPCall::hangup(int reason)
{
    std::lock_guard<std::recursive_mutex> lk {callMutex_};
    pendingRecord_ = false;
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
    if (getConnectionState() != ConnectionState::CONNECTED) {
        JAMI_WARN("[call:%s] Not connected, ignoring hold request", getCallId().c_str());
        return false;
    }

    if (not setState(CallState::HOLD)) {
        JAMI_WARN("[call:%s] Failed to set state to HOLD", getCallId().c_str());
        return false;
    }

    stopAllMedia();

    for (auto& stream : rtpStreams_) {
        stream.mediaAttribute_->onHold_ = true;
    }

    if (SIPSessionReinvite() != PJ_SUCCESS) {
        JAMI_WARN("[call:%s] Reinvite failed", getCallId().c_str());
        return false;
    }

    isWaitingForIceAndMedia_ = true;
    JAMI_DBG("[call:%s] Set state to HOLD", getCallId().c_str());
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
    if (getConnectionState() != ConnectionState::CONNECTED) {
        JAMI_WARN("[call:%s] Not connected, ignoring resume request", getCallId().c_str());
    }

    if (not setState(CallState::ACTIVE))
        return false;

    sdp_cb();

    {
        for (auto& stream : rtpStreams_) {
            stream.mediaAttribute_->onHold_ = false;
        }
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
    pendingRecord_ = false;
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

    JAMI_DBG("Sending device orientation via SIP INFO %d", rotation);

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
                // Ignore if the peer does not allow "MESSAGE" SIP method
                // NOTE:
                // The SIP "Allow" header is not mandatory as per RFC-3261. If it's
                // not present and since "MESSAGE" method is an extention method,
                // we choose to assume that the peer does not support the "MESSAGE"
                // method to prevent unexpected behavior when interoperating with
                // some SIP implementations.
                const std::string msgMethod {"MESSAGE"};
                if (not peerAllowsMethod(msgMethod)) {
                    JAMI_WARN("[call:%s] Peer does not allow \"%s\" method",
                              getCallId().c_str(),
                              msgMethod.c_str());
                    return;
                }

                im::sendSipMessage(inviteSession_.get(), messages);

            } catch (...) {
                JAMI_ERR("[call:%s] Failed to send SIP text message", getCallId().c_str());
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
    JAMI_DBG("[call:%s] removeCall()", getCallId().c_str());
    if (sdp_) {
        sdp_->setActiveLocalSdpSession(nullptr);
        sdp_->setActiveRemoteSdpSession(nullptr);
    }
    Call::removeCall();

    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        resetTransport(std::move(mediaTransport_));
    }

    setInviteSession();
    setSipTransport({});
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

bool
SIPCall::isIceEnabled() const
{
    return enableIce_;
}

void
SIPCall::setPeerUaVersion(std::string_view ua)
{
    if (peerUserAgent_ == ua or ua.empty()) {
        // Silently ignore if it did not change or empty.
        return;
    }

    if (peerUserAgent_.empty()) {
        JAMI_DBG("[call:%s] Set peer's User-Agent to [%.*s]",
                 getCallId().c_str(),
                 (int) ua.size(),
                 ua.data());
    } else if (not peerUserAgent_.empty()) {
        // Unlikely, but should be handled since we dont have control over the peer.
        // Even if it's unexpected, we still try to parse the UA version.
        JAMI_WARN("[call:%s] Peer's User-Agent unexpectedly changed from [%s] to [%.*s]",
                  getCallId().c_str(),
                  peerUserAgent_.c_str(),
                  (int) ua.size(),
                  ua.data());
    }

    peerUserAgent_ = ua;

    // User-agent parsing
    constexpr std::string_view PACK_NAME(PACKAGE_NAME " ");
    auto pos = ua.find(PACK_NAME);
    if (pos == std::string_view::npos) {
        // Must have the expected package name.
        JAMI_WARN("Could not find the expected package name in peer's User-Agent");
        return;
    }

    ua = ua.substr(pos + PACK_NAME.length());

    std::string_view version;
    // Unstable (un-released) versions has a hiphen + commit Id after
    // the version number. Find the commit Id if any, and ignore it.
    pos = ua.find('-');
    if (pos != std::string_view::npos) {
        // Get the version and ignore the commit ID.
        version = ua.substr(0, pos);
    } else {
        // Extract the version number.
        pos = ua.find(' ');
        if (pos != std::string_view::npos) {
            version = ua.substr(0, pos);
        }
    }

    if (version.empty()) {
        JAMI_DBG("[call:%s] Could not parse peer's version", getCallId().c_str());
        return;
    }

    auto peerVersion = split_string_to_unsigned(version, '.');
    if (peerVersion.size() > 4u) {
        JAMI_WARN("[call:%s] Could not parse peer's version", getCallId().c_str());
        return;
    }

    // Check if peer's version is at least 10.0.2 to enable multi-stream.
    peerSupportMultiStream_ = Account::meetMinimumRequiredVersion(peerVersion,
                                                                  MULTISTREAM_REQUIRED_VERSION);
    if (not peerSupportMultiStream_) {
        JAMI_DBG(
            "Peer's version [%.*s] does not support multi-stream. Min required version: [%.*s]",
            (int) version.size(),
            version.data(),
            (int) MULTISTREAM_REQUIRED_VERSION_STR.size(),
            MULTISTREAM_REQUIRED_VERSION_STR.data());
    }
}

void
SIPCall::setPeerAllowMethods(std::vector<std::string> methods)
{
    std::lock_guard<std::recursive_mutex> lock {callMutex_};
    peerAllowedMethods_ = methods;
}

bool
SIPCall::peerAllowsMethod(const std::string& method) const
{
    std::lock_guard<std::recursive_mutex> lock {callMutex_};

    if (std::find(peerAllowedMethods_.begin(), peerAllowedMethods_.end(), method)
        != peerAllowedMethods_.end()) {
        return true;
    }

    // Print peer's allowed methods
    JAMI_WARN() << fmt::format("[call:{}] Peer's allowed methods: {}",
                               getCallId(),
                               peerAllowedMethods_);
    return false;
}

void
SIPCall::onPeerRinging()
{
    JAMI_DBG("[call:%s] Peer ringing", getCallId().c_str());
    setState(ConnectionState::RINGING);
}

void
SIPCall::addLocalIceAttributes()
{
    if (not isIceEnabled())
        return;

    auto mediaTransport = getIceMedia();
    if (not mediaTransport) {
        JAMI_ERR("[call:%s] Invalid ICE instance", getCallId().c_str());
        return;
    }

    auto start = std::chrono::steady_clock::now();

    if (not mediaTransport->isInitialized()) {
        JAMI_DBG("[call:%s] Waiting for ICE initialization", getCallId().c_str());
        // we need an initialized ICE to progress further
        if (mediaTransport->waitForInitialization(DEFAULT_ICE_INIT_TIMEOUT) <= 0) {
            JAMI_ERR("[call:%s] ICE initialization timed out", getCallId().c_str());
            return;
        }
        // ICE initialization may take longer than usual in some cases,
        // for instance when TURN servers do not respond in time (DNS
        // resolution or other issues).
        auto duration = std::chrono::steady_clock::now() - start;
        if (duration > EXPECTED_ICE_INIT_MAX_TIME) {
            JAMI_WARN("[call:%s] ICE initialization time was unexpectedly high (%ld ms)",
                      getCallId().c_str(),
                      std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
        }
    }

    // Check the state of ICE instance, the initialization may have failed.
    if (not mediaTransport->isInitialized()) {
        JAMI_ERR("[call:%s] ICE session is not initialized", getCallId().c_str());
        return;
    }

    // Check the state, the call might have been canceled while waiting.
    // for initialization.
    if (getState() == Call::CallState::OVER) {
        JAMI_WARN("[call:%s] The call was terminated while waiting for ICE initialization",
                  getCallId().c_str());
        return;
    }

    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("No account detected");
        return;
    }
    if (not sdp_) {
        JAMI_ERR("No sdp detected");
        return;
    }

    JAMI_DBG("[call:%s] Add local attributes for ICE instance [%p]",
             getCallId().c_str(),
             mediaTransport.get());

    sdp_->addIceAttributes(mediaTransport->getLocalAttributes());

    if (account->isIceCompIdRfc5245Compliant()) {
        unsigned streamIdx = 0;
        for (auto const& stream : rtpStreams_) {
            if (not stream.mediaAttribute_->enabled_) {
                // Dont add ICE candidates if the media is disabled
                JAMI_DBG("[call:%s] media [%s] @ %u is disabled, dont add local candidates",
                         getCallId().c_str(),
                         stream.mediaAttribute_->toString().c_str(),
                         streamIdx);
                continue;
            }
            JAMI_DBG("[call:%s] add ICE local candidates for media [%s] @ %u",
                     getCallId().c_str(),
                     stream.mediaAttribute_->toString().c_str(),
                     streamIdx);
            // RTP
            sdp_->addIceCandidates(streamIdx,
                                   mediaTransport->getLocalCandidates(streamIdx, ICE_COMP_ID_RTP));
            // RTCP if it has its own port
            if (not rtcpMuxEnabled_) {
                sdp_->addIceCandidates(streamIdx,
                                       mediaTransport->getLocalCandidates(streamIdx,
                                                                          ICE_COMP_ID_RTP + 1));
            }

            streamIdx++;
        }
    } else {
        unsigned idx = 0;
        unsigned compId = 1;
        for (auto const& stream : rtpStreams_) {
            if (not stream.mediaAttribute_->enabled_) {
                // Skipping local ICE candidates if the media is disabled
                continue;
            }
            JAMI_DBG("[call:%s] add ICE local candidates for media [%s] @ %u",
                     getCallId().c_str(),
                     stream.mediaAttribute_->toString().c_str(),
                     idx);
            // RTP
            sdp_->addIceCandidates(idx, mediaTransport->getLocalCandidates(compId));
            compId++;

            // RTCP if it has its own port
            if (not rtcpMuxEnabled_) {
                sdp_->addIceCandidates(idx, mediaTransport->getLocalCandidates(compId));
                compId++;
            }

            idx++;
        }
    }
}

std::vector<IceCandidate>
SIPCall::getAllRemoteCandidates(IceTransport& transport) const
{
    std::vector<IceCandidate> rem_candidates;
    for (unsigned mediaIdx = 0; mediaIdx < static_cast<unsigned>(rtpStreams_.size()); mediaIdx++) {
        IceCandidate cand;
        for (auto& line : sdp_->getIceCandidates(mediaIdx)) {
            if (transport.parseIceAttributeLine(mediaIdx, line, cand)) {
                JAMI_DBG("[call:%s] Add remote ICE candidate: %s",
                         getCallId().c_str(),
                         line.c_str());
                rem_candidates.emplace_back(std::move(cand));
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

    if (stream.mediaAttribute_->sourceType_ == MediaSourceType::NONE) {
        stream.mediaAttribute_->sourceType_ = MediaSourceType::CAPTURE_DEVICE;
    }

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

    JAMI_DBG("[call:%s] Created %lu Media streams", getCallId().c_str(), rtpStreams_.size());

    return rtpStreams_.size();
}

bool
SIPCall::hasVideo() const
{
#ifdef ENABLE_VIDEO
    std::function<bool(const RtpStream& stream)> videoCheck = [](auto const& stream) {
        return stream.mediaAttribute_->type_ == MediaType::MEDIA_VIDEO;
    };

    const auto iter = std::find_if(rtpStreams_.begin(), rtpStreams_.end(), videoCheck);

    return iter != rtpStreams_.end();
#else
    return false;
#endif
}

bool
SIPCall::isCaptureDeviceMuted(const MediaType& mediaType) const
{
    // Return true only if all media of type 'mediaType' that use capture devices
    // source, are muted.
    std::function<bool(const RtpStream& stream)> mutedCheck = [&mediaType](auto const& stream) {
        return (stream.mediaAttribute_->type_ == mediaType
                and stream.mediaAttribute_->sourceType_ == MediaSourceType::CAPTURE_DEVICE
                and not stream.mediaAttribute_->muted_);
    };
    const auto iter = std::find_if(rtpStreams_.begin(), rtpStreams_.end(), mutedCheck);
    return iter == rtpStreams_.end();
}

void
SIPCall::updateNegotiatedMedia()
{
    JAMI_DBG("[call:%s] updating negotiated media", getCallId().c_str());

    if (not sipTransport_ or not sdp_) {
        JAMI_ERR("[call:%s] the call is in invalid state", getCallId().c_str());
        return;
    }

    auto slots = sdp_->getMediaSlots();
    bool peer_holding {true};
    int streamIdx = -1;

    for (const auto& slot : slots) {
        streamIdx++;
        const auto& local = slot.first;
        const auto& remote = slot.second;

        // Skip disabled media
        if (not local.enabled) {
            JAMI_DBG("[call:%s] [SDP:slot#%u] The media is disabled, skipping",
                     getCallId().c_str(),
                     streamIdx);
            continue;
        }

        if (static_cast<size_t>(streamIdx) >= rtpStreams_.size()) {
            throw std::runtime_error("Stream index is out-of-range");
        }

        auto const& rtpStream = rtpStreams_[streamIdx];

        if (not rtpStream.mediaAttribute_) {
            throw std::runtime_error("Missing media attribute");
        }

        // To enable a media, it must be enabled on both sides.
        rtpStream.mediaAttribute_->enabled_ = local.enabled and remote.enabled;

        if (not rtpStream.rtpSession_)
            throw std::runtime_error("Must have a valid RTP Session");

        if (local.type != MEDIA_AUDIO && local.type != MEDIA_VIDEO) {
            JAMI_ERR("[call:%s] Unexpected media type %u", getCallId().c_str(), local.type);
            throw std::runtime_error("Invalid media attribute");
        }

        if (local.type != remote.type) {
            JAMI_ERR("[call:%s] [SDP:slot#%u] Inconsistent media type between local and remote",
                     getCallId().c_str(),
                     streamIdx);
            continue;
        }

        if (local.enabled and not local.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing local codec", getCallId().c_str(), streamIdx);
            continue;
        }

        if (remote.enabled and not remote.codec) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Missing remote codec",
                      getCallId().c_str(),
                      streamIdx);
            continue;
        }

        if (isSrtpEnabled() and local.enabled and not local.crypto) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Secure mode but no local crypto attributes. "
                      "Ignoring the media",
                      getCallId().c_str(),
                      streamIdx);
            continue;
        }

        if (isSrtpEnabled() and remote.enabled and not remote.crypto) {
            JAMI_WARN("[call:%s] [SDP:slot#%u] Secure mode but no crypto remote attributes. "
                      "Ignoring the media",
                      getCallId().c_str(),
                      streamIdx);
            continue;
        }

        // Aggregate holding info over all remote streams
        peer_holding &= remote.onHold;

        configureRtpSession(rtpStream.rtpSession_, rtpStream.mediaAttribute_, local, remote);
    }

    if (not isSubcall() and peerHolding_ != peer_holding) {
        peerHolding_ = peer_holding;
        emitSignal<DRing::CallSignal::PeerHold>(getCallId(), peerHolding_);
    }

    // Notify using the parent Id if it's a subcall.
    auto callId = isSubcall() ? parent_->getCallId() : getCallId();
    emitSignal<DRing::CallSignal::MediaNegotiationStatus>(
        callId,
        DRing::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS,
        MediaAttribute::mediaAttributesToMediaMaps(getMediaAttributeList()));
}

void
SIPCall::startAllMedia()
{
    JAMI_DBG("[call:%s] starting the media", getCallId().c_str());

    if (not sipTransport_ or not sdp_) {
        JAMI_ERR("[call:%s] the call is in invalid state", getCallId().c_str());
        return;
    }

    if (isSrtpEnabled() && not sipTransport_->isSecure()) {
        JAMI_WARN("[call:%s] Crypto (SRTP) is negotiated over an insecure signaling transport",
                  getCallId().c_str());
    }

    // reset
    readyToRecord_ = false;
    resetMediaReady();
#ifdef ENABLE_VIDEO
    bool hasActiveVideo = false;
#endif

    int currentCompId = 1;

    for (auto iter = rtpStreams_.begin(); iter != rtpStreams_.end(); iter++) {
        if (not iter->mediaAttribute_) {
            throw std::runtime_error("Missing media attribute");
        }

#ifdef ENABLE_VIDEO
        if (iter->mediaAttribute_->type_ == MEDIA_VIDEO)
            hasActiveVideo |= iter->mediaAttribute_->enabled_;
#endif

        // Not restarting media loop on hold as it's a huge waste of CPU ressources
        // because of the audio loop
        if (getState() != CallState::HOLD) {
            if (isIceRunning()) {
                // Create sockets for RTP and RTCP, and start the session.
                auto iceRtpSocket = newIceSocket(currentCompId++);

                std::unique_ptr<IceSocket> iceRtcpSocket;
                if (not rtcpMuxEnabled_) {
                    iceRtcpSocket = newIceSocket(currentCompId++);
                }
                iter->rtpSession_->start(std::move(iceRtpSocket), std::move(iceRtcpSocket));
            } else {
                iter->rtpSession_->start(nullptr, nullptr);
            }
        }
    }

#ifdef ENABLE_VIDEO
    // TODO. Move this elsewhere (when adding participant to conf?)
    if (!hasActiveVideo && !getConfId().empty()) {
        auto conference = Manager::instance().getConferenceFromID(getConfId());
        if (conference->isVideoEnabled())
            conference->attachVideo(getReceiveVideoFrameActiveWriter().get(), getCallId());
    }
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

    {
        std::lock_guard<std::mutex> lk(sinksMtx_);
        for (auto it = callSinksMap_.begin(); it != callSinksMap_.end();) {
            auto& videoReceive = videoRtp->getVideoReceive();
            if (videoReceive) {
                videoReceive->detach(it->second.get());
            }

            it->second->stop();
            it = callSinksMap_.erase(it);
        }
    }

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
    requestMediaChange(MediaAttribute::mediaAttributesToMediaMaps(mediaList));
}

void
SIPCall::updateMediaStream(const MediaAttribute& newMediaAttr, size_t streamIdx)
{
    assert(streamIdx < rtpStreams_.size());

    auto const& rtpStream = rtpStreams_[streamIdx];
    assert(rtpStream.rtpSession_);

    auto const& mediaAttr = rtpStream.mediaAttribute_;
    assert(mediaAttr);

    bool notify = false;

    if (newMediaAttr.muted_ == mediaAttr->muted_) {
        // Nothing to do. Already in the desired state.
        JAMI_DBG("[call:%s] [%s] already %s",
                 getCallId().c_str(),
                 mediaAttr->label_.c_str(),
                 mediaAttr->muted_ ? "muted " : "un-muted ");

    } else {
        notify = true;
    }

    // Update
    mediaAttr->muted_ = newMediaAttr.muted_;
    // Only update source and type if actually set.
    if (not newMediaAttr.sourceUri_.empty())
        mediaAttr->sourceUri_ = newMediaAttr.sourceUri_;
    if (newMediaAttr.sourceType_ != MediaSourceType::NONE)
        mediaAttr->sourceType_ = newMediaAttr.sourceType_;

    JAMI_DBG("[call:%s] %s [%s]",
             getCallId().c_str(),
             mediaAttr->muted_ ? "muting" : "un-muting",
             mediaAttr->label_.c_str());

    if (notify and mediaAttr->type_ == MediaType::MEDIA_AUDIO) {
        rtpStream.rtpSession_->setMuted(mediaAttr->muted_);
        setMute(mediaAttr->muted_);
        if (not isSubcall())
            emitSignal<DRing::CallSignal::AudioMuted>(getCallId(), mediaAttr->muted_);
        return;
    }

#ifdef ENABLE_VIDEO
    if (notify and mediaAttr->type_ == MediaType::MEDIA_VIDEO and not isSubcall()) {
        emitSignal<DRing::CallSignal::VideoMuted>(getCallId(), mediaAttr->muted_);
    }
#endif
}

void
SIPCall::updateAllMediaStreams(const std::vector<MediaAttribute>& mediaAttrList)
{
    JAMI_DBG("[call:%s] New local media", getCallId().c_str());

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

        if (streamIdx == rtpStreams_.size()) {
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

        if (streamIdx == rtpStreams_.size()) {
            // Always needs a re-invite when a new media is added.
            return true;
        }

        // Changing the source needs a re-invite
        if (newAttr.sourceUri_ != rtpStreams_[streamIdx].mediaAttribute_->sourceUri_) {
            return true;
        }

#ifdef ENABLE_VIDEO
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
SIPCall::requestMediaChange(const std::vector<DRing::MediaMap>& mediaList)
{
    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, isSrtpEnabled());

    // TODO. is the right place?
    // Disable video if disabled in the account.
    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("[call:%s] No account detected", getCallId().c_str());
        return false;
    }
    if (not account->isVideoEnabled()) {
        for (auto& mediaAttr : mediaAttrList) {
            if (mediaAttr.type_ == MediaType::MEDIA_VIDEO) {
                // This an API misuse. The new medialist should not contain video
                // if it was disabled in the account settings.
                JAMI_ERR("[call:%s] New media has video, but it's disabled in the account. "
                         "Ignoring the change request!",
                         getCallId().c_str());
                return false;
            }
        }
    }

    // If the peer does not support multi-stream and the size of the new
    // media list is different from the current media list, the media
    // change request will be ignored.
    if (not peerSupportMultiStream_ and rtpStreams_.size() != mediaAttrList.size()) {
        JAMI_WARN("[call:%s] Peer does not support multi-stream. Media change request ignored",
                  getCallId().c_str());
        return false;
    }

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
    std::vector<MediaAttribute> mediaList;
    mediaList.reserve(rtpStreams_.size());
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
SIPCall::onMediaNegotiationComplete()
{
    JAMI_WARN("[call:%s] Media negotiation complete", getCallId().c_str());

    // Main call (no subcalls) must wait for ICE now, the rest of code needs to access
    // to a negotiated transport.
    runOnMainThread([w = weak()] {
        if (auto this_ = w.lock()) {
            std::lock_guard<std::recursive_mutex> lk {this_->callMutex_};
            JAMI_WARN("[call:%s] media changed", this_->getCallId().c_str());
            // The call is already ended, so we don't need to restart medias
            if (not this_->inviteSession_
                or this_->inviteSession_->state == PJSIP_INV_STATE_DISCONNECTED
                or not this_->sdp_) {
                return;
            }

            bool hasIce = this_->isIceEnabled();
            if (hasIce) {
                // If ICE is not used, start medias now
                auto rem_ice_attrs = this_->sdp_->getIceAttributes();
                hasIce = not rem_ice_attrs.ufrag.empty() and not rem_ice_attrs.pwd.empty();
            }
            if (hasIce) {
                if (not this_->isSubcall()) {
                    // Start ICE checks. Media will be started once ICE checks complete.
                    this_->startIceMedia();
                }
            } else {
                // No ICE, start media now.
                JAMI_WARN("[call:%s] ICE media disabled, using default media ports",
                          this_->getCallId().c_str());
                // Update the negotiated media.
                this_->updateNegotiatedMedia();

                // Start the media.
                this_->stopAllMedia();
                this_->startAllMedia();
            }
        }
    });
}

void
SIPCall::startIceMedia()
{
    JAMI_DBG("[call:%s] Starting ICE", getCallId().c_str());
    auto mediaTransport = getIceMedia();

    if (not mediaTransport or mediaTransport->isFailed()) {
        JAMI_ERR("[call:%s] Media ICE init failed", getCallId().c_str());
        onFailure(EIO);
        return;
    }

    if (mediaTransport->isStarted()) {
        // NOTE: for incoming calls, the ice is already there and running
        if (mediaTransport->isRunning())
            onIceNegoSucceed();
        return;
    }

    if (not mediaTransport->isInitialized()) {
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
    if (not mediaTransport->startIce(rem_ice_attrs, getAllRemoteCandidates(*mediaTransport))) {
        JAMI_ERR("[call:%s] Media ICE start failed", getCallId().c_str());
        onFailure(EIO);
    }
}

void
SIPCall::onIceNegoSucceed()
{
    JAMI_DBG("[call:%s] ICE negotiation succeeded", getCallId().c_str());

    // Check if the call is already ended, so we don't need to restart medias
    // This is typically the case in a multi-device context where one device
    // can stop a call. So do not start medias
    if (not inviteSession_ or inviteSession_->state == PJSIP_INV_STATE_DISCONNECTED or not sdp_) {
        JAMI_ERR("[call:%s] ICE negotiation succeeded, but call is in invalid state",
                 getCallId().c_str());
        return;
    }

    // Update the negotiated media.
    updateNegotiatedMedia();

    // Nego succeed: move to the new media transport
    stopAllMedia();

    startAllMedia();
}

bool
SIPCall::checkMediaChangeRequest(const std::vector<DRing::MediaMap>& remoteMediaList)
{
    // The current media is considered to have changed if one of the
    // following condtions is true:
    //
    // - the number of media changed
    // - the type of one of the media changed (unlikely)
    // - one of the media was enabled/disabled

    JAMI_DBG("[call:%s] Received a media change request", getCallId().c_str());

    auto remoteMediaAtrrList = MediaAttribute::buildMediaAttributesList(remoteMediaList,
                                                                        isSrtpEnabled());
    if (remoteMediaAtrrList.size() != rtpStreams_.size())
        return true;

    for (size_t i = 0; i < rtpStreams_.size(); i++) {
        if (remoteMediaAtrrList[i].type_ != rtpStreams_[i].mediaAttribute_->type_)
            return true;
        if (remoteMediaAtrrList[i].enabled_ != rtpStreams_[i].mediaAttribute_->enabled_)
            return true;
    }

    return false;
}

void
SIPCall::handleMediaChangeRequest(const std::vector<DRing::MediaMap>& remoteMediaList)
{
    JAMI_DBG("[call:%s] Handling media change request", getCallId().c_str());

    auto account = getAccount().lock();
    if (not account) {
        JAMI_ERR("No account detected");
        return;
    }

    // If multi-stream is supported and the offered media differ from
    // the current media, the request is reported to the client to be
    // processed. Otherwise, we answer with the current local media.

    if (account->isMultiStreamEnabled() and checkMediaChangeRequest(remoteMediaList)) {
        // Report the media change request.
        emitSignal<DRing::CallSignal::MediaChangeRequested>(getAccountId(),
                                                            getCallId(),
                                                            remoteMediaList);
    } else {
        auto localMediaList = MediaAttribute::mediaAttributesToMediaMaps(getMediaAttributeList());
        answerMediaChangeRequest(localMediaList);
    }
}

pj_status_t
SIPCall::onReceiveReinvite(const pjmedia_sdp_session* offer, pjsip_rx_data* rdata)
{
    JAMI_DBG("[call:%s] Received a re-invite", getCallId().c_str());

    pj_status_t res = PJ_SUCCESS;

    if (not sdp_) {
        JAMI_ERR("SDP session is invalid");
        return res;
    }

    sdp_->clearIce();
    auto acc = getSIPAccount();
    if (not acc) {
        JAMI_ERR("No account detected");
        return res;
    }

    Sdp::printSession(offer, "Remote session (media change request)", SdpDirection::OFFER);

    sdp_->setReceivedOffer(offer);

    auto const& mediaAttrList = Sdp::getMediaAttributeListFromSdp(offer);
    if (mediaAttrList.empty()) {
        JAMI_WARN("[call:%s] Media list is empty, ignoring", getCallId().c_str());
        return res;
    }

    if (upnp_) {
        openPortsUPnP();
    }

    pjsip_tx_data* tdata = nullptr;
    if (pjsip_inv_initial_answer(inviteSession_.get(), rdata, PJSIP_SC_TRYING, NULL, NULL, &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not create answer TRYING", getCallId().c_str());
        return res;
    }

    // Report the change request.
    auto const& remoteMediaList = MediaAttribute::mediaAttributesToMediaMaps(mediaAttrList);

    if (auto conf = Manager::instance().getConferenceFromCallID(getCallId())) {
        conf->handleMediaChangeRequest(shared_from_this(), remoteMediaList);
    } else {
        handleMediaChangeRequest(remoteMediaList);
    }

    return res;
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

    sdp_->setReceivedOffer(offer);

    // Use current media list.
    sdp_->processIncomingOffer(getMediaAttributeList());

    if (isIceEnabled() and offer != nullptr) {
        setupIceResponse();
    }

    sdp_->startNegotiation();

    pjsip_tx_data* tdata = nullptr;

    if (pjsip_inv_initial_answer(inviteSession_.get(),
                                 const_cast<pjsip_rx_data*>(rdata),
                                 PJSIP_SC_OK,
                                 NULL,
                                 NULL,
                                 &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not create initial answer OK", getCallId().c_str());
        return !PJ_SUCCESS;
    }

    // Add user-agent header
    sip_utils::addUserAgentHeader(getSIPAccount()->getUserAgentName(), tdata);

    if (pjsip_inv_answer(inviteSession_.get(), PJSIP_SC_OK, NULL, sdp_->getLocalSdpSession(), &tdata)
        != PJ_SUCCESS) {
        JAMI_ERR("Could not create answer OK");
        return !PJ_SUCCESS;
    }

    if (contactHeader_.empty()) {
        JAMI_ERR("[call:%s] Contact header is empty!", getCallId().c_str());
        return !PJ_SUCCESS;
    }

    sip_utils::addContactHeader(contactHeader_, tdata);

    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not send msg OK", getCallId().c_str());
        return !PJ_SUCCESS;
    }

    if (upnp_) {
        openPortsUPnP();
    }

    return PJ_SUCCESS;
}

void
SIPCall::onReceiveOfferIn200OK(const pjmedia_sdp_session* offer)
{
    if (not rtpStreams_.empty()) {
        JAMI_ERR("[call:%s] Unexpected offer in '200 OK' answer", getCallId().c_str());
        return;
    }

    auto acc = getSIPAccount();
    if (not acc) {
        JAMI_ERR("No account detected");
        return;
    }

    if (not sdp_) {
        JAMI_ERR("invalid SDP session");
        return;
    }

    JAMI_DBG("[call:%s] Received an offer in '200 OK' answer", getCallId().c_str());

    auto mediaList = Sdp::getMediaAttributeListFromSdp(offer);
    // If this method is called, it means we are expecting an offer
    // in the 200OK answer.
    if (mediaList.empty()) {
        JAMI_WARN("[call:%s] Remote media list is empty, ignoring", getCallId().c_str());
        return;
    }

    Sdp::printSession(offer, "Remote session (offer in 200 OK answer)", SdpDirection::OFFER);
    sdp_->clearIce();
    sdp_->setReceivedOffer(offer);

    // If we send an empty offer, video will be accepted only if locally
    // enabled by the user.
    for (auto& mediaAttr : mediaList) {
        if (mediaAttr.type_ == MediaType::MEDIA_VIDEO and not acc->isVideoEnabled()) {
            mediaAttr.enabled_ = false;
        }
    }

    initMediaStreams(mediaList);

    sdp_->processIncomingOffer(mediaList);

    if (upnp_) {
        openPortsUPnP();
    }

    if (isIceEnabled()) {
        setupIceResponse();
    }

    sdp_->startNegotiation();

    if (pjsip_inv_set_sdp_answer(inviteSession_.get(), sdp_->getLocalSdpSession()) != PJ_SUCCESS) {
        JAMI_ERR("[call:%s] Could not start media negotiation for a re-invite request",
                 getCallId().c_str());
    }
}

void
SIPCall::openPortsUPnP()
{
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

    auto details = Call::getDetails();

    details.emplace(DRing::Call::Details::PEER_HOLDING, peerHolding_ ? TRUE_STR : FALSE_STR);

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
    JAMI_DBG("[call:%s] Entering conference [%s]", getCallId().c_str(), confId.c_str());
    confID_ = confId;

#ifdef ENABLE_VIDEO
    auto conf = Manager::instance().getConferenceFromID(confId);
    if (conf == nullptr) {
        JAMI_ERR("[call:%s] Unknown conference [%s]", getCallId().c_str(), confId.c_str());
        return;
    }

    if (conf->isVideoEnabled()) {
        auto videoRtp = getVideoRtp();
        if (not videoRtp) {
            JAMI_ERR("[call:%s] Failed to get a valid video RTP session", getCallId().c_str());
            throw std::runtime_error("Failed to get a valid video RTP session");
        }
        videoRtp->enterConference(conf.get());
    }

#endif

#ifdef ENABLE_PLUGIN
    clearCallAVStreams();
#endif
}

void
SIPCall::exitConference()
{
    auto confId = getConfId();
    if (not confId.empty()) {
        JAMI_DBG("[call:%s] Leaving conference [%s]", getCallId().c_str(), confId.c_str());
    } else {
        JAMI_ERR("[call:%s] The call is not bound to any conference", getCallId().c_str());
        return;
    }

    auto const& audioRtp = getAudioRtp();
    if (audioRtp && !isCaptureDeviceMuted(MediaType::MEDIA_AUDIO)) {
        auto& rbPool = Manager::instance().getRingBufferPool();
        rbPool.bindCallID(getCallId(), RingBufferPool::DEFAULT_ID);
        rbPool.flush(RingBufferPool::DEFAULT_ID);
    }
#ifdef ENABLE_VIDEO
    auto const& videoRtp = getVideoRtp();
    if (videoRtp)
        videoRtp->exitConference();
#endif
#ifdef ENABLE_PLUGIN
    createCallAVStreams();
#endif
    confID_ = "";
}

std::shared_ptr<Observable<std::shared_ptr<MediaFrame>>>
SIPCall::getReceiveVideoFrameActiveWriter()
{
#ifdef ENABLE_VIDEO
    auto videoRtp = getVideoRtp();
    if (videoRtp)
        return videoRtp->getReceiveVideoFrameActiveWriter();
#endif

    return {};
}

bool
SIPCall::addDummyVideoRtpSession()
{
#ifdef ENABLE_VIDEO
    JAMI_DBG("[call:%s] Add dummy video stream", getCallId().c_str());

    MediaAttribute mediaAttr(MediaType::MEDIA_VIDEO,
                             true,
                             true,
                             false,
                             "dummy source",
                             DUMMY_VIDEO_STR);

    addMediaStream(mediaAttr);
    auto& stream = rtpStreams_.back();
    createRtpSession(stream);
    return stream.rtpSession_ != nullptr;
#endif

    return false;
}

void
SIPCall::removeDummyVideoRtpSessions()
{
    // It's not expected to have more than one dummy video stream, but
    // check just in case.
    auto removed = std::remove_if(rtpStreams_.begin(),
                                  rtpStreams_.end(),
                                  [](const RtpStream& stream) {
                                      return stream.mediaAttribute_->label_ == DUMMY_VIDEO_STR;
                                  });
    auto count = std::distance(removed, rtpStreams_.end());
    rtpStreams_.erase(removed, rtpStreams_.end());

    if (count > 0) {
        JAMI_DBG("[call:%s] Removed %lu dummy video stream(s)", getCallId().c_str(), count);
        if (count > 1) {
            JAMI_WARN("[call:%s] Expected to find 1 dummy video stream, found %lu",
                      getCallId().c_str(),
                      count);
        }
    }
}

void
SIPCall::createSinks(const ConfInfo& infos)
{
#ifdef ENABLE_VIDEO
    if (!hasVideo())
        return;

    std::lock_guard<std::mutex> lk(sinksMtx_);
    auto videoRtp = getVideoRtp();
    auto& videoReceive = videoRtp->getVideoReceive();
    if (!videoReceive)
        return;
    auto id = getConfId().empty() ? getCallId() : getConfId();
    Manager::instance().createSinkClients(id,
                                          infos,
                                          std::static_pointer_cast<video::VideoGenerator>(
                                              videoReceive),
                                          callSinksMap_);
#endif
}

std::shared_ptr<AudioRtpSession>
SIPCall::getAudioRtp() const
{
    // For the moment, the clients support only one audio stream, so we
    // return the first audio stream.
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

std::vector<std::shared_ptr<RtpSession>>
SIPCall::getRtpSessionList() const
{
    std::vector<std::shared_ptr<RtpSession>> rtpList;
    rtpList.reserve(rtpStreams_.size());
    for (auto const& stream : rtpStreams_) {
        rtpList.emplace_back(stream.rtpSession_);
    }
    return rtpList;
}

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
    JAMI_DBG("\t- Duration: %s", dht::print_duration(getCallDuration()).c_str());
    for (const auto& stream : rtpStreams_)
        JAMI_DBG("\t- Media: %s", stream.mediaAttribute_->toString(true).c_str());
#ifdef ENABLE_VIDEO
    if (auto codec = getVideoCodec())
        JAMI_DBG("\t- Video codec: %s", codec->systemCodecInfo.name.c_str());
#endif
    if (auto transport = getIceMedia()) {
        if (transport->isRunning())
            JAMI_DBG("\t- Medias: %s", transport->link().c_str());
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

void
SIPCall::createIceMediaTransport()
{
    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    std::lock_guard<std::mutex> lk(transportMtx_);
    resetTransport(std::move(mediaTransport_));
    mediaTransport_ = iceTransportFactory.createTransport(getCallId().c_str());
    if (mediaTransport_) {
        JAMI_DBG("[call:%s] Successfully created media ICE transport [ice:%p]",
                 getCallId().c_str(),
                 mediaTransport_.get());
    } else {
        JAMI_ERR("[call:%s] Failed to create media ICE transport", getCallId().c_str());
    }
}

bool
SIPCall::initIceMediaTransport(bool master, std::optional<IceTransportOptions> options)
{
    auto acc = getSIPAccount();
    if (!acc) {
        JAMI_ERR("No account detected");
        return false;
    }

    JAMI_DBG("[call:%s] Init media ICE transport", getCallId().c_str());
    auto mediaTransport = getIceMedia();
    if (not mediaTransport) {
        JAMI_ERR("[call:%s] Failed to create media ICE transport", getCallId().c_str());
        return false;
    }

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

    iceOptions.master = master;
    iceOptions.streamsCount = static_cast<unsigned>(rtpStreams_.size());
    // Each RTP stream requires a pair of ICE components (RTP + RTCP).
    iceOptions.compCountPerStream = ICE_COMP_COUNT_PER_STREAM;

    // Init ICE.
    mediaTransport->initIceInstance(iceOptions);

    return true;
}

std::vector<std::string>
SIPCall::getLocalIceCandidates(unsigned compId) const
{
    std::lock_guard<std::mutex> lk(transportMtx_);
    if (not mediaTransport_) {
        JAMI_WARN("[call:%s] no media ICE transport", getCallId().c_str());
        return {};
    }
    return mediaTransport_->getLocalCandidates(compId);
}

void
SIPCall::resetTransport(std::shared_ptr<IceTransport>&& transport)
{
    // Move the transport to another thread and destroy it there if possible
    if (transport) {
        dht::ThreadPool::io().run(
            [transport = std::move(transport)]() mutable { transport.reset(); });
    }
}

void
SIPCall::merge(Call& call)
{
    JAMI_DBG("[call:%s] merge subcall %s", getCallId().c_str(), call.getCallId().c_str());

    // This static cast is safe as this method is private and overload Call::merge
    auto& subcall = static_cast<SIPCall&>(call);

    std::lock(callMutex_, subcall.callMutex_);
    std::lock_guard<std::recursive_mutex> lk1 {callMutex_, std::adopt_lock};
    std::lock_guard<std::recursive_mutex> lk2 {subcall.callMutex_, std::adopt_lock};
    inviteSession_ = std::move(subcall.inviteSession_);
    if (inviteSession_)
        inviteSession_->mod_data[Manager::instance().sipVoIPLink().getModId()] = this;
    setSipTransport(std::move(subcall.sipTransport_));
    sdp_ = std::move(subcall.sdp_);
    peerHolding_ = subcall.peerHolding_;
    upnp_ = std::move(subcall.upnp_);
    contactHeader_ = std::move(subcall.contactHeader_);
    localAudioPort_ = subcall.localAudioPort_;
    localVideoPort_ = subcall.localVideoPort_;
    peerUserAgent_ = subcall.peerUserAgent_;
    peerSupportMultiStream_ = subcall.peerSupportMultiStream_;
    peerAllowedMethods_ = subcall.peerAllowedMethods_;

    Call::merge(subcall);
    if (isIceEnabled())
        startIceMedia();
}

bool
SIPCall::remoteHasValidIceAttributes()
{
    if (not sdp_) {
        throw std::runtime_error("Must have a valid SDP Session");
    }

    auto rem_ice_attrs = sdp_->getIceAttributes();
    if (rem_ice_attrs.ufrag.empty()) {
        JAMI_WARN("[call:%s] Missing ICE username fragment attribute in remote SDP",
                  getCallId().c_str());
        return false;
    }

    if (rem_ice_attrs.pwd.empty()) {
        JAMI_WARN("[call:%s] Missing ICE password attribute in remote SDP", getCallId().c_str());
        return false;
    }

    return true;
}

void
SIPCall::setIceMedia(std::shared_ptr<IceTransport> ice)
{
    JAMI_DBG("[call:%s] Setting ICE session [%p]", getCallId().c_str(), ice.get());

    std::lock_guard<std::mutex> lk(transportMtx_);
    if (not isSubcall()) {
        JAMI_ERR("[call:%s] The call is expected to be a sub-call", getCallId().c_str());
    }
    mediaTransport_ = std::move(ice);
}

void
SIPCall::setupIceResponse()
{
    JAMI_DBG("[call:%s] Setup ICE response", getCallId().c_str());

    auto account = getSIPAccount();
    if (not account) {
        JAMI_ERR("No account detected");
    }

    if (not remoteHasValidIceAttributes()) {
        // If ICE attributes are not present, skip the ICE initialization
        // step (most likely peer does not support/enable ICE).
        JAMI_WARN("[call:%s] no ICE data in remote SDP", getCallId().c_str());
        return;
    }

    auto opt = account->getIceOptions();

    // Try to use the discovered public address. If not available,
    // fallback on local address.
    opt.accountPublicAddr = account->getPublishedIpAddress();
    if (opt.accountLocalAddr) {
        opt.accountLocalAddr = ip_utils::getInterfaceAddr(account->getLocalInterface(),
                                                          opt.accountPublicAddr.getFamily());
    } else {
        // Just set the local address for both, most likely the account is not
        // registered.
        opt.accountLocalAddr = ip_utils::getInterfaceAddr(account->getLocalInterface(), AF_INET);
        opt.accountPublicAddr = opt.accountLocalAddr;
    }

    if (not opt.accountLocalAddr) {
        JAMI_ERR("[call:%s] No local address, ICE can't be initialized", getCallId().c_str());
        onFailure(EIO);
        return;
    }
    createIceMediaTransport();
    if (not initIceMediaTransport(false, opt)) {
        JAMI_ERR("[call:%s] ICE initialization failed", getCallId().c_str());
        // Fatal condition
        // TODO: what's SIP rfc says about that?
        // (same question in startIceMedia)
        onFailure(EIO);
        return;
    }

    // WARNING: This call blocks! (need ice init done)
    addLocalIceAttributes();
}

bool
SIPCall::isIceRunning() const
{
    std::lock_guard<std::mutex> lk(transportMtx_);
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
