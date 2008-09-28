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
#include "audio/codecDescriptor.h"
#include "useragent.h"

class AudioCodec;

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
    SIPCall(const CallID& id, Call::CallType type);

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

    /**
     * Setup incoming call, and verify for errors, before ringing the user.
     * @param pjsip_rx_data *rdata
     * @param pj_pool_t *pool
     * @return bool True on success
     *		    false otherwise
     */
    bool SIPCallInvite(pjsip_rx_data *rdata, pj_pool_t *pool);

    bool SIPCallAnsweredWithoutHold(pjsip_rx_data *rdata);
 
    /**
     * Save IP Address
     * @param ip std::string 
     * @return void
     */
    void setIp(std::string ip) {_ipAddr = ip;}

    /**
     * Get the local SDP 
     * @param void
     * @return _localSDP pjmedia_sdp_session
     */
    pjmedia_sdp_session* getLocalSDPSession( void ) { return _localSDP; }
    
    /**
     * Begin negociation of media information between caller and callee
     * @param pj_pool_t *pool
     * @return bool True if ok
     */
    bool startNegociation(pj_pool_t *pool);

    /**
     * Create the localSDP, media negociation and codec information
     * @param pj_pool_t *pool
     * @return void
     */
    bool createInitialOffer(pj_pool_t *pool);
    
    void setXferSub(pjsip_evsub* sub) {_xferSub = sub;}
    pjsip_evsub *getXferSub() {return _xferSub;}
    
    void setInvSession(pjsip_inv_session* inv) {_invSession = inv;}
    pjsip_inv_session *getInvSession() {return _invSession;}
    
  private:
    /**
     * Get a valid remote SDP or return a 400 bad request response if invalid
     * @param
     * @return
     */
    pjmedia_sdp_session* getRemoteSDPFromRequest(pjsip_rx_data *rdata);

    /**
     * Get a valid remote media
     * @param remote_sdp pjmedia_sdp_session*
     * @return pjmedia_sdp_media*. A valid sdp_media_t or 0
     */
    pjmedia_sdp_media* getRemoteMedia(pjmedia_sdp_session *remote_sdp);

    /**
     * Set Audio Port and Audio IP from Remote SDP Info
     * @param remote_med Remote Media info
     * @param remote_sdp Remote SDP pointer
     * @return bool True if everything is set correctly
     */
    bool setRemoteAudioFromSDP(pjmedia_sdp_session* remote_sdp, pjmedia_sdp_media* remote_med);

    /**
     * Set Audio Codec with the remote choice
     * @param remote_med Remote Media info
     * @return bool True if everything is set correctly
     */
    bool setAudioCodecFromSDP(pjmedia_sdp_media* remote_med);

    /** SIP call id */
    int _cid;

    /** SIP domain id */
    int _did;
    
    /** SIP transaction id */
    int _tid;

    /** Local SDP */
    pjmedia_sdp_session *_localSDP;

    /** negociator */
    pjmedia_sdp_neg *_negociator;
    
    /**
     * Set origin information for local SDP
     */
    void sdpAddOrigin( void );
    
    /**
     * Set connection information for local SDP
     */
    void sdpAddConnectionInfo( void );
    /**
     * Set media information including codec for localSDP
     * @param  pj_pool_t* pool
     * @return void
     */
    void sdpAddMediaDescription(pj_pool_t* pool);

    /** IP address */
    std::string _ipAddr;
    
    pjsip_evsub *_xferSub;
    pjsip_inv_session *_invSession;
};

#endif
