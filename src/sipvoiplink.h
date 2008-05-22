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
#include <eXosip2/eXosip.h>
#include "audio/audiortp.h"
#include <osip2/osip_mt.h>

class EventThread;
class SIPCall;

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

    int eXosip_running;

    /** 
     * Try to initiate the eXosip engine/thread and set config 
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
    bool sendRegister(void);

    /**
     * Build and send SIP unregistration request
     * @return bool True on success
     *		  false otherwise
     */
    bool sendUnregister(void);

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

    //void subscribePresenceForContact(Contact* contact);

    void publishPresenceStatus(std::string status);

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
    void setAuthName(const std::string& authname) { _authname = authname; }

    /**
     * Set the password
     * @param password	Password
     */
    void setPassword(const std::string& password) { _password = password; }

  private:

    /** 
     * Terminate every call not hangup | brutal | Protected by mutex 
     */
    void terminateSIPCall(); 

    /**
     * Get the local Ip by eXosip 
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
     * Create a new call and send a incoming call notification to the user
     * @param event eXosip Event
     */
    void SIPCallInvite(eXosip_event_t *event);

    /**
     * Use a exisiting call to restart the audio
     * @param event eXosip Event
     */
    void SIPCallReinvite(eXosip_event_t *event);

    /**
     * Tell the user that the call is ringing
     * @param event eXosip Event
     */
    void SIPCallRinging(eXosip_event_t *event);

    /**
     * Tell the user that the call was answered
     * @param event eXosip Event
     */
    void SIPCallAnswered(eXosip_event_t *event);

    /**
     * Handling 4XX error
     * @param event eXosip Event
     */
    void SIPCallRequestFailure(eXosip_event_t *event);

    /**
     * Handling 5XX/6XX error
     * @param event eXosip Event
     */
    void SIPCallServerFailure(eXosip_event_t *event);

    /**
     * Handle registration failure cases ( SIP_FORBIDDEN , SIP_UNAUTHORIZED )
     * @param event eXosip event
     */
    void SIPRegistrationFailure( eXosip_event_t *event );

    /**
     * Handling ack (restart audio if reinvite)
     * @param event eXosip Event
     */
    void SIPCallAck(eXosip_event_t *event);

    /**
     * Handling message inside a call (like dtmf)
     * @param event eXosip Event
     */
    void SIPCallMessageNew(eXosip_event_t *event);

    /**
     * Handle an INFO with application/dtmf-relay content-type
     * @param event eXosip Event
     */
    bool handleDtmfRelay(eXosip_event_t *event);

    /**
     * Peer close the connection
     * @param event eXosip Event
     */
    void SIPCallClosed(eXosip_event_t *event);

    /**
     * The call pointer was released
     * If the call was not cleared before, report an error
     * @param event eXosip Event
     */
    void SIPCallReleased(eXosip_event_t *event);

    /**
     * Receive a new Message request
     * Option/Notify/Message
     * @param event eXosip Event
     */
    void SIPMessageNew(eXosip_event_t *event);

    /**
     * Find a SIPCall with cid from eXosip Event
     * Explication there is no DID when the dialog is not establish...
     * @param cid call ID
     * @return SIPCall*	SIPCall pointer or 0
     */
    SIPCall* findSIPCallWithCid(int cid);

    /**
     * Find a SIPCall with cid and did from eXosip Event
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

    /** To build sdp when call is on-hold */
    int sdp_hold_call (sdp_message_t * sdp);

    /** To build sdp when call is off-hold */
    int sdp_off_hold_call (sdp_message_t * sdp);

    /** EventThread get every incoming events */
    EventThread* _evThread;

    /** Tell if eXosip was stared (eXosip_init) */
    bool _initDone;

    /** Registration identifier, needed by unregister to build message */
    int _eXosipRegID;

    /** Number of voicemail */
    int _nMsgVoicemail;

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
    AudioRtp _audiortp;
};

#endif
