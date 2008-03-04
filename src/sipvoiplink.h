/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

class EventThread;
class SIPCall;

/**
 * Specific VoIPLink for SIP (SIP core for incoming and outgoing events)
 * @author Yan Morin <yan.morin@gmail.com>
 */

class SIPVoIPLink : public VoIPLink
{
public:
  SIPVoIPLink(const AccountID& accountID);

  ~SIPVoIPLink();

  /** try to initiate the eXosip engine/thread and set config */
  bool init(void);
  void terminate(void);
  bool checkNetwork(void);
  void getEvent(void);

  bool sendRegister(void);
  bool sendUnregister(void);

  Call* newOutgoingCall(const CallID& id, const std::string& toUrl);
  bool answer(const CallID& id);

  bool hangup(const CallID& id);
  bool cancel(const CallID& id);
  bool onhold(const CallID& id);
  bool offhold(const CallID& id);
  bool transfer(const CallID& id, const std::string& to);
  bool refuse (const CallID& id);
  bool carryingDTMFdigits(const CallID& id, char code);
  bool sendMessage(const std::string& to, const std::string& body);
  bool isContactPresenceSupported();
  void subscribePresenceForContact(Contact* contact);
  void publishPresenceStatus(std::string status);
  
  // TODO Not used yet
  void sendMessageToContact(const CallID& id, const std::string& message);

  // SIP Specific

  /** If set to true, we check for a firewall
   * @param use true if we use STUN
   */
  void setUseStun(bool use) { _useStun = use; }

  /** The name of the STUN server
   * @param server Server FQDN/IP
   */
  void setStunServer(const std::string& server) { _stunServer = server; }

  /** Set the SIP proxy
   * @param proxy Proxy FQDN/IP
   */
  void setProxy(const std::string& proxy) { _proxy = proxy; }
  void setUserPart(const std::string& userpart) { _userpart = userpart; }
  void setAuthName(const std::string& authname) { _authname = authname; }
  void setPassword(const std::string& password) { _password = password; }


private:
  /** Terminate every call not hangup | brutal | Protected by mutex */
  void terminateSIPCall(); 

  /**
  * Get the local Ip by eXosip 
  * only if the local ip address is to his default value: 127.0.0.1
  * setLocalIpAdress
  * @return false if not found
  */
  bool loadSIPLocalIP();

  /**
   * send SIP authentification
   * @return true if sending succeed
   */
  bool sendSIPAuthentification();

  /**
   * Get a SIP From header ("fullname" <sip:userpart@hostpart>)
   * @param userpart
   * @param hostpart
   * @return SIP URI for from Header
   */
  std::string SIPFromHeader(const std::string& userpart, const std::string& hostpart);

  /**
   * Build a sip address with the number that you want to call
   * Example: sip:124@domain.com
   * @return result as a string
   */
  std::string SIPToHeader(const std::string& to);

  /**
   * Check if an url is sip-valid
   * @return true if osip tell that is valid
   */
  bool SIPCheckUrl(const std::string& url);


  /**
   * SIPOutgoingInvite do SIPStartCall
   * @return true if all is correct
   */
  bool SIPOutgoingInvite(SIPCall* call);

  /**
   * Start a SIP Call
   * @return true if all is correct
   */
  bool SIPStartCall(SIPCall* call, const std::string& subject);
  std::string getSipFrom();
  std::string getSipRoute();
  std::string getSipTo(const std::string& to_url);

  /**
   * Set audio (SDP) configuration for a call
   * localport, localip, localexternalport
   * @param call a SIPCall valid pointer
   * @return true
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
   * Handling ack (restart audio if reinvite)
   * @param event eXosip Event
   */
  void SIPCallAck(eXosip_event_t *event);

  /**
   * Handling message inside a call (like dtmf)
   * @param event eXosip Event
   */
  void SIPCallMessageNew(eXosip_event_t *event);
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
  * @return 0 or SIPCall pointer
  */
  SIPCall* findSIPCallWithCid(int cid);

  /**
  * Find a SIPCall with cid and did from eXosip Event
  * @param cid call ID
  * @param did domain ID
  * @return 0 or SIPCall pointer
  */
  SIPCall* findSIPCallWithCidDid(int cid, int did);
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

  /** SIP UserPart */
  std::string _userpart;

  /** SIP Authenfication name */
  std::string _authname;

  /** SIP Authenfication password */
  std::string _password; 

  /** Starting sound */
  AudioRtp _audiortp;
};

#endif
