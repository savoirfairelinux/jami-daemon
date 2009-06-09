/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
#include "numbercleaner.h"
#include <history/historymanager.h>

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

/** Define a type for a AccountMap container */
typedef std::map<AccountID, Account*> AccountMap;

/** Define a type for a CallID to AccountID Map inside ManagerImpl */
typedef std::map<CallID, AccountID> CallAccountMap;

typedef std::map<CallID, Call::CallConfiguration> CallConfigMap;

/** Define a type for CallID vector (waiting list, incoming not answered) */
typedef std::set<CallID> CallIDSet;

/** To send multiple string */
typedef std::list<std::string> TokenList;

/** Manager (controller) of sflphone daemon */
class ManagerImpl {
  public:
    ManagerImpl (void);
    ~ManagerImpl (void);

    /**
     * Initialisation of thread (sound) and map.
     * Init a new VoIPLink, audio codec and audio driver
     */
    void init (void);

    /**
     * Terminate all thread (sound, link) and unload AccountMap
     */
    void terminate (void);

    /**
     * Set user interface manager.
     * @param man The DBUS interface implementation
     */
    void setDBusManager (DBusManagerImpl* man) { _dbus = man; }

    /**
     * Accessor to audiodriver.
     * it's multi-thread and use mutex internally
     * @return AudioLayer*  The audio layer object
     */
    AudioLayer* getAudioDriver(void) const { return _audiodriver; }

    /**
     * Get a descriptor map of codec available
     * @return CodecDescriptor  The internal codec map
     */
    CodecDescriptor& getCodecDescriptorMap(void) {return _codecDescriptorMap;}

    /**
     * Functions which occur with a user's action
     * Place a new call
     * @param accountId	The account to make tha call with
     * @param id  The call identifier
     * @param to  The recipient of the call
     * @return bool true on success
     *		  false otherwise
     */
    bool outgoingCall(const AccountID& accountId, const CallID& id, const std::string& to);

    /**
     * Functions which occur with a user's action
     * Answer the call
     * @param id  The call identifier
     */
    bool answerCall(const CallID& id);

    /**
     * Functions which occur with a user's action
     * Hangup the call
     * @param id  The call identifier
     */
    bool hangupCall(const CallID& id);

    /**
     * Functions which occur with a user's action
     * Cancel the call
     * @param id  The call identifier
     */
    bool cancelCall(const CallID& id);

    /**
     * Functions which occur with a user's action
     * Put the call on hold
     * @param id  The call identifier
     */
    bool onHoldCall(const CallID& id);

    /**
     * Functions which occur with a user's action
     * Put the call off hold
     * @param id  The call identifier
     */
    bool offHoldCall(const CallID& id);

    /**
     * Functions which occur with a user's action
     * Transfer the call
     * @param id  The call identifier
     * @param to  The recipient of the transfer
     */
    bool transferCall(const CallID& id, const std::string& to);

    /**
     * Notify the client the transfer is successful
     */
    void transferSucceded();

    /**
     * Notify the client that the transfer failed
     */
    void transferFailed();

    /**
     * Functions which occur with a user's action
     * Refuse the call
     * @param id  The call identifier
     */
    bool refuseCall(const CallID& id);

    /**
     * Save config to file
     * @return true on success
     *	    false otherwise
     */
    bool saveConfig (void);

    /**
     * Send registration to all enabled accounts
     * @return 0 on registration success
     *          1 otherelse
     */
    int initRegisterAccounts();

    /**
     * @return true if we tried to register once
     */
    bool _hasTriedToRegister;

    /**
     * Handle choice of the DTMF-send-way
     * @param   id: callid of the line.
     * @param   code: pressed key.
     */
    bool sendDtmf(const CallID& id, char code);

    /**
     * Play the dtmf-associated sound
     * @param code  The pressed key
     * @param isTalking	In conversation or not. Useful to know whether or not the sound streams are started
     */
    bool playDtmf(char code, bool isTalking);

    /**
     * Play a ringtone
     * @return bool True on success
     *	      false otherwise
     */
    bool playTone ();

    /**
     * Play a special ringtone ( BUSY ) if there's at least one message on the voice mail
     * @return bool True on success
     *	      false otherwise
     */
    bool playToneWithMessage ();

    /**
     * Acts on the audio streams and audio files
     * @param stopAudio	Tells whether or not to stop the streams
     */
    void stopTone(bool stopAudio);

    /**
     * When receiving a new incoming call, add it to the callaccount map
     * and notify user
     * @param call A call pointer
     * @param accountId an account id
     * @return bool True if the call was added correctly
     */
    bool incomingCall(Call* call, const AccountID& accountId);

    /**
     * Notify the user that the recipient of the call has answered and the put the
     * call in Current state
     * @param id  The call identifier
     */
    void peerAnsweredCall(const CallID& id);

    /**
     * Rings back because the outgoing call is ringing and the put the
     * call in Ringing state
     * @param id  The call identifier
     */
    void peerRingingCall(const CallID& id);

    /**
     * Put the call in Hungup state, remove the call from the list
     * @param id  The call identifier
     */
    void peerHungupCall(const CallID& id);

    /**
     * Notify the client with an incoming message
     * @param accountId	The account identifier
     * @param message The content of the message
     */
    void incomingMessage(const AccountID& accountId, const std::string& message);

    /**
     * Notify the client he has voice mails
     * @param accountId	  The account identifier
     * @param nb_msg The number of messages
     */
    void startVoiceMessageNotification(const AccountID& accountId, int nb_msg);

    /**
     * Notify the client through DBus that registration state has been updated
     */
    void connectionStatusNotification(void);

    /**
     * ConfigurationManager - Send registration request
     * @param accountId The account to register/unregister
     * @param expire The flag for the type of registration
     *		 0 for unregistration request
     *		 1 for registration request
     */
    void sendRegister( const ::std::string& accountId , const int32_t& expire );

    bool getCallStatus(const std::string& sequenceId);

    /**
     * Get account list
     * @return std::vector<std::string> A list of accoundIDs
     */
    std::vector< std::string >  getAccountList();

    /**
     * Set the account order in the config file
     */
    void setAccountsOrder (const std::string& order);

    /**
     * Retrieve details about a given account
     * @param accountID	  The account identifier
     * @return std::map< std::string, std::string > The account details
     */
    std::map< std::string, std::string > getAccountDetails(const AccountID& accountID);

    /**
     * Retrieve details about a given call
     * @param callID	  The account identifier
     * @return std::map< std::string, std::string > The call details
     */
    std::map< std::string, std::string > getCallDetails(const CallID& callID);

    /**
     * Get call list
     * @return std::vector<std::string> A list of call IDs
     */
    std::vector< std::string >  getCallList (void);

    /**
     * Save the details of an existing account, given the account ID
     * This will load the configuration map with the given data.
     * It will also register/unregister links where the 'Enabled' switched.
     * @param accountID	  The account identifier
     * @param details	  The account parameters
     */
    void setAccountDetails( const ::std::string& accountID,
	const std::map< ::std::string, ::std::string >& details );

    /**
     * Add a new account, and give it a new account ID automatically
     * @param details The new account parameters
     * @return The account Id given to the new account
     */
    std::string addAccount(const std::map< ::std::string, ::std::string >& details);

    /**
     * Delete an existing account, unregister VoIPLink associated, and
     * purge from configuration.
     * @param accountID	The account unique ID
     */
    void removeAccount(const AccountID& accountID);

    /**
     * Get the list of codecs we supports, not ordered
     * @return The list of the codecs
     */
    std::vector< ::std::string > getCodecList( void );

    /**
     * Get the info about one codec
     * Name / Clock rate / bitrate / bandwidth
     * @param payload The payload of the codec
     * @return std::vector<::DBus::string> The information
     */
    std::vector< ::std::string > getCodecDetails( const int32_t& payload);

    /**
     * Get current codec name
     * @param call id
     * @return std::string The codec name
     */
    std::string getCurrentCodecName(const CallID& id);

    /**
     * Get a list of supported input audio plugin
     * @return std::vector<std::string> List of names
     */
    std::vector< std::string> getInputAudioPluginList(void);

    /**
     * Get a list of supported output audio plugin
     * @return std::vector<std::string> List of names
     */
    std::vector< std::string> getOutputAudioPluginList(void);

    /**
     * Set input audio plugin
     * @param audioPlugin The audio plugin
     */
    void setInputAudioPlugin(const std::string& audioPlugin);

    /**
     * Set output audio plugin
     * @param audioPlugin The audio plugin
     */
    void setOutputAudioPlugin(const std::string& audioPlugin);

    /**
     * Get list of supported audio output device
     * @return std::vector<std::string> A list of the audio devices supporting playback
     */
    std::vector<std::string> getAudioOutputDeviceList(void);

    /**
     * Set audio output device
     * @param index The index of the soundcard
     */
    void setAudioOutputDevice(const int index);

    /**
     * Get list of supported audio input device
     * @return std::vector<std::string> A list of the audio devices supporting capture
     */
    std::vector<std::string> getAudioInputDeviceList(void);

    /**
     * Set audio input device
     * @param index The index of the soundcard
     */
    void setAudioInputDevice(const int index);

    /**
     * Get string array representing integer indexes of output and input device
     * @return std::vector<std::string> A list of the current audio devices
     */
    std::vector<std::string> getCurrentAudioDevicesIndex();

    /**
     * Get index of an audio device
     * @param name The string description of an audio device
     * @return int  His index
     */
    int getAudioDeviceIndex( const std::string name );

    /*
     * Get current alsa plugin
     * @return std::string  The Alsa plugin
     */
    std::string getCurrentAudioOutputPlugin( void );

    /**
     * Convert a list of payload in a special format, readable by the server.
     * Required format: payloads separated with one slash.
     * @return std::string The serializabled string
     */
    std::string serialize(std::vector<std::string> v);

    std::vector<std::string> unserialize(std::string v);

    /**
     * Tells if IAX2 support is enabled
     * @return int 1 if IAX2 is enabled
     *	       0 otherwise
     */
    int isIax2Enabled( void );

    /**
     * Ringtone option.
     * If ringtone is enabled, ringtone on incoming call use custom choice. If not, only standart tone.
     * @return int	1 if enabled
     *	        0 otherwise
     */
    int isRingtoneEnabled( void );

    /**
     * Set the ringtone option
     * Inverse current value
     */
    void ringtoneEnabled( void );

    /**
     * Get the ringtone
     * @return gchar* The file name selected as a ringtone
     */
    std::string getRingtoneChoice( void );

    /**
     * Set a ringtone
     * @param tone The file name of the ringtone
     */
    void setRingtoneChoice( const std::string& );

    /**
     * Get the recording path from configuration tree
     * @return the string correspoding to the path
     */
    std::string getRecordPath( void );

    /**
     * Set the recoding path in the configuration tree
     * @param a string reresenting the path
     */
    void setRecordPath( const std::string& recPath);

    /**
     * Tells if the user wants to display the dialpad or not
     * @return int 1 if dialpad has to be displayed
     *	       0 otherwise
     */
    int getDialpad( void );

    /**
     * Set the dialpad visible or not
     */
    void setDialpad( void );

    /**
     * Tells if the user wants to display the volume controls or not
     * @return int 1 if the controls have to be displayed
     *	       0 otherwise
     */
    int getVolumeControls( void );

    /**
     * Set the volume controls ( mic and speaker ) visible or not
     */
    void setVolumeControls( void );

    /**
     * Set recording on / off
     * Start recording
     * @param id  The call identifier
     */
    void setRecordingCall(const CallID& id);

    /**
     * Return true if the call is currently recorded
     */
    bool isRecording(const CallID& id);

    /**
     * Set the maximum number of calls to keep in the history
     * @param calls The number of calls
     */
    void setMaxCalls( const int& calls );

    /**
     * Get the maximum number of calls to keep in the history
     * @return double The number of calls
     */
    int getMaxCalls( void );

    /**
     * Configure the start-up option
     * @return int	1 if SFLphone should start in the system tray
     *	        0 otherwise
     */
    int isStartHidden( void );

    /**
     * Configure the start-up option
     * At startup, SFLphone can be displayed or start hidden in the system tray
     */
    void startHidden( void );

    /**
     * Configure the popup behaviour
     * @return int	1 if it should popup on incoming calls
     *		0 if it should never popups
     */
    int popupMode( void );

    /**
     * Configure the popup behaviour
     * When SFLphone is in the system tray, you can configure when it popups
     * Never or only on incoming calls
     */
    void switchPopupMode( void );

    /**
     * Determine whether or not the search bar (history) should be displayed
     */
    int getSearchbar( void );

    /**
     * Configure the search bar behaviour
     */
    void setSearchbar( void );

    /**
     * Set the desktop notification level
     */
    void setNotify( void );

    /**
     * Get the desktop notification level
     * @return int The notification level
     */
    int32_t getNotify( void );

    /**
     * Set the desktop mail notification level
     */
    void setMailNotify( void );


    /**
     * Addressbook configuration
     */
    std::map<std::string, int32_t> getAddressbookSettings (void);

    /**
     * Addressbook configuration
     */
    void setAddressbookSettings (const std::map<std::string, int32_t>& settings);

    /**
     * Addressbook list
     */
    void setAddressbookList(const std::vector<  std::string >& list);

    /**
     * Addressbook list
     */
    std::vector <std::string> getAddressbookList( void );

    /**
     * Hook configuration
     */
    std::map<std::string, std::string> getHookSettings (void);

    /**
     * Hook configuration
     */
     void setHookSettings (const std::map<std::string, std::string>& settings);


    /**
     * Get the audio manager
     * @return int The audio manager
     *		    0 ALSA
     *		    1 PULSEAUDIO
     */
    int32_t getAudioManager( void );

    /**
     * Set the audio manager
     */
    void setAudioManager( const int32_t& api );

    void switchAudioManager( void );

    void setPulseAppVolumeControl( void );
    int32_t getPulseAppVolumeControl( void );

    /**
     * Get the desktop mail notification level
     * @return int The mail notification level
     */
    int32_t getMailNotify( void );

    /**
     * Get the expire value for SIP registration, set in the user configuration file
     */
    int getRegistrationExpireValue( void );

    /**
     * Retrieve the formatted list of codecs payload in the user config file and
     * load in the active list of codecs
     * @return std::vector<std::string>	  The vector containing the active codecs
     */
    std::vector<std::string> retrieveActiveCodecs( void );

    /**
     * Get the list of the active codecs
     * @return std::vector< ::std::string >  The list of active codecs
     */
    std::vector< ::std::string > getActiveCodecList( void );

    /**
     * Set the list of the active codecs
     * @param list  The new list of active codecs
     */
    void setActiveCodecList( const std::vector< ::std::string >& list);

    /*
     * Notify the client that an error occured
     * @param errCode The error code. Could be: ALSA_CAPTURE_ERROR
     *					       ALSA_PLAYBACK_ERROR
     */
    void notifyErrClient( const int32_t& errCode );

    /**
     * Retrieve in the configuration tree the value of a parameter in a specific section
     * @param section	The section to look in
     * @param name	The name of the parameter you want to get
     * @param arg	Undocumented
     * @return bool	true on success
     *			false otherwise
     */
    bool getConfig(const std::string& section, const std::string& name, TokenList& arg);

    /**
     * Change a specific value in the configuration tree.
     * This value will then be saved in the user config file sflphonedrc
     * @param section	The section name
     * @param name	The parameter name
     * @param value	The new string value
     * @return bool	true on success
     *		      false otherwise
     */
    bool setConfig(const std::string& section, const std::string& name, const std::string& value);

    /**
     * Change a specific value in the configuration tree.
     * This value will then be saved in the user config file sflphonedrc
     * @param section	The section name
     * @param name	The parameter name
     * @param value	The new int value
     * @return bool	true on success
     *		      false otherwise
     */
    bool setConfig(const std::string& section, const std::string& name, int value);

    /**
     * Get a int from the configuration tree
     * Throw an Conf::ConfigTreeItemException if not found
     * @param section The section name to look in
     * @param name    The parameter name
     * @return int    The int value
     */
    int getConfigInt(const std::string& section, const std::string& name);

    /**
     * Get a string from the configuration tree
     * Throw an Conf::ConfigTreeItemException if not found
     * @param section The section name to look in
     * @param name    The parameter name
     * @return sdt::string    The string value
     */
    std::string getConfigString(const std::string& section, const std::string& name);

    /**
     * Retrieve the soundcards index in the user config file and try to open audio devices
     * with a specific alsa plugin.
     * Set the audio layer sample rate
     */
    void selectAudioDriver(void);

    /**
     * Handle audio sounds heard by a caller while they wait for their
     * connection to a called party to be completed.
     */
    void ringback ();

    /**
     * Handle played music when an incoming call occurs
     */
    void ringtone ();

    /**
     * Handle played music when a congestion occurs
     */
    void congestion ();

    /**
     * Handle played sound when a call can not be conpleted because of a busy recipient
     */
    void callBusy(const CallID& id);

    /**
     * Handle played sound when a failure occurs
     */
    void callFailure(const CallID& id);

    /**
     * Retrieve the current telephone tone
     * @return AudioLoop*   The audio tone or 0 if no tone (init before calling this function)
     */
    AudioLoop* getTelephoneTone();

    /**
     * Retrieve the current telephone file
     * @return AudioLoop* The audio file or 0 if the wav is stopped
     */
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
     * Inline functions to manage speaker volume control
     * Read by main thread and AudioLayer thread
     * Write by main thread only
     * @return unsigned short	The volume value
     */
    unsigned short getSpkrVolume(void) { return _spkr_volume; }

    /*
     * Inline functions to manage speaker volume control
     * Read by main thread and AudioLayer thread
     * Write by main thread only
     * @param spkr_vol	The volume value
     */
    void setSpkrVolume(unsigned short spkr_vol);

    /*
     * Inline functions to manage mic volume control
     * Read by main thread and AudioLayer thread
     * Write by main thread only
     * @return unsigned short	The volume value
     */
    unsigned short getMicVolume(void) {  return _mic_volume;  }

    /*
     * Inline functions to manage mic volume control
     * Read by main thread and AudioLayer thread
     * Write by main thread only
     * @param mic_vol	The volume value
     */
    void setMicVolume(unsigned short mic_vol);

    // Manage information about firewall

    /*
     * Get information about firewall
     * @param  stunSvrAddr: stun server
     * @param  port         port number to open to test the connection
     * @return true if the connection is successful
     */
    bool getStunInfo(StunAddress4& stunSvrAddr, int port);

    /*
     * Inline functions to manage firewall settings
     * @return int The firewall port
     */
    inline int getFirewallPort(void) 		{ return _firewallPort; }

    /*
     * Inline functions to manage firewall settings
     * @param port The firewall port
     */
    inline void setFirewallPort(int port) 	{ _firewallPort = port; }

    /*
     * Inline functions to manage firewall settings
     * @return std::string The firewall address
     */
    inline std::string getFirewallAddress (void) 	{ return _firewallAddr; }

    /**
     * If you are behind a NAT, you have to use STUN server, specified in
     * STUN configuration(you can change this one by default) to give you an
     * public IP address and assign a port number.
     * Note: Set firewall port/address retreive
     * @param svr   Server on which to send request
     * @param port  On which port we want to listen to
     * @return true if we are behind a NAT (without error)
     */
    bool behindNat(const std::string& svr, int port);

    /**
     * Init default values for the different fields in the config file.
     * Fills the local _config (Conf::ConfigTree) with the default contents.
     * Called in main.cpp, just before Manager::init().
     */
    void initConfigFile ( bool load_user_value=true, std::string alternate="");

    /**
     * Tell if the setup was already loaded
     * @return bool True if yes
     *		  false otherwise
     */
    bool hasLoadedSetup() { return _setupLoaded; }

    /**
     * Return a new random callid that is not present in the list
     * @return CallID A brand new callid
     */
    CallID getNewCallID();

    /**
     * Get the current call id
     * @return CallID	The call id or ""
     */
    const CallID& getCurrentCallId();

    /**
     * Check if a call is the current one
     * @param callId the new callid
     * @return bool   True if the id is the current call
     */
    bool isCurrentCall(const CallID& callId);

    /**
     * Restart PJSIP
     * @param void
     * @return void
     */
    void restartPJSIP( );

    void unregisterCurSIPAccounts();

    void registerCurSIPAccounts(VoIPLink *link);

    /**
     * Returns a map with only the existing SIP accounts
     */
    AccountMap getSipAccountMap( void );

    /*
     * Initialize audiodriver
     */
    bool initAudioDriver(void);

  private:

    /**
     * Check if a process is running with the system command
     *
     * @return 0 on success
     *          1 otherelse
     */
    int app_is_running(std::string process);

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
     * Initialize zeroconf module and scanning
     */
    void initZeroconf(void);

    /*
     * Init the volume for speakers/micro from 0 to 100 value
     */
    void initVolume();

    /**
     * Tell if there is a current call processed
     * @return bool True if there is a current call
     */
    bool hasCurrentCall();

    /**
     * Switch of current call id
     * @param id The new callid
     */
    void switchCall(const CallID& id);

    /*
     * Play one tone
     * @return false if the driver is uninitialize
     */
    bool playATone(Tone::TONEID toneId);

    /** The configuration tree. It contains accounts parameters, general user settings ,audio settings, ... */
    Conf::ConfigTree _config;

    /** Current Call ID */
    CallID _currentCallId2;

    /** Protected current call access */
    ost::Mutex _currentCallMutex;

    /** Vector of CodecDescriptor */
    CodecDescriptor* _codecBuilder;

    /** Audio layer */
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
    // End of sound variable


    // Multithread variable (protected by _mutex)
    //
    /** Mutex to protect access to code section */
    ost::Mutex _mutex;

    // Multithread variable (non protected)
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
     * @return bool True if the call is waiting
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

    CallConfigMap _callConfigMap;

    bool associateConfigToCall (const CallID& callID, Call::CallConfiguration config);

    Call::CallConfiguration getConfigFromCall(const CallID& callID);

    bool removeCallConfig(const CallID& callID);

    /** Associate a new CallID to a AccountID
     * Protected by mutex
     * @param callID the new CallID not in the list yet
     * @param accountID the known accountID present in accountMap
     * @return bool True if the new association is create
     */
    bool associateCallToAccount(const CallID& callID, const AccountID& accountID);

    /** Remove a CallID/AccountID association
     * Protected by mutex
     * @param callID the CallID to remove
     * @return bool True if association is removed
     */
    bool removeCallAccount(const CallID& callID);

    /**
     *Contains a list of account (sip, aix, etc) and their respective voiplink/calls */
    AccountMap _accountMap;

    /**
     * Load the account from configuration
     * @return short Number of account
     */
    short loadAccountMap();

    /**
     * Load the accounts order set by the user from the sflphonedrc config file
     * @return std::vector<std::string> A vector containing the account ID's
     */
    std::vector<std::string> loadAccountOrder ();


    /**
     * Unload the account (delete them)
     */
    void unloadAccountMap();

   public:
     /**
     * Tell if an account exists
     * @param accountID account ID check
     * @return bool True if the account exists
     *		  false otherwise
     */
    bool accountExists(const AccountID& accountID);

    std::map<std::string, std::string> send_history_to_client (void); 

    void receive_history_from_client (std::map<std::string, std::string> history);
    /**
     * Get an account pointer
     * @param accountID account ID to get
     * @return Account*	 The account pointer or 0
     */
    Account* getAccount(const AccountID& accountID);

    /** Return the AccountID from a CallID
     * Protected by mutex
     * @param callID the CallID in the list
     * @return AccountID  The accountID associated or "" if the callID is not found
     */
    AccountID getAccountFromCall(const CallID& callID);

    /**
     * Get the voip link from the account pointer
     * @param accountID	  Account ID to get
     * @return VoIPLink*   The voip link from the account pointer or 0
     */
    VoIPLink* getAccountLink(const AccountID& accountID=AccountNULL);

    VoIPLink* getSIPAccountLink (void);

    AccountID getAccountIdFromNameAndServer(const std::string& userName, const std::string& server);

    int getSipPort();

    void setSipPort( int port );

    std::string getStunServer (void);
    void setStunServer (const std::string &server);

    int isStunEnabled (void);
    void enableStun (void);
private:

    // Copy Constructor
    ManagerImpl(const ManagerImpl& rh);

    // Assignment Operator
    ManagerImpl& operator=( const ManagerImpl& rh);

    NumberCleaner *_cleaner;

    /**
      * To handle the persistent history
      */
    HistoryManager *_history;

    /**
     * Check if the call is a classic call or a direct IP-to-IP call
     */
    void check_call_configuration (const CallID& id, const std::string& to, Call::CallConfiguration *callConfig);

#ifdef TEST
    bool testCallAccountMap();
    bool testAccountMap();
#endif

    friend class ConfigurationTest;
    friend class HistoryTest;
};

#endif // __MANAGER_H__
