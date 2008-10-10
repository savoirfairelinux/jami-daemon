/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#ifndef SIPVOIPLINK_H
#define SIPVOIPLINK_H

#include "voiplink.h"
#include <string>
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjnath/stun_config.h>

//TODO Remove this include if we don't need anything from it
#include <pjsip_simple.h>

#include <pjsip_ua.h>
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>

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
     * Check if a local IP can be found
     * @return bool True if network is reachable
     */
    bool checkNetwork(void);

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

    bool sendMessage(const std::string& to, const std::string& body);

    bool isContactPresenceSupported();

    // TODO Not used yet
    void sendMessageToContact(const CallID& id, const std::string& message);

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

    /** 
     * Set the SIP proxy
     * @param proxy Proxy FQDN/IP
     */
    void setProxy(const std::string& proxy) { _proxy = proxy; }

    /**
     * Set the authentification name
     * @param authname The authentification name
     */
    void setAuthName(const std::string& authname); //{ _authname = authname; }

    /**
     * Set the password
     * @param password	Password
     */
    void setPassword(const std::string& password); //{ _password = password; }
    
    void setSipServer(const std::string& sipServer);
   
    bool isRegister() {return _bRegister;}
    
    void setRegister(bool result) {_bRegister = result;}

  public:

    /** 
     * Terminate every call not hangup | brutal | Protected by mutex 
     */
    void terminateSIPCall(); 

    /**
     * Get the local Ip  
     * only if the local ip address is to his default value: 127.0.0.1
     * setLocalIpAdress
     * @return bool false if not found
     */
    bool loadSIPLocalIP();

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
     * Get the sip proxy (add sip: if there is one) 
     * @return std::string  Empty string or <sip:proxy;lr> url
     */
    std::string getSipRoute();

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
     * Find a SIPCall with cid
     * Explication there is no DID when the dialog is not establish...
     * @param cid call ID
     * @return SIPCall*	SIPCall pointer or 0
     */
    SIPCall* findSIPCallWithCid(int cid);

    /**
     * Find a SIPCall with cid and did
     * @param cid call ID
     * @param did domain ID
     * @return SIPCall*	SIPCall pointer or 0
     */
    SIPCall* findSIPCallWithCidDid(int cid, int did);

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

    /** SIP Proxy URL */
    std::string _proxy;

    /** SIP Authenfication name */
    std::string _authname;

    /** SIP Authenfication password */
    std::string _password; 

    /** Starting sound */
    AudioRtp* _audiortp;
    
    pj_str_t string2PJStr(const std::string &value);
private:
    pjsip_regc *_regc;
    std::string _server;
    bool _bRegister;
};

#endif
