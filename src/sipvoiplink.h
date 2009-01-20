/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SIPVOIPLINK_H
#define SIPVOIPLINK_H

#include "voiplink.h"
#include "useragent.h"

class EventThread;
class SIPCall;
class AudioRtp;

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events)
 */

class SIPVoIPLink : public VoIPLink
{
  public:

    /**
     * Constructor
     * @param accountID The account identifier
     */
    SIPVoIPLink(const AccountID& accountID);

    /**
     * Destructor
     */
    ~SIPVoIPLink();

    /* Copy Constructor */
    SIPVoIPLink(const SIPVoIPLink& rh);

    /* Assignment Operator */
    SIPVoIPLink& operator=( const SIPVoIPLink& rh);
   
    /** 
     * Try to initiate the pjsip engine/thread and set config 
     * @return bool True if OK
     */
    bool init(void);

    /**
     * Delete link-related stuuf like calls
     */
    void terminate(void);

    /**
     * Event listener. Each event send by the call manager is received and handled from here
     */
    void getEvent(void);

    /**
     * Build and send SIP registration request
     * @return bool True on success
     *		  false otherwise
     */
    int sendRegister(void);

    /**
     * Build and send SIP unregistration request
     * @return bool True on success
     *		  false otherwise
     */
    int sendUnregister(void);

    /**
     * Place a new call
     * @param id  The call identifier
     * @param toUrl  The Sip address of the recipient of the call
     * @return Call* The current call
     */
    Call* newOutgoingCall(const CallID& id, const std::string& toUrl);

    /**
     * Answer the call
     * @param id The call identifier
     * @return bool True on success
     */
    bool answer(const CallID& id);

    /**
     * Hang up the call
     * @param id The call identifier
     * @return bool True on success
     */
    bool hangup(const CallID& id);

    /**
     * Cancel the call
     * @param id The call identifier
     * @return bool True on success
     */
    bool cancel(const CallID& id);

    /**
     * Put the call on hold
     * @param id The call identifier
     * @return bool True on success
     */
    bool onhold(const CallID& id);

    /**
     * Put the call off hold
     * @param id The call identifier
     * @return bool True on success
     */
    bool offhold(const CallID& id);

    /**
     * Transfer the call
     * @param id The call identifier
     * @param to The recipient of the transfer
     * @return bool True on success
     */
    bool transfer(const CallID& id, const std::string& to);

    /** Handle the incoming refer msg, not finished yet */
    bool transferStep2();

    /**
     * Refuse the call
     * @param id The call identifier
     * @return bool True on success
     */
    bool refuse (const CallID& id);

    /**
     * Send DTMF
     * @param id The call identifier
     * @param code  The char code
     * @return bool True on success
     */
    bool carryingDTMFdigits(const CallID& id, char code);

    /** 
     * If set to true, we check for a firewall
     * @param use true if we use STUN
     */
    void setUseStun(bool use) { _useStun = use; }

    /** 
     * The name of the STUN server
     * @param server Server FQDN/IP
     */
    void setStunServer(const std::string& server) { _stunServer = server; }

    bool isRegister() {return _bRegister;}
    
    void setRegister(bool result) {_bRegister = result;}

  public:

    /** 
     * Terminate every call not hangup | brutal | Protected by mutex 
     */
    void terminateSIPCall(); 
    
    /**
     * send SIP authentification
     * @return bool true if sending succeed
     */
    bool sendSIPAuthentification();

    /**
     * Get a SIP From header ("fullname" <sip:userpart@hostpart>)
     * @param userpart User part
     * @param hostpart Host name
     * @return std::string  SIP URI for from Header
     */
    std::string SIPFromHeader(const std::string& userpart, const std::string& hostpart);

    /**
     * Build a sip address with the number that you want to call
     * Example: sip:124@domain.com
     * @param to  The header of the recipient
     * @return std::string  Result as a string
     */
    std::string SIPToHeader(const std::string& to);

    /**
     * Check if an url is sip-valid
     * @param url The url to check
     * @return bool True if osip tell that is valid
     */
    bool SIPCheckUrl(const std::string& url);


    /**
     * Send an outgoing call invite
     * @param call  The current call
     * @return bool True if all is correct
     */
    bool SIPOutgoingInvite(SIPCall* call);

    /**
     * Start a SIP Call
     * @param call  The current call
     * @param subject Undocumented
     * @return true if all is correct
     */
    bool SIPStartCall(SIPCall* call, const std::string& subject);

    /**
     * Get the Sip FROM url (add sip:, add @host, etc...)
     * @return std::string  The From url
     */
    std::string getSipFrom();

    /**
     * Get the Sip TO url (add sip:, add @host, etc...)
     * @param to_url  The To url
     * @return std::string  The SIP to address
     */
    std::string getSipTo(const std::string& to_url);

    /**
     * Set audio (SDP) configuration for a call
     * localport, localip, localexternalport
     * @param call a SIPCall valid pointer
     * @return bool True
     */
    bool setCallAudioLocal(SIPCall* call);

    /**
     * Tell the user that the call was answered
     * @param
     */
    void SIPCallAnswered(SIPCall *call, pjsip_rx_data *rdata);
    
    /**
     * Handling 5XX/6XX error
     * @param 
     */
    void SIPCallServerFailure(SIPCall *call);

    /**
     * Peer close the connection
     * @param
     */
    void SIPCallClosed(SIPCall *call);

    /**
     * The call pointer was released
     * If the call was not cleared before, report an error
     * @param
     */
    void SIPCallReleased(SIPCall *call);

    /**
     * SIPCall accessor
     * @param id  The call identifier
     * @return SIPCall*	  A pointer on SIPCall object
     */
    SIPCall* getSIPCall(const CallID& id);

    /** Tell if the initialisation was done */
    bool _initDone;

    /** when we init the listener, how many times we try to bind a port? */
    int _nbTryListenAddr;

    /** Do we use stun? */
    bool _useStun;

    /** What is the stun server? */
    std::string _stunServer;

    /** Local Extern Address is the IP address seen by peers for SIP listener */
    std::string _localExternAddress;

    /** Local Extern Port is the port seen by peers for SIP listener */
    unsigned int _localExternPort;  

    /** Starting sound */
    AudioRtp* _audiortp;
    
    pj_str_t string2PJStr(const std::string &value);

private:
    pjsip_regc *_regc;
    bool _bRegister;
};

#endif
