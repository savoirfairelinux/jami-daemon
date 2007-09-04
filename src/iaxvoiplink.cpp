/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
#include "audio/audiocodec.h"
#include "audio/audiolayer.h"

#include <samplerate.h>
#include <iax/iax-client.h>
#include <math.h>


#define IAX_BLOCKING    1
#define IAX_NONBLOCKING 0

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

#define RANDOM_IAX_PORT   rand() % 64000 + 1024

// from IAXC : iaxclient.h

#define IAX__20S_8KHZ_MAX   320 // 320 samples
#define IAX__20S_48KHZ_MAX  1920 // 320*6 samples, 6 = 48000/8000

#define CHK_VALID_CALL   if (call == NULL) { _debug("IAX: Call doesn't exists\n"); \
                                             return false; }



IAXVoIPLink::IAXVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID)
{
  _evThread = new EventThread(this);
  _regSession = NULL;

  // to get random number for RANDOM_PORT
  srand (time(NULL));

  audiolayer = NULL;

  _receiveDataDecoded = new int16[IAX__20S_48KHZ_MAX];
  _sendDataEncoded   =  new unsigned char[IAX__20S_8KHZ_MAX];

  // we estimate that the number of format after a conversion 8000->48000 is expanded to 6 times
  _dataAudioLayer = new SFLDataFormat[IAX__20S_48KHZ_MAX];
  _floatBuffer8000  = new float32[IAX__20S_8KHZ_MAX];
  _floatBuffer48000 = new float32[IAX__20S_48KHZ_MAX];
  _intBuffer8000  = new int16[IAX__20S_8KHZ_MAX];

  // libsamplerate-related
  _src_state_mic  = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);
  _src_state_spkr = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);

}


IAXVoIPLink::~IAXVoIPLink()
{
  delete _evThread; _evThread = NULL;
  _regSession = NULL; // shall not delete it
  terminate();

  audiolayer = NULL;
  delete [] _intBuffer8000; _intBuffer8000 = NULL;
  delete [] _floatBuffer48000; _floatBuffer48000 = NULL;
  delete [] _floatBuffer8000; _floatBuffer8000 = NULL;
  delete [] _dataAudioLayer; _dataAudioLayer = NULL;

  delete [] _sendDataEncoded; _sendDataEncoded = NULL;
  delete [] _receiveDataDecoded; _receiveDataDecoded = NULL;

  // libsamplerate-related
  _src_state_mic  = src_delete(_src_state_mic);
  _src_state_spkr = src_delete(_src_state_spkr);
}

bool
IAXVoIPLink::init()
{
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
  }
  if (port == IAX_FAILURE || nbTry==0) {
    _debug("Fail to initialize iax\n");
  }
  return returnValue;
}

void
IAXVoIPLink::terminate()
{
//  iaxc_shutdown();  
//  hangup all call
    terminateIAXCall();
//  iax_hangup(calls[callNo].session,"Dumped Call");
}

void
IAXVoIPLink::terminateIAXCall()
{
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  IAXCall *call;
  while( iter != _callMap.end() ) {
    call = dynamic_cast<IAXCall*>(iter->second);
    if (call) {
      _mutexIAX.enterMutex();
      iax_hangup(call->getSession(),"Dumped Call");
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
  // mutex here
  _mutexIAX.enterMutex();

  iax_event* event = NULL;
  IAXCall* call = NULL;
  while ( (event = iax_get_event(IAX_NONBLOCKING)) != NULL ) {

    // If we received an 'ACK', libiax2 tells apps to ignore them.
    if (event->etype == IAX_EVENT_NULL) {
      continue;
    }

    _debug ("Receive IAX Event: %d (0x%x)\n", event->etype, event->etype);

    call = iaxFindCallBySession(event->session);

    if (call) {

      _debug("  - We've got an associated call, handle call event\n");
      iaxHandleCallEvent(event, call);

    } else if (event->session && event->session == _regSession) {

      _debug("  - We've got an associated REGISTRATION session, handle registration process\n");
      // in iaxclient, there is many session handling, here, only one
      iaxHandleRegReply(event);

    } else {

      _debug ("  - We've got some other event, deal with them alone.\n");
      iaxHandlePrecallEvent(event);

    }

    iax_event_free(event);
  }

  // Woah, we should do that in another thread, which will always send out stuff..
  // send sound here
  if(_currentCall && audiolayer) {
    int samples = audiolayer->canGetMic(); 
    if (samples != 0) {
      //int  datalen = audiolayer->getMic(_sendDataEncoded, samples);
      //_debug("iax_send_voice(%p, %d, ,%d, %d)\n", _currentCall->getSession(), _currentCall->getFormat(), datalen, samples);
      //if ( iax_send_voice(_currentCall->getSession(), _currentCall->getFormat(), (char*)_sendDataEncoded, datalen, samples) == -1) {
      //	   // error sending voice
      //}
    }
  }

  // unlock mutex here
  _mutexIAX.leaveMutex();
  //iaxRefreshRegistrations();

  // thread wait 5 millisecond
  _evThread->sleep(5);
}





IAXCall* 
IAXVoIPLink::getIAXCall(const CallID& id) 
{
  Call* call = getCall(id);
  if (call) {
    return dynamic_cast<IAXCall*>(call);
  }
  return 0;
}



bool
IAXVoIPLink::setRegister() 
{
  bool result = false;
  if (_regSession == NULL) {
    if (_host.empty()) {
      Manager::instance().displayConfigError("Fill host field for IAX Account");
      return false;
    }
    if (_user.empty()) {
      Manager::instance().displayConfigError("Fill user field for IAX Account");
      return false;
    }

    // lock
    _mutexIAX.enterMutex();
    _regSession = iax_session_new();

    if (!_regSession) {
      _debug("error when generating new session for register");
    } else {
      // refresh
      // last reg
      char host[_host.length()+1]; 
      strcpy(host, _host.c_str());
      char user[_user.length()+1];
      strcpy(user, _user.c_str());
      char pass[_pass.length()+1]; 
      strcpy(pass, _pass.c_str());
      //iax_register don't use const char*

      _debug("IAX Sending registration to %s with user %s\n", host, user);
      int val = iax_register(_regSession, host, user, pass, 300);
      _debug ("Return value: %d\n", val);
      result = true;
    }

    _mutexIAX.leaveMutex();
  }
  return result;
}




bool
IAXVoIPLink::setUnregister()
{
  if (_regSession) {
    // lock here
    _mutexIAX.enterMutex();
    iax_destroy(_regSession);
    _regSession = NULL;
    // unlock here
    _mutexIAX.leaveMutex();
    return false;
  }
  return false;
}

Call* 
IAXVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
  IAXCall* call = new IAXCall(id, Call::Outgoing);

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
  
  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_answer(call->getSession());
  _mutexIAX.leaveMutex();

  call->setState(Call::Active);
  call->setConnectionState(Call::Connected);

  return true;
}

bool 
IAXVoIPLink::hangup(const CallID& id)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_hangup(call->getSession(), "Dumped Call");
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
  iax_quelch(call->getSession());
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

  // should we remove it?
  // removeCall(id);
}

bool 
IAXVoIPLink::refuse(const CallID& id)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_reject(call->getSession(), "Call rejected manually.");
  _mutexIAX.leaveMutex();
  removeCall(id);
}

bool
IAXVoIPLink::carryingDTMFdigits(const CallID& id, char code)
{
  IAXCall* call = getIAXCall(id);

  CHK_VALID_CALL;

  _mutexIAX.enterMutex();
  iax_send_dtmf(call->getSession(), code);
  _mutexIAX.leaveMutex();
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
  int16* output = 0; // for audio output
  
  switch(event->etype) {
  case IAX_EVENT_HANGUP:
    Manager::instance().peerHungupCall(id); 
    if (Manager::instance().isCurrentCall(id)) {
      _currentCall = 0;
      audiolayer->stopStream();
      // stop audio
    }
    removeCall(id);
    break;
    
  case IAX_EVENT_REJECT:
    Manager::instance().peerHungupCall(id); 
    if (Manager::instance().isCurrentCall(id)) {
      // stop audio
      _currentCall = 0;
      audiolayer->stopStream();
    }
    removeCall(id);
    break;

  case IAX_EVENT_ACCEPT:
    // accept
    // 

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
      _currentCall = call;
      audiolayer->startStream();
      // start audio here?
    } else {
      // deja connecté
      // ?
    }
    break;
    
  case IAX_EVENT_BUSY:
    call->setConnectionState(Call::Connected);
    call->setState(Call::Busy);
    Manager::instance().displayErrorText(id, "Busy");
    Manager::instance().callBusy(id);
    removeCall(id);
    break;
    
  case IAX_EVENT_VOICE:
    if (audiolayer) {
      AudioCodec* audiocodec = call->getAudioCodec();
      
      if (!audiocodec) {
	// libiax2 stores the voiceformat in the 'subclass' element.
	if (event->subclass) {
	  // Set the format, with the first voice packet
	  call->setFormat(event->subclass);
	  audiocodec = call->getAudioCodec();
	}  else {
	  // Send a VNAK, because they sent a Mini packet before
	  // a full VOICE packet (with the format inside)
	  _debug("IAX: sending VNAK, received mini packet before full VOICE packet.");
	  iax_vnak(event->session);
	  break;
	}
      }
	

      _debug("Receive: len=%d, format=%d, _receiveDataDecoded=%p\n", event->datalen, call->getFormat(), _receiveDataDecoded);
      unsigned char* data = (unsigned char*)event->data;
      unsigned int size   = event->datalen;

      if (size > IAX__20S_8KHZ_MAX) {
	_debug("The size %d is bigger than expected %d. Packet cropped. Ouch!\n", size, IAX__20S_8KHZ_MAX);
	size = IAX__20S_8KHZ_MAX;
      }

      // On pourrait ajuster le codec dynamiquement ici, comme c'est fait dans SIP.
      // à moins que IAX ne permette pas de changer le codec à chaque paquet.

      int expandedSize = audiocodec->codecDecode(_receiveDataDecoded, data, size);
      int nbInt16      = expandedSize/sizeof(int16);

      if (nbInt16 > IAX__20S_8KHZ_MAX) {
	_debug("We have decoded a IAX VOICE packet larger than expected: %s VS %s. Cropping.\n", nbInt16, IAX__20S_8KHZ_MAX);
	nbInt16 = IAX__20S_8KHZ_MAX;
      }
      
      SFLDataFormat* toAudioLayer;
      int nbSample = nbInt16;
      int nbSampleMaxRate = nbInt16 * 6;
      
      if ( audiolayer->getSampleRate() != audiocodec->getClockRate() && nbSample ) {
	// Do sample rate conversion
	double factord = (double) audiolayer->getSampleRate() / audiocodec->getClockRate();
	// SRC_DATA from samplerate.h
	SRC_DATA src_data;
	src_data.data_in = _floatBuffer8000;
	src_data.data_out = _floatBuffer48000;
	src_data.input_frames = nbSample;
	src_data.output_frames = (int) floor(factord * nbSample);
	src_data.src_ratio = factord;
	src_data.end_of_input = 0; /* More data will come */
	src_short_to_float_array(_receiveDataDecoded, _floatBuffer8000, nbSample);

	// samplerate convert, go!
	src_process(_src_state_spkr, &src_data);
	
	nbSample = ( src_data.output_frames_gen > IAX__20S_48KHZ_MAX) ? IAX__20S_48KHZ_MAX : src_data.output_frames_gen;
#ifdef DATAFORMAT_IS_FLOAT
	toAudioLayer = _floatBuffer48000;
#else
	src_float_to_short_array(_floatBuffer48000, _dataAudioLayer, nbSample);
	toAudioLayer = _dataAudioLayer;
#endif
  	
      } else {
	nbSample = nbInt16;
#ifdef DATAFORMAT_IS_FLOAT
	// convert _receiveDataDecoded to float inside _receiveData
	src_short_to_float_array(_receiveDataDecoded, _floatBuffer8000, nbSample);
	toAudioLayer = _floatBuffer8000;
#else
	toAudioLayer = _receiveDataDecoded; // int to int
#endif
      }
      audiolayer->putMain(toAudioLayer, nbSample * sizeof(SFLDataFormat));
    } else {
      _debug("IAX: incoming audio, but no sound card open");
    }
    break;
    
  case IAX_EVENT_TEXT:
    break;
    
  case IAX_EVENT_RINGA:
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
    _debug("Unknown event type: %d\n", event->etype);
    
  }
}


/**
 * Handle the registration process
 */
void
IAXVoIPLink::iaxHandleRegReply(iax_event* event) 
{
  if (event->etype == IAX_EVENT_REGREJ) {
    /* Authentication failed! */
    iax_destroy(_regSession);
    _regSession = NULL;
    Manager::instance().registrationFailed(getAccountID());

  }
  else if (event->etype == IAX_EVENT_REGACK) {
    /* Authentication succeeded */
    Manager::instance().registrationSucceed(getAccountID());
  }
}



void
IAXVoIPLink::iaxHandlePrecallEvent(iax_event* event)
{
  IAXCall* call = NULL;
  CallID   id;

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
      iax_reject(event->session, "Error ringing user.");

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
    
  default:
    _debug("Unknown event type: %d\n", event->etype);
  }
  
}

