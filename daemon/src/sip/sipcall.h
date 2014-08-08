/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
#include "audio/audiortp/audio_rtp_factory.h"
#ifdef SFL_VIDEO
#include "video/video_rtp_session.h"
#endif

#include "noncopyable.h"

#include "pjsip/sip_config.h"

#include <memory>

struct pjsip_evsub;
struct pj_caching_pool;
struct pj_pool_t;
struct pjsip_inv_session;

class Sdp;
class SIPAccount;

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
        SIPCall(SIPAccount& account, const std::string& id, Call::CallType type);

    public:
        /**
         * Destructor
         */
        ~SIPCall();

        /**
         * Return the local SDP session
         */
        Sdp& getLocalSDP() {
            return *local_sdp_;
        }

        /**
         * Returns a pointer to the AudioRtp object
         */
        sfl::AudioRtpFactory & getAudioRtp() {
            return audiortp_;
        }

#ifdef SFL_VIDEO
        /**
         * Returns a pointer to the VideoRtp object
         */
        sfl_video::VideoRtpSession &getVideoRtp () {
            return videortp_;
        }
#endif

        /**
         * Return the local memory pool for this call
         */
        pj_pool_t *getMemoryPool() {
            return pool_;
        }

        /**
         * The invite session to be reused in case of transfer
         */
        struct InvSessionDeleter { void operator()(pjsip_inv_session*) {} };
        std::unique_ptr<pjsip_inv_session, InvSessionDeleter> inv{};

        void setCallMediaLocal(const pj_sockaddr& localIP);

        void setContactHeader(pj_str_t *contact);

        void sendSIPInfo(const char *const body, const char *const subtype);

        void answer();

        void hangup(int reason);

        void refuse();

        void transfer(const std::string& to);

        bool attendedTransfer(const std::string& to);

        void onhold();

        void offhold();

        void peerHungup();

        void carryingDTMFdigits(char code);

#if HAVE_INSTANT_MESSAGING
        void sendTextMessage(const std::string& message,
                             const std::string& from);
#endif

        SIPAccount& getSIPAccount() const;

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

    private:
        NON_COPYABLE(SIPCall);

        // override of Call::createHistoryEntry
        std::map<std::string, std::string>
        createHistoryEntry() const;

        void stopRtpIfCurrent();

        /**
         * Transfer method used for both type of transfer
         */
        bool transferCommon(pj_str_t *dst);

        void internalOffHold(const std::function<void()> &SDPUpdateFunc);

        int SIPSessionReinvite();

        /**
         * Audio Rtp Session factory
         */
        sfl::AudioRtpFactory audiortp_;

#ifdef SFL_VIDEO
        /**
         * Video Rtp Session factory
         */
        sfl_video::VideoRtpSession videortp_;
#endif

        /**
         * The pool to allocate memory, released once call hang up
         */
        pj_pool_t *pool_;

        /**
         * The SDP session
         */
        std::unique_ptr<Sdp> local_sdp_;

        char contactBuffer_[PJSIP_MAX_URL_SIZE];
        pj_str_t contactHeader_;
};

#endif // __SIPCALL_H__
