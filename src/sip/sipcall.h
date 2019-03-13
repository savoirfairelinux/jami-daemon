/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "call.h"
#include "sip_utils.h"

#ifdef RING_VIDEO
#include "media/video/video_rtp_session.h"
#endif

#include "noncopyable.h"

#include "pjsip/sip_config.h"

#include <memory>

struct pjsip_evsub;
struct pjsip_inv_session;
struct pjmedia_sdp_session;
struct pj_ice_sess_cand;

namespace ring {

class Sdp;
class SIPAccountBase;
class SipTransport;
class AudioRtpSession;
class IceTransport;
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
public:
    static const char* const LINK_TYPE;

    /**
     * Destructor
     */
    ~SIPCall();

protected:
    /**
     * Constructor (protected)
     * @param id    The call identifier
     * @param type  The type of the call. Could be Incoming or Outgoing
     */
    SIPCall(SIPAccountBase& account, const std::string& id, Call::CallType type,
            const std::map<std::string, std::string>& details={});

public: // overridden
    const char* getLinkType() const override {
        return LINK_TYPE;
    }
    void answer() override;
    void hangup(int reason) override;
    void refuse() override;
    void transfer(const std::string& to) override;
    bool attendedTransfer(const std::string& to) override;
    bool onhold() override;
    bool offhold() override;
    void switchInput(const std::string& resource) override;
    void peerHungup() override;
    void carryingDTMFdigits(char code) override;

    /**
      * Send device orientation through SIP INFO
      * @param rotation Device orientation (0/90/180/270) (counterclockwise)
      */
    void setVideoOrientation(int rotation);

    void sendTextMessage(const std::map<std::string, std::string>& messages,
                         const std::string& from) override;
    void removeCall() override;
    void muteMedia(const std::string& mediaType, bool isMuted) override;
    void restartMediaSender() override;
    bool useVideoCodec(const AccountVideoCodecInfo* codec) const override;
    void sendKeyframe() override;
    std::map<std::string, std::string> getDetails() const override;

    virtual bool toggleRecording() override; // SIPCall needs to spread recorder to rtp sessions, so override

public: // SIP related
    /**
     * Return the SDP's manager of this call
     */
    Sdp& getSDP() {
        return *sdp_;
    }

    /**
     * Tell the user that the call is ringing
     * @param
     */
    void onPeerRinging();

    /**
     * Tell the user that the call was answered
     * @param
     */
    void onAnswered();

    /**
     * To call in case of server/internal error
     * @param cause Optionnal error code
     */
    void onFailure(signed cause=0);

    /**
     * Peer answered busy
     * @param
     */
    void onBusyHere();

    /**
     * Peer close the connection
     * @param
     */
    void onClosed();

    void onReceiveOffer(const pjmedia_sdp_session *offer);

    void onMediaUpdate();

    void setContactHeader(pj_str_t *contact);

    void setTransport(const std::shared_ptr<SipTransport>& t);

    SipTransport* getTransport() {
        return transport_.get();
    }

    void sendSIPInfo(const char *const body, const char *const subtype);

    void requestKeyframe();

    SIPAccountBase& getSIPAccount() const;

    void updateSDPFromSTUN();

    void setupLocalSDPFromIce();

    /**
     * Give peer SDP to the call for handling
     * @param sdp pointer on PJSIP sdp structure, could be nullptr (acts as no-op in such case)
     */
    void setRemoteSdp(const pjmedia_sdp_session* sdp);

    void terminateSipSession(int status);

    /**
     * The invite session to be reused in case of transfer
     */
    struct InvSessionDeleter {
        void operator()(pjsip_inv_session*) const noexcept;
    };

    std::unique_ptr<pjsip_inv_session, InvSessionDeleter> inv;

public: // NOT SIP RELATED (good candidates to be moved elsewhere)
    /**
     * Returns a pointer to the AudioRtpSession object
     */
    AudioRtpSession& getAVFormatRTP() const {
        return *avformatrtp_;
    }

#ifdef RING_VIDEO
    /**
     * Returns a pointer to the VideoRtp object
     */
    video::VideoRtpSession& getVideoRtp () {
        return *videortp_;
    }
#endif

    void setSecure(bool sec);

    bool isSecure() const {
        return srtpEnabled_;
    }

    void generateMediaPorts();

    void startAllMedia();

    void openPortsUPnP();

    void setPeerRegistredName(const std::string& name) {
        peerRegistredName_ = name;
    }

    void setPeerUri(const std::string peerUri) { peerUri_ = peerUri; }

    bool initIceMediaTransport(bool master, unsigned channel_num=4);

    bool isIceRunning() const;

    std::unique_ptr<IceSocket> newIceSocket(unsigned compId);

    IceTransport* getIceMediaTransport() const {
        return tmpMediaTransport_ ? tmpMediaTransport_.get() : mediaTransport_.get();
    }

private:
    NON_COPYABLE(SIPCall);

    void setCallMediaLocal();

    void waitForIceAndStartMedia();

    void stopAllMedia();

    /**
     * Transfer method used for both type of transfer
     */
    bool transferCommon(pj_str_t *dst);

    bool internalOffHold(const std::function<void()> &SDPUpdateFunc);

    int SIPSessionReinvite();

    std::vector<IceCandidate> getAllRemoteCandidates();

    void merge(Call& call) override; // not public - only called by Call

    inline std::shared_ptr<const SIPCall> shared() const {
        return std::static_pointer_cast<const SIPCall>(shared_from_this());
    }
    inline std::shared_ptr<SIPCall> shared() {
        return std::static_pointer_cast<SIPCall>(shared_from_this());
    }
    inline std::weak_ptr<const SIPCall> weak() const {
        return std::weak_ptr<const SIPCall>(shared());
    }
    inline std::weak_ptr<SIPCall> weak() {
        return std::weak_ptr<SIPCall>(shared());
    }

    std::unique_ptr<AudioRtpSession> avformatrtp_;

#ifdef RING_VIDEO
    /**
     * Video Rtp Session factory
     */
    std::unique_ptr<video::VideoRtpSession> videortp_;
#endif

    std::string mediaInput_;

    bool srtpEnabled_ {false};

    /**
     * Hold the transport used for SIP communication.
     * Will be different from the account registration transport for
     * non-IP2IP calls.
     */
    std::shared_ptr<SipTransport> transport_ {};

    /**
     * The SDP session
     */
    std::unique_ptr<Sdp> sdp_;
    bool peerHolding_ {false};

    bool isWaitingForIceAndMedia_ {false};
    enum class Request {
        HoldingOn,
        HoldingOff,
        SwitchInput,
        NoRequest
    };
    Request remainingRequest_ {Request::NoRequest};

    std::string peerRegistredName_ {};

    char contactBuffer_[PJSIP_MAX_URL_SIZE] {};
    pj_str_t contactHeader_ {contactBuffer_, 0};

    std::unique_ptr<ring::upnp::Controller> upnp_;

    /** Local audio port, as seen by me. */
    unsigned int localAudioPort_ {0};

    /** Local video port, as seen by me. */
    unsigned int localVideoPort_ {0};

    ///< Transport used for media streams
    std::shared_ptr<IceTransport> mediaTransport_;

    ///< Temporary transport for media. Replace mediaTransport_ when connected with success
    std::shared_ptr<IceTransport> tmpMediaTransport_;

    std::string peerUri_{};
};

// Helpers

/**
 * Obtain a shared smart pointer of instance
 */
inline std::shared_ptr<SIPCall> getPtr(SIPCall& call)
{
    return std::static_pointer_cast<SIPCall>(call.shared_from_this());
}

} // namespace ring
