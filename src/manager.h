/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

using namespace std;
using namespace ost;

class AudioLayer;
class CodecDescriptor;
class Error;
class GuiFramework;
class ToneGenerator;
class VoIPLink;

#define	NOTIFICATION_LEN	250
// Status
#define CONNECTED_STATUS	"Connected"
#define LOGGED_IN_STATUS	"Logged in"
#define RINGING_STATUS		"Ringing"
#define TRYING_STATUS		"Trying ..."
#define HUNGUP_STATUS       "Hung up"
#define ONHOLD_STATUS       "On hold ..."
#define TRANSFER_STATUS     "Transfer to:"
#define MUTE_ON_STATUS		"Mute on"
#define ENTER_NUMBER_STATUS "Enter Phone Number:"

/*
 * Define a type for a list of call
 */
typedef vector<Call*, allocator<Call*> > CallVector;

/*
 * Define a type for a list of VoIPLink
 */
typedef vector<VoIPLink*, allocator<VoIPLink*> > VoIPLinkVector;

/*
 * Define a type for a list of CodecDescriptor
 */
typedef vector<CodecDescriptor*, allocator<CodecDescriptor*> > CodecDescriptorVector;

struct device_t{
	const char* hostApiName;
	const char* deviceName;
};

class Manager {
public:
	Manager (void);
	~Manager (void);

	static device_t deviceParam;
	
	void init (void);
	void setGui (GuiFramework* gui);
	ToneGenerator* getTonegenerator(void);
	Error* error(void);
	AudioLayer* getAudioDriver(void);

	// Accessor to number of calls 
	unsigned int getNumberOfCalls (void);
	// Modifior of number of calls 
	void setNumberOfCalls (unsigned int nCalls);
	
	// Accessor to current call id 
	short getCurrentCallId (void);
	// Modifior of current call id 
	void setCurrentCallId (short currentCallId);

	// Accessor to the Call vector 
	CallVector* getCallVector (void);
	// Accessor to the Call with the id 'id' 
	Call* getCall (short id);
	
	unsigned int getNumberOfCodecs (void);
	void setNumberOfCodecs (unsigned int nb_codec);
	
	VoIPLinkVector* getVoIPLinkVector (void);

	CodecDescriptorVector* getCodecDescVector(void);

	inline bool getTonezone (void) { return _tonezone; }
	inline void setTonezone (bool b) { _tonezone = b; }

	/* 
	 * Attribute a new random id for a new call 
	 * and check if it's already attributed to existing calls. 
	 * If not exists, returns 'id' otherwise return 0 
	 */   
	short generateNewCallId (void);

	/*
	 * Add a new call at the end of the CallVector with identifiant 'id'
	 */
	void pushBackNewCall (short id, enum CallType type);
	
	/*
	 * Erase the Call(id) from the CallVector
	 */
	void deleteCall	(short id);
	
	int outgoingCall (const string& to);
	int hangupCall (short id);
	int cancelCall (short id);
	int answerCall (short id);
	int onHoldCall (short id);
	int offHoldCall (short id);
	int transferCall (short id, const string& to);
	int muteOn (short id);
	int muteOff (short id);
	int refuseCall (short id);

	int saveConfig (void);
	int registerVoIPLink (void);
	int quitApplication (void);
	int sendTextMessage (short id, const string& message);
	int accessToDirectory (void);
	
	/**
   	 * Handle choice of the DTMF-send-way
 	 *
 	 * @param   id: callid of the line.
  	 * @param   code: pressed key.
 	 */
	int sendDtmf (short id, char code);
	

	int incomingCall (short id);
	int peerAnsweredCall (short id);
	int peerRingingCall (short id);
	int peerHungupCall (short id);
	void displayTextMessage (short id, const string& message);
	void displayErrorText (const string& message);
	void displayError (const string& error);
	void displayStatus (const string& status);
	int selectedCall (void);
	bool isCurrentId (short id);
	
	/*
	 * Handle audio sounds heard by a caller while they wait for their 
	 * connection to a called party to be completed.
	 */
	void ringback (bool var);

	void ringtone (bool var);
	void congestion (bool var);

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
	
	inline bool getbCongestion 	(void) { return _congestion; }
	inline bool getbRingback 	(void) { return _ringback; }
	inline bool getbRingtone 	(void) { return _ringtone; }
	
	inline int getSpkrVolume 	(void) 			{ return _spkr_volume; }
	inline void setSpkrVolume 	(int spkr_vol) 	{ _spkr_volume = spkr_vol; }
	inline int getMicroVolume 	(void) 			{ return _mic_volume; }
	inline void setMicroVolume 	(int mic_vol) 	{ _mic_volume = mic_vol; }
	
	inline int getFirewallPort 		(void) 		{ return _firewallPort; }
	inline void setFirewallPort 	(int port) 	{ _firewallPort = port; }
	inline string getFirewallAddress (void) 	{ return _firewallAddr; }

	inline bool isDriverLoaded (void) { return _loaded; }
	inline void loaded (bool l) { _loaded = l; }

	static device_t deviceList (int);
	static int deviceCount (void);
	static bool defaultDevice (int);
	
private:

	/*
	 * Returns the number of calls in the vector
	 */
	unsigned int callVectorSize (void);

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
	 * Init default values for the different fields
	 */
	void initConfigFile (void);

	void initAudioCodec(void);
	void selectAudioDriver (void);
	
/////////////////////
// Private variables
/////////////////////
	ToneGenerator* _tone;
	Error* _error;
	GuiFramework* _gui;
	AudioLayer* _audiodriverPA;

	/*
	 * Vector of VoIPLink
	 */
	VoIPLinkVector* _voIPLinkVector;
	
	/*
	 * Vector of calls
	 */
	CallVector* _callVector;

	/*
	 * Vector of CodecDescriptor
	 */
	CodecDescriptorVector* _codecDescVector;

	/*
	 * Mutex to protect access to code section
	 */
	Mutex		_mutex;
	
	unsigned int _nCalls;
	short _currentCallId;

	/*
	 * For the call timer
	 */
	unsigned int _startTime;
	unsigned int _endTime;

	/* Path of the ConfigFile 
	 */
	string 	_path;
	int 	_exist;

	unsigned int _nCodecs;
	bool         _tonezone;
	bool		 _congestion;
	bool		 _ringback;
	bool		 _ringtone;

	// To handle volume control
	int 		_spkr_volume;
	int 		_mic_volume;

	// To handle firewall
	int			_firewallPort;
	string		_firewallAddr;

	// Variables used in exception
	bool 		_loaded;
};

#endif // __MANAGER_H__
