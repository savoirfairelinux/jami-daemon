/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
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


#define fill_config_str(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_str)))
#define fill_config_int(name, value) \
  (_config.addConfigTreeItem(section, Conf::ConfigTreeItem(std::string(name), std::string(value), type_int)))

#define MD5_APPEND(pms,buf,len) pj_md5_update(pms, (const pj_uint8_t*)buf, len)

ManagerImpl::ManagerImpl (void)
        : _hasTriedToRegister (false)
        , _config()
        , _currentCallId2()
        , _currentCallMutex()
        , _codecBuilder (NULL)
        , _audiodriver (NULL)
        , _dtmfKey (NULL)
        , _codecDescriptorMap()
        , _toneMutex()
        , _telephoneTone (NULL)
        , _audiofile()
        , _spkr_volume (0)
        , _mic_volume (0)
        , _mutex()
        , _dbus (NULL)
        , _waitingCall()
        , _waitingCallMutex()
        , _nbIncomingWaitingCall (0)
        , _path ("")
        , _exist (0)
        , _setupLoaded (false)
        , _callAccountMap()
        , _callAccountMapMutex()
        , _callConfigMap()
        , _accountMap()
        , _directIpAccount (NULL)
        , _cleaner (NULL)
        , _history (NULL)
{

    // initialize random generator for call id
    srand (time (NULL));

    _cleaner = new NumberCleaner ();
    _history = new HistoryManager ();

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
    // terminate();
    delete _cleaner;
    _cleaner=0;
    _debug ("%s stop correctly.\n", PROGNAME);
}

void
ManagerImpl::init()
{
    // Load accounts, init map
    loadAccountMap();

    initVolume();

    if (_exist == 0) {
        _debug ("Cannot create config file in your home directory\n");
    }

    initAudioDriver();

    selectAudioDriver();

    // Initialize the list of supported audio codecs
    initAudioCodec();

    AudioLayer *audiolayer = getAudioDriver();

    if (audiolayer != 0) {
        unsigned int sampleRate = audiolayer->getSampleRate();

        _debugInit ("Load Telephone Tone");
        std::string country = getConfigString (PREFERENCES, ZONE_TONE);
        _telephoneTone = new TelephoneTone (country, sampleRate);

        _debugInit ("Loading DTMF key");
        _dtmfKey = new DTMF (sampleRate);
    }

    if (audiolayer == 0)
        audiolayer->stopStream();


    // Load the history
    _history->load_history (getConfigInt (PREFERENCES, CONFIG_HISTORY_LIMIT));
}

void ManagerImpl::terminate()
{
    _debug ("ManagerImpl::terminate \n");
    saveConfig();

    unloadAccountMap();

    _debug ("Unload DTMF Key \n");
    delete _dtmfKey;

    _debug ("Unload Audio Driver \n");
    delete _audiodriver;
    _audiodriver = NULL;

    _debug ("Unload Telephone Tone \n");
    delete _telephoneTone;
    _telephoneTone = NULL;

    _debug ("Unload Audio Codecs \n");
    _codecDescriptorMap.deleteHandlePointer();

}

bool
ManagerImpl::isCurrentCall (const CallID& callId)
{
    return (_currentCallId2 == callId ? true : false);
}

bool
ManagerImpl::hasCurrentCall()
{
    // _debug ("ManagerImpl::hasCurrentCall current call ID = %s\n", _currentCallId2.c_str());

    if (_currentCallId2 != "") {
        return true;
    }

    return false;
}

const CallID&
ManagerImpl::getCurrentCallId()
{
    return _currentCallId2;
}

void
ManagerImpl::switchCall (const CallID& id)
{
    ost::MutexLock m (_currentCallMutex);
    _debug ("----- Switch current call id to %s -----\n", id.c_str());
    _currentCallId2 = id;

    /*
    AudioLayer *al = getAudioDriver();

    if (id != "") {

    if(isConference(id)) {

        Conference *conf;

        ConferenceMap::iterator iter = _conferencemap.find(id);
        if(iter != _conferencemap.end())
        {
    	_debug("    set call recordable in audio layer\n");
    	conf = iter->second;
    	al->setRecorderInstance((Recordable*)conf);
        }
    }
    else {

        // set the recordable instance in audiolayer
        AccountID account_id = getAccountFromCall(id);


        Call *call = NULL;
        call = getAccountLink (account_id)->getCall(id);

        _debug("    set call recordable in audio layer\n");
        al->setRecorderInstance((Recordable*)call);
    }
    }
    */
}


///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool
ManagerImpl::outgoingCall (const std::string& account_id, const CallID& call_id, const std::string& to)
{
    std::string pattern, to_cleaned;
    Call::CallConfiguration callConfig;
    SIPVoIPLink *siplink;

    _debug ("ManagerImpl::outgoingCall(%s)\n", call_id.c_str());

    CallID current_call_id = getCurrentCallId();

    if (getConfigString (HOOKS, PHONE_NUMBER_HOOK_ENABLED) ==  "1")
        _cleaner->set_phone_number_prefix (getConfigString (HOOKS, PHONE_NUMBER_HOOK_ADD_PREFIX));
    else
        _cleaner->set_phone_number_prefix ("");

    to_cleaned = _cleaner->clean (to);

    /* Check what kind of call we are dealing with */
    check_call_configuration (call_id, to_cleaned, &callConfig);

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        _debug ("    outgoingCall: Has current call (%s) put it onhold\n", current_call_id.c_str());

        // if this is not a conferenceand this and is not a conference participant

        if (!isConference (current_call_id) && !participToConference (current_call_id)) {
            _debug ("    outgoingCall: Put the current call (%s) on hold\n", current_call_id.c_str());
            onHoldCall (current_call_id);
        } else if (isConference (current_call_id) && !participToConference (call_id)) {
            _debug ("    outgoingCall: detach main participant from conference\n");
            detachParticipant (default_id, current_call_id);
        }
    }

    if (callConfig == Call::IPtoIP) {
        _debug ("    outgoingCall: Start IP to IP call\n");
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

    if (!accountExists (account_id)) {
        _debug ("! Manager Error: Outgoing Call: account doesn't exist\n");
        return false;
    }

    if (getAccountFromCall (call_id) != AccountNULL) {
        _debug ("! Manager Error: Outgoing Call: call id already exists\n");
        return false;
    }


    _debug ("- Manager Action: Adding Outgoing Call %s on account %s\n", call_id.data(), account_id.data());

    associateCallToAccount (call_id, account_id);

    if (getAccountLink (account_id)->newOutgoingCall (call_id, to_cleaned)) {
        switchCall (call_id);
        return true;
    } else {
        callFailure (call_id);
        _debug ("! Manager Error: An error occur, the call was not created\n");
    }

    return false;
}

//THREAD=Main : for outgoing Call
bool
ManagerImpl::answerCall (const CallID& call_id)
{

    _debug ("ManagerImpl::answerCall(%s)\n", call_id.c_str());

    stopTone ();

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    AccountID account_id = getAccountFromCall (call_id);

    if (account_id == AccountNULL) {
        _debug ("    answerCall: AccountId is null\n");
    }

    Call* call = NULL;

    call = getAccountLink (account_id)->getCall (call_id);

    if (call == NULL) {
        _debug ("    answerCall: Call is null\n");
    }

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        _debug ("    answerCall: Currently conversing with %s\n", current_call_id.c_str());
        // if it is not a conference and is not a conference participant

        if (!isConference (current_call_id) && !participToConference (current_call_id)) {
            _debug ("    answerCall: Put the current call (%s) on hold\n", current_call_id.c_str());
            onHoldCall (current_call_id);
        }

        // if we are talking to a conference and we are answering an incoming call
        else if (isConference (current_call_id) && !participToConference (call_id)) {
            _debug ("    answerCall: Detach main participant from conference\n");
            detachParticipant (default_id, current_call_id);
        }

    }


    if (!getAccountLink (account_id)->answer (call_id)) {
        // error when receiving...
        removeCallAccount (call_id);
        return false;
    }

    // if it was waiting, it's waiting no more
    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "CURRENT");

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
bool
ManagerImpl::hangupCall (const CallID& call_id)
{
    _debug ("ManagerImpl::hangupCall(%s)\n", call_id.c_str());
    PulseLayer *pulselayer;
    AccountID account_id;
    bool returnValue;

    // store the current call id
    CallID current_call_id = getCurrentCallId();

    stopTone ();

    /* Broadcast a signal over DBus */
    _debug ("    hangupCall: Send DBUS call state change (HUNGUP) for id %s\n", call_id.c_str());

    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");

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

        if (account_id == AccountNULL) {
            _debug ("! Manager Hangup Call: Call doesn't exists\n");
            return false;
        }

        returnValue = getAccountLink (account_id)->hangup (call_id);

        removeCallAccount (call_id);
    }

    int nbCalls = getCallList().size();

    AudioLayer *audiolayer = getAudioDriver();

    // stop streams
    if (audiolayer && (nbCalls <= 1)) {
        _debug ("    hangupCall: stop audio stream, ther is only %i call(s) remaining", nbCalls);
        audiolayer->stopStream();
    }

    if (_audiodriver->getLayerType() == PULSEAUDIO) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());
    }

    return returnValue;
}


bool
ManagerImpl::hangupConference (const ConfID& id)
{
    _debug ("ManagerImpl::hangupConference()\n");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    AccountID currentAccountId;

    // Call* call = NULL;


    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("ManagerImpl::hangupConference participant %s\n", (*iter_participant).c_str());

            hangupCall (*iter_participant);

            iter_participant++;

        }

    }

    switchCall ("");

    return true;
}


//THREAD=Main
bool
ManagerImpl::cancelCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;

    stopTone ();

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)->cancel (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("! Manager Cancel Call: Call doesn't exists\n");
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
bool
ManagerImpl::onHoldCall (const CallID& call_id)
{
    AccountID account_id;
    bool returnValue;

    _debug ("ManagerImpl::onHoldCall(%s)\n", call_id.c_str());

    stopTone ();

    CallID current_call_id = getCurrentCallId();

    _debug ("    onHoldCall: try to put call %s on hold\n", call_id.c_str());

    /* Direct IP to IP call */

    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> onhold (call_id);
    }

    /* Classic call, attached to an account */
    else {
        account_id = getAccountFromCall (call_id);

        if (account_id == AccountNULL) {
            _debug ("    onHoldCall: Account ID %s or callid %s doesn't exists\n", account_id.c_str(), call_id.c_str());
            return false;
        }

        returnValue = getAccountLink (account_id)->onhold (call_id);
    }

    removeWaitingCall (call_id);

    // keeps current call id if the action is not holding this call or a new outgoing call

    if (current_call_id == call_id) {

        switchCall ("");
    }

    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "HOLD");

    return returnValue;
}

//THREAD=Main
bool
ManagerImpl::offHoldCall (const CallID& call_id)
{

    AccountID account_id;
    bool returnValue, is_rec;
    std::string codecName;

    is_rec = false;

    _debug ("ManagerImpl::offHoldCall(%s)\n", call_id.c_str());

    stopTone ();

    CallID current_call_id = getCurrentCallId();

    //Place current call on hold if it isn't

    if (hasCurrentCall()) {
        // if this is not a conferenceand this and is not a conference participant
        if (!isConference (current_call_id) && !participToConference (current_call_id)) {
            _debug ("    offHoldCall: put current call (%s) on hold\n", current_call_id.c_str());
            onHoldCall (current_call_id);
        } else if (isConference (current_call_id) && !participToConference (call_id)) {
            _debug ("    offHoldCall Put current conference (%s) on hold\n", current_call_id.c_str());
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
            _debug ("Manager OffHold Call: Call doesn't exists\n");
            return false;
        }

        _debug ("Setting OFFHOLD, Account %s, callid %s\n", account_id.c_str(), call_id.c_str());

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


    // codecName = getCurrentCodecName (call_id);
    // _debug("ManagerImpl::hangupCall(): broadcast codec name %s \n",codecName.c_str());

    // if (_dbus) _dbus->getCallManager()->currentSelectedCodec (call_id,codecName.c_str());


    return returnValue;
}

//THREAD=Main
bool
ManagerImpl::transferCall (const CallID& call_id, const std::string& to)
{
    AccountID accountid;
    bool returnValue;

    stopTone ();

    CallID current_call_id = getCurrentCallId();

    /* Direct IP to IP call */

    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> transfer (call_id, to);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (call_id);

        if (accountid == AccountNULL) {
            _debug ("! Manager Transfer Call: Call doesn't exists\n");
            return false;
        }

        returnValue = getAccountLink (accountid)->transfer (call_id, to);

        removeCallAccount (call_id);
    }

    removeWaitingCall (call_id);

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

    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");

    return returnValue;
}

void ManagerImpl::transferFailed()
{
    if (_dbus) _dbus->getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceded()
{
    if (_dbus) _dbus->getCallManager()->transferSucceded();

}


//THREAD=Main : Call:Incoming
bool
ManagerImpl::refuseCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;

    CallID current_call_id = getCurrentCallId();

    stopTone ();


    int nbCalls = getCallList().size();

    // AudioLayer* audiolayer = getAudioDriver();

    if (nbCalls <= 1) {
        _debug ("    hangupCall: stop audio stream, ther is only %i call(s) remaining\n", nbCalls);

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
            _debug ("! Manager OffHold Call: Call doesn't exists\n");
            return false;
        }

        returnValue = getAccountLink (accountid)->refuse (id);

        removeCallAccount (id);
    }

    // if the call was outgoing or established, we didn't refuse it
    // so the method did nothing
    if (returnValue) {
        removeWaitingCall (id);

        if (_dbus) _dbus->getCallManager()->callStateChanged (id, "HUNGUP");

        // if(current_call_id.compare("") != 0)
        // switchCall ("");
    }



    return returnValue;
}


Conference*
ManagerImpl::createConference (const CallID& id1, const CallID& id2)
{
    _debug ("ManagerImpl::createConference()\n");

    Conference* conf = new Conference();

    // _conferencecall.insert(pair<CallID, Conference*>(id1, conf));
    // _conferencecall.insert(pair<CallID, Conference*>(id2, conf));
    _conferencemap.insert (pair<CallID, Conference*> (conf->getConfID(), conf));

    conf->add (id1);
    conf->add (id2);

    // broadcast a signal over dbus
    _dbus->getCallManager()->conferenceCreated (conf->getConfID());

    return conf;
}

void
ManagerImpl::removeConference (const ConfID& conference_id)
{

    _debug ("ManagerImpl::removeConference(%s)\n", conference_id.c_str());

    Conference* conf = NULL;

    _debug ("    removeConference: _conferencemap.size: %i\n", (int) _conferencemap.size());
    ConferenceMap::iterator iter = _conferencemap.find (conference_id);

    if (iter != _conferencemap.end()) {
        _debug ("    removeConference: Found conference id %s in conferencemap\n", conference_id.c_str());
        conf = iter->second;
    }

    if (conf == NULL) {

        _debug ("    removeConference: Error conference not found\n");
        return;
    }


    // We now need to bind the audio to the remain participant

    // unbind main participant from conference (just to be sure)
    _audiodriver->getMainBuffer()->unBindAll (default_id);

    ParticipantSet participants = conf->getParticipantList();

    // bind main participant to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end()) {

        // to avoid puting onhold the call
        // switchCall("");
        _audiodriver->getMainBuffer()->bindCallID (*iter_p, default_id);
    }

    // Then remove the conference from the conference map
    _debug ("ManagerImpl:: remove conference %s\n", conference_id.c_str());

    if (_conferencemap.erase (conference_id) == 1)
        _debug ("ManagerImpl:: conference %s removed succesfully\n", conference_id.c_str());
    else
        _debug ("ManagerImpl:: error cannot remove conference id: %s\n", conference_id.c_str());

    // broadcast a signal over dbus
    _debug ("ManagerImpl::removeConference broadcast call removed on dbus: %s\n", conference_id.c_str());

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

void
ManagerImpl::holdConference (const CallID& id)
{
    _debug ("ManagerImpl::holdConference()\n");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    AccountID currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("    holdConference: participant %s\n", (*iter_participant).c_str());
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);

            switchCall (*iter_participant);
            onHoldCall (*iter_participant);

            iter_participant++;

        }

        conf->setState (Conference::Hold);

        _dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());

    }




}


void
ManagerImpl::unHoldConference (const CallID& id)
{

    _debug ("ManagerImpl::unHoldConference()\n");

    Conference *conf;
    ConferenceMap::iterator iter_conf = _conferencemap.find (id);

    AccountID currentAccountId;

    Call* call = NULL;

    if (iter_conf != _conferencemap.end()) {
        conf = iter_conf->second;

        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end()) {
            _debug ("    unholdConference: participant %s\n", (*iter_participant).c_str());
            currentAccountId = getAccountFromCall (*iter_participant);
            call = getAccountLink (currentAccountId)->getCall (*iter_participant);

            offHoldCall (*iter_participant);

            iter_participant++;

        }

        conf->setState (Conference::Active_Atached);

        _dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());

    }

}

bool
ManagerImpl::isConference (const CallID& id)
{
    ConferenceMap::iterator iter = _conferencemap.find (id);

    if (iter == _conferencemap.end()) {
        return false;
    } else {
        return true;
    }
}

bool
ManagerImpl::participToConference (const CallID& call_id)
{

    AccountID accountId;

    Call* call = NULL;

    accountId = getAccountFromCall (call_id);
    call = getAccountLink (accountId)->getCall (call_id);

    if (call == NULL)
        return false;

    if (call->getConfId() == "") {
        return false;
    } else {
        return true;
    }
}


void
ManagerImpl::addParticipant (const CallID& call_id, const CallID& conference_id)
{
    _debug ("ManagerImpl::addParticipant(%s, %s)\n", call_id.c_str(), conference_id.c_str());

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

    _debug ("    addParticipant: enter main process\n");

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

        _debug ("    addParticipant: call state: %s\n", iter_details->second.c_str());

        if (iter_details->second == "HOLD") {
            _debug ("    OFFHOLD %s\n", call_id.c_str());

            // offHoldCall create a new rtp session which use addStream to bind participant
            offHoldCall (call_id);
        } else if (iter_details->second == "INCOMING") {
            _debug ("    ANSWER %s\n", call_id.c_str());
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
        _debug ("    addParticipant: Error, conference %s conference_id not found!\n", conference_id.c_str());
    }

}

void
ManagerImpl::addMainParticipant (const CallID& conference_id)
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
            _audiodriver->getMainBuffer()->bindCallID (*iter_participant, default_id);

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

        _dbus->getCallManager()->conferenceChanged (conference_id, conf->getStateStr());

    }

    switchCall (conference_id);
}


void
ManagerImpl::joinParticipant (const CallID& call_id1, const CallID& call_id2)
{
    _debug ("ManagerImpl::joinParticipant(%s, %s)\n", call_id1.c_str(), call_id2.c_str());
    // _debug("    Current call ID %s\n", getCurrentCallId().c_str());

    std::map<std::string, std::string> call1_details = getCallDetails (call_id1);
    std::map<std::string, std::string> call2_details = getCallDetails (call_id2);

    std::map<std::string, std::string>::iterator iter_details;

    AccountID currentAccountId;
    Call* call = NULL;

    CallID current_call_id = getCurrentCallId();
    _debug ("    joinParticipant: current_call_id %s\n", current_call_id.c_str());


    // detach from the conference and switch to this conference

    if ( (current_call_id != call_id1) && (current_call_id != call_id2)) {
        if (isConference (current_call_id))
            detachParticipant (default_id, current_call_id);
        else
            onHoldCall (current_call_id);
    }

    _debug ("    joinParticipant: create a conference\n");

    Conference *conf = createConference (call_id1, call_id2);
    switchCall (conf->getConfID());

    currentAccountId = getAccountFromCall (call_id1);
    call = getAccountLink (currentAccountId)->getCall (call_id1);
    call->setConfId (conf->getConfID());

    iter_details = call1_details.find ("CALL_STATE");
    _debug ("    joinParticipant: call1 %s state: %s\n", call_id1.c_str(), iter_details->second.c_str());

    if (iter_details->second == "HOLD") {
        _debug ("    OFFHOLD %s\n", call_id1.c_str());
        offHoldCall (call_id1);
    } else if (iter_details->second == "INCOMING") {
        _debug ("    ANSWER %s\n", call_id1.c_str());
        answerCall (call_id1);
    } else if (iter_details->second == "CURRENT") {
        _debug ("    CURRENT %s\n", call_id1.c_str());
        _audiodriver->getMainBuffer()->unBindAll (call_id1);
        conf->bindParticipant (call_id1);
    } else if (iter_details->second == "INACTIVE") {
        _debug ("    INACTIVE %s\n", call_id1.c_str());
        answerCall (call_id1);
    } else {
        _debug ("    CAll State not recognized\n");
    }

    currentAccountId = getAccountFromCall (call_id2);

    call = getAccountLink (currentAccountId)->getCall (call_id2);
    call->setConfId (conf->getConfID());

    iter_details = call2_details.find ("CALL_STATE");
    _debug ("    joinParticipant: call2 %s state: %s\n", call_id2.c_str(), iter_details->second.c_str());

    if (iter_details->second == "HOLD") {
        _debug ("    OFFHOLD %s\n", call_id2.c_str());
        offHoldCall (call_id2);
    } else if (iter_details->second == "INCOMING") {
        _debug ("    ANSWER %s\n", call_id2.c_str());
        answerCall (call_id2);
    } else if (iter_details->second == "CURRENT") {
        _debug ("    CURRENT %s\n", call_id2.c_str());
        _audiodriver->getMainBuffer()->unBindAll (call_id2);
        conf->bindParticipant (call_id2);
    } else if (iter_details->second == "INACTIVE") {
        _debug ("    INACTIVE %s\n", call_id2.c_str());
        answerCall (call_id2);
    } else {
        _debug ("    CAll State not recognized\n");
    }

    // finally bind main participant to conference
    // addMainParticipant(default_conf);


    // switchCall(conf->getConfID());

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();

}


void
ManagerImpl::detachParticipant (const CallID& call_id, const CallID& current_id)
{
    _debug ("ManagerImpl::detachParticipant(%s)\n", call_id.c_str());

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

            _debug ("    detachParticipant: detaching participant %s\n", call_id.c_str());

            std::map<std::string, std::string> call_details = getCallDetails (call_id);
            std::map<std::string, std::string>::iterator iter_details;

            iter_details = call_details.find ("CALL_STATE");

            if (iter_details->second == "RINGING") {
                removeParticipant (call_id);
            } else {
                _debug ("    ONHOLD %s\n", call_id.c_str());
                onHoldCall (call_id);

                removeParticipant (call_id);

                processRemainingParticipant (current_call_id, conf);
            }
        } else {


            _debug ("    detachParticipant: call is not conferencing, cannot detach\n");

        }
    } else {
        _debug ("    detachParticipant: unbind main participant from all\n");
        _audiodriver->getMainBuffer()->unBindAll (default_id);

        if (isConference (current_call_id)) {

            ConferenceMap::iterator iter = _conferencemap.find (current_call_id);
            Conference *conf = iter->second;

            conf->setState (Conference::Active_Detached);

            _dbus->getCallManager()->conferenceChanged (conf->getConfID(), conf->getStateStr());
        }

        switchCall ("");

    }

}


void
ManagerImpl::removeParticipant (const CallID& call_id)
{
    _debug ("ManagerImpl::removeParticipant(%s)\n", call_id.c_str());

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
        _debug ("    no conference created, cannot remove participant \n");
    } else {

        conf = iter->second;

        _debug ("    removeParticipant %s\n", call_id.c_str());
        conf->remove (call_id);
        call->setConfId ("");

    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();

}


void
ManagerImpl::processRemainingParticipant (CallID current_call_id, Conference *conf)
{

    _debug ("ManagerImpl::processRemainingParticipant()\n");

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
        removeConference (conf->getConfID());

        switchCall ("");
    }

}

void
ManagerImpl::joinConference (const CallID& conf_id1, const CallID& conf_id2)
{
    _debug ("ManagerImpl::joinConference(%s, %s)\n", conf_id1.c_str(), conf_id2.c_str());

    ConferenceMap::iterator iter;

    Conference *conf1 = NULL;
    Conference *conf2 = NULL;

    iter = _conferencemap.find (conf_id1);

    if (iter != _conferencemap.end())
        conf1 = iter->second;

    iter = _conferencemap.find (conf_id2);

    if (iter != _conferencemap.end())
        conf2 = iter->second;

    ParticipantSet participants = conf1->getParticipantList();

    ParticipantSet::iterator iter_participant = participants.begin();

    while (iter_participant != participants.end()) {
        detachParticipant (*iter_participant, "");
        addParticipant (*iter_participant, conf_id2);

        iter_participant++;
    }

    // detachParticipant(default_id, "");

}

void
ManagerImpl::addStream (const CallID& call_id)
{
    _debug ("ManagerImpl::addStream %s\n", call_id.c_str());

    AccountID currentAccountId;
    Call* call = NULL;

    currentAccountId = getAccountFromCall (call_id);
    call = getAccountLink (currentAccountId)->getCall (call_id);

    if (participToConference (call_id)) {

        // bind to conference participant
        ConferenceMap::iterator iter = _conferencemap.find (call->getConfId());

        if (iter != _conferencemap.end()) {
            Conference* conf = iter->second;

            // _debug ("    addStream: bind call %s to conference %s\n", call_id.c_str(), conf->getConfID().c_str());

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

        // _debug ("    addStream: bind call %s to main\n", call_id.c_str());

        // bind to main
        getAudioDriver()->getMainBuffer()->bindCallID (call_id);

        // _audiodriver->getMainBuffer()->flush(default_id);
        _audiodriver->flushUrgent();
        _audiodriver->flushMain();

    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();
}

void
ManagerImpl::removeStream (const CallID& call_id)
{
    _debug ("ManagerImpl::removeStream %s\n", call_id.c_str());

    getAudioDriver()->getMainBuffer()->unBindAll (call_id);

    if (participToConference (call_id)) {
        removeParticipant (call_id);
    }

    if (_audiodriver)
        _audiodriver->getMainBuffer()->stateInfo();
}

//THREAD=Main
bool
ManagerImpl::saveConfig (void)
{
    _debug ("Saving Configuration to XDG directory %s ... \n", _path.c_str());
    setConfig (AUDIO, VOLUME_SPKR, getSpkrVolume());
    setConfig (AUDIO, VOLUME_MICRO, getMicVolume());

    _setupLoaded = _config.saveConfigTree (_path.data());
    return _setupLoaded;
}

//THREAD=Main
int
ManagerImpl::initRegisterAccounts()
{
    int status;
    bool flag = true;
    AccountMap::iterator iter;

    _debugInit ("Initiate VoIP Links Registration");
    iter = _accountMap.begin();

    /* Loop on the account map previously loaded */

    while (iter != _accountMap.end()) {
        if (iter->second) {
            iter->second->loadConfig();

            // TODO: take off this protection by storing localIPAddress using interface name
            setConfig (iter->second->getAccountID(), LOCAL_ADDRESS, "0.0.0.0");

            /* If the account is set as enabled, try to register */

            if (iter->second->isEnabled()) {
                status = iter->second->registerVoIPLink();

                if (status != SUCCESS) {
                    flag = false;
                }
            }
        }

        iter++;
    }

    // calls the client notification here in case of errors at startup...
    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    ASSERT (flag, true);

    return SUCCESS;
}

//THREAD=Main
bool
ManagerImpl::sendDtmf (const CallID& id, char code)
{
    AccountID accountid = getAccountFromCall (id);

    if (accountid == AccountNULL) {
        playDtmf (code);
        return false;
    }

    int sendType = getConfigInt (SIGNALISATION, SEND_DTMF_AS);

    bool returnValue = false;

    switch (sendType) {

        case 0: // SIP INFO
            playDtmf (code);
            returnValue = getAccountLink (accountid)->carryingDTMFdigits (id, code);
            break;

        case 1: // Audio way
            break;

        case 2: // rfc 2833
            break;

        default: // unknown - error config?
            break;
    }

    return returnValue;
}

//THREAD=Main | VoIPLink
bool
ManagerImpl::playDtmf (char code)
{
    int pulselen, layer, size;
    bool ret = false;
    AudioLayer *audiolayer;
    SFLDataFormat *buf;

    _debug ("ManagerImpl::playDtmf\n");

    stopTone ();

    bool hasToPlayTone = getConfigBool (SIGNALISATION, PLAY_DTMF);

    if (!hasToPlayTone) {
        _debug ("    playDtmf: Do not have to play a tone...\n");
        return false;
    }

    // length in milliseconds
    pulselen = getConfigInt (SIGNALISATION, PULSE_LENGTH);

    if (!pulselen) {
        _debug ("    playDtmf: Pulse length is not set...\n");
        return false;
    }

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS
    audiolayer = getAudioDriver();

    layer = audiolayer->getLayerType();

    // fast return, no sound, so no dtmf
    if (audiolayer==0 || _dtmfKey == 0) {
        _debug ("    playDtmf: Error no audio layer...\n");
        return false;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    size = (int) ( (pulselen * (float) audiolayer->getSampleRate()) / 1000);

    _debug ("DTMF pulselen: %i\n", pulselen);

    _debug ("DTMF size: %i\n", size);

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
    } else {
        _debug ("    playDtmf: Error cannot play dtmf\n");
    }

    ret = true;

    // TODO Cache the DTMF

    delete[] buf;
    buf = 0;

    return ret;
}

// Multi-thread
bool
ManagerImpl::incomingCallWaiting()
{
    return (_nbIncomingWaitingCall > 0) ? true : false;
}

void
ManagerImpl::addWaitingCall (const CallID& id)
{
    ost::MutexLock m (_waitingCallMutex);
    _waitingCall.insert (id);
    _nbIncomingWaitingCall++;
}

void
ManagerImpl::removeWaitingCall (const CallID& id)
{
    ost::MutexLock m (_waitingCallMutex);
    // should return more than 1 if it erase a call

    if (_waitingCall.erase (id)) {
        _nbIncomingWaitingCall--;
    }
}

bool
ManagerImpl::isWaitingCall (const CallID& id)
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
bool
ManagerImpl::incomingCall (Call* call, const AccountID& accountId)
{
    PulseLayer *pulselayer;
    std::string from, number, display_name, display;

    stopTone ();

    _debug ("Incoming call %s for account %s\n", call->getCallId().data(), accountId.c_str());

    associateCallToAccount (call->getCallId(), accountId);

    // If account is null it is an ip to ip call

    if (accountId==AccountNULL) {

        associateConfigToCall (call->getCallId(), Call::IPtoIP);
    } else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber = call->getPeerNumber();

        int startIndex = peerNumber.find ("sip:");

        // if "sip:" is found => it is not an IAX call

        if (startIndex != (int) string::npos) {
            std::string strippedPeerNumber = peerNumber.substr (startIndex+4);
            call->setPeerNumber (strippedPeerNumber);
        }

    }

    _debug ("ManagerImpl::incomingCall :: hasCurrentCall() %i \n", hasCurrentCall());

    if (!hasCurrentCall()) {

        call->setConnectionState (Call::Ringing);
        ringtone();
        // switchCall (call->getCallId());

    }

    /*
    else {
        addWaitingCall(call->getCallId());
    }
    */

    addWaitingCall (call->getCallId());

    from = call->getPeerName();

    number = call->getPeerNumber();

    display_name = call->getDisplayName();

    // _debug(    "incomingCall from: %s, number: %s, display_name: %s\n", from.c_str(), number.c_str(), display_name.c_str());

    if (from != "" && number != "") {
        from.append (" <");
        from.append (number);
        from.append (">");
    } else if (from.empty()) {
        from.append ("<");
        from.append (number);
        from.append (">");
    }

    /*
    CallIDSet::iterator iter = _waitingCall.begin();
    while (iter != _waitingCall.end()) {
        CallID ident = *iter;
        _debug("ManagerImpl::incomingCall :: CALL iteration: %s \n",ident.c_str());
        ++iter;
    }
    */

    /* Broadcast a signal over DBus */
    _debug ("From: %s, Number: %s, DisplayName: %s\n", from.c_str(), number.c_str(), display_name.c_str());

    display = display_name;

    display.append (" ");

    display.append (from);


    if (_dbus) _dbus->getCallManager()->incomingCall (accountId, call->getCallId(), display.c_str());

    //if (_dbus) _dbus->getCallManager()->callStateChanged(call->getCallId(), "INCOMING");

    if (_audiodriver->getLayerType() == PULSEAUDIO)  {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());
    }

    return true;
}

//THREAD=VoIP
void
ManagerImpl::incomingMessage (const AccountID& accountId, const std::string& message)
{
    if (_dbus) {
        _dbus->getCallManager()->incomingMessage (accountId, message);
    }
}

//THREAD=VoIP CALL=Outgoing
void
ManagerImpl::peerAnsweredCall (const CallID& id)
{
    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall (id)) {
        stopTone ();
    }

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "CURRENT");

    // std::string codecName = getCurrentCodecName (id);

    // _debug("ManagerImpl::hangupCall(): broadcast codec name %s \n",codecName.c_str());
    // if (_dbus) _dbus->getCallManager()->currentSelectedCodec (id,codecName.c_str());

    // Required if there have been no sip reinvite, in this case we must reinit buffers since the
    _audiodriver->flushMain();

    _audiodriver->flushUrgent();
}

//THREAD=VoIP Call=Outgoing
void
ManagerImpl::peerRingingCall (const CallID& id)
{
    if (isCurrentCall (id)) {
        ringback();
    }

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void
ManagerImpl::peerHungupCall (const CallID& call_id)
{
    PulseLayer *pulselayer;
    AccountID account_id;
    bool returnValue;

    _debug ("ManagerImpl::peerHungupCall(%s)\n", call_id.c_str());

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
            stopTone ();

            switchCall ("");
        }
    }

    int nbCalls = getCallList().size();

    // stop streams

    if (nbCalls <= 1) {
        _debug ("    hangupCall: stop audio stream, ther is only %i call(s) remaining\n", nbCalls);

        AudioLayer* audiolayer = getAudioDriver();
        audiolayer->stopStream();
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (call_id) == Call::IPtoIP) {
        SIPVoIPLink::instance (AccountNULL)->hangup (call_id);
    }

    else {

        account_id = getAccountFromCall (call_id);

        if (account_id == AccountNULL) {
            _debug ("peerHungupCall: Call doesn't exists\n");
            return;
        }

        returnValue = getAccountLink (account_id)->peerHungup (call_id);
    }

    /* Broadcast a signal over DBus */
    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "HUNGUP");

    removeWaitingCall (call_id);

    removeCallAccount (call_id);

    if (_audiodriver->getLayerType() == PULSEAUDIO) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());
    }
}

//THREAD=VoIP
void
ManagerImpl::callBusy (const CallID& id)
{
    _debug ("Call busy\n");

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "BUSY");

    if (isCurrentCall (id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    removeCallAccount (id);

    removeWaitingCall (id);
}

//THREAD=VoIP
void
ManagerImpl::callFailure (const CallID& call_id)
{
    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "FAILURE");

    if (isCurrentCall (call_id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    CallID current_call_id = getCurrentCallId();

    if (participToConference (call_id)) {

        _debug ("Call %s participating to a conference failed\n", call_id.c_str());

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
void
ManagerImpl::startVoiceMessageNotification (const AccountID& accountId, int nb_msg)
{
    if (_dbus) _dbus->getCallManager()->voiceMailNotify (accountId, nb_msg) ;
}

void ManagerImpl::connectionStatusNotification()
{
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
    // AudioLoop *audioloop;
    AudioLayer *audiolayer;
    // unsigned int nbSamples;

    _debug ("ManagerImpl::playATone\n");

    hasToPlayTone = getConfigBool (SIGNALISATION, PLAY_TONES);

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
        /*
            audioloop = getTelephoneTone();
            nbSamples = audioloop->getSize();
            SFLDataFormat buf[nbSamples];


            if (audiolayer) {
                audiolayer->putUrgent (buf, nbSamples);
            } else
                return false;
        */
    }

    return true;
}

/**
 * Multi Thread
 */
void ManagerImpl::stopTone ()
{
    bool hasToPlayTone;

    _debug ("ManagerImpl::stopTone\n");

    hasToPlayTone = getConfigBool (SIGNALISATION, PLAY_TONES);

    if (!hasToPlayTone)
        return;

    _toneMutex.enterMutex();

    if (_telephoneTone != 0) {
        _telephoneTone->setCurrentTone (Tone::TONE_NULL);
    }

    _toneMutex.leaveMutex();

    // for ringing tone..
    _toneMutex.enterMutex();
    _audiofile.stop();
    _toneMutex.leaveMutex();
}

/**
 * Multi Thread
 */
bool
ManagerImpl::playTone()
{
    playATone (Tone::TONE_DIALTONE);
    return true;
}

/**
 * Multi Thread
 */
bool
ManagerImpl::playToneWithMessage()
{
    playATone (Tone::TONE_CONGESTION);
    return true;
}

/**
 * Multi Thread
 */
void
ManagerImpl::congestion ()
{
    playATone (Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringback ()
{
    _debug ("ManagerImpl::ringback\n");

    playATone (Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringtone()
{
    _debug ("ManagerImpl::ringtone\n");
    std::string ringchoice;
    AudioLayer *audiolayer;
    AudioCodec *codecForTone;
    int layer, samplerate;
    bool loadFile;

    if (isRingtoneEnabled()) {

        _debug ("  Tone is enabled\n");
        //TODO Comment this because it makes the daemon crashes since the main thread
        //synchronizes the ringtone thread.

        ringchoice = getConfigString (AUDIO, RING_CHOICE);
        //if there is no / inside the path

        if (ringchoice.find (DIR_SEPARATOR_CH) == std::string::npos) {
            // check inside global share directory
            ringchoice = std::string (PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR + DIR_SEPARATOR_STR + ringchoice;
        }

        audiolayer = getAudioDriver();

        layer = audiolayer->getLayerType();

        if (audiolayer == 0)
            return;


        samplerate  = audiolayer->getSampleRate();

        codecForTone = _codecDescriptorMap.getFirstCodecAvailable();

        _toneMutex.enterMutex();

        loadFile = _audiofile.loadFile (ringchoice, codecForTone , samplerate);

        _toneMutex.leaveMutex();

        if (loadFile) {

            _toneMutex.enterMutex();
            _audiofile.start();
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
ManagerImpl::getTelephoneTone()
{
    // _debug("ManagerImpl::getTelephoneTone()\n");
    if (_telephoneTone != 0) {
        ost::MutexLock m (_toneMutex);
        return _telephoneTone->getCurrentTone();
    } else {
        return 0;
    }
}

AudioLoop*
ManagerImpl::getTelephoneFile()
{
    // _debug("ManagerImpl::getTelephoneFile()\n");
    ost::MutexLock m (_toneMutex);

    if (_audiofile.isStarted()) {
        return &_audiofile;
    } else {
        return 0;
    }
}

void ManagerImpl::notificationIncomingCall (void)
{
    AudioLayer *audiolayer;
    std::ostringstream frequency;
    unsigned int samplerate, nbSampling;

    audiolayer = getAudioDriver();

    _debug ("ManagerImpl::notificationIncomingCall\n");

    if (audiolayer != 0) {
        samplerate = audiolayer->getSampleRate();
        frequency << "440/" << FRAME_PER_BUFFER;
        Tone tone (frequency.str(), samplerate);
        nbSampling = tone.getSize();
        SFLDataFormat buf[nbSampling];
        tone.getNext (buf, tone.getSize());
        /* Put the data in the urgent ring buffer */
        audiolayer->flushUrgent();
        audiolayer->putUrgent (buf, sizeof (SFLDataFormat) *nbSampling);
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
int
ManagerImpl::createSettingsPath (void)
{

    std::string xdg_config, xdg_env;

    _debug ("XDG_CONFIG_HOME: %s\n", XDG_CONFIG_HOME);

    xdg_config = std::string (HOMEDIR) + DIR_SEPARATOR_STR + ".config" + DIR_SEPARATOR_STR + PROGDIR;

    if (XDG_CONFIG_HOME != NULL) {
        xdg_env = std::string (XDG_CONFIG_HOME);
        (xdg_env.length() > 0) ? _path = xdg_env
                                         :	 _path = xdg_config;
    } else
        _path = xdg_config;

    if (mkdir (_path.data(), 0700) != 0) {
        // If directory	creation failed
        if (errno != EEXIST) {
            _debug ("Cannot create directory: %s\n", strerror (errno));
            return -1;
        }
    }

    // Load user's configuration
    _path = _path + DIR_SEPARATOR_STR + PROGNAME + "rc";

    return 1;
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initConfigFile (bool load_user_value, std::string alternate)
{
    _debug ("ManagerImpl::InitConfigFile\n");

    // Default values, that will be overwritten by the call to
    // 'populateFromFile' below.

    // Peer to peer settings
    _config.addDefaultValue (std::pair<std::string, std::string> (SRTP_ENABLE, FALSE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, "1"), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (ZRTP_HELLO_HASH, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, FALSE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_ENABLE, FALSE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_CA_LIST_FILE, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_PASSWORD, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_METHOD, "TLSv1"), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_CIPHERS, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_SERVER_NAME, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_VERIFY_SERVER, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, "2"), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, "0"), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (PUBLISHED_SAMEAS_LOCAL, TRUE_STR), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (LOCAL_PORT, DEFAULT_SIP_PORT), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (PUBLISHED_PORT, DEFAULT_SIP_PORT), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (LOCAL_ADDRESS, DEFAULT_ADDRESS), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (PUBLISHED_ADDRESS, DEFAULT_ADDRESS), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (STUN_ENABLE, DFT_STUN_ENABLE), IP2IP_PROFILE);
    _config.addDefaultValue (std::pair<std::string, std::string> (STUN_SERVER, DFT_STUN_SERVER), IP2IP_PROFILE);

    // Init display name to the username under which
    // this sflphone instance is running.
    std::string diplayName ("");
    uid_t uid = getuid();

    struct passwd * user_info = NULL;
    user_info = getpwuid (uid);

    if (user_info != NULL) {
        diplayName = user_info->pw_name;
    }

    _config.addDefaultValue (std::pair<std::string, std::string> (DISPLAY_NAME, diplayName), IP2IP_PROFILE);

    // Signalisation settings
    _config.addDefaultValue (std::pair<std::string, std::string> (SYMMETRIC, TRUE_STR), SIGNALISATION);
    _config.addDefaultValue (std::pair<std::string, std::string> (PLAY_DTMF, TRUE_STR), SIGNALISATION);
    _config.addDefaultValue (std::pair<std::string, std::string> (PLAY_TONES, TRUE_STR), SIGNALISATION);
    _config.addDefaultValue (std::pair<std::string, std::string> (PULSE_LENGTH, DFT_PULSE_LENGTH_STR), SIGNALISATION);
    _config.addDefaultValue (std::pair<std::string, std::string> (SEND_DTMF_AS, SIP_INFO_STR), SIGNALISATION);
    _config.addDefaultValue (std::pair<std::string, std::string> (ZRTP_ZIDFILE, ZRTP_ZID_FILENAME), SIGNALISATION);

    // Audio settings
    _config.addDefaultValue (std::pair<std::string, std::string> (ALSA_CARD_ID_IN, ALSA_DFT_CARD), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (ALSA_CARD_ID_OUT, ALSA_DFT_CARD), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (ALSA_SAMPLE_RATE, DFT_SAMPLE_RATE), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (ALSA_FRAME_SIZE, DFT_FRAME_SIZE), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (ALSA_PLUGIN, PCM_DEFAULT), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (RING_CHOICE, DFT_RINGTONE), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (VOLUME_SPKR, DFT_VOL_SPKR_STR), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (VOLUME_MICRO, DFT_VOL_MICRO_STR), AUDIO);
    _config.addDefaultValue (std::pair<std::string, std::string> (RECORD_PATH,DFT_RECORD_PATH), AUDIO);

    // General settings
    _config.addDefaultValue (std::pair<std::string, std::string> (ZONE_TONE, DFT_ZONE), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_RINGTONE, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_DIALPAD, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_SEARCHBAR, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_START, FALSE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_POPUP, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_NOTIFY, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_MAIL_NOTIFY, FALSE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_VOLUME, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_HISTORY_LIMIT, DFT_HISTORY_LIMIT), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_HISTORY_ENABLED, TRUE_STR), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_AUDIO, DFT_AUDIO_MANAGER), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_SIP_PORT, DFT_SIP_PORT), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNTS_ORDER, EMPTY_FIELD), PREFERENCES);
    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_MD5HASH, FALSE_STR), PREFERENCES);

    // Addressbook settings
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_ENABLE, TRUE_STR), ADDRESSBOOK);
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_MAX_RESULTS, "25"), ADDRESSBOOK);
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_CONTACT_PHOTO, FALSE_STR), ADDRESSBOOK);
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_BUSINESS, TRUE_STR), ADDRESSBOOK);
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_HOME, FALSE_STR), ADDRESSBOOK);
    _config.addDefaultValue (std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_MOBILE, FALSE_STR), ADDRESSBOOK);

    // Hooks settings
    _config.addDefaultValue (std::pair<std::string, std::string> (URLHOOK_SIP_FIELD, HOOK_DEFAULT_SIP_FIELD), HOOKS);
    _config.addDefaultValue (std::pair<std::string, std::string> (URLHOOK_COMMAND, HOOK_DEFAULT_URL_COMMAND), HOOKS);
    _config.addDefaultValue (std::pair<std::string, std::string> (URLHOOK_SIP_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue (std::pair<std::string, std::string> (URLHOOK_IAX2_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue (std::pair<std::string, std::string> (PHONE_NUMBER_HOOK_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue (std::pair<std::string, std::string> (PHONE_NUMBER_HOOK_ADD_PREFIX, EMPTY_FIELD), HOOKS);

    std::string path;
    // Loads config from ~/.sflphone/sflphonedrc or so..

    if (createSettingsPath() == 1 && load_user_value) {
        (alternate == "") ? path = _path : path = alternate;
        std::cout << path << std::endl;
        _exist = _config.populateFromFile (path);
    }

    // Globally shared default values (not to be populated from file)
    _config.addDefaultValue (std::pair<std::string, std::string> (HOSTNAME, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (AUTHENTICATION_USERNAME, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (USERNAME, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (PASSWORD, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (REALM, DEFAULT_REALM));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, DFT_EXPIRE_VALUE));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_RESOLVE_ONCE, FALSE_STR));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_MAILBOX, EMPTY_FIELD));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, TRUE_STR));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_CREDENTIAL_NUMBER, "0"));

    _config.addDefaultValue (std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, DEFAULT_ACCOUNT_TYPE));

    _setupLoaded = (_exist == 2) ? false : true;
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioCodec (void)
{
    _debugInit ("Active Codecs List");
    // init list of all supported codecs
    _codecDescriptorMap.init();
    // if the user never set the codec list, use the default configuration

    if (getConfigString (AUDIO, "ActiveCodecs") == "") {
        _codecDescriptorMap.setDefaultOrder();
    }

    // else retrieve the one set in the user config file
    else {
        std::vector<std::string> active_list = retrieveActiveCodecs();
        setActiveCodecList (active_list);
    }
}


void
ManagerImpl::setActiveCodecList (const std::vector<  std::string >& list)
{
    _debug ("Set active codecs list\n");
    _codecDescriptorMap.saveActiveCodecs (list);
    // setConfig
    std::string s = serialize (list);
    _debug ("Setting codec with payload number %s to the active list\n", s.c_str());
    setConfig ("Audio", "ActiveCodecs", s);
}

std::vector<std::string>
ManagerImpl::retrieveActiveCodecs()
{
    std::string s = getConfigString (AUDIO, "ActiveCodecs");
    return unserialize (s);
}

std::vector<std::string>
ManagerImpl::unserialize (std::string s)
{

    std::vector<std::string> list;
    std::string  temp;

    while (s.find ("/", 0) != std::string::npos) {
        size_t  pos = s.find ("/", 0);
        temp = s.substr (0, pos);
        s.erase (0, pos + 1);
        list.push_back (temp);
    }

    return list;
}

std::string
ManagerImpl::serialize (std::vector<std::string> v)
{
    unsigned int i;
    std::string res;

    for (i=0;i<v.size();i++) {
        res += v[i] + "/";
    }

    return res;
}


std::vector <std::string>
ManagerImpl::getActiveCodecList (void)
{
    _debug ("ManagerImpl::getActiveCodecList\n");
    std::vector< std::string > v;
    CodecOrder active = _codecDescriptorMap.getActiveCodecs();
    unsigned int i=0;
    size_t size = active.size();

    while (i<size) {
        std::stringstream ss;
        ss << active[i];
        v.push_back ( (ss.str()).data());
        _debug ("Codec with payload number %s is active\n", ss.str().data());
        i++;
    }

    return v;
}


/**
 * Send the list of codecs to the client through DBus.
 */
std::vector< std::string >
ManagerImpl::getCodecList (void)
{
    std::vector<std::string> list;
    //CodecMap codecs = _codecDescriptorMap.getCodecMap();
    CodecsMap codecs = _codecDescriptorMap.getCodecsMap();
    CodecOrder order = _codecDescriptorMap.getActiveCodecs();
    CodecsMap::iterator iter = codecs.begin();

    while (iter!=codecs.end()) {
        std::stringstream ss;

        if (iter->second != NULL) {
            ss << iter->first;
            list.push_back ( (ss.str()).data());
        }

        iter++;
    }

    return list;
}

std::vector<std::string>
ManagerImpl::getCodecDetails (const int32_t& payload)
{

    std::vector<std::string> v;
    std::stringstream ss;

    v.push_back (_codecDescriptorMap.getCodecName ( (AudioCodecType) payload));
    ss << _codecDescriptorMap.getSampleRate ( (AudioCodecType) payload);
    v.push_back ( (ss.str()).data());
    ss.str ("");
    ss << _codecDescriptorMap.getBitRate ( (AudioCodecType) payload);
    v.push_back ( (ss.str()).data());
    ss.str ("");
    ss << _codecDescriptorMap.getBandwidthPerCall ( (AudioCodecType) payload);
    v.push_back ( (ss.str()).data());
    ss.str ("");

    return v;
}

std::string
ManagerImpl::getCurrentCodecName (const CallID& id)
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
 * Get list of supported input audio plugin
 */
std::vector<std::string>
ManagerImpl::getInputAudioPluginList (void)
{
    std::vector<std::string> v;
    _debug ("Get input audio plugin list\n");

    v.push_back ("default");
    v.push_back ("surround40");
    v.push_back ("plug:hw");

    return v;
}

/**
 * Get list of supported output audio plugin
 */
std::vector<std::string>
ManagerImpl::getOutputAudioPluginList (void)
{
    std::vector<std::string> v;
    _debug ("Get output audio plugin list\n");

    v.push_back (PCM_DEFAULT);
    v.push_back (PCM_DMIX);

    return v;
}

/**
 * Set input audio plugin
 */
void
ManagerImpl::setInputAudioPlugin (const std::string& audioPlugin)
{
    int layer = _audiodriver -> getLayerType();

    if (CHECK_INTERFACE (layer , ALSA)) {
        _debug ("Set input audio plugin\n");
        _audiodriver -> setErrorMessage (-1);
        _audiodriver -> openDevice (_audiodriver -> getIndexIn(),
                                    _audiodriver -> getIndexOut(),
                                    _audiodriver -> getSampleRate(),
                                    _audiodriver -> getFrameSize(),
                                    SFL_PCM_CAPTURE,
                                    audioPlugin);

        if (_audiodriver -> getErrorMessage() != -1)
            notifyErrClient (_audiodriver -> getErrorMessage());
    } else {}

}

/**
 * Set output audio plugin
 */
void
ManagerImpl::setOutputAudioPlugin (const std::string& audioPlugin)
{

    int res;

    _debug ("Set output audio plugin\n");
    _audiodriver -> setErrorMessage (-1);
    res = _audiodriver -> openDevice (_audiodriver -> getIndexIn(),
                                      _audiodriver -> getIndexOut(),
                                      _audiodriver -> getSampleRate(),
                                      _audiodriver -> getFrameSize(),
                                      SFL_PCM_BOTH,
                                      audioPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    // set config
    if (res)   setConfig (AUDIO , ALSA_PLUGIN , audioPlugin);
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string>
ManagerImpl::getAudioOutputDeviceList (void)
{
    _debug ("Get audio output device list\n");
    AlsaLayer *layer;
    std::vector <std::string> devices;

    layer = dynamic_cast<AlsaLayer*> (getAudioDriver ());

    if (layer)
        devices = layer -> getSoundCardsInfo (SFL_PCM_PLAYBACK);

    return devices;
}

/**
 * Set audio output device
 */
void
ManagerImpl::setAudioOutputDevice (const int index)
{
    AlsaLayer *alsalayer;
    std::string alsaplugin;
    _debug ("Set audio output device: %i\n", index);

    _audiodriver -> setErrorMessage (-1);

    alsalayer = dynamic_cast<AlsaLayer*> (getAudioDriver ());
    alsaplugin = alsalayer->getAudioPlugin ();

    _debug ("  set output plugin: %s\n", alsaplugin.c_str());

    _audiodriver->openDevice (_audiodriver->getIndexIn(), index, _audiodriver->getSampleRate(), _audiodriver->getFrameSize(), SFL_PCM_PLAYBACK, alsaplugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    // set config
    setConfig (AUDIO , ALSA_CARD_ID_OUT , index);
}

/**
 * Get list of supported audio input device
 */
std::vector<std::string>
ManagerImpl::getAudioInputDeviceList (void)
{
    AlsaLayer *audiolayer;
    std::vector <std::string> devices;

    audiolayer = dynamic_cast<AlsaLayer *> (getAudioDriver());


    if (audiolayer)
        devices = audiolayer->getSoundCardsInfo (SFL_PCM_CAPTURE);

    return devices;
}

/**
 * Set audio input device
 */
void
ManagerImpl::setAudioInputDevice (const int index)
{
    AlsaLayer *alsalayer;
    std::string alsaplugin;

    _debug ("Set audio input device %i\n", index);

    _audiodriver -> setErrorMessage (-1);

    alsalayer = dynamic_cast<AlsaLayer*> (getAudioDriver ());
    alsaplugin = alsalayer->getAudioPlugin ();

    _debug ("  set input plugin: %s\n", alsaplugin.c_str());

    _audiodriver->openDevice (index, _audiodriver->getIndexOut(), _audiodriver->getSampleRate(), _audiodriver->getFrameSize(), SFL_PCM_CAPTURE, alsaplugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    // set config
    setConfig (AUDIO , ALSA_CARD_ID_IN , index);
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string>
ManagerImpl::getCurrentAudioDevicesIndex()
{
    _debug ("Get current audio devices index\n");
    std::vector<std::string> v;
    std::stringstream ssi , sso;
    sso << _audiodriver->getIndexOut();
    v.push_back (sso.str());
    ssi << _audiodriver->getIndexIn();
    v.push_back (ssi.str());
    return v;
}

int
ManagerImpl::isIax2Enabled (void)
{
#ifdef USE_IAX
    return true;
#else
    return false;
#endif
}

int
ManagerImpl::isRingtoneEnabled (void)
{
    return (getConfigString (PREFERENCES, CONFIG_RINGTONE) == "true") ? 1:0;
}

void
ManagerImpl::ringtoneEnabled (void)
{
    (getConfigString (PREFERENCES , CONFIG_RINGTONE) == RINGTONE_ENABLED) ? setConfig (PREFERENCES , CONFIG_RINGTONE , FALSE_STR) : setConfig (PREFERENCES , CONFIG_RINGTONE , TRUE_STR);
}

std::string
ManagerImpl::getRingtoneChoice (void)
{
    // we need the absolute path
    std::string tone_name = getConfigString (AUDIO , RING_CHOICE);
    std::string tone_path ;

    if (tone_name.find (DIR_SEPARATOR_CH) == std::string::npos) {
        // check in ringtone directory ($(PREFIX)/share/sflphone/ringtones)
        tone_path = std::string (PROGSHAREDIR) + DIR_SEPARATOR_STR + RINGDIR + DIR_SEPARATOR_STR + tone_name ;
    } else {
        // the absolute has been saved; do nothing
        tone_path = tone_name ;
    }

    _debug ("%s\n", tone_path.c_str());

    return tone_path;
}

void
ManagerImpl::setRingtoneChoice (const std::string& tone)
{
    // we save the absolute path
    setConfig (AUDIO , RING_CHOICE , tone);
}

std::string
ManagerImpl::getRecordPath (void)
{
    return getConfigString (AUDIO, RECORD_PATH);
}

void
ManagerImpl::setRecordPath (const std::string& recPath)
{
    _debug ("ManagerImpl::setRecordPath(%s)! \n", recPath.c_str());
    setConfig (AUDIO, RECORD_PATH, recPath);
}

bool
ManagerImpl::getMd5CredentialHashing (void)
{
    return getConfigBool (PREFERENCES, CONFIG_MD5HASH);
}

int
ManagerImpl::getDialpad (void)
{
    if (getConfigString (PREFERENCES, CONFIG_DIALPAD) == TRUE_STR) {
        return 1;
    } else {
        return 0;
    }
}

void
ManagerImpl::setDialpad (bool display)
{
    std::string set;

    display ? set = TRUE_STR : set = FALSE_STR;
    // If the value we received is different from the one saved in the config file, save the new value
    // Else do nothing

    if ( (display && (getConfigString (PREFERENCES, CONFIG_DIALPAD) != TRUE_STR)) ||
            (!display && (getConfigString (PREFERENCES, CONFIG_DIALPAD) != FALSE_STR)))
        setConfig (PREFERENCES, CONFIG_DIALPAD, set);
}

int
ManagerImpl::getVolumeControls (void)
{
    if (getConfigString (PREFERENCES , CONFIG_VOLUME) == TRUE_STR) {
        return 1;
    } else {
        return 0;
    }
}

void ManagerImpl::setVolumeControls (bool display)
{
    std::string set;

    display ? set = TRUE_STR : set = FALSE_STR;
    // If the value we received is different from the one saved in the config file, save the new value
    // Else do nothing

    if ( (display && (getConfigString (PREFERENCES, CONFIG_VOLUME) != TRUE_STR)) ||
            (!display && (getConfigString (PREFERENCES, CONFIG_VOLUME) != FALSE_STR)))
        setConfig (PREFERENCES, CONFIG_VOLUME, set);
}

void
ManagerImpl::setRecordingCall (const CallID& id)
{
    /*
    _debug ("ManagerImpl::setRecording()! \n");
    AccountID accountid = getAccountFromCall (id);

    getAccountLink (accountid)->setRecording (id);
    */
    AccountID accountid = getAccountFromCall (id);
    Recordable* rec = (Recordable*) getAccountLink (accountid)->getCall (id);

    rec->setRecording();
}

bool
ManagerImpl::isRecording (const CallID& id)
{
    /*
    _debug ("ManagerImpl::isRecording()! \n");
    AccountID accountid = getAccountFromCall (id);

    return getAccountLink (accountid)->isRecording (id);
    */

    AccountID accountid = getAccountFromCall (id);
    Recordable* rec = (Recordable*) getAccountLink (accountid)->getCall (id);

    return rec->isRecording();
}

void
ManagerImpl::startHidden (void)
{
    (getConfigString (PREFERENCES, CONFIG_START) ==  START_HIDDEN) ? setConfig (PREFERENCES , CONFIG_START , FALSE_STR) : setConfig (PREFERENCES , CONFIG_START , TRUE_STR);
}

int
ManagerImpl::isStartHidden (void)
{
    return (getConfigBool (PREFERENCES, CONFIG_START) == true) ? 1:0;
}

void
ManagerImpl::switchPopupMode (void)
{
    (getConfigString (PREFERENCES, CONFIG_POPUP) ==  WINDOW_POPUP) ? setConfig (PREFERENCES, CONFIG_POPUP, FALSE_STR) : setConfig (PREFERENCES, CONFIG_POPUP, TRUE_STR);
}

void ManagerImpl::setHistoryLimit (const int& days)
{
    setConfig (PREFERENCES, CONFIG_HISTORY_LIMIT, days);
}

int ManagerImpl::getHistoryLimit (void)
{
    return getConfigInt (PREFERENCES , CONFIG_HISTORY_LIMIT);
}

std::string ManagerImpl::getHistoryEnabled (void)
{
    return getConfigString (PREFERENCES, CONFIG_HISTORY_ENABLED);
}

void ManagerImpl::setHistoryEnabled (void)
{
    (getConfigString (PREFERENCES, CONFIG_HISTORY_ENABLED) == TRUE_STR) ? setConfig (PREFERENCES, CONFIG_HISTORY_ENABLED, FALSE_STR) : setConfig (PREFERENCES, CONFIG_HISTORY_ENABLED, TRUE_STR);
}

int
ManagerImpl::getSearchbar (void)
{
    return getConfigInt (PREFERENCES , CONFIG_SEARCHBAR);
}

void
ManagerImpl::setSearchbar (void)
{
    (getConfigInt (PREFERENCES , CONFIG_SEARCHBAR) ==  1) ? setConfig (PREFERENCES , CONFIG_SEARCHBAR , FALSE_STR) : setConfig (PREFERENCES , CONFIG_SEARCHBAR , TRUE_STR);
}

int
ManagerImpl::popupMode (void)
{
    return (getConfigBool (PREFERENCES, CONFIG_POPUP) == true) ? 1:0 ;
}

int32_t
ManagerImpl::getNotify (void)
{
    return (getConfigBool (PREFERENCES , CONFIG_NOTIFY) == true) ? 1:0;
}

void
ManagerImpl::setNotify (void)
{
    (getConfigString (PREFERENCES, CONFIG_NOTIFY) == NOTIFY_ALL) ?  setConfig (PREFERENCES, CONFIG_NOTIFY , FALSE_STR) : setConfig (PREFERENCES, CONFIG_NOTIFY , TRUE_STR);
}

int32_t
ManagerImpl::getMailNotify (void)
{
    return getConfigInt (PREFERENCES, CONFIG_MAIL_NOTIFY);
}

void ManagerImpl::setAudioManager (const int32_t& api)
{

    int type;
    std::string alsaPlugin;

    _debug ("Setting audio manager \n");

    if (!_audiodriver)
        return;

    type = _audiodriver->getLayerType();

    if (type == api) {
        _debug ("Audio manager chosen already in use. No changes made. \n");
        return;
    }

    setConfig (PREFERENCES , CONFIG_AUDIO , api) ;

    switchAudioManager();
    return;

}

int32_t
ManagerImpl::getAudioManager (void)
{
    return getConfigInt (PREFERENCES , CONFIG_AUDIO);
}

void
ManagerImpl::setMailNotify (void)
{
    (getConfigString (PREFERENCES , CONFIG_MAIL_NOTIFY) == NOTIFY_ALL) ?  setConfig (PREFERENCES , CONFIG_MAIL_NOTIFY , FALSE_STR) : setConfig (PREFERENCES , CONFIG_MAIL_NOTIFY , TRUE_STR);
}

void
ManagerImpl::notifyErrClient (const int32_t& errCode)
{
    if (_dbus) {
        _debug ("NOTIFY ERR NUMBER %i\n" , errCode);
        _dbus -> getConfigurationManager() -> errorAlert (errCode);
    }
}

int
ManagerImpl::getAudioDeviceIndex (const std::string name)
{
    AlsaLayer *alsalayer;

    _debug ("Get audio device index\n");

    alsalayer = dynamic_cast<AlsaLayer *> (getAudioDriver());

    if (alsalayer)
        return alsalayer -> soundCardGetIndex (name);
    else
        return 0;
}

std::string
ManagerImpl::getCurrentAudioOutputPlugin (void)
{
    AlsaLayer *alsalayer;

    _debug ("Get alsa plugin\n");

    alsalayer = dynamic_cast<AlsaLayer *> (getAudioDriver());

    if (alsalayer)   return alsalayer -> getAudioPlugin ();
    else            return getConfigString (AUDIO , ALSA_PLUGIN);
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
bool
ManagerImpl::initAudioDriver (void)
{

    int error;

    _debugInit ("AudioLayer Creation");

    if (getConfigInt (PREFERENCES , CONFIG_AUDIO) == ALSA) {
        _audiodriver = new AlsaLayer (this);
        _audiodriver->setMainBuffer (&_mainBuffer);
    } else if (getConfigInt (PREFERENCES , CONFIG_AUDIO) == PULSEAUDIO) {
        if (app_is_running ("pulseaudio") == 0) {
            _audiodriver = new PulseLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
        } else {
            _audiodriver = new AlsaLayer (this);
            setConfig (PREFERENCES, CONFIG_AUDIO, ALSA);
            _audiodriver->setMainBuffer (&_mainBuffer);
        }
    } else
        _debug ("Error - Audio API unknown\n");

    if (_audiodriver == 0) {
        _debug ("Init audio driver error\n");
        return false;
    } else {
        error = getAudioDriver()->getErrorMessage();

        if (error == -1) {
            _debug ("Init audio driver: %i\n", error);
            return false;
        }
    }

    return true;

}

/**
 * Initialization: Main Thread and gui
 */
void
ManagerImpl::selectAudioDriver (void)
{
    int layer, numCardIn, numCardOut, sampleRate, frameSize;
    std::string alsaPlugin;
    AlsaLayer *alsalayer;

    layer = _audiodriver->getLayerType();
    _debug ("Audio layer type: %i\n" , layer);

    /* Retrieve the global devices info from the user config */
    alsaPlugin = getConfigString (AUDIO , ALSA_PLUGIN);
    numCardIn  = getConfigInt (AUDIO , ALSA_CARD_ID_IN);
    numCardOut = getConfigInt (AUDIO , ALSA_CARD_ID_OUT);
    sampleRate = getConfigInt (AUDIO , ALSA_SAMPLE_RATE);

    if (sampleRate <=0 || sampleRate > 48000) {
        sampleRate = 44100;
    }

    frameSize = getConfigInt (AUDIO , ALSA_FRAME_SIZE);

    /* Only for the ALSA layer, we check the sound card information */

    if (layer == ALSA) {
        alsalayer = dynamic_cast<AlsaLayer*> (getAudioDriver ());

        if (!alsalayer -> soundCardIndexExist (numCardIn , SFL_PCM_CAPTURE)) {
            _debug (" Card with index %i doesn't exist or cannot capture. Switch to 0.\n", numCardIn);
            numCardIn = ALSA_DFT_CARD_ID ;
            setConfig (AUDIO , ALSA_CARD_ID_IN , ALSA_DFT_CARD_ID);
        }

        if (!alsalayer -> soundCardIndexExist (numCardOut , SFL_PCM_PLAYBACK)) {
            _debug (" Card with index %i doesn't exist or cannot playback . Switch to 0.\n", numCardOut);
            numCardOut = ALSA_DFT_CARD_ID ;
            setConfig (AUDIO , ALSA_CARD_ID_OUT , ALSA_DFT_CARD_ID);
        }
    }

    _audiodriver->setErrorMessage (-1);

    /* Open the audio devices */
    _audiodriver->openDevice (numCardIn , numCardOut, sampleRate, frameSize, SFL_PCM_BOTH, alsaPlugin);
    /* Notify the error if there is one */

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

}

void ManagerImpl::switchAudioManager (void)
{
    int type, samplerate, framesize, numCardIn, numCardOut;
    std::string alsaPlugin;

    _debug ("Switching audio manager \n");

    if (!_audiodriver)
        return;

    type = _audiodriver->getLayerType();

    samplerate = getConfigInt (AUDIO , ALSA_SAMPLE_RATE);

    framesize = getConfigInt (AUDIO , ALSA_FRAME_SIZE);

    _debug ("samplerate: %i, framesize %i\n", samplerate, framesize);

    alsaPlugin = getConfigString (AUDIO , ALSA_PLUGIN);

    numCardIn  = getConfigInt (AUDIO , ALSA_CARD_ID_IN);

    numCardOut = getConfigInt (AUDIO , ALSA_CARD_ID_OUT);

    _debug ("Deleting current layer... \n");

    // _audiodriver->closeLayer();
    delete _audiodriver;

    _audiodriver = NULL;

    switch (type) {

        case ALSA:
            _debug ("Creating Pulseaudio layer...\n");
            _audiodriver = new PulseLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        case PULSEAUDIO:
            _debug ("Creating ALSA layer...\n");
            _audiodriver = new AlsaLayer (this);
            _audiodriver->setMainBuffer (&_mainBuffer);
            break;

        default:
            _debug ("Error: audio layer unknown\n");
            break;
    }

    _audiodriver->setErrorMessage (-1);

    _audiodriver->openDevice (numCardIn , numCardOut, samplerate, framesize, SFL_PCM_BOTH, alsaPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    _debug ("Current device: %i \n", type);

    _debug ("has current call: %i \n", hasCurrentCall());

    if (hasCurrentCall())
        _audiodriver->startStream();


    // need to stop audio streams if there is currently no call
    // if ( (type != PULSEAUDIO) && (!hasCurrentCall())) {
    // _debug("There is currently a call!!\n");
    // _audiodriver->stopStream();

    // }
}

/**
 * Init the volume for speakers/micro from 0 to 100 value
 * Initialization: Main Thread
 */
void
ManagerImpl::initVolume()
{
    _debugInit ("Initiate Volume");
    setSpkrVolume (getConfigInt (AUDIO, VOLUME_SPKR));
    setMicVolume (getConfigInt (AUDIO, VOLUME_MICRO));
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
            if (pulselayer)  pulselayer->setPlaybackVolume (spkr_vol);
        }
    }
}


void ManagerImpl::setMicVolume (unsigned short mic_vol)
{
    _mic_volume = mic_vol;
}





void ManagerImpl::setLocalIp2IpInfo (const std::string& address)
{

    std::string ip_address = std::string (address);

    int index = ip_address.find_first_of (":");

    std::string local_address = ip_address.substr (0,index);
    std::string local_port = ip_address.substr (index+1);
    int newPort = atoi (local_port.c_str());

    _debug ("Setting new address %s and port %s for default account (ip to ip calls)\n", local_address.c_str(), local_port.c_str());

    int prevPort = getConfigInt (IP2IP_PROFILE, LOCAL_PORT);
    std::string prevAddress  = getConfigString (IP2IP_PROFILE, LOCAL_ADDRESS);

    if ( (prevPort != newPort) || (prevAddress.compare (local_address) != 0)) {


        if (_directIpAccount) {

            SIPAccount* account = dynamic_cast<SIPAccount*> (_directIpAccount);

            account->setLocalPort (newPort);
            account->setLocalAddress (local_address);
        }

        setConfig (IP2IP_PROFILE, LOCAL_ADDRESS, local_address);

        setConfig (IP2IP_PROFILE, LOCAL_PORT, newPort);

        SIPVoIPLink* siplink = SIPVoIPLink::instance ("");
        // if(siplink)
        siplink->updateAccountInfo (_directIpAccount->getAccountID());
        // this->restartPJSIP ();
    }
}


int ManagerImpl::getLocalIp2IpPort (void)
{

    /* The SIP port used for default account (IP to IP) calls */
    _debug ("Default account port %i\n", getConfigInt (IP2IP_PROFILE, LOCAL_PORT));

    return getConfigInt (IP2IP_PROFILE, LOCAL_PORT);

}


// TODO: rewrite this
/**
 * Main Thread
 */
bool
ManagerImpl::getCallStatus (const std::string& sequenceId UNUSED)
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
                    code="110";
                    status = "Trying";
                    break;

                case Call::Ringing:
                    code="111";
                    status = "Ringing";
                    break;

                case Call::Progressing:
                    code="125";
                    status = "Progressing";
                    break;

                case Call::Disconnected:
                    code="125";
                    status = "Disconnected";
                    break;

                default:
                    code="";
                    status= "";
            }
        } else {
            switch (call->getState()) {

                case Call::Active:

                case Call::Conferencing:
                    code="112";
                    status = "Established";
                    break;

                case Call::Hold:
                    code="114";
                    status = "Held";
                    break;

                case Call::Busy:
                    code="113";
                    status = "Busy";
                    break;

                case Call::Refused:
                    code="125";
                    status = "Refused";
                    break;

                case Call::Error:
                    code="125";
                    status = "Error";
                    break;

                case Call::Inactive:
                    code="125";
                    status = "Inactive";
                    break;
            }
        }

        // No Congestion
        // No Wrong Number
        // 116 <CSeq> <call-id> <acc> <destination> Busy
        destination = call->getPeerName();

        number = call->getPeerNumber();

        if (number!="") {
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
bool
ManagerImpl::getConfig (const std::string& section, const std::string& name, TokenList& arg)
{
    return _config.getConfigTreeItemToken (section, name, arg);
}

//THREAD=Main
// throw an Conf::ConfigTreeItemException if not found
int
ManagerImpl::getConfigInt (const std::string& section, const std::string& name)
{
    try {
        return _config.getConfigTreeItemIntValue (section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return 0;
}

bool
ManagerImpl::getConfigBool (const std::string& section, const std::string& name)
{
    try {
        return (_config.getConfigTreeItemValue (section, name) == TRUE_STR) ? true:false;
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return false;
}

//THREAD=Main
std::string
ManagerImpl::getConfigString (const std::string& section, const std::string&
                              name)
{
    try {
        return _config.getConfigTreeItemValue (section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }

    return "";
}

//THREAD=Main
bool
ManagerImpl::setConfig (const std::string& section, const std::string& name, const std::string& value)
{
    _debug ("ManagerImpl::setConfig %s %s %s\n", section.c_str(), name.c_str(), value.c_str());
    return _config.setConfigTreeItem (section, name, value);
}

//THREAD=Main
bool
ManagerImpl::setConfig (const std::string& section, const std::string& name, int value)
{
    std::ostringstream valueStream;
    valueStream << value;
    return _config.setConfigTreeItem (section, name, valueStream.str());
}

void ManagerImpl::setAccountsOrder (const std::string& order)
{
    _debug ("Setcreate accounts order : %s\n", order.c_str());
    // Set the new config
    setConfig (PREFERENCES, CONFIG_ACCOUNTS_ORDER, order);
}

std::vector< std::string >
ManagerImpl::getAccountList()
{
    std::vector< std::string > v;
    std::vector< std::string > account_order;
    unsigned int i;

    account_order = loadAccountOrder ();
    AccountMap::iterator iter;

    // If no order has been set, load the default one
    // ie according to the creation date.

    if (account_order.size () == 0) {
        iter = _accountMap.begin ();

        while (iter != _accountMap.end()) {
            if (iter->second != NULL && iter->first != IP2IP_PROFILE) {
                //_debug("PUSHING BACK %s\n", iter->first.c_str());
                v.push_back (iter->first.data());
            }

            iter++;
        }
    }

    // Otherelse, load the custom one
    // ie according to the saved order
    else {

        for (i=0; i<account_order.size (); i++) {
            // This account has not been loaded, so we ignore it
            if ( (iter=_accountMap.find (account_order[i])) != _accountMap.end()) {
                // If the account is valid
                if (iter->second != NULL && iter->first != IP2IP_PROFILE) {
                    v.push_back (iter->first.data ());
                }
            }
        }
    }

    return v;
}

std::map< std::string, std::string > ManagerImpl::getAccountDetails (const AccountID& accountID)
{
    std::map<std::string, std::string> a;

    Account * account = _accountMap[accountID];

    if (account == NULL) {
        _debug ("Cannot getAccountDetails on a non-existing accountID %s. Defaults will be used.\n", accountID.c_str());
    }

    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, getConfigString (accountID, CONFIG_ACCOUNT_ALIAS)));

    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, getConfigString (accountID, CONFIG_ACCOUNT_ENABLE)));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_RESOLVE_ONCE, getConfigString (accountID, CONFIG_ACCOUNT_RESOLVE_ONCE)));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, getConfigString (accountID, CONFIG_ACCOUNT_TYPE)));
    a.insert (std::pair<std::string, std::string> (HOSTNAME, getConfigString (accountID, HOSTNAME)));
    a.insert (std::pair<std::string, std::string> (USERNAME, getConfigString (accountID, USERNAME)));
    a.insert (std::pair<std::string, std::string> (PASSWORD, getConfigString (accountID, PASSWORD)));
    a.insert (std::pair<std::string, std::string> (REALM, getConfigString (accountID, REALM)));
    a.insert (std::pair<std::string, std::string> (AUTHENTICATION_USERNAME, getConfigString (accountID, AUTHENTICATION_USERNAME)));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_MAILBOX, getConfigString (accountID, CONFIG_ACCOUNT_MAILBOX)));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, getConfigString (accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE)));
    a.insert (std::pair<std::string, std::string> (PUBLISHED_SAMEAS_LOCAL, getConfigString (accountID, PUBLISHED_SAMEAS_LOCAL)));
    a.insert (std::pair<std::string, std::string> (LOCAL_ADDRESS, getConfigString (accountID, LOCAL_ADDRESS)));
    a.insert (std::pair<std::string, std::string> (PUBLISHED_ADDRESS, getConfigString (accountID, PUBLISHED_ADDRESS)));
    a.insert (std::pair<std::string, std::string> (LOCAL_PORT, getConfigString (accountID, LOCAL_PORT)));
    a.insert (std::pair<std::string, std::string> (PUBLISHED_PORT, getConfigString (accountID, PUBLISHED_PORT)));
    a.insert (std::pair<std::string, std::string> (DISPLAY_NAME, getConfigString (accountID, DISPLAY_NAME)));
    a.insert (std::pair<std::string, std::string> (STUN_ENABLE, getConfigString (accountID, STUN_ENABLE)));
    a.insert (std::pair<std::string, std::string> (STUN_SERVER, getConfigString (accountID, STUN_SERVER)));

    RegistrationState state;
    std::string registrationStateCode;
    std::string registrationStateDescription;

    if (account != NULL) {
        state = account->getRegistrationState();
        int code = account->getRegistrationStateDetailed().first;
        std::stringstream out;
        out << code;
        registrationStateCode = out.str();
        registrationStateDescription = account->getRegistrationStateDetailed().second;
    } else {
        state = Unregistered;
    }

    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATUS, mapStateNumberToString (state)));

    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_CODE, registrationStateCode));
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_DESCRIPTION, registrationStateDescription));
    a.insert (std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, getConfigString (accountID, SRTP_KEY_EXCHANGE)));
    a.insert (std::pair<std::string, std::string> (SRTP_ENABLE, getConfigString (accountID, SRTP_ENABLE)));
    a.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, getConfigString (accountID, ZRTP_DISPLAY_SAS)));
    a.insert (std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, getConfigString (accountID, ZRTP_DISPLAY_SAS_ONCE)));
    a.insert (std::pair<std::string, std::string> (ZRTP_HELLO_HASH, getConfigString (accountID, ZRTP_HELLO_HASH)));
    a.insert (std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, getConfigString (accountID, ZRTP_NOT_SUPP_WARNING)));

    a.insert (std::pair<std::string, std::string> (TLS_ENABLE, Manager::instance().getConfigString (accountID, TLS_ENABLE)));
    a.insert (std::pair<std::string, std::string> (TLS_CA_LIST_FILE, Manager::instance().getConfigString (accountID, TLS_CA_LIST_FILE)));
    a.insert (std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, Manager::instance().getConfigString (accountID, TLS_CERTIFICATE_FILE)));
    a.insert (std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, Manager::instance().getConfigString (accountID, TLS_PRIVATE_KEY_FILE)));
    a.insert (std::pair<std::string, std::string> (TLS_PASSWORD, Manager::instance().getConfigString (accountID, TLS_PASSWORD)));
    a.insert (std::pair<std::string, std::string> (TLS_METHOD, Manager::instance().getConfigString (accountID, TLS_METHOD)));
    a.insert (std::pair<std::string, std::string> (TLS_CIPHERS, Manager::instance().getConfigString (accountID, TLS_CIPHERS)));
    a.insert (std::pair<std::string, std::string> (TLS_SERVER_NAME, Manager::instance().getConfigString (accountID, TLS_SERVER_NAME)));
    a.insert (std::pair<std::string, std::string> (TLS_VERIFY_SERVER, Manager::instance().getConfigString (accountID, TLS_VERIFY_SERVER)));
    a.insert (std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, Manager::instance().getConfigString (accountID, TLS_VERIFY_CLIENT)));
    a.insert (std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, Manager::instance().getConfigString (accountID, TLS_REQUIRE_CLIENT_CERTIFICATE)));
    a.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, Manager::instance().getConfigString (accountID, TLS_NEGOTIATION_TIMEOUT_SEC)));
    a.insert (std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, Manager::instance().getConfigString (accountID, TLS_NEGOTIATION_TIMEOUT_MSEC)));

    return a;
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

    for (i = 0; i<16; ++i) {
        pj_val_to_hex_digit (digest[i], output);
        output += 2;
    }
}

std::string  ManagerImpl::computeMd5HashFromCredential (const std::string& username, const std::string& password, const std::string& realm)
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

    char ha1_null_terminated[PJSIP_MD5STRLEN+1];
    memcpy (ha1_null_terminated, ha1, sizeof (char) *PJSIP_MD5STRLEN);
    ha1_null_terminated[PJSIP_MD5STRLEN] = '\0';

    std::string hashedDigest = ha1_null_terminated;
    return hashedDigest;
}

void ManagerImpl::setCredential (const std::string& accountID, const int32_t& index, const std::map< std::string, std::string >& details)
{
    std::map<std::string, std::string>::iterator it;
    std::map<std::string, std::string> credentialInformation = details;

    std::string credentialIndex;
    std::stringstream streamOut;
    streamOut << index;
    credentialIndex = streamOut.str();

    std::string section = "Credential" + std::string (":") + accountID + std::string (":") + credentialIndex;

    _debug ("Setting credential in section %s\n", section.c_str());

    it = credentialInformation.find (USERNAME);
    std::string username;

    if (it == credentialInformation.end()) {
        username = EMPTY_FIELD;
    } else {
        username = it->second;
    }

    Manager::instance().setConfig (section, USERNAME, username);

    it = credentialInformation.find (REALM);
    std::string realm;

    if (it == credentialInformation.end()) {
        realm = EMPTY_FIELD;
    } else {
        realm = it->second;
    }

    Manager::instance().setConfig (section, REALM, realm);


    it = credentialInformation.find (PASSWORD);
    std::string password;

    if (it == credentialInformation.end()) {
        password = EMPTY_FIELD;
    } else {
        password = it->second;
    }

    if (getMd5CredentialHashing()) {
        // TODO: Fix this.
        // This is an extremly weak test in order to check
        // if the password is a hashed value. This is done
        // because deleteCredential() is called before this
        // method. Therefore, we cannot check if the value
        // is different from the one previously stored in
        // the configuration file. This is to avoid to
        // re-hash a hashed password.

        if (password.length() != 32) {
            password = computeMd5HashFromCredential (username, password, realm);
        }
    }

    Manager::instance().setConfig (section, PASSWORD, password);
}

//TODO: tidy this up. Make a macro or inline
// method to reduce the if/else mess.
// Even better, switch to XML !

void ManagerImpl::setAccountDetails (const std::string& accountID, const std::map< std::string, std::string >& details)
{

    std::string accountType;
    std::map <std::string, std::string> map_cpy;
    std::map<std::string, std::string>::iterator iter;

    // Work on a copy
    map_cpy = details;

    std::string username;
    std::string authenticationName;
    std::string password;
    std::string realm;
    std::string voicemail_count;

    if ( (iter = map_cpy.find (AUTHENTICATION_USERNAME)) != map_cpy.end()) {
        authenticationName = iter->second;
    }

    if ( (iter = map_cpy.find (USERNAME)) != map_cpy.end()) {
        username = iter->second;
    }

    if ( (iter = map_cpy.find (PASSWORD)) != map_cpy.end()) {
        password = iter->second;
    }

    if ( (iter = map_cpy.find (REALM)) != map_cpy.end()) {
        realm = iter->second;
    }

    setConfig (accountID, REALM, realm);

    setConfig (accountID, USERNAME, username);
    setConfig (accountID, AUTHENTICATION_USERNAME, authenticationName);

    if (!getMd5CredentialHashing()) {
        setConfig (accountID, PASSWORD, password);
    } else {
        // Make sure not to re-hash the password field if
        // it is already saved as a MD5 Hash.
        // TODO: This test is weak. Fix this.
        if ( (password.compare (getConfigString (accountID, PASSWORD)) != 0)) {
            _debug ("Password sent and password from config are different. Re-hashing\n");
            std::string hash;

            if (authenticationName.empty()) {
                hash = computeMd5HashFromCredential (username, password, realm);
            } else {
                hash = computeMd5HashFromCredential (authenticationName, password, realm);
            }

            setConfig (accountID, PASSWORD, hash);
        }
    }

    std::string alias;

    std::string mailbox;
    std::string accountEnable;
    std::string type;
    std::string resolveOnce;
    std::string registrationExpire;

    std::string hostname;
    std::string displayName;
    std::string publishedSameasLocal;
    std::string localAddress;
    std::string publishedAddress;
    std::string localPort;
    std::string publishedPort;
    std::string stunEnable;
    std::string stunServer;
    std::string srtpEnable;
    std::string zrtpDisplaySas;
    std::string zrtpDisplaySasOnce;
    std::string zrtpNotSuppWarning;
    std::string zrtpHelloHash;
    std::string srtpKeyExchange;

    std::string tlsEnable;
    std::string tlsCaListFile;
    std::string tlsCertificateFile;
    std::string tlsPrivateKeyFile;
    std::string tlsPassword;
    std::string tlsMethod;
    std::string tlsCiphers;
    std::string tlsServerName;
    std::string tlsVerifyServer;
    std::string tlsVerifyClient;
    std::string tlsRequireClientCertificate;
    std::string tlsNegotiationTimeoutSec;
    std::string tlsNegotiationTimeoutMsec;

    if ( (iter = map_cpy.find (HOSTNAME)) != map_cpy.end()) {
        hostname = iter->second;
    }

    if ( (iter = map_cpy.find (DISPLAY_NAME)) != map_cpy.end()) {
        displayName = iter->second;
    }

    if ( (iter = map_cpy.find (PUBLISHED_SAMEAS_LOCAL)) != map_cpy.end()) {
        publishedSameasLocal = iter->second;
    }

    if ( (iter = map_cpy.find (LOCAL_ADDRESS)) != map_cpy.end()) {
        localAddress = iter->second;
    }

    if ( (iter = map_cpy.find (PUBLISHED_ADDRESS)) != map_cpy.end()) {
        publishedAddress = iter->second;
    }

    if ( (iter = map_cpy.find (LOCAL_PORT)) != map_cpy.end()) {
        localPort = iter->second;
    }

    if ( (iter = map_cpy.find (PUBLISHED_PORT)) != map_cpy.end()) {
        publishedPort = iter->second;
    }

    if ( (iter = map_cpy.find (STUN_ENABLE)) != map_cpy.end()) {
        stunEnable = iter->second;
    }

    if ( (iter = map_cpy.find (STUN_SERVER)) != map_cpy.end()) {
        stunServer = iter->second;
    }

    if ( (iter = map_cpy.find (SRTP_ENABLE)) != map_cpy.end()) {
        srtpEnable = iter->second;
    }

    if ( (iter = map_cpy.find (ZRTP_DISPLAY_SAS)) != map_cpy.end()) {
        zrtpDisplaySas = iter->second;
    }

    if ( (iter = map_cpy.find (ZRTP_DISPLAY_SAS_ONCE)) != map_cpy.end()) {
        zrtpDisplaySasOnce = iter->second;
    }

    if ( (iter = map_cpy.find (ZRTP_NOT_SUPP_WARNING)) != map_cpy.end()) {
        zrtpNotSuppWarning = iter->second;
    }

    if ( (iter = map_cpy.find (ZRTP_HELLO_HASH)) != map_cpy.end()) {
        zrtpHelloHash = iter->second;
    }

    if ( (iter = map_cpy.find (SRTP_KEY_EXCHANGE)) != map_cpy.end()) {
        srtpKeyExchange = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_ALIAS)) != map_cpy.end()) {
        alias = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_MAILBOX)) != map_cpy.end()) {
        mailbox = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_ENABLE)) != map_cpy.end()) {
        accountEnable = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_TYPE)) != map_cpy.end()) {
        type = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_RESOLVE_ONCE)) != map_cpy.end()) {
        resolveOnce = iter->second;
    }

    if ( (iter = map_cpy.find (CONFIG_ACCOUNT_REGISTRATION_EXPIRE)) != map_cpy.end()) {
        registrationExpire = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_ENABLE)) != map_cpy.end()) {
        tlsEnable = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_CA_LIST_FILE)) != map_cpy.end()) {
        tlsCaListFile = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_CERTIFICATE_FILE)) != map_cpy.end()) {
        tlsCertificateFile = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_PRIVATE_KEY_FILE)) != map_cpy.end()) {
        tlsPrivateKeyFile = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_PASSWORD)) != map_cpy.end()) {
        tlsPassword = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_METHOD)) != map_cpy.end()) {
        tlsMethod = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_CIPHERS)) != map_cpy.end()) {
        tlsCiphers = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_SERVER_NAME)) != map_cpy.end()) {
        tlsServerName = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_VERIFY_SERVER)) != map_cpy.end()) {
        tlsVerifyServer = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_VERIFY_CLIENT)) != map_cpy.end()) {
        tlsVerifyClient = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_REQUIRE_CLIENT_CERTIFICATE)) != map_cpy.end()) {
        tlsRequireClientCertificate = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_NEGOTIATION_TIMEOUT_SEC)) != map_cpy.end()) {
        tlsNegotiationTimeoutSec = iter->second;
    }

    if ( (iter = map_cpy.find (TLS_NEGOTIATION_TIMEOUT_MSEC)) != map_cpy.end()) {
        tlsNegotiationTimeoutMsec = iter->second;
    }

    setConfig (accountID, HOSTNAME, hostname);

    setConfig (accountID, PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal);
    setConfig (accountID, LOCAL_ADDRESS, localAddress);
    setConfig (accountID, PUBLISHED_ADDRESS, publishedAddress);
    setConfig (accountID, LOCAL_PORT, localPort);
    setConfig (accountID, PUBLISHED_PORT, publishedPort);
    setConfig (accountID, DISPLAY_NAME, displayName);
    setConfig (accountID, SRTP_ENABLE, srtpEnable);
    setConfig (accountID, ZRTP_DISPLAY_SAS, zrtpDisplaySas);
    setConfig (accountID, ZRTP_DISPLAY_SAS_ONCE, zrtpDisplaySasOnce);
    setConfig (accountID, ZRTP_NOT_SUPP_WARNING, zrtpNotSuppWarning);
    setConfig (accountID, ZRTP_HELLO_HASH, zrtpHelloHash);
    setConfig (accountID, SRTP_KEY_EXCHANGE, srtpKeyExchange);

    setConfig (accountID, STUN_ENABLE, stunEnable);
    setConfig (accountID, STUN_SERVER, stunServer);

    setConfig (accountID, TLS_ENABLE, tlsEnable);
    setConfig (accountID, TLS_CA_LIST_FILE, tlsCaListFile);
    setConfig (accountID, TLS_CERTIFICATE_FILE, tlsCertificateFile);
    setConfig (accountID, TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile);
    setConfig (accountID, TLS_PASSWORD, tlsPassword);
    setConfig (accountID, TLS_METHOD, tlsMethod);
    setConfig (accountID, TLS_CIPHERS, tlsCiphers);
    setConfig (accountID, TLS_SERVER_NAME, tlsServerName);
    setConfig (accountID, TLS_VERIFY_SERVER, tlsVerifyServer);
    setConfig (accountID, TLS_VERIFY_CLIENT, tlsVerifyClient);
    setConfig (accountID, TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate);
    setConfig (accountID, TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec);
    setConfig (accountID, TLS_NEGOTIATION_TIMEOUT_MSEC, tlsNegotiationTimeoutMsec);

    setConfig (accountID, CONFIG_ACCOUNT_ALIAS, alias);
    setConfig (accountID, CONFIG_ACCOUNT_MAILBOX, mailbox);
    setConfig (accountID, CONFIG_ACCOUNT_ENABLE, accountEnable);
    setConfig (accountID, CONFIG_ACCOUNT_TYPE, type);
    setConfig (accountID, CONFIG_ACCOUNT_RESOLVE_ONCE, resolveOnce);
    setConfig (accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire);

    saveConfig();

    Account * acc = NULL;
    acc = getAccount (accountID);

    if (acc != NULL) {
        acc->loadConfig();

        if (acc->isEnabled()) {
            // acc->unregisterVoIPLink(); // do not need to send an unregister
            acc->registerVoIPLink();
        } else {
            acc->unregisterVoIPLink();
        }
    } else {
        _debug ("ManagerImpl::setAccountDetails: account is NULL\n");
    }

    // Update account details to the client side
    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();

}

void
ManagerImpl::sendRegister (const std::string& accountID , const int32_t& enable)
{

    // Update the active field
    setConfig (accountID, CONFIG_ACCOUNT_ENABLE, (enable == 1) ? TRUE_STR:FALSE_STR);

    Account* acc = getAccount (accountID);
    acc->loadConfig();

    // Test on the freshly updated value

    if (acc->isEnabled()) {
        // Verify we aren't already registered, then register
        _debug ("Send register for account %s\n" , accountID.c_str());
        acc->registerVoIPLink();
    } else {
        // Verify we are already registered, then unregister
        _debug ("Send unregister for account %s\n" , accountID.c_str());
        acc->unregisterVoIPLink();
    }

}

std::string
ManagerImpl::addAccount (const std::map< std::string, std::string >& details)
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

    _debug ("%s\n", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    if (accountType == "SIP") {
        newAccount = AccountCreator::createAccount (AccountCreator::SIP_ACCOUNT, newAccountID);
    } else if (accountType == "IAX") {
        newAccount = AccountCreator::createAccount (AccountCreator::IAX_ACCOUNT, newAccountID);
    } else {
        _debug ("Unknown %s param when calling addAccount(): %s\n", CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    _accountMap[newAccountID] = newAccount;

    setAccountDetails (accountID.str(), details);

    // Add the newly created account in the account order list
    account_list = getConfigString (PREFERENCES, CONFIG_ACCOUNTS_ORDER);

    if (account_list != "") {
        newAccountID += "/";
        // Prepend the new account
        account_list.insert (0, newAccountID);
        setConfig (PREFERENCES, CONFIG_ACCOUNTS_ORDER, account_list);
    }

    saveConfig();

    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();

    return newAccountID;
}

void
ManagerImpl::deleteAllCredential (const AccountID& accountID)
{
    int numberOfCredential = getConfigInt (accountID, CONFIG_CREDENTIAL_NUMBER);

    int i;

    for (i = 0; i < numberOfCredential; i++) {
        std::string credentialIndex;
        std::stringstream streamOut;
        streamOut << i;
        credentialIndex = streamOut.str();
        std::string section = "Credential" + std::string (":") + accountID + std::string (":") + credentialIndex;

        _config.removeSection (section);
    }

    if (accountID.empty() == false) {
        setConfig (accountID, CONFIG_CREDENTIAL_NUMBER, 0);
    }
}

void
ManagerImpl::removeAccount (const AccountID& accountID)
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

    _debug ("REMOVE ACCOUNT\n");

    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();

}

// ACCOUNT handling
bool
ManagerImpl::associateCallToAccount (const CallID& callID, const AccountID& accountID)
{
    if (getAccountFromCall (callID) == AccountNULL) { // nothing with the same ID
        if (accountExists (accountID)) {    // account id exist in AccountMap
            ost::MutexLock m (_callAccountMapMutex);
            _callAccountMap[callID] = accountID;
            _debug ("Associate Call %s with Account %s\n", callID.data(), accountID.data());
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

AccountID
ManagerImpl::getAccountFromCall (const CallID& callID)
{
    ost::MutexLock m (_callAccountMapMutex);
    CallAccountMap::iterator iter = _callAccountMap.find (callID);

    if (iter == _callAccountMap.end()) {
        return AccountNULL;
    } else {
        return iter->second;
    }
}

bool
ManagerImpl::removeCallAccount (const CallID& callID)
{
    ost::MutexLock m (_callAccountMapMutex);

    if (_callAccountMap.erase (callID)) {
        return true;
    }

    return false;
}

CallID
ManagerImpl::getNewCallID()
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

std::vector <std::string> ManagerImpl::loadAccountOrder (void)
{

    std::string account_list;
    std::vector <std::string> account_vect;

    account_list = getConfigString (PREFERENCES, CONFIG_ACCOUNTS_ORDER);
    return unserialize (account_list);
}


short
ManagerImpl::loadAccountMap()
{

    _debug ("ManagerImpl::loadAccountMap\n");

    short nbAccount = 0;
    TokenList sections = _config.getSections();
    std::string accountType;
    Account *tmpAccount = 0;
    std::vector <std::string> account_order;

    TokenList::iterator iter = sections.begin();

    // Those calls that are placed to an uri that cannot be
    // associated to an account are using that special account.
    // An account, that is not account, in the sense of
    // registration. This is useful since the Account object
    // provides a handful of method that simplifies URI creation
    // and loading of various settings.
    _directIpAccount = AccountCreator::createAccount (AccountCreator::SIP_DIRECT_IP_ACCOUNT, "");

    if (_directIpAccount == NULL) {
        _debug ("Failed to create direct ip calls \"account\"\n");
    } else {
        // Force the options to be loaded
        // No registration in the sense of
        // the REGISTER method is performed.
        _debug ("Succeed to create direct ip calls \"account\"\n");
        _accountMap[IP2IP_PROFILE] = _directIpAccount;
        _directIpAccount->registerVoIPLink();
    }

    while (iter != sections.end()) {
        // Check if it starts with "Account:" (SIP and IAX pour le moment)
        if ( (int) (iter->find ("Account:")) != 0) {
            iter++;
            continue;
        }

        accountType = getConfigString (*iter, CONFIG_ACCOUNT_TYPE);

        if (accountType == "SIP") {
            tmpAccount = AccountCreator::createAccount (AccountCreator::SIP_ACCOUNT, *iter);
        }

        else if (accountType == "IAX") {
            tmpAccount = AccountCreator::createAccount (AccountCreator::IAX_ACCOUNT, *iter);
        }

        else {
            _debug ("Unknown %s param in config file (%s)\n", CONFIG_ACCOUNT_TYPE, accountType.c_str());
        }

        if (tmpAccount != NULL) {
            _debug ("Loading account %s \n", iter->c_str());
            _accountMap[iter->c_str() ] = tmpAccount;
            nbAccount++;
        }

        iter++;
    }

    /*
    if (_directIpAccount == NULL) {
        _debug ("Failed to create direct ip calls \"account\"\n");
    } else {
        // Force the options to be loaded
        // No registration in the sense of
        // the REGISTER method is performed.
        _debug ("Succeed to create direct ip calls \"account\"\n");
        _directIpAccount->registerVoIPLink();
    _accountMap[IP2IP_PROFILE] = _directIpAccount;
    }
    */
    _debug ("nbAccount loaded %i \n", nbAccount);

    return nbAccount;
}

void
ManagerImpl::unloadAccountMap()
{

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {

        _debug ("-> Unloading account %s\n", iter->first.c_str());
        delete iter->second;
        iter->second = 0;

        iter++;
    }

    _accountMap.clear();
}

bool
ManagerImpl::accountExists (const AccountID& accountID)
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
        _debug ("Returns the direct IP account\n");
        return _directIpAccount;
    }

    AccountMap::iterator iter = _accountMap.find (accountID);

    if (iter == _accountMap.end()) {
        return NULL;
    }

    return iter->second;
}

AccountID
ManagerImpl::getAccountIdFromNameAndServer (const std::string& userName, const std::string& server)
{
    AccountMap::iterator iter;
    SIPAccount *account;
    _debug ("getAccountIdFromNameAndServer : username = %s , server = %s\n", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        _debug ("for : account = %s\n", iter->first.c_str());
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->fullMatch (userName, server)) {
                _debug ("Matching accountId in request is a fullmatch\n");
                return iter->first;
            }
        }
    }

    // We failed! Then only match the hostname
    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->hostnameMatch (server)) {
                _debug ("Matching accountId in request with hostname\n");
                return iter->first;
            }
        }
    }

    // We failed! Then only match the username
    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = dynamic_cast<SIPAccount *> (iter->second);

        if (account != NULL) {
            if (account->userMatch (userName)) {
                _debug ("Matching accountId in request with username\n");
                return iter->first;
            }
        }
    }

    // Failed again! return AccountNULL
    return AccountNULL;
}

void ManagerImpl::restartPJSIP (void)
{
    _debug ("ManagerImpl::restartPJSIP\n");
    VoIPLink *link = getSIPAccountLink();
    SIPVoIPLink *siplink = NULL;

    if (link) {
        siplink = dynamic_cast<SIPVoIPLink*> (getSIPAccountLink ());
    }

    _debug ("ManagerImpl::unregister sip account\n");

    this->unregisterCurSIPAccounts();
    /* Terminate and initialize the PJSIP library */

    if (siplink) {
        _debug ("ManagerImpl::Terminate sip\n");
        siplink->terminate ();
        siplink = SIPVoIPLink::instance ("");
        _debug ("ManagerImpl::Init new sip\n");
        siplink->init ();
    }

    _debug ("ManagerImpl::register sip account\n");

    /* Then register all enabled SIP accounts */
    this->registerCurSIPAccounts ();
}

VoIPLink* ManagerImpl::getAccountLink (const AccountID& accountID)
{
    if (accountID!=AccountNULL) {
        Account* acc = getAccount (accountID);

        if (acc) {
            return acc->getVoIPLink();
        }

        return 0;
    } else
        return SIPVoIPLink::instance ("");
}

VoIPLink* ManagerImpl::getSIPAccountLink()
{
    /* We are looking for the first SIP account we met because all the SIP accounts have the same voiplink */
    Account *account;
    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {

        account = iter->second;

        if (account->getType() == "sip") {
            return account->getVoIPLink();
        }

        ++iter;
    }

    return NULL;
}




pjsip_regc *getSipRegcFromID (const AccountID& id UNUSED)
{
    /*SIPAccount *tmp = dynamic_cast<SIPAccount *>getAccount(id);
    if(tmp != NULL)
      return tmp->getSipRegc();
    else*/
    return NULL;
}

void ManagerImpl::unregisterCurSIPAccounts()
{
    Account *current;

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {
        current = iter->second;

        if (current) {
            if (current->isEnabled() && current->getType() == "sip") {
                current->unregisterVoIPLink();
            }
        }

        iter++;
    }
}

void ManagerImpl::registerCurSIPAccounts (void)
{

    Account *current;

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {
        current = iter->second;

        if (current) {
            if (current->isEnabled() && current->getType() == "sip") {
                //current->setVoIPLink(link);
                current->registerVoIPLink();
            }
        }

        current = NULL;

        iter++;
    }
}


std::map<std::string, int32_t> ManagerImpl::getAddressbookSettings ()
{

    std::map<std::string, int32_t> settings;

    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_ENABLE", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_ENABLE)));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_MAX_RESULTS", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_MAX_RESULTS)));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_CONTACT_PHOTO)));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_BUSINESS)));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_HOME", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_HOME)));
    settings.insert (std::pair<std::string, int32_t> ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE", getConfigInt (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_MOBILE)));

    return settings;
}

void ManagerImpl::setAddressbookSettings (const std::map<std::string, int32_t>& settings)
{

    setConfig (ADDRESSBOOK, ADDRESSBOOK_ENABLE, (*settings.find ("ADDRESSBOOK_ENABLE")).second);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_MAX_RESULTS, (*settings.find ("ADDRESSBOOK_MAX_RESULTS")).second);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_CONTACT_PHOTO , (*settings.find ("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")).second);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_BUSINESS , (*settings.find ("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")).second);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_HOME , (*settings.find ("ADDRESSBOOK_DISPLAY_PHONE_HOME")).second);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_DISPLAY_PHONE_MOBILE , (*settings.find ("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")).second);

    // Write it to the configuration file
    saveConfig ();
}

void
ManagerImpl::setAddressbookList (const std::vector<  std::string >& list)
{

    std::string s = serialize (list);
    setConfig (ADDRESSBOOK, ADDRESSBOOK_LIST, s);
}

std::vector <std::string>
ManagerImpl::getAddressbookList (void)
{

    std::string s = getConfigString (ADDRESSBOOK, ADDRESSBOOK_LIST);
    return unserialize (s);
}

std::map<std::string, std::string> ManagerImpl::getHookSettings ()
{

    std::map<std::string, std::string> settings;

    settings.insert (std::pair<std::string, std::string> ("URLHOOK_SIP_FIELD", getConfigString (HOOKS, URLHOOK_SIP_FIELD)));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_COMMAND", getConfigString (HOOKS, URLHOOK_COMMAND)));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_SIP_ENABLED", getConfigString (HOOKS, URLHOOK_SIP_ENABLED)));
    settings.insert (std::pair<std::string, std::string> ("URLHOOK_IAX2_ENABLED", getConfigString (HOOKS, URLHOOK_IAX2_ENABLED)));
    settings.insert (std::pair<std::string, std::string> ("PHONE_NUMBER_HOOK_ENABLED", getConfigString (HOOKS, PHONE_NUMBER_HOOK_ENABLED)));
    settings.insert (std::pair<std::string, std::string> ("PHONE_NUMBER_HOOK_ADD_PREFIX", getConfigString (HOOKS, PHONE_NUMBER_HOOK_ADD_PREFIX)));

    return settings;
}

void ManagerImpl::setHookSettings (const std::map<std::string, std::string>& settings)
{

    setConfig (HOOKS, URLHOOK_SIP_FIELD, (*settings.find ("URLHOOK_SIP_FIELD")).second);
    setConfig (HOOKS, URLHOOK_COMMAND, (*settings.find ("URLHOOK_COMMAND")).second);
    setConfig (HOOKS, URLHOOK_SIP_ENABLED, (*settings.find ("URLHOOK_SIP_ENABLED")).second);
    setConfig (HOOKS, URLHOOK_IAX2_ENABLED, (*settings.find ("URLHOOK_IAX2_ENABLED")).second);
    setConfig (HOOKS, PHONE_NUMBER_HOOK_ENABLED, (*settings.find ("PHONE_NUMBER_HOOK_ENABLED")).second);
    setConfig (HOOKS, PHONE_NUMBER_HOOK_ADD_PREFIX, (*settings.find ("PHONE_NUMBER_HOOK_ADD_PREFIX")).second);

    // Write it to the configuration file
    saveConfig ();
}

void ManagerImpl::check_call_configuration (const CallID& id, const std::string &to, Call::CallConfiguration *callConfig)
{
    Call::CallConfiguration config;

    if (to.find (SIP_SCHEME) == 0 || to.find (SIPS_SCHEME) == 0) {
        _debug ("Sending Sip Call \n");
        config = Call::IPtoIP;
    } else {
        config = Call::Classic;
    }

    associateConfigToCall (id, config);

    *callConfig = config;
}


bool ManagerImpl::associateConfigToCall (const CallID& callID, Call::CallConfiguration config)
{

    if (getConfigFromCall (callID) == CallConfigNULL) { // nothing with the same ID
        _callConfigMap[callID] = config;
        _debug ("Associate Call %s with config %i\n", callID.data(), config);
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

std::map< std::string, std::string > ManagerImpl::getCallDetails (const CallID& callID)
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
    _debug ("%s\n",callID.c_str());
    // Then the VoIP link this account is linked with (IAX2 or SIP)

    if ( (account=getAccount (accountid)) != 0) {
        link = account->getVoIPLink ();

        if (link) {
            call = link->getCall (callID);
        }
    }

    if (call) {
        type << call->getCallType ();
        call_details.insert (std::pair<std::string, std::string> ("ACCOUNTID", accountid));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NUMBER", call->getPeerNumber ()));
        call_details.insert (std::pair<std::string, std::string> ("PEER_NAME", call->getPeerName ()));
        call_details.insert (std::pair<std::string, std::string> ("DISPLAY_NAME", call->getDisplayName ()));
        call_details.insert (std::pair<std::string, std::string> ("CALL_STATE", call->getStateStr ()));
        call_details.insert (std::pair<std::string, std::string> ("CALL_TYPE", type.str ()));
    } else {
        _debug ("Error: Managerimpl - getCallDetails ()\n");
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
    return _history->get_history_serialized ();
}

void ManagerImpl::receive_history_from_client (std::map<std::string, std::string> history)
{
    _history->set_serialized_history (history, Manager::instance().getConfigInt (PREFERENCES, CONFIG_HISTORY_LIMIT));
    _history->save_history ();
}


std::vector< std::string >
ManagerImpl::getCallList (void)
{
    std::vector< std::string > v;

    CallAccountMap::iterator iter = _callAccountMap.begin ();

    while (iter != _callAccountMap.end ()) {
        v.push_back (iter->first.data());
        iter++;
    }

    return v;
}


std::map< std::string, std::string >
ManagerImpl::getConferenceDetails (const ConfID& confID)
{

    std::map<std::string, std::string> conf_details;
    ConferenceMap::iterator iter_conf;

    iter_conf = _conferencemap.find (confID);

    Conference* conf = NULL;

    if (iter_conf != _conferencemap.end()) {

        conf = iter_conf->second;
        conf_details.insert (std::pair<std::string, std::string> ("CONFID", confID));
        conf_details.insert (std::pair<std::string, std::string> ("CONF_STATE", conf->getStateStr()));
    }

    return conf_details;
}


std::vector< std::string >
ManagerImpl::getConferenceList (void)
{
    _debug ("ManagerImpl::getConferenceList\n");
    std::vector< std::string > v;

    ConferenceMap::iterator iter = _conferencemap.begin();

    while (iter != _conferencemap.end ()) {
        v.push_back (iter->first);
        iter++;
    }

    return v;
}


std::vector< std::string >
ManagerImpl::getParticipantList (const std::string& confID)
{
    _debug ("ManagerImpl::getParticipantList\n");
    std::vector< std::string > v;

    ConferenceMap::iterator iter_conf = _conferencemap.find (confID);
    Conference *conf = NULL;

    if (iter_conf != _conferencemap.end())
        conf = iter_conf->second;

    if (conf != NULL) {
        ParticipantSet participants = conf->getParticipantList();
        ParticipantSet::iterator iter_participant = participants.begin();

        while (iter_participant != participants.end ()) {

            v.push_back (*iter_participant);

            iter_participant++;
        }
    }

    return v;
}

