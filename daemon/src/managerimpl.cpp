/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "global.h"
#include "fileutils.h"
#include "map_utils.h"
#include "sip/sipvoiplink.h"
#include "sip/sipaccount.h"
#include "sip/sipcall.h"
#include "im/instant_messaging.h"
#include "sip/sippresence.h"

#if HAVE_IAX
#include "iax/iaxaccount.h"
#include "iax/iaxcall.h"
#include "iax/iaxvoiplink.h"
#endif

#include "numbercleaner.h"
#include "config/yamlparser.h"
#include "config/yamlemitter.h"

#if HAVE_ALSA
#include "audio/alsa/alsalayer.h"
#endif

#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"
#include "audio/sound/dtmf.h"
#include "history/historynamecache.h"
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

static void
loadDefaultAccountMap()
{
    SIPVoIPLink::instance().loadIP2IPSettings();
}

ManagerImpl::ManagerImpl() :
    preferences(), voipPreferences(),
    hookPreference(),  audioPreference(), shortcutPreferences(),
    hasTriedToRegister_(false), audioCodecFactory(), client_(),
	config_(),
    currentCallId_(), currentCallMutex_(), audiodriver_(nullptr), dtmfKey_(), dtmfBuf_(0, AudioFormat::MONO),
    toneMutex_(), telephoneTone_(), audiofile_(), audioLayerMutex_(),
    waitingCalls_(), waitingCallsMutex_(), path_(),
    IPToIPMap_(), mainBuffer_(), conferenceMap_(), history_(), finished_(false)
{
    // initialize random generator for call id
    srand(time(nullptr));
}

ManagerImpl::~ManagerImpl()
{
}

bool ManagerImpl::parseConfiguration()
{
    bool result = true;

    FILE *file = fopen(path_.c_str(), "rb");

    try {
        if (file) {
            Conf::YamlParser parser(file);
            parser.serializeEvents();
            parser.composeEvents();
            parser.constructNativeData();
            const int error_count = loadAccountMap(parser);
            fclose(file);
            if (error_count > 0) {
                WARN("Errors while parsing %s", path_.c_str());
                result = false;
            }
        } else {
            WARN("Config file not found: creating default account map");
            loadDefaultAccountMap();
        }
    } catch (const Conf::YamlParserException &e) {
        // we only want to close the local file here and then rethrow the exception
        fclose(file);
        throw;
    }

    return result;
}

void ManagerImpl::init(const std::string &config_file)
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
    } catch (const Conf::YamlParserException &e) {
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
        } catch (const Conf::YamlParserException &e) {
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
                telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), audiodriver_->getSampleRate()));
            }
            dtmfKey_.reset(new DTMF(getMainBuffer().getInternalSamplingRate()));
        }
    }

    history_.load(preferences.getHistoryLimit());
    registerAccounts();
}

void ManagerImpl::setPath(const std::string &path) {
	history_.setPath(path);
}

int ManagerImpl::run()
{
    DEBUG("Starting client event loop");

    client_.registerCallback(std::bind(&ManagerImpl::pollEvents, std::ref(*this)));

    return client_.event_loop();
}

int ManagerImpl::interrupt()
{
    return client_.exit();
}

void ManagerImpl::finish()
{
    if (finished_)
        return;

    finished_ = true;

    try {

        std::vector<std::string> callList(getCallList());
        DEBUG("Hangup %zu remaining call(s)", callList.size());

        for (const auto &item : callList)
            hangupCall(item);

        saveConfig();

        unregisterAccounts();

        SIPVoIPLink::destroy();
#if HAVE_IAX
        IAXVoIPLink::unloadAccountMap();
#endif

        {
            std::lock_guard<std::mutex> lock(audioLayerMutex_);

            delete audiodriver_;
            audiodriver_ = nullptr;
        }

        saveHistory();
    } catch (const VoipLinkException &err) {
        ERROR("%s", err.what());
    }
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

void ManagerImpl::unsetCurrentCall()
{
    switchCall("");
}

void ManagerImpl::switchCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(currentCallMutex_);
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
            detachParticipant(MainBuffer::DEFAULT_ID);
    }

    DEBUG("Selecting account %s", account_id.c_str());

    // fallback using the default sip account if the specied doesn't exist
    std::string use_account_id;
    if (!accountExists(account_id)) {
        WARN("Account does not exist, trying with default SIP account");
        use_account_id = SIPAccount::IP2IP_PROFILE;
    }
    else {
        use_account_id = account_id;
    }

    try {
        auto call = getAccountLink(account_id)->newOutgoingCall(call_id, to_cleaned, use_account_id);

        // try to reverse match the peer name using the cache
        if (call->getDisplayName().empty()) {
            const std::string pseudo_contact_name(HistoryNameCache::getInstance().getNameFromHistory(call->getPeerNumber(), call->getAccountId()));
            if (not pseudo_contact_name.empty())
                call->setDisplayName(pseudo_contact_name);
        }
        switchCall(call_id);
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
bool ManagerImpl::answerCall(const std::string& call_id)
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
            detachParticipant(MainBuffer::DEFAULT_ID);
        }
    }

    try {
        VoIPLink *link = getAccountLink(call->getAccountId());
        if (link)
            link->answer(call.get());
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
        result = false;
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

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);

    client_.getCallManager()->callStateChanged(call_id, "CURRENT");

    return result;
}

void ManagerImpl::checkAudio()
{
    if (getCallList().empty()) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        if (audiodriver_)
            audiodriver_->stopStream();
    }

}

//THREAD=Main
bool ManagerImpl::hangupCall(const std::string& callId)
{
    // store the current call id
    std::string currentCallId(getCurrentCallId());

    stopTone();

    DEBUG("Send call state change (HUNGUP) for id %s", callId.c_str());
    client_.getCallManager()->callStateChanged(callId, "HUNGUP");

    /* We often get here when the call was hungup before being created */
    if (not isValidCall(callId) and not isIPToIP(callId)) {
        DEBUG("Could not hang up call %s, call not valid", callId.c_str());
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
        if (auto call = getCallFromCallID(callId)) {
            history_.addCall(call.get(), preferences.getHistoryLimit());
            auto link = getAccountLink(call->getAccountId());
            link->hangup(callId, 0);
            checkAudio();
            saveHistory();
        }
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        return false;
    }

    return true;
}

bool ManagerImpl::hangupConference(const std::string& id)
{
    DEBUG("Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end()) {
        Conference *conf = iter_conf->second;

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
bool ManagerImpl::onHoldCall(const std::string& callId)
{
    bool result = true;

    stopTone();

    std::string current_call_id(getCurrentCallId());

    try {
        std::string account_id(getAccountFromCall(callId));
        if (account_id.empty()) {
            DEBUG("Account ID %s or callid %s doesn't exist in call onHold", account_id.c_str(), callId.c_str());
            return false;
        }
        getAccountLink(account_id)->onhold(callId);
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
bool ManagerImpl::offHoldCall(const std::string& callId)
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
            detachParticipant(MainBuffer::DEFAULT_ID);
    }

    try {
        if (auto call = getCallFromCallID(callId))
            getAccountLink(call->getAccountId())->offhold(callId);
        else
            result = false;
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
        return false;
    }

    client_.getCallManager()->callStateChanged(callId, "UNHOLD");

    if (isConferenceParticipant(callId)) {
        auto call = getCallFromCallID(callId);
        if (call)
            switchCall(call->getConfId());
        else
            result = false;
    } else
        switchCall(callId);

    addStream(callId);

    return result;
}

//THREAD=Main
bool ManagerImpl::transferCall(const std::string& callId, const std::string& to)
{
    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else if (not isConference(getCurrentCallId()))
        unsetCurrentCall();

    std::string accountID(getAccountFromCall(callId));
    if (accountID.empty())
        return false;
    getAccountLink(accountID)->transfer(callId, to);

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall(callId);

    return true;
}

void ManagerImpl::transferFailed()
{
    client_.getCallManager()->transferFailed();
}

void ManagerImpl::transferSucceeded()
{
    client_.getCallManager()->transferSucceeded();
}

bool ManagerImpl::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    std::string accountid(getAccountFromCall(transferID));
    if (accountid.empty())
        return false;
    return getAccountLink(accountid)->attendedTransfer(transferID, targetID);
}

//THREAD=Main : Call:Incoming
bool ManagerImpl::refuseCall(const std::string& id)
{
    if (!isValidCall(id))
        return false;

    stopTone();

    if (getCallList().size() <= 1) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    std::string accountid = getAccountFromCall(id);
    if (accountid.empty())
        return false;
    getAccountLink(accountid)->refuse(id);

    checkAudio();

    removeWaitingCall(id);

    client_.getCallManager()->callStateChanged(id, "HUNGUP");

    // Disconnect streams
    removeStream(id);

    return true;
}

Conference*
ManagerImpl::createConference(const std::string& id1, const std::string& id2)
{
    DEBUG("Create conference with call %s and %s", id1.c_str(), id2.c_str());

    Conference* conf = new Conference;

    conf->add(id1);
    conf->add(id2);

    // Add conference to map
    conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));

    client_.getCallManager()->conferenceCreated(conf->getConfID());

    return conf;
}

void ManagerImpl::removeConference(const std::string& conference_id)
{
    DEBUG("Remove conference %s", conference_id.c_str());
    DEBUG("number of participants: %u", conferenceMap_.size());
    ConferenceMap::iterator iter = conferenceMap_.find(conference_id);

    Conference* conf = 0;

    if (iter != conferenceMap_.end())
        conf = iter->second;

    if (conf == 0) {
        ERROR("Conference not found");
        return;
    }

    client_.getCallManager()->conferenceRemoved(conference_id);

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getMainBuffer().unBindAll(MainBuffer::DEFAULT_ID);

    ParticipantSet participants(conf->getParticipantList());

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getMainBuffer().bindCallID(*iter_p, MainBuffer::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (conferenceMap_.erase(conference_id))
        DEBUG("Conference %s removed successfully", conference_id.c_str());
    else
        ERROR("Cannot remove conference: %s", conference_id.c_str());

    delete conf;
}

Conference*
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

    Conference *conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                 conf->getState() == Conference::ACTIVE_DETACHED_REC or
                 conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (const auto &item : participants) {
        switchCall(item);
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

    Conference *conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
        conf->getState() == Conference::ACTIVE_DETACHED_REC or
        conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (const auto &item : participants) {
        if (auto call = getCallFromCallID(item)) {
            // if one call is currently recording, the conference is in state recording
            isRec |= call->isRecording();

            switchCall(item);
            offHoldCall(item);
        }
    }

    conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);

    client_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());

    return true;
}

bool ManagerImpl::isConference(const std::string& id) const
{
    return conferenceMap_.find(id) != conferenceMap_.end();
}

bool ManagerImpl::isConferenceParticipant(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    return call and not call->getConfId().empty();
}

bool
ManagerImpl::addParticipant(const std::string& callId, const std::string& conferenceId)
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
            detachParticipant(MainBuffer::DEFAULT_ID);
        else
            onHoldCall(current_call_id);
    }

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    unsetCurrentCall();

    // Add main participant
    addMainParticipant(conferenceId);

    Conference* conf = iter->second;
    switchCall(conf->getConfID());

    // Add coresponding IDs in conf and call
    call->setConfId(conf->getConfID());
    conf->add(callId);

    // Connect new audio streams together
    getMainBuffer().unBindAll(callId);

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
            detachParticipant(MainBuffer::DEFAULT_ID);
        else
            onHoldCall(current_call_id);
    }

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        ConferenceMap::const_iterator iter = conferenceMap_.find(conference_id);

        if (iter == conferenceMap_.end() or iter->second == 0)
            return false;

        Conference *conf = iter->second;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &item_p : participants) {
            getMainBuffer().bindCallID(item_p, MainBuffer::DEFAULT_ID);
            // Reset ringbuffer's readpointers
            getMainBuffer().flush(item_p);
        }

        getMainBuffer().flush(MainBuffer::DEFAULT_ID);

        if (conf->getState() == Conference::ACTIVE_DETACHED)
            conf->setState(Conference::ACTIVE_ATTACHED);
        else if (conf->getState() == Conference::ACTIVE_DETACHED_REC)
            conf->setState(Conference::ACTIVE_ATTACHED_REC);
        else
            WARN("Invalid conference state while adding main participant");

        client_.getCallManager()->conferenceChanged(conference_id, conf->getStateStr());
    }

    switchCall(conference_id);
    return true;
}

std::shared_ptr<Call>
ManagerImpl::getCallFromCallID(const std::string &callID)
{
    std::shared_ptr<Call> call = SIPVoIPLink::instance().getSipCall(callID);
#if HAVE_IAX
    if (call)
        return call;

    call = IAXVoIPLink::getIaxCall(callID);
#endif

    return call;
}

bool
ManagerImpl::joinParticipant(const std::string& callId1, const std::string& callId2)
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
            detachParticipant(MainBuffer::DEFAULT_ID);
        else
            onHoldCall(current_call_id); // currently in a call
    }


    Conference *conf = createConference(callId1, callId2);

    call1->setConfId(conf->getConfID());
    getMainBuffer().unBindAll(callId1);

    call2->setConfId(conf->getConfID());
    getMainBuffer().unBindAll(callId2);

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
    switchCall(conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    conf->setRecordingFormat(mainBuffer_.getInternalAudioFormat());

    return true;
}

void ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        ERROR("Participant number must be higher or equal to 2");
        return;
    }

    Conference *conf = new Conference;

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
        conf->setRecordingFormat(mainBuffer_.getInternalAudioFormat());
    } else {
        delete conf;
    }
}

bool
ManagerImpl::detachParticipant(const std::string& call_id)
{
    const std::string current_call_id(getCurrentCallId());

    if (call_id != MainBuffer::DEFAULT_ID) {
        auto call = getCallFromCallID(call_id);
        if (!call) {
            ERROR("Could not find call %s", call_id.c_str());
            return false;
        }

        Conference *conf = getConferenceFromCallID(call_id);

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
        getMainBuffer().unBindAll(MainBuffer::DEFAULT_ID);

        if (not isConference(current_call_id)) {
            ERROR("Current call id (%s) is not a conference", current_call_id.c_str());
            return false;
        }

        ConferenceMap::iterator iter = conferenceMap_.find(current_call_id);

        Conference *conf = iter->second;
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

void ManagerImpl::removeParticipant(const std::string& call_id)
{
    DEBUG("Remove participant %s", call_id.c_str());

    // this call is no longer a conference participant
    auto call = getCallFromCallID(call_id);
    if (!call) {
        ERROR("Call not found");
        return;
    }

    ConferenceMap::const_iterator iter = conferenceMap_.find(call->getConfId());

    Conference *conf = iter->second;
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

void ManagerImpl::processRemainingParticipants(Conference &conf)
{
    const std::string current_call_id(getCurrentCallId());
    ParticipantSet participants(conf.getParticipantList());
    const size_t n = participants.size();
    DEBUG("Process remaining %d participant(s) from conference %s",
           n, conf.getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto &p : participants)
            getMainBuffer().flush(p);

        getMainBuffer().flush(MainBuffer::DEFAULT_ID);
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
                switchCall(*p);
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

    Conference *conf = conferenceMap_.find(conf_id1)->second;
    ParticipantSet participants(conf->getParticipantList());

    for (const auto &p : participants)
        addParticipant(p, conf_id2);

    return true;
}

void ManagerImpl::addStream(const std::string& call_id)
{
    DEBUG("Add audio stream %s", call_id.c_str());
    auto call = getCallFromCallID(call_id);
    if (call and isConferenceParticipant(call_id)) {
        DEBUG("Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = conferenceMap_.find(call->getConfId());

        if (iter != conferenceMap_.end() and iter->second) {
            Conference* conf = iter->second;

            conf->bindParticipant(call_id);
        }

    } else {
        DEBUG("Add stream to call");

        // bind to main
        getMainBuffer().bindCallID(call_id, MainBuffer::DEFAULT_ID);

        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->flushUrgent();
        audiodriver_->flushMain();
    }
}

void ManagerImpl::removeStream(const std::string& call_id)
{
    DEBUG("Remove audio stream %s", call_id.c_str());
    getMainBuffer().unBindAll(call_id);
}

// Must be invoked periodically by a timer from the main event loop
void ManagerImpl::pollEvents()
{
    if (finished_)
        return;

    SIPVoIPLink::instance().handleEvents();

#if HAVE_IAX
    for (auto &item : IAXVoIPLink::getAccounts())
        item.second->getVoIPLink()->handleEvents();
#endif
}

//THREAD=Main
void ManagerImpl::saveConfig()
{
    DEBUG("Saving Configuration to XDG directory %s", path_.c_str());
    if (audiodriver_ != nullptr) {
        audioPreference.setVolumemic(audiodriver_->getCaptureGain());
        audioPreference.setVolumespkr(audiodriver_->getPlaybackGain());
        audioPreference.setCaptureMuted(audiodriver_->isCaptureMuted());
        audioPreference.setPlaybackMuted(audiodriver_->isPlaybackMuted());
    }

    try {
        Conf::YamlEmitter emitter(path_.c_str());

        for (auto &item : SIPVoIPLink::instance().getAccounts())
            item.second->serialize(emitter);

#if HAVE_IAX
        for (auto &item : IAXVoIPLink::getAccounts())
            item.second->serialize(emitter);
#endif

        // FIXME: this is a hack until we get rid of accountOrder
        preferences.verifyAccountOrder(getAccountList());
        preferences.serialize(emitter);
        voipPreferences.serialize(emitter);
        hookPreference.serialize(emitter);
        audioPreference.serialize(emitter);
#ifdef SFL_VIDEO
        getVideoManager()->getVideoDeviceMonitor().serialize(emitter);
#endif
        shortcutPreferences.serialize(emitter);

        emitter.serializeData();
    } catch (const Conf::YamlEmitterException &e) {
        ERROR("ConfigTree: %s", e.what());
    }
}

//THREAD=Main
void ManagerImpl::sendDtmf(const std::string& id, char code)
{
    playDtmf(code);

    // return if we're not "in" a call
    if (id.empty())
        return;

    std::string accountid(getAccountFromCall(id));
    getAccountLink(accountid)->carryingDTMFdigits(id, code);
}

//THREAD=Main | VoIPLink
void ManagerImpl::playDtmf(char code)
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
    if (audiodriver_ == nullptr or not dtmfKey_) {
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
bool ManagerImpl::incomingCallsWaiting()
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    return not waitingCalls_.empty();
}

void ManagerImpl::addWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.insert(id);
}

void ManagerImpl::removeWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.erase(id);
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
void ManagerImpl::incomingCall(Call &call, const std::string& accountId)
{
    stopTone();
    const std::string callID(call.getCallId());

    if (accountId.empty())
        setIPToIPForCall(callID, true);
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
void ManagerImpl::incomingMessage(const std::string& callID,
                                  const std::string& from,
                                  const std::string& message)
{
    if (isConferenceParticipant(callID)) {
        Conference *conf = getConferenceFromCallID(callID);

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &item_p : participants) {

            if (item_p == callID)
                continue;

            std::string accountId(getAccountFromCall(item_p));

            DEBUG("Send message to %s, (%s)", item_p.c_str(), accountId.c_str());

            Account *account = getAccount(accountId);

            if (!account) {
                ERROR("Failed to get account while sending instant message");
                return;
            }

            account->getVoIPLink()->sendTextMessage(callID, message, from);
        }

        // in case of a conference we must notify client using conference id
        client_.getCallManager()->incomingMessage(conf->getConfID(), from, message);

    } else
        client_.getCallManager()->incomingMessage(callID, from, message);
}

//THREAD=VoIP
bool ManagerImpl::sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
    if (isConference(callID)) {
        DEBUG("Is a conference, send instant message to everyone");
        ConferenceMap::iterator it = conferenceMap_.find(callID);

        if (it == conferenceMap_.end())
            return false;

        Conference *conf = it->second;

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &participant : participants) {

            std::string accountId = getAccountFromCall(participant);

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(participant, message, from);
        }

        return true;
    }

    if (isConferenceParticipant(callID)) {
        DEBUG("Call is participant in a conference, send instant message to everyone");
        Conference *conf = getConferenceFromCallID(callID);

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &item_p : participants) {

            const std::string accountId(getAccountFromCall(item_p));

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(item_p, message, from);
        }
    } else {
        Account *account = getAccount(getAccountFromCall(callID));

        if (!account) {
            DEBUG("Failed to get account while sending instant message");
            return false;
        }

        account->getVoIPLink()->sendTextMessage(callID, message, from);
    }
    return true;
}
#endif // HAVE_INSTANT_MESSAGING

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall(const std::string& id)
{
    DEBUG("Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(id))
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
void ManagerImpl::peerRingingCall(const std::string& id)
{
    DEBUG("Peer call %s ringing", id.c_str());

    if (isCurrentCall(id))
        ringback();

    client_.getCallManager()->callStateChanged(id, "RINGING");
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall(const std::string& call_id)
{
    DEBUG("Peer hungup call %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        removeParticipant(call_id);
    } else if (isCurrentCall(call_id)) {
        stopTone();
        unsetCurrentCall();
    }

    if (auto call = getCallFromCallID(call_id)) {
        history_.addCall(call.get(), preferences.getHistoryLimit());
        getAccountLink(call->getAccountId())->peerHungup(call_id);
        saveHistory();
    }

    client_.getCallManager()->callStateChanged(call_id, "HUNGUP");

    removeWaitingCall(call_id);
    if (not incomingCallsWaiting())
        stopTone();

    removeStream(call_id);
}

//THREAD=VoIP
void ManagerImpl::callBusy(const std::string& id)
{
    client_.getCallManager()->callStateChanged(id, "BUSY");

    if (isCurrentCall(id)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    checkAudio();
    removeWaitingCall(id);
}

//THREAD=VoIP
void ManagerImpl::callFailure(const std::string& call_id)
{
    client_.getCallManager()->callStateChanged(call_id, "FAILURE");

    if (isCurrentCall(call_id)) {
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
void ManagerImpl::startVoiceMessageNotification(const std::string& accountId,
        int nb_msg)
{
    client_.getCallManager()->voiceMailNotify(accountId, nb_msg);
}

/**
 * Multi Thread
 */
void ManagerImpl::playATone(Tone::TONEID toneId)
{
    if (not voipPreferences.getPlayTones())
        return;

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (audiodriver_ == nullptr) {
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
void ManagerImpl::stopTone()
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

// Caller must hold toneMutex
void
ManagerImpl::updateAudioFile(const std::string &file, int sampleRate)
{
    audiofile_.reset(new AudioFile(file, sampleRate));
}

/**
 * Multi Thread
 */
void ManagerImpl::playRingtone(const std::string& accountID)
{
    Account *account = getAccount(accountID);

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

        if (!audiodriver_) {
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

AudioLoop* ManagerImpl::getTelephoneTone()
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
std::string ManagerImpl::retrieveConfigPath() const
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
void ManagerImpl::setAudioPlugin(const std::string& audioPlugin)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    audioPreference.setAlsaPlugin(audioPlugin);

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
void ManagerImpl::setAudioDevice(int index, DeviceType type)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (!audiodriver_) {
        ERROR("Audio driver not initialized");
        return ;
    }

    const bool wasStarted = audiodriver_->isStarted();
    audiodriver_->updatePreference(audioPreference, index, type);

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
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    return audiodriver_->getPlaybackDeviceList();
}


/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    return audiodriver_->getCaptureDeviceList();
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex()
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

int ManagerImpl::isRingtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void ManagerImpl::ringtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled(false) : account->setRingtoneEnabled(true);
}

bool ManagerImpl::getIsAlwaysRecording() const
{
    return audioPreference.getIsAlwaysRecording();
}

void ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
    return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

bool ManagerImpl::toggleRecordingCall(const std::string& id)
{
    std::shared_ptr<Call> call;
    Recordable* rec = nullptr;

    ConferenceMap::const_iterator it(conferenceMap_.find(id));
    if (it == conferenceMap_.end()) {
        DEBUG("toggle recording for call %s", id.c_str());
        getCallFromCallID(id);
        call = getCallFromCallID(id);
        rec = call.get();
    } else {
        DEBUG("toggle recording for conference %s", id.c_str());
        Conference *conf = it->second;
        if (conf) {
            rec = conf;
            if (rec->isRecording())
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

bool ManagerImpl::isRecording(const std::string& id)
{
    auto call = getCallFromCallID(id);
    return call and (static_cast<Recordable*>(call.get()))->isRecording();
}

bool ManagerImpl::startRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Start recorded file playback %s", filepath.c_str());

    int sampleRate;
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (!audiodriver_) {
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

int ManagerImpl::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

bool ManagerImpl::setAudioManager(const std::string &api)
{
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (!audiodriver_)
            return false;

        if (api == audioPreference.getAudioApi()) {
            DEBUG("Audio manager chosen already in use. No changes made. ");
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        bool wasStarted = audiodriver_->isStarted();
        delete audiodriver_;
        audioPreference.setAudioApi(api);
        audiodriver_ = audioPreference.createAudioLayer();

        if (wasStarted)
            audiodriver_->startStream();
    }

    saveConfig();

    // ensure that we completed the transition (i.e. no fallback was used)
    return api == audioPreference.getAudioApi();
}

std::string ManagerImpl::getAudioManager() const
{
    return audioPreference.getAudioApi();
}


int ManagerImpl::getAudioInputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (audiodriver_ == nullptr) {
        ERROR("Audio layer not initialized");
        return 0;
    }

    return audiodriver_->getAudioDeviceIndex(name, DeviceType::CAPTURE);
}

int ManagerImpl::getAudioOutputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (audiodriver_ == nullptr) {
        ERROR("Audio layer not initialized");
        return 0;
    }

    return audiodriver_->getAudioDeviceIndex(name, DeviceType::PLAYBACK);
}

std::string ManagerImpl::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getAlsaPlugin();
}


bool ManagerImpl::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce();
}

void ManagerImpl::setNoiseSuppressState(bool state)
{
    audioPreference.setNoiseReduce(state);
}

bool ManagerImpl::isAGCEnabled() const
{
    return audioPreference.isAGCEnabled();
}

void ManagerImpl::setAGCState(bool state)
{
    audioPreference.setAGCState(state);
}

/**
 * Initialization: Main Thread
 */
void ManagerImpl::initAudioDriver()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    audiodriver_ = audioPreference.createAudioLayer();
}

void ManagerImpl::hardwareAudioFormatChanged(AudioFormat format)
{
    audioFormatUsed(format);
}

void ManagerImpl::audioFormatUsed(AudioFormat format)
{
    AudioFormat currentFormat = mainBuffer_.getInternalAudioFormat();
    format.nb_channels = std::max(currentFormat.nb_channels, std::min(format.nb_channels, 2u)); // max 2 channels.
    format.sample_rate = std::max(currentFormat.sample_rate, format.sample_rate);

    if (currentFormat == format)
        return;

    DEBUG("Audio format changed: %s -> %s", currentFormat.toString().c_str(), format.toString().c_str());

    mainBuffer_.setInternalAudioFormat(format);

    {
        std::lock_guard<std::mutex> toneLock(toneMutex_);
        telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), format.sample_rate));
    }
    dtmfKey_.reset(new DTMF(format.sample_rate));
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
    DEBUG("Set accounts order : %s", order.c_str());
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

    vector<string> v;

    // Concatenate all account pointers in a single map
    AccountMap allAccounts(getAllAccounts());

    // If no order has been set, load the default one ie according to the creation date.
    if (account_order.empty()) {
        for (const auto &item : allAccounts) {
            if (item.first == SIPAccount::IP2IP_PROFILE || item.first.empty())
                continue;

            if (item.second)
                v.push_back(item.second->getAccountID());
        }
    }
    else {
        for (const auto &item : account_order) {
            if (item == SIPAccount::IP2IP_PROFILE or item.empty())
                continue;

            AccountMap::const_iterator account_iter = allAccounts.find(item);

            if (account_iter != allAccounts.end() and account_iter->second)
                v.push_back(account_iter->second->getAccountID());
        }
    }

    Account *account = getIP2IPAccount();
    if (account)
        v.push_back(account->getAccountID());
    else
        ERROR("could not find IP2IP profile in getAccount list");

    return v;
}

std::map<std::string, std::string> ManagerImpl::getAccountDetails(
    const std::string& accountID) const
{
    Account * account = getAccount(accountID);

    if (account) {
        return account->getAccountDetails();
    } else {
        ERROR("Could not get account details on a non-existing accountID %s", accountID.c_str());
        // return an empty map since we can't throw an exception to D-Bus
        return std::map<std::string, std::string>();
    }
}

// method to reduce the if/else mess.
// Even better, switch to XML !

void ManagerImpl::setAccountDetails(const std::string& accountID,
                                    const std::map<std::string, std::string>& details)
{
    DEBUG("Set account details for %s", accountID.c_str());

    Account* account = getAccount(accountID);

    if (account == nullptr) {
        ERROR("Could not find account %s", accountID.c_str());
        return;
    }

    // Ignore if nothing has changed
    if (details == account->getAccountDetails())
        return;

    // Unregister before modifying any account information
    account->unregisterVoIPLink([&](bool /* transport_free */) {
        account->setAccountDetails(details);
        // Serialize configuration to disk once it is done
        saveConfig();

        if (account->isEnabled())
            account->registerVoIPLink();
        else
            account->unregisterVoIPLink();

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

    std::string accountType;
    if (details.find(CONFIG_ACCOUNT_TYPE) == details.end())
        accountType = "SIP";
    else
        accountType = ((*details.find(CONFIG_ACCOUNT_TYPE)).second);

    DEBUG("Adding account %s", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    Account* newAccount = nullptr;

    if (accountType == "SIP") {
        newAccount = new SIPAccount(newAccountID, true);
        SIPVoIPLink::instance().getAccounts()[newAccountID] = newAccount;
    }
#if HAVE_IAX
    else if (accountType == "IAX") {
        newAccount = new IAXAccount(newAccountID);
        IAXVoIPLink::getAccounts()[newAccountID] = newAccount;
    }
#endif
    else {
        ERROR("Unknown %s param when calling addAccount(): %s",
               CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    newAccount->setAccountDetails(details);

    preferences.addAccount(newAccountID);

    newAccount->registerVoIPLink();

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
    Account* remAccount = getAccount(accountID);

    if (remAccount != nullptr) {
        remAccount->unregisterVoIPLink();
        SIPVoIPLink::instance().getAccounts().erase(accountID);
#if HAVE_IAX
        IAXVoIPLink::getAccounts().erase(accountID);
#endif
        // http://projects.savoirfairelinux.net/issues/show/2355
        // delete remAccount;
    }

    preferences.removeAccount(accountID);
    config_.removeSection(accountID);

    saveConfig();

    client_.getConfigurationManager()->accountsChanged();
}

std::string ManagerImpl::getAccountFromCall(const std::string& callID)
{
    if (auto call = getCallFromCallID(callID))
        return call->getAccountId();
    else
        return "";
}

// FIXME: get rid of this, there's no guarantee that
// a Call will still exist after this has been called.
bool ManagerImpl::isValidCall(const std::string& callID)
{
    return static_cast<bool>(getCallFromCallID(callID));
}

std::string ManagerImpl::getNewCallID()
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

std::vector<std::string> ManagerImpl::loadAccountOrder() const
{
    return Account::split_string(preferences.getAccountOrder());
}

#if HAVE_IAX
static void
loadAccount(const Conf::YamlNode *item, AccountMap &sipAccountMap,
            AccountMap &iaxAccountMap, int &errorCount,
            const std::string &accountOrder)
#else
static void
loadAccount(const Conf::YamlNode *item, AccountMap &sipAccountMap,
            int &errorCount, const std::string &accountOrder)
#endif
{
    if (!item) {
        ERROR("Could not load account");
        ++errorCount;
        return;
    }

    std::string accountType;
    item->getValue("type", &accountType);

    std::string accountid;
    item->getValue("id", &accountid);

    std::string accountAlias;
    item->getValue("alias", &accountAlias);
    const auto inAccountOrder = [&] (const std::string &id) {
        return accountOrder.find(id + "/") != std::string::npos; };

    if (!accountid.empty() and !accountAlias.empty()) {
        if (not inAccountOrder(accountid) and accountid != SIPAccount::IP2IP_PROFILE) {
            WARN("Dropping account %s, which is not in account order", accountid.c_str());
        } else if (accountType == "SIP") {
            Account *a = new SIPAccount(accountid, true);
            sipAccountMap[accountid] = a;
            a->unserialize(*item);
        } else if (accountType == "IAX") {
#if HAVE_IAX
            Account *a = new IAXAccount(accountid);
            iaxAccountMap[accountid] = a;
            a->unserialize(*item);
#else
            ERROR("Ignoring IAX account");
            ++errorCount;
#endif
        } else {
            ERROR("Ignoring unknown account type \"%s\"", accountType.c_str());
            ++errorCount;
        }
    }
}

int ManagerImpl::loadAccountMap(Conf::YamlParser &parser)
{
    using namespace Conf;

    // load saved preferences for IP2IP account from configuration file
    Sequence *seq = parser.getAccountSequence()->getSequence();

    // build preferences
    preferences.unserialize(*parser.getPreferenceNode());
    voipPreferences.unserialize(*parser.getVoipPreferenceNode());
    hookPreference.unserialize(*parser.getHookNode());
    audioPreference.unserialize(*parser.getAudioNode());
    shortcutPreferences.unserialize(*parser.getShortcutNode());

    int errorCount = 0;
#ifdef SFL_VIDEO
    VideoManager *controls(getVideoManager());
    try {
        MappingNode *videoNode = parser.getVideoNode();
        if (videoNode)
            controls->getVideoDeviceMonitor().unserialize(*videoNode);
    } catch (const YamlParserException &e) {
        ERROR("No video node in config file");
        ++errorCount;
    }
#endif

    const std::string accountOrder = preferences.getAccountOrder();

    AccountMap &sipAccounts = SIPVoIPLink::instance().getAccounts();
#if HAVE_IAX
    AccountMap &iaxAccounts = IAXVoIPLink::getAccounts();
    for (auto &s : *seq)
        loadAccount(s, sipAccounts, iaxAccounts, errorCount, accountOrder);
#else
    for (auto &s : *seq)
        loadAccount(s, sipAccounts, errorCount, accountOrder);
#endif

    // This must happen after account is loaded
    SIPVoIPLink::instance().loadIP2IPSettings();

    return errorCount;
}

bool ManagerImpl::accountExists(const std::string &accountID)
{
    bool ret = false;

    ret = SIPVoIPLink::instance().getAccounts().find(accountID) != SIPVoIPLink::instance().getAccounts().end();
#if HAVE_IAX
    if (ret)
        return ret;

    ret = IAXVoIPLink::getAccounts().find(accountID) != IAXVoIPLink::getAccounts().end();
#endif

    return ret;
}

SIPAccount*
ManagerImpl::getIP2IPAccount() const
{
    AccountMap::const_iterator iter = SIPVoIPLink::instance().getAccounts().find(SIPAccount::IP2IP_PROFILE);
    if (iter == SIPVoIPLink::instance().getAccounts().end())
        return nullptr;

    return static_cast<SIPAccount *>(iter->second);
}

Account*
ManagerImpl::getAccount(const std::string& accountID) const
{
    Account *account = nullptr;

    account = getSipAccount(accountID);
    if (account != nullptr)
        return account;

#if HAVE_IAX
    account = getIaxAccount(accountID);
    if (account != nullptr)
        return account;
#endif

    return nullptr;
}

SIPAccount *
ManagerImpl::getSipAccount(const std::string& accountID) const
{
    AccountMap::const_iterator iter = SIPVoIPLink::instance().getAccounts().find(accountID);
    if (iter != SIPVoIPLink::instance().getAccounts().end())
        return static_cast<SIPAccount *>(iter->second);

    return nullptr;
}

#if HAVE_IAX
IAXAccount *
ManagerImpl::getIaxAccount(const std::string& accountID) const
{
    AccountMap::const_iterator iter = IAXVoIPLink::getAccounts().find(accountID);
    if (iter != IAXVoIPLink::getAccounts().end())
        return static_cast<IAXAccount *>(iter->second);

    return nullptr;
}
#endif

AccountMap
ManagerImpl::getAllAccounts() const
{
    AccountMap all;
    all.insert(SIPVoIPLink::instance().getAccounts().begin(), SIPVoIPLink::instance().getAccounts().end());
#if HAVE_IAX
    all.insert(IAXVoIPLink::getAccounts().begin(), IAXVoIPLink::getAccounts().end());
#endif
    return all;
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

    // Then the VoIP link this account is linked with (IAX2 or SIP)
    if (auto call = getCallFromCallID(callID)) {
        return call->getDetails();
    } else {
        ERROR("Call is NULL");
        // FIXME: is this even useful?
        return Call::getNullDetails();
    }
}

std::vector<std::map<std::string, std::string> > ManagerImpl::getHistory()
{
    return history_.getSerialized();
}

std::vector<std::string>
ManagerImpl::getCallList() const
{
    std::vector<std::string> v(SIPVoIPLink::instance().getCallIDs());
#if HAVE_IAX
    const std::vector<std::string> iaxCalls(IAXVoIPLink::getCallIDs());
    v.insert(v.end(), iaxCalls.begin(), iaxCalls.end());
#endif
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

std::string ManagerImpl::getConferenceId(const std::string& callID)
{
    if (auto call = getCallFromCallID(callID))
        return call->getConfId();

    ERROR("Call is NULL");
    return "";
}

void ManagerImpl::saveHistory()
{
    if (!history_.save())
        ERROR("Could not save history!");
    else
        client_.getConfigurationManager()->historyChanged();
}

void ManagerImpl::clearHistory()
{
    history_.clear();
}

void ManagerImpl::startAudioDriverStream()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    audiodriver_->startStream();
}

void
ManagerImpl::freeAccount(const std::string& accountID)
{
    Account *account = getAccount(accountID);
    if (!account)
        return;
    for (const auto& call : account->getVoIPLink()->getCalls(accountID))
        hangupCall(call->getCallId());
    account->unregisterVoIPLink();
}

void
ManagerImpl::registerAccounts()
{
    auto allAccounts(getAccountList());

    for (auto &item : allAccounts) {
        Account *a = getAccount(item);

        if (!a)
            continue;

        a->loadConfig();

        if (a->isEnabled())
            a->registerVoIPLink();
    }
}

void
ManagerImpl::unregisterAccounts()
{
    for (auto &item : getAccountList()) {
        Account *a = getAccount(item);

        if (!a)
            continue;

        if (a->isEnabled())
            a->unregisterVoIPLink();
    }
}


VoIPLink* ManagerImpl::getAccountLink(const std::string& accountID)
{
    Account *account = getAccount(accountID);
    if (account == nullptr) {
        DEBUG("Could not find account for account %s, returning sip voip", accountID.c_str());
        return &SIPVoIPLink::instance();
    }

    if (not accountID.empty())
        return account->getVoIPLink();
    else {
        DEBUG("Account id is empty for voip link, returning sip voip");
        return &SIPVoIPLink::instance();
    }
}


void
ManagerImpl::sendRegister(const std::string& accountID, bool enable)
{
    Account* acc = getAccount(accountID);
    if (!acc)
        return;

    acc->setEnabled(enable);
    acc->loadConfig();

    Manager::instance().saveConfig();

    if (acc->isEnabled())
        acc->registerVoIPLink();
    else
        acc->unregisterVoIPLink();
}


AudioLayer*
ManagerImpl::getAudioDriver()
{
    return audiodriver_;
}


MainBuffer &
ManagerImpl::getMainBuffer()
{
    return mainBuffer_;
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
