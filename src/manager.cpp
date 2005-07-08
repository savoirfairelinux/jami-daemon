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

#include <sys/socket.h> // inet_ntoa()
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cc++/thread.h>
#include <cstdlib> 
#include <iostream>
#include <fstream> 
#include <string>
#include <vector>

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
#include "sipvoiplink.h"
#include "skin.h" 
#include "user_cfg.h"
#include "voIPLink.h" 
#include "gui/guiframework.h"

using namespace std;
using namespace ost;
 
device_t Manager::deviceParam;

Manager::Manager (void)
{
	// initialize random generator  
  	srand (time(NULL));

	// Init private variables 
	_callVector = new CallVector();
	_voIPLinkVector = new VoIPLinkVector();	
	_error = new Error(this);
	_tone = new ToneGenerator(this);	

	// Set a sip voip link by default
	_voIPLinkVector->push_back(new SipVoIPLink(DFT_VOIP_LINK, this));

	_nCalls = 0;
	_nCodecs = 0;
	_currentCallId = 0;
	_startTime = 0;
	_endTime = 0;
	_path = ""; 
	_tonezone = false;
	_congestion = false;
	_ringtone = false;
	_ringback = false;
	_exist = 0;
	_loaded = false;
	
	initConfigFile();
	_exist = createSettingsPath();
	if (_exist == 0) {
		_debug("Cannot create config file in your home directory\n");
	} 
}

Manager::~Manager (void) 
{
	delete _callVector;
	delete _voIPLinkVector;			
	delete _error;
	delete _tone;
	delete _codecDescVector; 
	delete _audiodriverPA;
} 

void 
Manager::init (void) 
{
	if (_exist == 2) {
		// If config-file doesn't exist, launch configuration setup
		_gui->setup();
	}
	initAudioCodec();

	try {
		selectAudioDriver();
		loaded(true);
	}
	catch (const portaudio::PaException &e)
	{
		displayErrorText(e.paErrorText());
	}
	catch (const portaudio::PaCppException &e)
	{
		displayErrorText(e.what());
	}
	catch (const exception &e)
	{
		displayErrorText(e.what());
	}
	catch (...)
	{ 
		displayErrorText("An unknown exception occured.");
	}
	
	_voIPLinkVector->at(DFT_VOIP_LINK)->init();
	if (get_config_fields_int(SIGNALISATION, AUTO_REGISTER) == YES and 
			_exist == 1) {
		if (registerVoIPLink() != 1) {
			_debug("Registration failed\n");
            displayErrorText("Check your configuration fields");
		}
	} 
}

void
Manager::setGui (GuiFramework* gui)
{
	_gui = gui;
}

ToneGenerator*
Manager::getTonegenerator (void) 
{
	return _tone;
}

Error*
Manager::error (void) 
{
	return _error;
}

AudioLayer*
Manager::getAudioDriver(void) 
{
	return _audiodriverPA;
}

unsigned int 
Manager::getNumberOfCalls (void)
{
	return _nCalls;
}

void 
Manager::setNumberOfCalls (unsigned int nCalls)
{
	_nCalls = nCalls;
}

short
Manager::getCurrentCallId (void)
{
	return _currentCallId;
}

void 
Manager::setCurrentCallId (short currentCallId)
{
	_currentCallId = currentCallId;
}

CallVector* 
Manager::getCallVector (void)
{
	return _callVector;
}

Call*
Manager::getCall (short id)
{
	if (id > 0 and _callVector->size() > 0) {
		for (unsigned int i = 0; i < _nCalls; i++) {
			if (_callVector->at(i)->getId() == id) {
				return _callVector->at(i);
			}
		}
		return NULL;
	} else {
		return NULL;
	}
}


unsigned int
Manager::getNumberOfCodecs (void)
{
	return _nCodecs;
}

void
Manager::setNumberOfCodecs (unsigned int nb_codec)
{
	_nCodecs = nb_codec;
}

VoIPLinkVector* 
Manager::getVoIPLinkVector (void)
{
	return _voIPLinkVector;
}

CodecDescriptorVector*
Manager::getCodecDescVector (void)
{
	return _codecDescVector;
}

void
Manager::pushBackNewCall (short id, enum CallType type)
{
	Call* call = new Call(this, id, type, _voIPLinkVector->at(DFT_VOIP_LINK));
	// Set the wanted voip-link (first of the list)
	_callVector->push_back(call);
}

void
Manager::deleteCall (short id)
{
	unsigned int i = 0;
	while (i < _callVector->size()) {
		if (_callVector->at(i)->getId() == id) {
			_callVector->erase(_callVector->begin()+i);
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
Manager::outgoingCall (const string& to)
{	
	short id;
	Call* call;
	
	id = generateNewCallId();
	pushBackNewCall(id, Outgoing);
	
	_debug("Outgoing Call with identifiant %d\n", id);
	call = getCall(id);
	
	call->setStatus(string(TRYING_STATUS));
	call->setState(Progressing);
	if (call->outgoingCall(id, to) == 0) {
		return id;
	} else {
		return 0;
	}
}

int 
Manager::hangupCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	call->hangup();
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	return 1;
}

int
Manager::cancelCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	call->cancel();
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	return 1;
}

int 
Manager::answerCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(Answered);
	call->answer();
	ringtone(false);
	return 1;
}

int 
Manager::onHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	call->setStatus(string(ONHOLD_STATUS));
	call->setState(OnHold);
	call->onHold();
	return 1;
}

int 
Manager::offHoldCall (short id)
{
	Call* call;
	call = getCall(id);
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(OffHold);
	call->offHold();	
	return 1;
}

int 
Manager::transferCall (short id, const string& to)
{
	Call* call;
	call = getCall(id);
	call->setStatus(string(TRANSFER_STATUS));
	call->setState(Transfered);
	call->transfer(to);
	return 1;
}

int 
Manager::muteOn (short id)
{
	Call* call;
	call = getCall(id);
	call->setStatus(string(MUTE_ON_STATUS));
	call->setState(MuteOn);
	call->muteOn();
	return 1;
}

int 
Manager::muteOff (short id)
{
	Call* call;
	call = getCall(id);
	call->setStatus(string(CONNECTED_STATUS));
	call->setState(MuteOff);
	call->muteOff();
	return 1;
}

int 
Manager::refuseCall (short id)
{
	Call *call;
	call = getCall(id);
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Refused);	
	call->refuse();
	ringtone(false);
	delete call;
	return 1;
}

int
Manager::saveConfig (void)
{
	return (Config::tree()->saveToFile(_path.data()) ? 1 : 0);
}

int 
Manager::registerVoIPLink (void)
{
	if (_voIPLinkVector->at(DFT_VOIP_LINK)->setRegister() == 0) {
		return 1;
	} else {
		return 0;
	}
}

int 
Manager::quitApplication (void)
{
	// Quit VoIP-link library
	_voIPLinkVector->at(DFT_VOIP_LINK)->quit();
	if (saveConfig()) {
		Config::deleteTree();
		return 1;
	} else {
		return 0;
	}
}

int 
Manager::sendTextMessage (short , const string& )
{
	return 1;
}

int 
Manager::accessToDirectory (void)
{
	return 1;
}

int 
Manager::sendDtmf (short id, char code)
{
	int sendType = get_config_fields_int(SIGNALISATION, SEND_DTMF_AS);
                                                                                
    switch (sendType) {
        // SIP INFO
        case 0:
			_voIPLinkVector->at(DFT_VOIP_LINK)->carryingDTMFdigits(id, code);
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
Manager::incomingCall (short id)
{
	Call* call;
	call = getCall(id);
	call->setType(Incoming);
	call->setStatus(string(RINGING_STATUS));
	call->setState(Progressing);
	ringtone(true);
	_gui->incomingCall(id);
	displayStatus(RINGING_STATUS);
	return 1;
}

// L'autre personne a repondu
int 
Manager::peerAnsweredCall (short id)
{
	Call* call;

	if (getbRingback()) {
		ringback(false);
	}	
	call = getCall(id);
	call->setStatus(string(CONNECTED_STATUS));

	call->setState(Answered);
	if (isCurrentId(id)) {
		_gui->peerAnsweredCall(id);
	}
	return 1;
}

int 
Manager::peerRingingCall (short id)
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
Manager::peerHungupCall (short id)
{
	Call* call;

	call = getCall(id);
	call->setStatus(string(HUNGUP_STATUS));
	call->setState(Hungup);
	_gui->peerHungupCall(id);
	if (getbRingback()) {
		ringback(false);
	}
	_mutex.enterMutex();
	_nCalls -= 1;
	_mutex.leaveMutex();
	deleteCall(id);
	return 1;
}

void 
Manager::displayTextMessage (short id, const string& message)
{
	_gui->displayTextMessage(id, message);
}

void 
Manager::displayErrorText (const string& message)
{
	_gui->displayErrorText(message);
}

void 
Manager::displayError (const string& error)
{
	_gui->displayStatus(error);
}

void 
Manager::displayStatus (const string& status)
{
	_gui->displayStatus(status);
}

int
Manager::selectedCall (void) 
{
	return _gui->selectedCall();
}

bool
Manager::isCurrentId (short id)
{
	return _gui->isCurrentId(id);
}

void
Manager::congestion (bool var) {
	if (isDriverLoaded()) {
		if (_congestion != var) {
			_congestion = var;
		}
		_tonezone = var;
		_tone->toneHandle(ZT_TONE_CONGESTION);
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

void
Manager::ringback (bool var) {
	if (isDriverLoaded()) {
		if (_ringback != var) {
			_ringback = var;
		}
		_tonezone = var;
		_tone->toneHandle(ZT_TONE_RINGTONE);
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

void
Manager::ringtone (bool var) 
{ 
	if (isDriverLoaded()) {
		if (getNumberOfCalls() > 1 and _tonezone and var == false) {
			// If more than one line is ringing
			_tonezone = false;
			_tone->playRingtone((_gui->getRingtoneFile()).data());
		}
		
		if (_ringtone != var) {
			_ringtone = var;
		}
																					
		_tonezone = var;
		if (getNumberOfCalls() == 1) {
			// If just one line is ringing
			_tone->playRingtone((_gui->getRingtoneFile()).data());
		} 
	} else {
        _error->errorName(OPEN_FAILED_DEVICE);
    }
}

void
Manager::notificationIncomingCall (void) {
    int16* buf_ctrl_vol;
    int16* buffer = new int16[SAMPLING_RATE];
	int size = SAMPLING_RATE/2;
	int k, spkrVolume;
                                                                                
    _tone->generateSin(440, 0, buffer);
           
	// Control volume
	buf_ctrl_vol = new int16[size];
	spkrVolume = getSpkrVolume();
	for (int j = 0; j < size; j++) {
		k = j*2;
		buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = buffer[j] * spkrVolume/100;
	}
	
	getAudioDriver()->urgentRingBuffer()->Put(buf_ctrl_vol, 
			size * CHANNELS);

	getAudioDriver()->startStream();
	getAudioDriver()->sleep(NOTIFICATION_LEN);
	getAudioDriver()->stopStream();
	
    delete[] buffer;
    delete[] buf_ctrl_vol;
}

void
Manager::getStunInfo (StunAddress4& stunSvrAddr) {
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

device_t
Manager::deviceList (int index)
{
	portaudio::AutoSystem autoSys;
	portaudio::System &sys = portaudio::System::instance(); 
	deviceParam.hostApiName = sys.deviceByIndex(index).hostApi().name();
	deviceParam.deviceName = sys.deviceByIndex(index).name();
	return deviceParam;
}

int
Manager::deviceCount (void)
{
	int numDevices = 0;
	
	portaudio::AutoSystem autoSys;
	portaudio::System &sys = portaudio::System::instance();
	numDevices = sys.deviceCount();
	return numDevices;	
}

bool
Manager::defaultDevice (int index) 
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
Manager::useStun (void) {
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
Manager::generateNewCallId (void)
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
Manager::callVectorSize (void)
{
	return _callVector->size();
}

int
Manager::createSettingsPath (void) {
	int exist = 1;
  	_path = string(HOMEDIR) + "/." + PROGNAME;
             
  	if (mkdir (_path.data(), 0755) != 0) {
		// If directory	creation failed
    	if (errno != EEXIST) {
			_debug("Cannot create directory: %d\n", strerror(errno));
			return -1;
      	} 	
  	} 

	// Load user's config
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
Manager::initConfigFile (void) 
{
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
	fill_config_fields_int(AUDIO, VOLUME_SPKR_X, DFT_VOL_SPKR_X); 
	fill_config_fields_int(AUDIO, VOLUME_SPKR_Y, DFT_VOL_SPKR_Y); 
	fill_config_fields_int(AUDIO, VOLUME_MICRO_X, DFT_VOL_MICRO_X); 
	fill_config_fields_int(AUDIO, VOLUME_MICRO_Y, DFT_VOL_MICRO_Y); 

	fill_config_fields_str(PREFERENCES, SKIN_CHOICE, DFT_SKIN); 
	fill_config_fields_int(PREFERENCES, CONFIRM_QUIT, YES); 
	fill_config_fields_str(PREFERENCES, ZONE_TONE, DFT_ZONE); 
	fill_config_fields_int(PREFERENCES, CHECKED_TRAY, NO); 
	fill_config_fields_str(PREFERENCES, VOICEMAIL_NUM, DFT_VOICEMAIL); 
}

void
Manager::initAudioCodec (void)
{
	_nCodecs = get_config_fields_int(AUDIO, NB_CODEC);
	_codecDescVector = new CodecDescriptorVector();
	
	_codecDescVector->push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC1)));

	_codecDescVector->push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC2)));
	
	_codecDescVector->push_back(new CodecDescriptor(
				get_config_fields_str(AUDIO, CODEC3)));
}

void
Manager::selectAudioDriver (void)
{
	
#if defined(AUDIO_PORTAUDIO)
	_audiodriverPA = new AudioLayer(this);
	_audiodriverPA->openDevice(get_config_fields_int(AUDIO, DRIVER_NAME));
#else
# error You must define one AUDIO driver to use.
#endif
}

// EOF
