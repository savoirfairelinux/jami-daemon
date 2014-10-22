/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include "logger.h"
#include "managerimpl.h"
#include "account_schema.h"

#include "fileutils.h"
#include "map_utils.h"
#include "account.h"
#if HAVE_DHT
#include "dht/dhtaccount.h"
#endif

#include "call_factory.h"

#include "sip/sip_utils.h"

#include "im/instant_messaging.h"

#include "numbercleaner.h"
#include "config/yamlparser.h"

#if HAVE_ALSA
#include "audio/alsa/alsalayer.h"
#endif

#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"
#include "audio/sound/dtmf.h"
#include "audio/ringbufferpool.h"
#include "history/history.h"
#include "manager.h"

#include "client/configurationmanager.h"
#include "client/callmanager.h"

#ifdef SFL_VIDEO
#include "client/videomanager.h"
#endif

#include "conference.h"

#include <cerrno>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>  // mkdir(2)
#include <memory>

using namespace sfl;

std::atomic_bool ManagerImpl::initialized = {false};

static void
copy_over(const std::string &srcPath, const std::string &destPath)
{
    std::ifstream src(srcPath.c_str());
    std::ofstream dest(destPath.c_str());
    dest << src.rdbuf();
    src.close();
    dest.close();
}

// Creates a backup of the file at "path" with a .bak suffix appended
static void
make_backup(const std::string &path)
{
    const std::string backup_path(path + ".bak");
    copy_over(path, backup_path);
}

// Restore last backup of the configuration file
static void
restore_backup(const std::string &path)
{
    const std::string backup_path(path + ".bak");
    copy_over(backup_path, path);
}

void
ManagerImpl::loadDefaultAccountMap()
{
    accountFactory_.initIP2IPAccount();
}

ManagerImpl::ManagerImpl() :
    preferences(), voipPreferences(),
    hookPreference(),  audioPreference(), shortcutPreferences(),
    hasTriedToRegister_(false), audioCodecFactory(), client_(),
    currentCallMutex_(), dtmfKey_(), dtmfBuf_(0, sfl::AudioFormat::MONO()),
    toneMutex_(), telephoneTone_(), audiofile_(), audioLayerMutex_(),
    waitingCalls_(), waitingCallsMutex_(), path_()
    , ringbufferpool_(new sfl::RingBufferPool)
    , callFactory(), conferenceMap_(), history_(),
    finished_(false), accountFactory_()
{
    // initialize random generator for call id
    srand(time(nullptr));
}

ManagerImpl::~ManagerImpl()
{}

bool
ManagerImpl::parseConfiguration()
{
    bool result = true;

    try {
        YAML::Node parsedFile = YAML::LoadFile(path_);
        const int error_count = loadAccountMap(parsedFile);

        if (error_count > 0) {
            WARN("Errors while parsing %s", path_.c_str());
            result = false;
        }
    } catch (const YAML::BadFile &e) {
        WARN("Could not open config file: creating default account map");
        loadDefaultAccountMap();
    }

    return result;
}

void
ManagerImpl::init(const std::string &config_file)
{
    // FIXME: this is no good
    initialized = true;

    path_ = config_file.empty() ? retrieveConfigPath() : config_file;
    DEBUG("Configuration file path: %s", path_.c_str());

    bool no_errors = true;

    // manager can restart without being recreated (android)
    finished_ = false;

    try {
        no_errors = parseConfiguration();
    } catch (const YAML::Exception &e) {
        ERROR("%s", e.what());
        no_errors = false;
    }

    // always back up last error-free configuration
    if (no_errors) {
        make_backup(path_);
    } else {
        // restore previous configuration
        WARN("Restoring last working configuration");

        try {
            // remove accounts from broken configuration
            removeAccounts();
            restore_backup(path_);
            parseConfiguration();
        } catch (const YAML::Exception &e) {
            ERROR("%s", e.what());
            WARN("Restoring backup failed, creating default account map");
            loadDefaultAccountMap();
        }
    }

    initAudioDriver();

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (audiodriver_) {
            {
                std::lock_guard<std::mutex> toneLock(toneMutex_);
                telephoneTone_.reset(new sfl::TelephoneTone(preferences.getZoneToneChoice(), audiodriver_->getSampleRate()));
            }
            dtmfKey_.reset(new sfl::DTMF(getRingBufferPool().getInternalSamplingRate()));
        }
    }

    history_.load(preferences.getHistoryLimit());
    registerAccounts();
}

void
ManagerImpl::setPath(const std::string &path)
{
    history_.setPath(path);
}

void
ManagerImpl::finish()
{
    if (finished_)
        return;

    finished_ = true;

    try {
        // Forbid call creation
        callFactory.forbid();

        // Hangup all remaining active calls
        DEBUG("Hangup %zu remaining call(s)", callFactory.callCount());
        for (const auto call : callFactory.getAllCalls())
            hangupCall(call->getCallId());
        callFactory.clear();

        // Save accounts config and call's history
        saveConfig();
        saveHistory();

        // Disconnect accounts, close link stacks and free allocated ressources
        unregisterAccounts();
        accountFactory_.clear();

        {
            std::lock_guard<std::mutex> lock(audioLayerMutex_);

            audiodriver_.reset();
        }
    } catch (const VoipLinkException &err) {
        ERROR("%s", err.what());
    }
}

bool
ManagerImpl::isCurrentCall(const Call& call) const
{
    return currentCall_.get() == &call;
}

bool
ManagerImpl::hasCurrentCall() const
{
    return static_cast<bool>(currentCall_);
}

std::shared_ptr<Call>
ManagerImpl::getCurrentCall() const
{
    return currentCall_;
}

const std::string
ManagerImpl::getCurrentCallId() const
{
    return currentCall_ ? currentCall_->getCallId() : "";
}

/**
 * Set current call ID to empty string
 */
void
ManagerImpl::unsetCurrentCall()
{
    currentCall_.reset();
}

/**
 * Switch of current call id
 * @param id The new callid
 */
void
ManagerImpl::switchCall(std::shared_ptr<Call> call)
{
    std::lock_guard<std::mutex> m(currentCallMutex_);
    DEBUG("----- Switch current call id to '%s' -----",
          call ? call->getCallId().c_str() : "<nullptr>");
    currentCall_ = call;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool
ManagerImpl::outgoingCall(const std::string& preferred_account_id,
                          const std::string& call_id,
                          const std::string& to,
                          const std::string& conf_id)
{
    if (call_id.empty()) {
        DEBUG("New outgoing call abort, missing callid");
        return false;
    }

    // Call ID must be unique
    if (isValidCall(call_id)) {
        ERROR("Call id already exists in outgoing call");
        return false;
    }

    DEBUG("New outgoing call %s to %s", call_id.c_str(), to.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    std::string prefix(hookPreference.getNumberAddPrefix());

    std::string to_cleaned(NumberCleaner::clean(to, prefix));

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        DEBUG("Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conference and this and is not a conference participant
        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id))
            onHoldCall(current_call_id);
        else if (isConference(current_call_id) and not isConferenceParticipant(call_id))
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
    }

    try {
        /* WARN: after this call the account_id is obsolete
         * as the factory may decide to use another account (like IP2IP).
         */
        DEBUG("New outgoing call to %s", to_cleaned.c_str());
        auto call = newOutgoingCall(call_id, to_cleaned, preferred_account_id);

        // try to reverse match the peer name using the cache
        if (call->getDisplayName().empty()) {
            const auto& name = history_.getNameFromHistory(call->getPeerNumber(),
                                                           call->getAccountId());
            const std::string pseudo_contact_name(name);
            if (not pseudo_contact_name.empty())
                call->setDisplayName(pseudo_contact_name);
        }
        switchCall(call);
        call->setConfId(conf_id);
    } catch (const VoipLinkException &e) {
        callFailure(call_id);
        ERROR("%s", e.what());
        return false;
    } catch (ost::Socket *) {
        callFailure(call_id);
        ERROR("Could not bind socket");
        return false;
    }

    return true;
}

//THREAD=Main : for outgoing Call
bool
ManagerImpl::answerCall(const std::string& call_id)
{
    bool result = true;

    auto call = getCallFromCallID(call_id);
    if (!call) {
        ERROR("Call %s is NULL", call_id.c_str());
        return false;
    }

    // If sflphone is ringing
    stopTone();

    // store the current call id
    std::string current_call_id(getCurrentCallId());

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        DEBUG("Currently conversing with %s", current_call_id.c_str());

        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id)) {
            DEBUG("Answer call: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall(current_call_id);
        } else if (isConference(current_call_id) and not isConferenceParticipant(call_id)) {
            // if we are talking to a conference and we are answering an incoming call
            DEBUG("Detach main participant from conference");
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
        }
    }

    try {
        call->answer();
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
        result = false;
    }

    // if it was waiting, it's waiting no more
    removeWaitingCall(call_id);

    // if we dragged this call into a conference already
    if (isConferenceParticipant(call_id))
        switchCall(callFactory.getCall(call->getConfId()));
    else
        switchCall(call);

    // Connect streams
    addStream(call_id);

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);

    client_.getCallManager()->callStateChanged(call_id, "CURRENT");

    return result;
}

void
ManagerImpl::checkAudio()
{
    if (getCallList().empty()) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        if (audiodriver_)
            audiodriver_->stopStream();
    }
}

//THREAD=Main
bool
ManagerImpl::hangupCall(const std::string& callId)
{
    // store the current call id
    std::string currentCallId(getCurrentCallId());

    stopTone();

    DEBUG("Send call state change (HUNGUP) for id %s", callId.c_str());
    client_.getCallManager()->callStateChanged(callId, "HUNGUP");

    /* We often get here when the call was hungup before being created */
    auto call = getCallFromCallID(callId);
    if (not call) {
        WARN("Could not hang up non-existant call %s", callId.c_str());
        checkAudio();
        return false;
    }

    // Disconnect streams
    removeStream(callId);

    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (not isConference(currentCallId))
            unsetCurrentCall();
    }

    try {
        history_.addCall(call.get(), preferences.getHistoryLimit());
        call->hangup(0);
        checkAudio();
        saveHistory();
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        return false;
    }

    return true;
}

bool
ManagerImpl::hangupConference(const std::string& id)
{
    DEBUG("Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end()) {
        auto conf = iter_conf->second;

        if (conf) {
            ParticipantSet participants(conf->getParticipantList());

            for (const auto &item : participants)
                hangupCall(item);
        } else {
            ERROR("No such conference %s", id.c_str());
            return false;
        }
    }

    unsetCurrentCall();

    return true;
}

//THREAD=Main
bool
ManagerImpl::onHoldCall(const std::string& callId)
{
    bool result = true;

    stopTone();

    std::string current_call_id(getCurrentCallId());

    try {
        if (auto call = getCallFromCallID(callId)) {
            call->onhold();
        } else {
            DEBUG("CallID %s doesn't exist in call onHold", callId.c_str());
            return false;
        }
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        result = false;
    }

    // Unbind calls in main buffer
    removeStream(callId);

    // Remove call from teh queue if it was still there
    removeWaitingCall(callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId)
        unsetCurrentCall();

    client_.getCallManager()->callStateChanged(callId, "HOLD");

    return result;
}

//THREAD=Main
bool
ManagerImpl::offHoldCall(const std::string& callId)
{
    bool result = true;

    stopTone();

    const std::string currentCallId(getCurrentCallId());

    // Place current call on hold if it isn't
    if (hasCurrentCall()) {
        if (not isConference(currentCallId) and not isConferenceParticipant(currentCallId)) {
            DEBUG("Has current call (%s), put on hold", currentCallId.c_str());
            onHoldCall(currentCallId);
        } else if (isConference(currentCallId) && callId != currentCallId) {
            holdConference(currentCallId);
        } else if (isConference(currentCallId) and not isConferenceParticipant(callId))
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
    }

    std::shared_ptr<Call> call;
    try {
        call = getCallFromCallID(callId);
        if (call)
            call->offhold();
        else
            result = false;
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        return false;
    }

    client_.getCallManager()->callStateChanged(callId, "UNHOLD");

    if (isConferenceParticipant(callId))
        switchCall(getCallFromCallID(call->getConfId()));
    else
        switchCall(call);

    addStream(callId);

    return result;
}

//THREAD=Main
bool
ManagerImpl::transferCall(const std::string& callId, const std::string& to)
{
    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else if (not isConference(getCurrentCallId()))
        unsetCurrentCall();

    if (auto call = getCallFromCallID(callId))
        call->transfer(to);
    else
        return false;

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall(callId);

    return true;
}

void
ManagerImpl::transferFailed()
{
    client_.getCallManager()->transferFailed();
}

void
ManagerImpl::transferSucceeded()
{
    client_.getCallManager()->transferSucceeded();
}

bool
ManagerImpl::attendedTransfer(const std::string& transferID,
                              const std::string& targetID)
{
    if (auto call = getCallFromCallID(transferID))
        return call->attendedTransfer(targetID);

    return false;
}

//THREAD=Main : Call:Incoming
bool
ManagerImpl::refuseCall(const std::string& id)
{
    auto call = getCallFromCallID(id);
    if (!call)
        return false;

    stopTone();

    if (getCallList().size() <= 1) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    call->refuse();

    checkAudio();

    removeWaitingCall(id);

    client_.getCallManager()->callStateChanged(id, "HUNGUP");

    // Disconnect streams
    removeStream(id);

    return true;
}

std::shared_ptr<Conference>
ManagerImpl::createConference(const std::string& id1, const std::string& id2)
{
    DEBUG("Create conference with call %s and %s", id1.c_str(), id2.c_str());

    auto conf = std::make_shared<Conference>();

    conf->add(id1);
    conf->add(id2);

    // Add conference to map
    conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));

    client_.getCallManager()->conferenceCreated(conf->getConfID());

    return conf;
}

void
ManagerImpl::removeConference(const std::string& conference_id)
{
    DEBUG("Remove conference %s", conference_id.c_str());
    DEBUG("number of participants: %u", conferenceMap_.size());
    ConferenceMap::iterator iter = conferenceMap_.find(conference_id);

    std::shared_ptr<Conference> conf;

    if (iter != conferenceMap_.end())
        conf = iter->second;

    if (not conf) {
        ERROR("Conference not found");
        return;
    }

    client_.getCallManager()->conferenceRemoved(conference_id);

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getRingBufferPool().unBindAll(sfl::RingBufferPool::DEFAULT_ID);

    ParticipantSet participants(conf->getParticipantList());

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getRingBufferPool().bindCallID(*iter_p, sfl::RingBufferPool::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (conferenceMap_.erase(conference_id))
        DEBUG("Conference %s removed successfully", conference_id.c_str());
    else
        ERROR("Cannot remove conference: %s", conference_id.c_str());
}

std::shared_ptr<Conference>
ManagerImpl::getConferenceFromCallID(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    if (!call)
        return nullptr;

    ConferenceMap::const_iterator iter(conferenceMap_.find(call->getConfId()));

    if (iter != conferenceMap_.end())
        return iter->second;
    else
        return nullptr;
}

bool
ManagerImpl::holdConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf == conferenceMap_.end())
        return false;

    auto conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                 conf->getState() == Conference::ACTIVE_DETACHED_REC or
                 conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (const auto &item : participants) {
        switchCall(getCallFromCallID(item));
        onHoldCall(item);
    }

    conf->setState(isRec ? Conference::HOLD_REC : Conference::HOLD);

    client_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());

    return true;
}

bool
ManagerImpl::unHoldConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf == conferenceMap_.end() or iter_conf->second == 0)
        return false;

    auto conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
        conf->getState() == Conference::ACTIVE_DETACHED_REC or
        conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (const auto &item : participants) {
        if (auto call = getCallFromCallID(item)) {
            // if one call is currently recording, the conference is in state recording
            isRec |= call->isRecording();

            switchCall(call);
            offHoldCall(item);
        }
    }

    conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);

    client_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());

    return true;
}

bool
ManagerImpl::isConference(const std::string& id) const
{
    return conferenceMap_.find(id) != conferenceMap_.end();
}

bool
ManagerImpl::isConferenceParticipant(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    return call and not call->getConfId().empty();
}

bool
ManagerImpl::addParticipant(const std::string& callId,
                            const std::string& conferenceId)
{
    DEBUG("Add participant %s to %s", callId.c_str(), conferenceId.c_str());
    ConferenceMap::iterator iter = conferenceMap_.find(conferenceId);

    if (iter == conferenceMap_.end()) {
        ERROR("Conference id is not valid");
        return false;
    }

    auto call = getCallFromCallID(callId);
    if (!call) {
        ERROR("Call id %s is not valid", callId.c_str());
        return false;
    }

    // ensure that calls are only in one conference at a time
    if (isConferenceParticipant(callId))
        detachParticipant(callId);

    // store the current call id (it will change in offHoldCall or in answerCall)
    std::string current_call_id(getCurrentCallId());

    // detach from prior communication and switch to this conference
    if (current_call_id != callId) {
        if (isConference(current_call_id))
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
        else
            onHoldCall(current_call_id);
    }

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    unsetCurrentCall();

    // Add main participant
    addMainParticipant(conferenceId);

    auto conf = iter->second;
    switchCall(getCallFromCallID(conf->getConfID()));

    // Add coresponding IDs in conf and call
    call->setConfId(conf->getConfID());
    conf->add(callId);

    // Connect new audio streams together
    getRingBufferPool().unBindAll(callId);

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
        ERROR("Participant list is empty for this conference");

    // Connect stream
    addStream(callId);
    return true;
}

bool
ManagerImpl::addMainParticipant(const std::string& conference_id)
{
    if (hasCurrentCall()) {
        std::string current_call_id(getCurrentCallId());

        if (isConference(current_call_id))
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
        else
            onHoldCall(current_call_id);
    }

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        ConferenceMap::const_iterator iter = conferenceMap_.find(conference_id);

        if (iter == conferenceMap_.end() or iter->second == 0)
            return false;

        auto conf = iter->second;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &item_p : participants) {
            getRingBufferPool().bindCallID(item_p, sfl::RingBufferPool::DEFAULT_ID);
            // Reset ringbuffer's readpointers
            getRingBufferPool().flush(item_p);
        }

        getRingBufferPool().flush(sfl::RingBufferPool::DEFAULT_ID);

        if (conf->getState() == Conference::ACTIVE_DETACHED)
            conf->setState(Conference::ACTIVE_ATTACHED);
        else if (conf->getState() == Conference::ACTIVE_DETACHED_REC)
            conf->setState(Conference::ACTIVE_ATTACHED_REC);
        else
            WARN("Invalid conference state while adding main participant");

        client_.getCallManager()->conferenceChanged(conference_id, conf->getStateStr());
    }

    switchCall(getCallFromCallID(conference_id));
    return true;
}

std::shared_ptr<Call>
ManagerImpl::getCallFromCallID(const std::string& callID)
{
    return callFactory.getCall(callID);
}

bool
ManagerImpl::joinParticipant(const std::string& callId1,
                             const std::string& callId2)
{
    if (callId1 == callId2) {
        ERROR("Cannot join participant %s to itself", callId1.c_str());
        return false;
    }

    // Set corresponding conference ids for call 1
    auto call1 = getCallFromCallID(callId1);
    if (!call1) {
        ERROR("Could not find call %s", callId1.c_str());
        return false;
    }

    // Set corresponding conderence details
    auto call2 = getCallFromCallID(callId2);
    if (!call2) {
        ERROR("Could not find call %s", callId2.c_str());
        return false;
    }

    // ensure that calls are only in one conference at a time
    if (isConferenceParticipant(callId1))
        detachParticipant(callId1);
    if (isConferenceParticipant(callId2))
        detachParticipant(callId2);

    std::map<std::string, std::string> call1Details(getCallDetails(callId1));
    std::map<std::string, std::string> call2Details(getCallDetails(callId2));

    std::string current_call_id(getCurrentCallId());
    DEBUG("Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ((current_call_id != callId1) and (current_call_id != callId2)) {
        // If currently in a conference
        if (isConference(current_call_id))
            detachParticipant(sfl::RingBufferPool::DEFAULT_ID);
        else
            onHoldCall(current_call_id); // currently in a call
    }


    auto conf = createConference(callId1, callId2);

    call1->setConfId(conf->getConfID());
    getRingBufferPool().unBindAll(callId1);

    call2->setConfId(conf->getConfID());
    getRingBufferPool().unBindAll(callId2);

    // Process call1 according to its state
    std::string call1_state_str(call1Details.find("CALL_STATE")->second);
    DEBUG("Process call %s state: %s", callId1.c_str(), call1_state_str.c_str());

    if (call1_state_str == "HOLD") {
        conf->bindParticipant(callId1);
        offHoldCall(callId1);
    } else if (call1_state_str == "INCOMING") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else if (call1_state_str == "CURRENT") {
        conf->bindParticipant(callId1);
    } else if (call1_state_str == "INACTIVE") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else
        WARN("Call state not recognized");

    // Process call2 according to its state
    std::string call2_state_str(call2Details.find("CALL_STATE")->second);
    DEBUG("Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

    if (call2_state_str == "HOLD") {
        conf->bindParticipant(callId2);
        offHoldCall(callId2);
    } else if (call2_state_str == "INCOMING") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else if (call2_state_str == "CURRENT") {
        conf->bindParticipant(callId2);
    } else if (call2_state_str == "INACTIVE") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else
        WARN("Call state not recognized");

    // Switch current call id to this conference
    switchCall(getCallFromCallID(conf->getConfID()));
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    conf->setRecordingFormat(ringbufferpool_->getInternalAudioFormat());

    return true;
}

void
ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        ERROR("Participant number must be higher or equal to 2");
        return;
    }

    auto conf = std::make_shared<Conference>();

    int successCounter = 0;

    for (const auto &p : participantList) {
        std::string numberaccount(p);
        std::string tostr(numberaccount.substr(0, numberaccount.find(",")));
        std::string account(numberaccount.substr(numberaccount.find(",") + 1, numberaccount.size()));

        std::string generatedCallID(getNewCallID());

        // Manager methods may behave differently if the call id participates in a conference
        conf->add(generatedCallID);

        unsetCurrentCall();

        // Create call
        bool callSuccess = outgoingCall(account, generatedCallID, tostr, conf->getConfID());

        // If not able to create call remove this participant from the conference
        if (!callSuccess)
            conf->remove(generatedCallID);
        else {
            client_.getCallManager()->newCallCreated(account, generatedCallID, tostr);
            successCounter++;
        }
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        conferenceMap_[conf->getConfID()] = conf;
        client_.getCallManager()->conferenceCreated(conf->getConfID());
        conf->setRecordingFormat(ringbufferpool_->getInternalAudioFormat());
    }
}

bool
ManagerImpl::detachParticipant(const std::string& call_id)
{
    const std::string current_call_id(getCurrentCallId());

    if (call_id != sfl::RingBufferPool::DEFAULT_ID) {
        auto call = getCallFromCallID(call_id);
        if (!call) {
            ERROR("Could not find call %s", call_id.c_str());
            return false;
        }

        auto conf = getConferenceFromCallID(call_id);

        if (conf == nullptr) {
            ERROR("Call is not conferencing, cannot detach");
            return false;
        }

        std::map<std::string, std::string> call_details(getCallDetails(call_id));
        std::map<std::string, std::string>::iterator iter_details(call_details.find("CALL_STATE"));

        if (iter_details == call_details.end()) {
            ERROR("Could not find CALL_STATE");
            return false;
        }

        // Don't hold ringing calls when detaching them from conferences
        if (iter_details->second != "RINGING")
            onHoldCall(call_id);

        removeParticipant(call_id);

    } else {
        DEBUG("Unbind main participant from conference %d");
        getRingBufferPool().unBindAll(sfl::RingBufferPool::DEFAULT_ID);

        if (not isConference(current_call_id)) {
            ERROR("Current call id (%s) is not a conference", current_call_id.c_str());
            return false;
        }

        ConferenceMap::iterator iter = conferenceMap_.find(current_call_id);

        auto conf = iter->second;
        if (iter == conferenceMap_.end() or conf == 0) {
            DEBUG("Conference is NULL");
            return false;
        }

        if (conf->getState() == Conference::ACTIVE_ATTACHED)
            conf->setState(Conference::ACTIVE_DETACHED);
        else if (conf->getState() == Conference::ACTIVE_ATTACHED_REC)
            conf->setState(Conference::ACTIVE_DETACHED_REC);
        else
            WARN("Undefined behavior, invalid conference state in detach participant");

        client_.getCallManager()->conferenceChanged(conf->getConfID(),
                                                  conf->getStateStr());

        unsetCurrentCall();
    }

    return true;
}

void
ManagerImpl::removeParticipant(const std::string& call_id)
{
    DEBUG("Remove participant %s", call_id.c_str());

    // this call is no longer a conference participant
    auto call = getCallFromCallID(call_id);
    if (!call) {
        ERROR("Call not found");
        return;
    }

    ConferenceMap::const_iterator iter = conferenceMap_.find(call->getConfId());

    auto conf = iter->second;
    if (iter == conferenceMap_.end() or conf == 0) {
        ERROR("No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    conf->remove(call_id);
    call->setConfId("");

    removeStream(call_id);

    client_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());

    processRemainingParticipants(*conf);
}

void
ManagerImpl::processRemainingParticipants(Conference &conf)
{
    const std::string current_call_id(getCurrentCallId());
    ParticipantSet participants(conf.getParticipantList());
    const size_t n = participants.size();
    DEBUG("Process remaining %d participant(s) from conference %s",
          n, conf.getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto &p : participants)
            getRingBufferPool().flush(p);

        getRingBufferPool().flush(sfl::RingBufferPool::DEFAULT_ID);
    } else if (n == 1) {
        // this call is the last participant, hence
        // the conference is over
        ParticipantSet::iterator p = participants.begin();

        if (auto call = getCallFromCallID(*p)) {
            call->setConfId("");
            // if we are not listening to this conference
            if (current_call_id != conf.getConfID())
                onHoldCall(call->getCallId());
            else
                switchCall(call);
        }

        DEBUG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
    } else {
        DEBUG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
        unsetCurrentCall();
    }
}

bool
ManagerImpl::joinConference(const std::string& conf_id1,
                            const std::string& conf_id2)
{
    if (conferenceMap_.find(conf_id1) == conferenceMap_.end()) {
        ERROR("Not a valid conference ID: %s", conf_id1.c_str());
        return false;
    }

    if (conferenceMap_.find(conf_id2) == conferenceMap_.end()) {
        ERROR("Not a valid conference ID: %s", conf_id2.c_str());
        return false;
    }

    auto conf = conferenceMap_.find(conf_id1)->second;
    ParticipantSet participants(conf->getParticipantList());

    for (const auto &p : participants)
        addParticipant(p, conf_id2);

    return true;
}

void
ManagerImpl::addStream(const std::string& call_id)
{
    DEBUG("Add audio stream %s", call_id.c_str());
    auto call = getCallFromCallID(call_id);
    if (call and isConferenceParticipant(call_id)) {
        DEBUG("Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = conferenceMap_.find(call->getConfId());

        if (iter != conferenceMap_.end() and iter->second) {
            auto conf = iter->second;

            conf->bindParticipant(call_id);
        }

    } else {
        DEBUG("Add stream to call");

        // bind to main
        getRingBufferPool().bindCallID(call_id, sfl::RingBufferPool::DEFAULT_ID);

        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        if (!audiodriver_) {
            ERROR("Audio driver not initialized");
            return;
        }
        audiodriver_->flushUrgent();
        audiodriver_->flushMain();
    }
}

void
ManagerImpl::removeStream(const std::string& call_id)
{
    DEBUG("Remove audio stream %s", call_id.c_str());
    getRingBufferPool().unBindAll(call_id);
}

void
ManagerImpl::registerEventHandler(uintptr_t handlerId, EventHandler handler)
{
    eventHandlerMap_.insert(std::make_pair(handlerId, handler));
}

void
ManagerImpl::unregisterEventHandler(uintptr_t handlerId)
{
    eventHandlerMap_.erase(handlerId);
}

// Must be invoked periodically by a timer from the main event loop
void ManagerImpl::pollEvents()
{
    if (finished_)
        return;

    // Make a copy of handlers map as handlers can modify this map
    const auto handlers = eventHandlerMap_;
    for (const auto& it : handlers)
        it.second();
}

//THREAD=Main
void
ManagerImpl::saveConfig()
{
    DEBUG("Saving Configuration to XDG directory %s", path_.c_str());

    if (audiodriver_) {
        audioPreference.setVolumemic(audiodriver_->getCaptureGain());
        audioPreference.setVolumespkr(audiodriver_->getPlaybackGain());
        audioPreference.setCaptureMuted(audiodriver_->isCaptureMuted());
        audioPreference.setPlaybackMuted(audiodriver_->isPlaybackMuted());
    }

    try {
        YAML::Emitter out;

        // FIXME maybe move this into accountFactory?
        out << YAML::BeginMap << YAML::Key << "accounts" << YAML::BeginSeq;

        for (const auto& account : accountFactory_.getAllAccounts()) {
            account->serialize(out);
        }
        out << YAML::EndSeq;

        // FIXME: this is a hack until we get rid of accountOrder
        preferences.verifyAccountOrder(getAccountList());
        preferences.serialize(out);
        voipPreferences.serialize(out);
        hookPreference.serialize(out);
        audioPreference.serialize(out);
#ifdef SFL_VIDEO
        getVideoManager()->getVideoDeviceMonitor().serialize(out);
#endif
        shortcutPreferences.serialize(out);

        std::ofstream fout(path_);
        fout << out.c_str();
    } catch (const YAML::Exception &e) {
        ERROR("%s", e.what());
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }
}

//THREAD=Main
void
ManagerImpl::sendDtmf(const std::string& id, char code)
{
    playDtmf(code);

    // return if we're not "in" a call
    if (id.empty())
        return;

    if (auto call = getCallFromCallID(id))
        call->carryingDTMFdigits(code);
}

//THREAD=Main | VoIPLink
void
ManagerImpl::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        DEBUG("Do not have to play a tone...");
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        DEBUG("Pulse length is not set...");
        return;
    }

    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

    // fast return, no sound, so no dtmf
    if (not audiodriver_ or not dtmfKey_) {
        DEBUG("No audio layer...");
        return;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    int size = (int)((pulselen * (float) audiodriver_->getSampleRate()) / 1000);
    dtmfBuf_.resize(size);

    // Handle dtmf
    dtmfKey_->startTone(code);

    // copy the sound
    if (dtmfKey_->generateDTMF(*dtmfBuf_.getChannel(0))) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();
        audiodriver_->startStream();

        // FIXME: do real synchronization
        int tries = 10;
        while (not audiodriver_->isStarted() and tries--) {
            WARN("Audio layer not ready yet");
            usleep(10000);
        }
        audiodriver_->putUrgent(dtmfBuf_);
    }

    // TODO Cache the DTMF
}

// Multi-thread
bool
ManagerImpl::incomingCallsWaiting()
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    return not waitingCalls_.empty();
}

void
ManagerImpl::addWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.insert(id);
}

void
ManagerImpl::removeWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.erase(id);
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
void
ManagerImpl::incomingCall(Call &call, const std::string& accountId)
{
    stopTone();
    const std::string callID(call.getCallId());

    if (accountId.empty())
        call.setIPToIP(true);
    else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber(call.getPeerNumber());

        const char SIP_PREFIX[] = "sip:";
        size_t startIndex = peerNumber.find(SIP_PREFIX);

        if (startIndex != std::string::npos)
            call.setPeerNumber(peerNumber.substr(startIndex + sizeof(SIP_PREFIX) - 1));
    }

    if (not hasCurrentCall()) {
        call.setConnectionState(Call::RINGING);
        playRingtone(accountId);
    }

    addWaitingCall(callID);

    std::string number(call.getPeerNumber());

    std::string from("<" + number + ">");

    client_.getCallManager()->incomingCall(accountId, callID, call.getDisplayName() + " " + from);
}

//THREAD=VoIP
#if HAVE_INSTANT_MESSAGING
void
ManagerImpl::incomingMessage(const std::string& callID,
                             const std::string& from,
                             const std::string& message)
{
    if (isConferenceParticipant(callID)) {
        auto conf = getConferenceFromCallID(callID);

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &item_p : participants) {

            if (item_p == callID)
                continue;

            DEBUG("Send message to %s", item_p.c_str());

            if (auto call = getCallFromCallID(item_p)) {
                call->sendTextMessage(message, from);
            } else {
                ERROR("Failed to get call while sending instant message");
                return;
            }
        }

        // in case of a conference we must notify client using conference id
        client_.getCallManager()->incomingMessage(conf->getConfID(), from, message);

    } else
        client_.getCallManager()->incomingMessage(callID, from, message);
}

//THREAD=VoIP
bool
ManagerImpl::sendTextMessage(const std::string& callID,
                             const std::string& message,
                             const std::string& from)
{
    if (isConference(callID)) {
        DEBUG("Is a conference, send instant message to everyone");
        ConferenceMap::iterator it = conferenceMap_.find(callID);

        if (it == conferenceMap_.end())
            return false;

        auto conf = it->second;

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &participant_id : participants) {

            if (auto call = getCallFromCallID(participant_id)) {
                call->sendTextMessage(message, from);
            } else {
                ERROR("Failed to get call while sending instant message");
                return false;
            }
        }

        return true;
    }

    if (isConferenceParticipant(callID)) {
        DEBUG("Call is participant in a conference, send instant message to everyone");
        auto conf = getConferenceFromCallID(callID);

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &participant_id : participants) {

            if (auto call = getCallFromCallID(participant_id)) {
                call->sendTextMessage(message, from);
            } else {
                ERROR("Failed to get call while sending instant message");
                return false;
            }
        }
    } else {
        if (auto call = getCallFromCallID(callID)) {
            call->sendTextMessage(message, from);
        } else {
            ERROR("Failed to get call while sending instant message");
            return false;
        }
    }
    return true;
}
#endif // HAVE_INSTANT_MESSAGING

//THREAD=VoIP CALL=Outgoing
void
ManagerImpl::peerAnsweredCall(const std::string& id)
{
    auto call = getCallFromCallID(id);
    if (!call) return;
    DEBUG("Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(*call))
        stopTone();

    // Connect audio streams
    addStream(id);

    if (audiodriver_) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->flushMain();
        audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(id);

    client_.getCallManager()->callStateChanged(id, "CURRENT");
}

//THREAD=VoIP Call=Outgoing
void
ManagerImpl::peerRingingCall(const std::string& id)
{
    auto call = getCallFromCallID(id);
    if (!call) return;
    DEBUG("Peer call %s ringing", id.c_str());

    if (isCurrentCall(*call))
        ringback();

    client_.getCallManager()->callStateChanged(id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void
ManagerImpl::peerHungupCall(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    if (!call) return;

    DEBUG("Peer hungup call %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        removeParticipant(call_id);
    } else if (isCurrentCall(*call)) {
        stopTone();
        unsetCurrentCall();
    }

    history_.addCall(call.get(), preferences.getHistoryLimit());
    call->peerHungup();
    saveHistory();

    client_.getCallManager()->callStateChanged(call_id, "HUNGUP");

    checkAudio();
    removeWaitingCall(call_id);
    if (not incomingCallsWaiting())
        stopTone();

    removeStream(call_id);
}

//THREAD=VoIP
void
ManagerImpl::callBusy(const std::string& id)
{
    auto call = getCallFromCallID(id);
    if (!call) return;

    client_.getCallManager()->callStateChanged(id, "BUSY");

    if (isCurrentCall(*call)) {
        playATone(sfl::Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    checkAudio();
    removeWaitingCall(id);
}

//THREAD=VoIP
void
ManagerImpl::callFailure(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    if (!call) return;

    client_.getCallManager()->callStateChanged(call_id, "FAILURE");

    if (isCurrentCall(*call)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    if (isConferenceParticipant(call_id)) {
        DEBUG("Call %s participating in a conference failed", call_id.c_str());
        // remove this participant
        removeParticipant(call_id);
    }

    checkAudio();
    removeWaitingCall(call_id);
}

//THREAD=VoIP
void
ManagerImpl::startVoiceMessageNotification(const std::string& accountId,
                                           int nb_msg)
{
    client_.getCallManager()->voiceMailNotify(accountId, nb_msg);
}

/**
 * Multi Thread
 */
void
ManagerImpl::playATone(Tone::TONEID toneId)
{
    if (not voipPreferences.getPlayTones())
        return;

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_) {
            ERROR("Audio layer not initialized");
            return;
        }

        audiodriver_->flushUrgent();
        audiodriver_->startStream();
    }

    {
        std::lock_guard<std::mutex> lock(toneMutex_);
        if (telephoneTone_)
            telephoneTone_->setCurrentTone(toneId);
    }
}

/**
 * Multi Thread
 */
void
ManagerImpl::stopTone()
{
    if (not voipPreferences.getPlayTones())
        return;

    std::lock_guard<std::mutex> lock(toneMutex_);
    if (telephoneTone_)
        telephoneTone_->setCurrentTone(Tone::TONE_NULL);

    if (audiofile_) {
        std::string filepath(audiofile_->getFilePath());

        client_.getCallManager()->recordPlaybackStopped(filepath);
        audiofile_.reset();
    }
}

/**
 * Multi Thread
 */
void
ManagerImpl::playTone()
{
    playATone(Tone::TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void
ManagerImpl::playToneWithMessage()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
ManagerImpl::congestion()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
ManagerImpl::ringback()
{
    playATone(Tone::TONE_RINGTONE);
}

// Caller must hold toneMutex
void
ManagerImpl::updateAudioFile(const std::string &file, int sampleRate)
{
    audiofile_.reset(new AudioFile(file, sampleRate));
}

/**
 * Multi Thread
 */
void
ManagerImpl::playRingtone(const std::string& accountID)
{
    const auto account = getAccount(accountID);

    if (!account) {
        WARN("Invalid account in ringtone");
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

    int audioLayerSmplr = 8000;
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_) {
            ERROR("no audio layer in ringtone");
            return;
        }

        audioLayerSmplr = audiodriver_->getSampleRate();
    }

    bool doFallback = false;

    {
        std::lock_guard<std::mutex> m(toneMutex_);

        if (audiofile_) {
            client_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
            audiofile_.reset();
        }

        try {
            updateAudioFile(ringchoice, audioLayerSmplr);
        } catch (const AudioFileException &e) {
            WARN("Ringtone error: %s", e.what());
            doFallback = true; // do ringback once lock is out of scope
        }
    } // leave mutex

    if (doFallback) {
        ringback();
        return;
    }

    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    // start audio if not started AND flush all buffers (main and urgent)
    audiodriver_->startStream();
}

AudioLoop*
ManagerImpl::getTelephoneTone()
{
    std::lock_guard<std::mutex> m(toneMutex_);
    if (telephoneTone_)
        return telephoneTone_->getCurrentTone();
    else
        return nullptr;
}

AudioLoop*
ManagerImpl::getTelephoneFile()
{
    std::lock_guard<std::mutex> m(toneMutex_);
    return audiofile_.get();
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 */
std::string
ManagerImpl::retrieveConfigPath() const
{
#ifdef __ANDROID__
    std::string configdir = "/data/data/org.sflphone";
#else
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR +
                            ".config" + DIR_SEPARATOR_STR + PACKAGE;
#endif

    const std::string xdg_env(XDG_CONFIG_HOME);
    if (not xdg_env.empty())
        configdir = xdg_env + DIR_SEPARATOR_STR + PACKAGE;

    if (mkdir(configdir.data(), 0700) != 0) {
        // If directory creation failed
        if (errno != EEXIST)
            DEBUG("Cannot create directory: %s!", configdir.c_str());
    }

    static const char * const PROGNAME = "sflphoned";
    return configdir + DIR_SEPARATOR_STR + PROGNAME + ".yml";
}

/**
 * Set input audio plugin
 */
void
ManagerImpl::setAudioPlugin(const std::string& audioPlugin)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    audioPreference.setAlsaPlugin(audioPlugin);

    bool wasStarted = audiodriver_->isStarted();

    // Recreate audio driver with new settings
    audiodriver_.reset(audioPreference.createAudioLayer());

    if (audiodriver_ and wasStarted)
        audiodriver_->startStream();
    else
        ERROR("No audio layer created, possibly built without audio support");
}

/**
 * Set audio output device
 */
void
ManagerImpl::setAudioDevice(int index, DeviceType type)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (not audiodriver_) {
        ERROR("Audio driver not initialized");
        return ;
    }

    const bool wasStarted = audiodriver_->isStarted();
    audiodriver_->updatePreference(audioPreference, index, type);

    // Recreate audio driver with new settings
    audiodriver_.reset(audioPreference.createAudioLayer());

    if (audiodriver_ and wasStarted)
        audiodriver_->startStream();
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string>
ManagerImpl::getAudioOutputDeviceList()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    return audiodriver_->getPlaybackDeviceList();
}

/**
 * Get list of supported audio input device
 */
std::vector<std::string>
ManagerImpl::getAudioInputDeviceList()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    return audiodriver_->getCaptureDeviceList();
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string>
ManagerImpl::getCurrentAudioDevicesIndex()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    std::vector<std::string> v;

    std::stringstream ssi, sso, ssr;
    sso << audiodriver_->getIndexPlayback();
    v.push_back(sso.str());
    ssi << audiodriver_->getIndexCapture();
    v.push_back(ssi.str());
    ssr << audiodriver_->getIndexRingtone();
    v.push_back(ssr.str());

    return v;
}

int
ManagerImpl::isRingtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void
ManagerImpl::ringtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled(false) : account->setRingtoneEnabled(true);
}

bool
ManagerImpl::getIsAlwaysRecording() const
{
    return audioPreference.getIsAlwaysRecording();
}

void
ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
    return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

bool
ManagerImpl::toggleRecordingCall(const std::string& id)
{
    std::shared_ptr<Recordable> rec;

    ConferenceMap::const_iterator it(conferenceMap_.find(id));
    if (it == conferenceMap_.end()) {
        DEBUG("toggle recording for call %s", id.c_str());
        rec = getCallFromCallID(id);
    } else {
        DEBUG("toggle recording for conference %s", id.c_str());
        auto conf = it->second;
        if (conf) {
            rec = conf;
            if (conf->isRecording())
                conf->setState(Conference::ACTIVE_ATTACHED);
            else
                conf->setState(Conference::ACTIVE_ATTACHED_REC);
        }
    }

    if (!rec) {
        ERROR("Could not find recordable instance %s", id.c_str());
        return false;
    }

    const bool result = rec->toggleRecording();
    client_.getCallManager()->recordPlaybackFilepath(id, rec->getFilename());
    client_.getCallManager()->recordingStateChanged(id, result);
    return result;
}

bool
ManagerImpl::isRecording(const std::string& id)
{
    auto call = getCallFromCallID(id);
    return call and (static_cast<Recordable*>(call.get()))->isRecording();
}

bool
ManagerImpl::startRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Start recorded file playback %s", filepath.c_str());

    int sampleRate;
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_) {
            ERROR("No audio layer in start recorded file playback");
            return false;
        }

        sampleRate = audiodriver_->getSampleRate();
    }

    {
        std::lock_guard<std::mutex> m(toneMutex_);

        if (audiofile_) {
            client_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
            audiofile_.reset();
        }

        try {
            updateAudioFile(filepath, sampleRate);
            if (not audiofile_)
                return false;
        } catch (const AudioFileException &e) {
            WARN("Audio file error: %s", e.what());
            return false;
        }
    } // release toneMutex

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->startStream();
    }

    return true;
}

void ManagerImpl::recordingPlaybackSeek(const double value)
{
    std::lock_guard<std::mutex> m(toneMutex_);
    if (audiofile_)
        audiofile_.get()->seek(value);
}

void ManagerImpl::stopRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Stop recorded file playback %s", filepath.c_str());

    checkAudio();

    {
        std::lock_guard<std::mutex> m(toneMutex_);
        audiofile_.reset();
    }
    client_.getCallManager()->recordPlaybackStopped(filepath);
}

void ManagerImpl::setHistoryLimit(int days)
{
    DEBUG("Set history limit");
    preferences.setHistoryLimit(days);
    saveConfig();
}

int
ManagerImpl::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

bool
ManagerImpl::setAudioManager(const std::string &api)
{
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_)
            return false;

        if (api == audioPreference.getAudioApi()) {
            DEBUG("Audio manager chosen already in use. No changes made. ");
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        bool wasStarted = audiodriver_->isStarted();
        audioPreference.setAudioApi(api);
        audiodriver_.reset(audioPreference.createAudioLayer());

        if (audiodriver_ and wasStarted)
            audiodriver_->startStream();
    }

    saveConfig();

    // ensure that we completed the transition (i.e. no fallback was used)
    return api == audioPreference.getAudioApi();
}

std::string
ManagerImpl::getAudioManager() const
{
    return audioPreference.getAudioApi();
}

int
ManagerImpl::getAudioInputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (not audiodriver_) {
        ERROR("Audio layer not initialized");
        return 0;
    }

    return audiodriver_->getAudioDeviceIndex(name, DeviceType::CAPTURE);
}

int
ManagerImpl::getAudioOutputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (not audiodriver_) {
        ERROR("Audio layer not initialized");
        return 0;
    }

    return audiodriver_->getAudioDeviceIndex(name, DeviceType::PLAYBACK);
}

std::string
ManagerImpl::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getAlsaPlugin();
}

bool
ManagerImpl::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce();
}

void
ManagerImpl::setNoiseSuppressState(bool state)
{
    audioPreference.setNoiseReduce(state);
}

bool
ManagerImpl::isAGCEnabled() const
{
    return audioPreference.isAGCEnabled();
}

void
ManagerImpl::setAGCState(bool state)
{
    audioPreference.setAGCState(state);
}

/**
 * Initialization: Main Thread
 */
void
ManagerImpl::initAudioDriver()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    audiodriver_.reset(audioPreference.createAudioLayer());
}

void
ManagerImpl::hardwareAudioFormatChanged(AudioFormat format)
{
    audioFormatUsed(format);
}

void
ManagerImpl::audioFormatUsed(AudioFormat format)
{
    AudioFormat currentFormat = ringbufferpool_->getInternalAudioFormat();
    format.nb_channels = std::max(currentFormat.nb_channels, std::min(format.nb_channels, 2u)); // max 2 channels.
    format.sample_rate = std::max(currentFormat.sample_rate, format.sample_rate);

    if (currentFormat == format)
        return;

    DEBUG("Audio format changed: %s -> %s", currentFormat.toString().c_str(), format.toString().c_str());

    ringbufferpool_->setInternalAudioFormat(format);

    {
        std::lock_guard<std::mutex> toneLock(toneMutex_);
        telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), format.sample_rate));
    }
    dtmfKey_.reset(new DTMF(format.sample_rate));
}

void
ManagerImpl::setAccountsOrder(const std::string& order)
{
    DEBUG("Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder(order);

    saveConfig();
}

std::vector<std::string>
ManagerImpl::getAccountList() const
{
    // TODO: this code looks weird. need further investigation!

    using std::vector;
    using std::string;
    vector<string> account_order(loadAccountOrder());

    // The IP2IP profile is always available, and first in the list

    vector<string> v;

    // Concatenate all account pointers in a single map
    const auto& allAccounts = accountFactory_.getAllAccounts();

    // If no order has been set, load the default one ie according to the creation date.
    if (account_order.empty()) {
        for (const auto &account : allAccounts) {
            if (account->isIP2IP())
                continue;
            v.push_back(account->getAccountID());
        }
    } else {
        const auto& ip2ipAccountID = getIP2IPAccount()->getAccountID();
        for (const auto& id : account_order) {
            if (id.empty() or id == ip2ipAccountID)
                continue;

            if (accountFactory_.hasAccount(id))
                v.push_back(id);
        }
    }

    if (const auto& account = getIP2IPAccount())
        v.push_back(account->getAccountID());
    else
        ERROR("could not find IP2IP profile in getAccount list");

    return v;
}

std::map<std::string, std::string>
ManagerImpl::getAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getAccountDetails();
    } else {
        ERROR("Could not get account details on a non-existing accountID %s", accountID.c_str());
        // return an empty map since we can't throw an exception to D-Bus
        return std::map<std::string, std::string>();
    }
}

std::map<std::string, std::string>
ManagerImpl::getVolatileAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getVolatileAccountDetails();
    } else {
        ERROR("Could not get volatile account details on a non-existing accountID %s", accountID.c_str());
        return {{}};
    }
}


// method to reduce the if/else mess.
// Even better, switch to XML !

void
ManagerImpl::setAccountDetails(const std::string& accountID,
                               const std::map<std::string, std::string>& details)
{
    DEBUG("Set account details for %s", accountID.c_str());

    const auto account = getAccount(accountID);

    if (account == nullptr) {
        ERROR("Could not find account %s", accountID.c_str());
        return;
    }

    // Ignore if nothing has changed
    if (details == account->getAccountDetails())
        return;

    // Unregister before modifying any account information
    account->doUnregister([&](bool /* transport_free */) {
        account->setAccountDetails(details);
        // Serialize configuration to disk once it is done
        saveConfig();

        if (account->isEnabled())
            account->doRegister();
        else
            account->doUnregister();

        // Update account details to the client side
        client_.getConfigurationManager()->accountsChanged();
    });
}

std::string
ManagerImpl::addAccount(const std::map<std::string, std::string>& details)
{
    /** @todo Deal with both the accountMap_ and the Configuration */

    std::string newAccountID;

    const std::vector<std::string> accountList(getAccountList());

    do {
        std::stringstream accountID;
        accountID << "Account:" << rand();
        newAccountID = accountID.str();
    } while (std::find(accountList.begin(), accountList.end(), newAccountID)
             != accountList.end());

    // Get the type

    const char* accountType;
    if (details.find(CONFIG_ACCOUNT_TYPE) != details.end())
        accountType = (*details.find(CONFIG_ACCOUNT_TYPE)).second.c_str();
    else
        accountType = AccountFactory::DEFAULT_ACCOUNT_TYPE;

    DEBUG("Adding account %s", newAccountID.c_str());

    auto newAccount = accountFactory_.createAccount(accountType, newAccountID);
    if (!newAccount) {
        ERROR("Unknown %s param when calling addAccount(): %s",
              CONFIG_ACCOUNT_TYPE, accountType);
        return "";
    }

    newAccount->setAccountDetails(details);

    preferences.addAccount(newAccountID);

    newAccount->doRegister();

    saveConfig();

    client_.getConfigurationManager()->accountsChanged();

    return newAccountID;
}

void ManagerImpl::removeAccounts()
{
    for (const auto &acc : getAccountList())
        removeAccount(acc);
}

void ManagerImpl::removeAccount(const std::string& accountID)
{
    // Get it down and dying
    if (const auto& remAccount = getAccount(accountID)) {
        remAccount->doUnregister();
        accountFactory_.removeAccount(*remAccount);
    }

    preferences.removeAccount(accountID);

    saveConfig();

    client_.getConfigurationManager()->accountsChanged();
}

bool
ManagerImpl::isValidCall(const std::string& callID)
{
    return static_cast<bool>(getCallFromCallID(callID));
}

std::string
ManagerImpl::getNewCallID()
{
    std::ostringstream random_id("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (isValidCall(random_id.str())) {
        random_id.clear();
        random_id << "s";
        random_id << (unsigned) rand();
    }

    return random_id.str();
}

std::vector<std::string>
ManagerImpl::loadAccountOrder() const
{
    return Account::split_string(preferences.getAccountOrder());
}

void
ManagerImpl::loadAccount(const YAML::Node &node, int &errorCount,
                         const std::string &accountOrder)
{
    using namespace yaml_utils;
    std::string accountType;
    parseValue(node, "type", accountType);

    std::string accountid;
    parseValue(node, "id", accountid);

    std::string accountAlias;
    parseValue(node, "alias", accountAlias);
    const auto inAccountOrder = [&](const std::string & id) {
        return accountOrder.find(id + "/") != std::string::npos;
    };

    if (!accountid.empty() and !accountAlias.empty()) {
        const auto& ip2ipAccountID = getIP2IPAccount()->getAccountID();
        if (not inAccountOrder(accountid) and accountid != ip2ipAccountID) {
            WARN("Dropping account %s, which is not in account order", accountid.c_str());
        } else if (accountFactory_.isSupportedType(accountType.c_str())) {
            std::shared_ptr<Account> a;
            if (accountid != ip2ipAccountID)
                a = accountFactory_.createAccount(accountType.c_str(), accountid);
            else
                a = accountFactory_.getIP2IPAccount();
            if (a) {
                a->unserialize(node);
            } else {
                ERROR("Failed to create account type \"%s\"", accountType.c_str());
                ++errorCount;
            }
        } else {
            ERROR("Ignoring unknown account type \"%s\"", accountType.c_str());
            ++errorCount;
        }
    }
}

int
ManagerImpl::loadAccountMap(const YAML::Node &node)
{
    using namespace Conf;

    accountFactory_.initIP2IPAccount();

    // build preferences
    preferences.unserialize(node);
    voipPreferences.unserialize(node);
    hookPreference.unserialize(node);
    audioPreference.unserialize(node);
    shortcutPreferences.unserialize(node);

    int errorCount = 0;
    try {
#ifdef SFL_VIDEO
        VideoManager *controls(getVideoManager());
        controls->getVideoDeviceMonitor().unserialize(node);
#endif
    } catch (const YAML::Exception &e) {
        ERROR("%s: No video node in config file", e.what());
        ++errorCount;
    }

    const std::string accountOrder = preferences.getAccountOrder();

    // load saved preferences for IP2IP account from configuration file
    const auto &accountList = node["accounts"];

    for (auto &a : accountList) {
        loadAccount(a, errorCount, accountOrder);
    }

    return errorCount;
}

std::map<std::string, std::string>
ManagerImpl::getCallDetails(const std::string &callID)
{
    if (auto call = getCallFromCallID(callID)) {
        return call->getDetails();
    } else {
        ERROR("Call is NULL");
        // FIXME: is this even useful?
        return Call::getNullDetails();
    }
}

std::vector<std::map<std::string, std::string> >
ManagerImpl::getHistory()
{
    return history_.getSerialized();
}

std::vector<std::string>
ManagerImpl::getCallList() const
{
    return callFactory.getCallIDs();
}

std::map<std::string, std::string>
ManagerImpl::getConferenceDetails(
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

std::vector<std::string>
ManagerImpl::getConferenceList() const
{
    std::vector<std::string> v;
    map_utils::vectorFromMapKeys(conferenceMap_, v);
    return v;
}

std::vector<std::string>
ManagerImpl::getDisplayNames(const std::string& confID) const
{
    std::vector<std::string> v;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        return iter_conf->second->getDisplayNames();
    } else {
        WARN("Did not find conference %s", confID.c_str());
    }

    return v;
}

std::vector<std::string>
ManagerImpl::getParticipantList(const std::string& confID) const
{
    std::vector<std::string> v;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        const ParticipantSet participants(iter_conf->second->getParticipantList());
        std::copy(participants.begin(), participants.end(), std::back_inserter(v));;
    } else
        WARN("Did not find conference %s", confID.c_str());

    return v;
}

std::string
ManagerImpl::getConferenceId(const std::string& callID)
{
    if (auto call = getCallFromCallID(callID))
        return call->getConfId();

    ERROR("Call is NULL");
    return "";
}

void
ManagerImpl::saveHistory()
{
    if (!history_.save())
        ERROR("Could not save history!");
    else
        client_.getConfigurationManager()->historyChanged();
}

void
ManagerImpl::clearHistory()
{
    history_.clear();
}

void
ManagerImpl::startAudioDriverStream()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    if (!audiodriver_) {
        ERROR("Audio driver not initialized");
        return;
    }
    audiodriver_->startStream();
}

void
ManagerImpl::registerAccounts()
{
    auto allAccounts(getAccountList());

    for (auto &item : allAccounts) {
        const auto a = getAccount(item);

        if (!a)
            continue;

        a->loadConfig();

        if (a->isEnabled())
            a->doRegister();
    }
}

void
ManagerImpl::unregisterAccounts()
{
    for (const auto& account : getAllAccounts()) {
        if (account->isEnabled())
            account->doUnregister();
    }
}

void
ManagerImpl::sendRegister(const std::string& accountID, bool enable)
{
    const auto acc = getAccount(accountID);
    if (!acc)
        return;

    acc->setEnabled(enable);
    acc->loadConfig();

    Manager::instance().saveConfig();

    if (acc->isEnabled())
        acc->doRegister();
    else
        acc->doUnregister();
}

std::shared_ptr<AudioLayer>
ManagerImpl::getAudioDriver()
{
    return audiodriver_;
}

Client*
ManagerImpl::getClient()
{
    return &client_;
}

#ifdef SFL_VIDEO
VideoManager *
ManagerImpl::getVideoManager()
{
    return client_.getVideoManager();
}
#endif

std::shared_ptr<Call>
ManagerImpl::newOutgoingCall(const std::string& id,
                             const std::string& toUrl,
                             const std::string& preferredAccountId)
{
    std::shared_ptr<Account> account = Manager::instance().getIP2IPAccount();
    std::string finalToUrl = toUrl;

#if HAVE_DHT
    if (toUrl.find("dht:") != std::string::npos) {
        WARN("DHT call detected");
        auto dhtAcc = getAllAccounts<DHTAccount>();
        if (not dhtAcc.empty())
            return dhtAcc.front()->newOutgoingCall(id, finalToUrl);
    }
#endif

    // FIXME: have a generic version to remove sip dependency
    sip_utils::stripSipUriPrefix(finalToUrl);

    if (!IpAddr::isValid(finalToUrl)) {
        account = getAccount(preferredAccountId);
        if (account)
            finalToUrl = toUrl;
        else
            WARN("Preferred account %s doesn't exist, using IP2IP account",
                 preferredAccountId.c_str());
    } else
        WARN("IP Url detected, using IP2IP account");

    if (!account) {
        ERROR("No suitable account found to create outgoing call");
        return nullptr;
    }

    return account->newOutgoingCall(id, finalToUrl);
}
