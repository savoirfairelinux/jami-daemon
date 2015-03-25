/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include "plugin_manager.h"

#include "fileutils.h"
#include "map_utils.h"
#include "account.h"
#include "string_utils.h"
#if HAVE_DHT
#include "ringdht/ringaccount.h"
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
#include "manager.h"

#ifdef RING_VIDEO
#include "client/videomanager.h"
#endif

#include "conference.h"
#include "ice_transport.h"

#include "client/signal.h"

#if HAVE_TLS
#include "gnutls_support.h"
#endif

#include "libav_utils.h"
#include "video/sinkclient.h"

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
#include <mutex>

namespace ring {

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

/**
 * Set pjsip's log level based on the SIPLOGLEVEL environment variable.
 * SIPLOGLEVEL = 0 minimum logging
 * SIPLOGLEVEL = 6 maximum logging
 */

/** Environment variable used to set pjsip's logging level */
static constexpr const char* SIPLOGLEVEL = "SIPLOGLEVEL";

static void
setSipLogLevel()
{
    char* envvar = getenv(SIPLOGLEVEL);
    int level = 0;

    if (envvar != nullptr) {
        if (not (std::istringstream(envvar) >> level))
            level = 0;

        // From 0 (min) to 6 (max)
        level = std::max(0, std::min(level, 6));
    }

    pj_log_set_level(level);
}

#if HAVE_TLS
/**
 * Set gnutls's log level based on the RING_TLS_LOGLEVEL environment variable.
 * RING_TLS_LOGLEVEL = 0 minimum logging (default)
 * RING_TLS_LOGLEVEL = 9 maximum logging
 */

static constexpr int RING_TLS_LOGLEVEL = 0;

static void
tls_print_logs(int level, const char* msg)
{
    RING_DBG("GnuTLS [%d]: %s", level, msg);
}

static void
setGnuTlsLogLevel()
{
    char* envvar = getenv("RING_TLS_LOGLEVEL");
    int level = RING_TLS_LOGLEVEL;

    if (envvar != nullptr) {
        int var_level;
        if (std::istringstream(envvar) >> var_level)
            level = var_level;

        // From 0 (min) to 9 (max)
        level = std::max(0, std::min(level, 9));
    }

    gnutls_global_set_log_level(level);
    gnutls_global_set_log_function(tls_print_logs);
}
#endif // HAVE_TLS

void
ManagerImpl::loadDefaultAccountMap()
{
    accountFactory_.initIP2IPAccount();
}

ManagerImpl::ManagerImpl() :
    pluginManager_(new PluginManager)
    , preferences(), voipPreferences(),
    hookPreference(),  audioPreference(), shortcutPreferences(),
    hasTriedToRegister_(false),
    currentCallMutex_(), dtmfKey_(), dtmfBuf_(0, AudioFormat::MONO()),
    toneMutex_(), telephoneTone_(), audiofile_(), audioLayerMutex_(),
    waitingCalls_(), waitingCallsMutex_(), path_()
    , ringbufferpool_(new RingBufferPool)
    , callFactory(), conferenceMap_()
    , accountFactory_(), ice_tf_()
    , gnutlGIG_ {tls::GnuTlsGlobalInit::make_guard()}
{
    // initialize random generator
    // mt19937_64 should be seeded with 2 x 32 bits
    std::random_device rdev;
    std::seed_seq seed {rdev(), rdev()};
    rand_.seed(seed);

    ring::libav_utils::ring_avcodec_init();
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
            RING_WARN("Errors while parsing %s", path_.c_str());
            result = false;
        }
    } catch (const YAML::BadFile &e) {
        RING_WARN("Could not open config file: creating default account map");
        loadDefaultAccountMap();
    }

    return result;
}

void
ManagerImpl::init(const std::string &config_file)
{
    // FIXME: this is no good
    initialized = true;

#define PJSIP_TRY(ret) do {                                 \
        if (ret != PJ_SUCCESS)                               \
            throw std::runtime_error(#ret " failed");        \
    } while (0)

    srand(time(NULL)); // to get random number for RANDOM_PORT

    // Initialize PJSIP (SIP and ICE implementation)
    PJSIP_TRY(pj_init());
    setSipLogLevel();
    PJSIP_TRY(pjlib_util_init());
    PJSIP_TRY(pjnath_init());
#undef TRY

    RING_DBG("pjsip version %s for %s initialized",
             pj_get_version(), PJ_OS_NAME);

#if HAVE_TLS
    setGnuTlsLogLevel();
    RING_DBG("GNU TLS version %s initialized", gnutls_check_version(nullptr));
#endif

    ice_tf_.reset(new IceTransportFactory());

    path_ = config_file.empty() ? retrieveConfigPath() : config_file;
    RING_DBG("Configuration file path: %s", path_.c_str());

    bool no_errors = true;

    // manager can restart without being recreated (android)
    finished_ = false;

    try {
        no_errors = parseConfiguration();
    } catch (const YAML::Exception &e) {
        RING_ERR("%s", e.what());
        no_errors = false;
    }

    // always back up last error-free configuration
    if (no_errors) {
        make_backup(path_);
    } else {
        // restore previous configuration
        RING_WARN("Restoring last working configuration");

        try {
            // remove accounts from broken configuration
            removeAccounts();
            restore_backup(path_);
            parseConfiguration();
        } catch (const YAML::Exception &e) {
            RING_ERR("%s", e.what());
            RING_WARN("Restoring backup failed, creating default account map");
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
            dtmfKey_.reset(new DTMF(getRingBufferPool().getInternalSamplingRate()));
        }
    }

    registerAccounts();
}

void
ManagerImpl::setPath(const std::string&)
{
    // FIME: needed?
}

void
ManagerImpl::finish() noexcept
{
    bool expected = false;
    if (not finished_.compare_exchange_strong(expected, true))
        return;

    try {
        // Forbid call creation
        callFactory.forbid();

        // Hangup all remaining active calls
        RING_DBG("Hangup %zu remaining call(s)", callFactory.callCount());
        for (const auto call : callFactory.getAllCalls())
            hangupCall(call->getCallId());
        callFactory.clear();

        saveConfig();

        // Disconnect accounts, close link stacks and free allocated ressources
        unregisterAccounts();
        accountFactory_.clear();

        {
            std::lock_guard<std::mutex> lock(audioLayerMutex_);

            audiodriver_.reset();
        }

        ice_tf_.reset();
        pj_shutdown();
    } catch (const VoipLinkException &err) {
        RING_ERR("%s", err.what());
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
    RING_DBG("----- Switch current call id to '%s' -----",
          call ? call->getCallId().c_str() : "<nullptr>");
    currentCall_ = call;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

std::string
ManagerImpl::outgoingCall(const std::string& preferred_account_id,
                          const std::string& to,
                          const std::string& conf_id)
{
    std::string current_call_id(getCurrentCallId());
    std::string prefix(hookPreference.getNumberAddPrefix());
    std::string to_cleaned(NumberCleaner::clean(to, prefix));
    std::shared_ptr<Call> call;

    try {
        /* RING_WARN: after this call the account_id is obsolete
         * as the factory may decide to use another account (like IP2IP).
         */
        RING_DBG("New outgoing call to %s", to_cleaned.c_str());
        call = newOutgoingCall(to_cleaned, preferred_account_id);
    } catch (const std::exception &e) {
        RING_ERR("%s", e.what());
        return {};
    }

    if (not call)
        return {};

    auto call_id = call->getCallId();

    stopTone();

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        RING_DBG("Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conference and this and is not a conference participant
        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id))
            onHoldCall(current_call_id);
        else if (isConference(current_call_id) and not isConferenceParticipant(call_id))
            detachParticipant(RingBufferPool::DEFAULT_ID);
    }

    switchCall(call);
    call->setConfId(conf_id);

    return call_id;
}

//THREAD=Main : for outgoing Call
bool
ManagerImpl::answerCall(const std::string& call_id)
{
    bool result = true;

    auto call = getCallFromCallID(call_id);
    if (!call) {
        RING_ERR("Call %s is NULL", call_id.c_str());
        return false;
    }

    // If ring is ringing
    stopTone();

    // store the current call id
    std::string current_call_id(getCurrentCallId());

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        RING_DBG("Currently conversing with %s", current_call_id.c_str());

        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id)) {
            RING_DBG("Answer call: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall(current_call_id);
        } else if (isConference(current_call_id) and not isConferenceParticipant(call_id)) {
            // if we are talking to a conference and we are answering an incoming call
            RING_DBG("Detach main participant from conference");
            detachParticipant(RingBufferPool::DEFAULT_ID);
        }
    }

    try {
        call->answer();
    } catch (const std::runtime_error &e) {
        RING_ERR("%s", e.what());
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
    addStream(*call);

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);

    //callStateChanged(call_id, "CURRENT");
    emitSignal<DRing::CallSignal::StateChange>(call_id, "CURRENT", 0);

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

    RING_DBG("Send call state change (HUNGUP) for id %s", callId.c_str());
    emitSignal<DRing::CallSignal::StateChange>(callId, "HUNGUP", 0);

    /* We often get here when the call was hungup before being created */
    auto call = getCallFromCallID(callId);
    if (not call) {
        RING_WARN("Could not hang up non-existant call %s", callId.c_str());
        checkAudio();
        return false;
    }

    // Disconnect streams
    removeStream(*call);

    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (not isConference(currentCallId))
            unsetCurrentCall();
    }

    try {
        call->hangup(0);
        checkAudio();
    } catch (const VoipLinkException &e) {
        RING_ERR("%s", e.what());
        return false;
    }

    return true;
}

bool
ManagerImpl::hangupConference(const std::string& id)
{
    RING_DBG("Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end()) {
        auto conf = iter_conf->second;

        if (conf) {
            ParticipantSet participants(conf->getParticipantList());

            for (const auto &item : participants)
                hangupCall(item);
        } else {
            RING_ERR("No such conference %s", id.c_str());
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

    if (auto call = getCallFromCallID(callId)) {
        try {
            call->onhold();
            removeStream(*call); // Unbind calls in main buffer
        } catch (const VoipLinkException &e) {
            RING_ERR("%s", e.what());
            result = false;
        }
    } else {
        RING_DBG("CallID %s doesn't exist in call onHold", callId.c_str());
        return false;
    }

    // Remove call from teh queue if it was still there
    removeWaitingCall(callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId)
        unsetCurrentCall();

    emitSignal<DRing::CallSignal::StateChange>(callId, "HOLD", 0);

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
            RING_DBG("Has current call (%s), put on hold", currentCallId.c_str());
            //FIXME: ebail
            // if 2 consecutive offHoldCall done, the second one should be ignored (already offhold)
            // this call put the call onHold
            onHoldCall(currentCallId);
        } else if (isConference(currentCallId) && callId != currentCallId) {
            holdConference(currentCallId);
        } else if (isConference(currentCallId) and not isConferenceParticipant(callId))
            detachParticipant(RingBufferPool::DEFAULT_ID);
    }

    std::shared_ptr<Call> call;
    try {
        call = getCallFromCallID(callId);
        if (call)
            call->offhold();
        else
            result = false;
    } catch (const VoipLinkException &e) {
        RING_ERR("%s", e.what());
        return false;
    }

    emitSignal<DRing::CallSignal::StateChange>(callId, "UNHOLD", 0);

    if (isConferenceParticipant(callId))
        switchCall(getCallFromCallID(call->getConfId()));
    else
        switchCall(call);

    addStream(*call);

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
    emitSignal<DRing::CallSignal::TransferFailed>();
}

void
ManagerImpl::transferSucceeded()
{
    transferSucceeded();
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

    emitSignal<DRing::CallSignal::StateChange>(id, "HUNGUP", 0);

    // Disconnect streams
    removeStream(*call);

    return true;
}

std::shared_ptr<Conference>
ManagerImpl::createConference(const std::string& id1, const std::string& id2)
{
    RING_DBG("Create conference with call %s and %s", id1.c_str(), id2.c_str());

    auto conf = std::make_shared<Conference>();

    conf->add(id1);
    conf->add(id2);

    // Add conference to map
    conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));

    emitSignal<DRing::CallSignal::ConferenceCreated>(conf->getConfID());

    return conf;
}

void
ManagerImpl::removeConference(const std::string& conference_id)
{
    RING_DBG("Remove conference %s", conference_id.c_str());
    RING_DBG("number of participants: %u", conferenceMap_.size());
    ConferenceMap::iterator iter = conferenceMap_.find(conference_id);

    std::shared_ptr<Conference> conf;

    if (iter != conferenceMap_.end())
        conf = iter->second;

    if (not conf) {
        RING_ERR("Conference not found");
        return;
    }

    emitSignal<DRing::CallSignal::ConferenceRemoved>(conference_id);

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);

    ParticipantSet participants(conf->getParticipantList());

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getRingBufferPool().bindCallID(*iter_p, RingBufferPool::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (conferenceMap_.erase(conference_id))
        RING_DBG("Conference %s removed successfully", conference_id.c_str());
    else
        RING_ERR("Cannot remove conference: %s", conference_id.c_str());
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

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

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

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

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
    RING_DBG("Add participant %s to %s", callId.c_str(), conferenceId.c_str());
    ConferenceMap::iterator iter = conferenceMap_.find(conferenceId);

    if (iter == conferenceMap_.end()) {
        RING_ERR("Conference id is not valid");
        return false;
    }

    auto call = getCallFromCallID(callId);
    if (!call) {
        RING_ERR("Call id %s is not valid", callId.c_str());
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
            detachParticipant(RingBufferPool::DEFAULT_ID);
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
        RING_ERR("Participant list is empty for this conference");

    // Connect stream
    addStream(*call);
    return true;
}

bool
ManagerImpl::addMainParticipant(const std::string& conference_id)
{
    if (hasCurrentCall()) {
        std::string current_call_id(getCurrentCallId());

        if (isConference(current_call_id))
            detachParticipant(RingBufferPool::DEFAULT_ID);
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
            getRingBufferPool().bindCallID(item_p, RingBufferPool::DEFAULT_ID);
            // Reset ringbuffer's readpointers
            getRingBufferPool().flush(item_p);
        }

        getRingBufferPool().flush(RingBufferPool::DEFAULT_ID);

        if (conf->getState() == Conference::ACTIVE_DETACHED)
            conf->setState(Conference::ACTIVE_ATTACHED);
        else if (conf->getState() == Conference::ACTIVE_DETACHED_REC)
            conf->setState(Conference::ACTIVE_ATTACHED_REC);
        else
            RING_WARN("Invalid conference state while adding main participant");

        emitSignal<DRing::CallSignal::ConferenceChanged>(conference_id, conf->getStateStr());
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
        RING_ERR("Cannot join participant %s to itself", callId1.c_str());
        return false;
    }

    // Set corresponding conference ids for call 1
    auto call1 = getCallFromCallID(callId1);
    if (!call1) {
        RING_ERR("Could not find call %s", callId1.c_str());
        return false;
    }

    // Set corresponding conderence details
    auto call2 = getCallFromCallID(callId2);
    if (!call2) {
        RING_ERR("Could not find call %s", callId2.c_str());
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
    RING_DBG("Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ((current_call_id != callId1) and (current_call_id != callId2)) {
        // If currently in a conference
        if (isConference(current_call_id))
            detachParticipant(RingBufferPool::DEFAULT_ID);
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
    RING_DBG("Process call %s state: %s", callId1.c_str(), call1_state_str.c_str());

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
        RING_WARN("Call state not recognized");

    // Process call2 according to its state
    std::string call2_state_str(call2Details.find("CALL_STATE")->second);
    RING_DBG("Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

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
        RING_WARN("Call state not recognized");

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
        RING_ERR("Participant number must be higher or equal to 2");
        return;
    }

    auto conf = std::make_shared<Conference>();

    int successCounter = 0;

    for (const auto &p : participantList) {
        std::string numberaccount(p);
        std::string tostr(numberaccount.substr(0, numberaccount.find(",")));
        std::string account(numberaccount.substr(numberaccount.find(",") + 1, numberaccount.size()));

        unsetCurrentCall();

        // Create call
        auto call_id = outgoingCall(account, tostr, conf->getConfID());
        if (call_id.empty())
            continue;

        // Manager methods may behave differently if the call id participates in a conference
        conf->add(call_id);

        emitSignal<DRing::CallSignal::NewCallCreated>(account, call_id, tostr);
        successCounter++;
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        conferenceMap_[conf->getConfID()] = conf;
        emitSignal<DRing::CallSignal::ConferenceCreated>(conf->getConfID());
        conf->setRecordingFormat(ringbufferpool_->getInternalAudioFormat());
    }
}

bool
ManagerImpl::detachParticipant(const std::string& call_id)
{
    const std::string current_call_id(getCurrentCallId());

    if (call_id != RingBufferPool::DEFAULT_ID) {
        auto call = getCallFromCallID(call_id);
        if (!call) {
            RING_ERR("Could not find call %s", call_id.c_str());
            return false;
        }

        auto conf = getConferenceFromCallID(call_id);

        if (conf == nullptr) {
            RING_ERR("Call is not conferencing, cannot detach");
            return false;
        }

        std::map<std::string, std::string> call_details(getCallDetails(call_id));
        std::map<std::string, std::string>::iterator iter_details(call_details.find("CALL_STATE"));

        if (iter_details == call_details.end()) {
            RING_ERR("Could not find CALL_STATE");
            return false;
        }

        // Don't hold ringing calls when detaching them from conferences
        if (iter_details->second != "RINGING")
            onHoldCall(call_id);

        removeParticipant(call_id);

    } else {
        RING_DBG("Unbind main participant from conference %d");
        getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);

        if (not isConference(current_call_id)) {
            RING_ERR("Current call id (%s) is not a conference", current_call_id.c_str());
            return false;
        }

        ConferenceMap::iterator iter = conferenceMap_.find(current_call_id);

        auto conf = iter->second;
        if (iter == conferenceMap_.end() or conf == 0) {
            RING_DBG("Conference is NULL");
            return false;
        }

        if (conf->getState() == Conference::ACTIVE_ATTACHED)
            conf->setState(Conference::ACTIVE_DETACHED);
        else if (conf->getState() == Conference::ACTIVE_ATTACHED_REC)
            conf->setState(Conference::ACTIVE_DETACHED_REC);
        else
            RING_WARN("Undefined behavior, invalid conference state in detach participant");

        emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

        unsetCurrentCall();
    }

    return true;
}

void
ManagerImpl::removeParticipant(const std::string& call_id)
{
    RING_DBG("Remove participant %s", call_id.c_str());

    // this call is no longer a conference participant
    auto call = getCallFromCallID(call_id);
    if (!call) {
        RING_ERR("Call not found");
        return;
    }

    ConferenceMap::const_iterator iter = conferenceMap_.find(call->getConfId());

    auto conf = iter->second;
    if (iter == conferenceMap_.end() or conf == 0) {
        RING_ERR("No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    conf->remove(call_id);
    call->setConfId("");

    removeStream(*call);

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

    processRemainingParticipants(*conf);
}

void
ManagerImpl::processRemainingParticipants(Conference &conf)
{
    const std::string current_call_id(getCurrentCallId());
    ParticipantSet participants(conf.getParticipantList());
    const size_t n = participants.size();
    RING_DBG("Process remaining %d participant(s) from conference %s",
          n, conf.getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto &p : participants)
            getRingBufferPool().flush(p);

        getRingBufferPool().flush(RingBufferPool::DEFAULT_ID);
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

        RING_DBG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
    } else {
        RING_DBG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
        unsetCurrentCall();
    }
}

bool
ManagerImpl::joinConference(const std::string& conf_id1,
                            const std::string& conf_id2)
{
    if (conferenceMap_.find(conf_id1) == conferenceMap_.end()) {
        RING_ERR("Not a valid conference ID: %s", conf_id1.c_str());
        return false;
    }

    if (conferenceMap_.find(conf_id2) == conferenceMap_.end()) {
        RING_ERR("Not a valid conference ID: %s", conf_id2.c_str());
        return false;
    }

    auto conf = conferenceMap_.find(conf_id1)->second;
    ParticipantSet participants(conf->getParticipantList());

    for (const auto &p : participants)
        addParticipant(p, conf_id2);

    return true;
}

void
ManagerImpl::addStream(Call& call)
{
    const auto call_id = call.getCallId();
    RING_DBG("Add audio stream %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        RING_DBG("Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = conferenceMap_.find(call_id);
        if (iter != conferenceMap_.end() and iter->second) {
            auto conf = iter->second;
            conf->bindParticipant(call_id);
        }
    } else {
        RING_DBG("Add stream to call");

        // bind to main
        getRingBufferPool().bindCallID(call_id, RingBufferPool::DEFAULT_ID);

        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        if (!audiodriver_) {
            RING_ERR("Audio driver not initialized");
            return;
        }
        audiodriver_->flushUrgent();
        audiodriver_->flushMain();
    }
    startAudioDriverStream();
}

void
ManagerImpl::removeStream(Call& call)
{
    const auto call_id = call.getCallId();
    RING_DBG("Remove audio stream %s", call_id.c_str());
    getRingBufferPool().unBindAll(call_id);
}

// Not thread-safe, SHOULD be called in same thread that run poolEvents()
void
ManagerImpl::registerEventHandler(uintptr_t handlerId, EventHandler handler)
{
    eventHandlerMap_[handlerId] = handler;
}

// Not thread-safe, SHOULD be called in same thread that run poolEvents()
void
ManagerImpl::unregisterEventHandler(uintptr_t handlerId)
{
    auto iter = eventHandlerMap_.find(handlerId);
    if (iter != eventHandlerMap_.end()) {
        if (iter == nextEventHandler_)
            nextEventHandler_ = eventHandlerMap_.erase(iter);
        else
            eventHandlerMap_.erase(iter);
    }
}

// Not thread-safe, SHOULD be called in same thread that run poolEvents()
void
ManagerImpl::addTask(const std::function<bool()>&& task)
{
    pendingTaskList_.emplace_back(task);
}

// Must be invoked periodically by a timer from the main event loop
void ManagerImpl::pollEvents()
{
    //-- Handlers
    {
        auto iter = eventHandlerMap_.begin();
        while (iter != eventHandlerMap_.end()) {
            if (finished_)
                return;

            // WARN: following callback can do anything and typically
            // calls (un)registerEventHandler.
            // Think twice before modify this code.

            nextEventHandler_ = std::next(iter);
            iter->second();
            iter = nextEventHandler_;
        }
    }

    //-- Tasks
    {
        auto tmpList = std::move(pendingTaskList_);
        pendingTaskList_.clear();
        auto iter = std::begin(tmpList);
        while (iter != tmpList.cend()) {
            if (finished_)
                return;

            auto next = std::next(iter);
            if (not (*iter)())
                tmpList.erase(iter);
            iter = next;
        }
        pendingTaskList_.splice(std::end(pendingTaskList_), tmpList);
    }
}

//THREAD=Main
void
ManagerImpl::saveConfig()
{
    RING_DBG("Saving Configuration to XDG directory %s", path_.c_str());

    if (audiodriver_) {
        audioPreference.setVolumemic(audiodriver_->getCaptureGain());
        audioPreference.setVolumespkr(audiodriver_->getPlaybackGain());
        audioPreference.setCaptureMuted(audiodriver_->isCaptureMuted());
        audioPreference.setPlaybackMuted(audiodriver_->isPlaybackMuted());
    }

    try {
        YAML::Emitter out;

        // FIXME maybe move this into accountFactory?
        out << YAML::BeginMap << YAML::Key << "accounts";
        out << YAML::Value << YAML::BeginSeq;

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
#ifdef RING_VIDEO
        getVideoDeviceMonitor().serialize(out);
#endif
        shortcutPreferences.serialize(out);

        std::ofstream fout(path_);
        fout << out.c_str();
    } catch (const YAML::Exception &e) {
        RING_ERR("%s", e.what());
    } catch (const std::runtime_error &e) {
        RING_ERR("%s", e.what());
    }
}

//THREAD=Main | VoIPLink
void
ManagerImpl::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        RING_DBG("Do not have to play a tone...");
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        RING_DBG("Pulse length is not set...");
        return;
    }

    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

    // fast return, no sound, so no dtmf
    if (not audiodriver_ or not dtmfKey_) {
        RING_DBG("No audio layer...");
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
            RING_WARN("Audio layer not ready yet");
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

    emitSignal<DRing::CallSignal::IncomingCall>(accountId, callID, call.getDisplayName() + " " + from);
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

            RING_DBG("Send message to %s", item_p.c_str());

            if (auto call = getCallFromCallID(item_p)) {
                call->sendTextMessage(message, from);
            } else {
                RING_ERR("Failed to get call while sending instant message");
                return;
            }
        }

        // in case of a conference we must notify client using conference id
        emitSignal<DRing::CallSignal::IncomingMessage>(conf->getConfID(), from, message);
    } else
        emitSignal<DRing::CallSignal::IncomingMessage>(callID, from, message);
}

//THREAD=VoIP
bool
ManagerImpl::sendTextMessage(const std::string& callID,
                             const std::string& message,
                             const std::string& from)
{
    if (isConference(callID)) {
        RING_DBG("Is a conference, send instant message to everyone");
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
                RING_ERR("Failed to get call while sending instant message");
                return false;
            }
        }

        return true;
    }

    if (isConferenceParticipant(callID)) {
        RING_DBG("Call is participant in a conference, send instant message to everyone");
        auto conf = getConferenceFromCallID(callID);

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (const auto &participant_id : participants) {

            if (auto call = getCallFromCallID(participant_id)) {
                call->sendTextMessage(message, from);
            } else {
                RING_ERR("Failed to get call while sending instant message");
                return false;
            }
        }
    } else {
        if (auto call = getCallFromCallID(callID)) {
            call->sendTextMessage(message, from);
        } else {
            RING_ERR("Failed to get call while sending instant message");
            return false;
        }
    }
    return true;
}
#endif // HAVE_INSTANT_MESSAGING

//THREAD=VoIP CALL=Outgoing
void
ManagerImpl::peerAnsweredCall(Call& call)
{
    const auto call_id = call.getCallId();
    RING_DBG("Peer answered call %s", call_id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(call))
        stopTone();

    // Connect audio streams
    addStream(call);

    if (audiodriver_) {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);
        audiodriver_->flushMain();
        audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);

    emitSignal<DRing::CallSignal::StateChange>(call_id, "CURRENT", 0);
}

//THREAD=VoIP Call=Outgoing
void
ManagerImpl::peerRingingCall(Call& call)
{
    const auto call_id = call.getCallId();
    RING_DBG("Peer call %s ringing", call_id.c_str());

    if (isCurrentCall(call))
        ringback();

    emitSignal<DRing::CallSignal::StateChange>(call_id, "RINGING", 0);
}

//THREAD=VoIP Call=Outgoing/Ingoing
void
ManagerImpl::peerHungupCall(Call& call)
{
    const auto call_id = call.getCallId();
    RING_DBG("Peer hungup call %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        removeParticipant(call_id);
    } else if (isCurrentCall(call)) {
        stopTone();
        unsetCurrentCall();
    }

    call.peerHungup();

    emitSignal<DRing::CallSignal::StateChange>(call_id, "HUNGUP", 0);

    checkAudio();
    removeWaitingCall(call_id);
    if (not incomingCallsWaiting())
        stopTone();

    removeStream(call);
}

//THREAD=VoIP
void
ManagerImpl::callBusy(Call& call)
{
    const auto call_id = call.getCallId();

    emitSignal<DRing::CallSignal::StateChange>(call_id, "BUSY", 0);

    if (isCurrentCall(call)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    checkAudio();
    removeWaitingCall(call_id);
}

//THREAD=VoIP
void
ManagerImpl::callFailure(Call& call, int code)
{
    const auto call_id = call.getCallId();

    emitSignal<DRing::CallSignal::StateChange>(call_id, "FAILURE", code);

    if (isCurrentCall(call)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    if (isConferenceParticipant(call_id)) {
        RING_DBG("Call %s participating in a conference failed", call_id.c_str());
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
    emitSignal<DRing::CallSignal::VoiceMailNotify>(accountId, nb_msg);
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
            RING_ERR("Audio layer not initialized");
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

        emitSignal<DRing::CallSignal::RecordPlaybackStopped>(filepath);
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
        RING_WARN("Invalid account in ringtone");
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
            RING_ERR("no audio layer in ringtone");
            return;
        }

        audioLayerSmplr = audiodriver_->getSampleRate();
    }

    bool doFallback = false;

    {
        std::lock_guard<std::mutex> m(toneMutex_);

        if (audiofile_) {
            emitSignal<DRing::CallSignal::RecordPlaybackStopped>(audiofile_->getFilePath());
            audiofile_.reset();
        }

        try {
            updateAudioFile(ringchoice, audioLayerSmplr);
        } catch (const AudioFileException &e) {
            RING_WARN("Ringtone error: %s", e.what());
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
    std::string configdir = "/data/data/cx.ring";
#elif __APPLE__
    std::string configdir = fileutils::get_home_dir() + DIR_SEPARATOR_STR
        + "Library" + DIR_SEPARATOR_STR + "Application Support"
        + DIR_SEPARATOR_STR + PACKAGE;
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
            RING_DBG("Cannot create directory: %s!", configdir.c_str());
    }

    static const char * const PROGNAME = "dring";
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
        RING_ERR("No audio layer created, possibly built without audio support");
}

/**
 * Set audio output device
 */
void
ManagerImpl::setAudioDevice(int index, DeviceType type)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (not audiodriver_) {
        RING_ERR("Audio driver not initialized");
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

bool
ManagerImpl::switchInput(const std::string& call_id, const std::string& res)
{
    auto call = getCallFromCallID(call_id);
    if (!call) {
        RING_ERR("Call %s is NULL", call_id.c_str());
        return false;
    }
    call->switchInput(res);
    return true;
}

int
ManagerImpl::isRingtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        RING_WARN("Invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void
ManagerImpl::ringtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        RING_WARN("Invalid account in ringtone enabled");
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
        RING_DBG("toggle recording for call %s", id.c_str());
        rec = getCallFromCallID(id);
    } else {
        RING_DBG("toggle recording for conference %s", id.c_str());
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
        RING_ERR("Could not find recordable instance %s", id.c_str());
        return false;
    }

    const bool result = rec->toggleRecording();
    emitSignal<DRing::CallSignal::RecordPlaybackFilepath>(id, rec->getFilename());
    emitSignal<DRing::CallSignal::RecordingStateChanged>(id, result);
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
    RING_DBG("Start recorded file playback %s", filepath.c_str());

    int sampleRate;
    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_) {
            RING_ERR("No audio layer in start recorded file playback");
            return false;
        }

        sampleRate = audiodriver_->getSampleRate();
    }

    {
        std::lock_guard<std::mutex> m(toneMutex_);

        if (audiofile_) {
            emitSignal<DRing::CallSignal::RecordPlaybackStopped>(audiofile_->getFilePath());
            audiofile_.reset();
        }

        try {
            updateAudioFile(filepath, sampleRate);
            if (not audiofile_)
                return false;
        } catch (const AudioFileException &e) {
            RING_WARN("Audio file error: %s", e.what());
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
    RING_DBG("Stop recorded file playback %s", filepath.c_str());

    checkAudio();

    {
        std::lock_guard<std::mutex> m(toneMutex_);
        audiofile_.reset();
    }
    emitSignal<DRing::CallSignal::RecordPlaybackStopped>(filepath);
}

void ManagerImpl::setHistoryLimit(int days)
{
    RING_DBG("Set history limit");
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
            RING_DBG("Audio manager chosen already in use. No changes made. ");
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
        RING_ERR("Audio layer not initialized");
        return 0;
    }

    return audiodriver_->getAudioDeviceIndex(name, DeviceType::CAPTURE);
}

int
ManagerImpl::getAudioOutputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);

    if (not audiodriver_) {
        RING_ERR("Audio layer not initialized");
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

AudioFormat
ManagerImpl::hardwareAudioFormatChanged(AudioFormat format)
{
    return audioFormatUsed(format);
}

AudioFormat
ManagerImpl::audioFormatUsed(AudioFormat format)
{
    AudioFormat currentFormat = ringbufferpool_->getInternalAudioFormat();
    format.nb_channels = std::max(currentFormat.nb_channels, std::min(format.nb_channels, 2u)); // max 2 channels.
    format.sample_rate = std::max(currentFormat.sample_rate, format.sample_rate);

    if (currentFormat == format)
        return format;

    RING_DBG("Audio format changed: %s -> %s", currentFormat.toString().c_str(), format.toString().c_str());

    ringbufferpool_->setInternalAudioFormat(format);

    {
        std::lock_guard<std::mutex> toneLock(toneMutex_);
        telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), format.sample_rate));
    }
    dtmfKey_.reset(new DTMF(format.sample_rate));
    return format;
}

void
ManagerImpl::setAccountsOrder(const std::string& order)
{
    RING_DBG("Set accounts order : %s", order.c_str());
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
        RING_ERR("could not find IP2IP profile in getAccount list");

    return v;
}

std::map<std::string, std::string>
ManagerImpl::getAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getAccountDetails();
    } else {
        RING_ERR("Could not get account details on a non-existing accountID %s", accountID.c_str());
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
        RING_ERR("Could not get volatile account details on a non-existing accountID %s", accountID.c_str());
        return {{}};
    }
}


// method to reduce the if/else mess.
// Even better, switch to XML !

void
ManagerImpl::setAccountDetails(const std::string& accountID,
                               const std::map<std::string, std::string>& details)
{
    RING_DBG("Set account details for %s", accountID.c_str());

    const auto account = getAccount(accountID);

    if (account == nullptr) {
        RING_ERR("Could not find account %s", accountID.c_str());
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

        if (account->isEnabled()) {
            account->doRegister();
        } else
            account->doUnregister();

        // Update account details to the client side
        emitSignal<DRing::ConfigurationSignal::AccountsChanged>();
    });
}

std::string
ManagerImpl::addAccount(const std::map<std::string, std::string>& details)
{
    /** @todo Deal with both the accountMap_ and the Configuration */
    std::string newAccountID;
    static std::uniform_int_distribution<uint64_t> rand_acc_id;

    const std::vector<std::string> accountList(getAccountList());

    do {
        std::ostringstream accId;
        accId << std::hex << rand_acc_id(rand_);
        newAccountID = accId.str();
    } while (std::find(accountList.begin(), accountList.end(), newAccountID)
             != accountList.end());

    // Get the type

    const char* accountType;
    if (details.find(Conf::CONFIG_ACCOUNT_TYPE) != details.end())
        accountType = (*details.find(Conf::CONFIG_ACCOUNT_TYPE)).second.c_str();
    else
        accountType = AccountFactory::DEFAULT_ACCOUNT_TYPE;

    RING_DBG("Adding account %s", newAccountID.c_str());

    auto newAccount = accountFactory_.createAccount(accountType, newAccountID);
    if (!newAccount) {
        RING_ERR("Unknown %s param when calling addAccount(): %s",
              Conf::CONFIG_ACCOUNT_TYPE, accountType);
        return "";
    }

    newAccount->setAccountDetails(details);

    preferences.addAccount(newAccountID);

    newAccount->doRegister();

    saveConfig();

    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();

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

    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();
}

bool
ManagerImpl::isValidCall(const std::string& callID)
{
    return static_cast<bool>(getCallFromCallID(callID));
}

std::string
ManagerImpl::getNewCallID()
{
    static std::uniform_int_distribution<uint64_t> rand_call_id;
    std::ostringstream random_id;

    // generate something like s7ea037947eb9fb2f
    do {
        random_id.clear();
        random_id << rand_call_id(rand_);
    } while (isValidCall(random_id.str()));

    return random_id.str();
}

std::vector<std::string>
ManagerImpl::loadAccountOrder() const
{
    return split_string(preferences.getAccountOrder(), '/');
}

void
ManagerImpl::loadAccount(const YAML::Node &node, int &errorCount,
                         const std::string &accountOrder)
{
    using yaml_utils::parseValue;

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
            RING_WARN("Dropping account %s, which is not in account order", accountid.c_str());
        } else if (accountFactory_.isSupportedType(accountType.c_str())) {
            std::shared_ptr<Account> a;
            if (accountid != ip2ipAccountID)
                a = accountFactory_.createAccount(accountType.c_str(), accountid);
            else
                a = accountFactory_.getIP2IPAccount();
            if (a) {
                a->unserialize(node);
            } else {
                RING_ERR("Failed to create account type \"%s\"", accountType.c_str());
                ++errorCount;
            }
        } else {
            RING_WARN("Ignoring unknown account type \"%s\"", accountType.c_str());
        }
    }
}

int
ManagerImpl::loadAccountMap(const YAML::Node &node)
{
    accountFactory_.initIP2IPAccount();

    // build preferences
    preferences.unserialize(node);
    voipPreferences.unserialize(node);
    hookPreference.unserialize(node);
    audioPreference.unserialize(node);
    shortcutPreferences.unserialize(node);

    int errorCount = 0;
    try {
#ifdef RING_VIDEO
        getVideoDeviceMonitor().unserialize(node);
#endif
    } catch (const YAML::Exception &e) {
        RING_ERR("%s: No video node in config file", e.what());
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
        RING_ERR("Call is NULL");
        // FIXME: is this even useful?
        return Call::getNullDetails();
    }
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
        RING_WARN("Did not find conference %s", confID.c_str());
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
        RING_WARN("Did not find conference %s", confID.c_str());

    return v;
}

std::string
ManagerImpl::getConferenceId(const std::string& callID)
{
    if (auto call = getCallFromCallID(callID))
        return call->getConfId();

    RING_ERR("Call is NULL");
    return "";
}

void
ManagerImpl::startAudioDriverStream()
{
    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    if (!audiodriver_) {
        RING_ERR("Audio driver not initialized");
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

        if (a->isEnabled()) {
            a->doRegister();
        }
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

    if (acc->isEnabled()) {
        acc->doRegister();
    } else
        acc->doUnregister();
}

std::shared_ptr<AudioLayer>
ManagerImpl::getAudioDriver()
{
    return audiodriver_;
}

std::shared_ptr<Call>
ManagerImpl::newOutgoingCall(const std::string& toUrl,
                             const std::string& preferredAccountId)
{
    auto account = Manager::instance().getIP2IPAccount();
    auto preferred = getAccount(preferredAccountId);
    std::string finalToUrl = toUrl;

#if HAVE_DHT
    if (toUrl.find("ring:") != std::string::npos) {
        if (preferred && preferred->getAccountType() == RingAccount::ACCOUNT_TYPE)
            return preferred->newOutgoingCall(finalToUrl);
        auto dhtAcc = getAllAccounts<RingAccount>();
        for (const auto& acc : dhtAcc)
            if (acc->isEnabled())
                return acc->newOutgoingCall(finalToUrl);
    }
#endif

    // FIXME: have a generic version to remove sip dependency
    sip_utils::stripSipUriPrefix(finalToUrl);

    if (!IpAddr::isValid(finalToUrl)) {
        account = preferred;
        if (account)
            finalToUrl = toUrl;
        else
            RING_WARN("Preferred account %s doesn't exist, using IP2IP account",
                 preferredAccountId.c_str());
    } else
        RING_WARN("IP Url detected, using IP2IP account");

    if (!account) {
        RING_ERR("No suitable account found to create outgoing call");
        return nullptr;
    }

    return account->newOutgoingCall(finalToUrl);
}

#ifdef RING_VIDEO
std::shared_ptr<video::SinkClient>
ManagerImpl::createSinkClient(const std::string& id)
{
    const auto& iter = sinkMap_.find(id);
    if (iter != std::end(sinkMap_)) {
        if (iter->second.expired())
            sinkMap_.erase(iter);
        else
            return nullptr;
    }

    auto sink = std::make_shared<video::SinkClient>(id);
    sinkMap_.emplace(id, sink);
    return sink;
}

std::shared_ptr<video::SinkClient>
ManagerImpl::getSinkClient(const std::string& id)
{
    const auto& iter = sinkMap_.find(id);
    if (iter != std::end(sinkMap_))
        if (auto sink = iter->second.lock())
            return sink;
    return nullptr;
}
#endif // RING_VIDEO

} // namespace ring
