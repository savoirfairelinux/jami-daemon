/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "managerimpl.h"

#include "account.h"
#include "dbus/callmanager.h"
#include "global.h"
#include "sip/sipaccount.h"

#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "audio/sound/tonelist.h"
#include "history/historymanager.h"
#include "accountcreator.h" // create new account
#include "sip/sipvoiplink.h"
#include "iax/iaxvoiplink.h"
#include "manager.h"
#include "dbus/configurationmanager.h"

#include "conference.h"

#include <cerrno>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>  // mkdir(2)

#include <pwd.h>       // getpwuid

#define DIRECT_IP_CALL	"IP CALL"

#define fill_config_str(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_str)))
#define fill_config_int(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_int)))

#define MD5_APPEND(pms,buf,len) pj_md5_update(pms, (const pj_uint8_t*)buf, len)

ManagerImpl::ManagerImpl (void) :
    _hasTriedToRegister (false), _config(), _currentCallId2(),
    _currentCallMutex(), _audiodriver (NULL),
    _dtmfKey (NULL), _audioCodecFactory(), _toneMutex(),
    _telephoneTone (NULL), _audiofile (NULL), _spkr_volume (0),
    _mic_volume (0), _mutex(), _dbus (NULL), _waitingCall(),
    _waitingCallMutex(), _nbIncomingWaitingCall (0), _path (""),
    _setupLoaded (false), _callAccountMap(),
    _callAccountMapMutex(), _callConfigMap(), _accountMap(),
    _directIpAccount (NULL), _cleaner (NULL), _history (NULL)
{

    // initialize random generator for call id
    srand (time (NULL));

    _cleaner = new NumberCleaner();
    _history = new HistoryManager();
    _imModule = new sfl::InstantMessaging();

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
    if (_audiofile) {
        delete _audiofile;
        _audiofile = NULL;
    }

    delete _cleaner;
    _cleaner = NULL;
    delete _history;
    _history = NULL;
    delete _imModule;
    _imModule = NULL;

    _debug ("Manager: %s stop correctly.", PROGNAME);
}

void ManagerImpl::init ()
{

    _debug ("Manager: Init");

    // Load accounts, init map
    buildConfiguration();

    _debug ("Manager: account map loaded");

    initVolume();
    initAudioDriver();
    selectAudioDriver();

    // Initialize the list of supported audio codecs
    initAudioCodec();

    audioLayerMutexLock();

    if (_audiodriver != NULL) {
        unsigned int sampleRate = _audiodriver->getSampleRate();

        _debugInit ("Manager: Load telephone tone");
        std::string country = preferences.getZoneToneChoice();
        _telephoneTone = new TelephoneTone (country, sampleRate);

        _debugInit ("Manager: Loading DTMF key (%d)", sampleRate);
        // if(sampleRate > 44100)

        sampleRate = 8000;

        _dtmfKey = new DTMF (sampleRate);
    }

    audioLayerMutexUnlock();

    // Load the history
    _history->load_history (preferences.getHistoryLimit());

    // Init the instant messaging module
    _imModule->init();


}

void ManagerImpl::terminate ()
{

    _debug ("Manager: Terminate ");

    std::vector<std::string> callList = getCallList();
    _debug ("Manager: Hangup %d remaining call", callList.size());
    std::vector<std::string>::iterator iter = callList.begin();

    while (iter != callList.end()) {
        hangupCall (*iter);
        iter++;
    }

    unloadAccountMap();

    _debug ("Manager: Unload DTMF key");
    delete _dtmfKey;

    _debug ("Manager: Unload telephone tone");
    delete _telephoneTone;
    _telephoneTone = NULL;

    audioLayerMutexLock();

    _debug ("Manager: Unload audio driver");
    delete _audiodriver;
    _audiodriver = NULL;

    _debug ("Manager: Unload audio codecs ");
    _audioCodecFactory.deleteHandlePointer();

}

bool ManagerImpl::isCurrentCall (const CallID& callId)
{
    return (_currentCallId2 == callId ? true : false);
}

bool ManagerImpl::hasCurrentCall ()
{
    // _debug ("ManagerImpl::hasCurrentCall current call ID = %s", _currentCallId2.c_str());

    if (_currentCallId2 != "") {
        return true;
    }

    return false;
}

const CallID&
ManagerImpl::getCurrentCallId ()
{
    return _currentCallId2;
}

void ManagerImpl::switchCall (const CallID& id)
{
    ost::MutexLock m (_currentCallMutex);
    _debug ("----- Switch current call id to %s -----", id.c_str());
    _currentCallId2 = id;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool ManagerImpl::outgoingCall (const std::string& account_id,
                                const CallID& call_id, const std::string& to, const std::string& conf_id)
{

    std::string pattern, to_cleaned;
    Call::CallConfiguration callConfig;
    SIPVoIPLink *siplink;

    if (call_id.empty()) {
        _debug ("Manager: New outgoing call abbort, missing callid");
        return false;
    }

    // Call ID must be unique
    if (getAccountFromCall (call_id) != "") {
        _error ("Manager: Error: Call id already exists in outgoing call");
        return false;
    }


    _debug ("Manager: New outgoing call %s to %s", call_id.c_str(), to.c_str());

    stopTone();

    CallID current_call_id = getCurrentCallId();

    if (hookPreference.getNumberEnabled()) {
        _cleaner->set_phone_number_prefix (hookPreference.getNumberAddPrefix());
    }
    else {
        _cleaner->set_phone_number_prefix ("");
    }
    to_cleaned = _cleaner->clean (to);

    /* Check what kind of call we are dealing with */
    checkCallConfiguration (call_id, to_cleaned, &callConfig);

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        _debug ("Manager: Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conferenceand this and is not a conference participant
        if (!isConference (current_call_id) && !participToConference (current_call_id)) {
       	    onHoldCall (current_call_id);
        } else if (isConference (current_call_id) && !participToConference (call_id)) {
            detachParticipant (default_id, current_call_id);
        }
    }

    if (callConfig == Call::IPtoIP) {
        _debug ("Manager: Start IP2IP call");
        /* We need to retrieve the sip voiplink instance */
        siplink = SIPVoIPLink::instance ();

        if (siplink->SIPNewIpToIpCall(call_id, to_cleaned)) {
            switchCall (call_id);
            return true;
        } else {
            callFailure (call_id);
        }

        return false;
    }

    _debug ("Manager: Selecting account %s", account_id.c_str());

    // Is this accout exist
    if (!accountExists (account_id)) {
        _error ("Manager: Error: Account doesn't exist in new outgoing call");
        return false;
    }

    if(!associateCallToAccount (call_id, account_id)) {
    	_warn("Manager: Warning: Could not associate call id %s to account id %s", call_id.c_str(), account_id.c_str());
    }

    Call *call = NULL;
    try {
        call = getAccountLink(account_id)->newOutgoingCall (call_id, to_cleaned);
        
        switchCall (call_id);
        
        call->setConfId(conf_id);
    } catch (VoipLinkException &e) {
        callFailure (call_id);
        _error ("Manager: %s", e.what());
	return false;
    }

    getMainBuffer()->stateInfo();

    return true;
}

//THREAD=Main : for outgoing Call
bool ManagerImpl::answerCall (const CallID& call_id)
{

    _debug ("Manager: Answer call %s", call_id.c_str());

    // If sflphone is ringing
    stopTone();

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    // Retreive call coresponding to this id
    std::string account_id = getAccountFromCall (call_id);
    Call *call = getAccountLink (account_id)->getCall (call_id);
    if (call == NULL) {
        _error("Manager: Error: Call is null");
    }

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        _debug ("Manager: Currently conversing with %s", current_call_id.c_str());

        if (!isConference(current_call_id) && !participToConference (current_call_id)) {
            // if it is not a conference and is not a conference participant
            _debug ("Manager: Answer call: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall (current_call_id);
        } else if (isConference (current_call_id) && !participToConference (call_id)) {
            // if we are talking to a conference and we are answering an incoming call
            _debug ("Manager: Detach main participant from conference");
            detachParticipant (default_id, current_call_id);
        }
    }

    try {
        if (!getAccountLink (account_id)->answer (call_id)) {
            removeCallAccount (call_id);
            return false;
        }
    }
    catch(VoipLinkException &e) {
    	_error("Manager: Error: %s", e.what());
    }


    // if it was waiting, it's waiting no more
    removeWaitingCall (call_id);

    // if we dragged this call into a conference already
    if (participToConference (call_id)) {
        switchCall (call->getConfId());
    } else {
        switchCall (call_id);
    }

    // Connect streams
    addStream (call_id);

    getMainBuffer()->stateInfo();

    // Start recording if set in preference
    if(audioPreference.getIsAlwaysRecording()) {
    	setRecordingCall(call_id);
    }

    // update call state on client side
    if (_dbus == NULL) {
    	_error("Manager: Error: DBUS was not initialized");
    	return false;
    }

    if(audioPreference.getIsAlwaysRecording()) {
        _dbus->getCallManager()->callStateChanged (call_id, "RECORD");
    }
    else {
    	_dbus->getCallManager()->callStateChanged(call_id, "CURRENT");
    }

    return true;
}

//THREAD=Main
bool ManagerImpl::hangupCall (const CallID& callId)
{
    bool returnValue = true;

    _info ("Manager: Hangup call %s", callId.c_str());


    // First stop audio layer if there is no call anymore
    int nbCalls = getCallList().size();
    if(nbCalls <= 0) {

    	audioLayerMutexLock();

        if(_audiodriver == NULL) {
        	audioLayerMutexUnlock();
        	_error("Manager: Error: Audio layer was not instantiated");
        	return returnValue;
        }

        _debug ("Manager: stop audio stream, there is no call remaining", nbCalls);
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }


    if (_dbus == NULL) {
    	_error("Manager: Error: Dbus layer have not been instantiated");
    	return false;
    }

    // store the current call id
    CallID currentCallId = getCurrentCallId();

    stopTone();

    /* Broadcast a signal over DBus */
    _debug ("Manager: Send DBUS call state change (HUNGUP) for id %s", callId.c_str());
    _dbus->getCallManager()->callStateChanged (callId, "HUNGUP");

    if (not isValidCall(callId) and not getConfigFromCall(callId) == Call::IPtoIP) {
    	_error("Manager: Error: Could not hang up call, call not valid");
        return false;
    }

    // Disconnect streams
    removeStream(callId);

    if (participToConference (callId)) {
    	Conference *conf = getConferenceFromCallID (callId);
        if (conf != NULL) {
            // remove this participant
            removeParticipant (callId);
            processRemainingParticipant (currentCallId, conf);
        }
    } else {
        // we are not participating to a conference, current call switched to ""
        if (!isConference (currentCallId)) {
            switchCall ("");
        }
    }

    if (getConfigFromCall (callId) == Call::IPtoIP) {
        /* Direct IP to IP call */
        returnValue = SIPVoIPLink::instance ()->hangup (callId);
    }
    else {
    	std::string accountId = getAccountFromCall (callId);
        returnValue = getAccountLink (accountId)->hangup (callId);
        removeCallAccount (callId);
    }

    getMainBuffer()->stateInfo();

    return returnValue;
}

bool ManagerImpl::hangupConference (const ConfID& id)
{
    _debug ("Manager: Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    std::string currentAccountId;

    if (iter_conf != _conferencemap.end()) {
        Conference *conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("Manager: Hangup conference participant %s", (*iter_participant).c_str());

            hangupCall (*iter_participant);

            iter_participant++;
        }
    }

    switchCall ("");

    getMainBuffer()->stateInfo();

    return true;
}

//THREAD=Main
bool ManagerImpl::cancelCall (const CallID& id)
{
    std::string accountid;
    bool returnValue;

    _debug ("Manager: Cancel call");

    stopTone();

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance ()->cancel (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == "") {
            _debug ("! Manager Cancel Call: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->cancel (id);

        removeCallAccount (id);
    }

    // it could be a waiting call?
    removeWaitingCall (id);

    removeStream(id);

    switchCall ("");

    getMainBuffer()->stateInfo();

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::onHoldCall (const CallID& callId)
{
    std::string account_id;
    bool returnValue = false;

    _debug ("Manager: Put call %s on hold", callId.c_str());

    stopTone();

    CallID current_call_id = getCurrentCallId();

    try {

    	if (getConfigFromCall (callId) == Call::IPtoIP) {
    		/* Direct IP to IP call */
    		returnValue = SIPVoIPLink::instance ()-> onhold (callId);
    	}
    	else {
    		/* Classic call, attached to an account */
    		account_id = getAccountFromCall (callId);

    		if (account_id == "") {
    			_debug ("Manager: Account ID %s or callid %s doesn't exists in call onHold", account_id.c_str(), callId.c_str());
    			return false;
    		}
    		returnValue = getAccountLink (account_id)->onhold (callId);
    	}
    }
    catch (VoipLinkException &e){
    	_error("Manager: Error: %s", e.what());
    }

    // Unbind calls in main buffer
    removeStream(callId);

    // Remove call from teh queue if it was still there
    removeWaitingCall (callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId) {
        switchCall ("");
    }

    if (_dbus == NULL) {
    	_error("Manager: Error: DBUS not initialized");
    	return false;
    }

    _dbus->getCallManager()->callStateChanged (callId, "HOLD");

    getMainBuffer()->stateInfo();

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::offHoldCall (const CallID& callId)
{

    std::string accountId;
    bool returnValue, isRec;
    std::string codecName;

    isRec = false;

    _debug ("Manager: Put call %s off hold", callId.c_str());

    stopTone();

    CallID currentCallId = getCurrentCallId();

    //Place current call on hold if it isn't

    if (hasCurrentCall()) {

        // if this is not a conference and this and is not a conference participant
        if (!isConference (currentCallId) && !participToConference (currentCallId)) {
        	_debug ("Manager: Has current call (%s), put on hold", currentCallId.c_str());
            onHoldCall (currentCallId);
        } else if (isConference (currentCallId) && !participToConference (callId)) {
            detachParticipant (default_id, currentCallId);
        }
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (callId) == Call::IPtoIP) {
        // is_rec = SIPVoIPLink::instance ()-> isRecording (call_id);
        returnValue = SIPVoIPLink::instance ()-> offhold (callId);
    }
    /* Classic call, attached to an account */
    else {
        accountId = getAccountFromCall (callId);

        _debug ("Manager: Setting offhold, Account %s, callid %s", accountId.c_str(), callId.c_str());

        isRec = getAccountLink (accountId)->getCall (callId)->isRecording();
        returnValue = getAccountLink (accountId)->offhold (callId);
    }

    if (_dbus == NULL) {
     	_error("Manager: Error: DBUS not initialized");   
    }

    if (isRec) {
    	_dbus->getCallManager()->callStateChanged (callId, "UNHOLD_RECORD");
    }
    else {
        _dbus->getCallManager()->callStateChanged (callId, "UNHOLD_CURRENT");
    }

    if (participToConference (callId)) {
        std::string currentAccountId;
        Call* call = NULL;

        currentAccountId = getAccountFromCall (callId);
        call = getAccountLink (currentAccountId)->getCall (callId);

        switchCall (call->getConfId());

    } else {
        switchCall (callId);
    }

    addStream(callId);

    getMainBuffer()->stateInfo();

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::transferCall (const CallID& callId, const std::string& to)
{
    bool returnValue = false;;

    _info ("Manager: Transfer call %s", callId.c_str());

    CallID currentCallId = getCurrentCallId();

    if(participToConference(callId)) {
	Conference *conf = getConferenceFromCallID(callId);
	if(conf == NULL) {
	    _error("Manager: Error: Could not find conference from call id");
        }

	removeParticipant (callId);
	processRemainingParticipant (callId, conf);
    }
    else {
	if(!isConference(currentCallId)) {
	    switchCall("");
	}
    }

    // Direct IP to IP call
    if (getConfigFromCall (callId) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance ()-> transfer (callId, to);
    }
    // Classic call, attached to an account
    else {

        std::string accountid = getAccountFromCall (callId);

        if (accountid == "") {
            _warn ("Manager: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->transfer (callId, to);

    }

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall (callId);

    getMainBuffer()->stateInfo();

    return returnValue;
}

void ManagerImpl::transferFailed ()
{

    _debug ("Manager: Transfer failed");

    if (_dbus)
        _dbus->getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceded ()
{

    _debug ("Manager: Transfer succeded");

    if (_dbus)
        _dbus->getCallManager()->transferSucceded();

}

bool ManagerImpl::attendedTransfer(const CallID& transferID, const CallID& targetID)
{
    bool returnValue = false;

    _debug("Manager: Attended transfer");

    // Direct IP to IP call
    if (getConfigFromCall (transferID) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance ()-> attendedTransfer(transferID, targetID);
    }
    else {	// Classic call, attached to an account

        std::string accountid = getAccountFromCall (transferID);

        if (accountid == "") {
            _warn ("Manager: Call doesn't exists");
	    return false;
        }

        returnValue = getAccountLink (accountid)->attendedTransfer (transferID, targetID);

    }

    getMainBuffer()->stateInfo();

    return returnValue;
}

//THREAD=Main : Call:Incoming
bool ManagerImpl::refuseCall (const CallID& id)
{
    std::string accountid;
    bool returnValue;

    _debug ("Manager: Refuse call %s", id.c_str());

    CallID current_call_id = getCurrentCallId();

    stopTone();

    int nbCalls = getCallList().size();

    if (nbCalls <= 1) {
        _debug ("    refuseCall: stop audio stream, there is only %d call(s) remaining", nbCalls);

        audioLayerMutexLock();
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance ()-> refuse (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == "") {
            _warn ("Manager: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->refuse (id);

        removeCallAccount (id);
    }

    // if the call was outgoing or established, we didn't refuse it
    // so the method did nothing
    if (returnValue) {
        removeWaitingCall (id);

        if (_dbus)
            _dbus->getCallManager()->callStateChanged (id, "HUNGUP");
    }

    // Disconnect streams
    removeStream(id);

    getMainBuffer()->stateInfo();

    return returnValue;
}

Conference*
ManagerImpl::createConference (const CallID& id1, const CallID& id2)
{
    _debug ("Manager: Create conference with call %s and %s", id1.c_str(), id2.c_str());

    Conference* conf = new Conference();

    conf->add (id1);
    conf->add (id2);

    // Add conference to map
    _conferencemap.insert (std::pair<CallID, Conference*> (conf->getConfID(), conf));

    // broadcast a signal over dbus
    if (_dbus) {
        _dbus->getCallManager()->conferenceCreated (conf->getConfID());
    }

    return conf;
}

void ManagerImpl::removeConference (const ConfID& conference_id)
{

    _debug ("Manager: Remove conference %s", conference_id.c_str());

    Conference* conf = NULL;

    _debug ("Manager: number of participant: %d", (int) _conferencemap.size());
    ConferenceMap::iterator iter = _conferencemap.find (conference_id);


    if (iter != _conferencemap.end()) {
        conf = iter->second;
    }

    if (conf == NULL) {
        _error ("Manager: Error: Conference not found");
        return;
    }

    // broadcast a signal over dbus
    if (_dbus) {
        _dbus->getCallManager()->conferenceRemoved (conference_id);
    }

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getMainBuffer()->unBindAll (default_id);

    ParticipantSet participants = conf->getParticipantList();

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end()) {

        getMainBuffer()->bindCallID (*iter_p, default_id);
    }

    // Then remove the conference from the conference map
    if (_conferencemap.erase (conference_id) == 1) {
        _debug ("Manager: Conference %s removed successfully", conference_id.c_str());
    }
    else {
        _error ("Manager: Error: Cannot remove conference: %s", conference_id.c_str());
    }

    delete conf;
}

Conference*
ManagerImpl::getConferenceFromCallID (const CallID& call_id)
{
    std::string account_id;
    Call* call = NULL;

    account_id = getAccountFromCall (call_id);
    call = getAccountLink (account_id)->getCall (call_id);

    ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

    if (iter != _conferencemap.end()) {
        return iter->second;
    } else {
        return NULL;
    }
}

void ManagerImpl::holdConference (const CallID& id)
{
    _debug ("Manager: Hold conference()");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);
    bool isRec = false;

    std::string currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        if(conf->getState() == Conference::ACTIVE_ATTACHED_REC) {
        	isRec = true;
        } else if (conf->getState() == Conference::ACTIVE_DETACHED_REC) {
        	isRec = true;
        } else if (conf->getState() == Conference::HOLD_REC) {
        	isRec = true;
        }

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("    holdConference: participant %s", (*iter_participant).c_str());
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);

            switchCall (*iter_participant);
            onHoldCall (*iter_participant);

            iter_participant++;

        }

        if(isRec) {
        	conf->setState(Conference::HOLD_REC);
        }
        else {
            conf->setState (Conference::HOLD);
        }

        if (_dbus) {
            _dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
        }

    }

}

void ManagerImpl::unHoldConference (const CallID& id)
{
    _debug ("Manager: Unhold conference()");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);
    bool isRec = false;

    std::string currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        if((conf->getState() == Conference::ACTIVE_ATTACHED_REC) ||
           (conf->getState() == Conference::ACTIVE_DETACHED_REC) ||
           (conf->getState() == Conference::HOLD_REC)) {
        	isRec = true;
        }

        while (iter_participant != participants.end()) {
            _debug ("    unholdConference: participant %s", (*iter_participant).c_str());
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);

            // if one call is currently recording, the conference is in state recording
            if(call->isRecording()) {
            	isRec = true;
            }

            offHoldCall (*iter_participant);

            iter_participant++;

        }

        if(isRec) {
            conf->setState (Conference::ACTIVE_ATTACHED_REC);
        }
        else {
        	conf->setState (Conference::ACTIVE_ATTACHED);
        }

        if (_dbus) {
            _dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
        }
    }

}

bool ManagerImpl::isConference (const CallID& id)
{
    ConferenceMap::iterator iter = _conferencemap.find (id);

    if (iter == _conferencemap.end()) {
        return false;
    } else {
        return true;
    }
}

bool ManagerImpl::participToConference (const CallID& call_id)
{
    std::string accountId = getAccountFromCall (call_id);
    Call *call = getAccountLink (accountId)->getCall (call_id);

    if (call == NULL) {
    	_error("Manager: Error call is NULL in particip to conference");
        return false;
    }

    if (call->getConfId() == "") {
        return false;
    }

    return true;
}

void ManagerImpl::addParticipant (const CallID& callId, const CallID& conferenceId)
{
    _debug ("Manager: Add participant %s to %s", callId.c_str(), conferenceId.c_str());

    ConferenceMap::iterator iter = _conferencemap.find (conferenceId);
    if (iter == _conferencemap.end()) {
    	_error("Manager: Error: Conference id is not valid");
    	return;
    }

    std::string currentAccountId = getAccountFromCall (callId);
    Call *call = getAccountLink (currentAccountId)->getCall (callId);
    if(call == NULL) {
    	_error("Manager: Error: Call id is not valid");
    	return;
    }

    // store the current call id (it will change in offHoldCall or in answerCall)
    CallID current_call_id = getCurrentCallId();

    // detach from prior communication and switch to this conference
    if (current_call_id != callId) {
        if (isConference (current_call_id)) {
            detachParticipant (default_id, current_call_id);
        } else {
            onHoldCall (current_call_id);
        }
    }
    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    switchCall ("");

    // Add main participant
    addMainParticipant (conferenceId);

    Conference* conf = iter->second;
    switchCall (conf->getConfID());

    // Add coresponding IDs in conf and call
    call->setConfId (conf->getConfID());
    conf->add (callId);

    // Connect new audio streams together
    getMainBuffer()->unBindAll (callId);

    std::map<std::string, std::string> callDetails = getCallDetails (callId);
    std::string callState = callDetails.find("CALL_STATE")->second;
    if (callState == "HOLD") {
    	conf->bindParticipant (callId);
    	offHoldCall (callId);
    } else if (callState == "INCOMING") {
    	conf->bindParticipant (callId);
    	answerCall (callId);
    } else if (callState == "CURRENT") {
    	conf->bindParticipant (callId);
    }

    ParticipantSet participants = conf->getParticipantList();
    if(participants.empty()) {
    	_error("Manager: Error: Participant list is empty for this conference");
    }

    // reset ring buffer for all conference participant
    ParticipantSet::iterator iter_p = participants.begin();
    while (iter_p != participants.end()) {
    	// flush conference participants only
    	getMainBuffer()->flush (*iter_p);
    	iter_p++;
    }

    getMainBuffer()->flush (default_id);

    // Connect stream
    addStream(callId);
}

void ManagerImpl::addMainParticipant (const CallID& conference_id)
{
    if (hasCurrentCall()) {
        CallID current_call_id = getCurrentCallId();

        if (isConference (current_call_id)) {
            detachParticipant (default_id, current_call_id);
        } else {
            onHoldCall (current_call_id);
        }
    }

    ConferenceMap::iterator iter = _conferencemap.find (conference_id);

    Conference *conf = NULL;

    audioLayerMutexLock();

    _debug("Manager: Add Main Participant");

    if (iter != _conferencemap.end()) {
        conf = iter->second;

        ParticipantSet participants = conf->getParticipantList();

        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            getMainBuffer()->bindCallID (*iter_participant, default_id);
            iter_participant++;
        }

        // Reset ringbuffer's readpointers
        iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            getMainBuffer()->flush (*iter_participant);
            iter_participant++;
        }

        getMainBuffer()->flush (default_id);

        if(conf->getState() == Conference::ACTIVE_DETACHED) {
            conf->setState (Conference::ACTIVE_ATTACHED);
        }
        else if(conf->getState() == Conference::ACTIVE_DETACHED_REC) {
        	conf->setState(Conference::ACTIVE_ATTACHED_REC);
        }
        else {
        	_warn("Manager: Warning: Invalid conference state while adding main participant");
        }

        if (_dbus)
            _dbus->getCallManager()->conferenceChanged (conference_id, conf->getStateStr());

    }

    audioLayerMutexUnlock();

    switchCall (conference_id);
}

void ManagerImpl::joinParticipant (const CallID& callId1, const CallID& callId2)
{
	bool isRec = false;

    _debug ("Manager: Join participants %s, %s", callId1.c_str(), callId2.c_str());

    std::map<std::string, std::string> call1Details = getCallDetails (callId1);
    std::map<std::string, std::string> call2Details = getCallDetails (callId2);

    CallID current_call_id = getCurrentCallId();
    _debug ("Manager: Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ( (current_call_id != callId1) && (current_call_id != callId2)) {

        if (isConference (current_call_id)) {
        	// If currently in a conference
            detachParticipant (default_id, current_call_id);
        }
        else {
            // If currently in a call
            onHoldCall (current_call_id);
        }
    }

    Conference *conf = createConference (callId1, callId2);

    // Set corresponding conference ids for call 1
    std::string currentAccountId1 = getAccountFromCall (callId1);
    Call *call1 = getAccountLink (currentAccountId1)->getCall (callId1);
    if(call1 == NULL) {
    	_error("Manager: Could not find call %s", callId1.c_str());
    }
    call1->setConfId (conf->getConfID());
    getMainBuffer()->unBindAll(callId1);

    // Set corresponding conderence details
    std::string currentAccountId2 = getAccountFromCall (callId2);
    Call *call2 = getAccountLink (currentAccountId2)->getCall (callId2);
    if(call2 == NULL) {
    	_error("Manager: Could not find call %s", callId2.c_str());
    }
    call2->setConfId (conf->getConfID());
    getMainBuffer()->unBindAll(callId2);


    // Process call1 according to its state
    std::string call1_state_str = call1Details.find ("CALL_STATE")->second;
    _debug ("Manager: Process call %s state: %s", callId1.c_str(), call1_state_str.c_str());

    if (call1_state_str == "HOLD") {
    	conf->bindParticipant (callId1);
        offHoldCall (callId1);
    } else if (call1_state_str == "INCOMING") {
    	conf->bindParticipant (callId1);
        answerCall (callId1);
    } else if (call1_state_str == "CURRENT") {
        conf->bindParticipant (callId1);
    } else if (call1_state_str == "RECORD") {
    	conf->bindParticipant(callId1);
    	isRec = true;
    } else if (call1_state_str == "INACTIVE") {
        conf->bindParticipant (callId1);
        answerCall (callId1);
    } else {
        _warn ("Manager: Call state not recognized");
    }

    // Process call2 according to its state
    std::string call2_state_str = call2Details.find ("CALL_STATE")->second;
    _debug ("Manager: Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

    if (call2_state_str == "HOLD") {
    	conf->bindParticipant (callId2);
        offHoldCall (callId2);
    } else if (call2_state_str == "INCOMING") {
    	conf->bindParticipant (callId2);
        answerCall (callId2);
    } else if (call2_state_str == "CURRENT") {
        conf->bindParticipant (callId2);
    } else if (call2_state_str == "RECORD") {
    	conf->bindParticipant (callId2);
    	isRec = true;
    } else if (call2_state_str == "INACTIVE") {
    	conf->bindParticipant (callId2);
        answerCall (callId2);
    } else {
        _warn ("Manager: Call state not recognized");
    }

    // Switch current call id to this conference
    switchCall (conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    audioLayerMutexLock();
    if (_audiodriver) {
    	conf->setRecordingSmplRate(_audiodriver->getSampleRate());
    }
    audioLayerMutexUnlock();

    getMainBuffer()->stateInfo();
}

void ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    bool callSuccess;
    int successCounter = 0;

    _debug("Manager: Create conference from participant list");

    // we must at least have 2 participant for a conference
    if(participantList.size() <= 1) {
        _error("Manager: Error: Participant number must be higher or equal to 2");
	return;
    }

    Conference *conf = new Conference();

    for(unsigned int i = 0; i < participantList.size(); i++) {
	std::string numberaccount = participantList[i];
	std::string tostr = numberaccount.substr(0, numberaccount.find(","));
        std::string account = numberaccount.substr(numberaccount.find(",")+1, numberaccount.size());
		
	std::string generatedCallID = getNewCallID();

	// Manager methods may behave differently if the call id particip to a conference
	conf->add(generatedCallID);

	switchCall("");

	// Create call
	callSuccess = outgoingCall(account, generatedCallID, tostr, conf->getConfID());

        // If not able to create call remove this participant from the conference
	if(!callSuccess)
	    conf->remove(generatedCallID);

	if(_dbus && callSuccess) {
	    _dbus->getCallManager()->newCallCreated(account, generatedCallID, tostr);
	    successCounter++;
	}	
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if(successCounter >= 2 ) {
        _conferencemap.insert(std::pair<CallID, Conference *> (conf->getConfID(), conf));

        if (_dbus) {
            _dbus->getCallManager()->conferenceCreated (conf->getConfID());
        }

	audioLayerMutexLock();
	if(_audiodriver) {
	    conf->setRecordingSmplRate(_audiodriver->getSampleRate());
        }
	audioLayerMutexUnlock();

	getMainBuffer()->stateInfo();
    }
    else {
	delete conf;
	conf = NULL;
    }
    
}

void ManagerImpl::detachParticipant (const CallID& call_id,
                                     const CallID& current_id)
{

    _debug ("Manager: Detach participant %s (current id: %s)", call_id.c_str(), current_id.c_str());


    CallID current_call_id = getCurrentCallId();

    if (call_id != default_id) {

        std::string currentAccountId = getAccountFromCall (call_id);
        Call *call = getAccountLink (currentAccountId)->getCall (call_id);

        if(call == NULL) {
        	_error("Manager: Error: Could not find call %s", call_id.c_str());
        	return;
        }

        // TODO: add conference_id as a second parameter
        ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

        Conference *conf = getConferenceFromCallID (call_id);
        if (conf == NULL) {
            _error ("Manager: Error: Call is not conferencing, cannot detach");
            return;
        }

        std::map<std::string, std::string> call_details = getCallDetails (call_id);
        std::map<std::string, std::string>::iterator iter_details;

        iter_details = call_details.find ("CALL_STATE");
        if(iter_details == call_details.end()) {
        	_error ("Manager: Error: Could not find CALL_STATE");
        	return;
        }

        if (iter_details->second == "RINGING") {
        	removeParticipant (call_id);
        } else {
        	onHoldCall (call_id);
        	removeParticipant (call_id);
        	processRemainingParticipant (current_call_id, conf);
        }
    } else {

        _debug ("Manager: Unbind main participant from conference %d");
        getMainBuffer()->unBindAll (default_id);

        if(!isConference(current_call_id)) {
        	_error("Manager: Warning: Current call id (%s) is not a conference", current_call_id.c_str());
        	return;
        }

        ConferenceMap::iterator iter = _conferencemap.find (current_call_id);
        Conference *conf = iter->second;

        if(conf == NULL) {
        	_debug("Manager: Error: Conference is NULL");
        	return;
        }

        if(conf->getState() == Conference::ACTIVE_ATTACHED) {
        	conf->setState(Conference::ACTIVE_DETACHED);
        }
        else if(conf->getState() == Conference::ACTIVE_ATTACHED_REC) {
        	conf->setState(Conference::ACTIVE_DETACHED_REC);
        }
        else {
        	_warn("Manager: Warning: Undefined behavior, invalid conference state in detach participant");
        }

        if (_dbus) {
        	_dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
        }

        switchCall ("");

    }
}

void ManagerImpl::removeParticipant (const CallID& call_id)
{
    _debug ("Manager: Remove participant %s", call_id.c_str());

    // TODO: add conference_id as a second parameter
    Conference* conf;

    std::string currentAccountId;
    Call* call = NULL;

    // this call is no more a conference participant
    currentAccountId = getAccountFromCall (call_id);
    call = getAccountLink (currentAccountId)->getCall (call_id);

    ConferenceMap conf_map = _conferencemap;
    ConferenceMap::iterator iter = conf_map.find (call->getConfId());

    if (iter == conf_map.end()) {
        _error ("Manager: Error: No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    conf = iter->second;

    _debug ("Manager: Remove participant %s", call_id.c_str());
    conf->remove (call_id);
    call->setConfId ("");

    removeStream(call_id);

    getMainBuffer()->stateInfo();
}

void ManagerImpl::processRemainingParticipant (CallID current_call_id, Conference *conf)
{

    _debug ("Manager: Process remaining %d participant(s) from conference %s",
            conf->getNbParticipants(), conf->getConfID().c_str());

    if (conf->getNbParticipants() > 1) {

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        // Reset ringbuffer's readpointers
        iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            getMainBuffer()->flush (*iter_participant);

            iter_participant++;
        }

        getMainBuffer()->flush (default_id);

    } else if (conf->getNbParticipants() == 1) {

        _debug ("Manager: Only one remaining participant");

        std::string currentAccountId;
        Call* call = NULL;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        // bind main participant to remaining conference call
        if (iter_participant != participants.end()) {

            // this call is no more a conference participant
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);
            call->setConfId ("");

            // if we are not listening to this conference

            if (current_call_id != conf->getConfID()) {
                onHoldCall (call->getCallId());
            } else {
                switchCall (*iter_participant);
            }
        }

        removeConference (conf->getConfID());

    } else {

        _debug ("Manager: No remaining participant, remove conference");

        removeConference (conf->getConfID());

        switchCall ("");
    }

}

void ManagerImpl::joinConference (const CallID& conf_id1,
                                  const CallID& conf_id2)
{
    _debug ("Manager: Join conference %s, %s", conf_id1.c_str(), conf_id2.c_str());

    ConferenceMap::iterator iter;

    Conference *conf1 = NULL;
    Conference *conf2 = NULL;

    iter = _conferencemap.find (conf_id1);

    if (iter != _conferencemap.end()) {
        conf1 = iter->second;
    } else {
        _error ("Manager: Error: Not a valid conference ID");
        return;
    }

    iter = _conferencemap.find (conf_id2);

    if (iter != _conferencemap.end()) {
        conf2 = iter->second;
    } else {
        _error ("Manager: Error: Not a valid conference ID");
        return;
    }

    ParticipantSet participants = conf1->getParticipantList();

    ParticipantSet::iterator iter_participant = participants.begin();

    while (iter_participant != participants.end()) {
        detachParticipant (*iter_participant, "");
        addParticipant (*iter_participant, conf_id2);

        iter_participant++;
    }

}

void ManagerImpl::addStream (const CallID& call_id)
{

    _debug ("Manager: Add audio stream %s", call_id.c_str());

    std::string currentAccountId;
    Call* call = NULL;

    currentAccountId = getAccountFromCall (call_id);
    call = getAccountLink (currentAccountId)->getCall (call_id);

    if (participToConference (call_id)) {

        _debug ("Manager: Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

        if (iter != _conferencemap.end()) {
            Conference* conf = iter->second;

            conf->bindParticipant (call_id);

            ParticipantSet participants = conf->getParticipantList();

            // reset ring buffer for all conference participant
            ParticipantSet::iterator iter_p = participants.begin();

            while (iter_p != participants.end()) {

                // to avoid puting onhold the call
                // switchCall("");
                getMainBuffer()->flush (*iter_p);

                iter_p++;
            }

            getMainBuffer()->flush (default_id);
        }

    } else {
        _debug ("Manager: Add stream to call");

        // bind to main
        getMainBuffer()->bindCallID (call_id);

        audioLayerMutexLock();
        _audiodriver->flushUrgent();
        _audiodriver->flushMain();
        audioLayerMutexUnlock();
    }

    getMainBuffer()->stateInfo();
}

void ManagerImpl::removeStream (const CallID& call_id)
{
    _debug ("Manager: Remove audio stream %s", call_id.c_str());

    getMainBuffer()->unBindAll (call_id);

    getMainBuffer()->stateInfo();
}

//THREAD=Main
bool ManagerImpl::saveConfig (void)
{
    _debug ("Manager: Saving Configuration to XDG directory %s", _path.c_str());
    audioPreference.setVolumemic (getMicVolume());
    audioPreference.setVolumespkr (getSpkrVolume());

    AccountMap::iterator iter = _accountMap.begin();

    try {
        // emitter = new Conf::YamlEmitter("sequenceEmitter.yml");
        emitter = new Conf::YamlEmitter (_path.c_str());

        while (iter != _accountMap.end()) {

        	// Skip the "" account ID (which refer to the IP2IP account)
            if (iter->first == "") {
                iter++;
                continue;
            }

            iter->second->serialize (emitter);
            iter++;
        }

        preferences.serialize (emitter);
        voipPreferences.serialize (emitter);
        addressbookPreference.serialize (emitter);
        hookPreference.serialize (emitter);
        audioPreference.serialize (emitter);
        shortcutPreferences.serialize (emitter);

        emitter->serializeData();

        delete emitter;
    } catch (Conf::YamlEmitterException &e) {
        _error ("ConfigTree: %s", e.what());
    }

    return _setupLoaded;
}

//THREAD=Main
bool ManagerImpl::sendDtmf (const CallID& id, char code)
{
    _debug ("Manager: Send DTMF for call %s", id.c_str());

    std::string accountid = getAccountFromCall (id);

    playDtmf (code);

    CallAccountMap::iterator iter = _callAccountMap.find (id);

    bool returnValue = getAccountLink (accountid)->carryingDTMFdigits (id, code);

    return returnValue;
}

//THREAD=Main | VoIPLink
bool ManagerImpl::playDtmf (char code)
{
    int pulselen, layerType, size;
    bool ret = false;
    SFLDataFormat *buf;

    stopTone();

    bool hasToPlayTone = voipPreferences.getPlayDtmf();

    if (!hasToPlayTone) {
        _debug ("Manager: playDtmf: Do not have to play a tone...");
        return false;
    }

    // length in milliseconds
    pulselen = voipPreferences.getPulseLength();

    if (!pulselen) {
        _debug ("Manager: playDtmf: Pulse length is not set...");
        return false;
    }

    audioLayerMutexLock();

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

    layerType = _audiodriver->getLayerType();

    // fast return, no sound, so no dtmf
    if (_audiodriver == NULL || _dtmfKey == NULL) {
        _debug ("Manager: playDtmf: Error no audio layer...");
        audioLayerMutexUnlock();
        return false;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    size = (int) ( (pulselen * (float) _audiodriver->getSampleRate()) / 1000);

    // this buffer is for mono
    // TODO <-- this should be global and hide if same size
    buf = new SFLDataFormat[size];

    // Handle dtmf
    _dtmfKey->startTone (code);

    // copy the sound
    if (_dtmfKey->generateDTMF (buf, size)) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();
        _audiodriver->startStream();
        _audiodriver->putUrgent (buf, size * sizeof (SFLDataFormat));
    }

    audioLayerMutexUnlock();

    ret = true;

    // TODO Cache the DTMF

    delete[] buf;
    buf = 0;

    return ret;
}

// Multi-thread
bool ManagerImpl::incomingCallWaiting ()
{
    return (_nbIncomingWaitingCall > 0) ? true : false;
}

void ManagerImpl::addWaitingCall (const CallID& id)
{

    _info ("Manager: Add waiting call %s (%d calls)", id.c_str(), _nbIncomingWaitingCall);

    ost::MutexLock m (_waitingCallMutex);
    _waitingCall.insert (id);
    _nbIncomingWaitingCall++;
}

void ManagerImpl::removeWaitingCall (const CallID& id)
{

    _info ("Manager: Remove waiting call %s (%d calls)", id.c_str(), _nbIncomingWaitingCall);

    ost::MutexLock m (_waitingCallMutex);
    // should return more than 1 if it erase a call

    if (_waitingCall.erase (id)) {
        _nbIncomingWaitingCall--;
    }
}

bool ManagerImpl::isWaitingCall (const CallID& id)
{
    CallIDSet::iterator iter = _waitingCall.find (id);

    if (iter != _waitingCall.end()) {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
bool ManagerImpl::incomingCall (Call* call, const std::string& accountId)
{

    std::string from, number, display_name, display;

    if (!call)
        _error ("Manager: Error: no call at this point");

    stopTone();

    _debug ("Manager: Incoming call %s for account %s", call->getCallId().data(), accountId.c_str());

    associateCallToAccount (call->getCallId(), accountId);

    // If account is null it is an ip to ip call
    if (accountId == "") {
        associateConfigToCall (call->getCallId(), Call::IPtoIP);
    } else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber = call->getPeerNumber();

        int startIndex = peerNumber.find ("sip:");

        if (startIndex != (int) std::string::npos) {
            std::string strippedPeerNumber = peerNumber.substr (startIndex + 4);
            call->setPeerNumber (strippedPeerNumber);
        }

    }

    if (!hasCurrentCall()) {
        _debug ("Manager: Has no current call start ringing");

        call->setConnectionState (Call::Ringing);
        ringtone (accountId);

    } else {
        _debug ("Manager: has current call, beep in current audio stream");
    }

    addWaitingCall (call->getCallId());

    from = call->getPeerName();
    number = call->getPeerNumber();
    display_name = call->getDisplayName();

    if (from != "" && number != "") {
        from.append (" <");
        from.append (number);
        from.append (">");
    } else if (from.empty()) {
        from.append ("<");
        from.append (number);
        from.append (">");
    }

    /* Broadcast a signal over DBus */
    _debug ("Manager: From: %s, Number: %s, Display Name: %s", from.c_str(), number.c_str(), display_name.c_str());

    display = display_name;
    display.append (" ");
    display.append (from);

    if (_dbus) {
        _dbus->getCallManager()->incomingCall (accountId, call->getCallId(), display.c_str());
    }

    return true;
}


//THREAD=VoIP
void ManagerImpl::incomingMessage (const CallID& callID,
                                   const std::string& from,
                                   const std::string& message)
{

    if (participToConference (callID)) {
        _debug ("Manager: Particip to a conference, send message to everyone");

        Conference *conf = getConferenceFromCallID (callID);

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {

            if (*iter_participant == callID)
                continue;

            std::string accountId = getAccountFromCall (*iter_participant);

            _debug ("Manager: Send message to %s, (%s)", (*iter_participant).c_str(), accountId.c_str());

            Account *account = getAccount (accountId);

            if (!account) {
                _debug ("Manager: Failed to get account while sending instant message");
                return;
            }

            if (account->getType() == "SIP")
                // link = dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId));
                dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId))->sendTextMessage (_imModule, callID, message, from);
            else if (account->getType() == "IAX")
                // link = dynamic_cast<IAXVoIPLink *> (account->getVoIPLink());
                dynamic_cast<IAXVoIPLink *> (account->getVoIPLink())->sendTextMessage (_imModule, callID, message, from);
            else {
                _debug ("Manager: Failed to get voip link while sending instant message");
                return;
            }

            iter_participant++;
        }

        // in case of a conference we must notify client using conference id
        if (_dbus) {
            _dbus->getCallManager()->incomingMessage (conf->getConfID(), from, message);
        }

    } else {

        if (_dbus) {
            _dbus->getCallManager()->incomingMessage (callID, from, message);
        }
    }
}


//THREAD=VoIP
bool ManagerImpl::sendTextMessage (const CallID& callID, const std::string& message, const std::string& from)
{

    if (isConference (callID)) {
        _debug ("Manager: Is a conference, send instant message to everyone");

        ConferenceMap::iterator it = _conferencemap.find (callID);

        if (it == _conferencemap.end())
            return false;

        Conference *conf = it->second;

        if (!conf)
            return false;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {

            std::string accountId = getAccountFromCall (*iter_participant);

            Account *account = getAccount (accountId);

            if (!account) {
                _debug ("Manager: Failed to get account while sending instant message");
                return false;
            }

            if (account->getType() == "SIP")
                // link = dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId));
                dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId))->sendTextMessage (_imModule, *iter_participant, message, from);
            else if (account->getType() == "IAX")
                // link = dynamic_cast<IAXVoIPLink *> (account->getVoIPLink());
                dynamic_cast<IAXVoIPLink *> (account->getVoIPLink())->sendTextMessage (_imModule, *iter_participant, message, from);
            else {
                _debug ("Manager: Failed to get voip link while sending instant message");
                return false;
            }

            iter_participant++;
        }

        return true;
    }

    if (participToConference (callID)) {
        _debug ("Manager: Particip to a conference, send instant message to everyone");

        Conference *conf = getConferenceFromCallID (callID);

        if (!conf)
            return false;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {

            std::string accountId = getAccountFromCall (*iter_participant);

            Account *account = getAccount (accountId);

            if (!account) {
                _debug ("Manager: Failed to get account while sending instant message");
                return false;
            }

            if (account->getType() == "SIP")
                // link = dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId));
                dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId))->sendTextMessage (_imModule, *iter_participant, message, from);
            else if (account->getType() == "IAX")
                // link = dynamic_cast<IAXVoIPLink *> (account->getVoIPLink());
                dynamic_cast<IAXVoIPLink *> (account->getVoIPLink())->sendTextMessage (_imModule, *iter_participant, message, from);
            else {
                _debug ("Manager: Failed to get voip link while sending instant message");
                return false;
            }

            iter_participant++;
        }

    } else {

        std::string accountId = getAccountFromCall (callID);

        Account *account = getAccount (accountId);

        if (!account) {
            _debug ("Manager: Failed to get account while sending instant message");
            return false;
        }

        if (account->getType() == "SIP")
            // link = dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId));
            dynamic_cast<SIPVoIPLink *> (getAccountLink (accountId))->sendTextMessage (_imModule, callID, message, from);
        else if (account->getType() == "IAX")
            // link = dynamic_cast<IAXVoIPLink *> (account->getVoIPLink());
            dynamic_cast<IAXVoIPLink *> (account->getVoIPLink())->sendTextMessage (_imModule, callID, message, from);
        else {
            _debug ("Manager: Failed to get voip link while sending instant message");
            return false;
        }
    }

    return true;
}

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall (const CallID& id)
{

    _debug ("Manager: Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall (id)) {
        stopTone();
    }


    // Connect audio streams
    addStream(id);

    audioLayerMutexLock();
    _audiodriver->flushMain();
    _audiodriver->flushUrgent();
    audioLayerMutexUnlock();

    if(audioPreference.getIsAlwaysRecording()) {
    	setRecordingCall(id);
    }

    if(_dbus == NULL) {
    	_error("Manager: Error: DBUS not initialized");
    	return;
    }

    if(audioPreference.getIsAlwaysRecording()) {
    	_dbus->getCallManager()->callStateChanged (id, "RECORD");
    }
    else {
    	_dbus->getCallManager()->callStateChanged(id, "CURRENT");
    }
}

//THREAD=VoIP Call=Outgoing
void ManagerImpl::peerRingingCall (const CallID& id)
{

    _debug ("Manager: Peer call %s ringing", id.c_str());

    if (isCurrentCall (id)) {
        ringback();
    }

    if (_dbus == NULL) {
    	_error("Manager: Error: DBUS not initialized");
    }

    _dbus->getCallManager()->callStateChanged (id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall (const CallID& call_id)
{
    std::string account_id;
    bool returnValue;

    _debug ("Manager: Peer hungup call %s", call_id.c_str());

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    if (participToConference (call_id)) {

        Conference *conf = getConferenceFromCallID (call_id);

        if (conf != NULL) {
            removeParticipant (call_id);
            processRemainingParticipant (current_call_id, conf);
        }
    } else {
        if (isCurrentCall (call_id)) {
            stopTone();
            switchCall("");
        }
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        SIPVoIPLink::instance ()->hangup (call_id);
    }
    else {
        account_id = getAccountFromCall (call_id);
        returnValue = getAccountLink (account_id)->peerHungup (call_id);
    }

    /* Broadcast a signal over DBus */
    if (_dbus) {
        _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");
    }

    removeWaitingCall (call_id);

    removeCallAccount (call_id);

    int nbCalls = getCallList().size();

    // stop streams

    if (nbCalls <= 0) {
        _debug ("Manager: Stop audio stream, ther is only %d call(s) remaining", nbCalls);

        audioLayerMutexLock();
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }
}

//THREAD=VoIP
void ManagerImpl::callBusy (const CallID& id)
{
    _debug ("Manager: Call %s busy", id.c_str());

    if (_dbus) {
        _dbus->getCallManager()->callStateChanged (id, "BUSY");
    }

    if (isCurrentCall (id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    removeCallAccount (id);

    removeWaitingCall (id);
}

//THREAD=VoIP
void ManagerImpl::callFailure (const CallID& call_id)
{
    if (_dbus) {
        _dbus->getCallManager()->callStateChanged (call_id, "FAILURE");
    }

    if (isCurrentCall (call_id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    CallID current_call_id = getCurrentCallId();

    if (participToConference (call_id)) {

        _debug ("Manager: Call %s participating to a conference failed", call_id.c_str());

        Conference *conf = getConferenceFromCallID (call_id);

        if (conf == NULL) {
        	_error("Manager: Could not retreive conference from call id %s", call_id.c_str());
        	return;
        }

        // remove this participant
        removeParticipant (call_id);

        processRemainingParticipant (current_call_id, conf);

    }

    removeCallAccount (call_id);

    removeWaitingCall (call_id);

}

//THREAD=VoIP
void ManagerImpl::startVoiceMessageNotification (const std::string& accountId,
        int nb_msg)
{
    if (_dbus) {
        _dbus->getCallManager()->voiceMailNotify (accountId, nb_msg);
    }
}

void ManagerImpl::connectionStatusNotification ()
{

    _debug ("Manager: connectionStatusNotification");

    if (_dbus != NULL) {
        _dbus->getConfigurationManager()->accountsChanged();
    }
}

/**
 * Multi Thread
 */
bool ManagerImpl::playATone (Tone::TONEID toneId)
{

    bool hasToPlayTone;

    // _debug ("Manager: Play tone %d", toneId);

    hasToPlayTone = voipPreferences.getPlayTones();

    if (!hasToPlayTone) {
        return false;
    }

    audioLayerMutexLock();

    if (_audiodriver == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return false;
    }
    _audiodriver->flushUrgent();
    _audiodriver->startStream();
    audioLayerMutexUnlock();

    if (_telephoneTone != 0) {
        _toneMutex.enterMutex();
        _telephoneTone->setCurrentTone (toneId);
        _toneMutex.leaveMutex();
    }

    return true;
}

/**
 * Multi Thread
 */
void ManagerImpl::stopTone ()
{
    bool hasToPlayTone = voipPreferences.getPlayTones();

    if (hasToPlayTone == false) {
        return;
    }

    _toneMutex.enterMutex();

    if (_telephoneTone != NULL) {
        _telephoneTone->setCurrentTone (Tone::TONE_NULL);
    }

    if (_audiofile) {
	std::string filepath = _audiofile->getFilePath();
	_dbus->getCallManager()->recordPlaybackStoped(filepath);
        _audiofile->stop();
	delete _audiofile;
	_audiofile = NULL;
    }

    _toneMutex.leaveMutex();
}

/**
 * Multi Thread
 */
bool ManagerImpl::playTone ()
{
    playATone (Tone::TONE_DIALTONE);
    return true;
}

/**
 * Multi Thread
 */
bool ManagerImpl::playToneWithMessage ()
{
    playATone (Tone::TONE_CONGESTION);
    return true;
}

/**
 * Multi Thread
 */
void ManagerImpl::congestion ()
{
    playATone (Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringback ()
{
    playATone (Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringtone (const std::string& accountID)
{
    std::string ringchoice;
    sfl::AudioCodec *codecForTone;
    int samplerate;

    _debug ("Manager: Ringtone");

    Account *account = getAccount (accountID);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone");
        return;
    }

    if (account->getRingtoneEnabled()) {

        _debug ("Manager: Tone is enabled");
        //TODO Comment this because it makes the daemon crashes since the main thread
        //synchronizes the ringtone thread.

        ringchoice = account->getRingtonePath();
        //if there is no / inside the path

        if (ringchoice.find (DIR_SEPARATOR_CH) == std::string::npos) {
            // check inside global share directory
            ringchoice = std::string (PROGSHAREDIR) + DIR_SEPARATOR_STR
                         + RINGDIR + DIR_SEPARATOR_STR + ringchoice;
        }

        audioLayerMutexLock();


        if (!_audiodriver) {
            _error ("Manager: Error: no audio layer in ringtone");
            audioLayerMutexUnlock();
            return;
        }

        samplerate = _audiodriver->getSampleRate();
        codecForTone = static_cast<sfl::AudioCodec *>(_audioCodecFactory.getFirstCodecAvailable());

        audioLayerMutexUnlock();

        _toneMutex.enterMutex();

        if (_audiofile) {
	    if(_dbus) {
		std::string filepath = _audiofile->getFilePath();
		_dbus->getCallManager()->recordPlaybackStoped(filepath);
	    }
            delete _audiofile;
            _audiofile = NULL;
        }

        std::string wave (".wav");
        size_t found = ringchoice.find (wave);

        try {

            if (found != std::string::npos) {
                _audiofile = static_cast<AudioFile *> (new WaveFile());
            }
            else {
                _audiofile = static_cast<AudioFile *> (new RawFile());
            }

            _debug ("Manager: ringChoice: %s, codecForTone: %d, samplerate %d", ringchoice.c_str(), codecForTone->getPayloadType(), samplerate);

            _audiofile->loadFile (ringchoice, codecForTone, samplerate);
        }
        catch (AudioFileException &e) {
	    _error("Manager: Exception: %s", e.what());
        }
    
        _audiofile->start();
        _toneMutex.leaveMutex();

        audioLayerMutexLock();
        // start audio if not started AND flush all buffers (main and urgent)
        _audiodriver->startStream();
        audioLayerMutexUnlock();

    } else {
        ringback();
    }
}

AudioLoop*
ManagerImpl::getTelephoneTone ()
{
    if (_telephoneTone != NULL) {
        ost::MutexLock m (_toneMutex);
        return _telephoneTone->getCurrentTone();
    } else {
        return NULL;
    }
}

AudioLoop*
ManagerImpl::getTelephoneFile ()
{
    ost::MutexLock m (_toneMutex);

    if (!_audiofile) {
        return NULL;
    }

    if (_audiofile->isStarted()) {
        return _audiofile;
    } else {
        return NULL;
    }
}

void ManagerImpl::notificationIncomingCall (void)
{
    std::ostringstream frequency;
    unsigned int sampleRate, nbSample;

    audioLayerMutexLock();

    if(_audiodriver == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return;
    }

    _debug ("ManagerImpl: Notification incoming call");

    // Enable notification only if more than one call
    if (hasCurrentCall()) {
        sampleRate = _audiodriver->getSampleRate();
        frequency << "440/" << 160;
        Tone tone (frequency.str(), sampleRate);
        nbSample = tone.getSize();
        SFLDataFormat buf[nbSample];
        tone.getNext (buf, nbSample);
        /* Put the data in the urgent ring buffer */
        _audiodriver->flushUrgent();
        _audiodriver->putUrgent (buf, sizeof (SFLDataFormat) * nbSample);
    }

    audioLayerMutexUnlock();
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 * @return 1: ok
 -1: error directory
 */
int ManagerImpl::createSettingsPath (void)
{

    std::string xdg_config, xdg_env;

    _debug ("XDG_CONFIG_HOME: %s", XDG_CONFIG_HOME);

    xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".config"
                 + DIR_SEPARATOR_STR + PROGDIR;

    if (XDG_CONFIG_HOME != NULL) {
        xdg_env = std::string (XDG_CONFIG_HOME);
        (xdg_env.length() > 0) ? _path = xdg_env : _path = xdg_config;
    } else
        _path = xdg_config;

    if (mkdir (_path.data(), 0700) != 0) {
        // If directory	creation failed
        if (errno != EEXIST) {
            _debug ("Cannot create directory: %s", strerror (errno));
            return -1;
        }
    }

    // Load user's configuration
    _path = _path + DIR_SEPARATOR_STR + PROGNAME + ".yml";

    return 1;
}

/**
 * Initialization: Main Thread
 */
void ManagerImpl::initConfigFile (bool load_user_value, std::string alternate)
{

    _debug ("Manager: Init config file");

    // Init display name to the username under which
    // this sflphone instance is running.
    uid_t uid = getuid();

    struct passwd * user_info = NULL;
    user_info = getpwuid (uid);

    std::string path;
    // Loads config from ~/.sflphone/sflphoned.yml or so..

    if (createSettingsPath() == 1 && load_user_value) {

        (alternate == "") ? path = _path : path = alternate;
        std::cout << path << std::endl;

        _path = path;
    }

    _debug ("Manager: configuration file path: %s", path.c_str());


    bool fileExist = true;
    bool out = false;

    if (path.empty()) {
        _error ("Manager: Error: XDG config file path is empty!");
        fileExist = false;
    }

    std::fstream file;

    file.open (path.data(), std::fstream::in);

    if (!file.is_open()) {

        _debug ("Manager: File %s not opened, create new one", path.c_str());
        file.open (path.data(), std::fstream::out);
        out = true;

        if (!file.is_open()) {
            _error ("Manager: Error: could not create empty configurationfile!");
            fileExist = false;
        }

        file.close();

        fileExist = false;
    }

    // get length of file:
    file.seekg (0, std::ios::end);
    int length = file.tellg();

    file.seekg (0, std::ios::beg);

    if (length <= 0) {
        _debug ("Manager: Configuration file length is empty", length);
        file.close();
        fileExist = false; // should load config
    }

    if (fileExist) {
        try {

            // parser = new Conf::YamlParser("sequenceParser.yml");
            parser = new Conf::YamlParser (_path.c_str());

            parser->serializeEvents();

            parser->composeEvents();

            parser->constructNativeData();

            _setupLoaded = true;

            _debug ("Manager: Configuration file parsed successfully");
        } catch (Conf::YamlParserException &e) {
            _error ("Manager: %s", e.what());
        }
    }
}

/**
 * Initialization: Main Thread
 */
void ManagerImpl::initAudioCodec (void)
{
    _info ("Manager: Init audio codecs");

    /* Init list of all supported codecs by the application.
     * This is a global list. Every account will inherit it.
     */
    _audioCodecFactory.init();
}

std::vector<std::string> ManagerImpl::unserialize (std::string s)
{

    std::vector<std::string> list;
    std::string temp;

    while (s.find ("/", 0) != std::string::npos) {
        size_t pos = s.find ("/", 0);
        temp = s.substr (0, pos);
        s.erase (0, pos + 1);
        list.push_back (temp);
    }

    return list;
}

std::string ManagerImpl::serialize (std::vector<std::string> v)
{
    unsigned int i;
    std::string res;

    for (i = 0; i < v.size(); i++) {
        res += v[i] + "/";
    }

    return res;
}

std::string ManagerImpl::getCurrentCodecName (const CallID& id)
{

    std::string accountid = getAccountFromCall (id);
    VoIPLink* link = getAccountLink (accountid);
    Call* call = link->getCall (id);
    std::string codecName = "";

    _debug("Manager: Get current codec name");

    
    if (!call) {
	// return an empty codec name
        return codecName;
    }

    Call::CallState state = call->getState();
    if (state == Call::Active || state == Call::Conferencing) {
        codecName = link->getCurrentCodecName(id);
    }

    return codecName;	
}

/**
 * Set input audio plugin
 */
void ManagerImpl::setAudioPlugin (const std::string& audioPlugin)
{

	audioLayerMutexLock();
    int layerType = _audiodriver -> getLayerType();

    audioPreference.setPlugin (audioPlugin);

    if (CHECK_INTERFACE (layerType , ALSA)) {
        _debug ("Set input audio plugin");
        _audiodriver -> setErrorMessage (-1);
        _audiodriver -> openDevice (_audiodriver->getIndexIn(), _audiodriver->getIndexOut(),
                                    _audiodriver->getIndexRing(), _audiodriver -> getSampleRate(),
                                    _audiodriver -> getFrameSize(), SFL_PCM_BOTH, audioPlugin);

        if (_audiodriver -> getErrorMessage() != -1)
            notifyErrClient (_audiodriver -> getErrorMessage());
    }
    audioLayerMutexUnlock();

}

/**
 * Set audio output device
 */
void ManagerImpl::setAudioDevice (const int index, int streamType)
{

    AlsaLayer *alsalayer = NULL;
    std::string alsaplugin;
    _debug ("Manager: Set audio device: %d", index);

    audioLayerMutexLock();

    if(_audiodriver == NULL) {
    	_warn ("Manager: Error: No audio driver");
    	audioLayerMutexUnlock();
    	return;
    }

    _audiodriver -> setErrorMessage (-1);

    alsalayer = dynamic_cast<AlsaLayer*> (_audiodriver);
    alsaplugin = alsalayer->getAudioPlugin();

    _debug ("Manager: Set ALSA plugin: %s", alsaplugin.c_str());


    switch (streamType) {
        case SFL_PCM_PLAYBACK:
            _debug ("Manager: Set output device");
            _audiodriver->openDevice (_audiodriver->getIndexIn(), index, _audiodriver->getIndexRing(),
                                      _audiodriver->getSampleRate(), _audiodriver->getFrameSize(),
                                      SFL_PCM_PLAYBACK, alsaplugin);
            audioPreference.setCardout (index);
            break;
        case SFL_PCM_CAPTURE:
            _debug ("Manager: Set input device");
            _audiodriver->openDevice (index, _audiodriver->getIndexOut(), _audiodriver->getIndexRing(),
                                      _audiodriver->getSampleRate(), _audiodriver->getFrameSize(),
                                      SFL_PCM_CAPTURE, alsaplugin);
            audioPreference.setCardin (index);
            break;
        case SFL_PCM_RINGTONE:
            _debug ("Manager: Set ringtone device");
            _audiodriver->openDevice (_audiodriver->getIndexOut(), _audiodriver->getIndexOut(), index,
                                      _audiodriver->getSampleRate(), _audiodriver->getFrameSize(),
                                      SFL_PCM_RINGTONE, alsaplugin);
            audioPreference.setCardring (index);
            break;
        default:
            _warn ("Unknown stream type");
    }

    if (_audiodriver -> getErrorMessage() != -1) {
        notifyErrClient (_audiodriver -> getErrorMessage());
    }

    audioLayerMutexUnlock();

}

/**
 * Get list of supported audio output device
 */
std::vector<std::string> ManagerImpl::getAudioOutputDeviceList (void)
{
    _debug ("Manager: Get audio output device list");
    AlsaLayer *alsalayer;
    std::vector<std::string> devices;

    audioLayerMutexLock();

    alsalayer = dynamic_cast<AlsaLayer*> (_audiodriver);

    if (alsalayer) {
        devices = alsalayer -> getSoundCardsInfo (SFL_PCM_PLAYBACK);
    }
    audioLayerMutexUnlock();

    return devices;
}


/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList (void)
{
    AlsaLayer *alsalayer;
    std::vector<std::string> devices;

    audioLayerMutexLock();

    alsalayer = dynamic_cast<AlsaLayer *> (_audiodriver);

    if (alsalayer == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return devices;
    }

    devices = alsalayer->getSoundCardsInfo (SFL_PCM_CAPTURE);

    audioLayerMutexUnlock();

    return devices;
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex ()
{
    _debug ("Get current audio devices index");

    audioLayerMutexLock();

    std::vector<std::string> v;

    if(_audiodriver == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return v;
    }

    std::stringstream ssi, sso, ssr;
    sso << _audiodriver->getIndexOut();
    v.push_back (sso.str());
    ssi << _audiodriver->getIndexIn();
    v.push_back (ssi.str());
    ssr << _audiodriver->getIndexRing();
    v.push_back (ssr.str());

    audioLayerMutexUnlock();

    return v;
}

int ManagerImpl::isIax2Enabled (void)
{
#ifdef USE_IAX
    return true;
#else
    return false;
#endif
}

int ManagerImpl::isRingtoneEnabled (const std::string& id)
{
    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled() ? 1 : 0;
}

void ManagerImpl::ringtoneEnabled (const std::string& id)
{

    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled (false) : account->setRingtoneEnabled (true);

}

std::string ManagerImpl::getRingtoneChoice (const std::string& id)
{

    // retreive specified account id
    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: Not a valid account ID for ringone choice");
        return std::string ("");
    }

    // we need the absolute path
    std::string tone_name = account->getRingtonePath();
    std::string tone_path;

    if (tone_name.find (DIR_SEPARATOR_CH) == std::string::npos) {
        // check in ringtone directory ($(PREFIX)/share/sflphone/ringtones)
        tone_path = std::string (PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR
                    + DIR_SEPARATOR_STR + tone_name;
    } else {
        // the absolute has been saved; do nothing
        tone_path = tone_name;
    }

    _debug ("Manager: get ringtone path %s", tone_path.c_str());

    return tone_path;
}

void ManagerImpl::setRingtoneChoice (const std::string& tone, const std::string& id)
{

    _debug ("Manager: Set ringtone path %s to account", tone.c_str());

    // retreive specified account id
    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: Not a valid account ID for ringtone choice");
        return;
    }

    // we save the absolute path
    account->setRingtonePath (tone);
}

std::string ManagerImpl::getRecordPath (void)
{
    return audioPreference.getRecordpath();
}

void ManagerImpl::setRecordPath (const std::string& recPath)
{
    _debug ("Manager: Set record path %s", recPath.c_str());
    audioPreference.setRecordpath (recPath);
}

bool ManagerImpl::getIsAlwaysRecording(void)
{
	return audioPreference.getIsAlwaysRecording();
}

void ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
	return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

bool ManagerImpl::getMd5CredentialHashing (void)
{
    return preferences.getMd5Hash();
}



void ManagerImpl::setRecordingCall (const CallID& id)
{
    Call *call = NULL;
    Conference *conf = NULL;
    Recordable* rec = NULL;

    if (!isConference (id)) {
        _debug ("Manager: Set recording for call %s", id.c_str());
        std::string accountid = getAccountFromCall (id);
        call = getAccountLink (accountid)->getCall (id);
        rec = static_cast<Recordable *>(call);
    } else {
        _debug ("Manager: Set recording for conference %s", id.c_str());
        ConferenceMap::iterator it = _conferencemap.find (id);
        conf = it->second;
        if(conf->isRecording()) {
        	conf->setState(Conference::ACTIVE_ATTACHED);
        }
        else {
        	conf->setState(Conference::ACTIVE_ATTACHED_REC);
        }
        rec = static_cast<Recordable *>(conf);
    }

    if (rec == NULL) {
	_error("Manager: Error: Could not find recordable instance %s", id.c_str());
	return;
    }

    rec->setRecording();

    if(_dbus)
	_dbus->getCallManager()->recordPlaybackFilepath(id, rec->getFileName());  
}

bool ManagerImpl::isRecording (const CallID& id)
{

    std::string accountid = getAccountFromCall (id);
    Recordable* rec = (Recordable*) getAccountLink (accountid)->getCall (id);

    bool ret = false;

    if (rec)
        ret = rec->isRecording();

    return ret;
}

bool ManagerImpl::startRecordedFilePlayback(const std::string& filepath) 
{
    int sampleRate;
 
    _debug("Manager: Start recorded file playback %s", filepath.c_str());

    audioLayerMutexLock();

    if(!_audiodriver) {
	_error("Manager: Error: No audio layer in start recorded file playback");
    }

    sampleRate = _audiodriver->getSampleRate();

    audioLayerMutexUnlock();

    _toneMutex.enterMutex();

    if(_audiofile) {
	 if(_dbus) {
	     std::string file = _audiofile->getFilePath();
	     _dbus->getCallManager()->recordPlaybackStoped(file);
         }
	 delete _audiofile;
	 _audiofile = NULL;
    }

    try {
        _audiofile = static_cast<AudioFile *>(new WaveFile());

        _audiofile->loadFile(filepath, NULL, sampleRate);
    }
    catch(AudioFileException &e) {
        _error("Manager: Exception: %s", e.what());
    }

    _audiofile->start();

    _toneMutex.leaveMutex();

    audioLayerMutexLock();
    _audiodriver->startStream();
    audioLayerMutexUnlock(); 

    return true;
}


void ManagerImpl::stopRecordedFilePlayback(const std::string& filepath)
{
    _debug("Manager: Stop recorded file playback %s", filepath.c_str());

    audioLayerMutexLock();
    _audiodriver->stopStream();
    audioLayerMutexUnlock();

    _toneMutex.enterMutex();
    if(_audiofile != NULL) {
        _audiofile->stop();
	delete _audiofile;
	_audiofile = NULL;
    }
    _toneMutex.leaveMutex();
}

void ManagerImpl::setHistoryLimit (const int& days)
{
    _debug ("Manager: Set history limit");

    preferences.setHistoryLimit (days);

    saveConfig();
}

int ManagerImpl::getHistoryLimit (void)
{
    return preferences.getHistoryLimit();
}

int32_t ManagerImpl::getMailNotify (void)
{
    return preferences.getNotifyMails();
}

void ManagerImpl::setMailNotify (void)
{
    _debug ("Manager: Set mail notify");

    preferences.getNotifyMails() ? preferences.setNotifyMails (true) : preferences.setNotifyMails (false);

    saveConfig();
}

void ManagerImpl::setAudioManager (const int32_t& api)
{

    int layerType;

    _debug ("Manager: Setting audio manager ");

    audioLayerMutexLock();

    if (!_audiodriver) {
    	audioLayerMutexUnlock();
        return;
    }

    layerType = _audiodriver->getLayerType();

    if (layerType == api) {
        _debug ("Manager: Audio manager chosen already in use. No changes made. ");
        audioLayerMutexUnlock();
        return;
    }

    audioLayerMutexUnlock();

    preferences.setAudioApi (api);

    switchAudioManager();

    saveConfig();

}

int32_t ManagerImpl::getAudioManager (void)
{
    return preferences.getAudioApi();
}


void ManagerImpl::notifyErrClient (const int32_t& errCode)
{
    if (_dbus) {
        _debug ("Manager: NOTIFY ERR NUMBER %d" , errCode);
        _dbus -> getConfigurationManager() -> errorAlert (errCode);
    }
}

int ManagerImpl::getAudioDeviceIndex (const std::string name)
{
    AlsaLayer *alsalayer;
    int soundCardIndex = 0;

    _debug ("Manager: Get audio device index");

    audioLayerMutexLock();

    if(_audiodriver == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return soundCardIndex;
    }

    alsalayer = dynamic_cast<AlsaLayer *> (_audiodriver);

    if (alsalayer) {
        soundCardIndex = alsalayer -> soundCardGetIndex (name);
    }

    audioLayerMutexUnlock();

    return soundCardIndex;
}

std::string ManagerImpl::getCurrentAudioOutputPlugin (void)
{
    _debug ("Manager: Get alsa plugin");

    return audioPreference.getPlugin();
}


std::string ManagerImpl::getNoiseSuppressState (void)
{

    // noise suppress disabled by default
    std::string state;

    state = audioPreference.getNoiseReduce() ? "enabled" : "disabled";

    return state;
}

void ManagerImpl::setNoiseSuppressState (std::string state)
{
    _debug ("Manager: Set noise suppress state: %s", state.c_str());

    bool isEnabled = (state == "enabled");

    audioPreference.setNoiseReduce (isEnabled);

    audioLayerMutexLock();

    if (_audiodriver) {
        _audiodriver->setNoiseSuppressState (isEnabled);
    }

    audioLayerMutexUnlock();
}

std::string ManagerImpl::getEchoCancelState(void)
{
	// echo canceller disabled by default
	std::string state;

	state = audioPreference.getEchoCancel() ? "enabled" : "disabled";

	return state;
}

void ManagerImpl::setEchoCancelState(std::string state)
{

	bool isEnabled = (state == "enabled");

	audioPreference.setEchoCancel(isEnabled);
}

int ManagerImpl::getEchoCancelTailLength(void)
{
	return audioPreference.getEchoCancelTailLength();
}

void ManagerImpl::setEchoCancelTailLength(int length)
{
	audioPreference.setEchoCancelTailLength(length);
}

int ManagerImpl::getEchoCancelDelay(void)
{
	return audioPreference.getEchoCancelDelay();
}

void ManagerImpl::setEchoCancelDelay(int delay)
{
	audioPreference.setEchoCancelDelay(delay);
}

int ManagerImpl::app_is_running (std::string process)
{
    std::ostringstream cmd;

    cmd << "ps -C " << process;
    return system (cmd.str().c_str());
}

/**
 * Initialization: Main Thread
 */
bool ManagerImpl::initAudioDriver (void)
{

    int error;

    _debugInit ("Manager: AudioLayer Creation");

    audioLayerMutexLock();

    if (preferences.getAudioApi() == ALSA) {
        _audiodriver = new AlsaLayer (this);
        _audiodriver->setMainBuffer (&_mainBuffer);
    } else if (preferences.getAudioApi() == PULSEAUDIO) {
        if (app_is_running ("pulseaudio") == 0) {
            _audiodriver = new PulseLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
        } else {
            _audiodriver = new AlsaLayer (this);
            preferences.setAudioApi (ALSA);
            _audiodriver->setMainBuffer (&_mainBuffer);
        }
    } else {
        _debug ("Error - Audio API unknown");
    }

    if (_audiodriver == NULL) {
        _debug ("Manager: Init audio driver error");
        audioLayerMutexUnlock();
        return false;
    } else {
        error = _audiodriver->getErrorMessage();

        if (error == -1) {
            _debug ("Manager: Init audio driver: %d", error);
            audioLayerMutexUnlock();
            return false;
        }
    }

    audioLayerMutexUnlock();

    return true;

}

/**
 * Initialization: Main Thread and gui
 */
void ManagerImpl::selectAudioDriver (void)
{
    int layerType, numCardIn, numCardOut, numCardRing, sampleRate, frameSize;
    std::string alsaPlugin;
    AlsaLayer *alsalayer;

    audioLayerMutexLock();

    if(_audiodriver == NULL) {
    	_debug("Manager: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return;
    }

    layerType = _audiodriver->getLayerType();
    _debug ("Manager: Audio layer type: %d" , layerType);

    /* Retrieve the global devices info from the user config */
    alsaPlugin = audioPreference.getPlugin();
    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    sampleRate = getMainBuffer()->getInternalSamplingRate();
    frameSize = audioPreference.getFramesize();

    /* Only for the ALSA layer, we check the sound card information */

    if (layerType == ALSA) {
        alsalayer = dynamic_cast<AlsaLayer*> (_audiodriver);

        if (!alsalayer -> soundCardIndexExist (numCardIn, SFL_PCM_CAPTURE)) {
            _debug (" Card with index %d doesn't exist or cannot capture. Switch to 0.", numCardIn);
            numCardIn = ALSA_DFT_CARD_ID;
            audioPreference.setCardin (ALSA_DFT_CARD_ID);
        }

        if (!alsalayer -> soundCardIndexExist (numCardOut, SFL_PCM_PLAYBACK)) {
            _debug (" Card with index %d doesn't exist or cannot playback. Switch to 0.", numCardOut);
            numCardOut = ALSA_DFT_CARD_ID;
            audioPreference.setCardout (ALSA_DFT_CARD_ID);
        }

        if (!alsalayer->soundCardIndexExist (numCardRing, SFL_PCM_RINGTONE)) {
            _debug (" Card with index %d doesn't exist or cannot ringtone. Switch to 0.", numCardRing);
            numCardRing = ALSA_DFT_CARD_ID;
            audioPreference.setCardring (ALSA_DFT_CARD_ID);
        }
    }

    _audiodriver->setErrorMessage (-1);

    /* Open the audio devices */
    _audiodriver->openDevice (numCardIn, numCardOut, numCardRing, sampleRate, frameSize,
                              SFL_PCM_BOTH, alsaPlugin);

    /* Notify the error if there is one */

    if (_audiodriver -> getErrorMessage() != -1) {
        notifyErrClient (_audiodriver -> getErrorMessage());
    }

    audioLayerMutexUnlock();

}

void ManagerImpl::switchAudioManager (void)
{
    int type, samplerate, framesize, numCardIn, numCardOut, numCardRing;
    std::string alsaPlugin;

    _debug ("Manager: Switching audio manager ");

    audioLayerMutexLock();

    if (_audiodriver == NULL) {
    	audioLayerMutexUnlock();
        return;
    }

    bool wasStarted = _audiodriver->isStarted();

    type = _audiodriver->getLayerType();

    samplerate = _mainBuffer.getInternalSamplingRate();
    framesize = audioPreference.getFramesize();

    _debug ("Manager: samplerate: %d, framesize %d", samplerate, framesize);

    alsaPlugin = audioPreference.getPlugin();

    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    _debug ("Manager: Deleting current layer... ");

    delete _audiodriver;
    _audiodriver = NULL;

    switch (type) {

        case ALSA:
            _debug ("Manager: Creating Pulseaudio layer...");
            _audiodriver = new PulseLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        case PULSEAUDIO:
            _debug ("Manager: Creating ALSA layer...");
            _audiodriver = new AlsaLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        default:
            _warn ("Manager: Error: audio layer unknown");
            break;
    }

    _audiodriver->setErrorMessage (-1);

    _audiodriver->openDevice (numCardIn, numCardOut, numCardRing, samplerate, framesize,
                              SFL_PCM_BOTH, alsaPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    _debug ("Manager: Current device: %d ", type);

    if (wasStarted) {
        _audiodriver->startStream();
    }

    audioLayerMutexUnlock();
}

void ManagerImpl::audioSamplingRateChanged (int samplerate)
{

    int type, currentSamplerate, framesize, numCardIn, numCardOut, numCardRing;
    std::string alsaPlugin;
    bool wasActive;

    audioLayerMutexLock();

    if (!_audiodriver) {
    	_debug("Manager: No Audio driver initialized");
    	audioLayerMutexUnlock();
        return;
    }


    // Only modify internal sampling rate if new sampling rate is higher
    currentSamplerate = _mainBuffer.getInternalSamplingRate();
    if(currentSamplerate >= samplerate) {
    	_debug("Manager: No need to update audio layer sampling rate");
    	audioLayerMutexUnlock();
    	return;
    }
    else {
        _debug ("Manager: Audio sampling rate changed");
    }

    type = _audiodriver->getLayerType();
    framesize = audioPreference.getFramesize();

    _debug ("Manager: New samplerate: %d, New framesize %d", samplerate, framesize);

    alsaPlugin = audioPreference.getPlugin();

    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    _debug ("Manager: Deleting current layer...");

    wasActive = _audiodriver->isStarted();

    delete _audiodriver;
    _audiodriver = NULL;

    switch (type) {

        case PULSEAUDIO:
            _debug ("Manager: Creating Pulseaudio layer...");
            _audiodriver = new PulseLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        case ALSA:
            _debug ("Manager: Creating ALSA layer...");
            _audiodriver = new AlsaLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        default:
            _warn ("Manager: Error: audio layer unknown");
            break;
    }

    if(_audiodriver == NULL) {
    	_debug("Manager: Error: Audio driver could not be initialized");
    	audioLayerMutexUnlock();
    	return;
    }

    _audiodriver->setErrorMessage (-1);

    _audiodriver->openDevice (numCardIn, numCardOut, numCardRing, samplerate, framesize,
                              SFL_PCM_BOTH, alsaPlugin);

    if (_audiodriver -> getErrorMessage() != -1) {
        notifyErrClient (_audiodriver -> getErrorMessage());
    }

    _debug ("Manager: Current device: %d ", type);

    _mainBuffer.setInternalSamplingRate(samplerate);

    unsigned int sampleRate = _audiodriver->getSampleRate();

    delete _telephoneTone;
    _telephoneTone = NULL;

    _debugInit ("Manager: Load telephone tone");
    std::string country = preferences.getZoneToneChoice();
    _telephoneTone = new TelephoneTone (country, sampleRate);

    if(_telephoneTone == NULL) {
        _debug("Manager: Error: Telephone tone is NULL");
    }
     
    delete _dtmfKey;
    _dtmfKey = NULL;

    _debugInit ("Manager: Loading DTMF key with sample rate %d", sampleRate);
    _dtmfKey = new DTMF (sampleRate);

    if(_dtmfKey == NULL) {
        _debug("Manager: Error: DtmfKey is NULL");
    }

    // Restart audio layer if it was active
    if (wasActive) {
        _audiodriver->startStream();
    }

    audioLayerMutexUnlock();
}

/**
 * Init the volume for speakers/micro from 0 to 100 value
 * Initialization: Main Thread
 */
void ManagerImpl::initVolume ()
{
    _debugInit ("Initiate Volume");
    setSpkrVolume (audioPreference.getVolumespkr());
    setMicVolume (audioPreference.getVolumemic());
}

void ManagerImpl::setSpkrVolume (unsigned short spkr_vol)
{
    PulseLayer *pulselayer = NULL;

    /* Set the manager sound volume */
    _spkr_volume = spkr_vol;

    audioLayerMutexLock();

    /* Only for PulseAudio */
    pulselayer = dynamic_cast<PulseLayer*> (_audiodriver);

    if (pulselayer) {
        if (pulselayer->getLayerType() == PULSEAUDIO) {
            if (pulselayer)
                pulselayer->setPlaybackVolume (spkr_vol);
        }
    }

    audioLayerMutexUnlock();
}

void ManagerImpl::setMicVolume (unsigned short mic_vol)
{
    _mic_volume = mic_vol;
}

int ManagerImpl::getLocalIp2IpPort (void)
{
    // The SIP port used for default account (IP to IP) calls=
    return preferences.getPortNum();

}

// TODO: rewrite this
/**
 * Main Thread
 */
bool ManagerImpl::getCallStatus (const std::string& sequenceId UNUSED)
{
    if (!_dbus) {
        return false;
    }

    ost::MutexLock m (_callAccountMapMutex);

    CallAccountMap::iterator iter = _callAccountMap.begin();
    TokenList tk;
    std::string code;
    std::string status;
    std::string destination;
    std::string number;

    while (iter != _callAccountMap.end()) {
        Call* call = getAccountLink (iter->second)->getCall (iter->first);
        Call::ConnectionState state = call->getConnectionState();

        if (state != Call::Connected) {
            switch (state) {

                case Call::Trying:
                    code = "110";
                    status = "Trying";
                    break;

                case Call::Ringing:
                    code = "111";
                    status = "Ringing";
                    break;

                case Call::Progressing:
                    code = "125";
                    status = "Progressing";
                    break;

                case Call::Disconnected:
                    code = "125";
                    status = "Disconnected";
                    break;

                default:
                    code = "";
                    status = "";
            }
        } else {
            switch (call->getState()) {

                case Call::Active:

                case Call::Conferencing:
                    code = "112";
                    status = "Established";
                    break;

                case Call::Hold:
                    code = "114";
                    status = "Held";
                    break;

                case Call::Busy:
                    code = "113";
                    status = "Busy";
                    break;

                case Call::Refused:
                    code = "125";
                    status = "Refused";
                    break;

                case Call::Error:
                    code = "125";
                    status = "Error";
                    break;

                case Call::Inactive:
                    code = "125";
                    status = "Inactive";
                    break;
            }
        }

        // No Congestion
        // No Wrong Number
        // 116 <CSeq> <call-id> <acc> <destination> Busy
        destination = call->getPeerName();

        number = call->getPeerNumber();

        if (number != "") {
            destination.append (" <");
            destination.append (number);
            destination.append (">");
        }

        tk.push_back (iter->second);

        tk.push_back (destination);
        tk.push_back (status);
        tk.clear();

        iter++;
    }

    return true;
}

//THREAD=Main
bool ManagerImpl::getConfig (const std::string& section,
                             const std::string& name, TokenList& arg)
{
    return _config.getConfigTreeItemToken (section, name, arg);
}

//THREAD=Main
// throw an Conf::ConfigTreeItemException if not found
int ManagerImpl::getConfigInt (const std::string& section,
                               const std::string& name)
{
    try {
        return _config.getConfigTreeItemIntValue (section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return 0;
}

bool ManagerImpl::getConfigBool (const std::string& section,
                                 const std::string& name)
{
    try {
        return (_config.getConfigTreeItemValue (section, name) == Conf::TRUE_STR) ? true
               : false;
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return false;
}

//THREAD=Main
std::string ManagerImpl::getConfigString (const std::string& section,
        const std::string& name)
{
    try {
        return _config.getConfigTreeItemValue (section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return "";
}

//THREAD=Main
bool ManagerImpl::setConfig (const std::string& section,
                             const std::string& name, const std::string& value)
{

    return _config.setConfigTreeItem (section, name, value);
}

//THREAD=Main
bool ManagerImpl::setConfig (const std::string& section,
                             const std::string& name, int value)
{
    std::ostringstream valueStream;
    valueStream << value;
    return _config.setConfigTreeItem (section, name, valueStream.str());
}

void ManagerImpl::setAccountsOrder (const std::string& order)
{
    _debug ("Manager: Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder (order);

    saveConfig();
}

std::vector<std::string> ManagerImpl::getAccountList ()
{

    std::vector<std::string> v;
    std::vector<std::string> account_order;
    unsigned int i;

    _debug ("Manager: Get account list");

    account_order = loadAccountOrder();
    AccountMap::iterator iter;

    // The IP2IP profile is always available, and first in the list
    iter = _accountMap.find (IP2IP_PROFILE);

    if (iter->second != NULL) {
        // _debug("PUSHING BACK %s", iter->first.c_str());
        // v.push_back(iter->first.data());
        v.push_back (iter->second->getAccountID());
    } else {
        _error ("Manager: could not find IP2IP profile in getAccount list");
    }

    // If no order has been set, load the default one
    // ie according to the creation date.

    if (account_order.size() == 0) {
        _debug ("Manager: account order is empty");
        iter = _accountMap.begin();

        while (iter != _accountMap.end()) {

            if (iter->second != NULL && iter->first != IP2IP_PROFILE && iter->first != "") {
                _debug ("PUSHING BACK %s", iter->first.c_str());
                // v.push_back(iter->first.data());
                v.push_back (iter->second->getAccountID());
            }

            iter++;
        }
    }

    // Otherelse, load the custom one
    // ie according to the saved order
    else {
        _debug ("Manager: Load account list according to preferences");

        for (i = 0; i < account_order.size(); i++) {
            // This account has not been loaded, so we ignore it
            if ( (iter = _accountMap.find (account_order[i]))
                    != _accountMap.end()) {
                // If the account is valid
                if (iter->second != NULL && iter->first != IP2IP_PROFILE && iter->first != "") {
                    //_debug("PUSHING BACK %s", iter->first.c_str());
                    // v.push_back(iter->first.data());
                    v.push_back (iter->second->getAccountID());
                }
            }
        }
    }

    return v;
}

std::map<std::string, std::string> ManagerImpl::getAccountDetails (
    const std::string& accountID)
{
    // Default account used to get default parameters if requested by client (to build new account)
    static const SIPAccount DEFAULT_ACCOUNT("default");

    Account * account = _accountMap[accountID];

    if (accountID.empty()) {
        _debug ("Manager: Returning default account settings");
        // return a default map
        return DEFAULT_ACCOUNT.getAccountDetails();
    } else if (account) {
        return account->getAccountDetails();
    } else {
        _debug ("Manager: Get account details on a non-existing accountID %s. Returning default", accountID.c_str());
        return DEFAULT_ACCOUNT.getAccountDetails();
    }

}

/* Transform digest to string.
 * output must be at least PJSIP_MD5STRLEN+1 bytes.
 * Helper function taken from sip_auth_client.c in
 * pjproject-1.0.3.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */

void ManagerImpl::digest2str (const unsigned char digest[], char *output)
{
    int i;

    for (i = 0; i < 16; ++i) {
        pj_val_to_hex_digit (digest[i], output);
        output += 2;
    }
}

std::string ManagerImpl::computeMd5HashFromCredential (
    const std::string& username, const std::string& password,
    const std::string& realm)
{
    pj_md5_context pms;
    unsigned char digest[16];
    char ha1[PJSIP_MD5STRLEN];

    pj_str_t usernamePjFormat = pj_str (strdup (username.c_str()));
    pj_str_t passwordPjFormat = pj_str (strdup (password.c_str()));
    pj_str_t realmPjFormat = pj_str (strdup (realm.c_str()));

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init (&pms);
    MD5_APPEND (&pms, usernamePjFormat.ptr, usernamePjFormat.slen);
    MD5_APPEND (&pms, ":", 1);
    MD5_APPEND (&pms, realmPjFormat.ptr, realmPjFormat.slen);
    MD5_APPEND (&pms, ":", 1);
    MD5_APPEND (&pms, passwordPjFormat.ptr, passwordPjFormat.slen);
    pj_md5_final (&pms, digest);

    digest2str (digest, ha1);

    char ha1_null_terminated[PJSIP_MD5STRLEN + 1];
    memcpy (ha1_null_terminated, ha1, sizeof (char) * PJSIP_MD5STRLEN);
    ha1_null_terminated[PJSIP_MD5STRLEN] = '\0';

    std::string hashedDigest = ha1_null_terminated;
    return hashedDigest;
}

void ManagerImpl::setCredential (const std::string& accountID UNUSED,
                                 const int32_t& index UNUSED, const std::map<std::string, std::string>& details UNUSED)
{

    _debug ("Manager: set credential");
}

// method to reduce the if/else mess.
// Even better, switch to XML !

void ManagerImpl::setAccountDetails (const std::string& accountID,
                                     const std::map<std::string, std::string>& details)
{

    _debug ("Manager: Set account details for %s", accountID.c_str());

    Account* account = getAccount(accountID);
    if (account == NULL) {
        _error ("Manager: Error: Could not find account %s", accountID.c_str());
        return;
    }

    account->setAccountDetails (details);

    // Serialize configuration to disk once it is done
    saveConfig();

    if (account->isEnabled()) {
        account->registerVoIPLink();
    }
    else {
        account->unregisterVoIPLink();
    }

    // Update account details to the client side
    if (_dbus) {
        _error("Manager: Error: Dbus not initialized");
        return;
    }

    _dbus->getConfigurationManager()->accountsChanged();

}

std::string ManagerImpl::addAccount (
    const std::map<std::string, std::string>& details)
{

    /** @todo Deal with both the _accountMap and the Configuration */
    std::string accountType, account_list;
    Account* newAccount;
    std::stringstream accountID;
    std::string newAccountID;

    accountID << "Account:" << time (NULL);
    newAccountID = accountID.str();

    // Get the type
    accountType = (*details.find (CONFIG_ACCOUNT_TYPE)).second;

    _debug ("Manager: Adding account %s", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    if (accountType == "SIP") {
        newAccount = AccountCreator::createAccount (AccountCreator::SIP_ACCOUNT,
                     newAccountID);
        newAccount->setVoIPLink();
    } else if (accountType == "IAX") {
        newAccount = AccountCreator::createAccount (AccountCreator::IAX_ACCOUNT,
                     newAccountID);
    } else {
        _error ("Unknown %s param when calling addAccount(): %s", CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    _accountMap[newAccountID] = newAccount;

    newAccount->setAccountDetails (details);

    // Add the newly created account in the account order list
    account_list = preferences.getAccountOrder();

    if (account_list != "") {
        newAccountID += "/";
        // Prepend the new account
        account_list.insert (0, newAccountID);

        preferences.setAccountOrder (account_list);
    } else {
        newAccountID += "/";
        account_list = newAccountID;
        preferences.setAccountOrder (account_list);
    }

    _debug ("AccountMap: %s", account_list.c_str());

    newAccount->setVoIPLink();

    newAccount->registerVoIPLink();

    saveConfig();

    if (_dbus)
        _dbus->getConfigurationManager()->accountsChanged();

    return accountID.str();
}

void ManagerImpl::deleteAllCredential (const std::string& accountID)
{

    _debug ("Manager: delete all credential");

    Account *account = getAccount (accountID);

    if (!account)
        return;

    if (account->getType() != "SIP")
        return;

    SIPAccount *sipaccount = (SIPAccount *) account;

    if (accountID.empty() == false) {
        sipaccount->setCredentialCount (0);
    }
}

void ManagerImpl::removeAccount (const std::string& accountID)
{
    // Get it down and dying
    Account* remAccount = NULL;
    remAccount = getAccount (accountID);

    if (remAccount != NULL) {
        remAccount->unregisterVoIPLink();
        _accountMap.erase (accountID);
        // http://projects.savoirfairelinux.net/issues/show/2355
        // delete remAccount;
    }

    _config.removeSection (accountID);

    saveConfig();

    _debug ("REMOVE ACCOUNT");

    if (_dbus)
        _dbus->getConfigurationManager()->accountsChanged();

}

// ACCOUNT handling
bool ManagerImpl::associateCallToAccount (const CallID& callID,
        const std::string& accountID)
{
    if (getAccountFromCall (callID) == "") { // nothing with the same ID
        if (accountExists (accountID)) { // account id exist in AccountMap
            ost::MutexLock m (_callAccountMapMutex);
            _callAccountMap[callID] = accountID;
            _debug ("Manager: Associate Call %s with Account %s", callID.data(), accountID.data());
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

std::string ManagerImpl::getAccountFromCall (const CallID& callID)
{
    ost::MutexLock m (_callAccountMapMutex);
    CallAccountMap::iterator iter = _callAccountMap.find (callID);

    if (iter == _callAccountMap.end()) {
        return "";
    } else {
        return iter->second;
    }
}

bool ManagerImpl::removeCallAccount (const CallID& callID)
{
    ost::MutexLock m (_callAccountMapMutex);

    if (_callAccountMap.erase (callID)) {
        return true;
    }

    return false;
}

bool ManagerImpl::isValidCall(const CallID& callID)
{
	ost::MutexLock m(_callAccountMapMutex);
	CallAccountMap::iterator iter = _callAccountMap.find (callID);

	if(iter != _callAccountMap.end()) {
		return true;
	}
	else {
		return false;
	}

}

CallID ManagerImpl::getNewCallID ()
{
    std::ostringstream random_id ("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (getAccountFromCall (random_id.str()) != "") {
        random_id.clear();
        random_id << "s";
        random_id << (unsigned) rand();
    }

    return random_id.str();
}

std::vector<std::string> ManagerImpl::loadAccountOrder (void)
{

    std::string account_list;
    std::vector<std::string> account_vect;

    account_list = preferences.getAccountOrder();

    _debug ("Manager: Load sccount order %s", account_list.c_str());

    return unserialize (account_list);
}

short ManagerImpl::buildConfiguration ()
{

    _debug ("Manager: Loading account map");

    loadIptoipProfile();

    int nbAccount = loadAccountMap();

    return nbAccount;
}

void ManagerImpl::loadIptoipProfile()
{

    _debug ("Manager: Create default \"account\" (used as default UDP transport)");

    // build a default IP2IP account with default parameters
    _directIpAccount = AccountCreator::createAccount (AccountCreator::SIP_DIRECT_IP_ACCOUNT, "");
    _accountMap[IP2IP_PROFILE] = _directIpAccount;
    _accountMap[""] = _directIpAccount;

    if (_directIpAccount == NULL) {
        _error ("Manager: Failed to create default \"account\"");
        return;
    }

    // If configuration file parsed, load saved preferences
    if (_setupLoaded) {

        _debug ("Manager: Loading IP2IP profile preferences from config");

        Conf::SequenceNode *seq = parser->getAccountSequence();

        Conf::Sequence::iterator iterIP2IP = seq->getSequence()->begin();
        std::string accID ("id");

        // Iterate over every account maps
        while (iterIP2IP != seq->getSequence()->end()) {

            Conf::MappingNode *map = (Conf::MappingNode *) (*iterIP2IP);

            // Get the account id
            std::string accountid;
            map->getValue (accID, &accountid);

            // if ID is IP2IP, unserialize
            if (accountid == "IP2IP") {
                _directIpAccount->unserialize (map);
                break;
            }

            iterIP2IP++;
        }
    }

    // Force IP2IP settings to be loaded to be loaded
    // No registration in the sense of the REGISTER method is performed.
    _directIpAccount->registerVoIPLink();

    // SIPVoIPlink is used as a singleton, it is the first call to instance here
    // The SIP library initialization is done in the SIPVoIPLink constructor
    // We need the IP2IP settings to be loaded at this time as they are used
    // for default sip transport

    _directIpAccount->setVoIPLink();

}

short ManagerImpl::loadAccountMap()
{
    _debug ("Manager: Load account map");

    // Conf::YamlParser *parser;
    int nbAccount = 0;

    if (!_setupLoaded) {
    	_error("Manager: Error: Configuration file not loaded yet, could not load config");
    	return 0;
    }

    // build preferences
    preferences.unserialize (parser->getPreferenceNode());
    voipPreferences.unserialize (parser->getVoipPreferenceNode());
    addressbookPreference.unserialize (parser->getAddressbookNode());
    hookPreference.unserialize (parser->getHookNode());
    audioPreference.unserialize (parser->getAudioNode());
    shortcutPreferences.unserialize (parser->getShortcutNode());

    Conf::SequenceNode *seq = parser->getAccountSequence();

    // Each element in sequence is a new account to create
    Conf::Sequence::iterator iterSeq;
    for (iterSeq = seq->getSequence()->begin(); iterSeq != seq->getSequence()->end(); ++iterSeq) {

        // Pointer to the account and account preferences map
        Account *tmpAccount = NULL;
        Conf::MappingNode *map = (Conf::MappingNode *) (*iterSeq);

        // Search for account types (IAX/IP2IP)
        std::string accountType = "SIP"; // Assume type is SIP if not specified
        map->getValue ("type", &accountType);

        // search for account id
        std::string accountid;
        map->getValue ("id", &accountid);

        // search for alias (to get rid of the "ghost" account)
        std::string accountAlias;
        map->getValue ("alias", &accountAlias);

        // do not insert in account map if id or alias is empty
        if (accountid.empty() || accountAlias.empty()) {
            continue;
        }

        // Create a default account for specific type
        if (accountType == "SIP" && accountid != "IP2IP") {
            tmpAccount = AccountCreator::createAccount (AccountCreator::SIP_ACCOUNT, accountid);
        } else if (accountType == "IAX" && accountid != "IP2IP") {
            tmpAccount = AccountCreator::createAccount (AccountCreator::IAX_ACCOUNT, accountid);
        }

        // Fill account with configuration preferences
        if (tmpAccount != NULL) {
            tmpAccount->unserialize (map);
            _accountMap[accountid] = tmpAccount;

            tmpAccount->setVoIPLink();
            nbAccount++;
        }
    }

    try {
        delete parser;
    } catch (Conf::YamlParserException &e) {
        _error ("Manager: %s", e.what());
    }

    parser = NULL;

    return nbAccount;

}

void ManagerImpl::unloadAccountMap ()
{
    _debug ("Manager: Unload account map");

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {
        // Avoid removing the IP2IP account twice
        if (iter->first != "") {
            delete iter->second;
            iter->second = NULL;
        }

        iter++;
    }

    _accountMap.clear();


}

bool ManagerImpl::accountExists (const std::string& accountID)
{
    AccountMap::iterator iter = _accountMap.find (accountID);

    if (iter == _accountMap.end()) {
        return false;
    }

    return true;
}

Account*
ManagerImpl::getAccount (const std::string& accountID)
{
    AccountMap::iterator iter = _accountMap.find (accountID);

    if (iter != _accountMap.end()) {
        return iter->second;
    }

    _debug ("Manager: Did not found account %s, returning IP2IP account", accountID.c_str());
    return _directIpAccount;
}

std::string ManagerImpl::getAccountIdFromNameAndServer (
    const std::string& userName, const std::string& server)
{

    AccountMap::iterator iter;
    SIPAccount *account;

    _info ("Manager : username = %s , server = %s", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->fullMatch (userName, server)) {
                _debug ("Manager: Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
                return iter->first;
            }
        }
    }

    // We failed! Then only match the hostname
    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->hostnameMatch (server)) {
                _debug ("Manager: Matching account id in request with hostname %s", server.c_str());
                return iter->first;
            }
        }
    }

    // We failed! Then only match the username
    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->userMatch (userName)) {
                _debug ("Manager: Matching account id in request with username %s", userName.c_str());
                return iter->first;
            }
        }
    }

    _debug ("Manager: Username %s or server %s doesn't match any account, using IP2IP", userName.c_str(), server.c_str());

    // Failed again! return ""
    return "";
}

std::map<std::string, int32_t> ManagerImpl::getAddressbookSettings ()
{

    std::map<std::string, int32_t> settings;

    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_ENABLE", addressbookPreference.getEnabled() ? 1 : 0));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_MAX_RESULTS", addressbookPreference.getMaxResults()));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO", addressbookPreference.getPhoto() ? 1 : 0));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS", addressbookPreference.getBusiness() ? 1 : 0));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_HOME", addressbookPreference.getHome() ? 1 : 0));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE", addressbookPreference.getMobile() ? 1 : 0));

    return settings;
}

void ManagerImpl::setAddressbookSettings (
    const std::map<std::string, int32_t>& settings)
{
    _debug ("Manager: Update addressbook settings");

    addressbookPreference.setEnabled ( (settings.find ("ADDRESSBOOK_ENABLE")->second == 1) ? true : false);
    addressbookPreference.setMaxResults (settings.find ("ADDRESSBOOK_MAX_RESULTS")->second);
    addressbookPreference.setPhoto ( (settings.find ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")->second == 1) ? true : false);
    addressbookPreference.setBusiness ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")->second == 1) ? true : false);
    addressbookPreference.setHone ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_HOME")->second == 1) ? true : false);
    addressbookPreference.setMobile ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")->second == 1) ? true : false);

    // Write it to the configuration file
    // TODO save config is called for updateAddressbookSettings, updateHookSettings, setHistoryLimit each called
    // when closing preference window (in this order)
    // saveConfig();
}

void ManagerImpl::setAddressbookList (const std::vector<std::string>& list)
{
    _debug ("Manager: Set addressbook list");

    std::string s = serialize (list);
    _debug("Manager: New addressbook list: %s", s.c_str());
    addressbookPreference.setList (s);

    saveConfig();
}

std::vector<std::string> ManagerImpl::getAddressbookList (void)
{

    std::string s = addressbookPreference.getList();
    return unserialize (s);
}

std::map<std::string, std::string> ManagerImpl::getHookSettings ()
{

    std::map<std::string, std::string> settings;


    settings.insert (std::pair<std::string, std::string> ("URLHOOK_IAX2_ENABLED", hookPreference.getIax2Enabled() ? "true" : "false"));
    settings.insert (std::pair<std::string, std::string> ("PHONE_NUMBER_HOOK_ADD_PREFIX", hookPreference.getNumberAddPrefix()));
    settings.insert (std::pair<std::string, std::string> ("PHONE_NUMBER_HOOK_ENABLED", hookPreference.getNumberEnabled() ? "true" : "false"));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_SIP_ENABLED", hookPreference.getSipEnabled() ? "true" : "false"));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_COMMAND", hookPreference.getUrlCommand()));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_SIP_FIELD", hookPreference.getUrlSipField()));

    return settings;
}

void ManagerImpl::setHookSettings (const std::map<std::string, std::string>& settings)
{

    hookPreference.setIax2Enabled (settings.find ("URLHOOK_IAX2_ENABLED")->second == "true");
    hookPreference.setNumberAddPrefix (settings.find ("PHONE_NUMBER_HOOK_ADD_PREFIX")->second);
    hookPreference.setNumberEnabled (settings.find ("PHONE_NUMBER_HOOK_ENABLED")->second == "true");
    hookPreference.setSipEnabled (settings.find ("URLHOOK_SIP_ENABLED")->second == "true");
    hookPreference.setUrlCommand (settings.find ("URLHOOK_COMMAND")->second);
    hookPreference.setUrlSipField (settings.find ("URLHOOK_SIP_FIELD")->second);

    // Write it to the configuration file
    // TODO save config is called for updateAddressbookSettings, updateHookSettings, setHistoryLimit each called
    // when closing preference window (in this order)
    // saveConfig();
}

void ManagerImpl::checkCallConfiguration (const CallID& id,
        const std::string &to, Call::CallConfiguration *callConfig)
{
    Call::CallConfiguration config;

    if (to.find (SIP_SCHEME) == 0 || to.find (SIPS_SCHEME) == 0) {
        _debug ("Manager: Sip scheme detected (sip: or sips:), sending IP2IP Call");
        config = Call::IPtoIP;
    } else {
        config = Call::Classic;
    }

    associateConfigToCall (id, config);

    *callConfig = config;
}

bool ManagerImpl::associateConfigToCall (const CallID& callID,
        Call::CallConfiguration config)
{

    if (getConfigFromCall (callID) == CallConfigNULL) { // nothing with the same ID
        _callConfigMap[callID] = config;
        _debug ("Manager: Associate call %s with config %d", callID.c_str(), config);
        return true;
    } else {
        return false;
    }
}

Call::CallConfiguration ManagerImpl::getConfigFromCall (const CallID& callID)
{

    CallConfigMap::iterator iter = _callConfigMap.find (callID);

    if (iter == _callConfigMap.end()) {
        return (Call::CallConfiguration) CallConfigNULL;
    } else {
        return iter->second;
    }
}

bool ManagerImpl::removeCallConfig (const CallID& callID)
{

    if (_callConfigMap.erase (callID)) {
        return true;
    }

    return false;
}

std::map<std::string, std::string> ManagerImpl::getCallDetails (const CallID& callID)
{

    std::map<std::string, std::string> call_details;
    std::string accountid;
    Account *account;
    VoIPLink *link;
    Call *call = NULL;
    std::stringstream type;

    // We need here to retrieve the call information attached to the call ID
    // To achieve that, we need to get the voip link attached to the call
    // But to achieve that, we need to get the account the call was made with

    // So first we fetch the account
    accountid = getAccountFromCall (callID);

    // Then the VoIP link this account is linked with (IAX2 or SIP)
    if ( (account = getAccount (accountid)) != NULL) {
        link = account->getVoIPLink();

        if (link) {
            call = link->getCall (callID);
        }
    }

    if (call) {
        type << call->getCallType();
        call_details.insert (std::pair<std::string, std::string> ("ACCOUNTID", accountid));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NUMBER", call->getPeerNumber()));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NAME", call->getPeerName()));
        call_details.insert (std::pair<std::string, std::string> ("DISPLAY_NAME", call->getDisplayName()));
        call_details.insert (std::pair<std::string, std::string> ("CALL_STATE", call->getStateStr()));
        call_details.insert (std::pair<std::string, std::string> ("CALL_TYPE", type.str()));
    } else {
        _error ("Manager: Error: getCallDetails()");
        call_details.insert (std::pair<std::string, std::string> ("ACCOUNTID", ""));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NUMBER", "Unknown"));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NAME", "Unknown"));
        call_details.insert (std::pair<std::string, std::string> ("DISPLAY_NAME", "Unknown"));
        call_details.insert (std::pair<std::string, std::string> ("CALL_STATE", "UNKNOWN"));
        call_details.insert (std::pair<std::string, std::string> ("CALL_TYPE", "0"));
    }

    return call_details;
}

std::vector<std::string> ManagerImpl::getHistorySerialized(void)
{
    _debug("Manager: Get history serialized"); 

    return _history->get_history_serialized();
}

void ManagerImpl::setHistorySerialized(std::vector<std::string> history)
{

    _debug("Manager: Set history serialized");

    _history->set_serialized_history (history, preferences.getHistoryLimit());;
    _history->save_history();
}

std::vector<std::string> ManagerImpl::getCallList (void)
{
    std::vector<std::string> v;

    CallAccountMap::iterator iter = _callAccountMap.begin();

    while (iter != _callAccountMap.end()) {
        v.push_back (iter->first.data());
        iter++;
    }

    return v;
}

std::map<std::string, std::string> ManagerImpl::getConferenceDetails (
    const ConfID& confID)
{

    std::map<std::string, std::string> conf_details;
    ConferenceMap::iterator iter_conf;

    iter_conf = _conferencemap.find (confID);

    Conference* conf = NULL;

    if (iter_conf != _conferencemap.end()) {

        conf = iter_conf->second;
        conf_details.insert (std::pair<std::string, std::string> ("CONFID",
                             confID));
        conf_details.insert (std::pair<std::string, std::string> ("CONF_STATE",
                             conf->getStateStr()));
    }

    return conf_details;
}

std::vector<std::string> ManagerImpl::getConferenceList (void)
{
    _debug ("ManagerImpl::getConferenceList");
    std::vector<std::string> v;

    ConferenceMap::iterator iter = _conferencemap.begin();

    while (iter != _conferencemap.end()) {
        v.push_back (iter->first);
        iter++;
    }

    return v;
}

std::vector<std::string> ManagerImpl::getParticipantList (
    const std::string& confID)
{

    _debug ("ManagerImpl: Get participant list %s", confID.c_str());
    std::vector<std::string> v;

    ConferenceMap::iterator iter_conf = _conferencemap.find (confID);
    Conference *conf = NULL;

    if (iter_conf != _conferencemap.end())
        conf = iter_conf->second;

    if (conf != NULL) {
        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {

            v.push_back (*iter_participant);

            iter_participant++;
        }
    } else {
        _warn ("Manager: Warning: Did not found conference %s", confID.c_str());
    }

    return v;
}


