/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <pjsip-simple/evsub.h>
#include <pjsip_ua.h>

#include "audio/audiortp/AudioRtpFactory.h"

class AudioCodec;
class Sdp;
class AudioRtp;

namespace sfl {
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
    SIPCall(const CallID& id, Call::CallType type, pj_pool_t *pool );

    /**
     * Destructor
     */
    ~SIPCall();

    /** 
     * Call Identifier
     * @return int  SIP call id
     */
    int  getCid() { return _cid; }
    
    /** 
     * Call Identifier
     * @param cid SIP call id
     */
    void setCid(int cid) { _cid = cid ; } 
    
    /** 
     * Domain identifier
     * @return int  SIP domain id
     */
    int  getDid() { return _did; }
    
    /** 
     * Domain identifier
     * @param did SIP domain id
     */
    void setDid(int did) { _did = did; } 
    
    /** 
     * Transaction identifier
     * @return int  SIP transaction id
     */
    int  getTid() { return _tid; }

    
    
    
    /** 
     * Transaction identifier
     * @param tid SIP transaction id
     */
    void setTid(int tid) { _tid = tid; } 

    void setXferSub(pjsip_evsub* sub) {_xferSub = sub;}

    pjsip_evsub *getXferSub() {return _xferSub;}
    
    void setInvSession(pjsip_inv_session* inv) {_invSession = inv;}

    pjsip_inv_session *getInvSession() {return _invSession;}
    
    Sdp* getLocalSDP (void) { return _local_sdp; }

    void setLocalSDP (Sdp *local_sdp) { _local_sdp = local_sdp; }

    /** Returns a pointer to the AudioRtp object */
    inline sfl::AudioRtpFactory * getAudioRtp(void) { return _audiortp; }

  private:

    int _cid;
    int _did;
    int _tid;

    // Copy Constructor
    SIPCall(const SIPCall& rh);

    // Assignment Operator
    SIPCall& operator=( const SIPCall& rh);

    /** Starting sound */
    sfl::AudioRtpFactory * _audiortp;

    pjsip_evsub *_xferSub;
    
	pjsip_inv_session *_invSession;
    
	Sdp *_local_sdp;

};

#endif
