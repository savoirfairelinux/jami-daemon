/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#ifndef __SIPCALL_H__
#define __SIPCALL_H__

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

namespace ring {

class Sdp;
class SIPAccountBase;
class SipTransport;
class AudioRtpSession;

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

    protected:
        /**
         * Constructor (protected)
         * @param id	The call identifier
         * @param type  The type of the call. Could be Incoming
         *						 Outgoing
         */
        SIPCall(SIPAccountBase& account, const std::string& id, Call::CallType type);

    public:
        /**
         * Destructor
         */
        ~SIPCall();

        /**
         * Return the SDP's manager of this call
         */
        Sdp& getSDP() {
            return *sdp_;
        }

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
            return videortp_;
        }
#endif

        /**
         * The invite session to be reused in case of transfer
         */
        struct InvSessionDeleter {
                void operator()(pjsip_inv_session*) const noexcept;
        };

        std::unique_ptr<pjsip_inv_session, InvSessionDeleter> inv;

        void setSecure(bool sec);

        bool isSecure() const {
            return srtpEnabled_;
        }

        void setCallMediaLocal(const pj_sockaddr& localIP);

        void generateMediaPorts();

        void setContactHeader(pj_str_t *contact);

        void setTransport(const std::shared_ptr<SipTransport>& t);

        inline const std::shared_ptr<SipTransport>& getTransport() {
            return transport_;
        }

        void sendSIPInfo(const char *const body, const char *const subtype);

        void answer();

        void hangup(int reason);

        void refuse();

        void transfer(const std::string& to);

        bool attendedTransfer(const std::string& to);

        void onhold();

        void offhold();

        void switchInput(const std::string& resource);

        void peerHungup();

        void carryingDTMFdigits(char code);

#if HAVE_INSTANT_MESSAGING
        void sendTextMessage(const std::string& message,
                             const std::string& from);
#endif

        SIPAccountBase& getSIPAccount() const;

        void updateSDPFromSTUN();

        /**
         * Tell the user that the call was answered
         * @param
         */
        void onAnswered();

        /**
         * Handling 5XX/6XX error
         * @param
         */
        void onServerFailure();

        /**
         * Peer close the connection
         * @param
         */
        void onClosed();

        void setupLocalSDPFromIce();

        bool startIce();

        void startAllMedia();

        void onMediaUpdate();

        void onReceiveOffer(const pjmedia_sdp_session *offer);

        void openPortsUPnP();

        virtual std::map<std::string, std::string> getDetails() const;

    private:
        NON_COPYABLE(SIPCall);

        void stopAllMedia();

        /**
         * Transfer method used for both type of transfer
         */
        bool transferCommon(pj_str_t *dst);

        void internalOffHold(const std::function<void()> &SDPUpdateFunc);

        int SIPSessionReinvite();

        std::vector<IceCandidate> getAllRemoteCandidates();

        std::unique_ptr<AudioRtpSession> avformatrtp_;

#ifdef RING_VIDEO
        /**
         * Video Rtp Session factory
         */
        video::VideoRtpSession videortp_;

        std::string videoInput_;
#endif

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

        char contactBuffer_[PJSIP_MAX_URL_SIZE] {};
        pj_str_t contactHeader_ {contactBuffer_, 0};

        std::unique_ptr<ring::upnp::Controller> upnp_;
};

} // namespace ring

#endif // __SIPCALL_H__
