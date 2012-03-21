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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "managerimpl.h"

#include "account.h"
#include "dbus/callmanager.h"
#include "global.h"
#include "sip/sipaccount.h"
#include "im/instant_messaging.h"
#include "iax/iaxaccount.h"
#include "numbercleaner.h"
#include "config/yamlparser.h"
#include "config/yamlemitter.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"
#include "audio/sound/dtmf.h"
#include "sip/sipvoiplink.h"
#include "iax/iaxvoiplink.h"
#include "manager.h"

#include "dbus/configurationmanager.h"

#include "conference.h"

#include <cerrno>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <tr1/functional>
#include <iterator>
#include <fstream>
#include <sstream>
#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>  // mkdir(2)

ManagerImpl::ManagerImpl() :
    preferences(), voipPreferences(), addressbookPreference(),
    hookPreference(),  audioPreference(), shortcutPreferences(),
    hasTriedToRegister_(false), audioCodecFactory(), dbus_(), config_(),
    currentCallId_(), currentCallMutex_(), audiodriver_(0), dtmfKey_(0),
    toneMutex_(), telephoneTone_(0), audiofile_(0), audioLayerMutex_(),
    waitingCall_(), waitingCallMutex_(), nbIncomingWaitingCall_(0), path_(),
    callAccountMap_(), callAccountMapMutex_(), IPToIPMap_(), accountMap_(),
    mainBuffer_(), conferenceMap_(), history_()
{
    // initialize random generator for call id
    srand(time(NULL));
}

// never call if we use only the singleton...
ManagerImpl::~ManagerImpl()
{}

void ManagerImpl::init(std::string config_file)
{
    path_ = config_file.empty() ? createConfigFile() : config_file;
    DEBUG("Manager: configuration file path: %s", path_.c_str());

    try {
        Conf::YamlParser parser(path_.c_str());
        parser.serializeEvents();
        parser.composeEvents();
        parser.constructNativeData();
        loadAccountMap(parser);
    } catch (const Conf::YamlParserException &e) {
        ERROR("Manager: %s", e.what());
        fflush(stderr);
        loadDefaultAccountMap();
    }

    initAudioDriver();

    {
        ost::MutexLock lock(audioLayerMutex_);
        if (audiodriver_) {
            telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), audiodriver_->getSampleRate()));
            dtmfKey_.reset(new DTMF(8000));
        }
    }

    history_.load(preferences.getHistoryLimit());
    registerAccounts();
}

void ManagerImpl::terminate()
{
    std::vector<std::string> callList(getCallList());
    DEBUG("Manager: Hangup %zu remaining call", callList.size());

    for (std::vector<std::string>::iterator iter = callList.begin(); iter != callList.end(); ++iter)
        hangupCall(*iter);

    saveConfig();

    unloadAccountMap();

    delete SIPVoIPLink::instance();

    ost::MutexLock lock(audioLayerMutex_);

    delete audiodriver_;
    audiodriver_ = NULL;
}

bool ManagerImpl::isCurrentCall(const std::string& callId) const
{
    return currentCallId_ == callId;
}

bool ManagerImpl::hasCurrentCall() const
{
    return not currentCallId_.empty();
}

std::string
ManagerImpl::getCurrentCallId() const
{
    return currentCallId_;
}

void ManagerImpl::switchCall(const std::string& id)
{
    ost::MutexLock m(currentCallMutex_);
    DEBUG("----- Switch current call id to %s -----", id.c_str());
    currentCallId_ = id;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool ManagerImpl::outgoingCall(const std::string& account_id,
                               const std::string& call_id,
                               const std::string& to,
                               const std::string& conf_id)
{
    if (call_id.empty()) {
        DEBUG("Manager: New outgoing call abort, missing callid");
        return false;
    }

    // Call ID must be unique
    if (not getAccountFromCall(call_id).empty()) {
        ERROR("Manager: Error: Call id already exists in outgoing call");
        return false;
    }

    DEBUG("Manager: New outgoing call %s to %s", call_id.c_str(), to.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    std::string prefix;
    if (hookPreference.getNumberEnabled())
        prefix = hookPreference.getNumberAddPrefix();

    std::string to_cleaned(NumberCleaner::clean(to, prefix));

    static const char * const SIP_SCHEME = "sip:";
    static const char * const SIPS_SCHEME = "sips:";

    bool IPToIP = to_cleaned.find(SIP_SCHEME) == 0 or
                  to_cleaned.find(SIPS_SCHEME) == 0;

    setIPToIPForCall(call_id, IPToIP);

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        DEBUG("Manager: Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conferenceand this and is not a conference participant
        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id))
            onHoldCall(current_call_id);
        else if (isConference(current_call_id) and not isConferenceParticipant(call_id))
            detachParticipant(Call::DEFAULT_ID, current_call_id);
    }

    if (IPToIP) {
        DEBUG("Manager: Start IP2IP call");

        /* We need to retrieve the sip voiplink instance */
        if (SIPVoIPLink::instance()->SIPNewIpToIpCall(call_id, to_cleaned)) {
            switchCall(call_id);
            return true;
        } else
            callFailure(call_id);

        return false;
    }

    DEBUG("Manager: Selecting account %s", account_id.c_str());

    // Is this account exist
    if (!accountExists(account_id)) {
        ERROR("Manager: Error: Account doesn't exist in new outgoing call");
        return false;
    }

    if (!associateCallToAccount(call_id, account_id))
        WARN("Manager: Warning: Could not associate call id %s to account id %s", call_id.c_str(), account_id.c_str());

    try {
        Call *call = getAccountLink(account_id)->newOutgoingCall(call_id, to_cleaned);

        switchCall(call_id);
        call->setConfId(conf_id);
    } catch (const VoipLinkException &e) {
        callFailure(call_id);
        ERROR("Manager: %s", e.what());
        return false;
    }

    getMainBuffer()->stateInfo();

    return true;
}

//THREAD=Main : for outgoing Call
bool ManagerImpl::answerCall(const std::string& call_id)
{
    DEBUG("Manager: Answer call %s", call_id.c_str());

    // If sflphone is ringing
    stopTone();

    // store the current call id
    std::string current_call_id(getCurrentCallId());

    // Retreive call coresponding to this id
    std::string account_id = getAccountFromCall(call_id);
    Call *call = getAccountLink(account_id)->getCall(call_id);

    if (call == NULL) {
        ERROR("Manager: Error: Call is null");
    }

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        DEBUG("Manager: Currently conversing with %s", current_call_id.c_str());

        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id)) {
            DEBUG("Manager: Answer call: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall(current_call_id);
        } else if (isConference(current_call_id) and not isConferenceParticipant(call_id)) {
            // if we are talking to a conference and we are answering an incoming call
            DEBUG("Manager: Detach main participant from conference");
            detachParticipant(Call::DEFAULT_ID, current_call_id);
        }
    }

    try {
        getAccountLink(account_id)->answer(call);
    } catch (const std::runtime_error &e) {
        ERROR("Manager: Error: %s", e.what());
    }

    // if it was waiting, it's waiting no more
    removeWaitingCall(call_id);

    // if we dragged this call into a conference already
    if (isConferenceParticipant(call_id))
        switchCall(call->getConfId());
    else
        switchCall(call_id);

    // Connect streams
    addStream(call_id);

    getMainBuffer()->stateInfo();

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        setRecordingCall(call_id);

    // update call state on client side
    if (audioPreference.getIsAlwaysRecording())
        dbus_.getCallManager()->callStateChanged(call_id, "RECORD");
    else
        dbus_.getCallManager()->callStateChanged(call_id, "CURRENT");

    return true;
}

//THREAD=Main
void ManagerImpl::hangupCall(const std::string& callId)
{
    DEBUG("Manager: Hangup call %s", callId.c_str());

    // store the current call id
    std::string currentCallId(getCurrentCallId());

    stopTone();

    /* Broadcast a signal over DBus */
    DEBUG("Manager: Send DBUS call state change (HUNGUP) for id %s", callId.c_str());
    dbus_.getCallManager()->callStateChanged(callId, "HUNGUP");

    if (not isValidCall(callId) and not isIPToIP(callId)) {
        ERROR("Manager: Error: Could not hang up call, call not valid");
        return;
    }

    // Disconnect streams
    removeStream(callId);

    if (isConferenceParticipant(callId)) {
        Conference *conf = getConferenceFromCallID(callId);

        if (conf != NULL) {
            // remove this participant
            removeParticipant(callId);
            processRemainingParticipants(currentCallId, conf);
        }
    } else {
        // we are not participating in a conference, current call switched to ""
        if (not isConference(currentCallId))
            switchCall("");
    }

    if (isIPToIP(callId)) {
        /* Direct IP to IP call */
        try {
            Call * call = SIPVoIPLink::instance()->getCall(callId);
            history_.addCall(call, preferences.getHistoryLimit());
            SIPVoIPLink::instance()->hangup(callId);
        } catch (const VoipLinkException &e) {
            ERROR("%s", e.what());
        }
    } else {
        std::string accountId(getAccountFromCall(callId));
        VoIPLink *link = getAccountLink(accountId);
        Call * call = link->getCall(callId);
        history_.addCall(call, preferences.getHistoryLimit());
        link->hangup(callId);
        removeCallAccount(callId);
    }

    getMainBuffer()->stateInfo();
}

bool ManagerImpl::hangupConference(const std::string& id)
{
    DEBUG("Manager: Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end()) {
        Conference *conf = iter_conf->second;

        if (conf) {
            ParticipantSet participants(conf->getParticipantList());

            for (ParticipantSet::const_iterator iter = participants.begin();
                    iter != participants.end(); ++iter)
                hangupCall(*iter);
        } else {
            ERROR("Manager: No such conference %s", id.c_str());
            return false;
        }
    }

    switchCall("");

    getMainBuffer()->stateInfo();

    return true;
}


//THREAD=Main
void ManagerImpl::onHoldCall(const std::string& callId)
{
    DEBUG("Manager: Put call %s on hold", callId.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    try {
        if (isIPToIP(callId)) {
            SIPVoIPLink::instance()-> onhold(callId);
        } else {
            /* Classic call, attached to an account */
            std::string account_id(getAccountFromCall(callId));

            if (account_id.empty()) {
                DEBUG("Manager: Account ID %s or callid %s doesn't exists in call onHold", account_id.c_str(), callId.c_str());
                return;
            }

            getAccountLink(account_id)->onhold(callId);
        }
    } catch (const VoipLinkException &e) {
        ERROR("Manager: Error: %s", e.what());
    }

    // Unbind calls in main buffer
    removeStream(callId);

    // Remove call from teh queue if it was still there
    removeWaitingCall(callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId)
        switchCall("");

    dbus_.getCallManager()->callStateChanged(callId, "HOLD");

    getMainBuffer()->stateInfo();
}

//THREAD=Main
void ManagerImpl::offHoldCall(const std::string& callId)
{
    std::string accountId;
    std::string codecName;

    DEBUG("Manager: Put call %s off hold", callId.c_str());

    stopTone();

    std::string currentCallId(getCurrentCallId());

    //Place current call on hold if it isn't

    if (hasCurrentCall()) {

        if (not isConference(currentCallId) and not isConferenceParticipant(currentCallId)) {
            DEBUG("Manager: Has current call (%s), put on hold", currentCallId.c_str());
            onHoldCall(currentCallId);
        } else if (isConference(currentCallId) and not isConferenceParticipant(callId))
            detachParticipant(Call::DEFAULT_ID, currentCallId);
    }

    bool isRec = false;

    if (isIPToIP(callId))
        SIPVoIPLink::instance()->offhold(callId);
    else {
        /* Classic call, attached to an account */
        accountId = getAccountFromCall(callId);

        DEBUG("Manager: Setting offhold, Account %s, callid %s", accountId.c_str(), callId.c_str());

        Call * call = getAccountLink(accountId)->getCall(callId);

        if (call) {
            isRec = call->isRecording();
            getAccountLink(accountId)->offhold(callId);
        }
    }

    dbus_.getCallManager()->callStateChanged(callId, isRec ? "UNHOLD_RECORD" : "UNHOLD_CURRENT");

    if (isConferenceParticipant(callId)) {
        std::string currentAccountId(getAccountFromCall(callId));
        Call *call = getAccountLink(currentAccountId)->getCall(callId);

        if (call)
            switchCall(call->getConfId());

    } else
        switchCall(callId);

    addStream(callId);

    getMainBuffer()->stateInfo();
}

//THREAD=Main
bool ManagerImpl::transferCall(const std::string& callId, const std::string& to)
{
    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
        Conference *conf = getConferenceFromCallID(callId);
        processRemainingParticipants(callId, conf);
    } else if (not isConference(getCurrentCallId()))
        switchCall("");

    // Direct IP to IP call
    if (isIPToIP(callId)) {
        SIPVoIPLink::instance()->transfer(callId, to);
    } else {
        std::string accountID(getAccountFromCall(callId));

        if (accountID.empty())
            return false;

        VoIPLink *link = getAccountLink(accountID);
        link->transfer(callId, to);
    }

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall(callId);

    getMainBuffer()->stateInfo();

    return true;
}

void ManagerImpl::transferFailed()
{
    dbus_.getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceeded()
{
    dbus_.getCallManager()->transferSucceeded();
}

bool ManagerImpl::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    if (isIPToIP(transferID))
        return SIPVoIPLink::instance()->attendedTransfer(transferID, targetID);

    // Classic call, attached to an account
    std::string accountid(getAccountFromCall(transferID));

    if (accountid.empty())
        return false;

    return getAccountLink(accountid)->attendedTransfer(transferID, targetID);
}

//THREAD=Main : Call:Incoming
void ManagerImpl::refuseCall(const std::string& id)
{
    stopTone();

    if (getCallList().size() <= 1) {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    /* Direct IP to IP call */

    if (isIPToIP(id))
        SIPVoIPLink::instance()->refuse(id);
    else {
        /* Classic call, attached to an account */
        std::string accountid = getAccountFromCall(id);

        if (accountid.empty())
            return;

        getAccountLink(accountid)->refuse(id);

        removeCallAccount(id);
    }

    removeWaitingCall(id);
    dbus_.getCallManager()->callStateChanged(id, "HUNGUP");

    // Disconnect streams
    removeStream(id);

    getMainBuffer()->stateInfo();
}

Conference*
ManagerImpl::createConference(const std::string& id1, const std::string& id2)
{
    DEBUG("Manager: Create conference with call %s and %s", id1.c_str(), id2.c_str());

    Conference* conf = new Conference;

    conf->add(id1);
    conf->add(id2);

    // Add conference to map
    conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));

    // broadcast a signal over dbus
    dbus_.getCallManager()->conferenceCreated(conf->getConfID());

    return conf;
}

void ManagerImpl::removeConference(const std::string& conference_id)
{
    DEBUG("Manager: Remove conference %s", conference_id.c_str());
    DEBUG("Manager: number of participants: %u", conferenceMap_.size());
    ConferenceMap::iterator iter = conferenceMap_.find(conference_id);

    Conference* conf = 0;

    if (iter != conferenceMap_.end())
        conf = iter->second;

    if (conf == NULL) {
        ERROR("Manager: Error: Conference not found");
        return;
    }

    // broadcast a signal over dbus
    dbus_.getCallManager()->conferenceRemoved(conference_id);

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getMainBuffer()->unBindAll(Call::DEFAULT_ID);

    ParticipantSet participants(conf->getParticipantList());

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getMainBuffer()->bindCallID(*iter_p, Call::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (conferenceMap_.erase(conference_id) == 1)
        DEBUG("Manager: Conference %s removed successfully", conference_id.c_str());
    else
        ERROR("Manager: Error: Cannot remove conference: %s", conference_id.c_str());

    delete conf;
}

Conference*
ManagerImpl::getConferenceFromCallID(const std::string& call_id)
{
    std::string account_id(getAccountFromCall(call_id));
    Call *call = getAccountLink(account_id)->getCall(call_id);

    ConferenceMap::const_iterator iter(conferenceMap_.find(call->getConfId()));

    if (iter != conferenceMap_.end())
        return iter->second;
    else
        return NULL;
}

void ManagerImpl::holdConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf == conferenceMap_.end())
        return;

    Conference *conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                 conf->getState() == Conference::ACTIVE_DETACHED_REC or
                 conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (ParticipantSet::const_iterator iter = participants.begin();
            iter != participants.end(); ++iter) {
        switchCall(*iter);
        onHoldCall(*iter);
    }

    conf->setState(isRec ? Conference::HOLD_REC : Conference::HOLD);
    dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
}

void ManagerImpl::unHoldConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end() and iter_conf->second) {
        Conference *conf = iter_conf->second;

        bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                     conf->getState() == Conference::ACTIVE_DETACHED_REC or
                     conf->getState() == Conference::HOLD_REC;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter = participants.begin(); iter!= participants.end(); ++iter) {
            Call *call = getAccountLink(getAccountFromCall(*iter))->getCall(*iter);

            // if one call is currently recording, the conference is in state recording
            isRec |= call->isRecording();

            offHoldCall(*iter);
        }

        conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);
        dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
    }
}

bool ManagerImpl::isConference(const std::string& id) const
{
    return conferenceMap_.find(id) != conferenceMap_.end();
}

bool ManagerImpl::isConferenceParticipant(const std::string& call_id)
{
    std::string accountId(getAccountFromCall(call_id));
    Call *call = getAccountLink(accountId)->getCall(call_id);
    return call and not call->getConfId().empty();
}

void ManagerImpl::addParticipant(const std::string& callId, const std::string& conferenceId)
{
    DEBUG("Manager: Add participant %s to %s", callId.c_str(), conferenceId.c_str());
    ConferenceMap::iterator iter = conferenceMap_.find(conferenceId);

    if (iter == conferenceMap_.end()) {
        ERROR("Manager: Error: Conference id is not valid");
        return;
    }

    std::string currentAccountId(getAccountFromCall(callId));
    Call *call = getAccountLink(currentAccountId)->getCall(callId);

    if (call == NULL) {
        ERROR("Manager: Error: Call id is not valid");
        return;
    }

    // store the current call id (it will change in offHoldCall or in answerCall)
    std::string current_call_id(getCurrentCallId());

    // detach from prior communication and switch to this conference
    if (current_call_id != callId) {
        if (isConference(current_call_id))
            detachParticipant(Call::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id);
    }

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    switchCall("");

    // Add main participant
    addMainParticipant(conferenceId);

    Conference* conf = iter->second;
    switchCall(conf->getConfID());

    // Add coresponding IDs in conf and call
    call->setConfId(conf->getConfID());
    conf->add(callId);

    // Connect new audio streams together
    getMainBuffer()->unBindAll(callId);

    std::map<std::string, std::string> callDetails(getCallDetails(callId));
    std::string callState(callDetails.find("CALL_STATE")->second);

    if (callState == "HOLD") {
        conf->bindParticipant(callId);
        offHoldCall(callId);
    } else if (callState == "INCOMING") {
        conf->bindParticipant(callId);
        answerCall(callId);
    } else if (callState == "CURRENT")
        conf->bindParticipant(callId);

    ParticipantSet participants(conf->getParticipantList());

    if (participants.empty())
        ERROR("Manager: Error: Participant list is empty for this conference");

    // reset ring buffer for all conference participant
    // flush conference participants only
    for (ParticipantSet::const_iterator p = participants.begin();
            p != participants.end(); ++p)
        getMainBuffer()->flush(*p);

    getMainBuffer()->flush(Call::DEFAULT_ID);

    // Connect stream
    addStream(callId);
}

void ManagerImpl::addMainParticipant(const std::string& conference_id)
{
    if (hasCurrentCall()) {
        std::string current_call_id(getCurrentCallId());

        if (isConference(current_call_id))
            detachParticipant(Call::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id);
    }

    {
        ost::MutexLock lock(audioLayerMutex_);

        ConferenceMap::const_iterator iter = conferenceMap_.find(conference_id);

        if (iter != conferenceMap_.end()) {
            Conference *conf = iter->second;

        ParticipantSet participants(conf->getParticipantList());

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
            WARN("Manager: Warning: Invalid conference state while adding main participant");

        dbus_.getCallManager()->conferenceChanged(conference_id, conf->getStateStr());
        }
    }

    switchCall(conference_id);
}

void ManagerImpl::joinParticipant(const std::string& callId1, const std::string& callId2)
{
    DEBUG("Manager: Join participants %s, %s", callId1.c_str(), callId2.c_str());

    std::map<std::string, std::string> call1Details(getCallDetails(callId1));
    std::map<std::string, std::string> call2Details(getCallDetails(callId2));

    std::string current_call_id(getCurrentCallId());
    DEBUG("Manager: Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ((current_call_id != callId1) and (current_call_id != callId2)) {
        // If currently in a conference
        if (isConference(current_call_id))
            detachParticipant(Call::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id); // currently in a call
    }

    Conference *conf = createConference(callId1, callId2);

    // Set corresponding conference ids for call 1
    std::string currentAccountId1 = getAccountFromCall(callId1);
    Call *call1 = getAccountLink(currentAccountId1)->getCall(callId1);

    if (call1 == NULL) {
        ERROR("Manager: Could not find call %s", callId1.c_str());
        return;
    }

    call1->setConfId(conf->getConfID());
    getMainBuffer()->unBindAll(callId1);

    // Set corresponding conderence details
    std::string currentAccountId2(getAccountFromCall(callId2));
    Call *call2 = getAccountLink(currentAccountId2)->getCall(callId2);

    if (call2 == NULL) {
        ERROR("Manager: Could not find call %s", callId2.c_str());
        return;
    }

    call2->setConfId(conf->getConfID());
    getMainBuffer()->unBindAll(callId2);

    // Process call1 according to its state
    std::string call1_state_str(call1Details.find("CALL_STATE")->second);
    DEBUG("Manager: Process call %s state: %s", callId1.c_str(), call1_state_str.c_str());

    if (call1_state_str == "HOLD") {
        conf->bindParticipant(callId1);
        offHoldCall(callId1);
    } else if (call1_state_str == "INCOMING") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else if (call1_state_str == "CURRENT")
        conf->bindParticipant(callId1);
    else if (call1_state_str == "RECORD")
        conf->bindParticipant(callId1);
    else if (call1_state_str == "INACTIVE") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else
        WARN("Manager: Call state not recognized");

    // Process call2 according to its state
    std::string call2_state_str(call2Details.find("CALL_STATE")->second);
    DEBUG("Manager: Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

    if (call2_state_str == "HOLD") {
        conf->bindParticipant(callId2);
        offHoldCall(callId2);
    } else if (call2_state_str == "INCOMING") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else if (call2_state_str == "CURRENT")
        conf->bindParticipant(callId2);
    else if (call2_state_str == "RECORD")
        conf->bindParticipant(callId2);
    else if (call2_state_str == "INACTIVE") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else
        WARN("Manager: Call state not recognized");

    // Switch current call id to this conference
    switchCall(conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    {
        ost::MutexLock lock(audioLayerMutex_);
        if (audiodriver_)
            conf->setRecordingSmplRate(audiodriver_->getSampleRate());
    }

    getMainBuffer()->stateInfo();
}

void ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        ERROR("Manager: Error: Participant number must be higher or equal to 2");
        return;
    }

    Conference *conf = new Conference;

    int successCounter = 0;

    for (std::vector<std::string>::const_iterator p = participantList.begin();
         p != participantList.end(); ++p) {
        std::string numberaccount(*p);
        std::string tostr(numberaccount.substr(0, numberaccount.find(",")));
        std::string account(numberaccount.substr(numberaccount.find(",") + 1, numberaccount.size()));

        std::string generatedCallID(getNewCallID());

        // Manager methods may behave differently if the call id participates in a conference
        conf->add(generatedCallID);

        switchCall("");

        // Create call
        bool callSuccess = outgoingCall(account, generatedCallID, tostr, conf->getConfID());

        // If not able to create call remove this participant from the conference
        if (!callSuccess)
            conf->remove(generatedCallID);
        else {
            dbus_.getCallManager()->newCallCreated(account, generatedCallID, tostr);
            successCounter++;
        }
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));
        dbus_.getCallManager()->conferenceCreated(conf->getConfID());

        {
            ost::MutexLock lock(audioLayerMutex_);

            if (audiodriver_)
                conf->setRecordingSmplRate(audiodriver_->getSampleRate());
        }

        getMainBuffer()->stateInfo();
    } else
        delete conf;
}

void ManagerImpl::detachParticipant(const std::string& call_id,
                                    const std::string& current_id)
{
    DEBUG("Manager: Detach participant %s (current id: %s)", call_id.c_str(),
           current_id.c_str());
    std::string current_call_id(getCurrentCallId());

    if (call_id != Call::DEFAULT_ID) {
        std::string currentAccountId(getAccountFromCall(call_id));
        Call *call = getAccountLink(currentAccountId)->getCall(call_id);

        if (call == NULL) {
            ERROR("Manager: Error: Could not find call %s", call_id.c_str());
            return;
        }

        Conference *conf = getConferenceFromCallID(call_id);

        if (conf == NULL) {
            ERROR("Manager: Error: Call is not conferencing, cannot detach");
            return;
        }

        std::map<std::string, std::string> call_details(getCallDetails(call_id));
        std::map<std::string, std::string>::iterator iter_details(call_details.find("CALL_STATE"));

        if (iter_details == call_details.end()) {
            ERROR("Manager: Error: Could not find CALL_STATE");
            return;
        }

        if (iter_details->second == "RINGING")
            removeParticipant(call_id);
        else {
            onHoldCall(call_id);
            removeParticipant(call_id);
            // Conference may have been deleted and set to 0 above
            processRemainingParticipants(current_call_id, conf);
            if (conf == 0) {
                ERROR("Manager: Error: Call is not conferencing, cannot detach");
                return;
            }
        }

        dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
    } else {
        DEBUG("Manager: Unbind main participant from conference %d");
        getMainBuffer()->unBindAll(Call::DEFAULT_ID);

        if (not isConference(current_call_id)) {
            ERROR("Manager: Warning: Current call id (%s) is not a conference", current_call_id.c_str());
            return;
        }

        ConferenceMap::iterator iter = conferenceMap_.find(current_call_id);

        if (iter == conferenceMap_.end() or iter->second == 0) {
            DEBUG("Manager: Error: Conference is NULL");
            return;
        }
        Conference *conf = iter->second;

        if (conf->getState() == Conference::ACTIVE_ATTACHED)
            conf->setState(Conference::ACTIVE_DETACHED);
        else if (conf->getState() == Conference::ACTIVE_ATTACHED_REC)
            conf->setState(Conference::ACTIVE_DETACHED_REC);
        else
            WARN("Manager: Warning: Undefined behavior, invalid conference state in detach participant");

        dbus_.getCallManager()->conferenceChanged(conf->getConfID(),
                                                  conf->getStateStr());

        switchCall("");
    }
}

void ManagerImpl::removeParticipant(const std::string& call_id)
{
    DEBUG("Manager: Remove participant %s", call_id.c_str());

    // this call is no more a conference participant
    const std::string currentAccountId(getAccountFromCall(call_id));
    Call *call = getAccountLink(currentAccountId)->getCall(call_id);

    ConferenceMap conf_map = conferenceMap_;
    ConferenceMap::const_iterator iter = conf_map.find(call->getConfId());

    if (iter == conf_map.end() or iter->second == 0) {
        ERROR("Manager: Error: No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    Conference *conf = iter->second;
    DEBUG("Manager: Remove participant %s", call_id.c_str());
    conf->remove(call_id);
    call->setConfId("");

    removeStream(call_id);
    getMainBuffer()->stateInfo();
    dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
}

void ManagerImpl::processRemainingParticipants(const std::string &current_call_id, Conference * &conf)
{
    ParticipantSet participants(conf->getParticipantList());
    size_t n = participants.size();
    DEBUG("Manager: Process remaining %d participant(s) from conference %s",
           n, conf->getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (ParticipantSet::const_iterator p = participants.begin();
             p != participants.end(); ++p)
            getMainBuffer()->flush(*p);

        getMainBuffer()->flush(Call::DEFAULT_ID);
    } else if (n == 1) {
        ParticipantSet::iterator p = participants.begin();

        // bind main participant to remaining conference call
        if (p != participants.end()) {
            // this call is no longer a conference participant
            std::string currentAccountId(getAccountFromCall(*p));
            Call *call = getAccountLink(currentAccountId)->getCall(*p);
            if (call) {
                call->setConfId("");
                // if we are not listening to this conference
                if (current_call_id != conf->getConfID())
                    onHoldCall(call->getCallId());
                else
                    switchCall(*p);
            }
        }

        removeConference(conf->getConfID());
        conf = 0;
    } else {
        DEBUG("Manager: No remaining participants, remove conference");
        removeConference(conf->getConfID());
        conf = 0;
        switchCall("");
    }
}

void ManagerImpl::joinConference(const std::string& conf_id1,
                                 const std::string& conf_id2)
{
    ConferenceMap::iterator iter(conferenceMap_.find(conf_id1));

    if (iter == conferenceMap_.end()) {
        ERROR("Manager: Error: Not a valid conference ID: %s", conf_id1.c_str());
        return;
    }

    if (conferenceMap_.find(conf_id2) != conferenceMap_.end()) {
        ERROR("Manager: Error: Not a valid conference ID: %s", conf_id2.c_str());
        return;
    }

    if (iter->second) {
        Conference *conf = iter->second;
        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator p = participants.begin();
                p != participants.end(); ++p) {
            detachParticipant(*p, "");
            addParticipant(*p, conf_id2);
        }
    }
}

void ManagerImpl::addStream(const std::string& call_id)
{
    DEBUG("Manager: Add audio stream %s", call_id.c_str());

    std::string currentAccountId(getAccountFromCall(call_id));
    Call *call = getAccountLink(currentAccountId)->getCall(call_id);

    if (call and isConferenceParticipant(call_id)) {
        DEBUG("Manager: Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = conferenceMap_.find(call->getConfId());

        if (iter != conferenceMap_.end() and iter->second) {
            Conference* conf = iter->second;

            conf->bindParticipant(call_id);

            ParticipantSet participants(conf->getParticipantList());

            // reset ring buffer for all conference participant
            for (ParticipantSet::const_iterator iter_p = participants.begin();
                    iter_p != participants.end(); ++iter_p)
                getMainBuffer()->flush(*iter_p);

            getMainBuffer()->flush(Call::DEFAULT_ID);
        }

    } else {
        DEBUG("Manager: Add stream to call");

        // bind to main
        getMainBuffer()->bindCallID(call_id);

        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->flushUrgent();
        audiodriver_->flushMain();
    }

    getMainBuffer()->stateInfo();
}

void ManagerImpl::removeStream(const std::string& call_id)
{
    DEBUG("Manager: Remove audio stream %s", call_id.c_str());
    getMainBuffer()->unBindAll(call_id);
    getMainBuffer()->stateInfo();
}

//THREAD=Main
void ManagerImpl::saveConfig()
{
    DEBUG("Manager: Saving Configuration to XDG directory %s", path_.c_str());
    AudioLayer *audiolayer = getAudioDriver();
    if (audiolayer != NULL) {
        audioPreference.setVolumemic(audiolayer->getCaptureGain());
        audioPreference.setVolumespkr(audiolayer->getPlaybackGain());
    }

    try {
        Conf::YamlEmitter emitter(path_.c_str());

        for (AccountMap::iterator iter = accountMap_.begin(); iter != accountMap_.end(); ++iter)
            iter->second->serialize(&emitter);

        preferences.serialize(&emitter);
        voipPreferences.serialize(&emitter);
        addressbookPreference.serialize(&emitter);
        hookPreference.serialize(&emitter);
        audioPreference.serialize(&emitter);
        shortcutPreferences.serialize(&emitter);

        emitter.serializeData();
    } catch (const Conf::YamlEmitterException &e) {
        ERROR("ConfigTree: %s", e.what());
    }
}

//THREAD=Main
void ManagerImpl::sendDtmf(const std::string& id, char code)
{
    std::string accountid(getAccountFromCall(id));
    playDtmf(code);
    getAccountLink(accountid)->carryingDTMFdigits(id, code);
}

//THREAD=Main | VoIPLink
void ManagerImpl::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        DEBUG("Manager: playDtmf: Do not have to play a tone...");
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        DEBUG("Manager: playDtmf: Pulse length is not set...");
        return;
    }

    ost::MutexLock lock(audioLayerMutex_);

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

    // fast return, no sound, so no dtmf
    if (audiodriver_ == NULL || dtmfKey_.get() == 0) {
        DEBUG("Manager: playDtmf: Error no audio layer...");
        return;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    int size = (int)((pulselen * (float) audiodriver_->getSampleRate()) / 1000);

    // this buffer is for mono
    // TODO <-- this should be global and hide if same size
    SFLDataFormat *buf = new SFLDataFormat[size];

    // Handle dtmf
    dtmfKey_->startTone(code);

    // copy the sound
    if (dtmfKey_->generateDTMF(buf, size)) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();
        audiodriver_->startStream();
        audiodriver_->putUrgent(buf, size * sizeof(SFLDataFormat));
    }

    // TODO Cache the DTMF

    delete [] buf;
}

// Multi-thread
bool ManagerImpl::incomingCallWaiting() const
{
    return nbIncomingWaitingCall_ > 0;
}

void ManagerImpl::addWaitingCall(const std::string& id)
{
    ost::MutexLock m(waitingCallMutex_);
    waitingCall_.insert(id);
    nbIncomingWaitingCall_++;
}

void ManagerImpl::removeWaitingCall(const std::string& id)
{
    ost::MutexLock m(waitingCallMutex_);

    if (waitingCall_.erase(id))
        nbIncomingWaitingCall_--;
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
void ManagerImpl::incomingCall(Call* call, const std::string& accountId)
{
    assert(call);
    stopTone();

    associateCallToAccount(call->getCallId(), accountId);

    if (accountId.empty())
        setIPToIPForCall(call->getCallId(), true);
    else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber(call->getPeerNumber());

        const char SIP_PREFIX[] = "sip:";
        size_t startIndex = peerNumber.find(SIP_PREFIX);

        if (startIndex != std::string::npos)
            call->setPeerNumber(peerNumber.substr(startIndex + sizeof(SIP_PREFIX) - 1));
    }

    if (not hasCurrentCall()) {
        call->setConnectionState(Call::RINGING);
        ringtone(accountId);
    }

    addWaitingCall(call->getCallId());

    std::string number(call->getPeerNumber());

    std::string from("<" + number + ">");
    dbus_.getCallManager()->incomingCall(accountId, call->getCallId(), call->getDisplayName() + " " + from);
}


//THREAD=VoIP
void ManagerImpl::incomingMessage(const std::string& callID,
                                  const std::string& from,
                                  const std::string& message)
{
    if (isConferenceParticipant(callID)) {
        Conference *conf = getConferenceFromCallID(callID);

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            if (*iter_p == callID)
                continue;

            std::string accountId(getAccountFromCall(*iter_p));

            DEBUG("Manager: Send message to %s, (%s)", (*iter_p).c_str(), accountId.c_str());

            Account *account = getAccount(accountId);

            if (!account) {
                ERROR("Manager: Failed to get account while sending instant message");
                return;
            }

            account->getVoIPLink()->sendTextMessage(callID, message, from);
        }

        // in case of a conference we must notify client using conference id
        dbus_.getCallManager()->incomingMessage(conf->getConfID(), from, message);

    } else
        dbus_.getCallManager()->incomingMessage(callID, from, message);
}


//THREAD=VoIP
bool ManagerImpl::sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
    if (isConference(callID)) {
        DEBUG("Manager: Is a conference, send instant message to everyone");
        ConferenceMap::iterator it = conferenceMap_.find(callID);

        if (it == conferenceMap_.end())
            return false;

        Conference *conf = it->second;

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            std::string accountId = getAccountFromCall(*iter_p);

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Manager: Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(*iter_p, message, from);
        }

        return true;
    }

    if (isConferenceParticipant(callID)) {
        DEBUG("Manager: Call is participant in a conference, send instant message to everyone");
        Conference *conf = getConferenceFromCallID(callID);

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            const std::string accountId(getAccountFromCall(*iter_p));

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Manager: Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(*iter_p, message, from);
        }
    } else {
        Account *account = getAccount(getAccountFromCall(callID));

        if (!account) {
            DEBUG("Manager: Failed to get account while sending instant message");
            return false;
        }

        account->getVoIPLink()->sendTextMessage(callID, message, from);
    }

    return true;
}

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall(const std::string& id)
{
    DEBUG("Manager: Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(id))
        stopTone();

    // Connect audio streams
    addStream(id);

    {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->flushMain();
        audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording()) {
        setRecordingCall(id);
        dbus_.getCallManager()->callStateChanged(id, "RECORD");
    } else
        dbus_.getCallManager()->callStateChanged(id, "CURRENT");
}

//THREAD=VoIP Call=Outgoing
void ManagerImpl::peerRingingCall(const std::string& id)
{
    DEBUG("Manager: Peer call %s ringing", id.c_str());

    if (isCurrentCall(id))
        ringback();

    dbus_.getCallManager()->callStateChanged(id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall(const std::string& call_id)
{
    DEBUG("Manager: Peer hungup call %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        Conference *conf = getConferenceFromCallID(call_id);

        if (conf != 0) {
            removeParticipant(call_id);
            processRemainingParticipants(getCurrentCallId(), conf);
        }
    } else {
        if (isCurrentCall(call_id)) {
            stopTone();
            switchCall("");
        }
    }

    /* Direct IP to IP call */
    if (isIPToIP(call_id)) {
        Call * call = SIPVoIPLink::instance()->getCall(call_id);
        history_.addCall(call, preferences.getHistoryLimit());
        SIPVoIPLink::instance()->hangup(call_id);
    }
    else {
        const std::string account_id(getAccountFromCall(call_id));
        VoIPLink *link = getAccountLink(account_id);
        Call * call = link->getCall(call_id);
        history_.addCall(call, preferences.getHistoryLimit());
        link->peerHungup(call_id);
    }

    /* Broadcast a signal over DBus */
    dbus_.getCallManager()->callStateChanged(call_id, "HUNGUP");

    removeWaitingCall(call_id);
    removeCallAccount(call_id);
    removeStream(call_id);

    if (getCallList().empty()) {
        DEBUG("Manager: Stop audio stream, there are no calls remaining");
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }
}

//THREAD=VoIP
void ManagerImpl::callBusy(const std::string& id)
{
    DEBUG("Manager: Call %s busy", id.c_str());
    dbus_.getCallManager()->callStateChanged(id, "BUSY");

    if (isCurrentCall(id)) {
        playATone(Tone::TONE_BUSY);
        switchCall("");
    }

    removeCallAccount(id);
    removeWaitingCall(id);
}

//THREAD=VoIP
void ManagerImpl::callFailure(const std::string& call_id)
{
    dbus_.getCallManager()->callStateChanged(call_id, "FAILURE");

    if (isCurrentCall(call_id)) {
        playATone(Tone::TONE_BUSY);
        switchCall("");
    }

    if (isConferenceParticipant(call_id)) {
        DEBUG("Manager: Call %s participating in a conference failed", call_id.c_str());
        Conference *conf = getConferenceFromCallID(call_id);

        if (conf == NULL) {
            ERROR("Manager: Could not retreive conference from call id %s", call_id.c_str());
            return;
        }

        // remove this participant
        removeParticipant(call_id);
        processRemainingParticipants(getCurrentCallId(), conf);
    }

    removeCallAccount(call_id);
    removeWaitingCall(call_id);
}

//THREAD=VoIP
void ManagerImpl::startVoiceMessageNotification(const std::string& accountId,
        int nb_msg)
{
    dbus_.getCallManager()->voiceMailNotify(accountId, nb_msg);
}

void ManagerImpl::connectionStatusNotification()
{
    dbus_.getConfigurationManager()->accountsChanged();
}

/**
 * Multi Thread
 */
void ManagerImpl::playATone(Tone::TONEID toneId)
{
    if (not voipPreferences.getPlayTones())
        return;

    {
        ost::MutexLock lock(audioLayerMutex_);

        if (audiodriver_ == NULL) {
            ERROR("Manager: Error: Audio layer not initialized");
            return;
        }

        audiodriver_->flushUrgent();
        audiodriver_->startStream();
    }

    if (telephoneTone_.get() != 0) {
        ost::MutexLock lock(toneMutex_);
        telephoneTone_->setCurrentTone(toneId);
    }
}

/**
 * Multi Thread
 */
void ManagerImpl::stopTone()
{
    if (not voipPreferences.getPlayTones())
        return;

    ost::MutexLock lock(toneMutex_);

    if (telephoneTone_.get() != NULL)
        telephoneTone_->setCurrentTone(Tone::TONE_NULL);

    if (audiofile_.get()) {
        std::string filepath(audiofile_->getFilePath());
        dbus_.getCallManager()->recordPlaybackStopped(filepath);
        audiofile_.reset(0);
    }
}

/**
 * Multi Thread
 */
void ManagerImpl::playTone()
{
    playATone(Tone::TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void ManagerImpl::playToneWithMessage()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void ManagerImpl::congestion()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringback()
{
    playATone(Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringtone(const std::string& accountID)
{
    Account *account = getAccount(accountID);

    if (!account) {
        WARN("Manager: Warning: invalid account in ringtone");
        return;
    }

    if (!account->getRingtoneEnabled()) {
        ringback();
        return;
    }

    std::string ringchoice = account->getRingtonePath();

    if (ringchoice.find(DIR_SEPARATOR_STR) == std::string::npos) {
        // check inside global share directory
        static const char * const RINGDIR = "ringtones";
        ringchoice = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR
                     + RINGDIR + DIR_SEPARATOR_STR + ringchoice;
    }

    int samplerate;
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_) {
            ERROR("Manager: Error: no audio layer in ringtone");
            return;
        }

        samplerate = audiodriver_->getSampleRate();
    }

    {
        ost::MutexLock m(toneMutex_);

        if (audiofile_.get()) {
            dbus_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
            audiofile_.reset(0);
        }

        try {
            if (ringchoice.find(".wav") != std::string::npos)
                audiofile_.reset(new WaveFile(ringchoice, samplerate));
            else {
                sfl::Codec *codec;
                if (ringchoice.find(".ul") != std::string::npos or ringchoice.find(".au") != std::string::npos)
                    codec = audioCodecFactory.getCodec(PAYLOAD_CODEC_ULAW);
                else
                    throw AudioFileException("Couldn't guess an appropriate decoder");

                audiofile_.reset(new RawFile(ringchoice, static_cast<sfl::AudioCodec *>(codec), samplerate));
            }
        } catch (const AudioFileException &e) {
            ERROR("Manager: Exception: %s", e.what());
        }
    } // leave mutex

    ost::MutexLock lock(audioLayerMutex_);
    // start audio if not started AND flush all buffers (main and urgent)
    audiodriver_->startStream();
}

AudioLoop* ManagerImpl::getTelephoneTone()
{
    if (telephoneTone_.get()) {
        ost::MutexLock m(toneMutex_);
        return telephoneTone_->getCurrentTone();
    } else
        return NULL;
}

AudioLoop*
ManagerImpl::getTelephoneFile()
{
    ost::MutexLock m(toneMutex_);
    return audiofile_.get();
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 */
std::string ManagerImpl::createConfigFile() const
{
    std::string configdir = std::string(HOMEDIR) + DIR_SEPARATOR_STR +
                            ".config" + DIR_SEPARATOR_STR + PACKAGE;

    if (XDG_CONFIG_HOME != NULL) {
        std::string xdg_env(XDG_CONFIG_HOME);
        if (not xdg_env.empty())
            configdir = xdg_env;
    }

    if (mkdir(configdir.data(), 0700) != 0) {
        // If directory creation failed
        if (errno != EEXIST)
           DEBUG("Cannot create directory: %m");
    }

    static const char * const PROGNAME = "sflphoned";
    return configdir + DIR_SEPARATOR_STR + PROGNAME + ".yml";
}

std::vector<std::string> ManagerImpl::unserialize(std::string s)
{
    std::vector<std::string> list;
    std::string temp;

    while (s.find("/", 0) != std::string::npos) {
        size_t pos = s.find("/", 0);
        temp = s.substr(0, pos);
        s.erase(0, pos + 1);
        list.push_back(temp);
    }

    return list;
}

std::string ManagerImpl::serialize(const std::vector<std::string> &v)
{
    std::ostringstream os;
    std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>(os, "/"));
    return os.str();
}

std::string ManagerImpl::getCurrentCodecName(const std::string& id)
{
    std::string accountid = getAccountFromCall(id);
    VoIPLink* link = getAccountLink(accountid);
    Call* call = link->getCall(id);
    std::string codecName;

    if (call) {
        Call::CallState state = call->getState();

        if (state == Call::ACTIVE or state == Call::CONFERENCING)
            codecName = link->getCurrentCodecName(call);
    }

    return codecName;
}

#ifdef SFL_VIDEO
std::string ManagerImpl::getCurrentVideoCodecName(const std::string& ID)
{
    std::string accountID = getAccountFromCall(ID);
    VoIPLink* link = getAccountLink(accountID);
    return link->getCurrentVideoCodecName(ID);
}
#endif

/**
 * Set input audio plugin
 */
void ManagerImpl::setAudioPlugin(const std::string& audioPlugin)
{
    ost::MutexLock lock(audioLayerMutex_);

    audioPreference.setPlugin(audioPlugin);

    AlsaLayer *alsa = dynamic_cast<AlsaLayer*>(audiodriver_);

    if (!alsa) {
        ERROR("Can't find alsa device");
        return;
    }

    bool wasStarted = audiodriver_->isStarted();

    // Recreate audio driver with new settings
    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

/**
 * Set audio output device
 */
void ManagerImpl::setAudioDevice(const int index, int streamType)
{
    ost::MutexLock lock(audioLayerMutex_);

    AlsaLayer *alsaLayer = dynamic_cast<AlsaLayer*>(audiodriver_);

    if (!alsaLayer) {
        ERROR("Can't find alsa device");
        return ;
    }

    bool wasStarted = audiodriver_->isStarted();

    switch (streamType) {
        case SFL_PCM_PLAYBACK:
            audioPreference.setCardout(index);
            break;
        case SFL_PCM_CAPTURE:
            audioPreference.setCardin(index);
            break;
        case SFL_PCM_RINGTONE:
            audioPreference.setCardring(index);
            break;
        default:
            break;
    }

    // Recreate audio driver with new settings
    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string> ManagerImpl::getAudioOutputDeviceList()
{
    std::vector<std::string> devices;

    ost::MutexLock lock(audioLayerMutex_);

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer*>(audiodriver_);

    if (alsalayer)
        devices = alsalayer->getAudioDeviceList(AUDIO_STREAM_PLAYBACK);

    return devices;
}


/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList()
{
    std::vector<std::string> devices;

    ost::MutexLock lock(audioLayerMutex_);

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer *>(audiodriver_);

    if (alsalayer)
        devices = alsalayer->getAudioDeviceList(AUDIO_STREAM_CAPTURE);

    return devices;
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex()
{
    ost::MutexLock lock(audioLayerMutex_);

    std::vector<std::string> v;

    AlsaLayer *alsa = dynamic_cast<AlsaLayer*>(audiodriver_);

    if (alsa) {
        std::stringstream ssi, sso, ssr;
        sso << alsa->getIndexPlayback();
        v.push_back(sso.str());
        ssi << alsa->getIndexCapture();
        v.push_back(ssi.str());
        ssr << alsa->getIndexRingtone();
        v.push_back(ssr.str());
    }

    return v;
}

int ManagerImpl::isRingtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Manager: Warning: invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void ManagerImpl::ringtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Manager: Warning: invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled(false) : account->setRingtoneEnabled(true);
}

std::string ManagerImpl::getRecordPath() const
{
    return audioPreference.getRecordpath();
}

void ManagerImpl::setRecordPath(const std::string& recPath)
{
    DEBUG("Manager: Set record path %s", recPath.c_str());
    audioPreference.setRecordpath(recPath);
}

bool ManagerImpl::getIsAlwaysRecording() const
{
    return audioPreference.getIsAlwaysRecording();
}

void ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
    return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

void ManagerImpl::setRecordingCall(const std::string& id)
{
    Recordable* rec = NULL;

    ConferenceMap::const_iterator it(conferenceMap_.find(id));
    if (it == conferenceMap_.end()) {
        DEBUG("Manager: Set recording for call %s", id.c_str());
        std::string accountid(getAccountFromCall(id));
        rec = getAccountLink(accountid)->getCall(id);
    } else {
        DEBUG("Manager: Set recording for conference %s", id.c_str());
        Conference *conf = it->second;

        if (conf) {
            rec = conf;
            if (rec->isRecording())
                conf->setState(Conference::ACTIVE_ATTACHED);
            else
                conf->setState(Conference::ACTIVE_ATTACHED_REC);
        }
    }

    if (rec == NULL) {
        ERROR("Manager: Error: Could not find recordable instance %s", id.c_str());
        return;
    }

    rec->setRecording();
    dbus_.getCallManager()->recordPlaybackFilepath(id, rec->getFilename());
}

bool ManagerImpl::isRecording(const std::string& id)
{
    const std::string accountid(getAccountFromCall(id));
    Recordable* rec = getAccountLink(accountid)->getCall(id);
    return rec and rec->isRecording();
}

bool ManagerImpl::startRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Manager: Start recorded file playback %s", filepath.c_str());

    int sampleRate;
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_) {
            ERROR("Manager: Error: No audio layer in start recorded file playback");
            return false;
        }

        sampleRate = audiodriver_->getSampleRate();
    }

    {
        ost::MutexLock m(toneMutex_);

        if (audiofile_.get()) {
            dbus_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
            audiofile_.reset(0);
        }

        try {
            audiofile_.reset(new WaveFile(filepath, sampleRate));
        } catch (const AudioFileException &e) {
            ERROR("Manager: Exception: %s", e.what());
        }
    } // release toneMutex

    ost::MutexLock lock(audioLayerMutex_);
    audiodriver_->startStream();

    return true;
}


void ManagerImpl::stopRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Manager: Stop recorded file playback %s", filepath.c_str());

    {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    {
        ost::MutexLock m(toneMutex_);
        audiofile_.reset(0);
    }
}

void ManagerImpl::setHistoryLimit(int days)
{
    DEBUG("Manager: Set history limit");
    preferences.setHistoryLimit(days);
    saveConfig();
}

int ManagerImpl::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

int32_t ManagerImpl::getMailNotify() const
{
    return preferences.getNotifyMails();
}

void ManagerImpl::setMailNotify()
{
    DEBUG("Manager: Set mail notify");
    preferences.getNotifyMails() ? preferences.setNotifyMails(true) : preferences.setNotifyMails(false);
    saveConfig();
}

void ManagerImpl::setAudioManager(const std::string &api)
{
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_)
            return;

        if (api == audioPreference.getAudioApi()) {
            DEBUG("Manager: Audio manager chosen already in use. No changes made. ");
            return;
        }
    }

    switchAudioManager();

    saveConfig();
}

std::string ManagerImpl::getAudioManager() const
{
    return audioPreference.getAudioApi();
}


int ManagerImpl::getAudioDeviceIndex(const std::string &name)
{
    int soundCardIndex = 0;

    ost::MutexLock lock(audioLayerMutex_);

    if (audiodriver_ == NULL) {
        ERROR("Manager: Error: Audio layer not initialized");
        return soundCardIndex;
    }

    AlsaLayer *alsalayer = dynamic_cast<AlsaLayer *>(audiodriver_);

    if (alsalayer)
        soundCardIndex = alsalayer -> getAudioDeviceIndex(name);

    return soundCardIndex;
}

std::string ManagerImpl::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getPlugin();
}


std::string ManagerImpl::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce() ? "enabled" : "disabled";
}

void ManagerImpl::setNoiseSuppressState(const std::string &state)
{
    audioPreference.setNoiseReduce(state == "enabled");
}

bool ManagerImpl::getEchoCancelState() const
{
    return audioPreference.getEchoCancel();
}

void ManagerImpl::setEchoCancelState(const std::string &state)
{
    audioPreference.setEchoCancel(state == "enabled");
}

int ManagerImpl::getEchoCancelTailLength() const
{
    return audioPreference.getEchoCancelTailLength();
}

void ManagerImpl::setEchoCancelTailLength(int length)
{
    audioPreference.setEchoCancelTailLength(length);
}

int ManagerImpl::getEchoCancelDelay() const
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
void ManagerImpl::initAudioDriver()
{
    ost::MutexLock lock(audioLayerMutex_);
    audiodriver_ = audioPreference.createAudioLayer();
}

void ManagerImpl::switchAudioManager()
{
    ost::MutexLock lock(audioLayerMutex_);

    bool wasStarted = audiodriver_->isStarted();
    delete audiodriver_;
    audiodriver_ = audioPreference.switchAndCreateAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

void ManagerImpl::audioSamplingRateChanged(int samplerate)
{
    ost::MutexLock lock(audioLayerMutex_);

    if (!audiodriver_) {
        DEBUG("Manager: No Audio driver initialized");
        return;
    }

    // Only modify internal sampling rate if new sampling rate is higher
    int currentSamplerate = mainBuffer_.getInternalSamplingRate();

    if (currentSamplerate >= samplerate) {
        DEBUG("Manager: No need to update audio layer sampling rate");
        return;
    } else
        DEBUG("Manager: Audio sampling rate changed: %d -> %d", currentSamplerate, samplerate);

    bool wasActive = audiodriver_->isStarted();

    mainBuffer_.setInternalSamplingRate(samplerate);

    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    unsigned int sampleRate = audiodriver_->getSampleRate();

    telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), sampleRate));
    dtmfKey_.reset(new DTMF(sampleRate));

    if (wasActive)
        audiodriver_->startStream();
}

//THREAD=Main
std::string ManagerImpl::getConfigString(const std::string& section,
                                         const std::string& name) const
{
    return config_.getConfigTreeItemValue(section, name);
}

//THREAD=Main
void ManagerImpl::setConfig(const std::string& section,
                            const std::string& name, const std::string& value)
{
    config_.setConfigTreeItem(section, name, value);
}

//THREAD=Main
void ManagerImpl::setConfig(const std::string& section,
                            const std::string& name, int value)
{
    std::ostringstream valueStream;
    valueStream << value;
    config_.setConfigTreeItem(section, name, valueStream.str());
}

void ManagerImpl::setAccountsOrder(const std::string& order)
{
    DEBUG("Manager: Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder(order);

    saveConfig();
}

std::vector<std::string> ManagerImpl::getAccountList() const
{
    using std::vector;
    using std::string;
    vector<string> account_order(loadAccountOrder());

    // The IP2IP profile is always available, and first in the list

    AccountMap::const_iterator ip2ip_iter = accountMap_.find(SIPAccount::IP2IP_PROFILE);

    vector<string> v;
    if (ip2ip_iter->second)
        v.push_back(ip2ip_iter->second->getAccountID());
    else
        ERROR("Manager: could not find IP2IP profile in getAccount list");

    // If no order has been set, load the default one ie according to the creation date.
    if (account_order.empty()) {
        for (AccountMap::const_iterator iter = accountMap_.begin(); iter != accountMap_.end(); ++iter) {
            if (iter->first == SIPAccount::IP2IP_PROFILE || iter->first.empty())
                continue;

            if (iter->second)
                v.push_back(iter->second->getAccountID());
        }
    }
    else {
        for (vector<string>::const_iterator iter = account_order.begin(); iter != account_order.end(); ++iter) {
            if (*iter == SIPAccount::IP2IP_PROFILE or iter->empty())
                continue;

            AccountMap::const_iterator account_iter = accountMap_.find(*iter);

            if (account_iter != accountMap_.end() and account_iter->second)
                v.push_back(account_iter->second->getAccountID());
        }
    }

    return v;
}

std::map<std::string, std::string> ManagerImpl::getAccountDetails(
    const std::string& accountID) const
{
    // Default account used to get default parameters if requested by client (to build new account)
    static const SIPAccount DEFAULT_ACCOUNT("default");

    if (accountID.empty()) {
        DEBUG("Manager: Returning default account settings");
        return DEFAULT_ACCOUNT.getAccountDetails();
    }

    AccountMap::const_iterator iter = accountMap_.find(accountID);
    Account * account = NULL;

    if (iter != accountMap_.end())
        account = iter->second;

    if (account)
        return account->getAccountDetails();
    else {
        DEBUG("Manager: Get account details on a non-existing accountID %s. Returning default", accountID.c_str());
        return DEFAULT_ACCOUNT.getAccountDetails();
    }
}

// method to reduce the if/else mess.
// Even better, switch to XML !

void ManagerImpl::setAccountDetails(const std::string& accountID,
                                    const std::map<std::string, std::string>& details)
{
    DEBUG("Manager: Set account details for %s", accountID.c_str());

    Account* account = getAccount(accountID);

    if (account == NULL) {
        ERROR("Manager: Error: Could not find account %s", accountID.c_str());
        return;
    }

    account->setAccountDetails(details);

    // Serialize configuration to disk once it is done
    saveConfig();

    if (account->isEnabled())
        account->registerVoIPLink();
    else
        account->unregisterVoIPLink();

    // Update account details to the client side
    dbus_.getConfigurationManager()->accountsChanged();
}

std::string ManagerImpl::addAccount(const std::map<std::string, std::string>& details)
{
    /** @todo Deal with both the accountMap_ and the Configuration */
    std::stringstream accountID;

    accountID << "Account:" << time(NULL);
    std::string newAccountID(accountID.str());

    // Get the type
    std::string accountType((*details.find(CONFIG_ACCOUNT_TYPE)).second);

    DEBUG("Manager: Adding account %s", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    Account* newAccount = NULL;

    if (accountType == "SIP")
        newAccount = new SIPAccount(newAccountID);
#if HAVE_IAX
    else if (accountType == "IAX")
        newAccount = new IAXAccount(newAccountID);
#endif
    else {
        ERROR("Unknown %s param when calling addAccount(): %s",
               CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    accountMap_[newAccountID] = newAccount;

    newAccount->setAccountDetails(details);

    // Add the newly created account in the account order list
    std::string accountList(preferences.getAccountOrder());

    newAccountID += "/";
    if (not accountList.empty()) {
        // Prepend the new account
        accountList.insert(0, newAccountID);
        preferences.setAccountOrder(accountList);
    } else {
        accountList = newAccountID;
        preferences.setAccountOrder(accountList);
    }

    DEBUG("AccountMap: %s", accountList.c_str());

    newAccount->registerVoIPLink();

    saveConfig();

    dbus_.getConfigurationManager()->accountsChanged();

    return accountID.str();
}

void ManagerImpl::removeAccount(const std::string& accountID)
{
    // Get it down and dying
    Account* remAccount = getAccount(accountID);

    if (remAccount != NULL) {
        remAccount->unregisterVoIPLink();
        accountMap_.erase(accountID);
        // http://projects.savoirfairelinux.net/issues/show/2355
        // delete remAccount;
    }

    config_.removeSection(accountID);

    saveConfig();

    dbus_.getConfigurationManager()->accountsChanged();
}

// ACCOUNT handling
bool ManagerImpl::associateCallToAccount(const std::string& callID,
        const std::string& accountID)
{
    if (getAccountFromCall(callID).empty() and accountExists(accountID)) {
        // account id exist in AccountMap
        ost::MutexLock m(callAccountMapMutex_);
        callAccountMap_[callID] = accountID;
        DEBUG("Manager: Associate Call %s with Account %s", callID.data(), accountID.data());
        return true;
    }

    return false;
}

std::string ManagerImpl::getAccountFromCall(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    CallAccountMap::iterator iter = callAccountMap_.find(callID);

    return (iter == callAccountMap_.end()) ? "" : iter->second;
}

void ManagerImpl::removeCallAccount(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    callAccountMap_.erase(callID);

    // Stop audio layer if there is no call anymore
    if (callAccountMap_.empty()) {
        ost::MutexLock lock(audioLayerMutex_);

        if (audiodriver_)
            audiodriver_->stopStream();
    }

}

bool ManagerImpl::isValidCall(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    return callAccountMap_.find(callID) != callAccountMap_.end();
}

std::string ManagerImpl::getNewCallID()
{
    std::ostringstream random_id("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (not getAccountFromCall(random_id.str()).empty()) {
        random_id.clear();
        random_id << "s";
        random_id << (unsigned) rand();
    }

    return random_id.str();
}

std::vector<std::string> ManagerImpl::loadAccountOrder() const
{
    return unserialize(preferences.getAccountOrder());
}

void ManagerImpl::loadDefaultAccountMap()
{
    // build a default IP2IP account with default parameters
    accountMap_[SIPAccount::IP2IP_PROFILE] = new SIPAccount(SIPAccount::IP2IP_PROFILE);
    SIPVoIPLink::instance()->createDefaultSipUdpTransport();
    accountMap_[SIPAccount::IP2IP_PROFILE]->registerVoIPLink();
}

namespace {
    bool isIP2IP(const Conf::YamlNode *node)
    {
        std::string id;
        dynamic_cast<const Conf::MappingNode *>(node)->getValue("id", &id);
        return id == "IP2IP";
    }

    void loadAccount(const Conf::YamlNode *item, AccountMap &accountMap)
    {
        const Conf::MappingNode *node = dynamic_cast<const Conf::MappingNode *>(item);
        std::string accountType;
        node->getValue("type", &accountType);

        std::string accountid;
        node->getValue("id", &accountid);

        std::string accountAlias;
        node->getValue("alias", &accountAlias);

        if (!accountid.empty() and !accountAlias.empty() and accountid != SIPAccount::IP2IP_PROFILE) {
            Account *a;
#if HAVE_IAX
            if (accountType == "IAX")
                a = new IAXAccount(accountid);
            else // assume SIP
#endif
                a = new SIPAccount(accountid);

            accountMap[accountid] = a;
            a->unserialize(node);
        }
    }

    void unloadAccount(std::pair<const std::string, Account*> &item)
    {
        // avoid deleting IP2IP account twice
        if (not item.first.empty()) {
            delete item.second;
            item.second = 0;
        }
    }
} // end anonymous namespace

void ManagerImpl::loadAccountMap(Conf::YamlParser &parser)
{
    using namespace Conf;
    // build a default IP2IP account with default parameters
    accountMap_[SIPAccount::IP2IP_PROFILE] = new SIPAccount(SIPAccount::IP2IP_PROFILE);

    // load saved preferences for IP2IP account from configuration file
    Sequence *seq = parser.getAccountSequence()->getSequence();
    Sequence::const_iterator ip2ip = std::find_if(seq->begin(), seq->end(), isIP2IP);
    if (ip2ip != seq->end()) {
        MappingNode *node = dynamic_cast<MappingNode*>(*ip2ip);
        accountMap_[SIPAccount::IP2IP_PROFILE]->unserialize(node);
    }

    // Initialize default UDP transport according to
    // IP to IP settings (most likely using port 5060)
    SIPVoIPLink::instance()->createDefaultSipUdpTransport();

    // Force IP2IP settings to be loaded to be loaded
    // No registration in the sense of the REGISTER method is performed.
    accountMap_[SIPAccount::IP2IP_PROFILE]->registerVoIPLink();

    // build preferences
    preferences.unserialize(parser.getPreferenceNode());
    voipPreferences.unserialize(parser.getVoipPreferenceNode());
    addressbookPreference.unserialize(parser.getAddressbookNode());
    hookPreference.unserialize(parser.getHookNode());
    audioPreference.unserialize(parser.getAudioNode());
    shortcutPreferences.unserialize(parser.getShortcutNode());

    using namespace std::tr1; // for std::tr1::bind and std::tr1::ref
    using namespace std::tr1::placeholders;
    // Each valid account element in sequence is a new account to load
    std::for_each(seq->begin(), seq->end(), bind(loadAccount, _1, ref(accountMap_)));
}

void ManagerImpl::unloadAccountMap()
{
    std::for_each(accountMap_.begin(), accountMap_.end(), unloadAccount);
    accountMap_.clear();
}

bool ManagerImpl::accountExists(const std::string &accountID)
{
    return accountMap_.find(accountID) != accountMap_.end();
}

SIPAccount*
ManagerImpl::getIP2IPAccount()
{
    return static_cast<SIPAccount*>(accountMap_[SIPAccount::IP2IP_PROFILE]);
}

Account*
ManagerImpl::getAccount(const std::string& accountID)
{
    AccountMap::const_iterator iter = accountMap_.find(accountID);
    if (iter != accountMap_.end())
        return iter->second;
    else
        return accountMap_[SIPAccount::IP2IP_PROFILE];
}

std::string ManagerImpl::getAccountIdFromNameAndServer(const std::string& userName, const std::string& server) const
{
    DEBUG("Manager : username = %s, server = %s", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    for (AccountMap::const_iterator iter = accountMap_.begin(); iter != accountMap_.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *>(iter->second);

        if (account and account->isEnabled() and account->fullMatch(userName, server)) {
            DEBUG("Manager: Matching account id in request is a fullmatch %s@%s", userName.c_str(), server.c_str());
            return iter->first;
        }
    }

    // We failed! Then only match the hostname
    for (AccountMap::const_iterator iter = accountMap_.begin(); iter != accountMap_.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *>(iter->second);

        if (account and account->isEnabled() and account->hostnameMatch(server)) {
            DEBUG("Manager: Matching account id in request with hostname %s", server.c_str());
            return iter->first;
        }
    }

    // We failed! Then only match the username
    for (AccountMap::const_iterator iter = accountMap_.begin(); iter != accountMap_.end(); ++iter) {
        SIPAccount *account = dynamic_cast<SIPAccount *>(iter->second);

        if (account and account->isEnabled() and account->userMatch(userName)) {
            DEBUG("Manager: Matching account id in request with username %s", userName.c_str());
            return iter->first;
        }
    }

    DEBUG("Manager: Username %s or server %s doesn't match any account, using IP2IP", userName.c_str(), server.c_str());

    return "";
}

std::map<std::string, int32_t> ManagerImpl::getAddressbookSettings() const
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

void ManagerImpl::setAddressbookSettings(const std::map<std::string, int32_t>& settings)
{
    addressbookPreference.setEnabled(settings.find("ADDRESSBOOK_ENABLE")->second == 1);
    addressbookPreference.setMaxResults(settings.find("ADDRESSBOOK_MAX_RESULTS")->second);
    addressbookPreference.setPhoto(settings.find("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")->second == 1);
    addressbookPreference.setBusiness(settings.find("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")->second == 1);
    addressbookPreference.setHone(settings.find("ADDRESSBOOK_DISPLAY_PHONE_HOME")->second == 1);
    addressbookPreference.setMobile(settings.find("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")->second == 1);
}

void ManagerImpl::setAddressbookList(const std::vector<std::string>& list)
{
    addressbookPreference.setList(ManagerImpl::serialize(list));
    saveConfig();
}

std::vector<std::string> ManagerImpl::getAddressbookList() const
{
    return unserialize(addressbookPreference.getList());
}

std::map<std::string, std::string> ManagerImpl::getHookSettings() const
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

void ManagerImpl::setHookSettings(const std::map<std::string, std::string>& settings)
{
    hookPreference.setIax2Enabled(settings.find("URLHOOK_IAX2_ENABLED")->second == "true");
    hookPreference.setNumberAddPrefix(settings.find("PHONE_NUMBER_HOOK_ADD_PREFIX")->second);
    hookPreference.setNumberEnabled(settings.find("PHONE_NUMBER_HOOK_ENABLED")->second == "true");
    hookPreference.setSipEnabled(settings.find("URLHOOK_SIP_ENABLED")->second == "true");
    hookPreference.setUrlCommand(settings.find("URLHOOK_COMMAND")->second);
    hookPreference.setUrlSipField(settings.find("URLHOOK_SIP_FIELD")->second);
}

void ManagerImpl::setIPToIPForCall(const std::string& callID, bool IPToIP)
{
    if (not isIPToIP(callID)) // no IPToIP calls with the same ID
        IPToIPMap_[callID] = IPToIP;
}

bool ManagerImpl::isIPToIP(const std::string& callID) const
{
    std::map<std::string, bool>::const_iterator iter = IPToIPMap_.find(callID);
    return iter != IPToIPMap_.end() and iter->second;
}

std::map<std::string, std::string> ManagerImpl::getCallDetails(const std::string &callID)
{
    // We need here to retrieve the call information attached to the call ID
    // To achieve that, we need to get the voip link attached to the call
    // But to achieve that, we need to get the account the call was made with

    // So first we fetch the account
    const std::string accountid(getAccountFromCall(callID));

    // Then the VoIP link this account is linked with (IAX2 or SIP)
    Call *call = NULL;

    if (Account *account = getAccount(accountid)) {
        VoIPLink *link = account->getVoIPLink();

        if (link)
            call = link->getCall(callID);
    }

    std::map<std::string, std::string> call_details;

    if (call) {
        std::ostringstream type;
        type << call->getCallType();
        call_details["ACCOUNTID"] = accountid;
        call_details["PEER_NUMBER"] = call->getPeerNumber();
        call_details["DISPLAY_NAME"] = call->getDisplayName();
        call_details["CALL_STATE"] = call->getStateStr();
        call_details["CALL_TYPE"] = type.str();
    } else {
        ERROR("Manager: Error: getCallDetails()");
        call_details["ACCOUNTID"] = "";
        call_details["PEER_NUMBER"] = "Unknown";
        call_details["PEER_NAME"] = "Unknown";
        call_details["DISPLAY_NAME"] = "Unknown";
        call_details["CALL_STATE"] = "UNKNOWN";
        call_details["CALL_TYPE"] = "0";
    }

    return call_details;
}

std::vector<std::map<std::string, std::string> > ManagerImpl::getHistory() const
{
    return history_.getSerialized();
}

namespace {
template <typename M, typename V>
void vectorFromMapKeys(const M &m, V &v)
{
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it)
        v.push_back(it->first);
}
}

std::vector<std::string> ManagerImpl::getCallList() const
{
    std::vector<std::string> v;
    vectorFromMapKeys(callAccountMap_, v);
    return v;
}

std::map<std::string, std::string> ManagerImpl::getConferenceDetails(
    const std::string& confID) const
{
    std::map<std::string, std::string> conf_details;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        conf_details["CONFID"] = confID;
        conf_details["CONF_STATE"] = iter_conf->second->getStateStr();
    }

    return conf_details;
}

std::vector<std::string> ManagerImpl::getConferenceList() const
{
    std::vector<std::string> v;
    vectorFromMapKeys(conferenceMap_, v);
    return v;
}

std::vector<std::string> ManagerImpl::getParticipantList(const std::string& confID) const
{
    std::vector<std::string> v;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        const ParticipantSet participants(iter_conf->second->getParticipantList());
        std::copy(participants.begin(), participants.end(), std::back_inserter(v));;
    } else
        WARN("Manager: Warning: Did not find conference %s", confID.c_str());

    return v;
}

void ManagerImpl::saveHistory()
{
    if (!history_.save())
        ERROR("Manager: could not save history!");
}

void ManagerImpl::clearHistory()
{
    history_.clear();
}
