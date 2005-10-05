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

# include <sys/types.h> // mkdir(2)
# include <sys/stat.h>	// mkdir(2)

//#include <sys/socket.h> // inet_ntoa()
//#include <netinet/in.h>
//#include <arpa/inet.h>

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
#include "call.h"
#include "error.h"
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

using namespace std;
using namespace ost;
 
ManagerImpl::ManagerImpl (void)
{
  // initialize random generator  
  srand (time(NULL));
  
  // Init private variables 
  _error = new Error();
  _tone = new ToneGenerator();	
  _hasZeroconf = false;
#ifdef USE_ZEROCONF
  _hasZeroconf = true;
  _DNSService = new DNSService();
#endif

  _nCalls = 0;
  _nCodecs = 0;
  _currentCallId = 0;
  _startTime = 0;
  _endTime = 0;
  _path = ""; 
  _exist = 0;
  _loaded = false;
  _gui = NULL;
  _audiodriverPA = NULL;

  // Initialize after by init() -> initVolume()
  _spkr_volume = 0;
  _mic_volume  = 0; 
  _mic_volume_before_mute = 0;

  _toneType = 0;

  _nbIncomingWaitingCall=0;

  _codecMap = CodecDescriptorMap().getMap();
}

ManagerImpl::~ManagerImpl (void) 
{
  terminate();
  for(VoIPLinkVector::iterator pos = _voIPLinkVector.begin();
      pos != _voIPLinkVector.end();
      pos++) {
    delete *pos;
  }

  for(CallVector::iterator pos = _callVector.begin();
      pos != _callVector.end();
      pos++) {
    delete *pos;
  }

  unloadAudioCodec();

  delete _audiodriverPA;
  delete _tone;
  delete _error;

#ifdef USE_ZEROCONF
  delete _DNSService;
#endif
} 

void 
ManagerImpl::init (void) 
{
  terminate();
  initZeroconf();
  initVolume();

  if (_exist == 0) {
    _debug("Cannot create config file in your home directory\n");
  } 

  try {
    selectAudioDriver();
    loaded(true);
  }
  catch (const portaudio::PaException &e)
    {
      displayError(e.paErrorText());
      throw e;
    }
  catch (const portaudio::PaCppException &e)
    {
      displayError(e.what());
      throw e;
    }
  catch (const exception &e)
    {
      displayError(e.what());
      throw e;
    }
  catch (...)
    { 
      displayError("An unknown exception occured.");
      throw;
    }

  initAudioCodec();

  // Set a sip voip link by default
  _voIPLinkVector.push_back(new SipVoIPLink(DFT_VOIP_LINK));
  _voIPLinkVector.at(DFT_VOIP_LINK)->init();

  if (_voIPLinkVector.at(DFT_VOIP_LINK)->checkNetwork()) {
    // If network is available

    if (getConfigInt(SIGNALISATION, AUTO_REGISTER) && _exist == 1) {
      if (registerVoIPLink() != 1) {
	_debug("Registration failed\n");
      }
    } 
  }
  
}

void ManagerImpl::terminate()
{
  for(VoIPLinkVector::iterator pos = _voIPLinkVector.begin();
      pos != _voIPLinkVector.end();
      pos++) {
    (*pos)->terminate();
  }

  _voIPLinkVector.clear();
}

void
ManagerImpl::setGui (GuiFramework* gui)
{
	_gui = gui;
  initGui();
}

/**
 * Gui initialisation (after setting the gui)
 */
void 
ManagerImpl::initGui() {
  if (_exist == 2) {
    // If config-file doesn't exist, launch configuration setup
    _gui->setup();
  }
}

Error*
ManagerImpl::error (void) 
{
	return _error;
}

AudioLayer*
ManagerImpl::getAudioDriver(void) 
{
	return _audiodriverPA;
}

unsigned int 
ManagerImpl::getNumberOfCalls (void)
{
	return _nCalls;
}

void 
ManagerImpl::setNumberOfCalls (unsigned int nCalls)
{
	_nCalls = nCalls;
}

short
ManagerImpl::getCurrentCallId (void)
{
	return _currentCallId;
}

void 
ManagerImpl::setCurrentCallId (short currentCallId)
{
	_currentCallId = currentCallId;
}

CallVector* 
ManagerImpl::getCallVector (void)
{
	return &_callVector;
}

CodecDescriptorVector* 
ManagerImpl::getCodecDescVector (void)
{
	return &_codecDescVector;
}

Call*
ManagerImpl::getCall (short id)
{
	if (id > 0 and _callVector.size() > 0) {
		for (unsigned int i = 0; i < _nCalls; i++) {
			if (_callVector.at(i)->getId() == id) {
				return _callVector.at(i);
			}
		}
		return NULL;
	} else {
		return NULL;
	}
}


unsigned int
ManagerImpl::getNumberOfCodecs (void)
{
	return _nCodecs;
}

void
ManagerImpl::setNumberOfCodecs (unsigned int nb_codec)
{
	_nCodecs = nb_codec;
}

VoIPLinkVector* 
ManagerImpl::getVoIPLinkVector (void)
{
	return &_voIPLinkVector;
}

/**
 * Multi Thread
 */
Call *
ManagerImpl::pushBackNewCall (short id, enum CallType type)
{
	Call* call = new Call(id, type, _voIPLinkVector.at(DFT_VOIP_LINK));
	// Set the wanted voip-link (first of the list)
	_callVector.push_back(call);
  return call;
}

void
ManagerImpl::deleteCall (short id)
{
  CallVector::iterator iter = _callVector.begin();

  while(iter!=_callVector.end()) {
    if ((*iter)->getId() == id) {
      delete (*iter);
      _callVector.erase(iter);
      return;
    }
    iter++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/**
 * Main thread
 */
int 
ManagerImpl::outgoingCall (const string& to)
{	
  short id = generateNewCallId();
  Call *call = pushBackNewCall(id, Outgoing);
  _debug("Outgoing Call with identifiant %d\n", id);

  call->setStatus(string(TRYING_STATUS));
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
ManagerImpl::hangupCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL) {
		return -1;
	}
	call->setStatus(string(HANGUP_STATUS));
	call->setState(Call::Hungup);

	int result = call->hangup();

	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();

	deleteCall(id);
  return result;
}

/**
 * User action (main thread)
 * Every Call
 */
int
ManagerImpl::cancelCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(HANGUP_STATUS));
	call->setState(Call::Hungup);
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
  decWaitingCall();
  stopTone();
	return call->cancel();
}

/**
 * User action (main thread)
 * Incoming Call
 */
int 
ManagerImpl::answerCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL)
		return -1;

	call->setStatus(string(CONNECTED_STATUS));
	call->setState(Call::Answered);

  decWaitingCall();
  stopTone();
  switchCall(id);
	return call->answer();
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::onHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(ONHOLD_STATUS));
	call->setState(Call::OnHold);
	return call->onHold();
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::offHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(Call::OffHold);
  setCurrentCallId(id);
	return call->offHold();	
}

/**
 * User action (main thread)
 * Every Call
 */
int 
ManagerImpl::transferCall (short id, const string& to)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(TRANSFER_STATUS));
	call->setState(Call::Transfered);
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
  _mic_volume = 0;
}

/**
 * User action (main thread)
 * All Call
 */
void
ManagerImpl::unmute() {
  _mic_volume = _mic_volume_before_mute;
}

/**
 * User action (main thread)
 * Every Call
 */
void 
ManagerImpl::muteOn (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return;
	call->setStatus(string(MUTE_ON_STATUS));
	call->setState(Call::MuteOn);
}

/**
 * User action (main thread)
 * Every Call
 */
void 
ManagerImpl::muteOff (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return;
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(Call::MuteOff);
}

/**
 * User action (main thread)
 * Call Incoming
 */
int 
ManagerImpl::refuseCall (short id)
{
  stopTone();
	Call *call;
	call = getCall(id);
	if (call == NULL)
		return -1;
  // don't cause a very bad segmentation fault
  // we refuse the call when we are trying to establish connection
  decWaitingCall();

  if ( call->getState() != Call::Progressing )
    return -1;

  call->setStatus(string(HANGUP_STATUS));
  call->setState(Call::Refused);
  
  _mutex.enterMutex();
  _nCalls -= 1;
  _mutex.leaveMutex();

  setCurrentCallId(0);
	int refuse = call->refuse();
  deleteCall(id);
  return refuse;
}

/**
 * User action (main thread)
 */
int
ManagerImpl::saveConfig (void)
{
  return (_config.saveConfigTree(_path.data()) ? 1 : 0);
}

/**
 * Initialize action (main thread)
 */
int 
ManagerImpl::registerVoIPLink (void)
{
	if (_voIPLinkVector.at(DFT_VOIP_LINK)->setRegister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * Terminate action (main thread)
 */
int 
ManagerImpl::unregisterVoIPLink (void)
{
	if (_voIPLinkVector.at(DFT_VOIP_LINK)->setUnregister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * Terminate action (main thread)
 */
int 
ManagerImpl::quitApplication (void)
{
  // Quit VoIP-link library
  terminate();
  return 0;
}

/**
 * ??? action
 */
int 
ManagerImpl::sendTextMessage (short , const string& )
{
	return 1;
}

/**
 * ??? action
 */
int 
ManagerImpl::accessToDirectory (void)
{
	return 1;
}

/**
 * User action (main thread)
 */
bool 
ManagerImpl::sendDtmf (short id, char code)
{
  int sendType = getConfigInt(SIGNALISATION, SEND_DTMF_AS);
  switch (sendType) {
        // SIP INFO
        case 0:
			_voIPLinkVector.at(DFT_VOIP_LINK)->carryingDTMFdigits(id, code);
			return true;
            break;
                                                                                
        // Audio way
        case 1:
			return false;
            break;
                                                                                
        // rfc 2833
        case 2:
			return false;
            break;
                                                                                
        default:
			return -1;
            break;
    }
}

/**
 * User action (main thread)
 */
bool
ManagerImpl::playDtmf(char code)
{
  stopTone();

  int16* _buf = new int16[SIZEBUF];
  bool returnValue = false;

  // Handle dtmf
  _key.startTone(code);
  if ( _key.generateDTMF(_buf, SAMPLING_RATE) ) {

    int k, spkrVolume;
    int16* buf_ctrl_vol;

    // Determine dtmf pulse length
    int pulselen = getConfigInt(SIGNALISATION, PULSE_LENGTH);
    int size = pulselen * (OCTETS /1000);
  
    buf_ctrl_vol = new int16[size*CHANNELS];
    spkrVolume = getSpkrVolume();
  
    // Control volume and format mono->stereo
    for (int j = 0; j < size; j++) {
      k = j*2;
      buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = _buf[j] * spkrVolume/100;
    }
  
    AudioLayer *audiolayer = getAudioDriver();
    _mutex.enterMutex();
    audiolayer->urgentRingBuffer().flush();

    // Put buffer to urgentRingBuffer 
    audiolayer->urgentRingBuffer().Put(buf_ctrl_vol, size * CHANNELS);

    // We activate the stream if it's not active yet.
    if (!audiolayer->isStreamActive()) {
      audiolayer->startStream();
      audiolayer->sleep(pulselen);
      audiolayer->urgentRingBuffer().flush();
      audiolayer->stopStream();
    } else {
      audiolayer->sleep(pulselen);
    }
    _mutex.leaveMutex();
    //setZonetone(false);
    delete[] buf_ctrl_vol;
    returnValue = true;
  }
  delete[] _buf;
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
}

void
ManagerImpl::decWaitingCall() {
  ost::MutexLock m(_incomingCallMutex);
  _nbIncomingWaitingCall--;
}

/**
 * SipEvent Thread
 */
int 
ManagerImpl::incomingCall (short id)
{
	Call* call = getCall(id);
	if (call == NULL)
		return -1;

	call->setType(Incoming);
	call->setStatus(string(RINGING_STATUS));
	call->setState(Call::Progressing);

  incWaitingCall();
	ringtone();

  // TODO: Account not yet implemented
  std::string accountId = "acc1";
  std::string from = call->getCallerIdName();
  std::string number = call->getCallerIdNumber();
  if ( number.length() ) {
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
ManagerImpl::peerAnsweredCall (short id)
{
  stopTone();

  Call* call = getCall(id);
  call->setStatus(string(CONNECTED_STATUS));
  call->setState(Call::Answered);

  // switch current call
  switchCall(id);
  _gui->peerAnsweredCall(id);
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
int
ManagerImpl::peerRingingCall (short id)
{
  Call* call = getCall(id);
  call->setStatus(string(RINGING_STATUS));
  call->setState(Call::Ringing);

  // ring
  ringback();
  _gui->peerRingingCall(id);
  return 1;
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
int 
ManagerImpl::peerHungupCall (short id)
{
  stopTone();
  Call* call = getCall(id);
  // TODO: check if it hungup when waiting or in a conversation
  //       to decWaitingCall ?
  if ( call->getState() != Call::Ringing) {
    decWaitingCall();
  }

  call->setStatus(string(HANGUP_STATUS));
  call->setState(Call::Hungup);

  _gui->peerHungupCall(id);

  // end up call
  _mutex.enterMutex();
  _nCalls -= 1;
  _mutex.leaveMutex();
  deleteCall(id);
  setCurrentCallId(0);
  return 1;
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayTextMessage (short id, const string& message)
{
  if(_gui) {
    _gui->displayTextMessage(id, message);
  }
  else {
    std::cout << message << std::endl;
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayErrorText (short id, const string& message)
{
  if(_gui) {
    _gui->displayErrorText(id, message);
  }
  else {
    std::cerr << message << std::endl;
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayError (const string& error)
{
  if(_gui) {
    _gui->displayStatus(error);
  }
  else {
    std::cerr << error << std::endl;
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void 
ManagerImpl::displayStatus (const string& status)
{
  if(_gui) {
    _gui->displayStatus(status);
  }
  else {
    std::cout<< status << std::endl;
  }
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void
ManagerImpl::startVoiceMessageNotification (const std::string& nb_msg)
{
  //_gui->startVoiceMessageNotification();
  _gui->sendVoiceNbMessage(nb_msg);
}

/**
 * SipEvent Thread
 * for outgoing call, send by SipEvent
 */
void
ManagerImpl::stopVoiceMessageNotification (void)
{
	//_gui->stopVoiceMessageNotification();
  _gui->sendVoiceNbMessage(std::string("0"));
}

/**
 * Multi Thread
 */
bool 
ManagerImpl::playATone(unsigned int tone) {
  if (isDriverLoaded()) {
    ost::MutexLock m(_toneMutex); 
    _toneType = tone;
    _tone->toneHandle(_toneType, getConfigString(PREFERENCES, ZONE_TONE));
    return true;
  }
  return false;
}

/**
 * Multi Thread
 */
void 
ManagerImpl::stopTone() {
  if (isDriverLoaded()) {
    ost::MutexLock m(_toneMutex);
    _toneType = ZT_TONE_NULL;
    _tone->stopTone();
    _mutex.enterMutex();
    getAudioDriver()->stopStream();
    _mutex.leaveMutex();
  }
}

/**
 * Multi Thread
 */
bool
ManagerImpl::playTone()
{
  return playATone(ZT_TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::congestion () {
  playATone(ZT_TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringback () {
  playATone(ZT_TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::callBusy(short id) {
  playATone(ZT_TONE_BUSY);
  Call* call = getCall(id);
  call->setState(Call::Busy);
}

/**
 * Multi Thread
 */
void
ManagerImpl::callFailure(short id) {
  playATone(ZT_TONE_BUSY);
  getCall(id)->setState(Call::Error);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringtone() 
{ 
  if (isDriverLoaded()) {
    _toneMutex.enterMutex(); 
    _toneType = ZT_TONE_NULL;
    int play = _tone->playRingtone(getConfigString(AUDIO, RING_CHOICE).c_str());
    _toneMutex.leaveMutex();
    if (play!=1) {
      ringback();
    }
  }
}

/**
 * Use Urgent Buffer
 * By AudioRTP thread
 */
void
ManagerImpl::notificationIncomingCall (void) {
  int16* buf_ctrl_vol;
  int16* buffer = new int16[SAMPLING_RATE];
  int size = SAMPLES_SIZE(FRAME_PER_BUFFER);//SAMPLING_RATE/2;
  int k, spkrVolume;

  _tone->generateSin(440, 0, buffer);

  // Volume Control 
  buf_ctrl_vol = new int16[size*CHANNELS];
  spkrVolume = getSpkrVolume();
  for (int j = 0; j < size; j++) {
    k = j*2;
    buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = buffer[j] * spkrVolume/100;
  }
  getAudioDriver()->putUrgent(buf_ctrl_vol, SAMPLES_SIZE(FRAME_PER_BUFFER));

  delete[] buf_ctrl_vol;
  delete[] buffer;
}

/**
 * Multi Thread
 */
void
ManagerImpl::getStunInfo (StunAddress4& stunSvrAddr) {
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
        _firewallAddr = string(addr);
        _debug("address firewall = %s\n",_firewallAddr.data());
    } else {
        _debug("Opened a stun socket pair FAILED\n");
    }
}

AudioDevice
ManagerImpl::deviceList (int index)
{
  AudioDevice deviceParam;
  deviceParam.hostApiName = 
    portaudio::System::instance().deviceByIndex(index).hostApi().name();
  deviceParam.deviceName = 
    portaudio::System::instance().deviceByIndex(index).name();
  return deviceParam;
}

int
ManagerImpl::deviceCount (void)
{
	int numDevices = 0;
	
	portaudio::AutoSystem autoSys;
	portaudio::System &sys = portaudio::System::instance();
	numDevices = sys.deviceCount();
	return numDevices;	
}

bool
ManagerImpl::defaultDevice (int index) 
{
	bool defaultDisplayed = false;

	portaudio::AutoSystem autoSys;
	portaudio::System &sys = portaudio::System::instance(); 
	if (sys.deviceByIndex(index).isSystemDefaultInputDevice()) {
		defaultDisplayed = true;
	}
	return defaultDisplayed;
}

bool
ManagerImpl::useStun (void) {
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
short 
ManagerImpl::generateNewCallId (void)
{
	short random_id = rand();  
	
	// Check if already a call with this id exists 
	while (getCall(random_id) != NULL or random_id <= 0) {
		random_id = rand();
	}
	_mutex.enterMutex();
	_nCalls += 1;
	_mutex.leaveMutex();
	// If random_id is not attributed, returns it.
	return random_id;
}

unsigned int 
ManagerImpl::callVectorSize (void)
{
	return _callVector.size();
}

/**
 * Initialization: Main Thread
 */
int
ManagerImpl::createSettingsPath (void) {
	int exist = 1;
  	_path = string(HOMEDIR) + "/." + PROGNAME;
             
  	if (mkdir (_path.data(), 0755) != 0) {
		// If directory	creation failed
    	if (errno != EEXIST) {
			_debug("Cannot create directory: %s\n", strerror(errno));
			return -1;
      	} 	
  	} 

	// Load user's configuration
	_path = _path + "/" + PROGNAME + "rc";

  exist = _config.populateFromFile(_path);
	
	if (exist == 0){
		// If populateFromFile failed
		return 0;
	} else if (exist == 2) {
		// If  file doesn't exist yet
		return 2;
	}
	return exist;
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
  fill_config_int(VOIP_LINK_ID, DFT_VOIP_LINK_STR);
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
  fill_config_int(NB_CODEC, DFT_NB_CODEC_STR);
  fill_config_str(CODEC1, DFT_CODEC);
  fill_config_str(CODEC2, DFT_CODEC);
  fill_config_str(CODEC3, DFT_CODEC);
  fill_config_str(CODEC4, DFT_CODEC);
  fill_config_str(CODEC5, DFT_CODEC);
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
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioCodec (void)
{
  // TODO: need to be more dynamic...
	_nCodecs = getConfigInt(AUDIO, NB_CODEC);
	_codecDescVector.push_back(new CodecDescriptor(
				getConfigString(AUDIO, CODEC1)));

	_codecDescVector.push_back(new CodecDescriptor(
				getConfigString(AUDIO, CODEC2)));
	
	_codecDescVector.push_back(new CodecDescriptor(
				getConfigString(AUDIO, CODEC3)));
}

/**
 * Terminate: Main Thread
 */
void 
ManagerImpl::unloadAudioCodec()
{
  CodecDescriptorVector::iterator iter = _codecDescVector.begin();
  while(iter!=_codecDescVector.end()) {
    delete *iter;
    *iter = NULL;
    _codecDescVector.erase(iter);
    iter++;
  }
}


/**
 * Initialization: Main Thread
 */
void
ManagerImpl::selectAudioDriver (void)
{
	
#if defined(AUDIO_PORTAUDIO)
  try {
	_audiodriverPA = new AudioLayer();
	_audiodriverPA->openDevice(getConfigInt(AUDIO, DRIVER_NAME));
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
  _useZeroconf = getConfigInt(PREFERENCES, CONFIG_ZEROCONF);

#ifdef USE_ZEROCONF
  if (_useZeroconf) {
    _DNSService->scanServices();
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
	setSpkrVolume(getConfigInt(AUDIO, VOLUME_SPKR));
	setMicroVolume(getConfigInt(AUDIO, VOLUME_MICRO));
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
  if (_useZeroconf && _gui != NULL) {
    TokenList arg;
    TokenList argTXT;
    std::string newService = "new service";
    std::string newTXT = "new txt record";
    DNSServiceMap services = _DNSService->getServices();
    DNSServiceMap::iterator iter = services.begin();
    arg.push_back(newService);
    while(iter!=services.end()) {
      arg.push_first(iter->first);
      _gui.sendMessage("100",sequenceId,arg);
      arg.pop_first(); // remove the first, the name

      TXTRecordMap record = iter->second.getTXTRecords();
      TXTRecordMap::iterator iterTXT = record.begin();
      while(iterTXT!=record.end()) {
        argTXT.flush();
        argTXT.push_back(iter->first);
        argTXT.push_back(iterTXT->first);
        argTXT.push_back(iterTXT->second);
        argTXT.push_back(newTXT);
        _gui.sendMessage("101",sequenceId,arg);
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
ManagerImpl::attachZeroconfEvents(const std::string& sequenceId, const Pattern::Observer &observer)
{
  bool returnValue = false;
  // don't need the _gui like getZeroconf function
  // because Observer is here
#ifdef USE_ZEROCONF
  if (_useZeroconf) {
    _DNSService->attach(sequenceId,observer);
    returnValue = true;
  }
#endif
  return returnValue;
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
  TokenList tk;
  Call* call;

  if (_gui!=NULL) {
    CallVector::iterator iter = _callVector.begin();
    while(iter!=_callVector.end()){
      call = (*iter);
      std::string status = call->getStatus();
      switch( call->getState() ) {
      case Call::Busy:
        code="113";
        break;

      case Call::Answered:
        code="112";
        status = "Established";
        break;

      case Call::Ringing:
        code="111";
        break;

      case Call::Progressing:
        code="110";
        status="Trying";
        break;
      default:
        code="115";
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
ManagerImpl::setConfig(const std::string& section, const std::string& name,
int value)
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
  if (name=="codecdescriptor") {
    TokenList tk;
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
    std::string path = std::string(PROGSHAREDIR) + "/" + RINGDIR;
    try {
      ost::Dir dir(path.c_str());
      const char *cFileName = NULL;
      std::string fileName;
      std::string filePathName;
      while ( (cFileName=dir++) != NULL ) {
        fileName = cFileName;
        filePathName = path + "/" + cFileName;
        if (fileName.length() && fileName[0]!='.' && !ost::isDir(fileName.c_str())) {
          _debug("Filename: %s\n", fileName.c_str());
        }
      }
      returnValue = true;
    } catch (...) {
      // error to open file dir
    }
  } else if (name=="") {
    returnValue = true;
  } else if (name=="") {
    returnValue = true;
  } else if (name=="") {
    returnValue = true;
  }
  return returnValue;
}

/**
 * Multi Thread
 */
void 
ManagerImpl::switchCall(short id)
{
  short currentCallId = getCurrentCallId();
  if (currentCallId!=0 && id!=currentCallId) {
    onHoldCall(currentCallId);
  }
  setCurrentCallId(id);
}

// EOF
