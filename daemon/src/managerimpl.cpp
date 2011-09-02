/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include "config.h"

#include "managerimpl.h"

#include "account.h"
#include "dbus/callmanager.h"
#include "global.h"
#include "sip/sipaccount.h"
#include "im/InstantMessaging.h"
#include "iax/iaxaccount.h"
#include "numbercleaner.h"

#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"
#include "audio/sound/dtmf.h"
#include "history/historymanager.h"
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

ManagerImpl::ManagerImpl (void) :
    _hasTriedToRegister (false), _config(), _currentCallId2(),
    _currentCallMutex(), _audiodriver (0),
    _dtmfKey (0), _audioCodecFactory(), _toneMutex(),
    _telephoneTone (0), _audiofile (0), _spkr_volume (0),
    _mic_volume (0), _waitingCall(),
    _waitingCallMutex(), _nbIncomingWaitingCall (0), _path (""),
    _callAccountMap(),
    _callAccountMapMutex(), _callConfigMap(), _accountMap(),
    _history (new HistoryManager), _imModule(new sfl::InstantMessaging)
{
    // initialize random generator for call id
    srand (time (NULL));
}

// never call if we use only the singleton...
ManagerImpl::~ManagerImpl (void)
{
    delete _imModule;
    delete _history;
	delete _audiofile;
}

void ManagerImpl::init (std::string config_file)
{
	if (config_file.empty())
		config_file = getConfigFile();
    _path = config_file;

    _debug ("Manager: configuration file path: %s", _path.c_str());

    Conf::YamlParser *parser;

	try {
		parser = new Conf::YamlParser (_path.c_str());
		parser->serializeEvents();
		parser->composeEvents();
		parser->constructNativeData();
	} catch (Conf::YamlParserException &e) {
		_error ("Manager: %s", e.what());
		delete parser;
		parser = NULL;
	}

	loadAccountMap(parser);
	delete parser;

    initVolume();
    initAudioDriver();

    // Initialize the list of supported audio codecs
    _audioCodecFactory.init();

    audioLayerMutexLock();

    if (_audiodriver) {
        _telephoneTone = new TelephoneTone (preferences.getZoneToneChoice(), _audiodriver->getSampleRate());
        _dtmfKey = new DTMF (8000);
    }

    audioLayerMutexUnlock();

    _history->load_history (preferences.getHistoryLimit());
    _imModule->init();
    registerAccounts();
}

void ManagerImpl::terminate ()
{
    std::vector<std::string> callList(getCallList());
    _debug ("Manager: Hangup %zu remaining call", callList.size());

    std::vector<std::string>::iterator iter;
    for (iter = callList.begin(); iter != callList.end(); ++iter)
        hangupCall (*iter);

    unloadAccountMap();

    delete _dtmfKey;

    delete _telephoneTone;
    _telephoneTone = NULL;

    audioLayerMutexLock();

    delete _audiodriver;
    _audiodriver = NULL;

    _audioCodecFactory.deleteHandlePointer();
    audioLayerMutexUnlock();
}

bool ManagerImpl::isCurrentCall (const std::string& callId)
{
    return _currentCallId2 == callId;
}

bool ManagerImpl::hasCurrentCall ()
{
	return not _currentCallId2.empty();
}

const std::string&
ManagerImpl::getCurrentCallId () const
{
    return _currentCallId2;
}

void ManagerImpl::switchCall (const std::string& id)
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
                                const std::string& call_id, const std::string& to, const std::string& conf_id)
{
    if (call_id.empty()) {
        _debug ("Manager: New outgoing call abbort, missing callid");
        return false;
    }

    // Call ID must be unique
    if (not getAccountFromCall (call_id).empty()) {
        _error ("Manager: Error: Call id already exists in outgoing call");
        return false;
    }

    _debug ("Manager: New outgoing call %s to %s", call_id.c_str(), to.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    std::string prefix;
    if (hookPreference.getNumberEnabled())
        prefix = hookPreference.getNumberAddPrefix();

    std::string to_cleaned(NumberCleaner::clean(to, prefix));

    Call::CallConfiguration callConfig;
    /* Check what kind of call we are dealing with */
    checkCallConfiguration (call_id, to_cleaned, &callConfig);

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        _debug ("Manager: Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conferenceand this and is not a conference participant
        if (!isConference (current_call_id) && !participToConference (current_call_id))
       	    onHoldCall (current_call_id);
        else if (isConference (current_call_id) && !participToConference (call_id))
            detachParticipant (Call::DEFAULT_ID, current_call_id);
    }

    if (callConfig == Call::IPtoIP) {
        _debug ("Manager: Start IP2IP call");
        /* We need to retrieve the sip voiplink instance */
        if (SIPVoIPLink::instance()->SIPNewIpToIpCall(call_id, to_cleaned)) {
            switchCall (call_id);
            return true;
        } else
            callFailure (call_id);

        return false;
    }

    _debug ("Manager: Selecting account %s", account_id.c_str());

    // Is this account exist
    if (!accountExists (account_id)) {
        _error ("Manager: Error: Account doesn't exist in new outgoing call");
        return false;
    }

    if(!associateCallToAccount (call_id, account_id))
    	_warn("Manager: Warning: Could not associate call id %s to account id %s", call_id.c_str(), account_id.c_str());

    try {
        Call *call = getAccountLink(account_id)->newOutgoingCall (call_id, to_cleaned);

        switchCall (call_id);
        call->setConfId(conf_id);
    } catch (const VoipLinkException &e) {
        callFailure (call_id);
        _error ("Manager: %s", e.what());
        return false;
    }

    getMainBuffer()->stateInfo();

    return true;
}

//THREAD=Main : for outgoing Call
bool ManagerImpl::answerCall (const std::string& call_id)
{
    _debug ("Manager: Answer call %s", call_id.c_str());

    // If sflphone is ringing
    stopTone();

    // store the current call id
    std::string current_call_id(getCurrentCallId());

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
            detachParticipant (Call::DEFAULT_ID, current_call_id);
        }
    }

    try {
        getAccountLink (account_id)->answer (call);
    }
    catch (const VoipLinkException &e) {
    	_error("Manager: Error: %s", e.what());
    }

    // if it was waiting, it's waiting no more
    removeWaitingCall (call_id);

    // if we dragged this call into a conference already
    if (participToConference (call_id))
        switchCall (call->getConfId());
    else
        switchCall (call_id);

    // Connect streams
    addStream (call_id);

    getMainBuffer()->stateInfo();

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
    	setRecordingCall(call_id);

    // update call state on client side
    if (audioPreference.getIsAlwaysRecording())
        _dbus.getCallManager()->callStateChanged (call_id, "RECORD");
    else
    	_dbus.getCallManager()->callStateChanged(call_id, "CURRENT");

    return true;
}

//THREAD=Main
void ManagerImpl::hangupCall (const std::string& callId)
{
    _info ("Manager: Hangup call %s", callId.c_str());

    // First stop audio layer if there is no call anymore
    if (getCallList().empty()) {

    	audioLayerMutexLock();

        if(_audiodriver == NULL) {
        	audioLayerMutexUnlock();
        	_error("Manager: Error: Audio layer was not instantiated");
        	return;
        }

        _debug ("Manager: stop audio stream, there is no call remaining");
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }

    // store the current call id
    std::string currentCallId(getCurrentCallId());

    stopTone();

    /* Broadcast a signal over DBus */
    _debug ("Manager: Send DBUS call state change (HUNGUP) for id %s", callId.c_str());
    _dbus.getCallManager()->callStateChanged (callId, "HUNGUP");

    if (not isValidCall(callId) and not getConfigFromCall(callId) == Call::IPtoIP) {
    	_error("Manager: Error: Could not hang up call, call not valid");
        return;
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
        if (!isConference (currentCallId))
            switchCall ("");
    }

    if (getConfigFromCall (callId) == Call::IPtoIP) {
        /* Direct IP to IP call */
        try {
            SIPVoIPLink::instance()->hangup (callId);
        }
        catch (const VoipLinkException &e)
        {
            _error("%s", e.what());
        }
    }
    else {
    	std::string accountId (getAccountFromCall (callId));
        getAccountLink (accountId)->hangup (callId);
        removeCallAccount (callId);
    }

    getMainBuffer()->stateInfo();
}

bool ManagerImpl::hangupConference (const std::string& id)
{
    _debug ("Manager: Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    std::string currentAccountId;

    if (iter_conf != _conferencemap.end()) {
        Conference *conf = iter_conf->second;

        const ParticipantSet &participants = conf->getParticipantList();
        ParticipantSet::const_iterator iter;
        for (iter = participants.begin(); iter != participants.end(); ++iter)
            hangupCall (*iter);
    }

    switchCall ("");

    getMainBuffer()->stateInfo();

    return true;
}


//THREAD=Main
void ManagerImpl::onHoldCall (const std::string& callId)
{
    _debug ("Manager: Put call %s on hold", callId.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    try {
    	if (getConfigFromCall (callId) == Call::IPtoIP) {
    		/* Direct IP to IP call */
            SIPVoIPLink::instance ()-> onhold (callId);
    	}
    	else {
    		/* Classic call, attached to an account */
            std::string account_id(getAccountFromCall (callId));

            if (account_id.empty()) {
                _debug ("Manager: Account ID %s or callid %s doesn't exists in call onHold", account_id.c_str(), callId.c_str());
                return;
            }
    		getAccountLink(account_id)->onhold(callId);
    	}
    }
    catch (const VoipLinkException &e) {
    	_error("Manager: Error: %s", e.what());
    }

    // Unbind calls in main buffer
    removeStream (callId);

    // Remove call from teh queue if it was still there
    removeWaitingCall (callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId)
        switchCall ("");

    _dbus.getCallManager()->callStateChanged (callId, "HOLD");

    getMainBuffer()->stateInfo();
}

//THREAD=Main
void ManagerImpl::offHoldCall (const std::string& callId)
{
    std::string accountId;
    std::string codecName;

    _debug ("Manager: Put call %s off hold", callId.c_str());

    stopTone();

    std::string currentCallId(getCurrentCallId());

    //Place current call on hold if it isn't

    if (hasCurrentCall()) {

        // if this is not a conference and this and is not a conference participant
        if (!isConference (currentCallId) && !participToConference (currentCallId)) {
        	_debug ("Manager: Has current call (%s), put on hold", currentCallId.c_str());
            onHoldCall (currentCallId);
        } else if (isConference (currentCallId) && !participToConference (callId))
            detachParticipant (Call::DEFAULT_ID, currentCallId);
    }

    bool isRec = false;

    /* Direct IP to IP call */
    if (getConfigFromCall (callId) == Call::IPtoIP)
        SIPVoIPLink::instance ()-> offhold (callId);
    else {
        /* Classic call, attached to an account */
        accountId = getAccountFromCall (callId);

        _debug ("Manager: Setting offhold, Account %s, callid %s", accountId.c_str(), callId.c_str());

        Call * call = getAccountLink (accountId)->getCall (callId);
        if (call)
        {
            isRec = call->isRecording();
            getAccountLink(accountId)->offhold(callId);
        }
    }

    _dbus.getCallManager()->callStateChanged (callId, isRec ? "UNHOLD_RECORD" : "UNHOLD_CURRENT");

    if (participToConference (callId)) {
        std::string currentAccountId(getAccountFromCall(callId));
        Call *call = getAccountLink (currentAccountId)->getCall (callId);

        if (call)
            switchCall (call->getConfId());

    } else
        switchCall (callId);

    addStream(callId);

    getMainBuffer()->stateInfo();
}

//THREAD=Main
bool ManagerImpl::transferCall (const std::string& callId, const std::string& to)
{
    bool returnValue = false;;

    _info ("Manager: Transfer call %s", callId.c_str());

    std::string currentCallId = getCurrentCallId();

    if (participToConference(callId)) {
        Conference *conf = getConferenceFromCallID(callId);
        if (conf == NULL)
            _error("Manager: Error: Could not find conference from call id");

        removeParticipant (callId);
        processRemainingParticipant (callId, conf);
    }
    else if (!isConference(currentCallId))
            switchCall("");

    // Direct IP to IP call
    if (getConfigFromCall (callId) == Call::IPtoIP)
        returnValue = SIPVoIPLink::instance ()-> transfer (callId, to);
    else {
        std::string accountid (getAccountFromCall (callId));

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
	_dbus.getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceded ()
{
	_dbus.getCallManager()->transferSucceded();
}

bool ManagerImpl::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    bool returnValue = false;

    _debug("Manager: Attended transfer");

    // Direct IP to IP call
    if (getConfigFromCall (transferID) == Call::IPtoIP)
        returnValue = SIPVoIPLink::instance ()-> attendedTransfer(transferID, targetID);
    else {	// Classic call, attached to an account

        std::string accountid = getAccountFromCall (transferID);

        if (accountid.empty()) {
            _warn ("Manager: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->attendedTransfer (transferID, targetID);
    }

    getMainBuffer()->stateInfo();

    return returnValue;
}

//THREAD=Main : Call:Incoming
bool ManagerImpl::refuseCall (const std::string& id)
{
    std::string accountid;
    bool returnValue;

    _debug ("Manager: Refuse call %s", id.c_str());

    std::string current_call_id = getCurrentCallId();

    stopTone();

    int nbCalls = getCallList().size();

    if (nbCalls <= 1) {
        _debug ("    refuseCall: stop audio stream, there is only %d call(s) remaining", nbCalls);

        audioLayerMutexLock();
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP)
        returnValue = SIPVoIPLink::instance ()-> refuse (id);
    else {
        /* Classic call, attached to an account */
        accountid = getAccountFromCall (id);

        if (accountid.empty()) {
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

        _dbus.getCallManager()->callStateChanged (id, "HUNGUP");
    }

    // Disconnect streams
    removeStream(id);

    getMainBuffer()->stateInfo();

    return returnValue;
}

Conference*
ManagerImpl::createConference (const std::string& id1, const std::string& id2)
{
    typedef std::pair<std::string, Conference*> ConferenceEntry;
    _debug ("Manager: Create conference with call %s and %s", id1.c_str(), id2.c_str());

    Conference* conf = new Conference;

    conf->add (id1);
    conf->add (id2);

    // Add conference to map
    _conferencemap.insert (ConferenceEntry (conf->getConfID(), conf));

    // broadcast a signal over dbus
    _dbus.getCallManager()->conferenceCreated (conf->getConfID());

    return conf;
}

void ManagerImpl::removeConference (const std::string& conference_id)
{
    _debug ("Manager: Remove conference %s", conference_id.c_str());

    _debug ("Manager: number of participant: %d", (int) _conferencemap.size());
    ConferenceMap::iterator iter = _conferencemap.find (conference_id);

    Conference* conf = NULL;

    if (iter != _conferencemap.end())
        conf = iter->second;

    if (conf == NULL) {
        _error ("Manager: Error: Conference not found");
        return;
    }

    // broadcast a signal over dbus
    _dbus.getCallManager()->conferenceRemoved (conference_id);

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getMainBuffer()->unBindAll (Call::DEFAULT_ID);

    const ParticipantSet &participants = conf->getParticipantList();

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getMainBuffer()->bindCallID (*iter_p, Call::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (_conferencemap.erase (conference_id) == 1)
        _debug ("Manager: Conference %s removed successfully", conference_id.c_str());
    else
        _error ("Manager: Error: Cannot remove conference: %s", conference_id.c_str());

    delete conf;
}

Conference*
ManagerImpl::getConferenceFromCallID (const std::string& call_id)
{
    std::string account_id;

    account_id = getAccountFromCall (call_id);
    Call *call = getAccountLink (account_id)->getCall (call_id);

    ConferenceMap::const_iterator iter = _conferencemap.find (call->getConfId());

    if (iter != _conferencemap.end())
        return iter->second;
    else
        return NULL;
}

void ManagerImpl::holdConference (const std::string& id)
{
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    if (iter_conf == _conferencemap.end())
    	return;

    Conference *conf = iter_conf->second;

	bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC ||
				 conf->getState() == Conference::ACTIVE_DETACHED_REC ||
				 conf->getState() == Conference::HOLD_REC;

	const ParticipantSet &participants = conf->getParticipantList();
	ParticipantSet::const_iterator iter;
	for (iter = participants.begin(); iter != participants.end(); ++iter) {
		switchCall (*iter);
		onHoldCall (*iter);
	}

	conf->setState(isRec ? Conference::HOLD_REC : Conference::HOLD);
	_dbus.getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
}

void ManagerImpl::unHoldConference (const std::string& id)
{
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);
    if (iter_conf != _conferencemap.end()) {
        Conference *conf = iter_conf->second;

        bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
					 conf->getState() == Conference::ACTIVE_DETACHED_REC or
					 conf->getState() == Conference::HOLD_REC;

        const ParticipantSet &participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter = participants.begin(); iter!= participants.end(); ++iter) {
            Call *call = getAccountLink(getAccountFromCall(*iter))->getCall(*iter);

            // if one call is currently recording, the conference is in state recording
            isRec |= call->isRecording();

            offHoldCall (*iter);
        }

        conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);
        _dbus.getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
    }
}

bool ManagerImpl::isConference (const std::string& id)
{
    return (_conferencemap.find(id) != _conferencemap.end());
}

bool ManagerImpl::participToConference (const std::string& call_id)
{
    std::string accountId(getAccountFromCall(call_id));
    Call *call = getAccountLink (accountId)->getCall(call_id);

    if (call == NULL) {
    	_error("Manager: Error call is NULL in particip to conference");
        return false;
    }

    if (call->getConfId().empty())
        return false;

    return true;
}

void ManagerImpl::addParticipant (const std::string& callId, const std::string& conferenceId)
{
    _debug ("Manager: Add participant %s to %s", callId.c_str(), conferenceId.c_str());
    ConferenceMap::iterator iter = _conferencemap.find (conferenceId);
    if (iter == _conferencemap.end()) {
    	_error("Manager: Error: Conference id is not valid");
    	return;
    }

    std::string currentAccountId(getAccountFromCall (callId));
    Call *call = getAccountLink (currentAccountId)->getCall (callId);
    if (call == NULL) {
    	_error("Manager: Error: Call id is not valid");
    	return;
    }

    // store the current call id (it will change in offHoldCall or in answerCall)
    std::string current_call_id(getCurrentCallId());

    // detach from prior communication and switch to this conference
    if (current_call_id != callId) {
        if (isConference (current_call_id))
            detachParticipant (Call::DEFAULT_ID, current_call_id);
        else
            onHoldCall (current_call_id);
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

    const ParticipantSet &participants(conf->getParticipantList());
    if (participants.empty())
    	_error("Manager: Error: Participant list is empty for this conference");

    // reset ring buffer for all conference participant
    // flush conference participants only
    for (ParticipantSet::const_iterator iter_p = participants.begin();
            iter_p != participants.end(); ++iter_p)
    	getMainBuffer()->flush(*iter_p);

    getMainBuffer()->flush (Call::DEFAULT_ID);

    // Connect stream
    addStream(callId);
}

void ManagerImpl::addMainParticipant (const std::string& conference_id)
{
    if (hasCurrentCall()) {
        std::string current_call_id(getCurrentCallId());

        if (isConference(current_call_id))
            detachParticipant(Call::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id);
    }

    audioLayerMutexLock();

    ConferenceMap::const_iterator iter = _conferencemap.find (conference_id);
    if (iter != _conferencemap.end()) {
        Conference *conf = iter->second;

        const ParticipantSet &participants = conf->getParticipantList();

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {
            getMainBuffer()->bindCallID(*iter_p, Call::DEFAULT_ID);
            // Reset ringbuffer's readpointers
			getMainBuffer()->flush(*iter_p);
        }

        getMainBuffer()->flush(Call::DEFAULT_ID);

        if (conf->getState() == Conference::ACTIVE_DETACHED)
            conf->setState(Conference::ACTIVE_ATTACHED);
        else if (conf->getState() == Conference::ACTIVE_DETACHED_REC)
        	conf->setState(Conference::ACTIVE_ATTACHED_REC);
        else
        	_warn("Manager: Warning: Invalid conference state while adding main participant");

        _dbus.getCallManager()->conferenceChanged (conference_id, conf->getStateStr());
    }

    audioLayerMutexUnlock();

    switchCall (conference_id);
}

void ManagerImpl::joinParticipant (const std::string& callId1, const std::string& callId2)
{
    _debug ("Manager: Join participants %s, %s", callId1.c_str(), callId2.c_str());

    std::map<std::string, std::string> call1Details = getCallDetails (callId1);
    std::map<std::string, std::string> call2Details = getCallDetails (callId2);

    std::string current_call_id = getCurrentCallId();
    _debug ("Manager: Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ( (current_call_id != callId1) && (current_call_id != callId2)) {

        if (isConference (current_call_id)) {
        	// If currently in a conference
            detachParticipant (Call::DEFAULT_ID, current_call_id);
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
    } else if (call1_state_str == "CURRENT")
        conf->bindParticipant (callId1);
    else if (call1_state_str == "RECORD")
    	conf->bindParticipant(callId1);
    else if (call1_state_str == "INACTIVE") {
        conf->bindParticipant (callId1);
        answerCall (callId1);
    } else
        _warn ("Manager: Call state not recognized");

    // Process call2 according to its state
    std::string call2_state_str = call2Details.find ("CALL_STATE")->second;
    _debug ("Manager: Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

    if (call2_state_str == "HOLD") {
    	conf->bindParticipant (callId2);
        offHoldCall (callId2);
    } else if (call2_state_str == "INCOMING") {
    	conf->bindParticipant (callId2);
        answerCall (callId2);
    } else if (call2_state_str == "CURRENT")
        conf->bindParticipant (callId2);
    else if (call2_state_str == "RECORD")
    	conf->bindParticipant (callId2);
    else if (call2_state_str == "INACTIVE") {
    	conf->bindParticipant (callId2);
        answerCall (callId2);
    } else
        _warn ("Manager: Call state not recognized");

    // Switch current call id to this conference
    switchCall (conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    audioLayerMutexLock();
    if (_audiodriver)
    	conf->setRecordingSmplRate(_audiodriver->getSampleRate());

    audioLayerMutexUnlock();

    getMainBuffer()->stateInfo();
}

void ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        _error("Manager: Error: Participant number must be higher or equal to 2");
        return;
    }

    Conference *conf = new Conference();

    int successCounter = 0;
    for (size_t i = 0; i != participantList.size(); i++) {
		std::string numberaccount(participantList[i]);
		std::string tostr(numberaccount.substr(0, numberaccount.find(",")));
        std::string account(numberaccount.substr(numberaccount.find(",")+1, numberaccount.size()));

		std::string generatedCallID(getNewCallID());

		// Manager methods may behave differently if the call id particip to a conference
		conf->add(generatedCallID);

		switchCall("");

		// Create call
		bool callSuccess = outgoingCall(account, generatedCallID, tostr, conf->getConfID());

			// If not able to create call remove this participant from the conference
		if (!callSuccess)
			conf->remove(generatedCallID);
		else {
			_dbus.getCallManager()->newCallCreated(account, generatedCallID, tostr);
			successCounter++;
		}
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2 ) {
        _conferencemap.insert(std::pair<std::string, Conference *> (conf->getConfID(), conf));
        _dbus.getCallManager()->conferenceCreated (conf->getConfID());

		audioLayerMutexLock();
		if(_audiodriver)
			conf->setRecordingSmplRate(_audiodriver->getSampleRate());

		audioLayerMutexUnlock();

		getMainBuffer()->stateInfo();
    } else
		delete conf;
}

void ManagerImpl::detachParticipant (const std::string& call_id,
                                     const std::string& current_id)
{
    _debug ("Manager: Detach participant %s (current id: %s)", call_id.c_str(), current_id.c_str());

    std::string current_call_id(getCurrentCallId());

    if (call_id != Call::DEFAULT_ID) {

        std::string currentAccountId(getAccountFromCall (call_id));
        Call *call = getAccountLink (currentAccountId)->getCall (call_id);

        if (call == NULL) {
        	_error("Manager: Error: Could not find call %s", call_id.c_str());
        	return;
        }

        // TODO: add conference_id as a second parameter
        ConferenceMap::iterator iter = _conferencemap.find(call->getConfId());

        Conference *conf = getConferenceFromCallID(call_id);
        if (conf == NULL) {
            _error ("Manager: Error: Call is not conferencing, cannot detach");
            return;
        }

        std::map<std::string, std::string> call_details(getCallDetails(call_id));
        std::map<std::string, std::string>::iterator iter_details(call_details.find ("CALL_STATE"));
        if (iter_details == call_details.end()) {
        	_error ("Manager: Error: Could not find CALL_STATE");
        	return;
        }

        if (iter_details->second == "RINGING")
        	removeParticipant (call_id);
        else {
        	onHoldCall (call_id);
        	removeParticipant (call_id);
        	processRemainingParticipant (current_call_id, conf);
        }
    } else {
        _debug ("Manager: Unbind main participant from conference %d");
        getMainBuffer()->unBindAll (Call::DEFAULT_ID);

        if (!isConference(current_call_id)) {
        	_error("Manager: Warning: Current call id (%s) is not a conference", current_call_id.c_str());
        	return;
        }

        ConferenceMap::iterator iter = _conferencemap.find (current_call_id);
        Conference *conf = iter->second;

        if (conf == NULL) {
        	_debug("Manager: Error: Conference is NULL");
        	return;
        }

        if (conf->getState() == Conference::ACTIVE_ATTACHED)
        	conf->setState(Conference::ACTIVE_DETACHED);
        else if(conf->getState() == Conference::ACTIVE_ATTACHED_REC)
        	conf->setState(Conference::ACTIVE_DETACHED_REC);
        else
        	_warn("Manager: Warning: Undefined behavior, invalid conference state in detach participant");

        _dbus.getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());

        switchCall ("");
    }
}

void ManagerImpl::removeParticipant (const std::string& call_id)
{
    _debug ("Manager: Remove participant %s", call_id.c_str());

    // this call is no more a conference participant
    const std::string currentAccountId(getAccountFromCall (call_id));
    Call *call = getAccountLink (currentAccountId)->getCall (call_id);

    ConferenceMap conf_map = _conferencemap;
    ConferenceMap::iterator iter = conf_map.find (call->getConfId());

    if (iter == conf_map.end()) {
        _error ("Manager: Error: No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    Conference *conf = iter->second;

    _debug ("Manager: Remove participant %s", call_id.c_str());
    conf->remove(call_id);
    call->setConfId ("");

    removeStream(call_id);
    getMainBuffer()->stateInfo();
}

void ManagerImpl::processRemainingParticipant (const std::string &current_call_id, Conference *conf)
{
    const ParticipantSet &participants = conf->getParticipantList();
    size_t n = participants.size();
    _debug ("Manager: Process remaining %d participant(s) from conference %s",
			n, conf->getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end();
                ++iter_p)
            getMainBuffer()->flush (*iter_p);

        getMainBuffer()->flush (Call::DEFAULT_ID);
    } else if (n == 1) {
        ParticipantSet::iterator iter_participant = participants.begin();

        // bind main participant to remaining conference call
        if (iter_participant != participants.end()) {

            // this call is no more a conference participant
            std::string currentAccountId(getAccountFromCall (*iter_participant));
            Call *call = getAccountLink(currentAccountId)->getCall(*iter_participant);
            call->setConfId ("");

            // if we are not listening to this conference
            if (current_call_id != conf->getConfID())
                onHoldCall (call->getCallId());
            else
                switchCall (*iter_participant);
        }

        removeConference (conf->getConfID());
    } else {
        _debug ("Manager: No remaining participant, remove conference");
        removeConference (conf->getConfID());
        switchCall ("");
    }
}

void ManagerImpl::joinConference (const std::string& conf_id1,
                                  const std::string& conf_id2)
{
    ConferenceMap::iterator iter(_conferencemap.find(conf_id1));

    if (iter == _conferencemap.end()) {
        _error ("Manager: Error: Not a valid conference ID: %s", conf_id1.c_str());
        return;
    }

    Conference *conf1 = iter->second;

    if (_conferencemap.find (conf_id2) != _conferencemap.end()) {
        _error ("Manager: Error: Not a valid conference ID: %s", conf_id2.c_str());
        return;
    }

    const ParticipantSet &participants(conf1->getParticipantList());

    for (ParticipantSet::const_iterator iter_p = participants.begin();
            iter_p != participants.end(); ++iter_p) {
        detachParticipant (*iter_p, "");
        addParticipant (*iter_p, conf_id2);
    }
}

void ManagerImpl::addStream (const std::string& call_id)
{
    _debug ("Manager: Add audio stream %s", call_id.c_str());

    std::string currentAccountId(getAccountFromCall(call_id));
    Call *call(getAccountLink(currentAccountId)->getCall(call_id));

    if (participToConference(call_id)) {
        _debug ("Manager: Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

        if (iter != _conferencemap.end()) {
            Conference* conf = iter->second;

            conf->bindParticipant (call_id);

            const ParticipantSet &participants(conf->getParticipantList());

            // reset ring buffer for all conference participant
            for (ParticipantSet::const_iterator iter_p = participants.begin();
                    iter_p != participants.end(); ++iter_p)
                getMainBuffer()->flush (*iter_p);

            getMainBuffer()->flush (Call::DEFAULT_ID);
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

void ManagerImpl::removeStream (const std::string& call_id)
{
    _debug ("Manager: Remove audio stream %s", call_id.c_str());
    getMainBuffer()->unBindAll (call_id);
    getMainBuffer()->stateInfo();
}

//THREAD=Main
void ManagerImpl::saveConfig (void)
{
    _debug ("Manager: Saving Configuration to XDG directory %s", _path.c_str());
    audioPreference.setVolumemic (getMicVolume());
    audioPreference.setVolumespkr (getSpkrVolume());

    try {
        // emitter = new Conf::YamlEmitter("sequenceEmitter.yml");
        emitter = new Conf::YamlEmitter (_path.c_str());

        for (AccountMap::iterator iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        	// Skip the "" account ID (which refer to the IP2IP account)
            if (iter->first.empty())
                continue;
            else
                iter->second->serialize (emitter);
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
}

//THREAD=Main
bool ManagerImpl::sendDtmf (const std::string& id, char code)
{
    std::string accountid(getAccountFromCall(id));
    playDtmf (code);
    return getAccountLink (accountid)->carryingDTMFdigits (id, code);
}

//THREAD=Main | VoIPLink
bool ManagerImpl::playDtmf (char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        _debug ("Manager: playDtmf: Do not have to play a tone...");
        return false;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        _debug ("Manager: playDtmf: Pulse length is not set...");
        return false;
    }

    audioLayerMutexLock();

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

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
    int size = (int) ( (pulselen * (float) _audiodriver->getSampleRate()) / 1000);

    // this buffer is for mono
    // TODO <-- this should be global and hide if same size
    SFLDataFormat *buf = new SFLDataFormat[size];

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

    // TODO Cache the DTMF

    delete[] buf;
    return true;
}

// Multi-thread
bool ManagerImpl::incomingCallWaiting ()
{
    return _nbIncomingWaitingCall > 0;
}

void ManagerImpl::addWaitingCall (const std::string& id)
{
    _info ("Manager: Add waiting call %s (%d calls)", id.c_str(), _nbIncomingWaitingCall);

    ost::MutexLock m(_waitingCallMutex);
    _waitingCall.insert (id);
    _nbIncomingWaitingCall++;
}

void ManagerImpl::removeWaitingCall (const std::string& id)
{
    _info ("Manager: Remove waiting call %s (%d calls)", id.c_str(), _nbIncomingWaitingCall);

    ost::MutexLock m(_waitingCallMutex);
    // should return more than 1 if it erase a call

    if (_waitingCall.erase (id))
        _nbIncomingWaitingCall--;
}

bool ManagerImpl::isWaitingCall (const std::string& id)
{
    return _waitingCall.find(id) != _waitingCall.end();
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
bool ManagerImpl::incomingCall (Call* call, const std::string& accountId)
{
    assert(call);
    stopTone();

    _debug ("Manager: Incoming call %s for account %s", call->getCallId().data(), accountId.c_str());

    associateCallToAccount (call->getCallId(), accountId);

    // If account is null it is an ip to ip call
    if (accountId.empty())
        associateConfigToCall (call->getCallId(), Call::IPtoIP);
    else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber(call->getPeerNumber());

        size_t startIndex = peerNumber.find ("sip:");

        if (startIndex != std::string::npos) {
            std::string strippedPeerNumber = peerNumber.substr (startIndex + 4);
            call->setPeerNumber (strippedPeerNumber);
        }
    }

    if (!hasCurrentCall()) {
        _debug ("Manager: Has no current call start ringing");
        call->setConnectionState (Call::Ringing);
        ringtone (accountId);

    } else
        _debug ("Manager: has current call, beep in current audio stream");

    addWaitingCall (call->getCallId());

    std::string from(call->getPeerName());
    std::string number(call->getPeerNumber());
    std::string display_name(call->getDisplayName());

    if (not from.empty() and not number.empty()) {
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

    std::string display(display_name);
    display.append(" ");
    display.append(from);

    _dbus.getCallManager()->incomingCall(accountId, call->getCallId(), display);
    return true;
}


//THREAD=VoIP
void ManagerImpl::incomingMessage (const std::string& callID,
                                   const std::string& from,
                                   const std::string& message)
{
    if (participToConference (callID)) {
        Conference *conf = getConferenceFromCallID (callID);

        const ParticipantSet &participants = conf->getParticipantList();
        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            if (*iter_p == callID)
                continue;

            std::string accountId(getAccountFromCall(*iter_p));

            _debug ("Manager: Send message to %s, (%s)", (*iter_p).c_str(), accountId.c_str());

            Account *account = getAccount (accountId);

            if (!account) {
                _error("Manager: Failed to get account while sending instant message");
                return;
            }
            account->getVoIPLink()->sendTextMessage (_imModule, callID, message, from);
        }

        // in case of a conference we must notify client using conference id
        _dbus.getCallManager()->incomingMessage (conf->getConfID(), from, message);

    } else
    	_dbus.getCallManager()->incomingMessage (callID, from, message);
}


//THREAD=VoIP
bool ManagerImpl::sendTextMessage (const std::string& callID, const std::string& message, const std::string& from)
{
    if (isConference (callID)) {
        _debug ("Manager: Is a conference, send instant message to everyone");
        ConferenceMap::iterator it = _conferencemap.find (callID);

        if (it == _conferencemap.end())
            return false;

        Conference *conf = it->second;

        if (!conf)
            return false;

        const ParticipantSet participants = conf->getParticipantList();

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            std::string accountId = getAccountFromCall (*iter_p);

            Account *account = getAccount (accountId);

            if (!account) {
                _debug ("Manager: Failed to get account while sending instant message");
                return false;
            }

			account->getVoIPLink()->sendTextMessage (_imModule, *iter_p, message, from);
        }

        return true;
    }

    if (participToConference (callID)) {
        _debug ("Manager: Particip to a conference, send instant message to everyone");

        Conference *conf = getConferenceFromCallID (callID);

        if (!conf)
            return false;

        const ParticipantSet participants(conf->getParticipantList());
        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            const std::string accountId(getAccountFromCall(*iter_p));

            const Account *account = getAccount (accountId);

            if (!account) {
                _debug ("Manager: Failed to get account while sending instant message");
                return false;
            }

			account->getVoIPLink()->sendTextMessage (_imModule, *iter_p, message, from);
        }
    } else {
        const Account *account = getAccount (getAccountFromCall(callID));

        if (!account) {
            _debug ("Manager: Failed to get account while sending instant message");
            return false;
        }

        account->getVoIPLink()->sendTextMessage (_imModule, callID, message, from);
    }

    return true;
}

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall (const std::string& id)
{
    _debug ("Manager: Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(id))
        stopTone();

    // Connect audio streams
    addStream(id);

    audioLayerMutexLock();
    _audiodriver->flushMain();
    _audiodriver->flushUrgent();
    audioLayerMutexUnlock();

    if (audioPreference.getIsAlwaysRecording()) {
    	setRecordingCall(id);
    	_dbus.getCallManager()->callStateChanged (id, "RECORD");
    }
    else
    	_dbus.getCallManager()->callStateChanged(id, "CURRENT");
}

//THREAD=VoIP Call=Outgoing
void ManagerImpl::peerRingingCall (const std::string& id)
{
    _debug ("Manager: Peer call %s ringing", id.c_str());

    if (isCurrentCall(id))
        ringback();

    _dbus.getCallManager()->callStateChanged (id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall (const std::string& call_id)
{
    _debug ("Manager: Peer hungup call %s", call_id.c_str());
    if (participToConference (call_id)) {
        Conference *conf = getConferenceFromCallID (call_id);

        if (conf != NULL) {
            removeParticipant (call_id);
            processRemainingParticipant (getCurrentCallId(), conf);
        }
    } else {
        if (isCurrentCall (call_id)) {
            stopTone();
            switchCall("");
        }
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP)
        SIPVoIPLink::instance ()->hangup (call_id);
    else {
        const std::string account_id(getAccountFromCall (call_id));
        getAccountLink(account_id)->peerHungup(call_id);
    }

    /* Broadcast a signal over DBus */
    _dbus.getCallManager()->callStateChanged (call_id, "HUNGUP");

    removeWaitingCall (call_id);
    removeCallAccount (call_id);
    removeStream (call_id);

    if (getCallList().empty()) {
        _debug ("Manager: Stop audio stream, ther is only %d call(s) remaining", getCallList().size());

        audioLayerMutexLock();
        _audiodriver->stopStream();
        audioLayerMutexUnlock();
    }
}

//THREAD=VoIP
void ManagerImpl::callBusy (const std::string& id)
{
    _debug ("Manager: Call %s busy", id.c_str());
    _dbus.getCallManager()->callStateChanged (id, "BUSY");

    if (isCurrentCall (id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    removeCallAccount (id);
    removeWaitingCall (id);
}

//THREAD=VoIP
void ManagerImpl::callFailure (const std::string& call_id)
{
	_dbus.getCallManager()->callStateChanged (call_id, "FAILURE");
    if (isCurrentCall (call_id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    if (participToConference (call_id)) {
        _debug ("Manager: Call %s participating to a conference failed", call_id.c_str());
        Conference *conf = getConferenceFromCallID (call_id);

        if (conf == NULL) {
        	_error("Manager: Could not retreive conference from call id %s", call_id.c_str());
        	return;
        }

        // remove this participant
        removeParticipant (call_id);
        processRemainingParticipant (getCurrentCallId(), conf);
    }

    removeCallAccount (call_id);
    removeWaitingCall (call_id);
}

//THREAD=VoIP
void ManagerImpl::startVoiceMessageNotification (const std::string& accountId,
        int nb_msg)
{
	_dbus.getCallManager()->voiceMailNotify (accountId, nb_msg);
}

void ManagerImpl::connectionStatusNotification ()
{
    if (_dbus.isConnected())
    	_dbus.getConfigurationManager()->accountsChanged();
}

/**
 * Multi Thread
 */
bool ManagerImpl::playATone (Tone::TONEID toneId)
{
    if (not voipPreferences.getPlayTones())
        return false;

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
    if (not voipPreferences.getPlayTones())
        return;

    _toneMutex.enterMutex();

    if (_telephoneTone != NULL)
        _telephoneTone->setCurrentTone (Tone::TONE_NULL);

    if (_audiofile) {
		std::string filepath(_audiofile->getFilePath());
		_dbus.getCallManager()->recordPlaybackStopped(filepath);
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
    playATone(Tone::TONE_DIALTONE);
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
    Account *account = getAccount (accountID);
    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone");
        return;
    }

    if (!account->getRingtoneEnabled()) {
    	ringback();
    	return;
    }

    std::string ringchoice = account->getRingtonePath();
	if (ringchoice.find (DIR_SEPARATOR_STR) == std::string::npos) {
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

	int samplerate = _audiodriver->getSampleRate();

	audioLayerMutexUnlock();

	_toneMutex.enterMutex();

	if (_audiofile) {
		_dbus.getCallManager()->recordPlaybackStopped(_audiofile->getFilePath());
		delete _audiofile;
		_audiofile = NULL;
	}

	try {
		if (ringchoice.find (".wav") != std::string::npos)
			_audiofile = new WaveFile(ringchoice, samplerate);
		else {
			sfl::Codec *codec;
			if (ringchoice.find (".ul") != std::string::npos || ringchoice.find (".au") != std::string::npos)
			     codec = _audioCodecFactory.getCodec(PAYLOAD_CODEC_ULAW);
			else
		        throw AudioFileException("Couldn't guess an appropriate decoder");
			_audiofile = new RawFile(ringchoice, static_cast<sfl::AudioCodec *>(codec), samplerate);
		}
	}
	catch (AudioFileException &e) {
		_error("Manager: Exception: %s", e.what());
	}

	_toneMutex.leaveMutex();

	audioLayerMutexLock();
	// start audio if not started AND flush all buffers (main and urgent)
	_audiodriver->startStream();
	audioLayerMutexUnlock();
}

AudioLoop*
ManagerImpl::getTelephoneTone ()
{
    if (_telephoneTone) {
        ost::MutexLock m (_toneMutex);
        return _telephoneTone->getCurrentTone();
    }
    else
        return NULL;
}

AudioLoop*
ManagerImpl::getTelephoneFile ()
{
    ost::MutexLock m (_toneMutex);

    return _audiofile;
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 */
std::string ManagerImpl::getConfigFile (void) const
{
	std::string configdir = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".config"
                 + DIR_SEPARATOR_STR + PROGDIR;

    if (XDG_CONFIG_HOME != NULL) {
        std::string xdg_env = std::string (XDG_CONFIG_HOME);
        if (not xdg_env.empty())
        	configdir = xdg_env;
    }

    if (mkdir (configdir.data(), 0700) != 0) {
        // If directory	creation failed
        if (errno != EEXIST)
            _debug ("Cannot create directory: %m");
    }

    return configdir + DIR_SEPARATOR_STR + PROGNAME + ".yml";
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

std::string ManagerImpl::serialize (const std::vector<std::string> &v)
{
    std::string res;

    for (std::vector<std::string>::const_iterator iter = v.begin(); iter != v.end(); ++iter)
        res += *iter + "/";

    return res;
}

std::string ManagerImpl::getCurrentCodecName (const std::string& id)
{
    std::string accountid = getAccountFromCall (id);
    VoIPLink* link = getAccountLink (accountid);
    Call* call = link->getCall (id);
    std::string codecName;

    if (call) {
        Call::CallState state = call->getState();
        if (state == Call::Active or state == Call::Conferencing)
            codecName = link->getCurrentCodecName(call);
    }

    return codecName;
}

/**
 * Set input audio plugin
 */
void ManagerImpl::setAudioPlugin (const std::string& audioPlugin)
{
	audioLayerMutexLock();

    audioPreference.setPlugin (audioPlugin);

    AlsaLayer *alsa = dynamic_cast<AlsaLayer*>(_audiodriver);
    if (!alsa) {
        _error("Can't find alsa device");
        audioLayerMutexUnlock();
        return ;
    }

    bool wasStarted = _audiodriver->isStarted();

    // Recreate audio driver with new settings
    delete _audiodriver;
    _audiodriver = new AlsaLayer;
    if (wasStarted)
        _audiodriver->startStream();

    audioLayerMutexUnlock();
}

/**
 * Set audio output device
 */
void ManagerImpl::setAudioDevice (const int index, int streamType)
{
    audioLayerMutexLock();

    AlsaLayer *alsaLayer = dynamic_cast<AlsaLayer*>(_audiodriver);
    if (!alsaLayer) {
        _error("Can't find alsa device");
        audioLayerMutexUnlock();
        return ;
    }

    bool wasStarted = _audiodriver->isStarted();

    switch (streamType) {
        case SFL_PCM_PLAYBACK:
            audioPreference.setCardout (index);
            break;
        case SFL_PCM_CAPTURE:
            audioPreference.setCardin (index);
            break;
        case SFL_PCM_RINGTONE:
            audioPreference.setCardring (index);
            break;
        default:
            break;
    }

    // Recreate audio driver with new settings
    delete _audiodriver;
    _audiodriver = new AlsaLayer;

    if (wasStarted)
        _audiodriver->startStream();

    audioLayerMutexUnlock();
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string> ManagerImpl::getAudioOutputDeviceList (void)
{
    std::vector<std::string> devices;

    audioLayerMutexLock();

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer*>(_audiodriver);

    if (alsalayer)
        devices = alsalayer->getSoundCardsInfo(SFL_PCM_PLAYBACK);

    audioLayerMutexUnlock();

    return devices;
}


/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList (void)
{
    std::vector<std::string> devices;

    audioLayerMutexLock();

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer *> (_audiodriver);
    if (alsalayer)
    	devices = alsalayer->getSoundCardsInfo (SFL_PCM_CAPTURE);

    audioLayerMutexUnlock();

    return devices;
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex ()
{
    audioLayerMutexLock();

    std::vector<std::string> v;

    AlsaLayer *alsa = dynamic_cast<AlsaLayer*>(_audiodriver);
    if (alsa) {
		std::stringstream ssi, sso, ssr;
		sso << alsa->getIndexOut();
		v.push_back (sso.str());
		ssi << alsa->getIndexIn();
		v.push_back (ssi.str());
		ssr << alsa->getIndexRing();
		v.push_back (ssr.str());
    }

    audioLayerMutexUnlock();

    return v;
}

int ManagerImpl::isRingtoneEnabled (const std::string& id)
{
    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
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

std::string ManagerImpl::getRecordPath (void) const
{
    return audioPreference.getRecordpath();
}

void ManagerImpl::setRecordPath (const std::string& recPath)
{
    _debug ("Manager: Set record path %s", recPath.c_str());
    audioPreference.setRecordpath (recPath);
}

bool ManagerImpl::getIsAlwaysRecording(void) const
{
	return audioPreference.getIsAlwaysRecording();
}

void ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
	return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

void ManagerImpl::setRecordingCall (const std::string& id)
{
    Recordable* rec = NULL;

    if (not isConference (id)) {
        _debug ("Manager: Set recording for call %s", id.c_str());
        std::string accountid(getAccountFromCall(id));
        rec = getAccountLink(accountid)->getCall(id);
    } else {
        _debug ("Manager: Set recording for conference %s", id.c_str());
        ConferenceMap::const_iterator it(_conferencemap.find(id));
        Conference *conf = it->second;
        if (rec->isRecording())
        	conf->setState(Conference::ACTIVE_ATTACHED);
        else
        	conf->setState(Conference::ACTIVE_ATTACHED_REC);

        rec = conf;
    }

    if (rec == NULL) {
        _error("Manager: Error: Could not find recordable instance %s", id.c_str());
        return;
    }

    rec->setRecording();
	_dbus.getCallManager()->recordPlaybackFilepath(id, rec->getFileName());
}

bool ManagerImpl::isRecording (const std::string& id)
{
    const std::string accountid(getAccountFromCall (id));
    Recordable* rec = getAccountLink (accountid)->getCall (id);
    return rec and rec->isRecording();
}

bool ManagerImpl::startRecordedFilePlayback(const std::string& filepath)
{
    _debug("Manager: Start recorded file playback %s", filepath.c_str());

    audioLayerMutexLock();

    if (!_audiodriver) {
        _error("Manager: Error: No audio layer in start recorded file playback");
        audioLayerMutexUnlock();
        return false;
    }

    int sampleRate = _audiodriver->getSampleRate();

    audioLayerMutexUnlock();

    _toneMutex.enterMutex();

    if (_audiofile) {
    	_dbus.getCallManager()->recordPlaybackStopped(_audiofile->getFilePath());
		delete _audiofile;
		_audiofile = NULL;
    }

    try {
        _audiofile = new WaveFile(filepath, sampleRate);
    }
    catch (const AudioFileException &e) {
        _error("Manager: Exception: %s", e.what());
    }

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
    delete _audiofile;
	_audiofile = NULL;
    _toneMutex.leaveMutex();
}

void ManagerImpl::setHistoryLimit (int days)
{
    _debug ("Manager: Set history limit");
    preferences.setHistoryLimit (days);
    saveConfig();
}

int ManagerImpl::getHistoryLimit (void) const
{
    return preferences.getHistoryLimit();
}

int32_t ManagerImpl::getMailNotify (void) const
{
    return preferences.getNotifyMails();
}

void ManagerImpl::setMailNotify (void)
{
    _debug ("Manager: Set mail notify");
    preferences.getNotifyMails() ? preferences.setNotifyMails (true) : preferences.setNotifyMails (false);
    saveConfig();
}

void ManagerImpl::setAudioManager (const std::string &api)
{
    audioLayerMutexLock();

    if (!_audiodriver) {
    	audioLayerMutexUnlock();
        return;
    }

    if (api == preferences.getAudioApi()) {
        _debug ("Manager: Audio manager chosen already in use. No changes made. ");
        audioLayerMutexUnlock();
        return;
    }

    audioLayerMutexUnlock();

    switchAudioManager();

    saveConfig();
}

std::string ManagerImpl::getAudioManager (void) const
{
    return preferences.getAudioApi();
}


int ManagerImpl::getAudioDeviceIndex (const std::string &name)
{
    int soundCardIndex = 0;

    audioLayerMutexLock();

    if (_audiodriver == NULL) {
    	_error("Manager: Error: Audio layer not initialized");
    	audioLayerMutexUnlock();
    	return soundCardIndex;
    }

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer *> (_audiodriver);

    if (alsalayer)
        soundCardIndex = alsalayer -> soundCardGetIndex (name);

    audioLayerMutexUnlock();

    return soundCardIndex;
}

std::string ManagerImpl::getCurrentAudioOutputPlugin (void) const
{
    return audioPreference.getPlugin();
}


std::string ManagerImpl::getNoiseSuppressState (void) const
{
    return audioPreference.getNoiseReduce() ? "enabled" : "disabled";
}

void ManagerImpl::setNoiseSuppressState (const std::string &state)
{
    audioPreference.setNoiseReduce (state == "enabled");
}

std::string ManagerImpl::getEchoCancelState() const
{
	return audioPreference.getEchoCancel() ? "enabled" : "disabled";
}

void ManagerImpl::setEchoCancelState(const std::string &state)
{
	audioPreference.setEchoCancel(state == "enabled");
}

int ManagerImpl::getEchoCancelTailLength(void) const
{
	return audioPreference.getEchoCancelTailLength();
}

void ManagerImpl::setEchoCancelTailLength(int length)
{
	audioPreference.setEchoCancelTailLength(length);
}

int ManagerImpl::getEchoCancelDelay(void) const
{
	return audioPreference.getEchoCancelDelay();
}

void ManagerImpl::setEchoCancelDelay(int delay)
{
	audioPreference.setEchoCancelDelay(delay);
}

/**
 * Initialization: Main Thread
 */
void ManagerImpl::initAudioDriver (void)
{
    audioLayerMutexLock();

    /* Retrieve the global devices info from the user config */
    int numCardIn = audioPreference.getCardin();
    int numCardOut = audioPreference.getCardout();
    int numCardRing = audioPreference.getCardring();

    if (!AlsaLayer::soundCardIndexExist (numCardIn, SFL_PCM_CAPTURE)) {
        _debug (" Card with index %d doesn't exist or cannot capture. Switch to 0.", numCardIn);
        numCardIn = ALSA_DFT_CARD_ID;
        audioPreference.setCardin (ALSA_DFT_CARD_ID);
    }

    if (!AlsaLayer::soundCardIndexExist (numCardOut, SFL_PCM_PLAYBACK)) {
        _debug (" Card with index %d doesn't exist or cannot playback. Switch to 0.", numCardOut);
        numCardOut = ALSA_DFT_CARD_ID;
        audioPreference.setCardout (ALSA_DFT_CARD_ID);
    }

    if (!AlsaLayer::soundCardIndexExist (numCardRing, SFL_PCM_RINGTONE)) {
        _debug (" Card with index %d doesn't exist or cannot ringtone. Switch to 0.", numCardRing);
        numCardRing = ALSA_DFT_CARD_ID;
        audioPreference.setCardring (ALSA_DFT_CARD_ID);
    }

    if (preferences.getAudioApi() == PULSEAUDIO_API_STR and system("ps -C pulseaudio") == 0)
		_audiodriver = new PulseLayer;
	else {
		preferences.setAudioApi(ALSA_API_STR);
        _audiodriver = new AlsaLayer;
    }

    audioLayerMutexUnlock();
}

void ManagerImpl::switchAudioManager (void)
{
    audioLayerMutexLock();

    bool wasStarted = _audiodriver->isStarted();
    delete _audiodriver;

    if (preferences.getAudioApi() == PULSEAUDIO_API_STR) {
    	_audiodriver = new AlsaLayer;
        preferences.setAudioApi(ALSA_API_STR);
    }
    else {
    	_audiodriver = new PulseLayer;
        preferences.setAudioApi(PULSEAUDIO_API_STR);
    }

    if (wasStarted)
        _audiodriver->startStream();

    audioLayerMutexUnlock();
}

void ManagerImpl::audioSamplingRateChanged (int samplerate)
{
    audioLayerMutexLock();

    if (!_audiodriver) {
    	_debug("Manager: No Audio driver initialized");
    	audioLayerMutexUnlock();
        return;
    }

    // Only modify internal sampling rate if new sampling rate is higher
    int currentSamplerate = _mainBuffer.getInternalSamplingRate();
    if (currentSamplerate >= samplerate) {
    	_debug("Manager: No need to update audio layer sampling rate");
    	audioLayerMutexUnlock();
    	return;
    }
    else
        _debug ("Manager: Audio sampling rate changed: %d -> %d", currentSamplerate, samplerate);

    bool wasActive = _audiodriver->isStarted();

    _mainBuffer.setInternalSamplingRate(samplerate);

    delete _audiodriver;
    if (preferences.getAudioApi() == PULSEAUDIO_API_STR)
    	_audiodriver = new PulseLayer;
    else
    	_audiodriver = new AlsaLayer;

    unsigned int sampleRate = _audiodriver->getSampleRate();

    delete _telephoneTone;
    _telephoneTone = new TelephoneTone(preferences.getZoneToneChoice(), sampleRate);

    delete _dtmfKey;
    _dtmfKey = new DTMF (sampleRate);

    if (wasActive)
        _audiodriver->startStream();

    audioLayerMutexUnlock();
}

/**
 * Init the volume for speakers/micro from 0 to 100 value
 * Initialization: Main Thread
 */
void ManagerImpl::initVolume ()
{
    setSpkrVolume (audioPreference.getVolumespkr());
    setMicVolume (audioPreference.getVolumemic());
}

void ManagerImpl::setSpkrVolume (unsigned short spkr_vol)
{
    _spkr_volume = spkr_vol;
}

void ManagerImpl::setMicVolume (unsigned short mic_vol)
{
    _mic_volume = mic_vol;
}

int ManagerImpl::getLocalIp2IpPort (void) const
{
    return preferences.getPortNum();
}


//THREAD=Main
bool ManagerImpl::getConfig (const std::string& section,
                             const std::string& name, TokenList& arg) const
{
    return _config.getConfigTreeItemToken (section, name, arg);
}

//THREAD=Main
// throw an Conf::ConfigTreeItemException if not found
int ManagerImpl::getConfigInt (const std::string& section,
                               const std::string& name) const
{
    try {
        return _config.getConfigTreeItemIntValue (section, name);
    } catch (const Conf::ConfigTreeItemException& e) {
        throw;
    }

    return 0;
}

bool ManagerImpl::getConfigBool (const std::string& section,
                                 const std::string& name) const
{
    try {
        return _config.getConfigTreeItemValue (section, name) == Conf::TRUE_STR;
    } catch (const Conf::ConfigTreeItemException& e) {
        throw;
    }

    return false;
}

//THREAD=Main
std::string ManagerImpl::getConfigString (const std::string& section,
        const std::string& name) const
{
    try {
        return _config.getConfigTreeItemValue (section, name);
    } catch (const Conf::ConfigTreeItemException& e) {
        throw;
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

std::vector<std::string> ManagerImpl::getAccountList () const
{
    using std::vector;
    using std::string;
    vector<string> account_order(loadAccountOrder());

    // The IP2IP profile is always available, and first in the list

    vector<string> v;

    AccountMap::const_iterator ip2ip_iter = _accountMap.find (IP2IP_PROFILE);
    if (ip2ip_iter->second)
        v.push_back (ip2ip_iter->second->getAccountID());
    else
        _error ("Manager: could not find IP2IP profile in getAccount list");

    // If no order has been set, load the default one
    // ie according to the creation date.

    if (account_order.empty()) {
        _debug ("Manager: account order is empty");
        for (AccountMap::const_iterator iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
            if (iter->second != NULL and iter->first != IP2IP_PROFILE and not iter->first.empty()) {
                _debug ("PUSHING BACK %s", iter->first.c_str());
                v.push_back (iter->second->getAccountID());
            }
        }
    }
    else {
        // otherwise, load the custom one
        // ie according to the saved order
        _debug ("Manager: Load account list according to preferences");

        for (vector<string>::const_iterator iter = account_order.begin(); iter != account_order.end(); ++iter) {
            // This account has not been loaded, so we ignore it
            AccountMap::const_iterator account_iter = _accountMap.find (*iter);
            if (account_iter != _accountMap.end()) {
                if (account_iter->second and (account_iter->first not_eq IP2IP_PROFILE) and not account_iter->first.empty()) {
                    // If the account is valid
                    v.push_back (account_iter->second->getAccountID());
                }
            }
        }
    }

    return v;
}

std::map<std::string, std::string> ManagerImpl::getAccountDetails (
    const std::string& accountID) const
{
    // Default account used to get default parameters if requested by client (to build new account)
    static const SIPAccount DEFAULT_ACCOUNT("default");

    if (accountID.empty()) {
        _debug ("Manager: Returning default account settings");
        return DEFAULT_ACCOUNT.getAccountDetails();
    }

    AccountMap::const_iterator iter = _accountMap.find(accountID);
    Account * account = 0;
    if (iter != _accountMap.end())
        account = iter->second;

    if (account)
        return account->getAccountDetails();
    else {
        _debug ("Manager: Get account details on a non-existing accountID %s. Returning default", accountID.c_str());
        return DEFAULT_ACCOUNT.getAccountDetails();
    }
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

    if (account->isEnabled())
        account->registerVoIPLink();
    else
        account->unregisterVoIPLink();

    // Update account details to the client side
    _dbus.getConfigurationManager()->accountsChanged();
}

std::string ManagerImpl::addAccount (
    const std::map<std::string, std::string>& details)
{
    /** @todo Deal with both the _accountMap and the Configuration */
    std::string accountType, account_list;
    std::stringstream accountID;

    accountID << "Account:" << time (NULL);
    std::string newAccountID(accountID.str());

    // Get the type
    accountType = (*details.find (CONFIG_ACCOUNT_TYPE)).second;

    _debug ("Manager: Adding account %s", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    Account* newAccount = NULL;
    if (accountType == "SIP") {
        newAccount = new SIPAccount(newAccountID);
    } else if (accountType == "IAX") {
        newAccount = new IAXAccount(newAccountID);
    } else {
        _error ("Unknown %s param when calling addAccount(): %s",
                CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    _accountMap[newAccountID] = newAccount;

    newAccount->setAccountDetails (details);

    // Add the newly created account in the account order list
    account_list = preferences.getAccountOrder();

    if (not account_list.empty()) {
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

    newAccount->registerVoIPLink();

    saveConfig();

    if (_dbus.isConnected())
        _dbus.getConfigurationManager()->accountsChanged();

    return accountID.str();
}

void ManagerImpl::removeAccount (const std::string& accountID)
{
    // Get it down and dying
    Account* remAccount = getAccount (accountID);

    if (remAccount != NULL) {
        remAccount->unregisterVoIPLink();
        _accountMap.erase (accountID);
        // http://projects.savoirfairelinux.net/issues/show/2355
        // delete remAccount;
    }

    _config.removeSection (accountID);

    saveConfig();

    _debug ("REMOVE ACCOUNT");

    if (_dbus.isConnected())
        _dbus.getConfigurationManager()->accountsChanged();
}

// ACCOUNT handling
bool ManagerImpl::associateCallToAccount (const std::string& callID,
        const std::string& accountID)
{
    if (getAccountFromCall(callID).empty() and accountExists(accountID)) {
        // account id exist in AccountMap
        ost::MutexLock m (_callAccountMapMutex);
        _callAccountMap[callID] = accountID;
        _debug ("Manager: Associate Call %s with Account %s", callID.data(), accountID.data());
        return true;
    }
    return false;
}

std::string ManagerImpl::getAccountFromCall (const std::string& callID)
{
    ost::MutexLock m (_callAccountMapMutex);
	CallAccountMap::iterator iter = _callAccountMap.find (callID);

	return (iter == _callAccountMap.end()) ? "" : iter->second;
}

bool ManagerImpl::removeCallAccount (const std::string& callID)
{
    ost::MutexLock m (_callAccountMapMutex);
    return _callAccountMap.erase (callID);
}

bool ManagerImpl::isValidCall(const std::string& callID)
{
	ost::MutexLock m(_callAccountMapMutex);
    return _callAccountMap.find (callID) != _callAccountMap.end();
}

std::string ManagerImpl::getNewCallID ()
{
    std::ostringstream random_id ("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (not getAccountFromCall (random_id.str()).empty()) {
        random_id.clear();
        random_id << "s";
        random_id << (unsigned) rand();
    }

    return random_id.str();
}

std::vector<std::string> ManagerImpl::loadAccountOrder (void) const
{
    const std::string account_list(preferences.getAccountOrder());

    _debug ("Manager: Load account order %s", account_list.c_str());

    return unserialize (account_list);
}

void ManagerImpl::loadAccountMap(Conf::YamlParser *parser)
{
	SIPVoIPLink *link = SIPVoIPLink::instance();

    // build a default IP2IP account with default parameters
    Account *ip2ip = new SIPAccount(IP2IP_PROFILE);
    _accountMap[IP2IP_PROFILE] = ip2ip;

    // If configuration file parsed, load saved preferences
    if (parser) {
        Conf::Sequence *seq = parser->getAccountSequence()->getSequence();
        for (Conf::Sequence::const_iterator iter = seq->begin(); iter != seq->end(); ++iter) {
            Conf::MappingNode *map = (Conf::MappingNode *) (*iter);
            std::string accountid;
            map->getValue ("id", &accountid);

            if (accountid == "IP2IP") {
                ip2ip->unserialize (map);
                break;
            }
        }
    }

    // Initialize default UDP transport according to
    // IP to IP settings (most likely using port 5060)
    link->createDefaultSipUdpTransport();

    // Call this method to create TLS listener
    link->createDefaultSipTlsListener();


    // Force IP2IP settings to be loaded to be loaded
    // No registration in the sense of the REGISTER method is performed.
    ip2ip->registerVoIPLink();

    if (!parser)
    	return;

	// build preferences
    preferences.unserialize (parser->getPreferenceNode());
    voipPreferences.unserialize (parser->getVoipPreferenceNode());
    addressbookPreference.unserialize (parser->getAddressbookNode());
    hookPreference.unserialize (parser->getHookNode());
    audioPreference.unserialize (parser->getAudioNode());
    shortcutPreferences.unserialize (parser->getShortcutNode());

    Conf::Sequence *seq = parser->getAccountSequence()->getSequence();

    // Each element in sequence is a new account to create
    for (Conf::Sequence::const_iterator iter = seq->begin(); iter != seq->end(); ++iter) {
        Conf::MappingNode *map = (Conf::MappingNode *) (*iter);

        std::string accountType;
        map->getValue ("type", &accountType);

        std::string accountid;
        map->getValue ("id", &accountid);

        std::string accountAlias;
        map->getValue ("alias", &accountAlias);

        if (accountid.empty() or accountAlias.empty() or accountid == IP2IP_PROFILE)
            continue;

        Account *a;
        if (accountType == "IAX")
        	a = new IAXAccount(accountid);
        else // assume SIP
        	a = new SIPAccount(accountid);

        _accountMap[accountid] = a;

		a->unserialize (map);
    }
}

void ManagerImpl::unloadAccountMap ()
{
    _debug ("Manager: Unload account map");

    AccountMap::iterator iter;
    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        // Avoid removing the IP2IP account twice
        if (iter->first != "")
            delete iter->second;
    }

    _accountMap.clear();

    delete SIPVoIPLink::instance();
}

bool ManagerImpl::accountExists (const std::string& accountID)
{
    return _accountMap.find (accountID) != _accountMap.end();
}

Account*
ManagerImpl::getAccount (const std::string& accountID) const
{
    AccountMap::const_iterator iter = _accountMap.find(accountID);
    if (iter != _accountMap.end())
		return iter->second;

    _debug ("Manager: Did not found account %s, returning IP2IP account", accountID.c_str());
    return getAccount(IP2IP_PROFILE);
}

std::string ManagerImpl::getAccountIdFromNameAndServer (
    const std::string& userName, const std::string& server) const
{
    _info ("Manager : username = %s , server = %s", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    for (AccountMap::const_iterator iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *> (iter->second);
        if (account and account->isEnabled() and account->fullMatch (userName, server)) {
            _debug ("Manager: Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
            return iter->first;
        }
    }

    // We failed! Then only match the hostname
    for (AccountMap::const_iterator iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *> (iter->second);

        if (account and account->isEnabled() and account->hostnameMatch (server)) {
            _debug ("Manager: Matching account id in request with hostname %s", server.c_str());
            return iter->first;
        }
    }

    // We failed! Then only match the username
    for (AccountMap::const_iterator iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *> (iter->second);

        if (account and account->isEnabled() and account->userMatch (userName)) {
            _debug ("Manager: Matching account id in request with username %s", userName.c_str());
            return iter->first;
        }
    }

    _debug ("Manager: Username %s or server %s doesn't match any account, using IP2IP", userName.c_str(), server.c_str());

    return "";
}

std::map<std::string, int32_t> ManagerImpl::getAddressbookSettings () const
{
    std::map<std::string, int32_t> settings;

    settings["ADDRESSBOOK_ENABLE"] = addressbookPreference.getEnabled();
    settings["ADDRESSBOOK_MAX_RESULTS"] = addressbookPreference.getMaxResults();
    settings["ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"] = addressbookPreference.getPhoto();
    settings["ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"] = addressbookPreference.getBusiness();
    settings["ADDRESSBOOK_DISPLAY_PHONE_HOME"] = addressbookPreference.getHome();
    settings["ADDRESSBOOK_DISPLAY_PHONE_MOBILE"] = addressbookPreference.getMobile();

    return settings;
}

void ManagerImpl::setAddressbookSettings (
    const std::map<std::string, int32_t>& settings)
{
    _debug ("Manager: Update addressbook settings");

    addressbookPreference.setEnabled (settings.find ("ADDRESSBOOK_ENABLE")->second == 1);
    addressbookPreference.setMaxResults (settings.find ("ADDRESSBOOK_MAX_RESULTS")->second);
    addressbookPreference.setPhoto (settings.find ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")->second == 1);
    addressbookPreference.setBusiness (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")->second == 1);
    addressbookPreference.setHone (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_HOME")->second == 1);
    addressbookPreference.setMobile (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")->second == 1);

    // Write it to the configuration file
    // TODO save config is called for updateAddressbookSettings, updateHookSettings, setHistoryLimit each called
    // when closing preference window (in this order)
    // saveConfig();
}

void ManagerImpl::setAddressbookList (const std::vector<std::string>& list)
{
    _debug ("Manager: Set addressbook list");

    std::string s = ManagerImpl::serialize (list);
    _debug("Manager: New addressbook list: %s", s.c_str());
    addressbookPreference.setList (s);

    saveConfig();
}

std::vector<std::string> ManagerImpl::getAddressbookList (void) const
{
    return unserialize (addressbookPreference.getList());
}

std::map<std::string, std::string> ManagerImpl::getHookSettings () const
{
    std::map<std::string, std::string> settings;

    settings["URLHOOK_IAX2_ENABLED"] = hookPreference.getIax2Enabled() ? "true" : "false";
    settings["PHONE_NUMBER_HOOK_ADD_PREFIX"] = hookPreference.getNumberAddPrefix();
    settings["PHONE_NUMBER_HOOK_ENABLED"] = hookPreference.getNumberEnabled() ? "true" : "false";
    settings["URLHOOK_SIP_ENABLED"] = hookPreference.getSipEnabled() ? "true" : "false";
    settings["URLHOOK_COMMAND"] = hookPreference.getUrlCommand();
    settings["URLHOOK_SIP_FIELD"] = hookPreference.getUrlSipField();

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

void ManagerImpl::checkCallConfiguration (const std::string& id,
        const std::string &to, Call::CallConfiguration *callConfig)
{
    Call::CallConfiguration config;

    if (to.find (SIP_SCHEME) == 0 or to.find (SIPS_SCHEME) == 0) {
        _debug ("Manager: Sip scheme detected (sip: or sips:), sending IP2IP Call");
        config = Call::IPtoIP;
    } else
        config = Call::Classic;

    associateConfigToCall (id, config);

    *callConfig = config;
}

bool ManagerImpl::associateConfigToCall (const std::string& callID,
        Call::CallConfiguration config)
{
    if (getConfigFromCall (callID) == CallConfigNULL) { // nothing with the same ID
        _callConfigMap[callID] = config;
        _debug ("Manager: Associate call %s with config %d", callID.c_str(), config);
        return true;
    } else
        return false;
}

Call::CallConfiguration ManagerImpl::getConfigFromCall (const std::string& callID) const
{
    CallConfigMap::const_iterator iter = _callConfigMap.find (callID);

    if (iter == _callConfigMap.end())
        return (Call::CallConfiguration) CallConfigNULL;
    else
        return iter->second;
}

bool ManagerImpl::removeCallConfig (const std::string& callID)
{
    return _callConfigMap.erase (callID);
}

std::map<std::string, std::string> ManagerImpl::getCallDetails (const std::string& callID)
{
    // We need here to retrieve the call information attached to the call ID
    // To achieve that, we need to get the voip link attached to the call
    // But to achieve that, we need to get the account the call was made with

    // So first we fetch the account
    const std::string accountid(getAccountFromCall (callID));

    // Then the VoIP link this account is linked with (IAX2 or SIP)
    Call *call = NULL;
    if (Account *account = getAccount (accountid)) {
        VoIPLink *link = account->getVoIPLink();

        if (link)
            call = link->getCall (callID);
    }

    std::map<std::string, std::string> call_details;
    if (call) {
        std::ostringstream type;
        type << call->getCallType();
        call_details["ACCOUNTID"] = accountid;
        call_details["PEER_NUMBER"] = call->getPeerNumber();
        call_details["PEER_NAME"] = call->getPeerName();
        call_details["DISPLAY_NAME"] = call->getDisplayName();
        call_details["CALL_STATE"] = call->getStateStr();
        call_details["CALL_TYPE"] = type.str();
    } else {
        _error ("Manager: Error: getCallDetails()");
        call_details["ACCOUNTID"] = "";
        call_details["PEER_NUMBER"] = "Unknown";
        call_details["PEER_NAME"] = "Unknown";
        call_details["DISPLAY_NAME"] = "Unknown";
        call_details["CALL_STATE"] = "UNKNOWN";
        call_details["CALL_TYPE"] = "0";
    }

    return call_details;
}

std::vector<std::string> ManagerImpl::getHistorySerialized(void) const
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

namespace {
template <typename M, typename V>
void vectorFromMapKeys(const M &m, V &v)
{
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it )
        v.push_back(it->first);
}
}

std::vector<std::string> ManagerImpl::getCallList (void) const
{
    std::vector<std::string> v;
    vectorFromMapKeys(_callAccountMap, v);
    return v;
}

std::map<std::string, std::string> ManagerImpl::getConferenceDetails (
    const std::string& confID) const
{
    std::map<std::string, std::string> conf_details;
    ConferenceMap::const_iterator iter_conf;

    iter_conf = _conferencemap.find (confID);

    if (iter_conf != _conferencemap.end()) {
        conf_details["CONFID"] = confID;
        conf_details["CONF_STATE"] = iter_conf->second->getStateStr();
    }

    return conf_details;
}

std::vector<std::string> ManagerImpl::getConferenceList (void) const
{
    std::vector<std::string> v;
    vectorFromMapKeys(_conferencemap, v);

    return v;
}

std::vector<std::string> ManagerImpl::getParticipantList (const std::string& confID) const
{
    ConferenceMap::const_iterator iter_conf = _conferencemap.find (confID);
    Conference *conf = NULL;

    if (iter_conf != _conferencemap.end())
        conf = iter_conf->second;

    std::vector<std::string> v;
    if (conf) {
        const ParticipantSet &participants = conf->getParticipantList();
        std::copy(participants.begin(), participants.end(), std::back_inserter(v));;
    } else
        _warn ("Manager: Warning: Did not found conference %s", confID.c_str());

    return v;
}
