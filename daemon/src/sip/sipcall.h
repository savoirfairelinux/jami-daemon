/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#ifndef SIPCALL_H
#define SIPCALL_H

#include "call.h"
#include <cassert>

class pjsip_evsub;
class pj_caching_pool;
class pj_pool_t;
class pjsip_inv_session;
class Sdp;

namespace sfl
{
class AudioRtpFactory;
}

/**
 * @file sipcall.h
 * @brief SIPCall are SIP implementation of a normal Call
 */
class SIPCall : public Call
{
    public:

        /**
         * Constructor
         * @param id	The call identifier
         * @param type  The type of the call. Could be Incoming
         *						 Outgoing
         */
        SIPCall (const std::string& id, Call::CallType type, pj_caching_pool *caching_pool);

        /**
         * Destructor
         */
        ~SIPCall ();
        /**
         * Get event subscription internal structure
         */
        pjsip_evsub *getXferSub() {
            return _xferSub;
        }

        /**
         * Set event subscription internal structure
         */
        void setXferSub (pjsip_evsub* sub) {
            _xferSub = sub;
        }

        void setInvSession (pjsip_inv_session* inv) {
            _invSession = inv;
        }

        pjsip_inv_session *getInvSession() {
            return _invSession;
        }

        void replaceInvSession (pjsip_inv_session *inv) {
            _invSession = inv;
        }

        /**
         * Return the local SDP session
         */
        Sdp* getLocalSDP (void) {
            return local_sdp_;
        }

        /**
         * Returns a pointer to the AudioRtp object
         */
        sfl::AudioRtpFactory * getAudioRtp (void) {
            return _audiortp;
        }

        /**
         * Return the local memory pool for this call
         */
        pj_pool_t *getMemoryPool(void) {
            return pool_;
        }

    private:

        // Copy Constructor
        SIPCall (const SIPCall& rh);

        // Assignment Operator
        SIPCall& operator= (const SIPCall& rh);

        /**
         * Call specific memory pool initialization size (based on empirical data)
         */
        static const int CALL_MEMPOOL_INIT_SIZE;

        /**
         * Call specific memory pool incrementation size
         */
        static const int CALL_MEMPOOL_INC_SIZE;

        /**
         * Call identifier
         */
        int _cid;

        /**
         * Domain identifier
         */
        int _did;

        /**
         * Transaction identifier
         */
        int _tid;

        /**
         * Audio Rtp Session factory
         */
        sfl::AudioRtpFactory * _audiortp;

        /**
         * Event subscription structure
         */
        pjsip_evsub *_xferSub;

        /**
         * The invite session to be reused in case of transfer
         */
        pjsip_inv_session *_invSession;

        /**
         * The pool to allocate memory, released once call hang up
         */
        pj_pool_t *pool_;

        /**
         * The SDP session
         */
        Sdp *local_sdp_;
};

#endif
