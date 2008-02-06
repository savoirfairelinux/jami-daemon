/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#include <errno.h>
#include <time.h>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>	// mkdir(2)

#include <cc++/socket.h>   // why do I need this here?
#include <ccrtp/channel.h> // why do I need this here?
#include <ccrtp/rtp.h>     // why do I need this here?
#include <cc++/file.h>

#include <boost/tokenizer.hpp>

#include "manager.h"
#include "account.h"
#include "audio/audiolayer.h"
#include "audio/audiocodec.h"
#include "audio/tonelist.h"

#include "accountcreator.h" // create new account
#include "voiplink.h"

#include "user_cfg.h"

#ifdef USE_ZEROCONF
#include "zeroconf/DNSService.h"
#include "zeroconf/DNSServiceTXTRecord.h"
#endif

#define fill_config_str(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_str)))
#define fill_config_int(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_int)))

ManagerImpl::ManagerImpl (void)
{
  // Init private variables 
  _hasZeroconf = false;
#ifdef USE_ZEROCONF
  _hasZeroconf = true;
  _DNSService = new DNSService();
#endif

  // setup
  _path = ""; 
  _exist = 0;
  _setupLoaded = false;
  _dbus = NULL;

  // sound
  _audiodriver = NULL;
  _dtmfKey = 0;
  _spkr_volume = 0;  // Initialize after by init() -> initVolume()
  _mic_volume  = 0;  // Initialize after by init() -> initVolume()
  _mic_volume_before_mute = 0; 

  // Call
  _nbIncomingWaitingCall=0;
  _hasTriedToRegister = false;

  // initialize random generator for call id
  srand (time(NULL));

#ifdef TEST
  testAccountMap();
  loadAccountMap();
  testCallAccountMap();
  unloadAccountMap();
#endif

  // should be call before initConfigFile
  // loadAccountMap();, called in init() now.
}

// never call if we use only the singleton...
ManagerImpl::~ManagerImpl (void) 
{
  terminate();

#ifdef USE_ZEROCONF
  delete _DNSService; _DNSService = 0;
#endif

  _debug("%s stop correctly.\n", PROGNAME);
}

void 
ManagerImpl::init() 
{
  // Load accounts, init map
  loadAccountMap();

  initVolume();

  if (_exist == 0) {
    _debug("Cannot create config file in your home directory\n");
  }

  initAudioDriver();
  selectAudioDriver();

  // Initialize the list of supported audio codecs
  initAudioCodec();

  AudioLayer *audiolayer = getAudioDriver();
  if (audiolayer!=0) {
    unsigned int sampleRate = audiolayer->getSampleRate();

    _debugInit("Load Telephone Tone");
    std::string country = getConfigString(PREFERENCES, ZONE_TONE);
    _telephoneTone = new TelephoneTone(country, sampleRate);

    _debugInit("Loading DTMF key");
    _dtmfKey = new DTMF(sampleRate);
  }

  // initRegisterAccounts was here, but we doing it after the gui loaded... 
  // the stun detection is long, so it's a better idea to do it after getEvents
  initZeroconf();
}

void ManagerImpl::terminate()
{
  saveConfig();

  unloadAccountMap();

  _debug("Unload DTMF Key\n");
  delete _dtmfKey;

  _debug("Unload Audio Driver\n");
  delete _audiodriver; _audiodriver = NULL;

  _debug("Unload Telephone Tone\n");
  delete _telephoneTone; _telephoneTone = NULL;
}

bool
ManagerImpl::isCurrentCall(const CallID& callId) {
  ost::MutexLock m(_currentCallMutex);
  return (_currentCallId2 == callId ? true : false);
}

bool
ManagerImpl::hasCurrentCall() {
  ost::MutexLock m(_currentCallMutex);
  _debug("Current call ID = %s\n", _currentCallId2.c_str());
  if ( _currentCallId2 != "") {
    return true;
  }
  return false;
}

const CallID& 
ManagerImpl::getCurrentCallId() {
  ost::MutexLock m(_currentCallMutex);
  return _currentCallId2;
}

void
ManagerImpl::switchCall(const CallID& id ) {
  ost::MutexLock m(_currentCallMutex);
  _currentCallId2 = id;
}


///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */ 
bool
ManagerImpl::outgoingCall(const std::string& accountid, const CallID& id, const std::string& to)
{
  if (!accountExists(accountid)) {
    _debug("! Manager Error: Outgoing Call: account doesn't exist\n");
    return false;
  }
  if (getAccountFromCall(id) != AccountNULL) {
    _debug("! Manager Error: Outgoing Call: call id already exists\n");
    return false;
  }
  if (hasCurrentCall()) {
    _debug("* Manager Info: there is currently a call, try to hold it\n");
    onHoldCall(getCurrentCallId());
  }
  _debug("- Manager Action: Adding Outgoing Call %s on account %s\n", id.data(), accountid.data());
  if ( getAccountLink(accountid)->newOutgoingCall(id, to) ) {
    associateCallToAccount( id, accountid );
    switchCall(id);
    return true;
  } else {
    _debug("! Manager Error: An error occur, the call was not created\n");
  }
  return false;
}

//THREAD=Main : for outgoing Call
bool
ManagerImpl::answerCall(const CallID& id)
{
  stopTone(false); 
  /*if (hasCurrentCall()) 
  { 
    onHoldCall(getCurrentCallId());
  }*/
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("Answering Call: Call doesn't exists\n");
    return false;
  }

  if (!getAccountLink(accountid)->answer(id)) {
    // error when receiving...
    removeCallAccount(id);
    return false;
  }

  //Place current call on hold if it isn't
  
  
  // if it was waiting, it's waiting no more
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "CURRENT");
  removeWaitingCall(id);
  switchCall(id);
  return true;
}

//THREAD=Main
bool 
ManagerImpl::sendTextMessage(const AccountID& accountId, const std::string& to, const std::string& message) 
{
  if (accountExists(accountId)) {
    return getAccountLink(accountId)->sendMessage(to, message);
  }
  return false;
}

//THREAD=Main
bool
ManagerImpl::hangupCall(const CallID& id)
{
  stopTone(true);
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "HUNGUP");
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    /** @todo We should tell the GUI that the call doesn't exist, so
     * it clears up. This can happen. */
    _debug("! Manager Hangup Call: Call doesn't exists\n");
    return false;
  }

  bool returnValue = getAccountLink(accountid)->hangup(id);
  removeCallAccount(id);
  switchCall("");
  
  
  return returnValue;
}

//THREAD=Main
bool
ManagerImpl::cancelCall (const CallID& id)
{
  stopTone(true);
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("! Manager Cancel Call: Call doesn't exists\n");
    return false;
  }

  bool returnValue = getAccountLink(accountid)->cancel(id);
  // it could be a waiting call?
  removeWaitingCall(id);
  removeCallAccount(id);
  switchCall("");
  
  return returnValue;
}

//THREAD=Main
bool
ManagerImpl::onHoldCall(const CallID& id)
{
  stopTone(true);
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("5 Manager On Hold Call: Account ID %s or callid %s desn't exists\n", accountid.c_str(), id.c_str());
    return false;
  }

  _debug("Setting ONHOLD, Account %s, callid %s\n", accountid.c_str(), id.c_str());

  bool returnValue = getAccountLink(accountid)->onhold(id);
  
  removeWaitingCall(id);
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "HOLD");
  switchCall("");
  
  return returnValue;
}

//THREAD=Main
bool
ManagerImpl::offHoldCall(const CallID& id)
{
  stopTone(false);
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("5 Manager OffHold Call: Call doesn't exists\n");
    return false;
  }
  
  //Place current call on hold if it isn't
  if (hasCurrentCall()) 
  { 
    onHoldCall(getCurrentCallId());
  }
  
  _debug("Setting OFFHOLD, Account %s, callid %s\n", accountid.c_str(), id.c_str());

  bool returnValue = getAccountLink(accountid)->offhold(id);
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "UNHOLD");
  switchCall(id);
  if (returnValue) {
    try {
      getAudioDriver()->startStream();
    } catch(...) {
      _debugException("! Manager Off hold could not start audio stream");
    }
  }
  return returnValue;
}

//THREAD=Main
bool
ManagerImpl::transferCall(const CallID& id, const std::string& to)
{
  stopTone(true);
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("! Manager Transfer Call: Call doesn't exists\n");
    return false;
  }
  bool returnValue = getAccountLink(accountid)->transfer(id, to);
  removeWaitingCall(id);
  removeCallAccount(id);
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "HUNGUP");
  switchCall("");

  return returnValue;
}

//THREAD=Main
void
ManagerImpl::mute() {
  _mic_volume_before_mute = _mic_volume;
  setMicVolume(0);
}

//THREAD=Main
void
ManagerImpl::unmute() {
  if ( _mic_volume == 0 ) {
    setMicVolume(_mic_volume_before_mute);
  }
}

//THREAD=Main : Call:Incoming
bool
ManagerImpl::refuseCall (const CallID& id)
{
  stopTone(true);
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("! Manager OffHold Call: Call doesn't exists\n");
    return false;
  }
  bool returnValue = getAccountLink(accountid)->refuse(id);
  // if the call was outgoing or established, we didn't refuse it
  // so the method did nothing
  if (returnValue) {
    removeWaitingCall(id);
    removeCallAccount(id);
    if (_dbus) _dbus->getCallManager()->callStateChanged(id, "HUNGUP");
    switchCall("");
  }
  return returnValue;
}

//THREAD=Main
bool
ManagerImpl::saveConfig (void)
{
  _debug("Saving Configuration...\n");
  setConfig(AUDIO, VOLUME_SPKR, getSpkrVolume());
  setConfig(AUDIO, VOLUME_MICRO, getMicVolume());

  _setupLoaded = _config.saveConfigTree(_path.data());
  return _setupLoaded;
}

//THREAD=Main
bool
ManagerImpl::initRegisterAccounts() 
{
  _debugInit("Initiate VoIP Links Registration");
  AccountMap::iterator iter = _accountMap.begin();
  while( iter != _accountMap.end() ) {
    if ( iter->second) {
      iter->second->loadConfig();
      if ( iter->second->isEnabled() ) {
	iter->second->registerVoIPLink();
      }
    }
    iter++;
  }
  return true;
}

//THREAD=Main
// Currently unused
bool
ManagerImpl::registerAccount(const AccountID& accountId)
{
  _debug("Register one VoIP Link\n");

  // right now, we don't support two SIP account
  // so we close everything before registring a new account
  Account* account = getAccount(accountId);
  if (account != 0) {
    AccountMap::iterator iter = _accountMap.begin();
    while ( iter != _accountMap.end() ) {
      if ( iter->second ) {
        iter->second->unregisterVoIPLink();
      }
      iter++;
    }
    account->registerVoIPLink();
  }
  return true;
}

//THREAD=Main
// Currently unused
bool 
ManagerImpl::unregisterAccount(const AccountID& accountId)
{
  _debug("Unregister one VoIP Link\n");

  if (accountExists( accountId ) ) {
    getAccount(accountId)->unregisterVoIPLink();
  }
  return true;
}

//THREAD=Main
bool 
ManagerImpl::sendDtmf(const CallID& id, char code)
{
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("Send DTMF: call doesn't exists\n");
    return false;
  }

  int sendType = getConfigInt(SIGNALISATION, SEND_DTMF_AS);
  bool returnValue = false;
  switch (sendType) {
  case 0: // SIP INFO
    playDtmf(code);
    returnValue = getAccountLink(accountid)->carryingDTMFdigits(id, code);
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

//THREAD=Main | VoIPLink
bool
ManagerImpl::playDtmf(char code)
{
  // HERE are the variable:
  // - boolean variable to play or not (config)
  // - length in milliseconds to play
  // - sample of audiolayer
  stopTone(false);
  int hasToPlayTone = getConfigInt(SIGNALISATION, PLAY_DTMF);
  if (!hasToPlayTone) return false;

  // length in milliseconds
  int pulselen = getConfigInt(SIGNALISATION, PULSE_LENGTH);
  if (!pulselen) { return false; }

  // numbers of int = length in milliseconds / 1000 (number of seconds)
  //                = number of seconds * SAMPLING_RATE by SECONDS
  AudioLayer* audiolayer = getAudioDriver();

  // fast return, no sound, so no dtmf
  if (audiolayer==0 || _dtmfKey == 0) { return false; }
  // number of data sampling in one pulselen depends on samplerate
  // size (n sampling) = time_ms * sampling/s 
  //                     ---------------------
  //                            ms/s
  int size = (int)(pulselen * ((float)audiolayer->getSampleRate()/1000));

  // this buffer is for mono
  // TODO <-- this should be global and hide if same size
  SFLDataFormat* _buf = new SFLDataFormat[size];
  bool returnValue = false;

  // Handle dtmf
  _dtmfKey->startTone(code);

  // copy the sound
  if ( _dtmfKey->generateDTMF(_buf, size) ) {

    // Put buffer to urgentRingBuffer 
    // put the size in bytes...
    // so size * 1 channel (mono) * sizeof (bytes for the data)
    audiolayer->putUrgent(_buf, size * sizeof(SFLDataFormat));

    try {
      // We activate the stream if it's not active yet.
      if (!audiolayer->isStreamActive()) {
        audiolayer->startStream();
      } else {
        audiolayer->sleep(pulselen); // in milliseconds
      }
    } catch(...) {
      _debugException("Portaudio exception when playing a dtmf");
    }
    returnValue = true;
  }

  // TODO: add caching
  delete[] _buf; _buf = 0;
  return returnValue;
}

// Multi-thread 
bool
ManagerImpl::incomingCallWaiting() {
  ost::MutexLock m(_waitingCallMutex);
  return (_nbIncomingWaitingCall > 0) ? true : false;
}

void
ManagerImpl::addWaitingCall(const CallID& id) {
  ost::MutexLock m(_waitingCallMutex);
  _waitingCall.insert(id);
  _nbIncomingWaitingCall++;
}

void
ManagerImpl::removeWaitingCall(const CallID& id) {
  ost::MutexLock m(_waitingCallMutex);
  // should return more than 1 if it erase a call
  if (_waitingCall.erase(id)) {
    _nbIncomingWaitingCall--;
  }
}

bool
ManagerImpl::isWaitingCall(const CallID& id) {
  ost::MutexLock m(_waitingCallMutex);
  CallIDSet::iterator iter = _waitingCall.find(id);
  if (iter != _waitingCall.end()) {
    return false;
  }
  return true;
}



///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone 
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread 
bool 
ManagerImpl::incomingCall(Call* call, const AccountID& accountId) 
{
  _debug("Incoming call\n");

  associateCallToAccount(call->getCallId(), accountId);

  if ( !hasCurrentCall() ) {
    _debug("INCOMING CALL!!!!!!\n");
    call->setConnectionState(Call::Ringing);
    ringtone();
    switchCall(call->getCallId());
  } else {
     addWaitingCall(call->getCallId());
  }

  std::string from = call->getPeerName();
  std::string number = call->getPeerNumber();

  if (from != "" && number != "") {
    from.append(" <");
    from.append(number);
    from.append(">");
  } else if ( from.empty() ) {
    from.append("<");
    from.append(number);
    from.append(">");
  }
  _dbus->getCallManager()->incomingCall(accountId, call->getCallId(), from);
  return true;
}

//THREAD=VoIP
void
ManagerImpl::incomingMessage(const AccountID& accountId, const std::string& message) {
  if (_dbus) {
    _dbus->getCallManager()->incomingMessage(accountId, message);
  }
}

//THREAD=VoIP CALL=Outgoing
void
ManagerImpl::peerAnsweredCall(const CallID& id)
{
  if (isCurrentCall(id)) {
    stopTone(false);
  }
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "CURRENT");
}

//THREAD=VoIP Call=Outgoing
void
ManagerImpl::peerRingingCall(const CallID& id)
{
  if (isCurrentCall(id)) {
    ringback();
  }
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void
ManagerImpl::peerHungupCall(const CallID& id)
{
  AccountID accountid = getAccountFromCall( id );
  if (accountid == AccountNULL) {
    _debug("peerHungupCall: Call doesn't exists\n");
    return;
  }
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "HUNGUP");
  if (isCurrentCall(id)) {
    stopTone(true);
    switchCall("");
  }
  removeWaitingCall(id);
  removeCallAccount(id);
  
}

//THREAD=VoIP
void
ManagerImpl::callBusy(const CallID& id) {
  _debug("Call busy\n");
  
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "BUSY");
  if (isCurrentCall(id) ) {
    playATone(Tone::TONE_BUSY);
    switchCall("");
  }
  removeCallAccount(id);
  removeWaitingCall(id);
}

//THREAD=VoIP
void
ManagerImpl::callFailure(const CallID& id) 
{
  _debug("Call failed\n");
  if (_dbus) _dbus->getCallManager()->callStateChanged(id, "FAILURE");
  if (isCurrentCall(id) ) {
    playATone(Tone::TONE_BUSY);
    switchCall("");
  }
  removeCallAccount(id);
  removeWaitingCall(id);
  
}

//THREAD=VoIP
void 
ManagerImpl::displayTextMessage(const CallID& id, const std::string& message)
{
  /*if(_gui) {
   _gui->displayTextMessage(id, message);
  }*/
}

//THREAD=VoIP
void 
ManagerImpl::displayErrorText(const CallID& id, const std::string& message)
{
  /*if(_gui) {
    _gui->displayErrorText(id, message);
  } else {
    std::cerr << message << std::endl;
  }*/
}

//THREAD=VoIP
void 
ManagerImpl::displayError (const std::string& error)
{
  /*if(_gui) {
    _gui->displayError(error);
  }*/
}

//THREAD=VoIP
void 
ManagerImpl::displayStatus(const std::string& status)
{
  /*if(_gui) {
    _gui->displayStatus(status);
  }*/
}

//THREAD=VoIP
void 
ManagerImpl::displayConfigError (const std::string& message)
{
  /*if(_gui) {
    _gui->displayConfigError(message);
  }*/
}

//THREAD=VoIP
void
ManagerImpl::startVoiceMessageNotification(const AccountID& accountId, const std::string& nb_msg)
{
  if (_dbus) _dbus->getCallManager()->voiceMailNotify(accountId, atoi(nb_msg.c_str()) );
}

//THREAD=VoIP
void
ManagerImpl::stopVoiceMessageNotification(const AccountID& accountId)
{
  if (_dbus) _dbus->getCallManager()->voiceMailNotify(accountId, 0 );
} 

//THREAD=VoIP
void 
ManagerImpl::registrationSucceed(const AccountID& accountid)
{
  Account* acc = getAccount(accountid);
 if ( acc ) { 
    //acc->setState(true); 
    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
  }
}

//THREAD=VoIP
void 
ManagerImpl::registrationFailed(const AccountID& accountid)
{
  Account* acc = getAccount(accountid);
  if ( acc ) { 
    //acc->setState(false);
    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
  }
}

/**
 * Multi Thread
 */
bool 
ManagerImpl::playATone(Tone::TONEID toneId) {
  int hasToPlayTone = getConfigInt(SIGNALISATION, PLAY_TONES);
  if (!hasToPlayTone) return false;

  if (_telephoneTone != 0) {
    _toneMutex.enterMutex();
    _telephoneTone->setCurrentTone(toneId);
    _toneMutex.leaveMutex();

    try {
      AudioLayer* audiolayer = getAudioDriver();
      if (audiolayer) { audiolayer->startStream(); }
    } catch(...) {
      _debugException("Off hold could not start audio stream");
      return false;
    }
  }
  return true;
}

/**
 * Multi Thread
 */
void 
ManagerImpl::stopTone(bool stopAudio=true) {
  int hasToPlayTone = getConfigInt(SIGNALISATION, PLAY_TONES);
  if (!hasToPlayTone) return;

  if (stopAudio) {
    try {
      AudioLayer* audiolayer = getAudioDriver();
      if (audiolayer) { audiolayer->stopStream(); }
    } catch(...) {
      _debugException("Stop tone and stop stream");
    }
  }

  _toneMutex.enterMutex();
  if (_telephoneTone != 0) {
    _telephoneTone->setCurrentTone(Tone::TONE_NULL);
  }
  _toneMutex.leaveMutex();

  // for ringing tone..
  _toneMutex.enterMutex();
  _audiofile.stop();
  _toneMutex.leaveMutex();
}

/**
 * Multi Thread
 */
bool
ManagerImpl::playTone()
{
  //return playATone(Tone::TONE_DIALTONE);
  playATone(Tone::TONE_DIALTONE);
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
ManagerImpl::ringtone() 
{
  int hasToPlayTone = getConfigInt(SIGNALISATION, PLAY_TONES);
  if (!hasToPlayTone) { return; }

  std::string ringchoice = getConfigString(AUDIO, RING_CHOICE);
  //if there is no / inside the path
  if ( ringchoice.find(DIR_SEPARATOR_CH) == std::string::npos ) {
    // check inside global share directory
    ringchoice = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR + DIR_SEPARATOR_STR + ringchoice; 
  }

  AudioLayer* audiolayer = getAudioDriver();
  if (audiolayer==0) { return; }
  int sampleRate  = audiolayer->getSampleRate();

  _toneMutex.enterMutex(); 
  bool loadFile = _audiofile.loadFile(ringchoice, sampleRate);
  _toneMutex.leaveMutex(); 
  if (loadFile) {
    _toneMutex.enterMutex(); 
    _audiofile.start();
    _toneMutex.leaveMutex(); 
    try {
      audiolayer->startStream();
    } catch(...) {
      _debugException("Audio file couldn't start audio stream");
    }
  } else {
    ringback();
  }
}

AudioLoop*
ManagerImpl::getTelephoneTone()
{
  if(_telephoneTone != 0) {
    ost::MutexLock m(_toneMutex);
    return _telephoneTone->getCurrentTone();
  }
  else {
    return 0;
  }
}

AudioLoop*
ManagerImpl::getTelephoneFile()
{
  ost::MutexLock m(_toneMutex);
  if(_audiofile.isStarted()) {
    return &_audiofile;
  } else {
    return 0;
  }
}


/**
 * Use Urgent Buffer
 * By AudioRTP thread
 */
void
ManagerImpl::notificationIncomingCall(void) {

  AudioLayer* audiolayer = getAudioDriver();
  if (audiolayer != 0) {
    unsigned int samplerate = audiolayer->getSampleRate();
    std::ostringstream frequency;
    frequency << "440/" << FRAME_PER_BUFFER;

    Tone tone(frequency.str(), samplerate);
    unsigned int nbSampling = tone.getSize();
    SFLDataFormat buf[nbSampling];
    tone.getNext(buf, tone.getSize());
    audiolayer->putUrgent(buf, sizeof(SFLDataFormat)*nbSampling);
  }
}

/**
 * Multi Thread
 */
bool
ManagerImpl::getStunInfo (StunAddress4& stunSvrAddr, int port) 
{
  StunAddress4 mappedAddr;
  struct in_addr in;
  char* addr;

  //int fd3, fd4;
  // bool ok = stunOpenSocketPair(stunSvrAddr, &mappedAddr, &fd3, &fd4, port);
  int fd1 = stunOpenSocket(stunSvrAddr, &mappedAddr, port);
  bool ok = (fd1 == -1 || fd1 == INVALID_SOCKET) ? false : true;
  if (ok) {
    closesocket(fd1);
    //closesocket(fd3);
    //closesocket(fd4);
    _firewallPort = mappedAddr.port;
    // Convert ipv4 address to host byte ordering
    in.s_addr = ntohl (mappedAddr.addr);
    addr = inet_ntoa(in);
    _firewallAddr = std::string(addr);
    _debug("STUN Firewall: [%s:%d]\n", _firewallAddr.data(), _firewallPort);
    return true;
  } else {
    _debug("Opening a stun socket pair failed\n");
  }
  return false;
}

bool
ManagerImpl::behindNat(const std::string& svr, int port)
{
  StunAddress4 stunSvrAddr;
  stunSvrAddr.addr = 0;
   
  // Convert char* to StunAddress4 structure
  bool ret = stunParseServerName ((char*)svr.data(), stunSvrAddr);
  if (!ret) {
    _debug("SIP: Stun server address (%s) is not valid\n", svr.data());
    return 0;
  }
  
  // Firewall address
  //_debug("STUN server: %s\n", svr.data());
  return getStunInfo(stunSvrAddr, port);
}


///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 * @return 1: ok
          -1: error directory
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
  return 1;
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

  // Default values, that will be overwritten by the call to
  // 'populateFromFile' below.
  fill_config_int(SYMMETRIC, YES_STR);
  fill_config_int(PLAY_DTMF, YES_STR);
  fill_config_int(PLAY_TONES, YES_STR);
  fill_config_int(PULSE_LENGTH, DFT_PULSE_LENGTH_STR);
  fill_config_int(SEND_DTMF_AS, SIP_INFO_STR);

  section = AUDIO;
  //fill_config_int(DRIVER_NAME, DFT_DRIVER_STR);
  fill_config_int(DRIVER_NAME_IN, DFT_DRIVER_STR);
  fill_config_int(DRIVER_NAME_OUT, DFT_DRIVER_STR);
  fill_config_int(DRIVER_SAMPLE_RATE, DFT_SAMPLE_RATE);
  fill_config_int(DRIVER_FRAME_SIZE, DFT_FRAME_SIZE);
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

  // Loads config from ~/.sflphone/sflphonedrc or so..
  if (createSettingsPath() == 1) {
    _exist = _config.populateFromFile(_path);
  }

  _setupLoaded = (_exist == 2 ) ? false : true;
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioCodec (void)
{
  _debugInit("Active Codecs List");
  // init list of all supported codecs
  _codecDescriptorMap.init();
  // if the user never set the codec list, use the default one
  if(getConfigString(AUDIO, "Activecodecs") == ""){
    _codecDescriptorMap.setDefaultOrder();
  }
  // else retrieve the one he set in the config file
  else{
    std::vector<std::string> active_list = retrieveActiveCodecs(); 
    setActiveCodecList(active_list);
  }
}

std::vector<std::string>
ManagerImpl::retrieveActiveCodecs()
{
  std::vector<std::string> order; 
  std::string list;
  std::string s = getConfigString(AUDIO, "ActiveCodecs");
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer; 
  boost::char_separator<char> slash("/");
  tokenizer tokens(s, slash); 
  for(tokenizer::iterator tok_iter = tokens.begin(); tok_iter!= tokens.end(); ++tok_iter)
  {
    printf("%s\n", (*tok_iter).c_str());
    order.push_back(*tok_iter);
  }
  return order;
}

void
ManagerImpl::setActiveCodecList(const std::vector<std::string>& list)
{
  _debug("Set active codecs list");
  _codecDescriptorMap.saveActiveCodecs(list);
  // setConfig
  std::string s = serialize(list);
  printf("%s\n", s.c_str());
  setConfig("Audio", "ActiveCodecs", s);
}

std::string
ManagerImpl::serialize(std::vector<std::string> v)
{
  int i;
  std::string res;
  for(i=0;i<v.size();i++)
  {
    res += v[i] + "/";
  }
  return res;
}


std::vector <std::string>
ManagerImpl::getActiveCodecList( void )
{
  _debug("Get Active codecs list");
  std::vector< std::string > v;
  CodecOrder active = _codecDescriptorMap.getActiveCodecs();
  int i=0;
  size_t size = active.size();
  while(i<size)
  {
    std::stringstream ss;
    ss << active[i];
    v.push_back((ss.str()).data());
    i++;
  }
  return v;
}


/**
 * Send the list of codecs to the client through DBus.
 */
std::vector< std::string >
ManagerImpl::getCodecList( void )
{
  std::vector<std::string> list;
  CodecMap codecs = _codecDescriptorMap.getCodecMap();
  CodecOrder order = _codecDescriptorMap.getActiveCodecs();
  CodecMap::iterator iter = codecs.begin();  
  
  while(iter!=codecs.end())
  {
    std::stringstream ss;
    if(iter->first!=-1)
    {
      ss << iter->first;
      list.push_back((ss.str()).data());
    }
    iter++;
  }
  return list;
}

std::vector<std::string>
ManagerImpl::getCodecDetails( const ::DBus::Int32& payload )
{

  std::vector<std::string> v;
  std::stringstream ss;
   
  v.push_back(_codecDescriptorMap.getCodecName((CodecType)payload));
  ss << _codecDescriptorMap.getSampleRate((CodecType)payload);
  v.push_back((ss.str()).data()); 
  ss.str("");
  ss << _codecDescriptorMap.getBitRate((CodecType)payload);
  v.push_back((ss.str()).data());
  ss.str("");
  ss << _codecDescriptorMap.getBandwidthPerCall((CodecType)payload);
  v.push_back((ss.str()).data());
  ss.str("");

  return v;
}


/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioDriver(void) 
{
  _debugInit("AudioLayer Creation");
  _audiodriver = new AudioLayer(this);
  if (_audiodriver == 0) {
    _debug("Init audio driver error\n");
  } else {
    std::string error = getAudioDriver()->getErrorMessage();
    if (!error.empty()) {
      _debug("Init audio driver: %s\n", error.c_str());
    }
  } 
}

/**
 * Initialization: Main Thread and gui
 */
void
ManagerImpl::selectAudioDriver (void)
{
  //int noDevice  = getConfigInt(AUDIO, DRIVER_NAME);
  int noDeviceIn  = getConfigInt(AUDIO, DRIVER_NAME_IN);
  int noDeviceOut = getConfigInt(AUDIO, DRIVER_NAME_OUT);
  int sampleRate  = getConfigInt(AUDIO, DRIVER_SAMPLE_RATE);
  if (sampleRate <=0 || sampleRate > 48000) {
      sampleRate = 8000;
  }
	int frameSize = getConfigInt(AUDIO, DRIVER_FRAME_SIZE);

  // this is when no audio device in/out are set
  // or the audio device in/out are set to 0
  // we take the nodevice instead
  // remove this hack, how can we change the device to 0, if the noDevice is 1?
  //if (noDeviceIn == 0 && noDeviceOut == 0) {
  //  noDeviceIn = noDeviceOut = noDevice;
  //}
  _debugInit(" AudioLayer Opening Device");
  _audiodriver->setErrorMessage("");
  _audiodriver->openDevice(noDeviceIn, noDeviceOut, sampleRate, frameSize);
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
  _debugInit("Initiate Volume");
  setSpkrVolume(getConfigInt(AUDIO, VOLUME_SPKR));
  setMicVolume(getConfigInt(AUDIO, VOLUME_MICRO));
}

/**
 * configuration function requests
 * Main Thread
 */
bool 
ManagerImpl::getZeroconf(const std::string& sequenceId)
{
  bool returnValue = false;
#ifdef USE_ZEROCONF
  int useZeroconf = getConfigInt(PREFERENCES, CONFIG_ZEROCONF);
  if (useZeroconf && _dbus != NULL) {
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
      //_gui->sendMessage("100",sequenceId,arg);
      arg.pop_front(); // remove the first, the name

      TXTRecordMap record = iter->second.getTXTRecords();
      TXTRecordMap::iterator iterTXT = record.begin();
      while(iterTXT!=record.end()) {
        argTXT.clear();
        argTXT.push_back(iter->first);
        argTXT.push_back(iterTXT->first);
        argTXT.push_back(iterTXT->second);
        argTXT.push_back(newTXT);
       // _gui->sendMessage("101",sequenceId,argTXT);
        iterTXT++;
      }
      iter++;
    }
    returnValue = true;
  }
#else
  (void)sequenceId;
#endif
  return returnValue;
}

/**
 * Main Thread
 */
bool 
ManagerImpl::attachZeroconfEvents(const std::string& sequenceId, Pattern::Observer& observer)
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
#else
  (void)sequenceId;
  (void)observer;
#endif
  return returnValue;
}
bool
ManagerImpl::detachZeroconfEvents(Pattern::Observer& observer)
{
  bool returnValue = false;
#ifdef USE_ZEROCONF
  if (_DNSService) {
    _DNSService->detach(observer);
    returnValue = true;
  }
#else
  (void)observer;
#endif
  return returnValue;
}

/**
 * Main Thread
 *
 * @todo When is this called ? Why this name 'getEvents' ?
 */
/**
 * DEPRECATED
bool
ManagerImpl::getEvents() {
  initRegisterAccounts();
  return true;
}
*/

// TODO: rewrite this
/**
 * Main Thread
 */
bool 
ManagerImpl::getCallStatus(const std::string& sequenceId)
{
  if (!_dbus) { return false; }
  ost::MutexLock m(_callAccountMapMutex);
  CallAccountMap::iterator iter = _callAccountMap.begin();
  TokenList tk;
  std::string code;
  std::string status;
  std::string destination;  
  std::string number;

  while (iter != _callAccountMap.end())
  {
    Call* call = getAccountLink(iter->second)->getCall(iter->first);
    Call::ConnectionState state = call->getConnectionState();
    if (state != Call::Connected) {
      switch(state) {
        case Call::Trying:       code="110"; status = "Trying";       break;
        case Call::Ringing:      code="111"; status = "Ringing";      break;
        case Call::Progressing:  code="125"; status = "Progressing";  break;
        case Call::Disconnected: code="125"; status = "Disconnected"; break;
        default: code=""; status= "";
      }
    } else {
      switch (call->getState()) {
        case Call::Active:       code="112"; status = "Established";  break;
        case Call::Hold:         code="114"; status = "Held";         break;
        case Call::Busy:         code="113"; status = "Busy";         break;
        case Call::Refused:      code="125"; status = "Refused";      break;
        case Call::Error:        code="125"; status = "Error";        break;
        case Call::Inactive:     code="125"; status = "Inactive";     break;
      }
    }

    // No Congestion
    // No Wrong Number
    // 116 <CSeq> <call-id> <acc> <destination> Busy
    destination = call->getPeerName();
    number = call->getPeerNumber();
    if (number!="") {
      destination.append(" <");
      destination.append(number);
      destination.append(">");
    }
    tk.push_back(iter->second);
    tk.push_back(destination);
    tk.push_back(status);
    //_gui->sendCallMessage(code, sequenceId, iter->first, tk);
    tk.clear();

    iter++;
  }
  
  return true;
}

//THREAD=Main
/* Unused, Deprecated */
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
    //_gui->sendMessage("100", sequenceId, tk);
    tk = iter.next();
  }
  return returnValue;
}

//THREAD=Main
bool 
ManagerImpl::getConfig(const std::string& section, const std::string& name, TokenList& arg)
{
  return _config.getConfigTreeItemToken(section, name, arg);
}

//THREAD=Main
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

//THREAD=Main
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

//THREAD=Main
bool 
ManagerImpl::setConfig(const std::string& section, const std::string& name, const std::string& value)
{
  return _config.setConfigTreeItem(section, name, value);
}

//THREAD=Main
bool 
ManagerImpl::setConfig(const std::string& section, const std::string& name, int value)
{
  std::ostringstream valueStream;
  valueStream << value;
  return _config.setConfigTreeItem(section, name, valueStream.str());
}

//THREAD=Main
bool 
ManagerImpl::getConfigList(const std::string& sequenceId, const std::string& name)
{
  /*
  bool returnValue = false;
  TokenList tk;
  if (name == "codecdescriptor") {

    CodecMap map = _codecDescriptorMap.getCodecMap();
    CodecMap::iterator iter = map.begin();
    while( iter != map.end() ) {
      tk.clear();
      std::ostringstream strType;
      strType << iter->first;
      tk.push_back(strType.str());
      if (iter->second != -1) {
        tk.push_back(iter->second);
      } else {
        tk.push_back(strType.str());
      }
     // _gui->sendMessage("100", sequenceId, tk);
      iter++;
    }
    returnValue = true;
  } else if (name == "ringtones") {
    // add empty line
    std::ostringstream str;
    str << 1;
    tk.push_back(str.str());
    tk.push_back(""); // filepath
    //_gui->sendMessage("100", sequenceId, tk);

    // share directory
    std::string path = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR;
    int nbFile = 1;
    returnValue = getDirListing(sequenceId, path, &nbFile);

    // home directory
    path = std::string(HOMEDIR) + DIR_SEPARATOR_STR + "." + PROGDIR + DIR_SEPARATOR_STR + RINGDIR;
    getDirListing(sequenceId, path, &nbFile);
  } else if (name == "audiodevice") {
    returnValue = getAudioDeviceList(sequenceId, AudioLayer::InputDevice | AudioLayer::OutputDevice);
  } else if (name == "audiodevicein") {
    returnValue = getAudioDeviceList(sequenceId, AudioLayer::InputDevice);
  } else if (name == "audiodeviceout") {
    returnValue = getAudioDeviceList(sequenceId, AudioLayer::OutputDevice);
  } else if (name == "countrytones") {
    returnValue = getCountryTones(sequenceId);
  }
  return returnValue;*/
  return true;
}

//THREAD=Main
bool 
ManagerImpl::getAudioDeviceList(const std::string& sequenceId, int ioDeviceMask) 
{
  AudioLayer* audiolayer = getAudioDriver();
  if (audiolayer == 0) { return false; }

  bool returnValue = false;
  
  // TODO: test when there is an error on initializing...
  TokenList tk;
  AudioDevice* device = 0;
  int nbDevice = audiolayer->getDeviceCount();
  
  for (int index = 0; index < nbDevice; index++ ) {
    device = audiolayer->getAudioDeviceInfo(index, ioDeviceMask);
    if (device != 0) {
      tk.clear();
      std::ostringstream str; str << index; tk.push_back(str.str());
      tk.push_back(device->getName());
      tk.push_back(device->getApiName());
      std::ostringstream rate; rate << (int)(device->getRate()); tk.push_back(rate.str());
      //_gui->sendMessage("100", sequenceId, tk);

      // don't forget to delete it after
      delete device; device = 0;
    }
  }
  returnValue = true;
  
  std::ostringstream rate; 
  rate << "VARIABLE";
  tk.clear();
  tk.push_back(rate.str());
  //_gui->sendMessage("101", sequenceId, tk);

  return returnValue;
}

//THREAD=Main
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

//THREAD=Main
void 
ManagerImpl::sendCountryTone(const std::string& sequenceId, int index, const std::string& name) {
  TokenList tk;
  std::ostringstream str; str << index; tk.push_back(str.str());
  tk.push_back(name);
  //_gui->sendMessage("100", sequenceId, tk);
}

//THREAD=Main
bool
ManagerImpl::getDirListing(const std::string& sequenceId, const std::string& path, int *nbFile) {
  TokenList tk;
  try {
    ost::Dir dir(path.c_str());
    const char *cFileName = 0;
    std::string fileName;
    std::string filePathName;
    while ( (cFileName=dir++) != 0 ) {
      fileName = cFileName;
      filePathName = path + DIR_SEPARATOR_STR + cFileName;
      if (fileName.length() && fileName[0]!='.' && !ost::isDir(filePathName.c_str())) {
        tk.clear();
        std::ostringstream str;
        str << (*nbFile);
        tk.push_back(str.str());
        tk.push_back(filePathName);
        //_gui->sendMessage("100", sequenceId, tk);
        (*nbFile)++;
      }
    }
    return true;
  } catch (...) {
    // error to open file dir
    return false;
  }
}

std::vector< std::string > 
ManagerImpl::getAccountList() 
{
  std::vector< std::string > v; 
    
  AccountMap::iterator iter = _accountMap.begin();
  while ( iter != _accountMap.end() ) {
    if ( iter->second != 0 ) {
      _debug("Account List: %s\n", iter->first.data()); 
      v.push_back(iter->first.data());
        
    }
    iter++;
  }
  _debug("Size: %d\n", v.size());
  return v;
}



std::map< std::string, std::string > 
ManagerImpl::getAccountDetails(const AccountID& accountID) 
{
  std::map<std::string, std::string> a;
  std::string accountType;
  enum VoIPLink::RegistrationState state = _accountMap[accountID]->getRegistrationState();
    
  accountType = getConfigString(accountID, CONFIG_ACCOUNT_TYPE);

  a.insert(
    std::pair<std::string, std::string>(
      CONFIG_ACCOUNT_ALIAS, 
      getConfigString(accountID, CONFIG_ACCOUNT_ALIAS)
      )
    );
  /*a.insert(
    std::pair<std::string, std::string>(
      CONFIG_ACCOUNT_AUTO_REGISTER, 
      getConfigString(accountID, CONFIG_ACCOUNT_AUTO_REGISTER)== "1" ? "TRUE": "FALSE"
      )
    );*/
  a.insert(
    std::pair<std::string, std::string>(
      CONFIG_ACCOUNT_ENABLE, 
      getConfigString(accountID, CONFIG_ACCOUNT_ENABLE) == "1" ? "TRUE": "FALSE"
      )
    );
  a.insert(
    std::pair<std::string, std::string>(
      "Status", 
      (state == VoIPLink::Registered ? "REGISTERED":
        (state == VoIPLink::Unregistered ? "UNREGISTERED":
          (state == VoIPLink::Trying ? "TRYING":
            (state == VoIPLink::Error ? "ERROR": "ERROR"))))
      )
    );
  a.insert(
    std::pair<std::string, std::string>(
      CONFIG_ACCOUNT_TYPE, accountType
      )
    );

  if (accountType == "SIP") {
    a.insert(
      std::pair<std::string, std::string>(
        SIP_FULL_NAME, 
        getConfigString(accountID, SIP_FULL_NAME)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_USER_PART, 
        getConfigString(accountID, SIP_USER_PART)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_AUTH_NAME, 
        getConfigString(accountID, SIP_AUTH_NAME)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_PASSWORD, 
        getConfigString(accountID, SIP_PASSWORD)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_HOST_PART, 
        getConfigString(accountID, SIP_HOST_PART)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_PROXY, 
        getConfigString(accountID, SIP_PROXY)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_STUN_SERVER, 
        getConfigString(accountID, SIP_STUN_SERVER)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        SIP_USE_STUN, 
        getConfigString(accountID, SIP_USE_STUN) == "1" ? "TRUE": "FALSE"
        )
      );
  }
  else if (accountType == "IAX") {
    a.insert(
      std::pair<std::string, std::string>(
        IAX_FULL_NAME, 
        getConfigString(accountID, IAX_FULL_NAME)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        IAX_HOST, 
        getConfigString(accountID, IAX_HOST)
        )
      );    
    a.insert(
      std::pair<std::string, std::string>(
        IAX_USER, 
        getConfigString(accountID, IAX_USER)
        )
      );
    a.insert(
      std::pair<std::string, std::string>(
        IAX_PASS, 
        getConfigString(accountID, IAX_PASS)
        )
      );
  }
  else {
    // Unknown type
    _debug("Unknown account type in getAccountDetails(): %s", accountType.c_str());
  }
     
  return a;
}

void 
ManagerImpl::setAccountDetails( const ::DBus::String& accountID, 
				const std::map< ::DBus::String, ::DBus::String >& details )
{
  std::string accountType = (*details.find(CONFIG_ACCOUNT_TYPE)).second;

  setConfig(accountID, CONFIG_ACCOUNT_ALIAS, (*details.find(CONFIG_ACCOUNT_ALIAS)).second);
  //setConfig(accountID, CONFIG_ACCOUNT_AUTO_REGISTER, 
  // (*details.find(CONFIG_ACCOUNT_AUTO_REGISTER)).second == "TRUE" ? "1": "0" );
  setConfig(accountID, CONFIG_ACCOUNT_ENABLE, 
	    (*details.find(CONFIG_ACCOUNT_ENABLE)).second == "TRUE" ? "1": "0" );
  setConfig(accountID, CONFIG_ACCOUNT_TYPE, accountType);

  if (accountType == "SIP") {
    setConfig(accountID, SIP_FULL_NAME, (*details.find(SIP_FULL_NAME)).second);
    setConfig(accountID, SIP_USER_PART, (*details.find(SIP_USER_PART)).second);
    setConfig(accountID, SIP_AUTH_NAME, (*details.find(SIP_AUTH_NAME)).second);
    setConfig(accountID, SIP_PASSWORD,  (*details.find(SIP_PASSWORD)).second);
    setConfig(accountID, SIP_HOST_PART, (*details.find(SIP_HOST_PART)).second);
    //setConfig(accountID, SIP_PROXY,     (*details.find(SIP_PROXY)).second);
    //setConfig(accountID, SIP_STUN_SERVER,(*details.find(SIP_STUN_SERVER)).second);
    //setConfig(accountID, SIP_USE_STUN,
	  //    (*details.find(SIP_USE_STUN)).second == "TRUE" ? "1" : "0");
  }
  else if (accountType == "IAX") {
    setConfig(accountID, IAX_FULL_NAME, (*details.find(IAX_FULL_NAME)).second);
    setConfig(accountID, IAX_HOST,      (*details.find(IAX_HOST)).second);
    setConfig(accountID, IAX_USER,      (*details.find(IAX_USER)).second);
    setConfig(accountID, IAX_PASS,      (*details.find(IAX_PASS)).second);    
  } else {
    _debug("Unknown account type in setAccountDetails(): %s\n", accountType.c_str());
  }
  
  saveConfig();

  /*
   * register if it was just enabled, and we hadn't registered
   * unregister if it was enabled/registered, and we want it closed
   */
  Account* acc = getAccount(accountID);

  acc->loadConfig();
  if (acc->isEnabled()) {
    // Verify we aren't already registered, then register
    if (acc->getRegistrationState() == VoIPLink::Unregistered) {
      acc->registerVoIPLink();
    }
  } else {
    // Verify we are already registered, then unregister
    if (acc->getRegistrationState() == VoIPLink::Registered) {
      acc->unregisterVoIPLink();
    }
  }

  /** @todo Make the daemon use the new settings */
  if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
}                   


void
ManagerImpl::addAccount(const std::map< ::DBus::String, ::DBus::String >& details)
{
  /** @todo Deal with both the _accountMap and the Configuration */
  std::string accountType = (*details.find(CONFIG_ACCOUNT_TYPE)).second;
  Account* newAccount;
  std::stringstream accountID;
  accountID << "Account:" << time(NULL);
  AccountID newAccountID = accountID.str();
  /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

  if (accountType == "SIP") {
    newAccount = AccountCreator::createAccount(AccountCreator::SIP_ACCOUNT, newAccountID);
  }
  else if (accountType == "IAX") {
    newAccount = AccountCreator::createAccount(AccountCreator::IAX_ACCOUNT, newAccountID);
  }
  else {
    _debug("Unknown %s param when calling addAccount(): %s\n", CONFIG_ACCOUNT_TYPE, accountType.c_str());
    return;
  }
  _accountMap[newAccountID] = newAccount;

  setAccountDetails(accountID.str(), details);

  saveConfig();
  
  if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
}

void 
ManagerImpl::removeAccount(const AccountID& accountID) 
{
  // Get it down and dying
  Account* remAccount = getAccount(accountID);

  if (remAccount) {
    remAccount->unregisterVoIPLink();
    _accountMap.erase(accountID);
    delete remAccount;
  }

  _config.removeSection(accountID);

  saveConfig();
  
  if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
}

std::string  
ManagerImpl::getDefaultAccount()
{
	
	std::string id;
	id = getConfigString(PREFERENCES, "DefaultAccount");
	_debug("Default Account = %s\n",id.c_str());	
	return id;
}

void
ManagerImpl::setDefaultAccount(const AccountID& accountID)
{
	// we write into the Preferences section the field Default
	setConfig("Preferences", "DefaultAccount", accountID);
}




//THREAD=Main
/*
 * Experimental...
 */
bool
ManagerImpl::setSwitch(const std::string& switchName, std::string& message) {
  AudioLayer* audiolayer = 0;
  if (switchName == "audiodriver" ) {
    // hangup all call here 
    audiolayer = getAudioDriver();

    int oldSampleRate = 0;
    if (audiolayer) { oldSampleRate = audiolayer->getSampleRate(); }

    selectAudioDriver();
    audiolayer = getAudioDriver();

    if (audiolayer) {
      std::string error = audiolayer->getErrorMessage();
      int newSampleRate = audiolayer->getSampleRate();

      if (!error.empty()) {
        message = error;
        return false;
      }

      if (newSampleRate != oldSampleRate) {
        _toneMutex.enterMutex();

        _debug("Unload Telephone Tone\n");
        delete _telephoneTone; _telephoneTone = NULL;
        _debug("Unload DTMF Key\n");
        delete _dtmfKey; _dtmfKey = NULL;

        _debug("Load Telephone Tone\n");
        std::string country = getConfigString(PREFERENCES, ZONE_TONE);
        _telephoneTone = new TelephoneTone(country, newSampleRate);

        _debugInit("Loading DTMF key");
        _dtmfKey = new DTMF(newSampleRate);

        _toneMutex.leaveMutex();
      }

      message = _("Change with success");
      playDtmf('9');
      getAudioDriver()->sleep(300); // in milliseconds
      playDtmf('1');
      getAudioDriver()->sleep(300); // in milliseconds
      playDtmf('1');
      return true;
    }
  } else if ( switchName == "echo" ) {
    audiolayer = getAudioDriver();
    if (audiolayer) {
      audiolayer->toggleEchoTesting();
      return true;
    }
  }


  return false;
}

// ACCOUNT handling
bool
ManagerImpl::associateCallToAccount(const CallID& callID, const AccountID& accountID)
{
  if (getAccountFromCall(callID) == AccountNULL) { // nothing with the same ID
    if (  accountExists(accountID)  ) { // account id exist in AccountMap
      ost::MutexLock m(_callAccountMapMutex);
      _callAccountMap[callID] = accountID;
      return true;
    } else {
      return false; 
    }
  } else {
    return false;
  }
}

AccountID
ManagerImpl::getAccountFromCall(const CallID& callID)
{
  ost::MutexLock m(_callAccountMapMutex);
  CallAccountMap::iterator iter = _callAccountMap.find(callID);
  if ( iter == _callAccountMap.end()) {
    return AccountNULL;
  } else {
    return iter->second;
  }
}

bool
ManagerImpl::removeCallAccount(const CallID& callID)
{
  ost::MutexLock m(_callAccountMapMutex);
  if ( _callAccountMap.erase(callID) ) {
    return true;
  }
  return false;
}

CallID 
ManagerImpl::getNewCallID() 
{
  std::ostringstream random_id("s");
  random_id << (unsigned)rand();
  
  // when it's not found, it return ""
  // generate, something like s10000s20000s4394040
  while (getAccountFromCall(random_id.str()) != AccountNULL) {
    random_id.clear();
    random_id << "s";
    random_id << (unsigned)rand();
  }
  return random_id.str();
}

short
ManagerImpl::loadAccountMap()
{
  _debugStart("Load account:");
  short nbAccount = 0;
  TokenList sections = _config.getSections();
  std::string accountType;
  Account* tmpAccount;

  // iter = std::string
  TokenList::iterator iter = sections.begin();
  while(iter != sections.end()) {
    // Check if it starts with "Account:" (SIP and IAX pour le moment)
    if (iter->find("Account:") == -1) {
      iter++;
      continue;
    }

    accountType = getConfigString(*iter, CONFIG_ACCOUNT_TYPE);
    if (accountType == "SIP") {
      tmpAccount = AccountCreator::createAccount(AccountCreator::SIP_ACCOUNT, *iter);
    }
    else if (accountType == "IAX") {
      tmpAccount = AccountCreator::createAccount(AccountCreator::IAX_ACCOUNT, *iter);
    }
    else {
      _debug("Unknown %s param in config file (%s)\n", CONFIG_ACCOUNT_TYPE, accountType.c_str());
    }

    if (tmpAccount != NULL) {
      _debugMid(" %s ", iter->c_str());
      _accountMap[iter->c_str()] = tmpAccount;
      nbAccount++;
    }

    _debugEnd("\n");

    iter++;
  }


  /*
  // SIP Loading X account...
  short nbAccountSIP = ACCOUNT_SIP_COUNT_DEFAULT;
  for (short iAccountSIP = 0; iAccountSIP<nbAccountSIP; iAccountSIP++) {
    std::ostringstream accountName;
    accountName << "SIP" << iAccountSIP;
    
    tmpAccount = AccountCreator::createAccount(AccountCreator::SIP_ACCOUNT, accountName.str());
     if (tmpAccount!=0) {
       _debugMid(" %s", accountName.str().data());
       _accountMap[accountName.str()] = tmpAccount;
      nbAccount++;
    }
  }

  // IAX Loading X account...
  short nbAccountIAX = ACCOUNT_IAX_COUNT_DEFAULT;
  for (short iAccountIAX = 0; iAccountIAX<nbAccountIAX; iAccountIAX++) {
    std::ostringstream accountName;
    accountName << "IAX" << iAccountIAX;
    tmpAccount = AccountCreator::createAccount(AccountCreator::IAX_ACCOUNT, accountName.str());
    if (tmpAccount!=0) {
       _debugMid(" %s", accountName.str().data());
       _accountMap[accountName.str()] = tmpAccount;
      nbAccount++;
    }
  }
  _debugEnd("\n");
  */

  return nbAccount;
}

void
ManagerImpl::unloadAccountMap()
{
  _debug("Unloading account map...\n");
  AccountMap::iterator iter = _accountMap.begin();
  while ( iter != _accountMap.end() ) {
    _debug("-> Deleting account %s\n", iter->first.c_str());
    delete iter->second; iter->second = 0;
    iter++;
  }
  _accountMap.clear();
}

bool
ManagerImpl::accountExists(const AccountID& accountID)
{
  AccountMap::iterator iter = _accountMap.find(accountID);
  if ( iter == _accountMap.end() ) {
    return false;
  }
  return true;
}

Account*
ManagerImpl::getAccount(const AccountID& accountID)
{
  AccountMap::iterator iter = _accountMap.find(accountID);
  if ( iter == _accountMap.end() ) {
    return 0;
  }
  return iter->second;
}

VoIPLink* 
ManagerImpl::getAccountLink(const AccountID& accountID)
{
  Account* acc = getAccount(accountID);
  if ( acc ) {
    return acc->getVoIPLink();
  }
  return 0;
}


#ifdef TEST
/** 
 * Test accountMap
 */
bool ManagerImpl::testCallAccountMap()
{
  if ( getAccountFromCall(1) != AccountNULL ) {
    _debug("TEST: getAccountFromCall with empty list failed\n");
  }
  if ( removeCallAccount(1) != false ) {
    _debug("TEST: removeCallAccount with empty list failed\n");
  }
  CallID newid = getNewCallID();
  if ( associateCallToAccount(newid, "acc0") == false ) {
    _debug("TEST: associateCallToAccount with new CallID empty list failed\n");
  }
  if ( associateCallToAccount(newid, "acc1") == true ) {
    _debug("TEST: associateCallToAccount with a known CallID failed\n");
  }
  if ( getAccountFromCall( newid ) != "acc0" ) {
    _debug("TEST: getAccountFromCall don't return the good account id\n");
  }
  CallID secondnewid = getNewCallID();
  if ( associateCallToAccount(secondnewid, "xxxx") == true ) {
    _debug("TEST: associateCallToAccount with unknown account id failed\n");
  }
  if ( removeCallAccount( newid ) != true ) {
    _debug("TEST: removeCallAccount don't remove the association\n");
  }

  return true;
}

/**
 * Test AccountMap
 */
bool ManagerImpl::testAccountMap() 
{
  if (loadAccountMap() != 2) {
    _debug("TEST: loadAccountMap didn't load 2 account\n");
  }
  if (accountExists("acc0") == false) {
    _debug("TEST: accountExists didn't find acc0\n");
  }
  if (accountExists("accZ") != false) {
    _debug("TEST: accountExists found an unknown account\n");
  }
  if (getAccount("acc0") == 0) {
    _debug("TEST: getAccount didn't find acc0\n");
  }
  if (getAccount("accZ") != 0) {
    _debug("TEST: getAccount found an unknown account\n");
  }
  unloadAccountMap();
  if ( accountExists("acc0") == true ) {
    _debug("TEST: accountExists found an account after unloadAccount\n");
  }
  return true;
}
#endif
