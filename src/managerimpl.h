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

#include "audio/tonelist.h" // for Tone::TONEID declaration
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

class TelephoneTone;

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
  CALLID getCurrentCallId (void) { 
    ost::MutexLock m(_mutex); return _currentCallId; 
  }
  // Modifior of current call id 
  void setCurrentCallId (CALLID id) { 
    ost::MutexLock m(_mutex); _currentCallId = id; 
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
	CALLID generateNewCallId (void);

	/*
	 * Add a new call at the end of the CallVector with identifiant 'id'
	 */
  Call* pushBackNewCall (CALLID id, enum CallType type);
  void callSetInfo(CALLID id, const std::string& name, const std::string& number);
  bool callCanBeAnswered(CALLID id);
  bool callCanBeClosed(CALLID id);
	
	/*
	 * Functions which occur with a user's action
	 */
	int outgoingCall (const std::string& to);
	int hangupCall (CALLID id);
	int cancelCall (CALLID id);
	int answerCall (CALLID id);
	int onHoldCall (CALLID id);
	int offHoldCall (CALLID id);
	int transferCall (CALLID id, const std::string& to);
  void mute();
  void unmute();
	int refuseCall (CALLID id);

	bool saveConfig (void);
	int registerVoIPLink (void);
	int unregisterVoIPLink (void);
	
	/**
   	 * Handle choice of the DTMF-send-way
 	 *
 	 * @param   id: callid of the line.
   * @param   code: pressed key.
 	 */
	bool sendDtmf (CALLID id, char code);
	bool playDtmf (char code);
	bool playTone ();
  void stopTone();

	int incomingCall (CALLID id, const std::string& name, const std::string& number);
	void peerAnsweredCall (CALLID id);
	int peerRingingCall (CALLID id);
	int peerHungupCall (CALLID id);
	void displayTextMessage (CALLID id, const std::string& message);
	void displayErrorText (CALLID id, const std::string& message);
	void displayError (const std::string& error);
	void displayStatus (const std::string& status);
  void displayConfigError(const std::string& message);

	void startVoiceMessageNotification (const std::string& nb_msg);
	void stopVoiceMessageNotification (void);

  // configuration function requests
  bool getEvents();
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
  void callBusy(CALLID id);
  void callFailure(CALLID id);

  // return 0 if no tone (init before calling this function)
  Tone* getTelephoneTone();

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
   * Read by main thread and AudioLayer thread
   * Write by main thread only
   */
  unsigned short getSpkrVolume(void) { 
    return _spkr_volume; 
  }
  void setSpkrVolume(unsigned short spkr_vol) { 
    _spkr_volume = spkr_vol;
  }
  unsigned short getMicVolume(void) { 
    return _mic_volume;
  }
  void setMicVolume(unsigned short mic_vol) { 
    _mic_volume = mic_vol; 
  }

  bool hasLoadedSetup() { return _setupLoaded; }
	
	/*
	 * Manage information about firewall
	 */
	inline int getFirewallPort 		(void) 		{ return _firewallPort; }
	inline void setFirewallPort 	(int port) 	{ _firewallPort = port; }
	inline std::string getFirewallAddress (void) 	{ return _firewallAddr; }

	/*
	 * Init default values for the different fields
	 */
	void initConfigFile (void);

  enum REGISTRATION_STATE {
    UNREGISTERED,
    REGISTERED,
    FAILED
  };

  REGISTRATION_STATE getRegistrationState() { return _registerState; }

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
  void deleteCall	(CALLID id);
  Call* getCall (CALLID id);

  /*
   * Play one tone
   * @return false if the driver is uninitialize
   */
  bool playATone(Tone::TONEID toneId);
  //bool playATone(unsigned int tone);
  
	/////////////////////
	// Private variables
	/////////////////////
	ToneGenerator* _tone;
  TelephoneTone* _telephoneTone;
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
	CALLID _currentCallId;

	/**
   * Path of the ConfigFile 
	 */
	std::string 	_path;
	int _exist;
  int _setupLoaded;

  // To handle volume control
  short _spkr_volume;
  short _mic_volume;
  short _mic_volume_before_mute;

	// To handle firewall
	int			_firewallPort;
	std::string		_firewallAddr;

  // true if we tried to register Once
  void initRegisterVoIPLink();
  bool    _hasTriedToRegister;
  // Register state
  REGISTRATION_STATE _registerState;


  void switchCall(CALLID id);

  // tell if we have zeroconf is enabled
  int _hasZeroconf;

#ifdef USE_ZEROCONF
  // DNSService contain every zeroconf services
  //  configuration detected on the network
  DNSService *_DNSService;
#endif

};

#endif // __MANAGER_H__
