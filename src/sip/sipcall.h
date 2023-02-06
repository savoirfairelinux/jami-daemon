/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "call.h"
#include "connectivity/ice_transport.h"
#include "media_codec.h" // for MediaType enum
#include "connectivity/sip_utils.h"
#include "sip/sdp.h"

#include "media/rtp_session.h"
#ifdef ENABLE_VIDEO
#include "media/video/video_receive_thread.h"
#include "media/video/video_rtp_session.h"
#endif
#ifdef ENABLE_PLUGIN
#include "plugin/streamdata.h"
#endif
#include "noncopyable.h"

#include <memory>
#include <optional>

extern "C" {
#include <pjsip/sip_config.h>
struct pjsip_evsub;
struct pjsip_inv_session;
struct pjmedia_sdp_session;
struct pj_ice_sess_cand;
struct pjsip_rx_data;
}

namespace jami {

class Sdp;
class SIPAccountBase;
class SipTransport;
class AudioRtpSession;
class IceSocket;

using IceCandidate = pj_ice_sess_cand;

namespace upnp {
class Controller;
}

/**
 * @file sipcall.h
 * @brief SIPCall are SIP implementation of a normal Call
 */
class SIPCall : public Call
{
private:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    NON_COPYABLE(SIPCall);

public:
    static constexpr LinkType LINK_TYPE = LinkType::SIP;

    struct RtpStream
    {
        std::shared_ptr<RtpSession> rtpSession_ {};
        std::shared_ptr<MediaAttribute> mediaAttribute_ {};
        std::shared_ptr<MediaAttribute> remoteMediaAttribute_;
        std::unique_ptr<IceSocket> rtpSocket_;
        std::unique_ptr<IceSocket> rtcpSocket_;
    };

    /**
     * Destructor
     */
    ~SIPCall();

    /**
     * Constructor
     * @param id The call identifier
     * @param type The type of the call (incoming/outgoing)
     * @param mediaList A list of medias to include in the call
     */
    SIPCall(const std::shared_ptr<SIPAccountBase>& account,
            const std::string& id,
            Call::CallType type,
            const std::vector<libjami::MediaMap>& mediaList);

    // Inherited from Call class
    LinkType getLinkType() const override { return LINK_TYPE; }

    // Override of Call class
private:
    void merge(Call& call) override; // not public - only called by Call

public:
    void answer() override;
    void answer(const std::vector<libjami::MediaMap>& mediaList) override;
    bool checkMediaChangeRequest(const std::vector<libjami::MediaMap>& remoteMediaList) override;
    void handleMediaChangeRequest(const std::vector<libjami::MediaMap>& remoteMediaList) override;
    void answerMediaChangeRequest(const std::vector<libjami::MediaMap>& mediaList,
                                  bool isRemote = false) override;
    void hangup(int reason) override;
    void refuse() override;
    void transfer(const std::string& to) override;
    bool attendedTransfer(const std::string& to) override;
    bool onhold(OnReadyCb&& cb) override;
    bool offhold(OnReadyCb&& cb) override;
    void switchInput(const std::string& resource = {}) override;
    void peerHungup() override;
    void carryingDTMFdigits(char code) override;
    bool requestMediaChange(const std::vector<libjami::MediaMap>& mediaList) override;
    std::vector<libjami::MediaMap> currentMediaList() const override;
    void sendTextMessage(const std::map<std::string, std::string>& messages,
                         const std::string& from) override;
    void removeCall() override;
    void muteMedia(const std::string& mediaType, bool isMuted) override;
    std::vector<MediaAttribute> getMediaAttributeList() const override;
    void restartMediaSender() override;
    std::shared_ptr<AccountCodecInfo> getAudioCodec() const override;
    std::shared_ptr<AccountCodecInfo> getVideoCodec() const override;
    void sendKeyframe(int streamIdx = -1) override;
    bool isIceEnabled() const override;
    std::map<std::string, std::string> getDetails() const override;
    void enterConference(std::shared_ptr<Conference> conference) override;
    void exitConference() override;
#ifdef ENABLE_VIDEO
    std::mutex sinksMtx_;
    void createSinks(const ConfInfo& infos) override;
    std::map<std::string, std::shared_ptr<video::SinkClient>> callSinksMap_ {};
#endif
    bool hasVideo() const override;

    // TODO: cleanup this (used by conference + Call::getDetails() (and clients can use this))
    bool isCaptureDeviceMuted(const MediaType& mediaType) const override;
    bool isSrtpEnabled() const
    {
        return srtpEnabled_;
    }
    // End of override of Call class

    // Override of Recordable class
    bool toggleRecording() override;
    // End of override of Recordable class

    // Override PeerRecorder
    void peerRecording(bool state) override;
    void peerMuted(bool state) override;
    void peerVoice(bool state) override;
    // end override PeerRecorder

    void monitor() const override;

    /**
     * Set peer's User-Agent found in the message header
     */
    void setPeerUaVersion(std::string_view ua);

    /**
     * Set peer's allowed methods
     */
    void setPeerAllowMethods(std::vector<std::string> methods);

    /**
     * Check if a SIP method is allowed by peer
     */
    bool isSipMethodAllowedByPeer(const std::string_view method) const;

    /**
     * Return the SDP's manager of this call
     */
    Sdp& getSDP()
    {
        return *sdp_;
    }

    // Implementation of events reported by SipVoipLink.
    /**
     * Call is in ringing state on peer's side
     */
    void onPeerRinging();
    /**
     * Peer answered the call
     */
    void onAnswered();
    /**
     * Called to report server/internal errors
     * @param cause Optional error code
     */
    void onFailure(signed cause = 0);
    /**
     * Peer answered busy
     */
    void onBusyHere();
    /**
     * Peer closed the connection
     */
    void onClosed();

    pj_status_t onReceiveReinvite(const pjmedia_sdp_session* offer, pjsip_rx_data* rdata);
    void onReceiveOfferIn200OK(const pjmedia_sdp_session* offer);

    /**
     * Called when the media negotiation (SDP offer/answer) has
     * completed.
     */
    void onMediaNegotiationComplete();
    // End fo SiPVoipLink events

    const std::string& getContactHeader() const;

    void setSipTransport(const std::shared_ptr<SipTransport>& transport,
                         const std::string& contactHdr = {});

    SipTransport* getTransport()
    {
        return sipTransport_.get();
    }

    void sendSIPInfo(std::string_view body, std::string_view subtype);

    void requestKeyframe(int streamIdx = -1);

    void updateRecState(bool state) override;

    std::shared_ptr<SIPAccountBase> getSIPAccount() const;

    bool remoteHasValidIceAttributes() const;
    void addLocalIceAttributes();

    std::shared_ptr<IceTransport> getIceMedia() const
    {
        std::lock_guard<std::mutex> lk(transportMtx_);
        return reinvIceMedia_ ? reinvIceMedia_ : iceMedia_;
    };

    // Set ICE instance. Must be called only for sub-calls
    void setIceMedia(std::shared_ptr<IceTransport> ice, bool isReinvite = false);

    // Switch to re-invite ICE media if needed
    void switchToIceReinviteIfNeeded();

    /**
     * Setup ICE locally to answer to an ICE offer. The ICE session has
     * the controlled role (slave)
     */
    void setupIceResponse(bool isReinvite = false);

    void terminateSipSession(int status);

    /**
     * The invite session to be reused in case of transfer
     */
    struct InvSessionDeleter
    {
        void operator()(pjsip_inv_session*) const noexcept;
    };

#ifdef ENABLE_VIDEO
    void setRotation(int streamIdx, int rotation);
#endif
    // Get the list of current RTP sessions
    std::vector<std::shared_ptr<RtpSession>> getRtpSessionList(
        MediaType type = MediaType::MEDIA_ALL) const;
    static size_t getActiveMediaStreamCount(const std::vector<MediaAttribute>& mediaAttrList);

    void setPeerRegisteredName(const std::string& name)
    {
        peerRegisteredName_ = name;
    }

    void setPeerUri(const std::string& peerUri)
    {
        peerUri_ = peerUri;
    }

    // Create a new ICE media session. If we already have an instance,
    // it will be destroyed first.
    bool createIceMediaTransport(bool isReinvite);

    // Initialize the ICE session.
    // The initialization is performed asynchronously, i.e, the instance
    // may not be ready to use when this method returns.
    bool initIceMediaTransport(bool master,
                               std::optional<IceTransportOptions> options = std::nullopt);

    std::vector<std::string> getLocalIceCandidates(unsigned compId) const;

    void setInviteSession(pjsip_inv_session* inviteSession = nullptr);

    std::unique_ptr<pjsip_inv_session, InvSessionDeleter> inviteSession_;

    inline std::weak_ptr<const SIPCall> weak() const
    {
        return std::weak_ptr<const SIPCall>(shared());
    }
    inline std::weak_ptr<SIPCall> weak()
    {
        return std::weak_ptr<SIPCall>(shared());
    }
    /**
     * Announce to the client that medias are successfully negotiated
     */
    void reportMediaNegotiationStatus();

private:
    void generateMediaPorts();

    void openPortsUPnP();

    bool isIceRunning() const;

    std::unique_ptr<IceSocket> newIceSocket(unsigned compId);

    void deinitRecorder();

    void rtpSetupSuccess(MediaType type, bool isRemote);

    void setupVoiceCallback(const std::shared_ptr<RtpSession>& rtpSession);

    void sendMuteState(bool state);
    void sendVoiceActivity(std::string_view streamId, bool state);

    void resetTransport(std::shared_ptr<IceTransport>&& transport);

    /**
     * Send device orientation through SIP INFO
     * @param streamIdx  The stream to rotate
     * @param rotation   Device orientation (0/90/180/270) (counterclockwise)
     */
    void setVideoOrientation(int streamIdx, int rotation);

    mutable std::mutex transportMtx_ {};

#ifdef ENABLE_PLUGIN
    /**
     * Call Streams and some typedefs
     */
    using AVMediaStream = Observable<std::shared_ptr<MediaFrame>>;
    using MediaStreamSubject = PublishMapSubject<std::shared_ptr<MediaFrame>, AVFrame*>;

    /**
     * @brief createCallAVStream
     * Creates a call AV stream like video input, video receive, audio input or audio receive
     * @param StreamData The type of the stream (audio/video, input/output,
     * @param streamSource
     * @param mediaStreamSubject
     */
    void createCallAVStream(const StreamData& StreamData,
                            AVMediaStream& streamSource,
                            const std::shared_ptr<MediaStreamSubject>& mediaStreamSubject);
    /**
     * @brief createCallAVStreams
     * Creates all Call AV Streams (2 if audio, 4 if audio video)
     */
    void createCallAVStreams();

    /**
     * @brief Detach all plugins from call streams;
     */
    void clearCallAVStreams();

    std::mutex avStreamsMtx_ {};
    std::map<std::string, std::shared_ptr<MediaStreamSubject>> callAVStreams;
#endif // ENABLE_PLUGIN

    void setCallMediaLocal();
    void startIceMedia();
    void onIceNegoSucceed();
    void setupNegotiatedMedia();
    void startAllMedia();
    void stopAllMedia();
    void updateRemoteMedia();

    /**
     * Transfer method used for both type of transfer
     */
    bool transferCommon(const pj_str_t* dst);

    bool internalOffHold(const std::function<void()>& SDPUpdateFunc);

    bool hold();

    bool unhold();

    // Update the attributes of a media stream
    void updateMediaStream(const MediaAttribute& newMediaAttr, size_t streamIdx);
    bool updateAllMediaStreams(const std::vector<MediaAttribute>& mediaAttrList, bool isRemote);
    // Check if a SIP re-invite must be sent to negotiate the new media
    bool isReinviteRequired(const std::vector<MediaAttribute>& mediaAttrList);
    // Check if a new ICE media session is needed when performing a re-invite
    bool isNewIceMediaRequired(const std::vector<MediaAttribute>& mediaAttrList);
    void requestReinvite(const std::vector<MediaAttribute>& mediaAttrList, bool needNewIce);
    int SIPSessionReinvite(const std::vector<MediaAttribute>& mediaAttrList, bool needNewIce);
    int SIPSessionReinvite();
    // Add a media stream to the call.
    void addMediaStream(const MediaAttribute& mediaAttr);
    // Init media streams
    size_t initMediaStreams(const std::vector<MediaAttribute>& mediaAttrList);
    // Create a new stream from SDP description.
    void createRtpSession(RtpStream& rtpStream);
    // Configure the RTP session from SDP description.
    void configureRtpSession(const std::shared_ptr<RtpSession>& rtpSession,
                             const std::shared_ptr<MediaAttribute>& mediaAttr,
                             const MediaDescription& localMedia,
                             const MediaDescription& remoteMedia);
    // Find the stream index with the matching label
    int findRtpStreamIndex(const std::string& label) const;

    std::vector<IceCandidate> getAllRemoteCandidates(IceTransport& transport) const;

    inline std::shared_ptr<const SIPCall> shared() const
    {
        return std::static_pointer_cast<const SIPCall>(shared_from_this());
    }
    inline std::shared_ptr<SIPCall> shared()
    {
        return std::static_pointer_cast<SIPCall>(shared_from_this());
    }

    // Peer's User-Agent.
    std::string peerUserAgent_ {};
    // Flag to indicate if the peer's Daemon version supports multi-stream.
    bool peerSupportMultiStream_ {false};
    // Flag to indicate if the peer's Daemon version can negotiate more than 2 ICE medias
    bool peerSupportMultiIce_ {false};

    // Flag to indicate if the peer's Daemon version supports re-invite
    // without ICE renegotiation.
    bool peerSupportReuseIceInReinv_ {false};

    // Peer's allowed methods.
    std::vector<std::string> peerAllowedMethods_;

    // Vector holding the current RTP sessions.
    std::vector<RtpStream> rtpStreams_;

    /**
     * Hold the transport used for SIP communication.
     * Will be different from the account registration transport for
     * non-IP2IP calls.
     */
    std::shared_ptr<SipTransport> sipTransport_ {};

    /**
     * The SDP session
     */
    std::unique_ptr<Sdp> sdp_ {};
    bool peerHolding_ {false};

    bool isWaitingForIceAndMedia_ {false};
    enum class Request { HoldingOn, HoldingOff, SwitchInput, NoRequest };
    Request remainingRequest_ {Request::NoRequest};

    std::string peerRegisteredName_ {};

    std::string contactHeader_ {};

    std::shared_ptr<jami::upnp::Controller> upnp_;

    /** Local audio port, as seen by me. */
    unsigned int localAudioPort_ {0};
    /** Local video port, as seen by me. */
    unsigned int localVideoPort_ {0};

    bool mediaRestartRequired_ {true};
    bool enableIce_ {true};
    bool srtpEnabled_ {false};
    bool rtcpMuxEnabled_ {false};

    // ICE media transport
    std::shared_ptr<IceTransport> iceMedia_;
    // Re-invite (temporary) ICE media transport.
    std::shared_ptr<IceTransport> reinvIceMedia_;

    std::string peerUri_ {};

    bool readyToRecord_ {false};
    bool pendingRecord_ {false};

    time_point lastKeyFrameReq_ {time_point::min()};

    OnReadyCb holdCb_ {};
    OnReadyCb offHoldCb_ {};

    std::atomic_bool waitForIceInit_ {false};

    std::map<const std::string, bool> mediaReady_ {{"a:local", false},
                                                   {"a:remote", false},
                                                   {"v:local", false},
                                                   {"v:remote", false}};

    void resetMediaReady();
    void detachAudioFromConference();

    std::mutex setupSuccessMutex_;
#ifdef ENABLE_VIDEO
    int rotation_ {0};
#endif
};

// Helpers

/**
 * Obtain a shared smart pointer of instance
 */
inline std::shared_ptr<SIPCall>
getPtr(SIPCall& call)
{
    return std::static_pointer_cast<SIPCall>(call.shared_from_this());
}

} // namespace jami
