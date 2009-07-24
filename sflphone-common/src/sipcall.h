/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
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
 */
#ifndef SIPCALL_H
#define SIPCALL_H

#include "call.h"
#include "sipvoiplink.h"
#include "sdp.h"
#include "audio/audiortp.h"

class AudioCodec;
class AudioRtp;

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
    inline AudioRtp * getAudioRtp(void) { return _audiortp; }

    /** Returns a pointer to the AudioRtp object specific to this call */
    inline AudioRtp * setAudioRtp(AudioRtp* audiortp) { _audiortp = audiortp; }


  private:

    Sdp *_local_sdp;

    int _cid;
    int _did;
    int _tid;

    // Copy Constructor
    SIPCall(const SIPCall& rh);

    // Assignment Operator
    SIPCall& operator=( const SIPCall& rh);
    
    /** Starting sound */
    AudioRtp* _audiortp;
    
    pjsip_evsub *_xferSub;
    pjsip_inv_session *_invSession;
};

#endif
