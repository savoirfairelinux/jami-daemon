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
#include <eXosip2/eXosip.h>

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
     * @return int  SIP call id : protected by eXosip lock 
     */
    int  getCid() { return _cid; }
    
    /** 
     * Call Identifier
     * @param cid SIP call id : protected by eXosip lock 
     */
    void setCid(int cid) { _cid = cid ; } 
    
    /** 
     * Domain identifier
     * @return int  SIP domain id : protected by eXosip lock  
     */
    int  getDid() { return _did; }
    
    /** 
     * Domain identifier
     * @param did SIP domain id : protected by eXosip lock 
     */
    void setDid(int did) { _did = did; } 
    
    /** 
     * Transaction identifier
     * @return int  SIP transaction id : protected by eXosip lock  
     */
    int  getTid() { return _tid; }
    
    /** 
     * Transaction identifier
     * @param tid SIP transaction id : protected by eXosip lock 
     */
    void setTid(int tid) { _tid = tid; } 

    /**
     * Setup incoming call, and verify for errors, before ringing the user.
     * @param event eXosip Event
     * @return bool True on success
     *		    false otherwise
     */
    bool SIPCallInvite(eXosip_event_t *event);

    /**
     * newReinviteCall is called when the IP-Phone user receives a change in the call
     * it's almost an newIncomingCall but we send a 200 OK
     * See: 3.7.  Session with re-INVITE (IP Address Change)
     * @param event eXosip Event
     * @return bool True if ok
     */
    bool SIPCallReinvite(eXosip_event_t *event);

    /**
     * Peer answered to a call (on hold or not)
     * @param event eXosip Event
     * @return bool True if ok
     */
    bool SIPCallAnswered(eXosip_event_t *event);

    /**
     * We retreive final SDP info if they changed
     * @param event eXosip Event
     * @return bool True if ok (change / no change) or false on error
     */
    bool SIPCallAnsweredWithoutHold(eXosip_event_t *event);

    //TODO: humm?
    int sdp_complete_message(sdp_message_t * remote_sdp, osip_message_t * msg);


  private:

    // TODO: hum???
    int sdp_analyse_attribute (sdp_message_t * sdp, sdp_media_t * med);
    
    /**
     * Set peer name and number with event->request->from
     * @param event eXosip event
     * @return bool False if the event is invalid
     */
    bool setPeerInfoFromRequest(eXosip_event_t *event);

    /**
     * Get a valid remote SDP or return a 400 bad request response if invalid
     * @param event eXosip event
     * @return sdp_message_t* A valid remote_sdp or 0
     */
    sdp_message_t* getRemoteSDPFromRequest(eXosip_event_t *event);

    /**
     * Get a valid remote media or return a 415 unsupported media type
     * @param tid transaction id
     * @param remote_sdp Remote SDP pointer
     * @return sdp_media_t* A valid sdp_media_t or 0
     */
    sdp_media_t* getRemoteMedia(int tid, sdp_message_t* remote_sdp);

    /**
     * Set Audio Port and Audio IP from Remote SDP Info
     * @param remote_med Remote Media info
     * @param remote_sdp Remote SDP pointer
     * @return bool True if everything is set correctly
     */
    bool setRemoteAudioFromSDP(sdp_media_t* remote_med, sdp_message_t* remote_sdp);

    /**
     * Set Audio Codec with the remote choice
     * @param remote_med Remote Media info
     * @return bool True if everything is set correctly
     */
    bool setAudioCodecFromSDP(sdp_media_t* remote_med, int tid);

    /** SIP call id */
    int _cid;

    /** SIP domain id */
    int _did;
    
    /** SIP transaction id */
    int _tid;

};

#endif
