/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
/*
 * YM: 2006-11-15: changes unsigned int to std::string::size_type, thanks to Pierre Pomes (AMD64 compilation)
 */
#include "sipvoiplink.h"
#include "eventthread.h"
#include "sipcall.h"
#include <sstream> // for istringstream
#include "sipaccount.h"
#include "useragent.h"
#include "audio/audiortp.h"
        
#include "manager.h"
#include "user_cfg.h" // SIGNALISATION / PULSE #define

#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2

// 1XX responses
#define DIALOG_ESTABLISHED 101
// see: osip_const.h

// need for hold/unhold
#define INVITE_METHOD "INVITE"

SIPVoIPLink::SIPVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID)
 , _initDone(false)
 , _nbTryListenAddr(2) // number of times to try to start SIP listener
 , _useStun(false)
 , _stunServer("")
 , _localExternAddress("") 
 , _localExternPort(0)
 , _audiortp(new AudioRtp())
 , _regc()
 , _bRegister(false)
{
  // to get random number for RANDOM_PORT
  srand (time(NULL));
}

SIPVoIPLink::~SIPVoIPLink()
{
  terminate();
}

bool 
SIPVoIPLink::init()
{
  _regc = NULL;
  _initDone = true;
  return true;
}

void 
SIPVoIPLink::terminate()
{
  _initDone = false;
}

void
SIPVoIPLink::terminateSIPCall()
{
  
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  SIPCall *call;
  while( iter != _callMap.end() ) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call) {
      //TODO terminate the sip call
      delete call; call = 0;
    }
    iter++;
  }
  _callMap.clear();
}

void
SIPVoIPLink::getEvent()
{
    // Nothing anymore. PJSIP is based on asynchronous events
}

int
SIPVoIPLink::sendRegister()
{
  AccountID id;
  pj_status_t status;
  
  id = getAccountID();

  if(_regc) {
      status = pjsip_regc_destroy(_regc);
      _regc = NULL;
      PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
  }

  _bRegister = true;
  
  int expire_value = Manager::instance().getRegistrationExpireValue();
  _debug("SIP Registration Expire Value = %i\n" , expire_value);

  setRegistrationState(Trying);

  return Manager::instance().getUserAgent()->addAccount(id, &_regc, getHostname(), getUsername(), getPassword(), expire_value);
}

std::string
SIPVoIPLink::SIPFromHeader(const std::string& userpart, const std::string& hostpart) 
{
  return ("\"" + getFullName() + "\"" + " <sip:" + userpart + "@" + hostpart + ">");
}

bool
SIPVoIPLink::sendSIPAuthentification() 
{
  if (getUsername().empty()) {
    /** @todo Ajouter ici un call à setRegistrationState(Error, "Fill balh") ? */
    return false;
  }
  if (getPassword().empty()) {
    /** @todo Même chose ici  ? */
    return false;
  }

  return true;
}

int
SIPVoIPLink::sendUnregister()
{
  _debug("SEND UNREGISTER for account %s\n" , getAccountID().c_str());

  if(!_bRegister){
      setRegistrationState(VoIPLink::Unregistered); 
      return true;
  }
  
  _bRegister = false;
  
  Manager::instance().getUserAgent()->removeAccount(_regc);
  
  return true;
}

Call* 
SIPVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
  SIPCall* call = new SIPCall(id, Call::Outgoing);
  if (call) {
    //call->setPeerNumber(toUrl);
    call->setPeerNumber(getSipTo(toUrl));
    _debug("Try to make a call to: %s with call ID: %s\n", toUrl.data(), id.data());
    // we have to add the codec before using it in SIPOutgoingInvite...
    call->setCodecMap(Manager::instance().getCodecDescriptorMap());
    if ( SIPOutgoingInvite(call) ) {
      call->setConnectionState(Call::Progressing);
      call->setState(Call::Active);
      addCall(call);
    } else {
      delete call; call = 0;
    }
  }
  return call;
}

bool
SIPVoIPLink::answer(const CallID& id)
{
  _debug("- SIP Action: start answering\n");

  SIPCall* call = getSIPCall(id);
  if (call==0) {
    _debug("! SIP Failure: SIPCall doesn't exists\n");
    return false;
  }

  int i = Manager::instance().getUserAgent()->answer(call);
  
  if (i != 0) {
    _debug("< SIP Building Error: send 400 Bad Request\n");
  } else {
    // use exosip, bug locked
    i = 0;
    _debug("* SIP Info: Starting AudioRTP when answering\n");
    if (_audiortp->createNewSession(call) >= 0) {
      call->setAudioStart(true);
      call->setConnectionState(Call::Connected);
      call->setState(Call::Active);
      return true;
    } else {
      _debug("! SIP Failure: Unable to start sound when answering %s/%d\n", __FILE__, __LINE__);
    }
  }
  removeCall(call->getCallId());
  return false;
}

bool
SIPVoIPLink::hangup(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

    Manager::instance().getUserAgent()->hangup(call);
  
  // Release RTP thread
  if (Manager::instance().isCurrentCall(id)) {
    _debug("* SIP Info: Stopping AudioRTP for hangup\n");
    _audiortp->closeRtpSession();
  }
  removeCall(id);
  return true;
}

bool
SIPVoIPLink::cancel(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

  _debug("- SIP Action: Cancel call %s [cid: %3d]\n", id.data(), call->getCid()); 

  removeCall(id);

  return true;
}

bool
SIPVoIPLink::onhold(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: call doesn't exist\n"); return false; }  


  // Stop sound
  call->setAudioStart(false);
  call->setState(Call::Hold);
  _debug("* SIP Info: Stopping AudioRTP for onhold action\n");
  _audiortp->closeRtpSession();

  Manager::instance().getUserAgent()->onhold(call);

  return true;
}

bool 
SIPVoIPLink::offhold(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }

  Manager::instance().getUserAgent()->offhold(call);

  // Enable audio
  _debug("* SIP Info: Starting AudioRTP when offhold\n");
  call->setState(Call::Active);
  // it's sure that this is the current call id...
  if (_audiortp->createNewSession(call) < 0) {
    _debug("! SIP Failure: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    return false;
  }
  return true;
}

bool 
SIPVoIPLink::transfer(const CallID& id, const std::string& to)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Failure: Call doesn't exist\n"); return false; }  

  std::string tmp_to = SIPToHeader(to);
  if (tmp_to.find("@") == std::string::npos) {
    tmp_to = tmp_to + "@" + getHostname();
  }

  _debug("In transfer, tmp_to is %s\n", tmp_to.data());

  Manager::instance().getUserAgent()->transfer(call, tmp_to);

  //_audiortp->closeRtpSession();
  // shall we delete the call?
  //removeCall(id);
  return true;
}

bool SIPVoIPLink::transferStep2()
{
    _audiortp->closeRtpSession();
    return true;
}

bool
SIPVoIPLink::refuse (const CallID& id)
{
  SIPCall* call = getSIPCall(id);

  if (call==0) { _debug("Call doesn't exist\n"); return false; }  

  // can't refuse outgoing call or connected
  if (!call->isIncoming() || call->getConnectionState() == Call::Connected) { 
    _debug("It's not an incoming call, or it's already answered\n");
    return false; 
  }

  Manager::instance().getUserAgent()->refuse(call);
  
  return true;
}

bool 
SIPVoIPLink::carryingDTMFdigits(const CallID& id, char code UNUSED)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("Call doesn't exist\n"); return false; }

  int duration = Manager::instance().getConfigInt(SIGNALISATION, PULSE_LENGTH);
  const int body_len = 1000;
  char *dtmf_body = new char[body_len];
 
  snprintf(dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);
 
  return Manager::instance().getUserAgent()->carryingDTMFdigits(call, dtmf_body);
}

bool
SIPVoIPLink::SIPOutgoingInvite(SIPCall* call) 
{
  // If no SIP proxy setting for direct call with only IP address
  if (!SIPStartCall(call, "")) {
    _debug("! SIP Failure: call not started\n");
    return false;
  }
  return true;
}

bool
SIPVoIPLink::SIPStartCall(SIPCall* call, const std::string& subject UNUSED) 
{
    std::string to;

    if (!call) 
        return false;

    to = getSipTo(call->getPeerNumber());
    _debug("            To: %s\n", to.data());

    return Manager::instance().getUserAgent()->makeOutgoingCall(to, call, getAccountID());
}

std::string
SIPVoIPLink::getSipFrom() {

  // Form the From header field basis on configuration panel
  std::string hostname;
  
  hostname = getHostname();

  if ( hostname.empty() ) {
    hostname = _localIPAddress;
  }
  return SIPFromHeader(getUsername(), hostname);
}

std::string
SIPVoIPLink::getSipTo(const std::string& to_url) {
  // Form the From header field basis on configuration panel
  //bool isRegistered = (_eXosipRegID == EXOSIP_ERROR_STD) ? false : true;

  // add a @host if we are registered and there is no one inside the url
    if (to_url.find("@") == std::string::npos) {// && isRegistered) {
    std::string host = getHostname();
    if(!host.empty()) {
      return SIPToHeader(to_url + "@" + host);
    }
  }
  return SIPToHeader(to_url);
}

std::string
SIPVoIPLink::SIPToHeader(const std::string& to) 
{
  if (to.find("sip:") == std::string::npos) {
    return ("sip:" + to );
  } else {
    return to;
  }
}

bool
SIPVoIPLink::SIPCheckUrl(const std::string& url UNUSED)
{
  return true;
}

bool
SIPVoIPLink::setCallAudioLocal(SIPCall* call) 
{
  // Setting Audio
  unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
  unsigned int callLocalExternAudioPort = callLocalAudioPort;
  if (_useStun) {
    // If use Stun server
    if (Manager::instance().behindNat(_stunServer, callLocalAudioPort)) {
      callLocalExternAudioPort = Manager::instance().getFirewallPort();
    }
  }
  _debug("            Setting local audio port to: %d\n", callLocalAudioPort);
  _debug("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);
  
  // Set local audio port for SIPCall(id)
  call->setLocalIp(_localIPAddress);
  call->setLocalAudioPort(callLocalAudioPort);
  call->setLocalExternAudioPort(callLocalExternAudioPort);

  return true;
}

void
SIPVoIPLink::SIPCallServerFailure(SIPCall *call) 
{
  //if (!event->response) { return; }
  //switch(event->response->status_code) {
  //case SIP_SERVICE_UNAVAILABLE: // 500
  //case SIP_BUSY_EVRYWHERE:     // 600
  //case SIP_DECLINE:             // 603
    //SIPCall* call = findSIPCallWithCid(event->cid);
    if (call != 0) {
        _debug("Server error!\n");
      CallID id = call->getCallId();
      Manager::instance().callFailure(id);
      removeCall(id);
    }
  //break;
  //}
}

void
SIPVoIPLink::SIPCallClosed(SIPCall *call) 
{
  // it was without did before
  //SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) { return; }

  CallID id = call->getCallId();
  //call->setDid(event->did);
  if (Manager::instance().isCurrentCall(id)) {
    call->setAudioStart(false);
    _debug("* SIP Info: Stopping AudioRTP when closing\n");
    _audiortp->closeRtpSession();
  }
  _debug("After close RTP\n");
  Manager::instance().peerHungupCall(id);
  removeCall(id);
  _debug("After remove call ID\n");
}

void
SIPVoIPLink::SIPCallReleased(SIPCall *call)
{
  // do cleanup if exists
  // only cid because did is always 0 in these case..
  //SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) { return; }

  // if we are here.. something when wrong before...
  _debug("SIP call release\n");
  CallID id = call->getCallId();
  Manager::instance().callFailure(id);
  removeCall(id);
}

void
SIPVoIPLink::SIPCallAnswered(SIPCall *call, pjsip_rx_data *rdata)
{
  //SIPCall* call = dynamic_cast<SIPCall *>(theCall);//findSIPCallWithCid(event->cid);
  if (!call) {
    _debug("! SIP Failure: unknown call\n");
    return;
  }
  //call->setDid(event->did);

  if (call->getConnectionState() != Call::Connected) {
    //call->SIPCallAnswered(event);
    call->SIPCallAnsweredWithoutHold(rdata);

    call->setConnectionState(Call::Connected);
    call->setState(Call::Active);

    Manager::instance().peerAnsweredCall(call->getCallId());
    if (Manager::instance().isCurrentCall(call->getCallId())) {
      _debug("* SIP Info: Starting AudioRTP when answering\n");
      if ( _audiortp->createNewSession(call) < 0) {
        _debug("RTP Failure: unable to create new session\n");
      } else {
        call->setAudioStart(true);
      }
    }
  } else {
     _debug("* SIP Info: Answering call (on/off hold to send ACK)\n");
     //call->SIPCallAnswered(event);
  }
}

SIPCall* 
SIPVoIPLink::findSIPCallWithCid(int cid) 
{
  if (cid < 1) {
    _debug("! SIP Error: Not enough information for this event\n");
    return NULL;
  }
  ost::MutexLock m(_callMapMutex);
  SIPCall* call = 0;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call && call->getCid() == cid) {
      return call;
    }
    iter++;
  }
  return NULL;
}

SIPCall* 
SIPVoIPLink::findSIPCallWithCidDid(int cid, int did) 
{
  if (cid < 1 && did < -1) {
    _debug("! SIP Error: Not enough information for this event\n");
    return NULL;
  }
  ost::MutexLock m(_callMapMutex);
  SIPCall* call = 0;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call && call->getCid() == cid && call->getDid() == did) {
      return call;
    }
    iter++;
  }
  return NULL;
}

SIPCall*
SIPVoIPLink::getSIPCall(const CallID& id) 
{
  Call* call = getCall(id);
  if (call) {
    return dynamic_cast<SIPCall*>(call);
  }
  return NULL;
}
/*
bool
SIPVoIPLink::handleDtmfRelay(eXosip_event_t* event) {

  SIPCall* call = findSIPCallWithCidDid(event->cid, event->did);
  if (call==0) { return false; }


  bool returnValue = false;
  osip_body_t *body = NULL;
  // Get the message body
  if (0 == osip_message_get_body(event->request, 0, &body) && body->body != 0 )   {
    _debug("* SIP Info: Text body: %s\n", body->body);
    std::string dtmfBody(body->body);
    std::string::size_type posStart = 0;
    std::string::size_type posEnd = 0;
    std::string signal;
    std::string duration;
    // search for signal=and duration=
    posStart = dtmfBody.find("Signal=");
    if (posStart != std::string::npos) {
      posStart += strlen("Signal=");
      posEnd = dtmfBody.find("\n", posStart);
      if (posEnd == std::string::npos) {
        posEnd = dtmfBody.length();
      }
      signal = dtmfBody.substr(posStart, posEnd-posStart+1);
      _debug("* SIP Info: Signal value: %s\n", signal.c_str());
      
      if (!signal.empty()) {
        if (Manager::instance().isCurrentCall(call->getCallId())) {
          Manager::instance().playDtmf(signal[0], true);
          returnValue = true;
        }
      }

 // we receive the duration, but we use our configuration...

      posStart = dtmfBody.find("Duration=");
      if (posStart != std::string::npos) {
        posStart += strlen("Duration=");
        posEnd = dtmfBody.find("\n", posStart);
        if (posEnd == std::string::npos) {
            posEnd = dtmfBody.length();
        }
        duration = dtmfBody.substr(posStart, posEnd-posStart+1);
        _debug("Duration value: %s\n", duration.c_str());
        returnValue = true;
      }

    }
  }
  return returnValue;
}
*/
///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

pj_str_t SIPVoIPLink::string2PJStr(const std::string &value)
{
    char tmp[256];
    
    strcpy(tmp, value.data());
    return pj_str(tmp);
}
