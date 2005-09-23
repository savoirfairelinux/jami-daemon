/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#include <cstdlib> 
#include <iostream>
#include <fstream> 
#include <string>
#include <vector>

#include "sipvoiplink.h"
#include "manager.h"
#include "audio/audiocodec.h"
#include "audio/audiolayer.h"
#include "audio/codecDescriptor.h"
#include "audio/ringbuffer.h"
#include "audio/tonegenerator.h"
#include "call.h"
#include "configuration.h"  
#include "configurationtree.h" 
#include "error.h"
#include "user_cfg.h"
#include "voIPLink.h" 
#include "gui/guiframework.h"

#ifdef USE_ZEROCONF
#include "zeroconf/DNSService.h"
#endif

using namespace std;
using namespace ost;
 
ManagerImpl::ManagerImpl (void)
{
  // initialize random generator  
  srand (time(NULL));
  
  // Init private variables 
  _error = new Error();
  _tone = new ToneGenerator();	
  
#ifdef USE_ZEROCONF
  _DNSService = new DNSService();
#endif

  _nCalls = 0;
  _nCodecs = 0;
  _currentCallId = 0;
  _startTime = 0;
  _endTime = 0;
  _path = ""; 
  _zonetone = false;
  _congestion = false;
  _ringtone = false;
  _ringback = false;
  _exist = 0;
  _loaded = false;
  _gui = NULL;
  _audiodriverPA = NULL;

  // Initialize after by init() -> initVolume()
  _spkr_volume = 0;
  _mic_volume  = 0;
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

  delete _error;
  delete _tone;
  delete _audiodriverPA;
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

  // Set a sip voip link by default
  _voIPLinkVector.push_back(new SipVoIPLink(DFT_VOIP_LINK));

  if (_exist == 0) {
    _debug("Cannot create config file in your home directory\n");
  } 

  initAudioCodec();

  try {
    selectAudioDriver();
    loaded(true);
  }
  catch (const portaudio::PaException &e)
    {
      displayError(e.paErrorText());
    }
  catch (const portaudio::PaCppException &e)
    {
      displayError(e.what());
    }
  catch (const exception &e)
    {
      displayError(e.what());
    }
  catch (...)
    { 
      displayError("An unknown exception occured.");
    }

  _voIPLinkVector.at(DFT_VOIP_LINK)->init();



  if (_voIPLinkVector.at(DFT_VOIP_LINK)->checkNetwork()) {
    // If network is available

    if (get_config_fields_int(SIGNALISATION, AUTO_REGISTER) == YES and 
	_exist == 1) {
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


ToneGenerator*
ManagerImpl::getTonegenerator (void) 
{
	return _tone;
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
	unsigned int i = 0;
	while (i < _callVector.size()) {
		if (_callVector.at(i)->getId() == id) {
			_callVector.erase(_callVector.begin()+i);
			return;
		} else {
			i++;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
int 
ManagerImpl::outgoingCall (const string& to)
{	
	short id;
	Call* call;
	
	id = generateNewCallId();
	call = pushBackNewCall(id, Outgoing);
	_debug("Outgoing Call with identifiant %d\n", id);
	
	//call = getCall(id);
	//if (call == NULL)
	//	return 0;
	
	call->setStatus(string(TRYING_STATUS));
	call->setState(Progressing);
	if (call->outgoingCall(to) == 0) {
		return id;
	} else {
		return 0;
	}
}

int 
ManagerImpl::hangupCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL) {
		return -1;
	}
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	if (getbRingtone()) {
		ringtone(false);
	}
	int result = call->hangup();
  congestion(false);
  return result;
}

int
ManagerImpl::cancelCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	return call->cancel();
}

int 
ManagerImpl::answerCall (short id)
{
	Call* call;

	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(Answered);
	ringtone(false);
	return call->answer();
}

int 
ManagerImpl::onHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(ONHOLD_STATUS));
	call->setState(OnHold);
	
	return call->onHold();
}

int 
ManagerImpl::offHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(OffHold);
	return call->offHold();	
}

int 
ManagerImpl::transferCall (short id, const string& to)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(TRANSFER_STATUS));
	call->setState(Transfered);
	return call->transfer(to);
}

void 
ManagerImpl::muteOn (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return;
	call->setStatus(string(MUTE_ON_STATUS));
	call->setState(MuteOn);
}

void 
ManagerImpl::muteOff (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return;
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(MuteOff);
}

int 
ManagerImpl::refuseCall (short id)
{
	Call *call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Refused);	
	ringtone(false);
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	return call->refuse();
}

int
ManagerImpl::saveConfig (void)
{
	return (Config::tree()->saveToFile(_path.data()) ? 1 : 0);
}

int 
ManagerImpl::registerVoIPLink (void)
{
	if (_voIPLinkVector.at(DFT_VOIP_LINK)->setRegister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
ManagerImpl::unregisterVoIPLink (void)
{
	if (_voIPLinkVector.at(DFT_VOIP_LINK)->setUnregister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
ManagerImpl::quitApplication (void)
{
  // Quit VoIP-link library
  terminate();
  Config::deleteTree();
  return 0;
}

int 
ManagerImpl::sendTextMessage (short , const string& )
{
	return 1;
}

int 
ManagerImpl::accessToDirectory (void)
{
	return 1;
}

int 
ManagerImpl::sendDtmf (short id, char code)
{
	int sendType = get_config_fields_int(SIGNALISATION, SEND_DTMF_AS);
                                                                                
    switch (sendType) {
        // SIP INFO
        case 0:
			_voIPLinkVector.at(DFT_VOIP_LINK)->carryingDTMFdigits(id, code);
			return 1;
            break;
                                                                                
        // Audio way
        case 1:
			return 1;
            break;
                                                                                
        // rfc 2833
        case 2:
			return 1;
            break;
                                                                                
        default:
			return -1;
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone 
///////////////////////////////////////////////////////////////////////////////

int 
ManagerImpl::incomingCall (short id)
{
	Call* call;
	call = getCall(id);
	if (call == NULL)
		return -1;
	call->setType(Incoming);
	call->setStatus(string(RINGING_STATUS));
	call->setState(Progressing);
	ringtone(true);
	displayStatus(RINGING_STATUS);
	return _gui->incomingCall(id);
}

void 
ManagerImpl::peerAnsweredCall (short id)
{
	Call* call;

	if (getbRingback()) {
		ringback(false);
	}	
	call = getCall(id);
	call->setStatus(string(CONNECTED_STATUS));

	call->setState(Answered);
	//if (isCurrentId(id)) {
		_gui->peerAnsweredCall(id);
	//}
}

int 
ManagerImpl::peerRingingCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(RINGING_STATUS));
	call->setState(Ringing);

	ringback(true);
	_gui->peerRingingCall(id);
	displayStatus(RINGING_STATUS);	
	return 1;
}

int 
ManagerImpl::peerHungupCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	_gui->peerHungupCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	if (getbRingtone()) {
		ringtone(false);
	}
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	return 1;
}

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

//int
//ManagerImpl::selectedCall (void) 
//{
//	return _gui->selectedCall();
//}
//
//bool
//ManagerImpl::isCurrentId (short id)
//{
//	return _gui->isCurrentId(id);
//}
//
void
ManagerImpl::startVoiceMessageNotification (void)
{
	_gui->startVoiceMessageNotification();
}

void
ManagerImpl::stopVoiceMessageNotification (void)
{
	_gui->stopVoiceMessageNotification();
}

void
ManagerImpl::congestion (bool var) {
	if (isDriverLoaded()) {
		if (_congestion != var) {
			_congestion = var;
		}
		_zonetone = var;
		_debug("ManagerImpl::congestion : Tone Handle Congestion\n");
		_tone->toneHandle(ZT_TONE_CONGESTION);
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

void
ManagerImpl::ringback (bool var) {
	if (isDriverLoaded()) {
		if (_ringback != var) {
			_ringback = var;
		}
		_zonetone = var;
		_tone->toneHandle(ZT_TONE_RINGTONE);
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

void
ManagerImpl::ringtone (bool var) 
{ 
	if (isDriverLoaded()) {
		if (getNumberOfCalls() > 1 and _zonetone and var == false) {
			// If more than one line is ringing
			_zonetone = false;
			_tone->playRingtone((_gui->getRingtoneFile()).data());
		}
		_ringtone = var;
		_zonetone = var;
		if (getNumberOfCalls() == 1) {
			// If just one line is ringing
			_tone->playRingtone((_gui->getRingtoneFile()).data());
		} 
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

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
	
	getAudioDriver()->urgentRingBuffer().Put(buf_ctrl_vol, 
			SAMPLES_SIZE(FRAME_PER_BUFFER));

    delete[] buf_ctrl_vol;
    delete[] buffer;
}

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
    if (get_config_fields_int(SIGNALISATION, USE_STUN) == YES) {
        return true;
    } else {
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

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

	exist = Config::tree()->populateFromFile(_path);
	
	if (exist == 0){
		// If populateFromFile failed
		return 0;
	} else if (exist == 2) {
		// If  file doesn't exist yet
		return 2;
	}
	return exist;
}

void
ManagerImpl::initConfigFile (void) 
{
	_exist = createSettingsPath();

	fill_config_fields_int(SIGNALISATION, VOIP_LINK_ID, DFT_VOIP_LINK); 	
	fill_config_fields_str(SIGNALISATION, FULL_NAME, EMPTY_FIELD);
	fill_config_fields_str(SIGNALISATION, USER_PART, EMPTY_FIELD); 
	fill_config_fields_str(SIGNALISATION, AUTH_USER_NAME, EMPTY_FIELD); 
	fill_config_fields_str(SIGNALISATION, PASSWORD, EMPTY_FIELD); 
	fill_config_fields_str(SIGNALISATION, HOST_PART, EMPTY_FIELD); 
	fill_config_fields_str(SIGNALISATION, PROXY, EMPTY_FIELD); 
	fill_config_fields_int(SIGNALISATION, AUTO_REGISTER, YES);
	fill_config_fields_int(SIGNALISATION, PLAY_TONES, YES); 
	fill_config_fields_int(SIGNALISATION, PULSE_LENGTH, DFT_PULSE_LENGTH); 
	fill_config_fields_int(SIGNALISATION, SEND_DTMF_AS, SIP_INFO); 
	fill_config_fields_str(SIGNALISATION, STUN_SERVER, DFT_STUN_SERVER); 
	fill_config_fields_int(SIGNALISATION, USE_STUN, NO); 

	fill_config_fields_int(AUDIO, DRIVER_NAME, DFT_DRIVER); 
	fill_config_fields_int(AUDIO, NB_CODEC, DFT_NB_CODEC); 
	fill_config_fields_str(AUDIO, CODEC1, DFT_CODEC); 
	fill_config_fields_str(AUDIO, CODEC2, DFT_CODEC); 
	fill_config_fields_str(AUDIO, CODEC3, DFT_CODEC); 
	fill_config_fields_str(AUDIO, CODEC4, DFT_CODEC); 
	fill_config_fields_str(AUDIO, CODEC5, DFT_CODEC); 
	fill_config_fields_str(AUDIO, RING_CHOICE, DFT_RINGTONE); 
	fill_config_fields_int(AUDIO, VOLUME_SPKR, DFT_VOL_SPKR); 
	fill_config_fields_int(AUDIO, VOLUME_MICRO, DFT_VOL_MICRO); 

	fill_config_fields_int(AUDIO, VOLUME_SPKR_X, DFT_VOL_SPKR_X); 
	fill_config_fields_int(AUDIO, VOLUME_SPKR_Y, DFT_VOL_SPKR_Y); 
	fill_config_fields_int(AUDIO, VOLUME_MICRO_X, DFT_VOL_MICRO_X); 
	fill_config_fields_int(AUDIO, VOLUME_MICRO_Y, DFT_VOL_MICRO_Y); 

	fill_config_fields_str(PREFERENCES, SKIN_CHOICE, DFT_SKIN); 
	fill_config_fields_int(PREFERENCES, CONFIRM_QUIT, YES); 
	fill_config_fields_str(PREFERENCES, ZONE_TONE, DFT_ZONE); 
	fill_config_fields_int(PREFERENCES, CHECKED_TRAY, NO); 
	fill_config_fields_str(PREFERENCES, VOICEMAIL_NUM, DFT_VOICEMAIL); 
  
	fill_config_fields_int(PREFERENCES, CONFIG_ZEROCONF, CONFIG_ZEROCONF_DEFAULT); 
}

void
ManagerImpl::initAudioCodec (void)
{
	_nCodecs = get_config_fields_int(AUDIO, NB_CODEC);
	_codecDescVector.push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC1)));

	_codecDescVector.push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC2)));
	
	_codecDescVector.push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC3)));
}

void
ManagerImpl::selectAudioDriver (void)
{
	
#if defined(AUDIO_PORTAUDIO)
	_audiodriverPA = new AudioLayer();
	_audiodriverPA->openDevice(get_config_fields_int(AUDIO, DRIVER_NAME));
#else
# error You must define one AUDIO driver to use.
#endif
}

/**
 * Initialize the Zeroconf scanning services loop
 * Informations will be store inside a map DNSService->_services
 */
void 
ManagerImpl::initZeroconf(void) 
{
  _useZeroconf = get_config_fields_int(PREFERENCES, CONFIG_ZEROCONF);

#ifdef USE_ZEROCONF
  if (_useZeroconf) {
    _DNSService->scanServices();
  }
#endif
}

/*
 * Init the volume for speakers/micro from 0 to 100 value
 */
void
ManagerImpl::initVolume()
{
	setSpkrVolume(get_config_fields_int(AUDIO, VOLUME_SPKR));
	setMicroVolume(get_config_fields_int(AUDIO, VOLUME_MICRO));
}

// EOF
