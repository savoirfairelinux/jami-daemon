/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <cc++/thread.h>
#include <string>
#include <vector>

#include "../stund/stun.h"
#include "call.h"
#include "audio/audiodevice.h"
#include "observer.h"
#include "config/config.h"
#include "audio/dtmf.h"
#include "audio/codecDescriptor.h"

class AudioLayer;
class CodecDescriptor;
class Error;
class GuiFramework;
class ToneGenerator;
class VoIPLink;
#ifdef USE_ZEROCONF
class DNSService;
#endif

#define	NOTIFICATION_LEN	250
// Status
#define CONNECTED_STATUS	"Connected"
#define LOGGED_IN_STATUS	"Logged in"
#define RINGING_STATUS		"Ringing"
#define TRYING_STATUS		"Trying ..."
#define HANGUP_STATUS       "Hang up"
#define ONHOLD_STATUS       "On hold ..."
#define TRANSFER_STATUS     "Transfer to:"
#define MUTE_ON_STATUS		"Mute on"
#define ENTER_NUMBER_STATUS "Enter Phone Number:"

/*
 * Define a type for a list of call
 */
typedef std::vector< Call* > CallVector;

/*
 * Define a type for a list of VoIPLink
 */
typedef std::vector< VoIPLink* > VoIPLinkVector;

/*
 * Define a type for a list of CodecDescriptor
 */
typedef std::vector< CodecDescriptor* > CodecDescriptorVector;

/*
 * Structure for audio device
 */
//struct device_t{
//	const char* hostApiName;
//	const char* deviceName;
//};

/**
 * To send multiple string
 */
typedef std::list<std::string> TokenList;

class ManagerImpl {
public:
	ManagerImpl (void);
	~ManagerImpl (void);

	// Init a new VoIPLink, audio codec and audio driver
	void init (void);
	void terminate (void);

	// Set the graphic user interface
	void setGui (GuiFramework* gui);
	
	// Accessor to error
	Error* error(void) const { return _error; }

	// Accessor to audiodriver
  // it multi-thread and use mutex internally
	AudioLayer* getAudioDriver(void) const { return _audiodriverPA ;}

  // Accessor to current call id 
  short getCurrentCallId (void) { 
    ost::MutexLock m(_mutex); return _currentCallId; 
  }
  // Modifior of current call id 
  void setCurrentCallId (short currentCallId) { 
    ost::MutexLock m(_mutex);_currentCallId = currentCallId; 
  }

  // Accessor to VoIPLinkVector
  VoIPLinkVector* getVoIPLinkVector (void) {return &_voIPLinkVector;}
  // Codec Descriptor
  CodecDescriptorVector* getCodecDescVector(void) {return &_codecDescVector;}

  /* 
	 * Attribute a new random id for a new call 
	 * and check if it's already attributed to existing calls. 
	 * If not exists, returns 'id' otherwise return 0 
	 */   
	short generateNewCallId (void);

	/*
	 * Add a new call at the end of the CallVector with identifiant 'id'
	 */
	Call* pushBackNewCall (short id, enum CallType type);
	
  bool callCanHangup(short id);
  bool callCanAnswer(short id);
  bool callCanClose(short id);
	
	/*
	 * Functions which occur with a user's action
	 */
	int outgoingCall (const std::string& to);
	int hangupCall (short id);
	int cancelCall (short id);
	int answerCall (short id);
	int onHoldCall (short id);
	int offHoldCall (short id);
	int transferCall (short id, const std::string& to);
  void mute();
  void unmute();
	int refuseCall (short id);

	bool saveConfig (void);
	int registerVoIPLink (void);
	int unregisterVoIPLink (void);
	
	/**
   	 * Handle choice of the DTMF-send-way
 	 *
 	 * @param   id: callid of the line.
   * @param   code: pressed key.
 	 */
	bool sendDtmf (short id, char code);
	bool playDtmf (char code);
	bool playTone ();
  void stopTone();

	int incomingCall (short id);
	void peerAnsweredCall (short id);
	int peerRingingCall (short id);
	int peerHungupCall (short id);
	void displayTextMessage (short id, const std::string& message);
	void displayErrorText (short id, const std::string& message);
	void displayError (const std::string& error);
	void displayStatus (const std::string& status);
  void displayConfigError(const std::string& message);

//	int selectedCall (void);
//	bool isCurrentId (short id);
	void startVoiceMessageNotification (const std::string& nb_msg);
	void stopVoiceMessageNotification (void);

  // configuration function requests
  bool getZeroconf(const std::string& sequenceId);
  bool attachZeroconfEvents(const std::string& sequenceId, Pattern::Observer& observer);
  bool detachZeroconfEvents(Pattern::Observer& observer);
  bool getCallStatus(const std::string& sequenceId);
  bool getConfigAll(const std::string& sequenceId);
  bool getConfig(const std::string& section, const std::string& name, TokenList& arg);
  bool setConfig(const std::string& section, const std::string& name, const std::string& value);
  bool setConfig(const std::string& section, const std::string& name,
int value);
  bool getConfigList(const std::string& sequenceId, const std::string& name);

  // configuration function for extern
  // throw an Conf::ConfigTreeItemException if not found
  int getConfigInt(const std::string& section, const std::string& name);
  std::string getConfigString(const std::string& section, const std::string&
name);

	/*
	 * Handle audio sounds heard by a caller while they wait for their 
	 * connection to a called party to be completed.
	 */
	void ringback ();

	/*
	 * Handle played music when an incoming call occurs
	 */
	void ringtone ();
	void congestion ();
  void callBusy(short callId);
  void callFailure(short callId);

  /**
   * @return true is there is one or many incoming call waiting
   * new call, not anwsered or refused
   */
  bool incomingCallWaiting(void);
	/*
	 * Notification of incoming call when you are already busy
	 */
	void notificationIncomingCall (void);

	/*
	 * Get information about firewall 
	 * @param	stunSvrAddr: stun server
	 */
	void getStunInfo (StunAddress4& stunSvrAddr);
	bool useStun (void);
	
	/*
	 * Inline functions to manage volume control
	 */
	inline int getSpkrVolume 	(void) 			{ ost::MutexLock m(_mutex); return _spkr_volume; }
	inline void setSpkrVolume 	(int spkr_vol) 	{ ost::MutexLock m(_mutex); _spkr_volume = spkr_vol; }
	inline int getMicVolume 	(void) 			{ ost::MutexLock m(_mutex); return _mic_volume; }
	inline void setMicVolume 	(int mic_vol) 	{ ost::MutexLock m(_mutex); _mic_volume = _mic_volume_before_mute = mic_vol; }

  bool hasLoadedSetup() { return _setupLoaded; }
	
	/*
	 * Manage information about firewall
	 */
	inline int getFirewallPort 		(void) 		{ return _firewallPort; }
	inline void setFirewallPort 	(int port) 	{ _firewallPort = port; }
	inline std::string getFirewallAddress (void) 	{ return _firewallAddr; }

	/*
	 * Manage information about audio driver
	 */
	inline bool isDriverLoaded (void) const { return _loaded; }
	inline void loaded (bool l) { _loaded = l; }

	/* 
	 * Functions about audio device
	 */
/*
	AudioDevice deviceList (int);
	int deviceCount (void);
	bool defaultDevice (int);
*/	
	/*
	 * Init default values for the different fields
	 */
	void initConfigFile (void);

private:

  
	/**
 	 * Create .PROGNAME directory in home user and create 
	 * configuration tree from the settings file if this file exists.
 	 *
 	 * @return	0 if creating file failed
	 *			1 if config-file exists
	 *			2 if file doesn't exist yet.
 	 */
	int createSettingsPath (void);

	/*
	 * Initialize audiocodec
	 */
	void initAudioCodec(void);
  void unloadAudioCodec(void);
	
	/*
	 * Initialize audiodriver
	 */
	void selectAudioDriver (void);

  /*
   * Initialize zeroconf module and scanning
   */
  void initZeroconf(void);
  
  /*
   * Init the Gui interface (after setting it) inside setGui
   */
  void initGui();

  /*
   * Init the volume for speakers/micro from 0 to 100 value
   */ 
  void initVolume();

  /**
   * Configuration
   */
  bool getDirListing(const std::string& sequenceId, const std::string& path, int *nbFile);
  bool getAudioDeviceList(const std::string& sequenceId);
  Conf::ConfigTree _config;
  bool getCountryTones(const std::string& sequenceId);
  void sendCountryTone(const std::string& sequenceId, int index, const std::string& name);

  /*
   * Erase the Call(id) from the CallVector
   */
  void deleteCall	(short id);
  Call* getCall (short id);

  /*
   * Play one tone
   * @return false if the driver is uninitialize
   */
  bool playATone(unsigned int tone);
  
	/////////////////////
	// Private variables
	/////////////////////
	ToneGenerator* _tone;
  ost::Mutex _toneMutex;
  int _toneType;

	Error* _error;
	GuiFramework* _gui;
	AudioLayer* _audiodriverPA;
  DTMF _key;

	/*
	 * Vector of VoIPLink
	 */
	VoIPLinkVector _voIPLinkVector;
	
	/*
	 * Vector of calls
	 */
	CallVector _callVector;

	/*
	 * Vector of CodecDescriptor
	 */
	CodecDescriptorVector _codecDescVector;

  // map of codec (for configlist request)
  CodecMap _codecMap;

	/*
	 * Mutex to protect access to code section
	 */
	ost::Mutex _mutex;

  /**
   * Incomings Call:
   */
  ost::Mutex _incomingCallMutex;
  unsigned int _nbIncomingWaitingCall;
  void incWaitingCall(void);
  void decWaitingCall(void);
	
  // Current callid 
	short _currentCallId;

	/*
	 * For the call timer
	 */
	unsigned int _startTime;
	unsigned int _endTime;

	/* Path of the ConfigFile 
	 */
	std::string 	_path;
	int _exist;
  int _setupLoaded;

	unsigned int _nCodecs;

	// To handle volume control
	int 		_spkr_volume;
	int 		_mic_volume;
  int 		_mic_volume_before_mute;

	// To handle firewall
	int			_firewallPort;
	std::string		_firewallAddr;

	// Variables used in exception
	bool 		_loaded;

  // tell if we have zeroconf is enabled
  int _hasZeroconf;

  void switchCall(short id);

#ifdef USE_ZEROCONF
  // DNSService contain every zeroconf services
  //  configuration detected on the network
  DNSService *_DNSService;
#endif

};

#endif // __MANAGER_H__
