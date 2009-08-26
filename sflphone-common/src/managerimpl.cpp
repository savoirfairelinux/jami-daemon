/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
        , _firewallPort()
        , _firewallAddr ("")
        , _hasZeroconf (false)
        , _callAccountMap()
        , _callAccountMapMutex()
        , _callConfigMap()
        , _accountMap()
        , _cleaner (NULL)
        , _history (NULL)
        , _directIpAccount (NULL)
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
    _debug ("Current call ID = %s\n", _currentCallId2.c_str());

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
    _currentCallId2 = id;
}


///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool
ManagerImpl::outgoingCall (const std::string& accountid, const CallID& id, const std::string& to)
{
    std::string pattern, to_cleaned;
    Call::CallConfiguration callConfig;
    SIPVoIPLink *siplink;

    _debug ("ManagerImpl::outgoingCall() method \n");

    if (getConfigString (HOOKS, PHONE_NUMBER_HOOK_ENABLED) ==  "1")
        _cleaner->set_phone_number_prefix (getConfigString (HOOKS, PHONE_NUMBER_HOOK_ADD_PREFIX));
    else
        _cleaner->set_phone_number_prefix ("");

    to_cleaned = _cleaner->clean (to);

    /* Check what kind of call we are dealing with */
    check_call_configuration (id, to_cleaned, &callConfig);

    if (callConfig == Call::IPtoIP) {
        _debug ("Start IP to IP call\n");
        /* We need to retrieve the sip voiplink instance */
        siplink = SIPVoIPLink::instance ("");

        if (siplink->new_ip_to_ip_call (id, to_cleaned)) {
            switchCall (id);
            return true;
        } else {
            callFailure (id);
        }

        return false;
    }

    if (!accountExists (accountid)) {
        _debug ("! Manager Error: Outgoing Call: account doesn't exist\n");
        return false;
    }

    if (getAccountFromCall (id) != AccountNULL) {
        _debug ("! Manager Error: Outgoing Call: call id already exists\n");
        return false;
    }

    if (hasCurrentCall()) {
        _debug ("* Manager Info: there is currently a call, try to hold it\n");
        onHoldCall (getCurrentCallId());
    }

    _debug ("- Manager Action: Adding Outgoing Call %s on account %s\n", id.data(), accountid.data());

    associateCallToAccount (id, accountid);

    if (getAccountLink (accountid)->newOutgoingCall (id, to_cleaned)) {
        switchCall (id);
        return true;
    } else {
        callFailure (id);
        _debug ("! Manager Error: An error occur, the call was not created\n");
    }

    return false;
}

//THREAD=Main : for outgoing Call
bool
ManagerImpl::answerCall (const CallID& id)
{
    stopTone (true);

    AccountID currentAccountId;
    currentAccountId = getAccountFromCall (id);
    if(currentAccountId == AccountNULL) {
        _debug("ManagerImpl::answerCall : AccountId is null\n");
    }
    
    Call* currentCall = NULL;
    currentCall = getAccountLink (currentAccountId)->getCall (id);
    if (currentCall == NULL) {
        _debug("ManagerImpl::answerCall : currentCall is null\n");
    }
    
    Call* lastCall = NULL;
    if (!getCurrentCallId().empty()) {
        lastCall = getAccountLink (currentAccountId)->getCall (getCurrentCallId());
        if (lastCall == NULL) {
            _debug("ManagerImpl::answerCall : lastCall is null\n");
        }
    }

    _debug ("ManagerImpl::answerCall :: current call->getState %i \n", currentCall->getState());
    _debug ("Try to answer call: %s\n", id.data());

    if (lastCall != NULL) {
        if (lastCall->getState() == Call::Active) {
            _debug ("* Manager Info: there is currently a call, try to hold it\n");
            onHoldCall (getCurrentCallId());
        }
    }

    if (!getAccountLink (currentAccountId)->answer (id)) {
        // error when receiving...
        removeCallAccount (id);
        return false;
    }

    // if it was waiting, it's waiting no more
    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "CURRENT");
        
    std::string codecName = Manager::instance().getCurrentCodecName (id);
    if (_dbus) _dbus->getCallManager()->currentSelectedCodec (id,codecName.c_str());
        
    removeWaitingCall (id);

    switchCall (id);

    return true;
}

//THREAD=Main
bool
ManagerImpl::hangupCall (const CallID& id)
{
    _debug ("ManagerImpl::hangupCall()\n");
    PulseLayer *pulselayer;
    AccountID accountid;
    bool returnValue;
    AudioLayer *audiolayer;

    stopTone (false);

    /* Broadcast a signal over DBus */

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "HUNGUP");

    _debug ("Stop audio stream\n");

    audiolayer = getAudioDriver();

    int nbCalls = getCallList().size();

    _debug ("hangupCall: callList is of size %i call(s)\n", nbCalls);

    // stop stream
    if (! (nbCalls > 1))
        audiolayer->stopStream();

    /* Direct IP to IP call */
    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)->hangup (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("! Manager Hangup Call: Call doesn't exists\n");
            return false;
        }

        returnValue = getAccountLink (accountid)->hangup (id);

        removeCallAccount (id);
    }

    switchCall ("");

    if (_audiodriver->getLayerType() == PULSEAUDIO && getConfigInt (PREFERENCES , CONFIG_PA_VOLUME_CTRL)) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());

        if (pulselayer)  pulselayer->restorePulseAppsVolume();
    }



    return returnValue;
}

//THREAD=Main
bool
ManagerImpl::cancelCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;

    stopTone (true);

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
ManagerImpl::onHoldCall (const CallID& id)
{
    AccountID accountid;
    bool returnValue;
    CallID call_id;

    stopTone (true);

    call_id = id;

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> onhold (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("Manager On Hold Call: Account ID %s or callid %s doesn't exists\n", accountid.c_str(), id.c_str());
            return false;
        }

        returnValue = getAccountLink (accountid)->onhold (id);
    }

    removeWaitingCall (id);

    switchCall ("");

    if (_dbus) _dbus->getCallManager()->callStateChanged (call_id, "HOLD");

    return returnValue;
}

//THREAD=Main
bool
ManagerImpl::offHoldCall (const CallID& id)
{

    AccountID accountid;
    bool returnValue, rec;
    std::string codecName;
    CallID call_id;

    stopTone (false);

    call_id = id;
    //Place current call on hold if it isn't

    if (hasCurrentCall()) {
        _debug ("Put the current call (ID=%s) on hold\n", getCurrentCallId().c_str());
        onHoldCall (getCurrentCallId());
    }

    /* Direct IP to IP call */
    if (getConfigFromCall (id) == Call::IPtoIP) {
        rec = SIPVoIPLink::instance (AccountNULL)-> isRecording (id);
        returnValue = SIPVoIPLink::instance (AccountNULL)-> offhold (id);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("Manager OffHold Call: Call doesn't exists\n");
            return false;
        }

        _debug ("Setting OFFHOLD, Account %s, callid %s\n", accountid.c_str(), id.c_str());

        rec = getAccountLink (accountid)->isRecording (id);
        returnValue = getAccountLink (accountid)->offhold (id);
    }


    if (_dbus) {
        if (rec)
            _dbus->getCallManager()->callStateChanged (call_id, "UNHOLD_RECORD");
        else
            _dbus->getCallManager()->callStateChanged (call_id, "UNHOLD_CURRENT");

    }

    switchCall (id);

    codecName = getCurrentCodecName (id);
    // _debug("ManagerImpl::hangupCall(): broadcast codec name %s \n",codecName.c_str());

    if (_dbus) _dbus->getCallManager()->currentSelectedCodec (id,codecName.c_str());

    return returnValue;
}

//THREAD=Main
bool
ManagerImpl::transferCall (const CallID& id, const std::string& to)
{
    AccountID accountid;
    bool returnValue;

    stopTone (true);

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        returnValue = SIPVoIPLink::instance (AccountNULL)-> transfer (id, to);
    }

    /* Classic call, attached to an account */
    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("! Manager Transfer Call: Call doesn't exists\n");
            return false;
        }

        returnValue = getAccountLink (accountid)->transfer (id, to);

        removeCallAccount (id);
    }

    removeWaitingCall (id);

    switchCall ("");

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "HUNGUP");

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

    stopTone (true);

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

        switchCall ("");
    }

    return returnValue;
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
        //_debug("Send DTMF: call doesn't exists\n");
        playDtmf (code, false);
        return false;
    }

    int sendType = getConfigInt (SIGNALISATION, SEND_DTMF_AS);

    bool returnValue = false;

    switch (sendType) {

        case 0: // SIP INFO
            playDtmf (code , true);
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
ManagerImpl::playDtmf (char code, bool isTalking)
{
    int pulselen, layer, size;
    bool ret = false;
    AudioLayer *audiolayer;
    SFLDataFormat *buf;

    stopTone (false);

    bool hasToPlayTone = getConfigBool (SIGNALISATION, PLAY_DTMF);

    if (!hasToPlayTone)
        return false;

    // length in milliseconds
    pulselen = getConfigInt (SIGNALISATION, PULSE_LENGTH);

    if (!pulselen)
        return false;

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS
    audiolayer = getAudioDriver();

    layer = audiolayer->getLayerType();

    // fast return, no sound, so no dtmf
    if (audiolayer==0 || _dtmfKey == 0)
        return false;

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    size = (int) (pulselen * ( (float) audiolayer->getSampleRate() /1000));

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
    std::string from, number;

    stopTone (true);

    _debug ("Incoming call %s for account %s\n", call->getCallId().data(), accountId.c_str());

    associateCallToAccount (call->getCallId(), accountId);

    if (accountId==AccountNULL)
        associateConfigToCall (call->getCallId(), Call::IPtoIP);

    _debug ("ManagerImpl::incomingCall :: hasCurrentCall() %i \n",hasCurrentCall());

    if (!hasCurrentCall()) {
        call->setConnectionState (Call::Ringing);
        ringtone();
        switchCall (call->getCallId());

    }

    /*
    else {
        addWaitingCall(call->getCallId());
    }
    */

    addWaitingCall (call->getCallId());

    from = call->getPeerName();

    number = call->getPeerNumber();

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
    if (_dbus) _dbus->getCallManager()->incomingCall (accountId, call->getCallId(), from);

    //if (_dbus) _dbus->getCallManager()->callStateChanged(call->getCallId(), "INCOMING");

    // Reduce volume of the other pulseaudio-connected audio applications
    if (_audiodriver->getLayerType() == PULSEAUDIO && getConfigInt (PREFERENCES , CONFIG_PA_VOLUME_CTRL)) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());

        if (pulselayer)  pulselayer->reducePulseAppsVolume();
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
    if (isCurrentCall (id)) {
        stopTone (false);
    }

    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "CURRENT");

    std::string codecName = getCurrentCodecName (id);

    // _debug("ManagerImpl::hangupCall(): broadcast codec name %s \n",codecName.c_str());
    if (_dbus) _dbus->getCallManager()->currentSelectedCodec (id,codecName.c_str());
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
ManagerImpl::peerHungupCall (const CallID& id)
{
    PulseLayer *pulselayer;
    AccountID accountid;
    bool returnValue;

    /* Direct IP to IP call */

    if (getConfigFromCall (id) == Call::IPtoIP) {
        SIPVoIPLink::instance (AccountNULL)->hangup (id);
    }

    else {
        accountid = getAccountFromCall (id);

        if (accountid == AccountNULL) {
            _debug ("peerHungupCall: Call doesn't exists\n");
            return;
        }

        returnValue = getAccountLink (accountid)->peerHungup (id);
    }

    /* Broadcast a signal over DBus */
    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "HUNGUP");

    if (isCurrentCall (id)) {
        stopTone (true);
        switchCall ("");
    }

    removeWaitingCall (id);

    removeCallAccount (id);

    if (_audiodriver->getLayerType() == PULSEAUDIO && getConfigInt (PREFERENCES , CONFIG_PA_VOLUME_CTRL)) {
        pulselayer = dynamic_cast<PulseLayer *> (getAudioDriver());

        if (pulselayer)  pulselayer->restorePulseAppsVolume();
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
ManagerImpl::callFailure (const CallID& id)
{
    if (_dbus) _dbus->getCallManager()->callStateChanged (id, "FAILURE");

    _debug ("CALL ID = %s\n" , id.c_str());

    if (isCurrentCall (id)) {
        playATone (Tone::TONE_BUSY);
        switchCall ("");
    }

    removeCallAccount (id);

    removeWaitingCall (id);

}

//THREAD=VoIP
void
ManagerImpl::startVoiceMessageNotification (const AccountID& accountId, int nb_msg)
{
    if (_dbus) _dbus->getCallManager()->voiceMailNotify (accountId, nb_msg) ;
}

void ManagerImpl::connectionStatusNotification()
{
    if (_dbus)
        _dbus->getConfigurationManager()->accountsChanged();
}

/**
 * Multi Thread
 */
bool ManagerImpl::playATone (Tone::TONEID toneId)
{
    bool hasToPlayTone;
    AudioLoop *audioloop;
    AudioLayer *audiolayer;
    unsigned int nbSamples;

    hasToPlayTone = getConfigBool (SIGNALISATION, PLAY_TONES);

    if (!hasToPlayTone)
        return false;

    audiolayer = getAudioDriver();

    if (_telephoneTone != 0) {
        _toneMutex.enterMutex();
        _telephoneTone->setCurrentTone (toneId);
        _toneMutex.leaveMutex();

        audioloop = getTelephoneTone();
        nbSamples = audioloop->getSize();
        SFLDataFormat buf[nbSamples];

        if (audiolayer) {
            audiolayer->putUrgent (buf, nbSamples);
        } else
            return false;
    }

    return true;
}

/**
 * Multi Thread
 */
void ManagerImpl::stopTone (bool stopAudio=true)
{
    bool hasToPlayTone;

    hasToPlayTone = getConfigBool(SIGNALISATION, PLAY_TONES);

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
    playATone (Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringtone()
{
    std::string ringchoice;
    AudioLayer *audiolayer;
    AudioCodec *codecForTone;
    int layer, samplerate;
    bool loadFile;

    // stopTone(true);

    if (isRingtoneEnabled()) {
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

            if (CHECK_INTERFACE (layer, ALSA)) {
                //ringback();

            } else {
                audiolayer->startStream();
            }
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

    if (audiolayer != 0) {
        samplerate = audiolayer->getSampleRate();
        frequency << "440/" << FRAME_PER_BUFFER;
        Tone tone (frequency.str(), samplerate);
        nbSampling = tone.getSize();
        SFLDataFormat buf[nbSampling];
        tone.getNext (buf, tone.getSize());
        /* Put the data in the urgent ring buffer */
        audiolayer->putUrgent (buf, sizeof (SFLDataFormat) *nbSampling);
    }
}

/**
 * Multi Thread
 */
bool
ManagerImpl::getStunInfo (StunAddress4& stunSvrAddr, int port)
{
    StunAddress4 mappedAddr;

    struct in_addr in;
    char* addr;

    int fd1 = stunOpenSocket (stunSvrAddr, &mappedAddr, port);
    bool ok = (fd1 == -1 || fd1 == INVALID_SOCKET) ? false : true;

    if (ok) {
        closesocket (fd1);
        _firewallPort = mappedAddr.port;
        // Convert ipv4 address to host byte ordering
        in.s_addr = ntohl (mappedAddr.addr);
        addr = inet_ntoa (in);
        _firewallAddr = std::string (addr);
        _debug ("STUN Firewall: [%s:%d]\n", _firewallAddr.data(), _firewallPort);
        return true;
    } else {
        _debug ("Opening a stun socket pair failed\n");
    }

    return false;
}

bool
ManagerImpl::isBehindNat (const std::string& svr, int port)
{
    StunAddress4 stunSvrAddr;
    stunSvrAddr.addr = 0;

    // Convert char* to StunAddress4 structure
    bool ret = stunParseServerName ( (char*) svr.data(), stunSvrAddr);

    if (!ret) {
        _debug ("SIP: Stun server address (%s) is not valid\n", svr.data());
        return 0;
    }

    // Firewall address
    _debug ("STUN server: %s\n", svr.data());

    return getStunInfo (stunSvrAddr, port);
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

    //_path = std::string (HOMEDIR) + DIR_SEPARATOR_STR + "." + PROGDIR;
    if (XDG_CONFIG_HOME != NULL) 
	{
		xdg_env = std::string (XDG_CONFIG_HOME);
		(xdg_env.length() > 0) ? _path = xdg_env
							:	 _path = xdg_config;
	}
	else
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
    _debug("ManagerImpl::InitConfigFile\n");
    
    // Default values, that will be overwritten by the call to
    // 'populateFromFile' below.    
     
    // Peer to peer settings
    _config.addDefaultValue(std::pair<std::string, std::string> (SRTP_ENABLE, FALSE_STR), IP2IP_PROFILE);
    _config.addDefaultValue(std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, "1"), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (ZRTP_HELLO_HASH, TRUE_STR), IP2IP_PROFILE);      
    _config.addDefaultValue(std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, TRUE_STR), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, FALSE_STR), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, TRUE_STR), IP2IP_PROFILE);          
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_ENABLE, FALSE_STR), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_CA_LIST_FILE, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, EMPTY_FIELD), IP2IP_PROFILE);
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, EMPTY_FIELD), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_PASSWORD, EMPTY_FIELD), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_METHOD, "TLSv1"), IP2IP_PROFILE);        
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_CIPHERS, EMPTY_FIELD), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_SERVER_NAME, EMPTY_FIELD), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_VERIFY_SERVER, TRUE_STR), IP2IP_PROFILE);        
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, TRUE_STR), IP2IP_PROFILE);    
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, TRUE_STR), IP2IP_PROFILE);        
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, "2"), IP2IP_PROFILE);        
    _config.addDefaultValue(std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, "0"), IP2IP_PROFILE); 
    _config.addDefaultValue(std::pair<std::string, std::string> (LOCAL_PORT, DEFAULT_SIP_PORT), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (PUBLISHED_PORT, DEFAULT_SIP_PORT), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (LOCAL_ADDRESS, DEFAULT_ADDRESS), IP2IP_PROFILE);  
    _config.addDefaultValue(std::pair<std::string, std::string> (PUBLISHED_ADDRESS, DEFAULT_ADDRESS), IP2IP_PROFILE);
    
    // Init display name to the username under which 
    // this sflphone instance is running.
    std::string diplayName("");
    uid_t uid = getuid();
    struct passwd * user_info = NULL;
    user_info = getpwuid(uid);
    if (user_info != NULL) {
        diplayName = user_info->pw_name;
    }
    _config.addDefaultValue(std::pair<std::string, std::string> (DISPLAY_NAME, diplayName), IP2IP_PROFILE);                       
    
    // Signalisation settings       
    _config.addDefaultValue(std::pair<std::string, std::string> (SYMMETRIC, TRUE_STR), SIGNALISATION);  
    _config.addDefaultValue(std::pair<std::string, std::string> (PLAY_DTMF, TRUE_STR), SIGNALISATION);  
    _config.addDefaultValue(std::pair<std::string, std::string> (PLAY_TONES, TRUE_STR), SIGNALISATION);      
    _config.addDefaultValue(std::pair<std::string, std::string> (PULSE_LENGTH, DFT_PULSE_LENGTH_STR), SIGNALISATION);  
    _config.addDefaultValue(std::pair<std::string, std::string> (SEND_DTMF_AS, SIP_INFO_STR), SIGNALISATION);         
    _config.addDefaultValue(std::pair<std::string, std::string> (ZRTP_ZIDFILE, ZRTP_ZID_FILENAME), SIGNALISATION);        
 
    // Audio settings           
    _config.addDefaultValue(std::pair<std::string, std::string> (ALSA_CARD_ID_IN, ALSA_DFT_CARD), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (ALSA_CARD_ID_OUT, ALSA_DFT_CARD), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (ALSA_SAMPLE_RATE, DFT_SAMPLE_RATE), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (ALSA_FRAME_SIZE, DFT_FRAME_SIZE), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (ALSA_PLUGIN, PCM_DEFAULT), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (RING_CHOICE, DFT_RINGTONE), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (VOLUME_SPKR, DFT_VOL_SPKR_STR), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (VOLUME_MICRO, DFT_VOL_MICRO_STR), AUDIO);
    _config.addDefaultValue(std::pair<std::string, std::string> (RECORD_PATH,DFT_RECORD_PATH), AUDIO);

    // General settings       
    _config.addDefaultValue(std::pair<std::string, std::string> (ZONE_TONE, DFT_ZONE), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_RINGTONE, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_DIALPAD, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_SEARCHBAR, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_START, FALSE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_POPUP, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_NOTIFY, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_MAIL_NOTIFY, FALSE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_VOLUME, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_HISTORY_LIMIT, DFT_HISTORY_LIMIT), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_HISTORY_ENABLED, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_AUDIO, DFT_AUDIO_MANAGER), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_PA_VOLUME_CTRL, TRUE_STR), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_SIP_PORT, DFT_SIP_PORT), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNTS_ORDER, EMPTY_FIELD), PREFERENCES);
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_MD5HASH, FALSE_STR), PREFERENCES);

    // Addressbook settings       
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_ENABLE, TRUE_STR), ADDRESSBOOK);
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_MAX_RESULTS, "25"), ADDRESSBOOK);
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_CONTACT_PHOTO, FALSE_STR), ADDRESSBOOK);
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_BUSINESS, TRUE_STR), ADDRESSBOOK);
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_HOME, FALSE_STR), ADDRESSBOOK);
    _config.addDefaultValue(std::pair<std::string, std::string> (ADDRESSBOOK_DISPLAY_PHONE_MOBILE, FALSE_STR), ADDRESSBOOK);

    // Hooks settings       
    _config.addDefaultValue(std::pair<std::string, std::string> (URLHOOK_SIP_FIELD, HOOK_DEFAULT_SIP_FIELD), HOOKS);
    _config.addDefaultValue(std::pair<std::string, std::string> (URLHOOK_COMMAND, HOOK_DEFAULT_URL_COMMAND), HOOKS);
    _config.addDefaultValue(std::pair<std::string, std::string> (URLHOOK_SIP_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue(std::pair<std::string, std::string> (URLHOOK_IAX2_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue(std::pair<std::string, std::string> (PHONE_NUMBER_HOOK_ENABLED, FALSE_STR), HOOKS);
    _config.addDefaultValue(std::pair<std::string, std::string> (PHONE_NUMBER_HOOK_ADD_PREFIX, EMPTY_FIELD), HOOKS);

    std::string path;
    // Loads config from ~/.sflphone/sflphonedrc or so..
    if (createSettingsPath() == 1 && load_user_value) {
        (alternate == "") ? path = _path : path = alternate;
        std::cout << path << std::endl;
        _exist = _config.populateFromFile (path);
    }
    
    // Globally shared default values (not to be populated from file)    
    _config.addDefaultValue(std::pair<std::string, std::string> (HOSTNAME, EMPTY_FIELD));
    _config.addDefaultValue(std::pair<std::string, std::string> (AUTHENTICATION_USERNAME, EMPTY_FIELD));
    _config.addDefaultValue(std::pair<std::string, std::string> (USERNAME, EMPTY_FIELD));              
    _config.addDefaultValue(std::pair<std::string, std::string> (PASSWORD, EMPTY_FIELD));                  
    _config.addDefaultValue(std::pair<std::string, std::string> (REALM, DEFAULT_REALM));
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, DFT_EXPIRE_VALUE)); 
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_RESOLVE_ONCE, FALSE_STR));        
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, EMPTY_FIELD));        
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_MAILBOX, EMPTY_FIELD));            
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, TRUE_STR));            
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_CREDENTIAL_NUMBER, "0"));            
    _config.addDefaultValue(std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, DEFAULT_ACCOUNT_TYPE));            
    _config.addDefaultValue(std::pair<std::string, std::string> (STUN_ENABLE, DFT_STUN_ENABLE));        
    _config.addDefaultValue(std::pair<std::string, std::string> (STUN_SERVER, DFT_STUN_SERVER)); 
                
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
    // _debug("ManagerImpl::getCurrentCodecName method called \n");
    AccountID accountid = getAccountFromCall (id);
    // _debug("ManagerImpl::getCurrentCodecName : %s \n",getAccountLink(accountid)->getCurrentCodecName().c_str());
    return getAccountLink (accountid)->getCurrentCodecName();
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
    //return ( IAX2_ENABLED ) ? true : false;
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
ManagerImpl::getMd5CredentialHashing(void)
{
    return getConfigBool(PREFERENCES, CONFIG_MD5HASH);
}

void 
ManagerImpl::setMd5CredentialHashing(bool enabled)
{
    if (enabled) {
        setConfig(PREFERENCES, CONFIG_MD5HASH, TRUE_STR);
    } else {
        setConfig(PREFERENCES, CONFIG_MD5HASH, FALSE_STR);
    }
}

int
ManagerImpl::getDialpad (void)
{
    if (getConfigString(PREFERENCES, CONFIG_DIALPAD) == TRUE_STR) {
        return 1;
    } else {
        return 0;
    }
}

void
ManagerImpl::setDialpad (void)
{
    (getConfigString(PREFERENCES, CONFIG_DIALPAD) == TRUE_STR) ? setConfig(PREFERENCES, CONFIG_DIALPAD, FALSE_STR) : setConfig (PREFERENCES, CONFIG_DIALPAD, TRUE_STR);
}

std::string ManagerImpl::getStunServer (void)
{
    return getConfigString (SIGNALISATION , STUN_SERVER);
}

void ManagerImpl::setStunServer (const std::string &server)
{
    setConfig (SIGNALISATION , STUN_SERVER, server);
}

int ManagerImpl::isStunEnabled (void)
{
    return getConfigString(SIGNALISATION, STUN_ENABLE) == TRUE_STR ? 1:0;
}

void ManagerImpl::enableStun (void)
{
    /* Update the config */
    (getConfigString(SIGNALISATION , STUN_ENABLE) == TRUE_STR) ? setConfig(SIGNALISATION , STUN_ENABLE , FALSE_STR) : setConfig (SIGNALISATION , STUN_ENABLE , TRUE_STR);

    /* Restart PJSIP */
    this->restartPJSIP ();
}


int
ManagerImpl::getVolumeControls (void)
{
    if (getConfigString(PREFERENCES , CONFIG_VOLUME) == TRUE_STR) {
        return 1;
    } else {
        return 0;
    }
}

void
ManagerImpl::setVolumeControls (void)
{
    (getConfigString(PREFERENCES, CONFIG_VOLUME) == TRUE_STR) ? setConfig(PREFERENCES , CONFIG_VOLUME , FALSE_STR) : setConfig (PREFERENCES , CONFIG_VOLUME , TRUE_STR);
}

void
ManagerImpl::setRecordingCall (const CallID& id)
{
    _debug ("ManagerImpl::setRecording()! \n");
    AccountID accountid = getAccountFromCall (id);

    getAccountLink (accountid)->setRecording (id);
}

bool
ManagerImpl::isRecording (const CallID& id)
{
    _debug ("ManagerImpl::isRecording()! \n");
    AccountID accountid = getAccountFromCall (id);

    return getAccountLink (accountid)->isRecording (id);
}

void
ManagerImpl::startHidden (void)
{
    (getConfigString(PREFERENCES, CONFIG_START) ==  START_HIDDEN) ? setConfig(PREFERENCES , CONFIG_START , FALSE_STR) : setConfig (PREFERENCES , CONFIG_START , TRUE_STR);
}

int
ManagerImpl::isStartHidden (void)
{
    return (getConfigBool(PREFERENCES, CONFIG_START) == true) ? 1:0;
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

int ManagerImpl::getHistoryEnabled (void)
{
    return getConfigInt (PREFERENCES, CONFIG_HISTORY_ENABLED);
}

void ManagerImpl::setHistoryEnabled (void)
{
    (getConfigInt (PREFERENCES, CONFIG_HISTORY_ENABLED) == 1) ? setConfig (PREFERENCES, CONFIG_HISTORY_ENABLED, FALSE_STR) : setConfig (PREFERENCES, CONFIG_HISTORY_ENABLED, TRUE_STR);
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
    return (getConfigBool(PREFERENCES, CONFIG_POPUP) == true) ? 1:0 ;
}

int32_t
ManagerImpl::getNotify (void)
{
    return (getConfigBool(PREFERENCES , CONFIG_NOTIFY) == true) ? 1:0;
}

void
ManagerImpl::setNotify (void)
{
    (getConfigString(PREFERENCES, CONFIG_NOTIFY) == NOTIFY_ALL) ?  setConfig (PREFERENCES, CONFIG_NOTIFY , FALSE_STR) : setConfig (PREFERENCES, CONFIG_NOTIFY , TRUE_STR);
}

int32_t
ManagerImpl::getMailNotify (void)
{
    return getConfigInt(PREFERENCES, CONFIG_MAIL_NOTIFY);
}

int32_t
ManagerImpl::getPulseAppVolumeControl (void)
{
    return getConfigInt (PREFERENCES , CONFIG_PA_VOLUME_CTRL);
}

void
ManagerImpl::setPulseAppVolumeControl (void)
{
    (getConfigInt (PREFERENCES , CONFIG_PA_VOLUME_CTRL) == 1) ? setConfig (PREFERENCES , CONFIG_PA_VOLUME_CTRL , FALSE_STR) : setConfig (PREFERENCES , CONFIG_PA_VOLUME_CTRL , TRUE_STR) ;
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
    } else if (getConfigInt (PREFERENCES , CONFIG_AUDIO) == PULSEAUDIO) {
        if (app_is_running ("pulseaudio") == 0) {
            _audiodriver = new PulseLayer (this);
        } else {
            _audiodriver = new AlsaLayer (this);
            setConfig (PREFERENCES, CONFIG_AUDIO, ALSA);
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

    alsaPlugin = getConfigString (AUDIO , ALSA_PLUGIN);

    numCardIn  = getConfigInt (AUDIO , ALSA_CARD_ID_IN);

    numCardOut = getConfigInt (AUDIO , ALSA_CARD_ID_OUT);

    _debug ("Deleting current layer... \n");

    //_audiodriver->closeLayer();
    delete _audiodriver;

    _audiodriver = NULL;

    switch (type) {

        case ALSA:
            _debug ("Creating Pulseaudio layer...\n");
            _audiodriver = new PulseLayer (this);
            break;

        case PULSEAUDIO:
            _debug ("Creating ALSA layer...\n");
            _audiodriver = new AlsaLayer (this);
            break;

        default:
            _debug ("Error: audio layer unknown\n");
    }

    _audiodriver->setErrorMessage (-1);

    _audiodriver->openDevice (numCardIn , numCardOut, samplerate, framesize, SFL_PCM_BOTH, alsaPlugin);

    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    _debug ("Current device: %i \n", type);

    _debug ("has current call: %i \n", hasCurrentCall());

    // need to stop audio streams if there is currently no call
    if ( (type != PULSEAUDIO) && (!hasCurrentCall())) {
        // _debug("There is currently a call!!\n");
        _audiodriver->stopStream();

    }
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

void ManagerImpl::setSipPort (int port)
{
    _debug("Setting to new port %d\n", port);
    int prevPort = getConfigInt (PREFERENCES , CONFIG_SIP_PORT);
    if(prevPort != port){
        setConfig(PREFERENCES, CONFIG_SIP_PORT, port);
        this->restartPJSIP ();
    }
}

int ManagerImpl::getSipPort (void)
{
    return getConfigInt (PREFERENCES , CONFIG_SIP_PORT);
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
    _debug("ManagerImp::setConfig %s %s %s\n", section.c_str(), name.c_str(), value.c_str());
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
    _debug ("Set accounts order : %s\n", order.c_str());
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
            if (iter->second != NULL) {
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
                if (iter->second != 0) {
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
    if(account == NULL) {
        _debug("Cannot getAccountDetails on a non-existing accountID. Defaults will be used.\n");
    }
    
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, getConfigString(accountID, CONFIG_ACCOUNT_ALIAS)));
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, getConfigString(accountID, CONFIG_ACCOUNT_ENABLE)));
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_RESOLVE_ONCE, getConfigString(accountID, CONFIG_ACCOUNT_RESOLVE_ONCE)));
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, getConfigString(accountID, CONFIG_ACCOUNT_TYPE)));    
    a.insert(std::pair<std::string, std::string> (HOSTNAME, getConfigString(accountID, HOSTNAME)));
    a.insert(std::pair<std::string, std::string> (USERNAME, getConfigString(accountID, USERNAME)));
    a.insert(std::pair<std::string, std::string> (PASSWORD, getConfigString(accountID, PASSWORD)));        
    a.insert(std::pair<std::string, std::string> (REALM, getConfigString(accountID, REALM)));
    a.insert(std::pair<std::string, std::string> (AUTHENTICATION_USERNAME, getConfigString(accountID, AUTHENTICATION_USERNAME)));
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_MAILBOX, getConfigString(accountID, CONFIG_ACCOUNT_MAILBOX)));
    a.insert(std::pair<std::string, std::string> (CONFIG_ACCOUNT_REGISTRATION_EXPIRE, getConfigString(accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE)));
    a.insert(std::pair<std::string, std::string> (LOCAL_ADDRESS, getConfigString(accountID, LOCAL_ADDRESS)));
    a.insert(std::pair<std::string, std::string> (PUBLISHED_ADDRESS, getConfigString(accountID, PUBLISHED_ADDRESS)));
    a.insert(std::pair<std::string, std::string> (LOCAL_PORT, getConfigString(accountID, LOCAL_PORT)));
    a.insert(std::pair<std::string, std::string> (PUBLISHED_PORT, getConfigString(accountID, PUBLISHED_PORT)));
    a.insert(std::pair<std::string, std::string> (DISPLAY_NAME, getConfigString(accountID, DISPLAY_NAME)));
    a.insert(std::pair<std::string, std::string> (STUN_ENABLE, getConfigString(accountID, STUN_ENABLE)));
    a.insert(std::pair<std::string, std::string> (STUN_SERVER, getConfigString(accountID, STUN_SERVER)));                        
    
    RegistrationState state; 
    if (account != NULL) {
        state = account->getRegistrationState();           
    } else {
        state = Unregistered;
    }
    a.insert(std::pair<std::string, std::string> ("Status", mapStateNumberToString (state)));
    a.insert(std::pair<std::string, std::string> (SRTP_KEY_EXCHANGE, getConfigString(accountID, SRTP_KEY_EXCHANGE)));
    a.insert(std::pair<std::string, std::string> (SRTP_ENABLE, getConfigString(accountID, SRTP_ENABLE)));    
    a.insert(std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS, getConfigString(accountID, ZRTP_DISPLAY_SAS)));
    a.insert(std::pair<std::string, std::string> (ZRTP_DISPLAY_SAS_ONCE, getConfigString(accountID, ZRTP_DISPLAY_SAS_ONCE)));            
    a.insert(std::pair<std::string, std::string> (ZRTP_HELLO_HASH, getConfigString(accountID, ZRTP_HELLO_HASH)));    
    a.insert(std::pair<std::string, std::string> (ZRTP_NOT_SUPP_WARNING, getConfigString(accountID, ZRTP_NOT_SUPP_WARNING)));    

    a.insert(std::pair<std::string, std::string> (TLS_ENABLE, Manager::instance().getConfigString(accountID, TLS_ENABLE)));    
    a.insert(std::pair<std::string, std::string> (TLS_CA_LIST_FILE, Manager::instance().getConfigString(accountID, TLS_CA_LIST_FILE)));
    a.insert(std::pair<std::string, std::string> (TLS_CERTIFICATE_FILE, Manager::instance().getConfigString(accountID, TLS_CERTIFICATE_FILE)));
    a.insert(std::pair<std::string, std::string> (TLS_PRIVATE_KEY_FILE, Manager::instance().getConfigString(accountID, TLS_PRIVATE_KEY_FILE)));
    a.insert(std::pair<std::string, std::string> (TLS_PASSWORD, Manager::instance().getConfigString(accountID, TLS_PASSWORD)));
    a.insert(std::pair<std::string, std::string> (TLS_METHOD, Manager::instance().getConfigString(accountID, TLS_METHOD)));
    a.insert(std::pair<std::string, std::string> (TLS_CIPHERS, Manager::instance().getConfigString(accountID, TLS_CIPHERS)));
    a.insert(std::pair<std::string, std::string> (TLS_SERVER_NAME, Manager::instance().getConfigString(accountID, TLS_SERVER_NAME)));
    a.insert(std::pair<std::string, std::string> (TLS_VERIFY_SERVER, Manager::instance().getConfigString(accountID, TLS_VERIFY_SERVER)));    
    a.insert(std::pair<std::string, std::string> (TLS_VERIFY_CLIENT, Manager::instance().getConfigString(accountID, TLS_VERIFY_CLIENT)));    
    a.insert(std::pair<std::string, std::string> (TLS_REQUIRE_CLIENT_CERTIFICATE, Manager::instance().getConfigString(accountID, TLS_REQUIRE_CLIENT_CERTIFICATE)));    
    a.insert(std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_SEC, Manager::instance().getConfigString(accountID, TLS_NEGOTIATION_TIMEOUT_SEC)));    
    a.insert(std::pair<std::string, std::string> (TLS_NEGOTIATION_TIMEOUT_MSEC, Manager::instance().getConfigString(accountID, TLS_NEGOTIATION_TIMEOUT_MSEC)));   
            
    return a;
}

/* Transform digest to string.
 * output must be at least PJSIP_MD5STRLEN+1 bytes.
 * Helper function taken from sip_auth_client.c in 
 * pjproject-1.0.3.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */

void ManagerImpl::digest2str(const unsigned char digest[], char *output)
{
    int i;
    for (i = 0; i<16; ++i) {
        pj_val_to_hex_digit(digest[i], output);
        output += 2;
    }    
}

std::string  ManagerImpl::computeMd5HashFromCredential(const std::string& username, const std::string& password, const std::string& realm)
{
    pj_md5_context pms;
    unsigned char digest[16];
    char ha1[PJSIP_MD5STRLEN];

    pj_str_t usernamePjFormat = pj_str(strdup(username.c_str()));
    pj_str_t passwordPjFormat = pj_str(strdup(password.c_str()));
    pj_str_t realmPjFormat = pj_str(strdup(realm.c_str()));

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init(&pms);
    MD5_APPEND( &pms, usernamePjFormat.ptr, usernamePjFormat.slen);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, realmPjFormat.ptr, realmPjFormat.slen);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, passwordPjFormat.ptr, passwordPjFormat.slen);
    pj_md5_final(&pms, digest);

    digest2str(digest, ha1);

    char ha1_null_terminated[PJSIP_MD5STRLEN+1];
    memcpy(ha1_null_terminated, ha1, sizeof(char)*PJSIP_MD5STRLEN);
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
    
    std::string section = "Credential" + std::string(":") + accountID + std::string(":") + credentialIndex;
    
    _debug("Setting credential in section %s\n", section.c_str());
    
    it = credentialInformation.find(USERNAME);
    std::string username;
    if (it == credentialInformation.end()) { 
        username = EMPTY_FIELD;
    } else {
        username = it->second;
    }
    Manager::instance().setConfig (section, USERNAME, username);

    it = credentialInformation.find(REALM);
    std::string realm;
    if (it == credentialInformation.end()) { 
        realm = EMPTY_FIELD;
    } else {
        realm = it->second;
    }
    Manager::instance().setConfig (section, REALM, realm);

    
    it = credentialInformation.find(PASSWORD);
    std::string password;
    if (it == credentialInformation.end()) { 
        password = EMPTY_FIELD;
    } else {
        password = it->second;
    }
    
    if(getMd5CredentialHashing()) {
        // TODO: Fix this.
        // This is an extremly weak test in order to check 
        // if the password is a hashed value. This is done
        // because deleteCredential() is called before this 
        // method. Therefore, we cannot check if the value 
        // is different from the one previously stored in 
        // the configuration file.
         
        if(password.length() != 32) {
            password = computeMd5HashFromCredential(username, password, realm);
        }
    } 
        
    Manager::instance().setConfig (section, PASSWORD, password);
}

//TODO: tidy this up. Make a macro or inline 
// method to reduce the if/else mess.

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

    if((iter = map_cpy.find(AUTHENTICATION_USERNAME)) != map_cpy.end()) { authenticationName = iter->second; }
    if((iter = map_cpy.find(USERNAME)) != map_cpy.end()) { username = iter->second; }
    if((iter = map_cpy.find(PASSWORD)) != map_cpy.end()) { password = iter->second; }
    if((iter = map_cpy.find(REALM)) != map_cpy.end()) { realm = iter->second; }
																																																															
    if(!getMd5CredentialHashing()) {
        setConfig(accountID, REALM, realm);
        setConfig(accountID, USERNAME, username);
        setConfig(accountID, PASSWORD, password);    
        setConfig(accountID, AUTHENTICATION_USERNAME, authenticationName);
    } else {
        // Make sure not to re-hash the password field if
        // it is already saved as a MD5 Hash.
        // TODO: This test is weak. Fix this.
        if ((password.compare(getConfigString(accountID, PASSWORD)) != 0) && (password.length() != 32)) {
            _debug("Password sent and password from config are different. Re-hashing\n");
            std::string hash;
            if(authenticationName.empty()) {
                hash = computeMd5HashFromCredential(username, password, realm);
            } else {
                hash = computeMd5HashFromCredential(authenticationName, password, realm);
            }
            setConfig(accountID, PASSWORD, hash);
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
 
    if((iter = map_cpy.find(HOSTNAME)) != map_cpy.end()) { hostname = iter->second; }
    if((iter = map_cpy.find(DISPLAY_NAME)) != map_cpy.end()) { displayName = iter->second; } 
    if((iter = map_cpy.find(LOCAL_ADDRESS)) != map_cpy.end()) { localAddress = iter->second; }           
    if((iter = map_cpy.find(PUBLISHED_ADDRESS)) != map_cpy.end()) { publishedAddress = iter->second; }        
    if((iter = map_cpy.find(LOCAL_PORT)) != map_cpy.end()) { localPort = iter->second; }
    if((iter = map_cpy.find(PUBLISHED_PORT)) != map_cpy.end()) { publishedPort = iter->second; } 
    if((iter = map_cpy.find(STUN_ENABLE)) != map_cpy.end()) { stunEnable = iter->second; } 
    if((iter = map_cpy.find(STUN_SERVER)) != map_cpy.end()) { stunServer = iter->second; }                           
    if((iter = map_cpy.find(SRTP_ENABLE)) != map_cpy.end()) { srtpEnable = iter->second; }
    if((iter = map_cpy.find(ZRTP_DISPLAY_SAS)) != map_cpy.end()) { zrtpDisplaySas = iter->second; }
    if((iter = map_cpy.find(ZRTP_DISPLAY_SAS_ONCE)) != map_cpy.end()) { zrtpDisplaySasOnce = iter->second; }
    if((iter = map_cpy.find(ZRTP_NOT_SUPP_WARNING)) != map_cpy.end()) { zrtpNotSuppWarning = iter->second; }    
    if((iter = map_cpy.find(ZRTP_HELLO_HASH)) != map_cpy.end()) { zrtpHelloHash = iter->second; }    
    if((iter = map_cpy.find(SRTP_KEY_EXCHANGE)) != map_cpy.end()) { srtpKeyExchange = iter->second; }           
 
    if((iter = map_cpy.find(CONFIG_ACCOUNT_ALIAS)) != map_cpy.end()) { alias = iter->second; }
    if((iter = map_cpy.find(CONFIG_ACCOUNT_MAILBOX)) != map_cpy.end()) { mailbox = iter->second; }
    if((iter = map_cpy.find(CONFIG_ACCOUNT_ENABLE)) != map_cpy.end()) { accountEnable = iter->second; }
    if((iter = map_cpy.find(CONFIG_ACCOUNT_TYPE)) != map_cpy.end()) { type = iter->second; }
    if((iter = map_cpy.find(CONFIG_ACCOUNT_RESOLVE_ONCE)) != map_cpy.end()) { resolveOnce = iter->second; }
    if((iter = map_cpy.find(CONFIG_ACCOUNT_REGISTRATION_EXPIRE)) != map_cpy.end()) { registrationExpire = iter->second; }

    if((iter = map_cpy.find(TLS_ENABLE)) != map_cpy.end()) { tlsEnable = iter->second; }
    if((iter = map_cpy.find(TLS_CA_LIST_FILE)) != map_cpy.end()) { tlsCaListFile = iter->second; }
    if((iter = map_cpy.find(TLS_CERTIFICATE_FILE)) != map_cpy.end()) { tlsCertificateFile = iter->second; }
    if((iter = map_cpy.find(TLS_PRIVATE_KEY_FILE)) != map_cpy.end()) { tlsPrivateKeyFile = iter->second; }
    if((iter = map_cpy.find(TLS_PASSWORD)) != map_cpy.end()) { tlsPassword = iter->second; }
    if((iter = map_cpy.find(TLS_METHOD)) != map_cpy.end()) { tlsMethod = iter->second; }
    if((iter = map_cpy.find(TLS_CIPHERS)) != map_cpy.end()) { tlsCiphers = iter->second; }
    if((iter = map_cpy.find(TLS_SERVER_NAME)) != map_cpy.end()) { tlsServerName = iter->second; }
    if((iter = map_cpy.find(TLS_VERIFY_SERVER)) != map_cpy.end()) { tlsVerifyServer = iter->second; }
    if((iter = map_cpy.find(TLS_VERIFY_CLIENT)) != map_cpy.end()) { tlsVerifyClient = iter->second; }                
    if((iter = map_cpy.find(TLS_REQUIRE_CLIENT_CERTIFICATE)) != map_cpy.end()) { tlsRequireClientCertificate = iter->second; }                 
    if((iter = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_SEC)) != map_cpy.end()) { tlsNegotiationTimeoutSec = iter->second; }                          
    if((iter = map_cpy.find(TLS_NEGOTIATION_TIMEOUT_MSEC)) != map_cpy.end()) { tlsNegotiationTimeoutMsec = iter->second; }      
    
    _debug("Enable account %s\n", accountEnable.c_str());        																									
    setConfig(accountID, HOSTNAME, hostname);
    setConfig(accountID, LOCAL_ADDRESS, localAddress);    
    setConfig(accountID, PUBLISHED_ADDRESS, publishedAddress);            
    setConfig(accountID, LOCAL_PORT, localPort);    
    setConfig(accountID, PUBLISHED_PORT, publishedPort);
    setConfig(accountID, DISPLAY_NAME, displayName);                
    setConfig(accountID, SRTP_ENABLE, srtpEnable);
    setConfig(accountID, ZRTP_DISPLAY_SAS, zrtpDisplaySas);
    setConfig(accountID, ZRTP_DISPLAY_SAS_ONCE, zrtpDisplaySasOnce);        
    setConfig(accountID, ZRTP_NOT_SUPP_WARNING, zrtpNotSuppWarning);   
    setConfig(accountID, ZRTP_HELLO_HASH, zrtpHelloHash);    
    setConfig(accountID, SRTP_KEY_EXCHANGE, srtpKeyExchange);											
    
    setConfig(accountID, TLS_ENABLE, tlsEnable);   
    setConfig(accountID, TLS_CA_LIST_FILE, tlsCaListFile);    
    setConfig(accountID, TLS_CERTIFICATE_FILE, tlsCertificateFile);    
    setConfig(accountID, TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile);    
    setConfig(accountID, TLS_PASSWORD, tlsPassword);    
    setConfig(accountID, TLS_METHOD, tlsMethod);    
    setConfig(accountID, TLS_CIPHERS, tlsCiphers);    
    setConfig(accountID, TLS_SERVER_NAME, tlsServerName);    
    setConfig(accountID, TLS_VERIFY_SERVER, tlsVerifyServer);    
    setConfig(accountID, TLS_VERIFY_CLIENT, tlsVerifyClient);    
    setConfig(accountID, TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate);    
    setConfig(accountID, TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeoutSec);     
    setConfig(accountID, TLS_NEGOTIATION_TIMEOUT_MSEC, tlsNegotiationTimeoutMsec);      
    
    setConfig(accountID, CONFIG_ACCOUNT_ALIAS, alias);
    setConfig(accountID, CONFIG_ACCOUNT_MAILBOX, mailbox);           
    setConfig(accountID, CONFIG_ACCOUNT_ENABLE, accountEnable);
    setConfig(accountID, CONFIG_ACCOUNT_TYPE, type);														
    setConfig(accountID, CONFIG_ACCOUNT_RESOLVE_ONCE, resolveOnce);
    setConfig(accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire);
																					
    saveConfig();

    Account * acc = NULL;
    acc = getAccount (accountID);
    if (acc != NULL) {
        acc->loadConfig();
        
        if (acc->isEnabled()) {
            acc->unregisterVoIPLink();
            acc->registerVoIPLink();
        } else {
            acc->unregisterVoIPLink();
        }
    } else {
        _debug("ManagerImpl::setAccountDetails: account is NULL\n");
    }
    
    // Update account details to the client side
    if (_dbus) _dbus->getConfigurationManager()->accountsChanged();
    
}

void
ManagerImpl::sendRegister (const std::string& accountID , const int32_t& enable)
{

    _debug ("ManagerImpl::sendRegister \n");
    
    // Update the active field
    setConfig (accountID, CONFIG_ACCOUNT_ENABLE, (enable == 1) ? TRUE_STR:FALSE_STR);
    _debug ("ManagerImpl::sendRegister set config done\n");

    Account* acc = getAccount (accountID);
    acc->loadConfig();
    _debug ("ManagerImpl::sendRegister acc->loadconfig done\n");

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
ManagerImpl::deleteAllCredential(const AccountID& accountID) 
{
    int numberOfCredential = getConfigInt (accountID, CONFIG_CREDENTIAL_NUMBER);
     
    int i;
    for(i = 0; i < numberOfCredential; i++) {   
        std::string credentialIndex;
        std::stringstream streamOut;
        streamOut << i;
        credentialIndex = streamOut.str();
        std::string section = "Credential" + std::string(":") + accountID + std::string(":") + credentialIndex;
        
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
    Account* remAccount = getAccount (accountID);

    if (remAccount) {
        remAccount->unregisterVoIPLink();
        _accountMap.erase (accountID);
        delete remAccount;
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

    short nbAccount = 0;
    TokenList sections = _config.getSections();
    std::string accountType;
    Account *tmpAccount = 0;
    std::vector <std::string> account_order;

    TokenList::iterator iter = sections.begin();

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

    // Those calls that are placed to an uri that cannot be 
    // associated to an account are using that special account.
    // An account, that is not account, in the sense of 
    // registration. This is useful since the Account object 
    // provides a handful of method that simplifies URI creation
    // and loading of various settings.
    _directIpAccount = AccountCreator::createAccount (AccountCreator::SIP_DIRECT_IP_ACCOUNT, "");
    if (_directIpAccount == NULL) {
        _debug("Failed to create direct ip calls \"account\"\n");
    } else {
        // Force the options to be loaded
        // No registration in the sense of 
        // the REGISTER method is performed.
        _directIpAccount->registerVoIPLink(); 
    }
          
    _debug ("nbAccount loaded %i \n",nbAccount);

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
    SIPVoIPLink *siplink;
    siplink = dynamic_cast<SIPVoIPLink*> (getSIPAccountLink ());

    this->unregisterCurSIPAccounts();
    /* Terminate and initialize the PJSIP library */

    if (siplink) {
        siplink->terminate ();
        siplink = SIPVoIPLink::instance ("");
        siplink->init ();
    }

    /* Then register all enabled SIP accounts */
    this->registerCurSIPAccounts (siplink);
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
    AccountMap::iterator iter;

    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = iter->second;

        if (account->getType() == "sip") {
            return account->getVoIPLink();
        }
    }

    return NULL;
}




pjsip_regc
*getSipRegcFromID (const AccountID& id UNUSED)
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

void ManagerImpl::registerCurSIPAccounts (VoIPLink *link)
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

    if (to.find(SIP_SCHEME) == 0 || to.find(SIPS_SCHEME) == 0) {
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
