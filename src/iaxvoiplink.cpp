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

#include <samplerate.h>
#include <iax/iax-client.h>
#include <math.h>
#include <dlfcn.h>


#define IAX_BLOCKING    1
#define IAX_NONBLOCKING 0

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

#define RANDOM_IAX_PORT   rand() % 64000 + 1024

// from IAXC : iaxclient.h

#define IAX__20S_8KHZ_MAX   320 //320 samples, IAX packets can have more than 20ms.
#define IAX__20S_48KHZ_MAX  1920 // 320*6 samples = 1920, 6 = 48000/8000 

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
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  IAXCall *call;
  while( iter != _callMap.end() ) {
    call = dynamic_cast<IAXCall*>(iter->second);
    if (call) {
      _mutexIAX.enterMutex();
      iax_hangup(call->getSession(), "Dumped Call");
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

AudioCodec* 
IAXVoIPLink::loadCodec(int payload)
{
   using std::cerr;

   switch(payload)
   {
     case 0:
       handle_codec = dlopen(CODECS_DIR "/libcodec_ulaw.so", RTLD_LAZY);
       break;
     case 3:
       handle_codec = dlopen(CODECS_DIR "/libcodec_gsm.so", RTLD_LAZY);
       break;
     case 8:
       handle_codec = dlopen(CODECS_DIR "/libcodec_alaw.so", RTLD_LAZY);
       break;
     case 97:
       handle_codec = dlopen(CODECS_DIR "/libcodec_ilbc.so", RTLD_LAZY);
       break;
     case 110:
       handle_codec = dlopen(CODECS_DIR "/libcodec_speex.so", RTLD_LAZY);
       break;
   }
   if(!handle_codec){
        cerr<<"cannot load library: "<< dlerror() <<'\n';
   }
   // reset errors
   dlerror();   

   // load the symbols
  create_t* create_codec = (create_t*)dlsym(handle_codec, "create");
  const char* dlsym_error = dlerror();
  if(dlsym_error){
        cerr << "Cannot load symbol create: " << dlsym_error << '\n';
  }
  return create_codec();
}

void
IAXVoIPLink::unloadCodec(AudioCodec* audiocodec)
{
  using std::cerr;

  destroy_t* destroy_codec = (destroy_t*) dlsym(handle_codec, "destroy");
  const char* dlsym_error = dlerror();
  if(dlsym_error){
       cerr << "Cannot load symbol destroy" << dlsym_error << '\n';
  }
  destroy_codec(audiocodec);
  dlclose(handle_codec);
}

void
IAXVoIPLink::sendAudioFromMic(void)
{
  
  IAXCall* currentCall = getIAXCall(Manager::instance().getCurrentCallId());

  if (!currentCall) {
    // Let's mind our own business.
    return;
  }

  // Just make sure the currentCall is in state to receive audio right now.
  //_debug("Here we get: connectionState: %d   state: %d \n",
  // currentCall->getConnectionState(),
  // currentCall->getState());

  if (currentCall->getConnectionState() != Call::Connected ||
      currentCall->getState() != Call::Active) {
    return;
  }

  AudioCodec* audiocodec = loadCodec(currentCall->getAudioCodec());
  if (!audiocodec) {
    // Audio codec still not determined.
    if (audiolayer) {
      // To keep latency low..
      //audiolayer->flushMic();
    }
    return;
  }

  // Send sound here
  if (audiolayer) {

    // we have to get 20ms of data from the mic *20/1000 = /50
    // rate/50 shall be lower than IAX__20S_48KHZ_MAX
    int maxBytesToGet = audiolayer->getSampleRate()/50*sizeof(SFLDataFormat);

    // available bytes inside ringbuffer
    int availBytesFromMic = audiolayer->canGetMic();

    if (availBytesFromMic < maxBytesToGet) {
      // We need packets full!
      return;
    }

    // take the lowest
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
    //_debug("available = %d, maxBytesToGet = %d\n", availBytesFromMic, maxBytesToGet);
    
    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = audiolayer->getMic(_dataAudioLayer, bytesAvail) / sizeof(SFLDataFormat);

    // Audio ici est PARFAIT

    int16* toIAX = NULL;
    //if (audiolayer->getSampleRate() != audiocodec->getClockRate() && nbSample) {
    if (audiolayer->getSampleRate() != audiocodec->getClockRate() && nbSample) {
      SRC_DATA src_data;
#ifdef DATAFORMAT_IS_FLOAT   
      src_data.data_in = _dataAudioLayer;
#else
      src_short_to_float_array(_dataAudioLayer, _floatBuffer48000, nbSample);
      src_data.data_in = _floatBuffer48000; 
#endif
      
      // Audio parfait à ce point.

      double factord = (double) audiocodec->getClockRate() / audiolayer->getSampleRate();
      
      src_data.src_ratio = factord;
      src_data.input_frames = nbSample;
      src_data.output_frames = (int) floor(factord * nbSample);
      src_data.data_out = _floatBuffer8000;
      src_data.end_of_input = 0; 
      
      src_process(_src_state_mic, &src_data);
      
      nbSample = src_data.output_frames_gen;

      // Bon, l'audio en float 8000 est laid mais yé consistant.

      src_float_to_short_array (_floatBuffer8000, _intBuffer8000, nbSample);
      toIAX = _intBuffer8000;

      // Audio bon ici aussi..

    } else {
#ifdef DATAFORMAT_IS_FLOAT
      // convert _receiveDataDecoded to float inside _receiveData
      src_float_to_short_array(_dataAudioLayer, _intBuffer8000, nbSample);
      toIAX = _intBuffer8000;
      //if (nbSample > IAX__20S_8KHZ_MAX) { _debug("Alert from mic, nbSample %d is bigger than expected %d\n", nbSample, IAX__20S_8KHZ_MAX); }
#else
      toIAX = _dataAudioLayer; // int to int
#endif
    }

    // NOTE: L'audio ici est bon.

    //
    // LE PROBLÈME est dans cette snippet de fonction:
    // C'est une fonction destructrice ! On n'en veut pas!
    //if ( nbSample < (IAX__20S_8KHZ_MAX - 10) ) { // if only 10 is missing, it's ok
      // fill end with 0...
      //_debug("begin: %p, nbSample: %d\n", toIAX, nbSample);
      //_debug("has to fill: %d chars at %p\n", (IAX__20S_8KHZ_MAX-nbSample)*sizeof(int16), toIAX + nbSample);
      //memset(toIAX + nbSample, 0, (IAX__20S_8KHZ_MAX-nbSample)*sizeof(int16));
      //nbSample = IAX__20S_8KHZ_MAX;
    //}
    
    //_debug("AR: Nb sample: %d int, [0]=%d [1]=%d [2]=%d\n", nbSample, toIAX[0], toIAX[1], toIAX[2]);
    // NOTE: Le son dans toIAX (nbSamle*sizeof(int16)) est mauvais,
    // s'il passe par le snippet précédent.


    // DEBUG
    //_fstream.write((char *) toIAX, nbSample*sizeof(int16));
    //_fstream.flush();


    // for the mono: range = 0 to IAX_FRAME2SEND * sizeof(int16)
    int compSize = audiocodec->codecEncode(_sendDataEncoded, toIAX, nbSample*sizeof(int16));

      


    // Send it out!
    _mutexIAX.enterMutex();
    // Make sure the session and the call still exists.
    if (currentCall->getSession()) {
      if ( iax_send_voice(currentCall->getSession(), currentCall->getFormat(), (unsigned char*)_sendDataEncoded, compSize, nbSample) == -1) {
	_debug("IAX: Error sending voice data.\n");
      }
    }
    _mutexIAX.leaveMutex();
  }
  unloadCodec(audiocodec);
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



bool
IAXVoIPLink::sendRegister() 
{
  bool result = false;

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




bool
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

  setRegistrationState(Unregistered);

  return false;
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
      audiolayer->stopStream();
      // stop audio
    }
    removeCall(id);
    break;
    
  case IAX_EVENT_REJECT:
    Manager::instance().peerHungupCall(id); 
    if (Manager::instance().isCurrentCall(id)) {
      // stop audio
      audiolayer->stopStream();
    }
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
      audiolayer->startStream();
      // start audio here?
    } else {
      // deja connecté ?
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
    iaxHandleVoiceEvent(event, call);
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
    _debug("Unknown event type (in call event): %d\n", event->etype);
    
  }
}


/* Handle audio event, VOICE packet received */
void
IAXVoIPLink::iaxHandleVoiceEvent(iax_event* event, IAXCall* call)
{ 
    // If we receive datalen == 0, some things of the jitter buffer in libiax2/iax.c
    // were triggered
    if (!event->datalen) {
      // Skip this empty packet.
      //_debug("IAX: Skipping empty jitter-buffer interpolated packet\n");
      return;
    }
AudioCodec* audiocodec;

    if (audiolayer) {
      
      // On-the-fly codec changing (normally, when we receive a full packet)
      // as per http://tools.ietf.org/id/draft-guy-iax-03.txt
      // - subclass holds the voiceformat property.
      if (event->subclass && event->subclass != call->getFormat()) {
	call->setFormat(event->subclass);
      }
      audiocodec = loadCodec(call->getAudioCodec());
      //_debug("Receive: len=%d, format=%d, _receiveDataDecoded=%p\n", event->datalen, call->getFormat(), _receiveDataDecoded);
     
      unsigned char* data = (unsigned char*)event->data;
      unsigned int size   = event->datalen;

      if (size > IAX__20S_8KHZ_MAX) {
	_debug("The size %d is bigger than expected %d. Packet cropped. Ouch!\n", size, IAX__20S_8KHZ_MAX);
	size = IAX__20S_8KHZ_MAX;
      }

      int expandedSize = audiocodec->codecDecode(_receiveDataDecoded, data, size);
      int nbInt16      = expandedSize/sizeof(int16);

      if (nbInt16 > IAX__20S_8KHZ_MAX) {
	_debug("We have decoded an IAX VOICE packet larger than expected: %s VS %s. Cropping.\n", nbInt16, IAX__20S_8KHZ_MAX);
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
	src_data.end_of_input = 0;
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
      audiolayer->playSamples(toAudioLayer, nbSample * sizeof(SFLDataFormat));
    } else {
      _debug("IAX: incoming audio, but no sound card open");
    }
  unloadCodec(audiocodec);

}


/**
 * Handle the registration process
 */
void
IAXVoIPLink::iaxHandleRegReply(iax_event* event) 
{
  if (event->etype == IAX_EVENT_REGREJ) {
    /* Authentication failed! */
    _mutexIAX.enterMutex();
    iax_destroy(_regSession);
    _mutexIAX.leaveMutex();
    _regSession = NULL;

    setRegistrationState(Error, "Registration failed");
    //Manager::instance().registrationFailed(getAccountID());

  }
  else if (event->etype == IAX_EVENT_REGACK) {
    /* Authentication succeeded */
    _mutexIAX.enterMutex();
    iax_destroy(_regSession);
    _mutexIAX.leaveMutex();
    _regSession = NULL;

    // I mean, save the timestamp, so that we re-register again in the REFRESH time.
    // Defaults to 60, as per draft-guy-iax-03.
    _nextRefreshStamp = time(NULL) + (event->ies.refresh ? event->ies.refresh : 60);

    setRegistrationState(Registered);
    //Manager::instance().registrationSucceed(getAccountID());
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


