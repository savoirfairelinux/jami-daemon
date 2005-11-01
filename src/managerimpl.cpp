/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <errno.h>
#include <time.h>

#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>	// mkdir(2)

#include <cc++/thread.h>
#include <cc++/file.h>

#include <cstdlib> 
#include <iostream>
#include <fstream> 
#include <string>
#include <vector>

#include "sipvoiplink.h"
#include "manager.h"
#include "audio/audiocodec.h"
#include "audio/audiolayer.h"
#include "audio/ringbuffer.h"
#include "audio/tonegenerator.h"
#include "audio/tonelist.h"

#include "call.h"
//#include "error.h"
#include "user_cfg.h"
#include "voIPLink.h" 
#include "gui/guiframework.h"

#ifdef USE_ZEROCONF
#include "zeroconf/DNSService.h"
#include "zeroconf/DNSServiceTXTRecord.h"
#endif

#define fill_config_str(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_str)))
#define fill_config_int(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_int)))

#define DFT_VOIP_LINK 0

ManagerImpl::ManagerImpl (void)
{
  // Init private variables 
  //_error = new Error();

  _hasZeroconf = false;
#ifdef USE_ZEROCONF
  _hasZeroconf = true;
  _DNSService = new DNSService();
#endif

  // setup
  _path = ""; 
  _exist = 0;
  _setupLoaded = false;
  _gui = NULL;

  // SOUND:
  _codecMap = CodecDescriptorMap().getMap();
  _audiodriverPA = NULL;

  // Initialize after by init() -> initVolume()
  _spkr_volume = 0;
  _mic_volume  = 0; 
  _mic_volume_before_mute = 0;

  _tone = new ToneGenerator();	
  _toneType = ZT_TONE_NULL;

  // Call
  _currentCallId = 0;
  _nbIncomingWaitingCall=0;
  _registerState = UNREGISTERED;
  _hasTriedToRegister = false;
  // initialize random generator for call id
  srand (time(NULL));
}

// never call if we use only the singleton...
ManagerImpl::~ManagerImpl (void) 
{
  terminate();
  delete _tone;  _tone = NULL;

#ifdef USE_ZEROCONF
  delete _DNSService; _DNSService = NULL;
#endif

  //delete _error; _error = NULL;

  _debug("%s stop correctly.\n", PROGNAME);
}

void 
ManagerImpl::init() 
{
  initVolume();

  if (_exist == 0) {
    _debug("Cannot create config file in your home directory\n");
  }

  _debugInit("Load Telephone Tone");
  std::string country = getConfigString(PREFERENCES, ZONE_TONE);
  _telephoneTone = new TelephoneTone(country);

  try {
    selectAudioDriver();
  }
  catch (const portaudio::PaException &e) {
      getAudioDriver()->setErrorMessage(e.paErrorText());
  }
  catch (const portaudio::PaCppException &e) {
      getAudioDriver()->setErrorMessage(e.what());
  }
  catch (const std::runtime_error &e) {
      getAudioDriver()->setErrorMessage(e.what());
  }
  catch (...) {
      displayError("An unknown exception occured while selecting audio driver.");
      throw;
  }
  initAudioCodec();

  _debugInit("Adding new VoIP Link");
  // Set a sip voip link by default
  _voIPLinkVector.push_back(new SipVoIPLink());

  // initRegisterVoIP was here, but we doing it after the gui loaded... 
  // the stun detection is long, so it's a better idea to do it after getEvents
  initZeroconf();
}

void ManagerImpl::terminate()
{
  saveConfig();

  _debug("Removing VoIP Links...\n");
  for(VoIPLinkVector::iterator pos = _voIPLinkVector.begin();
      pos != _voIPLinkVector.end();
      pos++) {
    delete *pos;
    *pos = NULL;
  }
  _voIPLinkVector.clear();

  _debug("Removing calls\n");
  _mutex.enterMutex();
  for(CallVector::iterator pos = _callVector.begin();
      pos != _callVector.end();
      pos++) {
    delete *pos;   *pos = NULL;
  }
  _callVector.clear();
  _mutex.leaveMutex();

  unloadAudioCodec();

  _debug("Unload Audio Driver\n");
  delete _audiodriverPA; _audiodriverPA = NULL;

  _debug("Unload Telephone Tone\n");
  delete _telephoneTone; _telephoneTone = 0;
}

void
ManagerImpl::setGui (GuiFramework* gui)
{
	_gui = gui;
}

/**
 * Multi Thread with _mutex for callVector
 */
Call *
ManagerImpl::pushBackNewCall (CALLID id, enum CallType type)
{
  ost::MutexLock m(_mutex);
  Call* call = new Call(id, type, _voIPLinkVector.at(DFT_VOIP_LINK));
  // Set the wanted voip-link (first of the list)
  _callVector.push_back(call);
  return call;
}

/**
 * Multi Thread with _mutex for callVector
 */
Call*
ManagerImpl::getCall (CALLID id)
{
  _debug("CALL: Getting call %d\n", id);
  Call* call = NULL;
  unsigned int size = _callVector.size();
  for (unsigned int i = 0; i < size; i++) {
    call = _callVector.at(i);
    if (call && call->getId() == id) {
      break;
    } else {
      call = NULL;
    }
  }
  return call;
}

/**
 * Multi Thread with _mutex for callVector
 */
void
ManagerImpl::deleteCall (CALLID id)
{
  _debug("CALL: Deleting call %d\n", id);
  CallVector::iterator iter = _callVector.begin();
  while(iter!=_callVector.end()) {
    Call *call = *iter;
    if (call != NULL && call->getId() == id) {
      if (call->getFlagNotAnswered() && call->isIncomingType() && call->getState() != Call::NotExist) {
        decWaitingCall();
      }
      delete (*iter); *iter = NULL; 
      call = NULL;
      _callVector.erase(iter);
      return;
    }
    iter++;
  }
}

void
ManagerImpl::setCurrentCallId(CALLID id)
{
  _debug("CALL: Setting current callid %d to %d\n", _currentCallId, id);
  _currentCallId = id;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/**
 * Main thread
 */
int 
ManagerImpl::outgoingCall (const std::string& to)
{	
  CALLID id = generateNewCallId();
  Call *call = pushBackNewCall(id, Outgoing);
  ost::MutexLock m(_mutex);
  call->setState(Call::Progressing);
  call->setCallerIdNumber(to);
  if (call->outgoingCall(to) == 0) {
    return id;
  } else {
    return 0;
  }
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::hangupCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == NULL) {
    return -1;
  }
  int result = -1;
  if (call->getState() != Call::Error) { 
    result = call->hangup();
  }
  deleteCall(id);
  // current call id or no line selected
  if (id == _currentCallId || _currentCallId == 0) {
    stopTone(); // stop tone, like a 700 error: number not found Not Found
  }
  return result;
}

/**
 * User action (main thread)
 * Every Call
 * -1 : call not found
 *  0 : already in this state...
 */
int
ManagerImpl::cancelCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == NULL) { 
    return -1; 
  }
  int result = call->cancel();
  deleteCall(id);
  stopTone();
  return result;
}

/**
 * User action (main thread)
 * Incoming Call
 */
int 
ManagerImpl::answerCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == NULL) {
    return -1;
  }
  if (call->getFlagNotAnswered()) {
    decWaitingCall();
    call->setFlagNotAnswered(false);
  }
  if (call->getState() != Call::OnHold) {
    switchCall(id);
  }
  stopTone(); // before answer, don't stop the audio stream after open it
  return call->answer();
}

/**
 * User action (main thread)
 * Every Call
 * @return 0 if it fails, -1 if not present
 */
int 
ManagerImpl::onHoldCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == NULL) {
    return -1;
  }
  if ( call->getState() == Call::OnHold || call->isNotAnswered()) {
    return 1;
  }
  setCurrentCallId(0);
  return call->onHold();
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::offHoldCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  stopTone();
  Call* call = getCall(id);
  if (call == 0) {
    return -1;
  }
  if (call->getState() == Call::OffHold) {
    return 1;
  }
  _debug("CALL: setting current id = %d\n", id);
  setCurrentCallId(id);
  int returnValue = call->offHold();
  // start audio if it's ok
  if (returnValue != -1) {
    getAudioDriver()->startStream();
  }
  return returnValue;
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::transferCall (CALLID id, const std::string& to)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == 0) {
    return -1;
  }
  setCurrentCallId(0);
  return call->transfer(to);
}

/**
 * User action (main thread)
 * All Call
 */
void
ManagerImpl::mute() {
  _mic_volume_before_mute = _mic_volume;
  setMicVolume(0);
}

/**
 * User action (main thread)
 * All Call
 */
void
ManagerImpl::unmute() {
  if ( _mic_volume == 0 ) {
    setMicVolume(_mic_volume_before_mute);
  }
}

/**
 * User action (main thread)
 * Call Incoming
 */
int 
ManagerImpl::refuseCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call *call = getCall(id);
  if (call == NULL) {
    return -1;
  }

  if ( call->getState() != Call::Progressing ) {
    return -1;
  }
  int refuse = call->refuse();

  setCurrentCallId(0);
  deleteCall(id);
  stopTone();
  return refuse;
}

/**
 * User action (main thread)
 */
bool
ManagerImpl::saveConfig (void)
{
  _debug("Saving Configuration...\n");
  setConfig(AUDIO, VOLUME_SPKR, getSpkrVolume());
  setConfig(AUDIO, VOLUME_MICRO, getMicVolume());

  _setupLoaded = _config.saveConfigTree(_path.data());
  return _setupLoaded;
}

/**
 * Main Thread
 */
bool
ManagerImpl::initRegisterVoIPLink() 
{
  int returnValue = true;
  _debug("Initiate VoIP Link Registration\n");
  if (_hasTriedToRegister == false) {
    if ( _voIPLinkVector.at(DFT_VOIP_LINK)->init() ) { 
      // we call here, because it's long...
      // If network is available and exosip is start..
      if (getConfigInt(SIGNALISATION, AUTO_REGISTER) && _exist == 1) {
        registerVoIPLink();
        _hasTriedToRegister = true;
      }
    } else {
      returnValue = false;
    }
  }
  return returnValue;
}


/**
 * Initialize action (main thread)
 * Note that Registration is only send if STUN is not activated
 * @return 1 if setRegister is call without failure, else return 0
 */
int 
ManagerImpl::registerVoIPLink (void)
{
  _debug("Register VoIP Link\n");
  int returnValue = 0;
  // Cyrille always want to register to receive call | 2005-10-24 10:50
  //if ( !useStun() ) {
    if (_voIPLinkVector.at(DFT_VOIP_LINK)->setRegister() >= 0) {
      returnValue = 1;
      _registerState = REGISTERED;
    } else {
      _registerState = FAILED;
    }
  //} else {
  //  _registerState = UNREGISTERED;
  //}
  return returnValue;
}

/**
 * Terminate action (main thread)
 * @return 1 if the unregister method is send correctly
 */
int 
ManagerImpl::unregisterVoIPLink (void)
{
  _debug("Unregister VoIP Link\n");
	if (_voIPLinkVector.at(DFT_VOIP_LINK)->setUnregister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * User action (main thread)
 */
bool 
ManagerImpl::sendDtmf (CALLID id, char code)
{
  int sendType = getConfigInt(SIGNALISATION, SEND_DTMF_AS);
  int returnValue = false;
  switch (sendType) {
  case 0: // SIP INFO
    playDtmf(code);
    _voIPLinkVector.at(DFT_VOIP_LINK)->carryingDTMFdigits(id, code);
    returnValue = true;
    break;

  case 1: // Audio way
    break;
  case 2: // rfc 2833
    break;
  default: // unknown - error config?
    break;
  }
  return returnValue;
}

/**
 * User action (main thread)
 */
bool
ManagerImpl::playDtmf(char code)
{
  stopTone();

  // length in milliseconds
  int pulselen = getConfigInt(SIGNALISATION, PULSE_LENGTH);
  if (!pulselen) { return false; }

  // numbers of int = length in milliseconds / 1000 (number of seconds)
  //                = number of seconds * SAMPLING_RATE by SECONDS
  int size = pulselen * (SAMPLING_RATE/1000);

  // this buffer is for mono
  int16* _buf = new int16[size];
  bool returnValue = false;

  // Handle dtmf
  _key.startTone(code);

  // copy the sound...
  if ( _key.generateDTMF(_buf, size) ) {
    int k;

    // allocation of more space, for stereo conversion
    int16* buf_ctrl_vol = new int16[size*CHANNELS];

    // Control volume and format mono->stereo
    for (int j = 0; j < size; j++) {
      k = j<<1; // fast multiplication by two
      buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = _buf[j];
    }

    AudioLayer *audiolayer = getAudioDriver();

    // Put buffer to urgentRingBuffer 
    // put the size in bytes...
    // so size * CHANNELS * 2 (bytes for the int16)
    int nbInt16InChar = sizeof(int16)/sizeof(char);
    audiolayer->putUrgent(buf_ctrl_vol, size * CHANNELS * nbInt16InChar);

    // We activate the stream if it's not active yet.
    if (!audiolayer->isStreamActive()) {
      audiolayer->startStream();
      //TODO: Is this really what we want?
      //audiolayer->sleep(pulselen);
      //audiolayer->stopStream();
    } else {
      audiolayer->sleep(pulselen); // in milliseconds
    }
    delete[] buf_ctrl_vol; buf_ctrl_vol = 0;
    returnValue = true;
  }
  delete[] _buf; _buf = 0;
  return returnValue;
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone 
////////////////////////////////////////////////////////////////////////////////
/**
 * Multi-thread
 */
bool
ManagerImpl::incomingCallWaiting() {
  ost::MutexLock m(_incomingCallMutex);
  return (_nbIncomingWaitingCall > 0) ? true : false;
}

void
ManagerImpl::incWaitingCall() {
  ost::MutexLock m(_incomingCallMutex);
  _nbIncomingWaitingCall++;
  _debug("incWaitingCall: %d\n", _nbIncomingWaitingCall);
}

void
ManagerImpl::decWaitingCall() {
  ost::MutexLock m(_incomingCallMutex);
  _nbIncomingWaitingCall--;
  _debug("decWaitingCall: %d\n", _nbIncomingWaitingCall);
}


/**
 * SipEvent Thread
 * Set the call info for incoming call
 */
void
ManagerImpl::callSetInfo(CALLID id, const std::string& name, const std::string& number)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != 0) {
    call->setCallerIdName(name);
    call->setCallerIdNumber(number);
  }
}

/**
 * SipEvent Thread
 * ask if it can close the call
 */
bool
ManagerImpl::callCanBeClosed(CALLID id) {
  bool returnValue = false;
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != NULL && call->getState() != Call::Progressing) {
    returnValue = true;
  }
  return returnValue;
}

/**
 * SipEvent Thread
 * ask if it can answer the call
 */
bool
ManagerImpl::callCanBeAnswered(CALLID id) {
  bool returnValue = false;
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != NULL && ( call->getFlagNotAnswered() || 
       (call->getState()!=Call::OnHold && call->getState()!=Call::OffHold) )) {
    returnValue = true;
  }
  return returnValue;
}

/**
 * SipEvent Thread
 * ask if it can start the sound thread
 */
bool
ManagerImpl::callIsOnHold(CALLID id) {
  bool returnValue = false;
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != NULL && (call->getState()==Call::OnHold)) {
    returnValue = true;
  }
  return returnValue;
}

/**
 * SipEvent Thread
 */
int 
ManagerImpl::incomingCall (CALLID id, const std::string& name, const std::string& number)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call == NULL) {
    return -1;
  }
  call->setType(Incoming);
  call->setState(Call::Progressing);

  if ( _currentCallId == 0 ) {
    switchCall(id);
    call->setFlagNotAnswered(false);
    ringtone();
  } else {
    incWaitingCall();
  }

  // TODO: Account not yet implemented
  std::string accountId = "acc1";
  std::string from = name;     call->setCallerIdName(name);
  call->setCallerIdNumber(number);
  if ( !number.empty() ) {
    from.append(" <");
    from.append(number);
    from.append(">");
  }
  return _gui->incomingCall(id, accountId, from);
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::peerAnsweredCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != 0) {
    call->setFlagNotAnswered(false);
    call->setState(Call::Answered);

    stopTone();
    // switch current call
    switchCall(id);
    if (_gui) _gui->peerAnsweredCall(id);
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
int
ManagerImpl::peerRingingCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != 0) {
    call->setState(Call::Ringing);

    // ring
    ringback();
    if (_gui) _gui->peerRingingCall(id);
  }
  return 1;
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
int 
ManagerImpl::peerHungupCall (CALLID id)
{
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if ( call == NULL ) {
    return -1;
  }
  if ( _currentCallId == id ) {
    stopTone();
  }

  if (_gui) _gui->peerHungupCall(id);
  deleteCall(id);
  call->setState(Call::Hungup);

  setCurrentCallId(0);
  return 1;
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayTextMessage (CALLID id, const std::string& message)
{
  if(_gui) {
    _gui->displayTextMessage(id, message);
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayErrorText (CALLID id, const std::string& message)
{
  if(_gui) {
    _gui->displayErrorText(id, message);
  } else {
    std::cerr << message << std::endl;
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayError (const std::string& voIPError)
{
  if(_gui) {
    _gui->displayError(voIPError);
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayStatus (const std::string& status)
{
  if(_gui) {
    _gui->displayStatus(status);
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayConfigError (const std::string& message)
{
  if(_gui) {
    _gui->displayConfigError(message);
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void
ManagerImpl::startVoiceMessageNotification (const std::string& nb_msg)
{
  if (_gui) _gui->sendVoiceNbMessage(nb_msg);
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void
ManagerImpl::stopVoiceMessageNotification (void)
{
  if (_gui) _gui->sendVoiceNbMessage(std::string("0"));
}

/**
 * Multi Thread
 */
bool 
ManagerImpl::playATone(Tone::TONEID toneId) {
  _toneMutex.enterMutex();
  _telephoneTone->setCurrentTone(toneId);
  _toneMutex.leaveMutex();
  getAudioDriver()->startStream();
  return true;
}

/**
 * Multi Thread
 */
void 
ManagerImpl::stopTone() {
  _debug("TONE: stop tone/stream...\n");
  getAudioDriver()->stopStream();

  _toneMutex.enterMutex();
  _telephoneTone->setCurrentTone(Tone::TONE_NULL);
  _toneMutex.leaveMutex();

  // for ringing tone..
  _toneMutex.enterMutex();
  if ( _toneType != ZT_TONE_NULL ) {
    _toneType = ZT_TONE_NULL;
    _tone->stopTone();
  }
  _toneMutex.leaveMutex();
}

/**
 * Multi Thread
 */
bool
ManagerImpl::playTone()
{
  _debug("TONE: play dialtone...\n");
  return playATone(Tone::TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::congestion () {
  playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringback () {
  playATone(Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::callBusy(CALLID id) {
  playATone(Tone::TONE_BUSY);
  ost::MutexLock m(_mutex);
  Call* call = getCall(id);
  if (call != 0) {
    call->setState(Call::Busy);
  }
}

/**
 * Multi Thread
 */
void
ManagerImpl::callFailure(CALLID id) {
  playATone(Tone::TONE_BUSY);
  _mutex.enterMutex();
  Call* call = getCall(id);
  if (call != 0) {
    call->setState(Call::Error);
  }
  _mutex.leaveMutex();
  if (_gui) {
    _gui->callFailure(id);
  }
}

Tone *
ManagerImpl::getTelephoneTone()
{
  if(_telephoneTone) {
    ost::MutexLock m(_toneMutex);
    return _telephoneTone->getCurrentTone();
  }
  else {
    return 0;
  }
}


/**
 * Multi Thread
 */
void
ManagerImpl::ringtone() 
{
  //std::string ringchoice = getConfigString(AUDIO, RING_CHOICE);
  // if there is no / inside the path
  //if ( ringchoice.find(DIR_SEPARATOR_CH) == std::string::npos ) {
    // check inside global share directory
  //  ringchoice = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR + DIR_SEPARATOR_STR + ringchoice; 
  //}
  //_toneMutex.enterMutex(); 
  //_toneType = ZT_TONE_FILE;
  //int play = _tone->playRingtone(ringchoice.c_str());
  //_toneMutex.leaveMutex();
  //if (play!=1) {
    ringback();
  //}
}

/**
 * Use Urgent Buffer
 * By AudioRTP thread
 */
void
ManagerImpl::notificationIncomingCall (void) {
  int16* buf_ctrl_vol;
  int16* buffer = new int16[SAMPLING_RATE];
  int size = SAMPLES_SIZE(FRAME_PER_BUFFER); //SAMPLING_RATE/2;
  int k;
  //int spkrVolume;

  _tone->generateSin(440, 0, buffer);

  // Volume Control 
  buf_ctrl_vol = new int16[size*CHANNELS];
  // spkrVolume = getSpkrVolume();
  for (int j = 0; j < size; j++) {
    k = j<<1; // fast multiply by two
    buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = buffer[j];
    // * spkrVolume/100;
  }
  getAudioDriver()->putUrgent(buf_ctrl_vol, SAMPLES_SIZE(FRAME_PER_BUFFER));

  delete[] buf_ctrl_vol;  buf_ctrl_vol = NULL;
  delete[] buffer;        buffer = NULL;
}

/**
 * Multi Thread
 */
void
ManagerImpl::getStunInfo (StunAddress4& stunSvrAddr) 
{
  StunAddress4 mappedAddr;
  struct in_addr in;
  char* addr;
  char to[16];
  bzero (to, 16);

  int fd3, fd4;
  bool ok = stunOpenSocketPair(stunSvrAddr,
                                    &mappedAddr,
                                    &fd3,
                                    &fd4);
  if (ok) {
    closesocket(fd3);
    closesocket(fd4);
    _debug("Got port pair at %d\n", mappedAddr.port);
    _firewallPort = mappedAddr.port;
    // Convert ipv4 address to host byte ordering
    in.s_addr = ntohl (mappedAddr.addr);
    addr = inet_ntoa(in);
    _firewallAddr = std::string(addr);
    _debug("address firewall = %s\n",_firewallAddr.data());
  } else {
    _debug("Opened a stun socket pair FAILED\n");
  }
}

bool
ManagerImpl::useStun (void) 
{
  if (getConfigInt(SIGNALISATION, USE_STUN)) {
      return true;
  } else {
      return false;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

/**
 * Multi Thread
 */
CALLID 
ManagerImpl::generateNewCallId (void)
{
  CALLID random_id = (unsigned)rand();

  // Check if already a call with this id exists 
  _mutex.enterMutex();
  while (getCall(random_id) != NULL && random_id != 0) {
    random_id = rand();
  }
  _mutex.leaveMutex();
  // If random_id is not attributed, returns it.
  return random_id;
}

/**
 * Initialization: Main Thread
 * @return 1: ok
          -1: error directory
           0: unable to load the setting
           2: file doesn't exist yet
 */
int
ManagerImpl::createSettingsPath (void) {
  _path = std::string(HOMEDIR) + DIR_SEPARATOR_STR + "." + PROGDIR;

  if (mkdir (_path.data(), 0755) != 0) {
    // If directory	creation failed
    if (errno != EEXIST) {
      _debug("Cannot create directory: %s\n", strerror(errno));
      return -1;
    }
  }

  // Load user's configuration
  _path = _path + DIR_SEPARATOR_STR + PROGNAME + "rc";
  return _config.populateFromFile(_path);
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initConfigFile (void) 
{
  std::string type_str("string");
  std::string type_int("int");

  std::string section;
  section = SIGNALISATION;
  fill_config_int(SYMMETRIC, YES_STR);
  fill_config_str(FULL_NAME, EMPTY_FIELD);
  fill_config_str(USER_PART, EMPTY_FIELD);
  fill_config_str(AUTH_USER_NAME, EMPTY_FIELD);
  fill_config_str(PASSWORD, EMPTY_FIELD);
  fill_config_str(HOST_PART, EMPTY_FIELD);
  fill_config_str(PROXY, EMPTY_FIELD);
  fill_config_int(AUTO_REGISTER, YES_STR);
  fill_config_int(PLAY_TONES, YES_STR);
  fill_config_int(PULSE_LENGTH, DFT_PULSE_LENGTH_STR);
  fill_config_int(SEND_DTMF_AS, SIP_INFO_STR);
  fill_config_str(STUN_SERVER, DFT_STUN_SERVER);
  fill_config_int(USE_STUN, NO_STR);

  section = AUDIO;
  fill_config_int(DRIVER_NAME, DFT_DRIVER_STR);
  fill_config_str(CODEC1, DFT_CODEC);
  fill_config_str(CODEC2, DFT_CODEC);
  fill_config_str(CODEC3, DFT_CODEC);
  fill_config_str(RING_CHOICE, DFT_RINGTONE);
  fill_config_int(VOLUME_SPKR, DFT_VOL_SPKR_STR);
  fill_config_int(VOLUME_MICRO, DFT_VOL_MICRO_STR);

  section = PREFERENCES;
  fill_config_str(SKIN_CHOICE, DFT_SKIN);
  fill_config_int(CONFIRM_QUIT, YES_STR);
  fill_config_str(ZONE_TONE, DFT_ZONE);
  fill_config_int(CHECKED_TRAY, NO_STR);
  fill_config_str(VOICEMAIL_NUM, DFT_VOICEMAIL);
  fill_config_int(CONFIG_ZEROCONF, CONFIG_ZEROCONF_DEFAULT_STR);

  _exist = createSettingsPath();
  _setupLoaded = (_exist == 2 ) ? false : true;
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioCodec (void)
{
  _debugInit("Load Audio Codecs");
  // TODO: need to be more dynamic...
  _codecDescVector.push_back(new CodecDescriptor(getConfigString(AUDIO, CODEC1)));
  _codecDescVector.push_back(new CodecDescriptor(getConfigString(AUDIO, CODEC2)));
  _codecDescVector.push_back(new CodecDescriptor(getConfigString(AUDIO, CODEC3)));
}

/**
 * Terminate: Main Thread
 */
void 
ManagerImpl::unloadAudioCodec()
{
  _debug("Unload Audio Codecs\n");
  CodecDescriptorVector::iterator iter = _codecDescVector.begin();
  while(iter!=_codecDescVector.end()) {
    delete *iter; *iter = NULL;
    iter++;
  }
  _codecDescVector.clear();
}


/**
 * Initialization: Main Thread
 */
void
ManagerImpl::selectAudioDriver (void)
{
#if defined(AUDIO_PORTAUDIO)
  try {
    _debugInit("AudioLayer Creation");
    _audiodriverPA = new AudioLayer();
    int noDevice = getConfigInt(AUDIO, DRIVER_NAME);
    _debugInit(" AudioLayer Device Count");
    int nbDevice = portaudio::System::instance().deviceCount();
    if (nbDevice == 0) {
      throw std::runtime_error("Portaudio detect no sound card.");
    } else if (noDevice >= nbDevice) {
      _debug("Portaudio auto-select device #0 because device #%d is not found\n", noDevice);
      _setupLoaded = false;
      noDevice = 0;
    }
    _debugInit(" AudioLayer Opening Device");
    _audiodriverPA->openDevice(noDevice);
  } catch(...) {
    throw;
  }
#else
# error You must define one AUDIO driver to use.
#endif
}

/**
 * Initialize the Zeroconf scanning services loop
 * Informations will be store inside a map DNSService->_services
 * Initialization: Main Thread
 */
void 
ManagerImpl::initZeroconf(void) 
{
#ifdef USE_ZEROCONF
  _debugInit("Zeroconf Initialization");
  int useZeroconf = getConfigInt(PREFERENCES, CONFIG_ZEROCONF);

  if (useZeroconf) {
    _DNSService->startScanServices();
  }
#endif
}

/**
 * Init the volume for speakers/micro from 0 to 100 value
 * Initialization: Main Thread
 */
void
ManagerImpl::initVolume()
{
  _debugInit("Initiate Volume\n");
  setSpkrVolume(getConfigInt(AUDIO, VOLUME_SPKR));
  setMicVolume(getConfigInt(AUDIO, VOLUME_MICRO));
}

/**
 * configuration function requests
 * Main Thread
 */
bool 
ManagerImpl::getZeroconf(const std::string& )
{
  bool returnValue = false;
#ifdef USE_ZEROCONF
  int useZeroconf = getConfigInt(PREFERENCES, CONFIG_ZEROCONF);
  if (useZeroconf && _gui != NULL) {
    TokenList arg;
    TokenList argTXT;
    std::string newService = "new service";
    std::string newTXT = "new txt record";
    if (!_DNSService->isStart()) { _DNSService->startScanServices(); }
    DNSServiceMap services = _DNSService->getServices();
    DNSServiceMap::iterator iter = services.begin();
    arg.push_back(newService);
    while(iter!=services.end()) {
      arg.push_front(iter->first);
      _gui->sendMessage("100",sequenceId,arg);
      arg.pop_front(); // remove the first, the name

      TXTRecordMap record = iter->second.getTXTRecords();
      TXTRecordMap::iterator iterTXT = record.begin();
      while(iterTXT!=record.end()) {
        argTXT.clear();
        argTXT.push_back(iter->first);
        argTXT.push_back(iterTXT->first);
        argTXT.push_back(iterTXT->second);
        argTXT.push_back(newTXT);
        _gui->sendMessage("101",sequenceId,argTXT);
        iterTXT++;
      }
      iter++;
    }
    returnValue = true;
  }
#endif
  return returnValue;
}

/**
 * Main Thread
 */
bool 
ManagerImpl::attachZeroconfEvents(const std::string& , Pattern::Observer& )
{
  bool returnValue = false;
  // don't need the _gui like getZeroconf function
  // because Observer is here
#ifdef USE_ZEROCONF
  int useZeroconf = getConfigInt(PREFERENCES, CONFIG_ZEROCONF);
  if (useZeroconf) {
    if (!_DNSService->isStart()) { _DNSService->startScanServices(); }
    _DNSService->attach(observer);
    returnValue = true;
  }
#endif
  return returnValue;
}
bool
ManagerImpl::detachZeroconfEvents(Pattern::Observer& )
{
  bool returnValue = false;
#ifdef USE_ZEROCONF
  if (_DNSService) {
    _DNSService->detach(observer);
    returnValue = true;
  }
#endif
  return returnValue;
}

/**
 * Main Thread
 */
bool
ManagerImpl::getEvents() {
  initRegisterVoIPLink();
  return true;
}

/**
 * Main Thread
 */
bool 
ManagerImpl::getCallStatus(const std::string& sequenceId)
{
  // TODO: implement account
  std::string accountId = "acc1"; 
  std::string code;
  std::string status;
  TokenList tk;
  Call* call;

  if (_gui!=NULL) {
    ost::MutexLock m(_mutex);
    CallVector::iterator iter = _callVector.begin();
    while(iter!=_callVector.end()){
      call = (*iter);
      switch( call->getState() ) {
       case Call::Progressing: code="110"; status="Trying";        break;
       case Call::Ringing:     code="111"; status = "Ringing";     break;
       case Call::Answered:    code="112"; status = "Established"; break;
       case Call::Busy:        code="113"; status = "Busy";        break;
       case Call::OnHold:      code="114"; status = "Held";        break;
       case Call::OffHold:     code="115"; status = "Unheld";      break;
       default:                code="125"; status="Other";    
      }

      // No Congestion
      // No Wrong Number
      // 116 <CSeq> <call-id> <acc> <destination> Busy
      std::string destination = call->getCallerIdName();
      std::string number = call->getCallerIdNumber();
      if (number!="") {
        destination.append(" <");
        destination.append(number);
        destination.append(">");
      }
      tk.push_back(accountId);
      tk.push_back(destination);
      tk.push_back(status);
      _gui->sendCallMessage(code, sequenceId, (*iter)->getId(), tk);
      iter++;
      tk.clear();
    }
  }
  return true;
}

/**
 * Main Thread
 */
bool 
ManagerImpl::getConfigAll(const std::string& sequenceId)
{
  bool returnValue = false;
  Conf::ConfigTreeIterator iter = _config.createIterator();
  TokenList tk = iter.begin();
  if (tk.size()) {
    returnValue = true;
  }
  while (tk.size()) {
    _gui->sendMessage("100", sequenceId, tk);
    tk = iter.next();
  }
  return returnValue;
}

/**
 * Main Thread
 */
bool 
ManagerImpl::getConfig(const std::string& section, const std::string& name, TokenList& arg)
{
  return _config.getConfigTreeItemToken(section, name, arg);
}

/**
 * Main Thread
 */
// throw an Conf::ConfigTreeItemException if not found
int 
ManagerImpl::getConfigInt(const std::string& section, const std::string& name)
{
  try {
    return _config.getConfigTreeItemIntValue(section, name);
  } catch (Conf::ConfigTreeItemException& e) {
    throw e;
  }
  return 0;
}

/**
 * Main Thread
 */
std::string 
ManagerImpl::getConfigString(const std::string& section, const std::string&
name)
{
  try {
    return _config.getConfigTreeItemValue(section, name);
  } catch (Conf::ConfigTreeItemException& e) {
    throw e;
  }
  return "";
}

/**
 * Main Thread
 */
bool 
ManagerImpl::setConfig(const std::string& section, const std::string& name, const std::string& value)
{
  return _config.setConfigTreeItem(section, name, value);
}

/**
 * Main Thread
 */
bool 
ManagerImpl::setConfig(const std::string& section, const std::string& name, int value)
{
  std::ostringstream valueStream;
  valueStream << value;
  return _config.setConfigTreeItem(section, name, valueStream.str());
}

/**
 * Main Thread
 */
bool 
ManagerImpl::getConfigList(const std::string& sequenceId, const std::string& name)
{
  bool returnValue = false;
  TokenList tk;
  if (name=="codecdescriptor") {

    CodecMap::iterator iter = _codecMap.begin();
    while( iter != _codecMap.end() ) {
      tk.clear();
      std::ostringstream strType;
      strType << iter->first;
      tk.push_back(strType.str());
      tk.push_back(iter->second);
      _gui->sendMessage("100", sequenceId, tk);
      iter++;
    }
    returnValue = true;
  } else if (name=="ringtones") {
    std::string path = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR;
    int nbFile = 0;
    returnValue = getDirListing(sequenceId, path, &nbFile);

    path = std::string(HOMEDIR) + DIR_SEPARATOR_STR + "." + PROGDIR + DIR_SEPARATOR_STR + RINGDIR;
    getDirListing(sequenceId, path, &nbFile);
  } else if (name=="audiodevice") {
    returnValue = getAudioDeviceList(sequenceId);
  } else if (name=="countrytones") {
    returnValue = getCountryTones(sequenceId);
  }
  return returnValue;
}

/**
 * User request Main Thread (list)
 */
bool 
ManagerImpl::getAudioDeviceList(const std::string& sequenceId) 
{
  bool returnValue = false;
  try {
    // TODO: test when there is an error on initializing...
    TokenList tk;
    portaudio::System& sys = portaudio::System::instance();

    const char *hostApiName;
    const char *deviceName;

    for (int index = 0; index < sys.deviceCount(); index++ ) {
      portaudio::Device& device = sys.deviceByIndex(index);
      hostApiName = device.hostApi().name();
      deviceName  = device.name();

      tk.clear();
      std::ostringstream str; str << index; tk.push_back(str.str());
      tk.push_back(deviceName);
      tk.push_back(std::string(hostApiName));
      _gui->sendMessage("100", sequenceId, tk);
    }
    returnValue = true;
  } catch (...) {
    returnValue = false;
  }
  return returnValue;
}

bool
ManagerImpl::getCountryTones(const std::string& sequenceId)
{
  // see ToneGenerator for the list...
  sendCountryTone(sequenceId, 1, "North America");
  sendCountryTone(sequenceId, 2, "France");
  sendCountryTone(sequenceId, 3, "Australia");
  sendCountryTone(sequenceId, 4, "United Kingdom");
  sendCountryTone(sequenceId, 5, "Spain");
  sendCountryTone(sequenceId, 6, "Italy");
  sendCountryTone(sequenceId, 7, "Japan");

  return true;
}

void 
ManagerImpl::sendCountryTone(const std::string& sequenceId, int index, const std::string& name) {
  TokenList tk;
  std::ostringstream str; str << index; tk.push_back(str.str());
  tk.push_back(name);
  _gui->sendMessage("100", sequenceId, tk);
}

/**
 * User action : main thread
 */
bool
ManagerImpl::getDirListing(const std::string& sequenceId, const std::string& path, int *nbFile) {
  TokenList tk;
  try {
    ost::Dir dir(path.c_str());
    const char *cFileName = NULL;
    std::string fileName;
    std::string filePathName;
    while ( (cFileName=dir++) != NULL ) {
      fileName = cFileName;
      filePathName = path + DIR_SEPARATOR_STR + cFileName;
      if (fileName.length() && fileName[0]!='.' && !ost::isDir(filePathName.c_str())) {
        tk.clear();
        std::ostringstream str;
        str << (*nbFile);
        tk.push_back(str.str());
        tk.push_back(filePathName);
        _gui->sendMessage("100", sequenceId, tk);
        (*nbFile)++;
      }
    }
    return true;
  } catch (...) {
    // error to open file dir
    return false;
  }
}

/**
 * Multi Thread
 */
void 
ManagerImpl::switchCall(CALLID id)
{
  // we can only switch the current call id if we 
  // it's not selected yet..
  if (_currentCallId == 0 ) {
    setCurrentCallId(id);
  }
}


