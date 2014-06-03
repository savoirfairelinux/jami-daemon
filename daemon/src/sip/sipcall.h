/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

struct pjsip_evsub;
struct pj_caching_pool;
struct pj_pool_t;
struct pjsip_inv_session;
class Sdp;

/**
 * @file sipcall.h
 * @brief SIPCall are SIP implementation of a normal Call
 */
class SIPCall : public Call {
    public:

        /**
         * Constructor
         * @param id	The call identifier
         * @param type  The type of the call. Could be Incoming
         *						 Outgoing
         */
        SIPCall(const std::string& id, Call::CallType type,
                pj_caching_pool *caching_pool, const std::string &account_id);

        /**
         * Destructor
         */
        ~SIPCall();

        /**
         * Return the local SDP session
         */
        Sdp* getLocalSDP() {
            return local_sdp_;
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

        void answer();

        /**
         * The invite session to be reused in case of transfer
         */
        pjsip_inv_session *inv;

        void setContactHeader(pj_str_t *contact);

        void onhold();
        void offhold(const std::function<void()> &SDPUpdateFunc);

    private:

        // override of Call::createHistoryEntry
        std::map<std::string, std::string>
        createHistoryEntry() const;

        NON_COPYABLE(SIPCall);

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
        Sdp *local_sdp_;

        char contactBuffer_[PJSIP_MAX_URL_SIZE];
        pj_str_t contactHeader_;
};

#endif // __SIPCALL_H__
