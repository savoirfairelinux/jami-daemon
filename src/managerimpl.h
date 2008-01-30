/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#ifndef __MANAGER_H__
#define __MANAGER_H__

//#define TEST

#include <string>
#include <vector>
#include <set>
#include <map>
#include <cc++/thread.h>
#include "dbus/dbusmanager.h"

#include "stund/stun.h"
#include "observer.h"
#include "config/config.h"

#include "account.h"
#include "call.h"

#include "audio/tonelist.h" // for Tone::TONEID declaration
#include "audio/audiofile.h"
#include "audio/dtmf.h"
#include "audio/codecDescriptor.h"


class AudioLayer;
class CodecDescriptor;
class GuiFramework;
class TelephoneTone;
class VoIPLink;

#ifdef USE_ZEROCONF
class DNSService;
#endif

/**
 * Define a type for a AccountMap container
 */
typedef std::map<AccountID, Account*> AccountMap;
 
/**
 * Define a type for a CallID to AccountID Map inside ManagerImpl
 */
typedef std::map<CallID, AccountID> CallAccountMap;

/**
 * Define a type for CallID vector (waiting list, incoming not answered)
 */
typedef std::set<CallID> CallIDSet;

/**
 * To send multiple string
 */
typedef std::list<std::string> TokenList;

/**
 * Manager (controller) of sflphone daemon
 */
class ManagerImpl {
public:
  ManagerImpl (void);
  ~ManagerImpl (void);

  /**
   * Initialisation of thread (sound) and map.
   *
   * Init a new VoIPLink, audio codec and audio driver
   */
  void init (void);

  /**
   * Terminate all thread (sound, link) and unload AccountMap
   */
  void terminate (void);

  /**
   * Set user interface manaager.
   * @param man The DBUS interface implementation
   */
  void setDBusManager (DBusManagerImpl* man) { _dbus = man; }

  /**
   * Accessor to audiodriver.
   *
   * it's multi-thread and use mutex internally
   */
  AudioLayer* getAudioDriver(void) const { return _audiodriver; }

  /**
   * Get a descriptor map of codec available
   */
  CodecDescriptor& getCodecDescriptorMap(void) {return _codecDescriptorMap;}

  /**
   * Functions which occur with a user's action
   */
  bool outgoingCall(const AccountID& accountId, const CallID& id, const std::string& to);
  bool answerCall(const CallID& id);
  bool hangupCall(const CallID& id);
  bool cancelCall(const CallID& id);
  bool onHoldCall(const CallID& id);
  bool offHoldCall(const CallID& id);
  bool transferCall(const CallID& id, const std::string& to);
  void mute();
  void unmute();
  bool refuseCall(const CallID& id);

  /** Save config to file */
  bool saveConfig (void);

  /**
   * Send registration information (shake hands) for a specific AccountID
   *
   * @param accountId Account to register
   * @return true if sendRegister was called without failure, else return false
   */
  bool registerAccount(const AccountID& accountId);

  /**
   * Send unregistration for a specific account. If the protocol
   * doesn't need to send anything, then the state of the account
   * will be set to 'Unregistered', and related objects destroyed.
   *
   * @param accountId Account to unregister
   * @return true if the unregister method is send correctly
   */
  bool unregisterAccount(const AccountID& accountId);

  /**
   * Send registration to all enabled accounts
   * 
   * @return false if exosip or the network checking fails
   */
  bool initRegisterAccounts();

  /**
   * True if we tried to register Once
   */
  bool _hasTriedToRegister;


  /**
   * Undocumented
   */
  bool sendTextMessage(const AccountID& accountId, const std::string& to, const std::string& message);
	
  /*
   * Handle choice of the DTMF-send-way
   *
   * @param   id: callid of the line.
   * @param   code: pressed key.
   */
  bool sendDtmf(const CallID& id, char code);
  bool playDtmf(char code);
  bool playTone ();
  void stopTone(bool stopAudio/*=true*/);

  // From links
  /**
   * When receiving a new incoming call, add it to the callaccount map
   * and notify user
   * @param call A call pointer
   * @param accountid an account id
   * @return true if the call was added correctly
   */
  bool incomingCall(Call* call, const AccountID& accountId);
  void peerAnsweredCall(const CallID& id);
  void peerRingingCall(const CallID& id);
  void peerHungupCall(const CallID& id);
  void incomingMessage(const AccountID& accountId, const std::string& message);

  void displayTextMessage (const CallID& id, const std::string& message);
  void displayErrorText (const CallID& id, const std::string& message);
  void displayError(const std::string& error);
  void displayStatus(const std::string& status);
  void displayConfigError(const std::string& message);

  void startVoiceMessageNotification(const AccountID& accountId, const std::string& nb_msg);
  void stopVoiceMessageNotification(const AccountID& accountId);

  /** Notify the user that registration succeeded  */
  void registrationSucceed(const AccountID& accountId);
  /** Notify the user that registration succeeded  */
  void registrationFailed(const AccountID& accountId);

  // configuration function requests

  /**
   * Start events thread. This function actually only calls the private
   * initRegisterVoIPLink().
   *
   * This function should definitively be renamed!
   *
   * @todo Receive account name (???)
   *
   * DEPRECATED
   */
  //bool getEvents();

  //
  bool getZeroconf(const std::string& sequenceId);
  bool attachZeroconfEvents(const std::string& sequenceId, Pattern::Observer& observer);
  bool detachZeroconfEvents(Pattern::Observer& observer);
  bool getCallStatus(const std::string& sequenceId);

  /** 
   * Get account list 
   * @return A list of accoundIDs
   */
  std::vector< std::string >  getAccountList();

  /**
   * Retrieve details about a given account
   */
  std::map< std::string, std::string > getAccountDetails(const AccountID& accountID);

  /**
   * Save the details of an existing account, given the account ID
   *
   * This will load the configuration map with the given data.
   * It will also register/unregister links where the 'Enabled' switched.
   */
  void setAccountDetails( const ::DBus::String& accountID, 
                   const std::map< ::DBus::String, ::DBus::String >& details );

  /**
   * Add a new account, and give it a new account ID automatically
   */
  void addAccount(const std::map< ::DBus::String, ::DBus::String >& details);

  /**
   * Delete an existing account, unregister VoIPLink associated, and
   * purge from configuration.
   */
  void removeAccount(const AccountID& accountID);
  
  /*
   * Get the default account
   * @return The default account
   */
  std::string getDefaultAccount();

  /**
   * Set the prefered order for codecs.
   * Called by D-Bus command: "setCodecPreferedOrder"
   *
   * @param codec_name The name of the prefered codec.
   */
  void setPreferedCodec(const ::DBus::String& codec_name);
  
/**
 * Get the prefered codec
 * @return The description of the prefered codec
 */
  std::string getPreferedCodec(  );

  /**
   * Get the list of codecs we supports, ordered by the user
   * @return The list of the codecs
   */  
  std::vector< ::DBus::String > getCodecList( void ); 

  /**
   * Get the default list of codecs we supports
   * @ return The list of the codecs
   */  
  std::vector< ::DBus::String > getDefaultCodecList( void ); 

/**
 * Get the sample rate of a codec
 * @param name: The description of the codec
 * @return The sample rate of the specified codec
 */	
  unsigned int clockRate(std::string& name); 

/**
 * Get the list of the standart sound sample rates
 * Values: { 44100 , 44000 , 96000 }
 * @return The list of the sample rates
 */     
  std::vector< ::DBus::String> getSampleRateList( void );

  /*
   * Set an account as default
   * @param The ID of the account we want to set as default
   */
  void setDefaultAccount(const AccountID& accountID);


  bool getConfigAll(const std::string& sequenceId);
  bool getConfig(const std::string& section, const std::string& name, TokenList& arg);
  bool setConfig(const std::string& section, const std::string& name, const std::string& value);
  bool setConfig(const std::string& section, const std::string& name, int value);
  bool getConfigList(const std::string& sequenceId, const std::string& name);
  void selectAudioDriver(void);

  /** 
   * Set Audio Driver with switchName == audiodriver 
   * @param sflphoned internal parameter to change
   * @param message to return to the user
   * @return true if everything is ok
   */
  bool setSwitch(const std::string& switchName, std::string& message);

  // configuration function for extern
  // throw an Conf::ConfigTreeItemException if not found
  /** Get a int from the config tree */
  int getConfigInt(const std::string& section, const std::string& name);
  /** Get a string from the config tree */
  std::string getConfigString(const std::string& section, const std::string& name);

	/**
	 * Handle audio sounds heard by a caller while they wait for their 
	 * connection to a called party to be completed.
	 */
  void ringback ();

	/**
	 * Handle played music when an incoming call occurs
	 */
  void ringtone ();
  void congestion ();
  void callBusy(const CallID& id);
  void callFailure(const CallID& id);

  /** @return 0 if no tone (init before calling this function) */
  AudioLoop* getTelephoneTone();
  /** @return 0 if the wav is stopped */
  AudioLoop* getTelephoneFile();

  /**
   * @return true is there is one or many incoming call waiting
   * new call, not anwsered or refused
   */
  bool incomingCallWaiting(void);
	/**
	 * Notification of incoming call when you are already busy
	 */
  void notificationIncomingCall(void);


  /*
   * Inline functions to manage volume control
   * Read by main thread and AudioLayer thread
   * Write by main thread only
   */
  unsigned short getSpkrVolume(void) { return _spkr_volume; }
  void setSpkrVolume(unsigned short spkr_vol) {  _spkr_volume = spkr_vol; }
  unsigned short getMicVolume(void) {  return _mic_volume;  }
  void setMicVolume(unsigned short mic_vol) {    _mic_volume = mic_vol;   }

  // Manage information about firewall
  /*
   * Get information about firewall 
   * @param  stunSvrAddr: stun server
   * @param  port         port number to open to test the connection
   * @return true if the connection is successful
   */
  bool getStunInfo(StunAddress4& stunSvrAddr, int port);

  inline int getFirewallPort(void) 		{ return _firewallPort; }
  inline void setFirewallPort(int port) 	{ _firewallPort = port; }
  inline std::string getFirewallAddress (void) 	{ return _firewallAddr; }

  /**
   * If you are behind a NAT, you have to use STUN server, specified in 
   * STUN configuration(you can change this one by default) to give you an 
   * public IP address and assign a port number.
   * Note: Set firewall port/address retreive
   * @param svr  : serveur on which to send request
   * @param port : on which port we want to listen to
   * 
   * Return true if we are behind a NAT (without error)
   */
  bool behindNat(const std::string& svr, int port);

  /**
   * Init default values for the different fields in the config file.
   * Fills the local _config (Conf::ConfigTree) with the default contents.
   *
   * Called in main.cpp, just before Manager::init().
   */
  void initConfigFile (void);

  /**
   * Tell if the setup was already loaded
   */
  bool hasLoadedSetup() { return _setupLoaded; }
	
  /** Return a new random callid that is not present in the list
   * @return a brand new callid
   */
  CallID getNewCallID();

  /**
   * Get the current call id
   * @return the call id or ""
   */
  const CallID& getCurrentCallId();

  /**
   * Check if a call is the current one
   * @param id the new callid
   * @return if the id is the current call
   */
  bool isCurrentCall(const CallID& callId);

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
   * Initialize audiocodec with config setting
   */
  void initAudioCodec(void);

  /*
   * Initialize audiodriver
   */
  void initAudioDriver(void);

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
  bool getAudioDeviceList(const std::string& sequenceId, int ioDeviceMask);
  Conf::ConfigTree _config;
  bool getCountryTones(const std::string& sequenceId);
  void sendCountryTone(const std::string& sequenceId, int index, const std::string& name);
   


  /**
   * Tell if there is a current call processed
   * @return true if there is a current call
   */
  bool hasCurrentCall();

  /**
   * Switch of current call id
   * @param id the new callid
   */
  void switchCall(const CallID& id);

  /** Current Call ID */
  CallID _currentCallId2;

  /** Protected current call access */
  ost::Mutex _currentCallMutex;


  /*
   * Play one tone
   * @return false if the driver is uninitialize
   */
  bool playATone(Tone::TONEID toneId);
  
  //
  // Multithread variable with extern accessor and change only inside the main thread
  //
  /** Vector of CodecDescriptor */
  CodecDescriptor* _codecBuilder;

  //
  // Sound variable
  //
  AudioLayer* _audiodriver;

  // Main thread
  DTMF* _dtmfKey;

  // map of codec (for configlist request)
  CodecDescriptor _codecDescriptorMap;

  /////////////////////
  // Protected by Mutex
  /////////////////////
  ost::Mutex _toneMutex;
  TelephoneTone* _telephoneTone;
  AudioFile _audiofile;

  // To handle volume control
  short _spkr_volume;
  short _mic_volume;
  short _mic_volume_before_mute;
  // End of sound variable


  // Multithread variable (protected by _mutex)
  // 
  /** Mutex to protect access to code section */
  ost::Mutex _mutex;

  //
  // Multithread variable (non protected)
  //
  DBusManagerImpl * _dbus;

  /** Waiting Call Vectors */
  CallIDSet _waitingCall;
  /** Protect waiting call list, access by many voip/audio threads */
  ost::Mutex _waitingCallMutex;
  /** Number of waiting call, synchronize with waitingcall callidvector */
  unsigned int _nbIncomingWaitingCall;
  /**
   * Add incoming callid to the waiting list
   * @param id CallID to add
   */
  void addWaitingCall(const CallID& id);
  /**
   * Remove incoming callid to the waiting list
   * @param id CallID to remove
   */
  void removeWaitingCall(const CallID& id);
  /**
   * Tell if a call is waiting and should be remove
   * @param id CallID to test
   * @return true if the call is waiting
   */
  bool isWaitingCall(const CallID& id);

	/**
   * Path of the ConfigFile 
	 */
  std::string 	_path;
  int _exist;
  int _setupLoaded;

	// To handle firewall
  int _firewallPort;
  std::string _firewallAddr;

  // tell if we have zeroconf is enabled
  int _hasZeroconf;

#ifdef USE_ZEROCONF
  // DNSService contain every zeroconf services
  //  configuration detected on the network
  DNSService *_DNSService;
#endif

  /** Map to associate a CallID to the good account */
  CallAccountMap _callAccountMap;
  /** Mutex to lock the call account map (main thread + voiplink thread) */
  ost::Mutex _callAccountMapMutex;

  /** Associate a new CallID to a AccountID
   * Protected by mutex
   * @param callID the new CallID not in the list yet
   * @param accountID the known accountID present in accountMap
   * @return true if the new association is create
   */
  bool associateCallToAccount(const CallID& callID, const AccountID& accountID);

  /** Return the AccountID from a CallID
   * Protected by mutex
   * @param callID the CallID in the list
   * @return the accountID associated or "" if the callID is not found
   */
  AccountID getAccountFromCall(const CallID& callID);

  /** Remove a CallID/AccountID association
   * Protected by mutex
   * @param callID the CallID to remove
   * @return true if association is removed
   */
  bool removeCallAccount(const CallID& callID);

  /** Contains a list of account (sip, aix, etc) and their respective voiplink/calls */
  AccountMap _accountMap;

  /**
   * Load the account from configuration
   * @return number of account
   */
  short loadAccountMap();

  /**
   * Unload the account (delete them)
   */
  void unloadAccountMap();

  /**
   * Tell if an account exists
   * @param accountID account ID check
   */
  bool accountExists(const AccountID& accountID);

  /**
   * Get an account pointer
   * @param accountID account ID to get
   * @param the account pointer or 0
   */
  Account* getAccount(const AccountID& accountID);

  /**
   * Get the voip link from the account pointer
   * @param accountID account ID to get
   * @param the voip link from the account pointer or 0
   */
  VoIPLink* getAccountLink(const AccountID& accountID);


  #ifdef TEST
  bool testCallAccountMap();
  bool testAccountMap();
  #endif

};

#endif // __MANAGER_H__
