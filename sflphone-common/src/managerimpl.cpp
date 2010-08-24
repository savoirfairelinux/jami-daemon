/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include "user_cfg.h"
#include "global.h"
#include "sip/sipaccount.h"

#include "audio/audiolayer.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "audio/sound/tonelist.h"
#include "history/historymanager.h"
#include "accountcreator.h" // create new account
#include "sip/sipvoiplink.h"
#include "manager.h"
#include "dbus/configurationmanager.h"

#include "conference.h"

#include <errno.h>
#include <time.h>
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

// Default account used to get default parametersa if requested by client (to build ne account)
SIPAccount defaultAccount ("default");

ManagerImpl::ManagerImpl (void) :
        _hasTriedToRegister (false), _config(), _currentCallId2(),
        _currentCallMutex(), _codecBuilder (NULL), _audiodriver (NULL),
        _dtmfKey (NULL), _codecDescriptorMap(), _toneMutex(),
        _telephoneTone (NULL), _audiofile (NULL), _spkr_volume (0),
        _mic_volume (0), _mutex(), _dbus (NULL), _waitingCall(),
        _waitingCallMutex(), _nbIncomingWaitingCall (0), _path (""),
        _exist (0), _setupLoaded (false), _callAccountMap(),
        _callAccountMapMutex(), _callConfigMap(), _accountMap(),
        _directIpAccount (NULL), _cleaner (NULL), _history (NULL)
{

    // initialize random generator for call id
    srand (time (NULL));

    _cleaner = new NumberCleaner();
    _history = new HistoryManager();

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

    // terminate();
    delete _cleaner;
    _cleaner = NULL;
    delete _history;
    _history = NULL;

    _debug ("Manager: %s stop correctly.", PROGNAME);
}

void ManagerImpl::init ()
{

    _debug ("Manager: Init");

    // Load accounts, init map
    buildConfiguration();

    _debug ("Manager: account map loaded");

    initVolume();

    if (_exist == 0) {
        _warn ("Manager: Cannot create config file in your home directory");
    }

    initAudioDriver();

    selectAudioDriver();

    // Initialize the list of supported audio codecs
    initAudioCodec();

    AudioLayer *audiolayer = getAudioDriver();

    if (audiolayer) {
        unsigned int sampleRate = audiolayer->getSampleRate();

        _debugInit ("Manager: Load telephone tone");
        std::string country = preferences.getZoneToneChoice();
        _telephoneTone = new TelephoneTone (country, sampleRate);

        _debugInit ("Manager: Loading DTMF key");
        _dtmfKey = new DTMF (sampleRate);
    }

    // Load the history
    _history->load_history (preferences.getHistoryLimit());
}

void ManagerImpl::terminate ()
{

    _debug ("Manager: Terminate ");
    saveConfig();

    unloadAccountMap();

    _debug ("Manager: Unload DTMF key");
    delete _dtmfKey;

    _debug ("Manager: Unload telephone tone");
    delete _telephoneTone;
    _telephoneTone = NULL;

    _debug ("Manager: Unload audio driver");
    delete _audiodriver;
    _audiodriver = NULL;

    _debug ("Manager: Unload telephone tone");
    delete _telephoneTone;
    _telephoneTone = NULL;

    _debug ("Manager: Unload audio codecs ");
    _codecDescriptorMap.deleteHandlePointer();

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
                                const CallID& call_id, const std::string& to)
{

    std::string pattern, to_cleaned;
    Call::CallConfiguration callConfig;
    SIPVoIPLink *siplink;

    if (call_id.empty()) {
        _debug ("Manager: New outgoing call abbort, missing callid");
        return false;
    }

    _debug ("Manager: New outgoing call %s to %s", call_id.c_str(), to.c_str());

    CallID current_call_id = getCurrentCallId();

    if (hookPreference.getNumberEnabled())
        _cleaner->set_phone_number_prefix (hookPreference.getNumberAddPrefix());
    else
        _cleaner->set_phone_number_prefix ("");

    to_cleaned = _cleaner->clean (to);

    /* Check what kind of call we are dealing with */
    check_call_configuration (call_id, to_cleaned, &callConfig);

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
        siplink = SIPVoIPLink::instance ("");

        if (siplink->new_ip_to_ip_call (call_id, to_cleaned)) {
            switchCall (call_id);
            return true;
        } else {
            callFailure (call_id);
        }

        return false;
    }

    _debug ("Manager: Selecting account %s", account_id.c_str());

    if (!accountExists (account_id)) {
        _error ("Manager: Error: Account doesn't exist in new outgoing call");
        return false;
    }

    if (getAccountFromCall (call_id) != AccountNULL) {
        _error ("Manager: Error: Call id already exists in outgoing call");
        return false;
    }

    _debug ("Manager: Adding Outgoing Call %s on account %s", call_id.data(), account_id.data());
    associateCallToAccount (call_id, account_id);

    if (getAccountLink (account_id)->newOutgoingCall (call_id, to_cleaned)) {
        switchCall (call_id);
        return true;
    } else {
        callFailure (call_id);
        _debug ("Manager: Error: An error occur, the call was not created");
    }

    return false;
}

//THREAD=Main : for outgoing Call
bool ManagerImpl::answerCall (const CallID& call_id)
{

    _debug ("ManagerImpl: Answer call %s", call_id.c_str());

    stopTone();

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    AccountID account_id = getAccountFromCall (call_id);

    if (account_id == AccountNULL) {
        _debug ("    answerCall: AccountId is null");
    }

    Call* call = NULL;

    call = getAccountLink (account_id)->getCall (call_id);

    if (call == NULL) {
        _debug ("    answerCall: Call is null");
    }

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        _debug ("    answerCall: Currently conversing with %s", current_call_id.c_str());
        // if it is not a conference and is not a conference participant

        if (!isConference (current_call_id) && !participToConference (
                    current_call_id)) {
            _debug ("    answerCall: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall (current_call_id);
        }

        // if we are talking to a conference and we are answering an incoming call
        else if (isConference (current_call_id)
                 && !participToConference (call_id)) {
            _debug ("    answerCall: Detach main participant from conference");
            detachParticipant (default_id, current_call_id);
        }

    }

    if (!getAccountLink (account_id)->answer (call_id)) {
        // error when receiving...
        removeCallAccount (call_id);
        return false;
    }

    // if it was waiting, it's waiting no more
    if (_dbus)
        _dbus->getCallManager()->callStateChanged (call_id, "CURRENT");

    // std::string codecName = Manager::instance().getCurrentCodecName (call_id);
    // if (_dbus) _dbus->getCallManager()->currentSelectedCodec (call_id, codecName.c_str());

    removeWaitingCall (call_id);

    // if we dragged this call into a conference already
    if (participToConference (call_id)) {

        // AccountID currentAccountId;
        // Call* call = NULL;

        // currentAccountId = getAccountFromCall (call_id);
        // call = getAccountLink (currentAccountId)->getCall (call_id);

        switchCall (call->getConfId());
    } else {
        switchCall (call_id);
    }

    return true;
}

//THREAD=Main
bool ManagerImpl::hangupCall (const CallID& call_id)
{

    _info ("Manager: Hangup call %s", call_id.c_str());

    PulseLayer *pulselayer;
    AccountID account_id;
    bool returnValue = true;

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    stopTone();

    /* Broadcast a signal over DBus */
    _debug ("Manager: Send DBUS call state change (HUNGUP) for id %s", call_id.c_str());

    if (_dbus)
        _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");

    if (participToConference (call_id)) {

        Conference *conf = getConferenceFromCallID (call_id);

        if (conf != NULL) {
            // remove this participant
            removeParticipant (call_id);

            processRemainingParticipant (current_call_id, conf);
        }

    } else {
        // we are not participating to a conference, current call switched to ""
        if (!isConference (current_call_id))
            switchCall ("");
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)->hangup (call_id);
    }
    /* Classic call, attached to an account */
    else {
        account_id = getAccountFromCall (call_id);

        // Account may be NULL if call have not been sent yet
        if (account_id == AccountNULL) {
            _error ("Manager: Error: account id is NULL in hangup");
            returnValue = false;
        } else {
            returnValue = getAccountLink (account_id)->hangup (call_id);
            removeCallAccount (call_id);
        }
    }

    int nbCalls = getCallList().size();

    AudioLayer *audiolayer = getAudioDriver();

    // stop streams
    if (audiolayer && (nbCalls <= 0)) {
        _debug ("Manager: stop audio stream, ther is only %i call(s) remaining", nbCalls);
        audiolayer->stopStream();
    }

    if (_audiodriver->getLayerType() == PULSEAUDIO) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());
    }

    return returnValue;
}

bool ManagerImpl::hangupConference (const ConfID& id)
{

    _debug ("Manager: Hangup conference %s", id.c_str());

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    AccountID currentAccountId;

    // Call* call = NULL;


    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("Manager: Hangup onference participant %s", (*iter_participant).c_str());

            hangupCall (*iter_participant);

            iter_participant++;

        }

    }

    switchCall ("");

    return true;
}

//THREAD=Main
bool ManagerImpl::cancelCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;

    _debug ("Manager: Cancel call");

    stopTone();

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)->cancel (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("! Manager Cancel Call: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->cancel (id);

        removeCallAccount (id);
    }

    // it could be a waiting call?
    removeWaitingCall (id);

    switchCall ("");

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::onHoldCall (const CallID& call_id)
{
    AccountID account_id;
    bool returnValue;

    _debug ("Manager: Put call %s on hold", call_id.c_str());

    stopTone();

    CallID current_call_id = getCurrentCallId();


    /* Direct IP to IP call */

    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> onhold (call_id);
    }

    /* Classic call, attached to an account */
    else {
        account_id = getAccountFromCall (call_id);

        if (account_id == AccountNULL) {
            _debug ("Manager: Account ID %s or callid %s doesn't exists in call onHold", account_id.c_str(), call_id.c_str());
            return false;
        }

        returnValue = getAccountLink (account_id)->onhold (call_id);
    }

    removeWaitingCall (call_id);

    // keeps current call id if the action is not holding this call or a new outgoing call

    if (current_call_id == call_id) {

        switchCall ("");
    }

    if (_dbus)
        _dbus->getCallManager()->callStateChanged (call_id, "HOLD");

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::offHoldCall (const CallID& call_id)
{

    AccountID account_id;
    bool returnValue, is_rec;
    std::string codecName;

    is_rec = false;

    _debug ("Manager: Put call %s off hold", call_id.c_str());

    stopTone();

    CallID current_call_id = getCurrentCallId();

    //Place current call on hold if it isn't

    if (hasCurrentCall()) {
        // if this is not a conferenceand this and is not a conference participant
        if (!isConference (current_call_id) && !participToConference (
                    current_call_id)) {
            onHoldCall (current_call_id);
        } else if (isConference (current_call_id) && !participToConference (
                       call_id)) {
            detachParticipant (default_id, current_call_id);
        }
    }

    // switch current call id to id since sipvoip link need it to amke a call
    // switchCall(id);

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        // is_rec = SIPVoIPLink::instance (AccountNULL)-> isRecording (call_id);
        returnValue = SIPVoIPLink::instance (AccountNULL)-> offhold (call_id);
    }

    /* Classic call, attached to an account */
    else {
        account_id = getAccountFromCall (call_id);

        if (account_id == AccountNULL) {
            _warn ("Manager: Error: Call doesn't exists in off hold");
            return false;
        }

        _debug ("Manager: Setting offhold, Account %s, callid %s", account_id.c_str(), call_id.c_str());

        is_rec = getAccountLink (account_id)->getCall (call_id)->isRecording();
        returnValue = getAccountLink (account_id)->offhold (call_id);
    }

    if (_dbus) {
        if (is_rec)
            _dbus->getCallManager()->callStateChanged (call_id, "UNHOLD_RECORD");
        else
            _dbus->getCallManager()->callStateChanged (call_id, "UNHOLD_CURRENT");

    }

    if (participToConference (call_id)) {

        AccountID currentAccountId;
        Call* call = NULL;

        currentAccountId = getAccountFromCall (call_id);
        call = getAccountLink (currentAccountId)->getCall (call_id);

        switchCall (call->getConfId());

    } else {
        switchCall (call_id);
        _audiodriver->flushMain();
    }

    return returnValue;
}

//THREAD=Main
bool ManagerImpl::transferCall (const CallID& call_id, const std::string& to)
{
    AccountID accountid;
    bool returnValue;

    _info ("Manager: Transfer call %s", call_id.c_str());

    CallID current_call_id = getCurrentCallId();

    // Direct IP to IP call
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> transfer (call_id, to);
    }
    // Classic call, attached to an account
    else {

        accountid = getAccountFromCall (call_id);

        if (accountid == AccountNULL) {
            _warn ("Manager: Call doesn't exists");
            return false;
        }

        returnValue = getAccountLink (accountid)->transfer (call_id, to);

    }

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall (call_id);

    return returnValue;
}

void ManagerImpl::transferFailed ()
{

    _debug ("UserAgent: Transfer failed");

    if (_dbus)
        _dbus->getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceded ()
{

    _debug ("UserAgent: Transfer succeded");

    if (_dbus)
        _dbus->getCallManager()->transferSucceded();

}

//THREAD=Main : Call:Incoming
bool ManagerImpl::refuseCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;

    _debug ("Manager: Refuse call %s", id.c_str());

    CallID current_call_id = getCurrentCallId();

    stopTone();

    int nbCalls = getCallList().size();

    // AudioLayer* audiolayer = getAudioDriver();

    if (nbCalls <= 1) {
        _debug ("    refuseCall: stop audio stream, there is only %i call(s) remaining", nbCalls);

        AudioLayer* audiolayer = getAudioDriver();
        audiolayer->stopStream();
    }

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> refuse (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
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
    _dbus->getCallManager()->conferenceCreated (conf->getConfID());

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

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    _audiodriver->getMainBuffer()->unBindAll (default_id);

    ParticipantSet participants = conf->getParticipantList();

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end()) {

        _audiodriver->getMainBuffer()->bindCallID (*iter_p, default_id);
    }

    // Then remove the conference from the conference map
    if (_conferencemap.erase (conference_id) == 1)
        _debug ("Manager: Conference %s removed successfully", conference_id.c_str());
    else
        _error ("Manager: Error: Cannot remove conference: %s", conference_id.c_str());

    // broadcast a signal over dbus
    _dbus->getCallManager()->conferenceRemoved (conference_id);

}

Conference*
ManagerImpl::getConferenceFromCallID (const CallID& call_id)
{
    AccountID account_id;
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

    AccountID currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

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

        conf->setState (Conference::Hold);

        _dbus->getCallManager()->conferenceChanged (conf->getConfID(),
                conf->getStateStr());

    }

}

void ManagerImpl::unHoldConference (const CallID& id)
{

    _debug ("Manager: Unhold conference()");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    AccountID currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("    unholdConference: participant %s", (*iter_participant).c_str());
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);

            offHoldCall (*iter_participant);

            iter_participant++;

        }

        conf->setState (Conference::Active_Atached);

        _dbus->getCallManager()->conferenceChanged (conf->getConfID(),
                conf->getStateStr());

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

    AccountID accountId;

    Call* call = NULL;

    accountId = getAccountFromCall (call_id);
    call = getAccountLink (accountId)->getCall (call_id);

    if (call == NULL) {
        return false;

    }

    if (call->getConfId() == "") {
        return false;
    } else {

        return true;
    }
}

void ManagerImpl::addParticipant (const CallID& call_id, const CallID& conference_id)
{
    _debug ("ManagerImpl: Add participant %s to %s", call_id.c_str(), conference_id.c_str());

    std::map<std::string, std::string> call_details = getCallDetails (call_id);

    ConferenceMap::iterator iter = _conferencemap.find (conference_id);
    std::map<std::string, std::string>::iterator iter_details;

    // store the current call id (it will change in offHoldCall or in answerCall)
    CallID current_call_id = getCurrentCallId();

    // detach from the conference and switch to this conference

    if (current_call_id != call_id) {
        if (isConference (current_call_id)) {
            detachParticipant (default_id, current_call_id);
        } else
            onHoldCall (current_call_id);
    }

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    switchCall ("");

    addMainParticipant (conference_id);

    _debug ("    addParticipant: enter main process");

    if (iter != _conferencemap.end()) {

        Conference* conf = iter->second;
        switchCall (conf->getConfID());

        AccountID currentAccountId;
        Call* call = NULL;

        currentAccountId = getAccountFromCall (call_id);
        call = getAccountLink (currentAccountId)->getCall (call_id);
        call->setConfId (conf->getConfID());

        conf->add (call_id);

        iter_details = call_details.find ("CALL_STATE");

        _debug ("    addParticipant: call state: %s", iter_details->second.c_str());

        if (iter_details->second == "HOLD") {
            _debug ("    OFFHOLD %s", call_id.c_str());

            // offHoldCall create a new rtp session which use addStream to bind participant
            offHoldCall (call_id);
        } else if (iter_details->second == "INCOMING") {
            _debug ("    ANSWER %s", call_id.c_str());
            // answerCall create a new rtp session which use addStream to bind participant
            answerCall (call_id);
        } else if (iter_details->second == "CURRENT") {
            // Already a curent call, so we beed to reset audio stream bindings manually
            _audiodriver->getMainBuffer()->unBindAll (call_id);
            conf->bindParticipant (call_id);
        }

        // _dbus->getCallManager()->conferenceChanged(conference_id, conf->getStateStr());

        ParticipantSet participants = conf->getParticipantList();

        // reset ring buffer for all conference participant
        ParticipantSet::iterator iter_p = participants.begin();

        while (iter_p != participants.end()) {

            // flush conference participants only
            _audiodriver->getMainBuffer()->flush (*iter_p);

            iter_p++;
        }

        _audiodriver->getMainBuffer()->flush (default_id);
    } else {
        _debug ("    addParticipant: Error, conference %s conference_id not found!", conference_id.c_str());
    }

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

    if (iter != _conferencemap.end()) {
        conf = iter->second;

        ParticipantSet participants = conf->getParticipantList();

        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _audiodriver->getMainBuffer()->bindCallID (*iter_participant,
                    default_id);

            iter_participant++;
        }

        // Reset ringbuffer's readpointers
        iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _audiodriver->getMainBuffer()->flush (*iter_participant);

            iter_participant++;
        }

        _audiodriver->getMainBuffer()->flush (default_id);

        conf->setState (Conference::Active_Atached);

        _dbus->getCallManager()->conferenceChanged (conference_id,
                conf->getStateStr());

    }

    switchCall (conference_id);
}

void ManagerImpl::joinParticipant (const CallID& call_id1, const CallID& call_id2)
{

    _debug ("Manager: Join participants %s, %s", call_id1.c_str(), call_id2.c_str());

    std::map<std::string, std::string> call1_details = getCallDetails (call_id1);
    std::map<std::string, std::string> call2_details = getCallDetails (call_id2);

    std::map<std::string, std::string>::iterator iter_details;

    // Test if we have valid call ids
    iter_details = call1_details.find ("PEER_NUMBER");

    if (iter_details->second == "Unknown") {
        _error ("Manager: Error: Id %s is not a valid call", call_id1.c_str());
        return;
    }

    iter_details = call2_details.find ("PEER_NUMBER");

    if (iter_details->second == "Unknown") {
        _error ("Manager: Error: Id %s is not a valid call", call_id2.c_str());
        return;
    }

    AccountID currentAccountId;
    Call* call = NULL;

    CallID current_call_id = getCurrentCallId();
    _debug ("Manager: current_call_id %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ( (current_call_id != call_id1) && (current_call_id != call_id2)) {

        // If currently in a conference
        if (isConference (current_call_id))
            detachParticipant (default_id, current_call_id);
        // If currently in a call
        else
            onHoldCall (current_call_id);
    }

    _debug ("Manager: Create a conference");

    Conference *conf = createConference (call_id1, call_id2);
    switchCall (conf->getConfID());

    currentAccountId = getAccountFromCall (call_id1);
    call = getAccountLink (currentAccountId)->getCall (call_id1);
    call->setConfId (conf->getConfID());

    iter_details = call1_details.find ("CALL_STATE");
    _debug ("Manager: Process call %s state: %s", call_id1.c_str(), iter_details->second.c_str());

    if (iter_details->second == "HOLD") {
        offHoldCall (call_id1);
    } else if (iter_details->second == "INCOMING") {
        answerCall (call_id1);
    } else if (iter_details->second == "CURRENT") {
        _audiodriver->getMainBuffer()->unBindAll (call_id1);
        conf->bindParticipant (call_id1);
    } else if (iter_details->second == "INACTIVE") {
        answerCall (call_id1);
    } else {
        _warn ("Manager: Call state not recognized");
    }

    currentAccountId = getAccountFromCall (call_id2);

    call = getAccountLink (currentAccountId)->getCall (call_id2);
    call->setConfId (conf->getConfID());

    iter_details = call2_details.find ("CALL_STATE");
    _debug ("Manager: Process call %s state: %s", call_id2.c_str(), iter_details->second.c_str());

    if (iter_details->second == "HOLD") {
        offHoldCall (call_id2);
    } else if (iter_details->second == "INCOMING") {
        answerCall (call_id2);
    } else if (iter_details->second == "CURRENT") {
        _audiodriver->getMainBuffer()->unBindAll (call_id2);
        conf->bindParticipant (call_id2);
    } else if (iter_details->second == "INACTIVE") {
        answerCall (call_id2);
    } else {
        _warn ("Manager: Call state not recognized");
    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();

}

void ManagerImpl::detachParticipant (const CallID& call_id,
                                     const CallID& current_id)
{

    _debug ("Manager: Detach participant %s from conference", call_id.c_str());

    CallID current_call_id = current_id;

    current_call_id = getCurrentCallId();

    if (call_id != default_id) {
        AccountID currentAccountId;
        Call* call = NULL;

        currentAccountId = getAccountFromCall (call_id);
        call = getAccountLink (currentAccountId)->getCall (call_id);

        // TODO: add conference_id as a second parameter
        ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

        Conference *conf = getConferenceFromCallID (call_id);

        if (conf != NULL) {

            _debug ("Manager: Detaching participant %s", call_id.c_str());
            std::map<std::string, std::string> call_details = getCallDetails (
                        call_id);
            std::map<std::string, std::string>::iterator iter_details;

            iter_details = call_details.find ("CALL_STATE");

            if (iter_details->second == "RINGING") {

                removeParticipant (call_id);
            } else {
                onHoldCall (call_id);
                removeParticipant (call_id);
                processRemainingParticipant (current_call_id, conf);

                _dbus->getCallManager()->conferenceChanged (conf->getConfID(),
                        conf->getStateStr());
            }
        } else {

            _debug ("Manager: Call is not conferencing, cannot detach");

        }
    } else {
        _debug ("Manager: Unbind main participant from all");
        _audiodriver->getMainBuffer()->unBindAll (default_id);

        if (isConference (current_call_id)) {

            ConferenceMap::iterator iter = _conferencemap.find (current_call_id);
            Conference *conf = iter->second;

            conf->setState (Conference::Active_Detached);

            _dbus->getCallManager()->conferenceChanged (conf->getConfID(),
                    conf->getStateStr());
        }

        switchCall ("");

    }

}

void ManagerImpl::removeParticipant (const CallID& call_id)
{
    _debug ("Manager: Remove participant %s", call_id.c_str());

    // TODO: add conference_id as a second parameter
    Conference* conf;

    AccountID currentAccountId;
    Call* call = NULL;

    // this call is no more a conference participant
    currentAccountId = getAccountFromCall (call_id);
    call = getAccountLink (currentAccountId)->getCall (call_id);

    ConferenceMap conf_map = _conferencemap;
    ConferenceMap::iterator iter = conf_map.find (call->getConfId());

    if (iter == conf_map.end()) {
        _debug ("Manager: Error: No conference created, cannot remove participant");
    } else {

        conf = iter->second;

        _debug ("Manager: Remove participant %s", call_id.c_str());
        conf->remove (call_id);
        call->setConfId ("");

    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();

}

void ManagerImpl::processRemainingParticipant (CallID current_call_id,
        Conference *conf)
{

    _debug ("Manager: Process remaining %d participant(s) from conference %s",
            conf->getNbParticipants(), conf->getConfID().c_str());

    if (conf->getNbParticipants() > 1) {

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        // Reset ringbuffer's readpointers
        iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _audiodriver->getMainBuffer()->flush (*iter_participant);

            iter_participant++;
        }

        _audiodriver->getMainBuffer()->flush (default_id);

    } else if (conf->getNbParticipants() == 1) {

        _debug ("Manager: Only one remaining participant");

        AccountID currentAccountId;
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

    // detachParticipant(default_id, "");

}

void ManagerImpl::addStream (const CallID& call_id)
{

    _debug ("Manager: Add audio stream %s", call_id.c_str());

    AccountID currentAccountId;
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
                _audiodriver->getMainBuffer()->flush (*iter_p);

                iter_p++;
            }

            _audiodriver->getMainBuffer()->flush (default_id);
        }

    } else {

        _debug ("Manager: Add stream to call");

        // bind to main
        getAudioDriver()->getMainBuffer()->bindCallID (call_id);

        // _audiodriver->getMainBuffer()->flush(default_id);
        _audiodriver->flushUrgent();
        _audiodriver->flushMain();

    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();
}

void ManagerImpl::removeStream (const CallID& call_id)
{
    _debug ("Manager: Remove audio stream %s", call_id.c_str());

    getAudioDriver()->getMainBuffer()->unBindAll (call_id);

    if (participToConference (call_id)) {
        removeParticipant (call_id);
    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();
}

//THREAD=Main
bool ManagerImpl::saveConfig (void)
{
    _debug ("Manager: Saving Configuration to XDG directory %s ... ", _path.c_str());
    audioPreference.setVolumemic (getMicVolume());
    audioPreference.setVolumespkr (getSpkrVolume());

    AccountMap::iterator iter = _accountMap.begin();

    try {
        // emitter = new Conf::YamlEmitter("sequenceEmitter.yml");
        emitter = new Conf::YamlEmitter (_path.c_str());

        while (iter != _accountMap.end()) {
            _debug ("Manager: Saving account: %s", iter->first.c_str());

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

    // _setupLoaded = _config.saveConfigTree(_path.data());
    return _setupLoaded;
}

//THREAD=Main
bool ManagerImpl::sendDtmf (const CallID& id, char code)
{

    AccountID accountid = getAccountFromCall (id);

    bool returnValue = false;

    playDtmf (code);

    CallAccountMap::iterator iter = _callAccountMap.find (id);

    // Make sure the call exist before sending DTMF, ths could be simply call dialing
    if (iter != _callAccountMap.end())
        returnValue = getAccountLink (accountid)->carryingDTMFdigits (id, code);

    return returnValue;
}

//THREAD=Main | VoIPLink
bool ManagerImpl::playDtmf (char code)
{
    int pulselen, layer, size;
    bool ret = false;
    AudioLayer *audiolayer;
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

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS
    audiolayer = getAudioDriver();

    layer = audiolayer->getLayerType();

    // fast return, no sound, so no dtmf
    if (audiolayer == 0 || _dtmfKey == 0) {
        _debug ("Manager: playDtmf: Error no audio layer...");
        return false;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    size = (int) ( (pulselen * (float) audiolayer->getSampleRate()) / 1000);

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
        audiolayer->startStream();
        audiolayer->putUrgent (buf, size * sizeof (SFLDataFormat));
    }

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
bool ManagerImpl::incomingCall (Call* call, const AccountID& accountId)
{

    std::string from, number, display_name, display;

    if (!call)
        _error ("Manager: Error: no call at this point");

    stopTone();

    _debug ("Manager: Incoming call %s for account %s", call->getCallId().data(), accountId.c_str());

    associateCallToAccount (call->getCallId(), accountId);

    // If account is null it is an ip to ip call
    if (accountId == AccountNULL) {
        associateConfigToCall (call->getCallId(), Call::IPtoIP);
    } else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber = call->getPeerNumber();

        int startIndex = peerNumber.find ("sip:");

        if (startIndex != (int) string::npos) {
            std::string strippedPeerNumber = peerNumber.substr (startIndex + 4);
            call->setPeerNumber (strippedPeerNumber);
        }

    }

    if (!hasCurrentCall()) {
        _debug ("Manager: Has no current call");

        call->setConnectionState (Call::Ringing);
        ringtone (accountId);

    } else {
        _debug ("Manager: has current call");
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

    if (_dbus)
        _dbus->getCallManager()->incomingCall (accountId, call->getCallId(), display.c_str());

    return true;
}

//THREAD=VoIP
void ManagerImpl::incomingMessage (const AccountID& accountId,
                                   const std::string& message)
{
    if (_dbus) {
        _dbus->getCallManager()->incomingMessage (accountId, message);
    }
}

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall (const CallID& id)
{

    _debug ("Manager: Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall (id)) {
        stopTone();
    }

    if (_dbus)
        _dbus->getCallManager()->callStateChanged (id, "CURRENT");

    _audiodriver->flushMain();

    _audiodriver->flushUrgent();
}

//THREAD=VoIP Call=Outgoing
void ManagerImpl::peerRingingCall (const CallID& id)
{

    _debug ("Manager: Peer call %s ringing", id.c_str());

    if (isCurrentCall (id)) {
        ringback();
    }

    if (_dbus)
        _dbus->getCallManager()->callStateChanged (id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall (const CallID& call_id)
{
    PulseLayer *pulselayer;
    AccountID account_id;
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

            switchCall ("");
        }
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        SIPVoIPLink::instance (AccountNULL)->hangup (call_id);
    }

    else {

        account_id = getAccountFromCall (call_id);

        returnValue = getAccountLink (account_id)->peerHungup (call_id);
    }

    /* Broadcast a signal over DBus */
    if (_dbus)
        _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");

    removeWaitingCall (call_id);

    removeCallAccount (call_id);

    int nbCalls = getCallList().size();

    // stop streams

    if (nbCalls <= 0) {
        _debug ("Manager: Stop audio stream, ther is only %i call(s) remaining", nbCalls);

        AudioLayer* audiolayer = getAudioDriver();
        audiolayer->stopStream();
    }

    if (_audiodriver->getLayerType() == PULSEAUDIO) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());
    }
}

//THREAD=VoIP
void ManagerImpl::callBusy (const CallID& id)
{
    _debug ("Manager: Call %s busy", id.c_str());

    if (_dbus)
        _dbus->getCallManager()->callStateChanged (id, "BUSY");

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
    if (_dbus)
        _dbus->getCallManager()->callStateChanged (call_id, "FAILURE");

    if (isCurrentCall (call_id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    CallID current_call_id = getCurrentCallId();

    if (participToConference (call_id)) {

        _debug ("Manager: Call %s participating to a conference failed", call_id.c_str());

        Conference *conf = getConferenceFromCallID (call_id);

        if (conf != NULL) {
            // remove this participant
            removeParticipant (call_id);

            processRemainingParticipant (current_call_id, conf);
        }

    }

    removeCallAccount (call_id);

    removeWaitingCall (call_id);

}

//THREAD=VoIP
void ManagerImpl::startVoiceMessageNotification (const AccountID& accountId,
        int nb_msg)
{
    if (_dbus)
        _dbus->getCallManager()->voiceMailNotify (accountId, nb_msg);
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
    AudioLayer *audiolayer;

    // _debug ("Manager: Play tone %d", toneId);

    hasToPlayTone = voipPreferences.getPlayTones();

    if (!hasToPlayTone)
        return false;

    audiolayer = getAudioDriver();

    if (audiolayer) {

        audiolayer->flushUrgent();
        audiolayer->startStream();
    }

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
    bool hasToPlayTone;

    hasToPlayTone = voipPreferences.getPlayTones();

    if (!hasToPlayTone)
        return;

    _toneMutex.enterMutex();

    if (_telephoneTone != 0) {
        _telephoneTone->setCurrentTone (Tone::TONE_NULL);
    }

    if (_audiofile)
        _audiofile->stop();

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
void ManagerImpl::ringtone (const AccountID& accountID)
{
    std::string ringchoice;
    AudioLayer *audiolayer;
    AudioCodec *codecForTone;
    int layer, samplerate;
    bool loadFile;

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

        audiolayer = getAudioDriver();

        if (!audiolayer) {
            _error ("Manager: Error: no audio layer in ringtone");
            return;
        }

        layer = audiolayer->getLayerType();
        samplerate = audiolayer->getSampleRate();
        codecForTone = _codecDescriptorMap.getFirstCodecAvailable();

        _toneMutex.enterMutex();

        if (_audiofile) {
            delete _audiofile;
            _audiofile = NULL;
        }

        std::string wave (".wav");
        size_t found = ringchoice.find (wave);

        if (found != std::string::npos)
            _audiofile = static_cast<AudioFile *> (new WaveFile());
        else
            _audiofile = static_cast<AudioFile *> (new RawFile());

        loadFile = false;

        if (_audiofile)
            loadFile = _audiofile->loadFile (ringchoice, codecForTone, samplerate);

        _toneMutex.leaveMutex();

        if (loadFile) {

            _toneMutex.enterMutex();
            _audiofile->start();
            _toneMutex.leaveMutex();

            // start audio if not started AND flush all buffers (main and urgent)
            audiolayer->startStream();

        } else {
            ringback();
        }

    } else {
        ringback();
    }
}

AudioLoop*
ManagerImpl::getTelephoneTone ()
{
    // _debug("ManagerImpl::getTelephoneTone()");
    if (_telephoneTone != 0) {
        ost::MutexLock m (_toneMutex);
        return _telephoneTone->getCurrentTone();
    } else {
        return 0;
    }
}

AudioLoop*
ManagerImpl::getTelephoneFile ()
{
    // _debug("ManagerImpl::getTelephoneFile()");
    ost::MutexLock m (_toneMutex);

    if (!_audiofile)
        return NULL;

    if (_audiofile->isStarted()) {
        return _audiofile;
    } else {
        return NULL;
    }
}

void ManagerImpl::notificationIncomingCall (void)
{
    AudioLayer *audiolayer;
    std::ostringstream frequency;
    unsigned int samplerate, nbSampling;

    audiolayer = getAudioDriver();

    _debug ("ManagerImpl::notificationIncomingCall");

    if (audiolayer != 0) {
        samplerate = audiolayer->getSampleRate();
        frequency << "440/" << 160;
        Tone tone (frequency.str(), samplerate);
        nbSampling = tone.getSize();
        SFLDataFormat buf[nbSampling];
        tone.getNext (buf, tone.getSize());
        /* Put the data in the urgent ring buffer */
        audiolayer->flushUrgent();
        audiolayer->putUrgent (buf, sizeof (SFLDataFormat) * nbSampling);
    }
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
        _exist = _config.populateFromFile (path);
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
    _codecDescriptorMap.init();
}

/*
 * TODO Retrieve the active codec list per account
 */
std::vector<std::string> ManagerImpl::retrieveActiveCodecs ()
{

    // This property is now set per account basis so we should remove it...
    std::string s = "";
    _info ("Manager: Retrieve active codecs: %s", s.c_str ());
    return unserialize (s);
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

    AccountID accountid = getAccountFromCall (id);
    VoIPLink* link = getAccountLink (accountid);
    Call* call = link->getCall (id);

    if (!call)
        return "";

    if (call->getState() != Call::Active)
        return "";
    else
        return link->getCurrentCodecName();
}

/**
 * Set input audio plugin
 */
void ManagerImpl::setInputAudioPlugin (const std::string& audioPlugin)
{
    int layer = _audiodriver -> getLayerType();

    if (CHECK_INTERFACE (layer , ALSA)) {
        _debug ("Set input audio plugin");
        _audiodriver -> setErrorMessage (-1);
        _audiodriver -> openDevice (_audiodriver->getIndexIn(), _audiodriver->getIndexOut(),
                                    _audiodriver->getIndexRing(), _audiodriver -> getSampleRate(),
                                    _audiodriver -> getFrameSize(), SFL_PCM_CAPTURE, audioPlugin);

        if (_audiodriver -> getErrorMessage() != -1)
            notifyErrClient (_audiodriver -> getErrorMessage());
    } else {
    }

}

/**
 * Set output audio plugin
 */
void ManagerImpl::setOutputAudioPlugin (const std::string& audioPlugin)
{

    int res;

    _debug ("Manager: Set output audio plugin");
    _audiodriver -> setErrorMessage (-1);
    res = _audiodriver -> openDevice (_audiodriver->getIndexIn(), _audiodriver->getIndexOut(),
                                      _audiodriver->getIndexRing(), _audiodriver -> getSampleRate(),
                                      _audiodriver -> getFrameSize(), SFL_PCM_BOTH, audioPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    // set config
    if (res)
        audioPreference.setPlugin (audioPlugin);

    //setConig(AUDIO, ALSA_PLUGIN, audioPlugin);
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string> ManagerImpl::getAudioOutputDeviceList (void)
{
    _debug ("Manager: Get audio output device list");
    AlsaLayer *layer;
    std::vector<std::string> devices;

    layer = dynamic_cast<AlsaLayer*> (getAudioDriver());

    if (layer)
        devices = layer -> getSoundCardsInfo (SFL_PCM_PLAYBACK);

    return devices;
}

/**
 * Set audio output device
 */
void ManagerImpl::setAudioDevice (const int index, int streamType)
{

    AlsaLayer *alsalayer = NULL;
    std::string alsaplugin;
    _debug ("Manager: Set audio device: %i", index);

    _audiodriver -> setErrorMessage (-1);

    if (! (alsalayer = dynamic_cast<AlsaLayer*> (getAudioDriver()))) {
        _warn ("Manager: Error: No audio driver");
        return;
    }

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

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

}

/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList (void)
{
    AlsaLayer *audiolayer;
    std::vector<std::string> devices;

    audiolayer = dynamic_cast<AlsaLayer *> (getAudioDriver());

    if (audiolayer)
        devices = audiolayer->getSoundCardsInfo (SFL_PCM_CAPTURE);

    return devices;
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex ()
{
    _debug ("Get current audio devices index");
    std::vector<std::string> v;
    std::stringstream ssi, sso, ssr;
    sso << _audiodriver->getIndexOut();
    v.push_back (sso.str());
    ssi << _audiodriver->getIndexIn();
    v.push_back (ssi.str());
    ssr << _audiodriver->getIndexRing();
    v.push_back (ssr.str());
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

int ManagerImpl::isRingtoneEnabled (const AccountID& id)
{
    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled() ? 1 : 0;
}

void ManagerImpl::ringtoneEnabled (const AccountID& id)
{

    Account *account = getAccount (id);

    if (!account) {
        _warn ("Manager: Warning: invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled (false) : account->setRingtoneEnabled (true);

}

std::string ManagerImpl::getRingtoneChoice (const AccountID& id)
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

void ManagerImpl::setRingtoneChoice (const std::string& tone, const AccountID& id)
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

bool ManagerImpl::getMd5CredentialHashing (void)
{
    return preferences.getMd5Hash();
}



void ManagerImpl::setRecordingCall (const CallID& id)
{

    Recordable* rec = NULL;

    if (!isConference (id)) {
        _debug ("Manager: Set recording for call %s", id.c_str());
        AccountID accountid = getAccountFromCall (id);
        rec = (Recordable *) getAccountLink (accountid)->getCall (id);
    } else {
        _debug ("Manager: Ser recording for conference %s", id.c_str());
        ConferenceMap::iterator it = _conferencemap.find (id);
        rec = (Recordable *) it->second;
    }

    if (rec)
        rec->setRecording();

}

bool ManagerImpl::isRecording (const CallID& id)
{

    AccountID accountid = getAccountFromCall (id);
    Recordable* rec = (Recordable*) getAccountLink (accountid)->getCall (id);

    bool ret = false;

    if (rec)
        ret = rec->isRecording();

    return ret;
}


void ManagerImpl::setHistoryLimit (const int& days)
{
    preferences.setHistoryLimit (days);
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
    preferences.getNotifyMails() ? preferences.setNotifyMails (true) : preferences.setNotifyMails (false);
}

void ManagerImpl::setAudioManager (const int32_t& api)
{

    int type;
    std::string alsaPlugin;

    _debug ("Setting audio manager ");

    if (!_audiodriver)
        return;

    type = _audiodriver->getLayerType();

    if (type == api) {
        _debug ("Audio manager chosen already in use. No changes made. ");
        return;
    }

    preferences.setAudioApi (api);

    switchAudioManager();
    return;

}

int32_t ManagerImpl::getAudioManager (void)
{
    return preferences.getAudioApi();
}


void ManagerImpl::notifyErrClient (const int32_t& errCode)
{
    if (_dbus) {
        _debug ("NOTIFY ERR NUMBER %i" , errCode);
        _dbus -> getConfigurationManager() -> errorAlert (errCode);
    }
}

int ManagerImpl::getAudioDeviceIndex (const std::string name)
{
    AlsaLayer *alsalayer;

    _debug ("Get audio device index");

    alsalayer = dynamic_cast<AlsaLayer *> (getAudioDriver());

    if (alsalayer)
        return alsalayer -> soundCardGetIndex (name);
    else
        return 0;
}

std::string ManagerImpl::getCurrentAudioOutputPlugin (void)
{
    AlsaLayer *alsalayer;

    _debug ("Get alsa plugin");

    alsalayer = dynamic_cast<AlsaLayer *> (getAudioDriver());

    if (alsalayer)
        return alsalayer -> getAudioPlugin();
    else
        return audioPreference.getPlugin();
}


std::string ManagerImpl::getEchoCancelState (void)
{

    std::string state;

    state = audioPreference.getEchoCancel() ? "enabled" : "disabled";

    return state;
}

void ManagerImpl::setEchoCancelState (std::string state)
{
    _debug ("Manager: Set echo suppress state: %s", state.c_str());

    bool isEnabled = state == "enabled" ? true : false;

    audioPreference.setEchoCancel (isEnabled);

    if (_audiodriver) {
        _audiodriver->setEchoCancelState (isEnabled);
    }
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

    bool isEnabled = state == "enabled" ? true : false;

    audioPreference.setNoiseReduce (isEnabled);

    if (_audiodriver) {
        _audiodriver->setNoiseSuppressState (isEnabled);
    }
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

    _debugInit ("AudioLayer Creation");

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
    } else
        _debug ("Error - Audio API unknown");

    if (_audiodriver == 0) {
        _debug ("Init audio driver error");
        return false;
    } else {
        error = getAudioDriver()->getErrorMessage();

        if (error == -1) {
            _debug ("Init audio driver: %i", error);
            return false;
        }
    }

    return true;

}

/**
 * Initialization: Main Thread and gui
 */
void ManagerImpl::selectAudioDriver (void)
{
    int layer, numCardIn, numCardOut, numCardRing, sampleRate, frameSize;
    std::string alsaPlugin;
    AlsaLayer *alsalayer;

    layer = _audiodriver->getLayerType();
    _debug ("Audio layer type: %i" , layer);

    /* Retrieve the global devices info from the user config */
    alsaPlugin = audioPreference.getPlugin();
    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    sampleRate = _mainBuffer.getInternalSamplingRate();
    frameSize = audioPreference.getFramesize();

    /* Only for the ALSA layer, we check the sound card information */

    if (layer == ALSA) {
        alsalayer = dynamic_cast<AlsaLayer*> (getAudioDriver());

        if (!alsalayer -> soundCardIndexExist (numCardIn, SFL_PCM_CAPTURE)) {
            _debug (" Card with index %i doesn't exist or cannot capture. Switch to 0.", numCardIn);
            numCardIn = ALSA_DFT_CARD_ID;
            audioPreference.setCardin (ALSA_DFT_CARD_ID);
        }

        if (!alsalayer -> soundCardIndexExist (numCardOut, SFL_PCM_PLAYBACK)) {
            _debug (" Card with index %i doesn't exist or cannot playback. Switch to 0.", numCardOut);
            numCardOut = ALSA_DFT_CARD_ID;
            audioPreference.setCardout (ALSA_DFT_CARD_ID);
        }

        if (!alsalayer->soundCardIndexExist (numCardRing, SFL_PCM_RINGTONE)) {
            _debug (" Card with index %i doesn't exist or cannot ringtone. Switch to 0.", numCardRing);
            numCardRing = ALSA_DFT_CARD_ID;
            audioPreference.setCardring (ALSA_DFT_CARD_ID);
        }
    }

    _audiodriver->setErrorMessage (-1);

    /* Open the audio devices */
    _audiodriver->openDevice (numCardIn, numCardOut, numCardRing, sampleRate, frameSize,
                              SFL_PCM_BOTH, alsaPlugin);

    /* Notify the error if there is one */

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

}

void ManagerImpl::switchAudioManager (void)
{
    int type, samplerate, framesize, numCardIn, numCardOut, numCardRing;
    std::string alsaPlugin;

    _debug ("Manager: Switching audio manager ");

    if (!_audiodriver)
        return;

    type = _audiodriver->getLayerType();

    samplerate = _mainBuffer.getInternalSamplingRate();
    framesize = audioPreference.getFramesize();

    _debug ("Manager: samplerate: %i, framesize %i", samplerate, framesize);

    alsaPlugin = audioPreference.getPlugin();

    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    _debug ("Manager: Deleting current layer... ");

    // ost::MutexLock lock (*getAudioLayerMutex());
    getAudioLayerMutex()->enter();

    // _audiodriver->closeLayer();
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

    _debug ("Manager: Current device: %i ", type);
    _debug ("Manager: Has current call: %i ", hasCurrentCall());

    if (hasCurrentCall())
        _audiodriver->startStream();

    // ost::MutexLock unlock (*getAudioLayerMutex());
    getAudioLayerMutex()->leave();

    // need to stop audio streams if there is currently no call
    // if ( (type != PULSEAUDIO) && (!hasCurrentCall())) {
    // _debug("There is currently a call!!");
    // _audiodriver->stopStream();

    // }
}

void ManagerImpl::audioSamplingRateChanged (void)
{

    int type, samplerate, framesize, numCardIn, numCardOut, numCardRing;
    std::string alsaPlugin;

    _debug ("Manager: Audio Sampling Rate");

    if (!_audiodriver)
        return;

    type = _audiodriver->getLayerType();

    samplerate = _mainBuffer.getInternalSamplingRate();
    framesize = audioPreference.getFramesize();

    _debug ("Mnager: samplerate: %i, framesize %i", samplerate, framesize);

    alsaPlugin = audioPreference.getPlugin();

    numCardIn = audioPreference.getCardin();
    numCardOut = audioPreference.getCardout();
    numCardRing = audioPreference.getCardring();

    _debug ("Manager: Deleting current layer... ");

    // ost::MutexLock lock (*getAudioLayerMutex());
    getAudioLayerMutex()->enter();

    // _audiodriver->closeLayer();
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

    _audiodriver->setErrorMessage (-1);

    _audiodriver->openDevice (numCardIn, numCardOut, numCardRing, samplerate, framesize,
                              SFL_PCM_BOTH, alsaPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    _debug ("Manager: Current device: %i ", type);
    _debug ("Manager: Has current call: %i ", hasCurrentCall());

    if (_audiodriver) {
        unsigned int sampleRate = _audiodriver->getSampleRate();

        delete _telephoneTone;

        _debugInit ("Manager: Load telephone tone");
        std::string country = preferences.getZoneToneChoice();
        _telephoneTone = new TelephoneTone (country, sampleRate);


        delete _dtmfKey;

        _debugInit ("Manager: Loading DTMF key");
        _dtmfKey = new DTMF (sampleRate);
    }

    if (hasCurrentCall())
        _audiodriver->startStream();

    // ost::MutexLock unlock (*getAudioLayerMutex());
    getAudioLayerMutex()->leave();

    // need to stop audio streams if there is currently no call
    // if ( (type != PULSEAUDIO) && (!hasCurrentCall())) {
    // _debug("There is currently a call!!");
    // _audiodriver->stopStream();

    // }
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

    /* Only for PulseAudio */
    pulselayer = dynamic_cast<PulseLayer*> (getAudioDriver());

    if (pulselayer) {
        if (pulselayer->getLayerType() == PULSEAUDIO) {
            if (pulselayer)
                pulselayer->setPlaybackVolume (spkr_vol);
        }
    }
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
        return (_config.getConfigTreeItemValue (section, name) == TRUE_STR) ? true
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

            if (iter->second != NULL && iter->first != IP2IP_PROFILE) {
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
                if (iter->second != NULL && iter->first != IP2IP_PROFILE) {
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
    const AccountID& accountID)
{

    _debug ("Manager: get account details %s", accountID.c_str());

    Account * account;

    if (! (account = _accountMap[accountID])) {
        _debug ("Manager: Get account details on a non-existing accountID %s. Returning default", accountID.c_str());
        // return a default map
        return defaultAccount.getAccountDetails();
    } else
        return account->getAccountDetails();

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

    _debug ("Manager: Set account details %s", accountID.c_str());

    Account* account;

    if (! (account = getAccount (accountID))) {
        _warn ("Manager: Cannot setAccountDetails on a non-existing accountID %s.", accountID.c_str());
        return;
    }

    account->setAccountDetails (details);

    saveConfig();

    if (account->isEnabled())
        account->registerVoIPLink();
    else
        account->unregisterVoIPLink();

    // Update account details to the client side
    if (_dbus)
        _dbus->getConfigurationManager()->accountsChanged();

}

std::string ManagerImpl::addAccount (
    const std::map<std::string, std::string>& details)
{

    /** @todo Deal with both the _accountMap and the Configuration */
    std::string accountType, account_list;
    Account* newAccount;
    std::stringstream accountID;
    AccountID newAccountID;

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

void ManagerImpl::deleteAllCredential (const AccountID& accountID)
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

void ManagerImpl::removeAccount (const AccountID& accountID)
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
        const AccountID& accountID)
{
    if (getAccountFromCall (callID) == AccountNULL) { // nothing with the same ID
        if (accountExists (accountID)) { // account id exist in AccountMap
            ost::MutexLock m (_callAccountMapMutex);
            _callAccountMap[callID] = accountID;
            _debug ("Associate Call %s with Account %s", callID.data(), accountID.data());
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

AccountID ManagerImpl::getAccountFromCall (const CallID& callID)
{
    ost::MutexLock m (_callAccountMapMutex);
    CallAccountMap::iterator iter = _callAccountMap.find (callID);

    if (iter == _callAccountMap.end()) {
        return AccountNULL;
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

CallID ManagerImpl::getNewCallID ()
{
    std::ostringstream random_id ("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (getAccountFromCall (random_id.str()) != AccountNULL) {
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

    if (_directIpAccount == NULL) {
        _error ("Manager: Failed to create default \"account\"");
        return;
    }

    // If configuration file parsed, load saved preferences
    if (_setupLoaded) {

        _debug ("Manager: Loading IP2IP profile preferences from config");

        Conf::SequenceNode *seq = parser->getAccountSequence();

        Conf::Sequence::iterator iterIP2IP = seq->getSequence()->begin();
        Conf::Key accID ("id");

        // Iterate over every account maps
        while (iterIP2IP != seq->getSequence()->end()) {

            Conf::MappingNode *map = (Conf::MappingNode *) (*iterIP2IP);

            // Get the account id
            Conf::ScalarNode * val = (Conf::ScalarNode *) (map->getValue (accID));
            Conf::Value accountid = val->getValue();

            // if ID is IP2IP, unserialize
            if (accountid == "IP2IP") {

                try {
                    _directIpAccount->unserialize (map);
                } catch (SipAccountException &e) {
                    _error ("Manager: %s", e.what());
                }

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

    // _directIpAccount->setVoIPLink(SIPVoIPLink::instance (""));
    _directIpAccount->setVoIPLink();

}

short ManagerImpl::loadAccountMap()
{

    _debug ("Manager: Load account map");

    // Conf::YamlParser *parser;
    int nbAccount = 0;

    if (!_setupLoaded)
        return 0;

    // build preferences
    preferences.unserialize ( (Conf::MappingNode *) (parser->getPreferenceSequence()));
    voipPreferences.unserialize ( (Conf::MappingNode *) (parser->getVoipPreferenceSequence()));
    addressbookPreference.unserialize ( (Conf::MappingNode *) (parser->getAddressbookSequence()));
    hookPreference.unserialize ( (Conf::MappingNode *) (parser->getHookSequence()));
    audioPreference.unserialize ( (Conf::MappingNode *) (parser->getAudioSequence()));
    shortcutPreferences.unserialize ( (Conf::MappingNode *) (parser->getShortcutSequence()));

    Conf::SequenceNode *seq = parser->getAccountSequence();

    // Each element in sequence is a new account to create
    Conf::Sequence::iterator iterSeq = seq->getSequence()->begin();

    Conf::Key accTypeKey ("type");
    Conf::Key accID ("id");

    while (iterSeq != seq->getSequence()->end()) {

        Account *tmpAccount = NULL;
        Conf::MappingNode *map = (Conf::MappingNode *) (*iterSeq);

        Conf::ScalarNode * val = (Conf::ScalarNode *) (map->getValue (accTypeKey));
        Conf::Value accountType = val->getValue();

        val = (Conf::ScalarNode *) (map->getValue (accID));
        Conf::Value accountid = val->getValue();

        if (accountType == "SIP" && accountid != "IP2IP") {
            _debug ("Manager: Create SIP account: %s", accountid.c_str());
            tmpAccount = AccountCreator::createAccount (AccountCreator::SIP_ACCOUNT, accountid);
        } else if (accountType == "IAX" && accountid != "IP2IP") {
            _debug ("Manager: Create IAX account: %s", accountid.c_str());
            tmpAccount = AccountCreator::createAccount (AccountCreator::IAX_ACCOUNT, accountid);
        }


        if (tmpAccount != NULL) {

            try {
                tmpAccount->unserialize (map);
            } catch (SipAccountException &e) {
                _error ("Manager: %s", e.what());
            }

            _accountMap[accountid] = tmpAccount;
            _debug ("Manager: Loading account %s (size %d)", accountid.c_str(), _accountMap.size());

            tmpAccount->setVoIPLink();
            nbAccount++;
        }

        iterSeq++;
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

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {

        _debug ("Unloading account %s", iter->first.c_str());

        delete iter->second;
        iter->second = NULL;

        iter++;
    }

    _debug ("Manager: Clear account map");
    _accountMap.clear();
    _debug ("Manager: Unload account map");


}

bool ManagerImpl::accountExists (const AccountID& accountID)
{
    AccountMap::iterator iter = _accountMap.find (accountID);

    if (iter == _accountMap.end()) {
        return false;
    }

    return true;
}

Account*
ManagerImpl::getAccount (const AccountID& accountID)
{
    // In our definition,
    // this is the "direct ip calls account"
    if (accountID == AccountNULL) {
        _debug ("Returns the direct IP account");
        return _directIpAccount;
    }

    AccountMap::iterator iter = _accountMap.find (accountID);

    if (iter == _accountMap.end()) {
        return NULL;
    }

    return iter->second;
}

AccountID ManagerImpl::getAccountIdFromNameAndServer (
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

    // Failed again! return AccountNULL
    return AccountNULL;
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


    addressbookPreference.setEnabled ( (settings.find ("ADDRESSBOOK_ENABLE")->second == 1) ? true : false);
    addressbookPreference.setMaxResults (settings.find ("ADDRESSBOOK_MAX_RESULTS")->second);
    addressbookPreference.setPhoto ( (settings.find ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")->second == 1) ? true : false);
    addressbookPreference.setBusiness ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")->second == 1) ? true : false);
    addressbookPreference.setHone ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_HOME")->second == 1) ? true : false);
    addressbookPreference.setMobile ( (settings.find ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")->second == 1) ? true : false);

    // Write it to the configuration file
    saveConfig();
}

void ManagerImpl::setAddressbookList (const std::vector<std::string>& list)
{

    std::string s = serialize (list);
    addressbookPreference.setList (s);

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

    hookPreference.setIax2Enabled ( (settings.find ("URLHOOK_IAX2_ENABLED")->second == "true") ? true : false);
    hookPreference.setNumberAddPrefix (settings.find ("PHONE_NUMBER_HOOK_ADD_PREFIX")->second);
    hookPreference.setNumberEnabled ( (settings.find ("PHONE_NUMBER_HOOK_ENABLED")->second == "true") ? true : false);
    hookPreference.setSipEnabled ( (settings.find ("URLHOOK_SIP_ENABLED")->second == "true") ? true : false);
    hookPreference.setUrlCommand (settings.find ("URLHOOK_COMMAND")->second);
    hookPreference.setUrlSipField (settings.find ("URLHOOK_SIP_FIELD")->second);

    // Write it to the configuration file
    saveConfig();
}

void ManagerImpl::check_call_configuration (const CallID& id,
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
        _debug ("Manager: Associate call %s with config %i", callID.c_str(), config);
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
    AccountID accountid;
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
    if ( (account = getAccount (accountid)) != 0) {
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
        call_details.insert (std::pair<std::string, std::string> ("ACCOUNTID", AccountNULL));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NUMBER", "Unknown"));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NAME", "Unknown"));
        call_details.insert (std::pair<std::string, std::string> ("CALL_STATE", "UNKNOWN"));
        call_details.insert (std::pair<std::string, std::string> ("CALL_TYPE", "0"));
    }

    return call_details;
}

std::map<std::string, std::string> ManagerImpl::send_history_to_client (void)
{
    return _history->get_history_serialized();
}

void ManagerImpl::receive_history_from_client (std::map<std::string,
        std::string> history)
{

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


