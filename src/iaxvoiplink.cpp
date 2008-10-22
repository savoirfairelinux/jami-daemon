/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "iaxvoiplink.h"
#include "global.h" // for _debug
#include "iaxcall.h"
#include "eventthread.h"

#include "manager.h"
#include "audio/audiolayer.h"

//#include <iax/iax-client.h>
#include <math.h>
#include <dlfcn.h>

#define IAX_BLOCKING    1
#define IAX_NONBLOCKING 0

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

#define RANDOM_IAX_PORT   rand() % 64000 + 1024

#define MUSIC_ONHOLD true

#define CHK_VALID_CALL   if (call == NULL) { _debug("IAX: Call doesn't exists\n"); \
  return false; }

  IAXVoIPLink::IAXVoIPLink(const AccountID& accountID)
: VoIPLink(accountID)
{
  _evThread = new EventThread(this);
  _regSession = NULL;
  _nextRefreshStamp = 0;

  // to get random number for RANDOM_PORT
  srand (time(NULL));

  audiolayer = NULL;

  converter = new SamplerateConverter();

  int nbSamplesMax = (int) ( converter->getFrequence() * converter->getFramesize() / 1000 );

  micData = new SFLDataFormat[nbSamplesMax];
  micDataConverted = new SFLDataFormat[nbSamplesMax];
  micDataEncoded = new unsigned char[nbSamplesMax];

  spkrDataConverted = new SFLDataFormat[nbSamplesMax];
  spkrDataDecoded = new SFLDataFormat[nbSamplesMax];
}


IAXVoIPLink::~IAXVoIPLink()
{
  delete _evThread; _evThread = NULL;
  _regSession = NULL; // shall not delete it
  terminate();

  audiolayer = NULL;

  delete [] micData;  micData = NULL;
  delete [] micDataConverted;  micDataConverted = NULL;
  delete [] micDataEncoded;  micDataEncoded = NULL;

  delete [] spkrDataDecoded; spkrDataDecoded = NULL;
  delete [] spkrDataConverted; spkrDataConverted = NULL;
}

  bool
IAXVoIPLink::init()
{
  // If it was done, don't do it again, until we call terminate()
  if (_initDone)
    return false;

  bool returnValue = false;
  //_localAddress = "127.0.0.1";
  // port 0 is default
  //  iax_enable_debug(); have to enable debug when compiling iax...
  int port = IAX_DEFAULT_PORTNO;
  int last_port = 0;
  int nbTry = 3;

  while (port != IAX_FAILURE && nbTry) {
    last_port = port;
    port = iax_init(port);
    if ( port < 0 ) {
      _debug("IAX Warning: already initialize on port %d\n", last_port);
      port = RANDOM_IAX_PORT;
    } else if (port == IAX_FAILURE) {
      _debug("IAX Fail to start on port %d", last_port);
      port = RANDOM_IAX_PORT;
    } else {
      _debug("IAX Info: listening on port %d\n", last_port);
      _localPort = last_port;
      returnValue = true;
      _evThread->start();

      audiolayer = Manager::instance().getAudioDriver();
      break;
    }
    nbTry--;

    _initDone = true;
  }
  if (port == IAX_FAILURE || nbTry==0) {
    _debug("Fail to initialize iax\n");

    _initDone = false;
  }

  return returnValue;
}

  void
IAXVoIPLink::terminate()
{
  // If it was done, don't do it again, until we call init()
  if (!_initDone)
    return;

  // iaxc_shutdown();  

  // Hangup all calls
  terminateIAXCall();

  _initDone = false;
}

  void
IAXVoIPLink::terminateIAXCall()
{
  std::string reason = "Dumped Call";
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  IAXCall *call;
  while( iter != _callMap.end() ) {
    call = dynamic_cast<IAXCall*>(iter->second);
    if (call) {
      _mutexIAX.enterMutex();
      iax_hangup(call->getSession(), (char*)reason.c_str());
      _mutexIAX.leaveMutex();
      call->setSession(NULL);
      delete call; call = NULL;
    }
    iter++;
  }
  _callMap.clear();
}

  void
IAXVoIPLink::getEvent() 
{
  IAXCall* call = NULL;

  // lock iax_ stuff..
  _mutexIAX.enterMutex();
  iax_event* event = NULL;
  while ( (event = iax_get_event(IAX_NONBLOCKING)) != NULL ) {
    // If we received an 'ACK', libiax2 tells apps to ignore them.
    if (event->etype == IAX_EVENT_NULL) {
      continue;
    }

    //_debug ("Receive IAX Event: %d (0x%x)\n", event->etype, event->etype);

    call = iaxFindCallBySession(event->session);

    if (call) {
      // We know that call, deal with it
      iaxHandleCallEvent(event, call);
      //_audiocodec = Manager::instance().getCodecDescriptorMap().getCodec( call -> getAudioCodec() ); 
    }
    else if (event->session && event->session == _regSession) {
      // This is a registration session, deal with it
      iaxHandleRegReply(event);
    }
    else {
      // We've got an event before it's associated with any call
      iaxHandlePrecallEvent(event);
    }

    iax_event_free(event);
  }
  _mutexIAX.leaveMutex();

  // Do the doodle-moodle to send audio from the microphone to the IAX channel.
  sendAudioFromMic();

  // Refresh registration.
  if (_nextRefreshStamp && _nextRefreshStamp - 2 < time(NULL)) {
    sendRegister();
  }

  // thread wait 3 millisecond
  _evThread->sleep(3);
}

  void
IAXVoIPLink::sendAudioFromMic(void)
{
    int maxBytesToGet, availBytesFromMic, bytesAvail, nbSample, compSize;
    AudioCodec *ac;

    IAXCall* currentCall = getIAXCall(Manager::instance().getCurrentCallId());
  
    if (!currentCall) {
        // Let's mind our own business.
        return;
    }

    if( currentCall -> getAudioCodec() < 0 )
        return;

  // Just make sure the currentCall is in state to receive audio right now.
  //_debug("Here we get: connectionState: %d   state: %d \n",
  // currentCall->getConnectionState(),
  // currentCall->getState());

  if (currentCall->getConnectionState() != Call::Connected ||
      currentCall->getState() != Call::Active) {
    return;
  }

  ac = currentCall -> getCodecMap().getCodec( currentCall -> getAudioCodec() );
  if (!ac) {
    // Audio codec still not determined.
    if (audiolayer) {
      // To keep latency low..
      //audiolayer->flushMic();
    }
    return;
  }

  // Send sound here
  if (audiolayer) {

    _debug("hum");
    // we have to get 20ms of data from the mic *20/1000 = /50
    // rate/50 shall be lower than IAX__20S_48KHZ_MAX
    maxBytesToGet = audiolayer->getSampleRate()* audiolayer->getFrameSize() / 1000 * sizeof(SFLDataFormat);

    // available bytes inside ringbuffer
    availBytesFromMic = audiolayer->canGetMic();

    if (availBytesFromMic < maxBytesToGet) {
      // We need packets full!
      return;
    }

    // take the lowest
    bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
    //_debug("available = %d, maxBytesToGet = %d\n", availBytesFromMic, maxBytesToGet);

    // Get bytes from micRingBuffer to data_from_mic
    nbSample = audiolayer->getMic( micData, bytesAvail ) / sizeof(SFLDataFormat);

    // resample
    nbSample = converter->downsampleData( micData , micDataConverted , (int)ac ->getClockRate() ,  (int)audiolayer->getSampleRate() , nbSample );

    // for the mono: range = 0 to IAX_FRAME2SEND * sizeof(int16)
    compSize = ac->codecEncode( micDataEncoded, micDataConverted , nbSample*sizeof(int16));

    // Send it out!
    _mutexIAX.enterMutex();
    // Make sure the session and the call still exists.
    if (currentCall->getSession()) {
      if ( iax_send_voice(currentCall->getSession(), currentCall->getFormat(), micDataEncoded, compSize, nbSample) == -1) {
	_debug("IAX: Error sending voice data.\n");
      }
    }
    _mutexIAX.leaveMutex();
  }
}


  IAXCall* 
IAXVoIPLink::getIAXCall(const CallID& id) 
{
  Call* call = getCall(id);
  if (call) {
    return dynamic_cast<IAXCall*>(call);
  }
  return NULL;
}


 int 
IAXVoIPLink::sendRegister() 
{
  bool result = false;
  if (_host.empty()) {
    return false;
  }
  if (_user.empty()) {
    return false;
  }

  // lock
  _mutexIAX.enterMutex();

  // Always use a brand new session
  if (_regSession) {
    iax_destroy(_regSession);
  }

  _regSession = iax_session_new();

  if (!_regSession) {
    _debug("Error when generating new session for register");
  } else {
    // refresh
    // last reg
    char host[_host.length()+1]; 
    strcpy(host, _host.c_str());
    char user[_user.length()+1];
    strcpy(user, _user.c_str());
    char pass[_pass.length()+1]; 
    strcpy(pass, _pass.c_str());
    // iax_register doesn't use const char*

    _debug("IAX Sending registration to %s with user %s\n", host, user);
    int val = iax_register(_regSession, host, user, pass, 120);
    _debug ("Return value: %d\n", val);
    // set the time-out to 15 seconds, after that, resend a registration request.
    // until we unregister.
    _nextRefreshStamp = time(NULL) + 10;
    result = true;

    setRegistrationState(Trying);
  }

  // unlock
  _mutexIAX.leaveMutex();

  return result;
}



 int 
IAXVoIPLink::sendUnregister()
{
  _mutexIAX.enterMutex();
  if (_regSession) {
    /** @todo Should send a REGREL in sendUnregister()... */

    //iax_send_regrel(); doesn't exist yet :)
    iax_destroy(_regSession);

    _regSession = NULL;
  }
  _mutexIAX.leaveMutex();

  _nextRefreshStamp = 0;

  _debug("IAX2 send unregister\n");
  setRegistrationState(Unregistered);

  return SUCCESS;
}

  Call* 
IAXVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
  IAXCall* call = new IAXCall(id, Call::Outgoing);
  call->setCodecMap(Manager::instance().getCodecDescriptorMap());


  if (call) {
    call->setPeerNumber(toUrl);

    if ( iaxOutgoingInvite(call) ) {
      call->setConnectionState(Call::Progressing);
      call->setState(Call::Active);
      addCall(call);
    } else {
      delete call; call = NULL;
    }
  }
  return call;
}


  bool 
IAXVoIPLink::answer(const CallID& id)
{
  IAXCall* call = getIAXCall(id);
  call->setCodecMap(Manager::instance().getCodecDescriptorMap());

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_answer(call->getSession());
  _mutexIAX.leaveMutex();

  call->setState(Call::Active);
  call->setConnectionState(Call::Connected);
  // Start audio
  audiolayer->startStream();
  //audiolayer->flushMic();

  return true;
}

  bool 
IAXVoIPLink::hangup(const CallID& id)
{
  IAXCall* call = getIAXCall(id);
  std::string reason = "Dumped Call";
  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_hangup(call->getSession(), (char*) reason.c_str());
  _mutexIAX.leaveMutex();
  call->setSession(NULL);
  if (Manager::instance().isCurrentCall(id)) {
    // stop audio
  }
  removeCall(id);
  return true;	
}

  bool 
IAXVoIPLink::onhold(const CallID& id) 
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  //if (call->getState() == Call::Hold) { _debug("Call is already on hold\n"); return false; }

  _mutexIAX.enterMutex();
  iax_quelch_moh(call->getSession() , MUSIC_ONHOLD);
  _mutexIAX.leaveMutex();

  call->setState(Call::Hold);
  return true;
}

  bool 
IAXVoIPLink::offhold(const CallID& id)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  //if (call->getState() == Call::Active) { _debug("Call is already active\n"); return false; }
  _mutexIAX.enterMutex();
  iax_unquelch(call->getSession());
  _mutexIAX.leaveMutex();
  audiolayer->startStream();
  call->setState(Call::Active);
  return true;
}

  bool 
IAXVoIPLink::transfer(const CallID& id, const std::string& to)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  char callto[to.length()+1];
  strcpy(callto, to.c_str());

  _mutexIAX.enterMutex();
  iax_transfer(call->getSession(), callto); 
  _mutexIAX.leaveMutex();

  return true;

  // should we remove it?
  // removeCall(id);
}

  bool 
IAXVoIPLink::refuse(const CallID& id)
{
  IAXCall* call = getIAXCall(id);
  std::string reason = "Call rejected manually.";

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_reject(call->getSession(), (char*) reason.c_str());
  _mutexIAX.leaveMutex();
  removeCall(id);

  return true;
}

  bool
IAXVoIPLink::carryingDTMFdigits(const CallID& id, char code)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_send_dtmf(call->getSession(), code);
  _mutexIAX.leaveMutex();

  return true;
}



  bool
IAXVoIPLink::iaxOutgoingInvite(IAXCall* call) 
{
  struct iax_session *newsession;
  ost::MutexLock m(_mutexIAX);

  newsession = iax_session_new();
  if (!newsession) {
    _debug("IAX Error: Can't make new session for a new call\n");
    return false;
  }
  call->setSession(newsession);
  /* reset activity and ping "timers" */
  // iaxc_note_activity(callNo);

  std::string strNum = _user + ":" + _pass + "@" + _host + "/" + call->getPeerNumber();  

  char user[_user.length()+1];
  strcpy(user, _user.c_str());

  char num[strNum.length()+1];
  strcpy(num, strNum.c_str());

  char* lang = NULL;
  int wait = 0;
  /** @todo Make preference dynamic, and configurable */
  int audio_format_preferred =  call->getFirstMatchingFormat(call->getSupportedFormat());
  int audio_format_capability = call->getSupportedFormat();

  _debug("IAX New call: %s\n", num);
  iax_call(newsession, user, user, num, lang, wait, audio_format_preferred, audio_format_capability);

  return true;
}


  IAXCall* 
IAXVoIPLink::iaxFindCallBySession(struct iax_session* session) 
{
  // access to callMap shoud use that
  // the code below is like findSIPCallWithCid() 
  ost::MutexLock m(_callMapMutex);	
  IAXCall* call = NULL;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<IAXCall*>(iter->second);
    if (call && call->getSession() == session) {
      return call;
    }
    iter++;
  }
  return NULL; // not found
}

  void
IAXVoIPLink::iaxHandleCallEvent(iax_event* event, IAXCall* call) 
{
  // call should not be 0
  // note activity?
  //
  CallID id = call->getCallId();

  switch(event->etype) {
    case IAX_EVENT_HANGUP:
      Manager::instance().peerHungupCall(id); 
      if (Manager::instance().isCurrentCall(id)) {
	audiolayer->stopStream();
	// stop audio
      }
      removeCall(id);
      break;

    case IAX_EVENT_REJECT:
      //Manager::instance().peerHungupCall(id); 
      if (Manager::instance().isCurrentCall(id)) {
	// stop audio
	audiolayer->stopStream();
      }
      call->setConnectionState(Call::Connected);
      call->setState(Call::Error);
      Manager::instance().callFailure(id);
      removeCall(id);
      break;

    case IAX_EVENT_ACCEPT:
      // Call accepted over there by the computer, not the user yet.
      if (event->ies.format) {
	call->setFormat(event->ies.format);
      }
      break;

    case IAX_EVENT_ANSWER:
      if (call->getConnectionState() != Call::Connected){
	call->setConnectionState(Call::Connected);
	call->setState(Call::Active);

	if (event->ies.format) {
	  // Should not get here, should have been set in EVENT_ACCEPT
	  call->setFormat(event->ies.format);
	}

	Manager::instance().peerAnsweredCall(id);
	//audiolayer->flushMic();
	//audiolayer->startStream();
	// start audio here?
      } else {
	// deja connecté ?
      }
      break;

    case IAX_EVENT_BUSY:
      call->setConnectionState(Call::Connected);
      call->setState(Call::Busy);
      Manager::instance().callBusy(id);
      removeCall(id);
      break;

    case IAX_EVENT_VOICE:
      //_debug("Should have a decent value!!!!!! = %i\n" , call -> getAudioCodec());
      //if( !audiolayer -> isCaptureActive())
	//audiolayer->startStream();
      iaxHandleVoiceEvent(event, call);
      break;

    case IAX_EVENT_TEXT:
      break;

    case IAX_EVENT_RINGA:
      call->setConnectionState(Call::Ringing);
      Manager::instance().peerRingingCall(call->getCallId());
      break;

    case IAX_IE_MSGCOUNT:	
      break;
    case IAX_EVENT_PONG:
      break;

    case IAX_EVENT_URL:
      break;

      //    case IAX_EVENT_CNG: ??
      //    break;

    case IAX_EVENT_TIMEOUT:
      break;

    case IAX_EVENT_TRANSFER:
      break;

    default:
      _debug("Unknown event type (in call event): %d\n", event->etype);

  }
}


/* Handle audio event, VOICE packet received */
  void
IAXVoIPLink::iaxHandleVoiceEvent(iax_event* event, IAXCall* call)
{ 


    unsigned char *data;
    unsigned int size, max, nbInt16;
    int expandedSize, nbSample;    
    AudioCodec *ac;

  // If we receive datalen == 0, some things of the jitter buffer in libiax2/iax.c
  // were triggered
  if (!event->datalen) {
    // Skip this empty packet.
    //_debug("IAX: Skipping empty jitter-buffer interpolated packet\n");
    return;
  }

  if (audiolayer) {
    // On-the-fly codec changing (normally, when we receive a full packet)
    // as per http://tools.ietf.org/id/draft-guy-iax-03.txt
    // - subclass holds the voiceformat property.
    if (event->subclass && event->subclass != call->getFormat()) {
      call->setFormat(event->subclass);
    }
    //_debug("Receive: len=%d, format=%d, _receiveDataDecoded=%p\n", event->datalen, call->getFormat(), _receiveDataDecoded);
    ac = call->getCodecMap().getCodec( call -> getAudioCodec() );

    data = (unsigned char*)event->data;
    size   = event->datalen;

    // Decode data with relevant codec
    max = (int)( ac->getClockRate() * audiolayer->getFrameSize() / 1000 );

    if (size > max) {
      _debug("The size %d is bigger than expected %d. Packet cropped. Ouch!\n", size, max);
      size = max;
    }

    expandedSize = ac->codecDecode( spkrDataDecoded , data , size );
    nbInt16      = expandedSize/sizeof(int16);

    if (nbInt16 > max) {
      _debug("We have decoded an IAX VOICE packet larger than expected: %i VS %i. Cropping.\n", nbInt16, max);
      nbInt16 = max;
    }

    nbSample = nbInt16;
    // resample
    nbInt16 = converter->upsampleData( spkrDataDecoded , spkrDataConverted , ac->getClockRate() , audiolayer->getSampleRate() , nbSample);

    audiolayer->playSamples( spkrDataConverted , nbInt16 * sizeof(SFLDataFormat), true);

  } else {
    _debug("IAX: incoming audio, but no sound card open");
  }

}

/**
 * Handle the registration process
 */
  void
IAXVoIPLink::iaxHandleRegReply(iax_event* event) 
{

    int voicemail;
    std::string account_id;
 
    if (event->etype == IAX_EVENT_REGREJ) {
        /* Authentication failed! */
        _mutexIAX.enterMutex();
        iax_destroy(_regSession);
        _mutexIAX.leaveMutex();
        _regSession = NULL;
        setRegistrationState(ErrorAuth);
    }
    
    else if (event->etype == IAX_EVENT_REGACK) {
        /* Authentication succeeded */
        _mutexIAX.enterMutex();

        // Looking for the voicemail information
        //if( event->ies != 0 )        
        voicemail = event->ies.msgcount;
        _debug("iax voicemail number notification: %i\n", voicemail);
        // Notify the client if new voicemail waiting for the current account
	account_id = getAccountID();
        Manager::instance().startVoiceMessageNotification(account_id.c_str(), voicemail);

        iax_destroy(_regSession);
        _mutexIAX.leaveMutex();
        _regSession = NULL;

        // I mean, save the timestamp, so that we re-register again in the REFRESH time.
        // Defaults to 60, as per draft-guy-iax-03.
        _nextRefreshStamp = time(NULL) + (event->ies.refresh ? event->ies.refresh : 60);
        setRegistrationState(Registered);
    }
}

  void
IAXVoIPLink::iaxHandlePrecallEvent(iax_event* event)
{
  IAXCall* call = NULL;
  CallID   id;
  std::string reason = "Error ringing user.";

  switch(event->etype) {
    case IAX_EVENT_REGACK:
    case IAX_EVENT_REGREJ:
      _debug("IAX Registration Event in a pre-call setup\n");
      break;

    case IAX_EVENT_REGREQ:
      // Received when someone wants to register to us!?!
      // Asterisk receives and answers to that, not us, we're a phone.
      _debug("Registration by a peer, don't allow it\n");
      break;

    case IAX_EVENT_CONNECT:
      // We've got an incoming call! Yikes!
      _debug("> IAX_EVENT_CONNECT (receive)\n");

      id = Manager::instance().getNewCallID();


      call = new IAXCall(id, Call::Incoming);

      if (!call) {
	_debug("! IAX Failure: unable to create an incoming call");
	return;
      }

      // Setup the new IAXCall
      // Associate the call to the session.
      call->setSession(event->session);

      // setCallAudioLocal(call);
      call->setCodecMap(Manager::instance().getCodecDescriptorMap());
      call->setConnectionState(Call::Progressing);


      if (event->ies.calling_number)
	call->setPeerNumber(std::string(event->ies.calling_number));
      if (event->ies.calling_name)
	call->setPeerName(std::string(event->ies.calling_name));

      if (Manager::instance().incomingCall(call, getAccountID())) {
	/** @todo Faudra considérer éventuellement le champ CODEC PREFS pour
	 * l'établissement du codec de transmission */

	// Remote lists its capabilities
	int format = call->getFirstMatchingFormat(event->ies.capability);
	// Remote asks for preferred codec voiceformat
	int pref_format = call->getFirstMatchingFormat(event->ies.format);

	// Priority to remote's suggestion. In case it's a forwarding, no transcoding
	// will be needed from the server, thus less latency.
	if (pref_format)
	  format = pref_format;

	iax_accept(event->session, format);
	iax_ring_announce(event->session);

	addCall(call);
      } else {
	// reject call, unable to add it
	iax_reject(event->session, (char*)reason.c_str());

	delete call; call = NULL;
      }

      break;

    case IAX_EVENT_HANGUP:
      // Remote peer hung up
      call = iaxFindCallBySession(event->session);
      id = call->getCallId();

      Manager::instance().peerHungupCall(id);
      removeCall(id);
      break;

    case IAX_EVENT_TIMEOUT: // timeout for an unknown session

      break;

    case IAX_IE_MSGCOUNT:	
      //_debug("messssssssssssssssssssssssssssssssssssssssssssssssages\n");
      break;

    default:
      _debug("Unknown event type (in precall): %d\n", event->etype);
  }

}

  int 
IAXVoIPLink::iaxCodecMapToFormat(IAXCall* call)
{
  CodecOrder map = call->getCodecMap().getActiveCodecs();
  printf("taytciatcia = %i\n", map.size());
  return 0;
}


