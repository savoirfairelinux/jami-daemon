/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"

#include "logger.h"
#include "account_schema.h"

#include "fileutils.h"
#include "map_utils.h"
#include "account.h"
#include "string_utils.h"
#include "jamidht/jamiaccount.h"
#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include "call_factory.h"

#include "sip/sip_utils.h"
#include "sip/sipvoiplink.h"
#include "sip/sipaccount.h"

#include "im/instant_messaging.h"

#include "config/yamlparser.h"

#if HAVE_ALSA
#include "audio/alsa/alsalayer.h"
#endif

#include "media/localrecordermanager.h"
#include "audio/sound/tonelist.h"
#include "audio/sound/dtmf.h"
#include "audio/ringbufferpool.h"

#ifdef ENABLE_VIDEO
#include "client/videomanager.h"
#include "video/video_scaler.h"
#endif

#include "conference.h"
#include "ice_transport.h"

#include "client/ring_signal.h"
#include "dring/call_const.h"
#include "dring/account_const.h"

#include "libav_utils.h"
#include "video/sinkclient.h"
#include "audio/tonecontrol.h"

#include "data_transfer.h"

#include <opendht/thread_pool.h>

#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>

#ifndef WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef TARGET_OS_IOS
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <cerrno>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <mutex>
#include <list>
#include <random>

namespace jami {

/** To store conference objects by conference ids */
using ConferenceMap = std::map<std::string, std::shared_ptr<Conference>>;

/** To store uniquely a list of Call ids */
using CallIDSet = std::set<std::string>;

static constexpr std::chrono::seconds ICE_INIT_TIMEOUT{10};
static constexpr const char* PACKAGE_OLD = "ring";

std::atomic_bool Manager::initialized = {false};

static void
copy_over(const std::string &srcPath, const std::string &destPath)
{
    std::ifstream src = fileutils::ifstream(srcPath.c_str());
    std::ofstream dest = fileutils::ofstream(destPath.c_str());
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
check_rename(const std::string& old_dir, const std::string& new_dir)
{
    if (old_dir == new_dir or not fileutils::isDirectory(old_dir))
        return;

    if (not fileutils::isDirectory(new_dir)) {
        JAMI_WARN() << "Migrating " << old_dir << " to " << new_dir;
        std::rename(old_dir.c_str(), new_dir.c_str());
    } else {
        for (const auto &file : fileutils::readDirectory(old_dir)) {
            auto old_dest = fileutils::getFullPath(old_dir, file);
            auto new_dest = fileutils::getFullPath(new_dir, file);
            if (fileutils::isDirectory(old_dest) and fileutils::isDirectory(new_dest)) {
                check_rename(old_dest, new_dest);
            } else {
                JAMI_WARN() << "Migrating " << old_dest << " to " << new_dest;
                std::rename(old_dest.c_str(), new_dest.c_str());
            }
        }
        fileutils::removeAll(old_dir);
    }
}

/**
 * Set OpenDHT's log level based on the DHTLOGLEVEL environment variable.
 * DHTLOGLEVEL = 0 minimum logging (=disable)
 * DHTLOGLEVEL = 1 (=ERROR only)
 * DHTLOGLEVEL = 2 (+=WARN)
 * DHTLOGLEVEL = 3 maximum logging (+=DEBUG)
 */

/** Environment variable used to set OpenDHT's logging level */
static constexpr const char* DHTLOGLEVEL = "DHTLOGLEVEL";

static void
setDhtLogLevel()
{
#ifndef RING_UWP
    char* envvar = getenv(DHTLOGLEVEL);
    int level = 0;

    if (envvar != nullptr) {
        if (not (std::istringstream(envvar) >> level))
            level = 0;

        // From 0 (min) to 3 (max)
        level = std::max(0, std::min(level, 3));
        JAMI_DBG("DHTLOGLEVEL=%u", level);
    }
    Manager::instance().dhtLogLevel = level;
#else
    Manager::instance().dhtLogLevel = 0;
#endif
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
#ifndef RING_UWP
    char* envvar = getenv(SIPLOGLEVEL);

    int level = 0;

    if (envvar != nullptr) {
        if (not (std::istringstream(envvar) >> level))
            level = 0;

        // From 0 (min) to 6 (max)
        level = std::max(0, std::min(level, 6));
    }
#else
    int level = 0;
#endif

    pj_log_set_level(level);
    pj_log_set_log_func([](int level, const char *data, int /*len*/) {
        if      (level < 2) JAMI_ERR() << data;
        else if (level < 4) JAMI_WARN() << data;
        else                JAMI_DBG() << data;
    });
}

/**
 * Set gnutls's log level based on the RING_TLS_LOGLEVEL environment variable.
 * RING_TLS_LOGLEVEL = 0 minimum logging (default)
 * RING_TLS_LOGLEVEL = 9 maximum logging
 */

static constexpr int RING_TLS_LOGLEVEL = 0;

static void
tls_print_logs(int level, const char* msg)
{
    JAMI_XDBG("[%d]GnuTLS: %s", level, msg);
}

static void
setGnuTlsLogLevel()
{
#ifndef RING_UWP
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
#else
    gnutls_global_set_log_level(RING_TLS_LOGLEVEL);
#endif
    gnutls_global_set_log_function(tls_print_logs);
}

//==============================================================================

struct Manager::ManagerPimpl
{
    explicit ManagerPimpl(Manager& base);

    bool parseConfiguration();

    /*
     * Play one tone
     * @return false if the driver is uninitialize
     */
    void playATone(Tone::TONEID toneId);

    int getCurrentDeviceIndex(DeviceType type);

    /**
     * Process remaining participant given a conference and the current call id.
     * Mainly called when a participant is detached or hagned up
     * @param current call id
     * @param conference pointer
     */
    void processRemainingParticipants(Conference &conf);

    /**
     * Create config directory in home user and return configuration file path
     */
    std::string retrieveConfigPath() const;

    void unsetCurrentCall();

    void switchCall(const std::string& id);
    void switchCall(const std::shared_ptr<Call>& call);

    /**
     * Add incoming callid to the waiting list
     * @param id std::string to add
     */
    void addWaitingCall(const std::string& id);

    /**
     * Remove incoming callid to the waiting list
     * @param id std::string to remove
     */
    void removeWaitingCall(const std::string& id);

    void loadAccount(const YAML::Node &item, int &errorCount);

    void sendTextMessageToConference(const Conference& conf,
                                     const std::map<std::string, std::string>& messages,
                                     const std::string& from) const noexcept;

    void bindCallToConference(Call& call, Conference& conf);

    template <class T>
    std::shared_ptr<T> findAccount(const std::function<bool(const std::shared_ptr<T>&)>&);

    Manager& base_; // pimpl back-pointer

    std::shared_ptr<asio::io_context> ioContext_;
    std::thread ioContextRunner_;

    /** Main scheduler */
    ScheduledExecutor scheduler_;

    std::atomic_bool autoAnswer_ {false};

    /** Application wide tone controller */
    ToneControl toneCtrl_;

    /** Current Call ID */
    std::string currentCall_;

    /** Protected current call access */
    std::mutex currentCallMutex_;

    /** Audio layer */
    std::shared_ptr<AudioLayer> audiodriver_{nullptr};

    // Main thread
    std::unique_ptr<DTMF> dtmfKey_;

    /** Buffer to generate DTMF */
    AudioBuffer dtmfBuf_;

    // To handle volume control
    // short speakerVolume_;
    // short micVolume_;
    // End of sound variable

    /**
     * Mutex used to protect audio layer
     */
    std::mutex audioLayerMutex_;

    /**
     * Waiting Call Vectors
     */
    CallIDSet waitingCalls_;

    /**
     * Protect waiting call list, access by many voip/audio threads
     */
    std::mutex waitingCallsMutex_;

    /**
     * Path of the ConfigFile
     */
    std::string path_;

    /**
     * Instance of the RingBufferPool for the whole application
     *
     * In order to send signal to other parts of the application, one must pass through the RingBufferMananger.
     * Audio instances must be registered into the RingBufferMananger and bound together via the Manager.
     *
     */
    std::unique_ptr<RingBufferPool> ringbufferpool_;

    // Map containing conference pointers
    ConferenceMap conferenceMap_;

    std::atomic_bool finished_ {false};

    std::mt19937_64 rand_;

    /* ICE support */
    std::unique_ptr<IceTransportFactory> ice_tf_;

    /* Sink ID mapping */
    std::map<std::string, std::weak_ptr<video::SinkClient>> sinkMap_;

#ifdef ENABLE_VIDEO
    std::unique_ptr<VideoManager> videoManager_;
#endif
};

Manager::ManagerPimpl::ManagerPimpl(Manager& base)
    : base_(base)
    , ioContext_(std::make_shared<asio::io_context>())
    , toneCtrl_(base.preferences)
    , dtmfBuf_(0, AudioFormat::MONO())
    , ringbufferpool_(new RingBufferPool)
    , rand_(dht::crypto::getSeededRandomEngine<std::mt19937_64>())
#ifdef ENABLE_VIDEO
    , videoManager_(new VideoManager)
#endif
{
    jami::libav_utils::ring_avcodec_init();

    ioContextRunner_ = std::thread([context = ioContext_](){
        try {
            auto work = asio::make_work_guard(*context);
            context->run();
        } catch (const std::exception& ex) {
            JAMI_ERR("Unexpected io_context thread exception: %s", ex.what());
        }
    });
}

bool
Manager::ManagerPimpl::parseConfiguration()
{
    bool result = true;

    try {
        std::ifstream file = fileutils::ifstream(path_);
        YAML::Node parsedFile = YAML::Load(file);
        file.close();
        const int error_count = base_.loadAccountMap(parsedFile);

        if (error_count > 0) {
            JAMI_WARN("Errors while parsing %s", path_.c_str());
            result = false;
        }
    } catch (const YAML::BadFile &e) {
        JAMI_WARN("Could not open configuration file");
        result = false;
    }

    return result;
}

/**
 * Multi Thread
 */
void
Manager::ManagerPimpl::playATone(Tone::TONEID toneId)
{
    if (not base_.voipPreferences.getPlayTones())
        return;

    {
        std::lock_guard<std::mutex> lock(audioLayerMutex_);

        if (not audiodriver_) {
            JAMI_ERR("Audio layer not initialized");
            return;
        }

        audiodriver_->flushUrgent();
        audiodriver_->startStream();
    }

    toneCtrl_.play(toneId);
}

int
Manager::ManagerPimpl::getCurrentDeviceIndex(DeviceType type)
{
    if (not audiodriver_)
        return -1;
    switch (type) {
        case DeviceType::PLAYBACK:
            return audiodriver_->getIndexPlayback();
        case DeviceType::RINGTONE:
            return audiodriver_->getIndexRingtone();
        case DeviceType::CAPTURE:
            return audiodriver_->getIndexCapture();
        default:
            return -1;
    }
}

void
Manager::ManagerPimpl::processRemainingParticipants(Conference& conf)
{
    const std::string current_call_id(base_.getCurrentCallId());
    ParticipantSet participants(conf.getParticipantList());
    const size_t n = participants.size();
    JAMI_DBG("Process remaining %zu participant(s) from conference %s",
          n, conf.getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto &p : participants)
            base_.getRingBufferPool().flush(p);

        base_.getRingBufferPool().flush(RingBufferPool::DEFAULT_ID);
    } else if (n == 1) {
        // this call is the last participant, hence
        // the conference is over
        auto p = participants.begin();
        if (auto call = base_.getCallFromCallID(*p)) {
            call->setConfId("");
            // if we are not listening to this conference
            if (current_call_id != conf.getConfID())
                base_.onHoldCall(call->getCallId());
            else
                switchCall(call);
        }

        JAMI_DBG("No remaining participants, remove conference");
        base_.removeConference(conf.getConfID());
    } else {
        JAMI_DBG("No remaining participants, remove conference");
        base_.removeConference(conf.getConfID());
        unsetCurrentCall();
    }
}

/**
 * Initialization: Main Thread
 */
std::string
Manager::ManagerPimpl::retrieveConfigPath() const
{
    static const char * const PROGNAME = "dring";
    return fileutils::get_config_dir() + DIR_SEPARATOR_STR + PROGNAME + ".yml";
}

void
Manager::ManagerPimpl::unsetCurrentCall()
{
    currentCall_ = "";
}

void
Manager::ManagerPimpl::switchCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(currentCallMutex_);
    JAMI_DBG("----- Switch current call id to '%s' -----", not id.empty() ? id.c_str() : "none");
    currentCall_ = id;
}

void
Manager::ManagerPimpl::switchCall(const std::shared_ptr<Call>& call)
{
    switchCall(call->getCallId());
}

void
Manager::ManagerPimpl::addWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.insert(id);
}

void
Manager::ManagerPimpl::removeWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.erase(id);
}

void
Manager::ManagerPimpl::loadAccount(const YAML::Node &node, int &errorCount)
{
    using yaml_utils::parseValue;

    std::string accountType;
    parseValue(node, "type", accountType);

    std::string accountid;
    parseValue(node, "id", accountid);

    if (!accountid.empty()) {
        if (base_.accountFactory.isSupportedType(accountType.c_str())) {
            if (auto a = base_.accountFactory.createAccount(accountType.c_str(), accountid)) {
                a->unserialize(node);
            } else {
                JAMI_ERR("Failed to create account type \"%s\"", accountType.c_str());
                ++errorCount;
            }
        } else {
            JAMI_WARN("Ignoring unknown account type \"%s\"", accountType.c_str());
        }
    }
}

//THREAD=VoIP
void
Manager::ManagerPimpl::sendTextMessageToConference(const Conference& conf,
                                     const std::map<std::string, std::string>& messages,
                                     const std::string& from) const noexcept
{
    ParticipantSet participants(conf.getParticipantList());
    for (const auto& call_id: participants) {
        try {
            auto call = base_.getCallFromCallID(call_id);
            if (not call)
                throw std::runtime_error("no associated call");
            call->sendTextMessage(messages, from);
        } catch (const std::exception& e) {
            JAMI_ERR("Failed to send message to conference participant %s: %s",
                     call_id.c_str(), e.what());
        }
    }
}

void
Manager::ManagerPimpl::bindCallToConference(Call& call, Conference& conf)
{
    const auto& call_id = call.getCallId();
    const auto& conf_id = conf.getConfID();
    const auto& state = call.getStateStr();

    // ensure that calls are only in one conference at a time
    if (base_.isConferenceParticipant(call_id))
        base_.detachParticipant(call_id);

    JAMI_DBG("[call:%s] bind to conference %s (callState=%s)",
             call_id.c_str(), conf_id.c_str(), state.c_str());

    base_.getRingBufferPool().unBindAll(call_id);

    conf.add(call_id);
    call.setConfId(conf_id);

    if (state == "HOLD") {
        conf.bindParticipant(call_id);
        base_.offHoldCall(call_id);
    } else if (state == "INCOMING") {
        conf.bindParticipant(call_id);
        base_.answerCall(call_id);
    } else if (state == "CURRENT") {
        conf.bindParticipant(call_id);
    } else if (state == "INACTIVE") {
        conf.bindParticipant(call_id);
        base_.answerCall(call_id);
    } else
        JAMI_WARN("[call:%s] call state %s not recognized for conference",
                  call_id.c_str(), state.c_str());
}

//==============================================================================

Manager&
Manager::instance()
{
    // Meyers singleton
    static Manager instance;

    // This will give a warning that can be ignored the first time instance()
    // is called...subsequent warnings are more serious
    if (not Manager::initialized)
        JAMI_DBG("Not initialized");

    return instance;
}

Manager::Manager()
    : preferences()
    , voipPreferences()
    , hookPreference()
    , audioPreference()
    , shortcutPreferences()
#ifdef ENABLE_VIDEO
    , videoPreferences()
#endif
    , callFactory()
    , accountFactory()
    , dataTransfers(std::make_unique<DataTransferFacade>())
    , pimpl_ (new ManagerPimpl(*this))
{}

Manager::~Manager()
{}

void
Manager::setAutoAnswer(bool enable)
{
    pimpl_->autoAnswer_ = enable;
}

void
Manager::init(const std::string &config_file)
{
    // FIXME: this is no good
    initialized = true;

#ifndef WIN32
    // Set the max number of open files.
    struct rlimit nofiles;
    if (getrlimit(RLIMIT_NOFILE, &nofiles) == 0) {
        if (nofiles.rlim_cur < nofiles.rlim_max && nofiles.rlim_cur < 1024u) {
            nofiles.rlim_cur = std::min<rlim_t>(nofiles.rlim_max, 8192u);
            setrlimit(RLIMIT_NOFILE, &nofiles);
        }
    }
#endif

#define PJSIP_TRY(ret) do {                                  \
        if ((ret) != PJ_SUCCESS)                               \
            throw std::runtime_error(#ret " failed");        \
    } while (0)

    srand(time(nullptr)); // to get random number for RANDOM_PORT

    // Initialize PJSIP (SIP and ICE implementation)
    PJSIP_TRY(pj_init());
    setSipLogLevel();
    PJSIP_TRY(pjlib_util_init());
    PJSIP_TRY(pjnath_init());
#undef PJSIP_TRY

    setGnuTlsLogLevel();

    JAMI_DBG("Using PJSIP version %s for %s", pj_get_version(), PJ_OS_NAME);
    JAMI_DBG("Using GnuTLS version %s", gnutls_check_version(nullptr));
    JAMI_DBG("Using OpenDHT version %s", dht::version());

    setDhtLogLevel();

    check_rename(fileutils::get_cache_dir(PACKAGE_OLD), fileutils::get_cache_dir());
    check_rename(fileutils::get_data_dir(PACKAGE_OLD), fileutils::get_data_dir());
    check_rename(fileutils::get_config_dir(PACKAGE_OLD), fileutils::get_config_dir());

    pimpl_->ice_tf_.reset(new IceTransportFactory());

    pimpl_->path_ = config_file.empty() ? pimpl_->retrieveConfigPath() : config_file;
    JAMI_DBG("Configuration file path: %s", pimpl_->path_.c_str());

    bool no_errors = true;

    // manager can restart without being recreated (android)
    pimpl_->finished_ = false;

    try {
        no_errors = pimpl_->parseConfiguration();
    } catch (const YAML::Exception &e) {
        JAMI_ERR("%s", e.what());
        no_errors = false;
    }

    // always back up last error-free configuration
    if (no_errors) {
        make_backup(pimpl_->path_);
    } else {
        // restore previous configuration
        JAMI_WARN("Restoring last working configuration");

        // keep a reference to sipvoiplink while destroying the accounts
        const auto sipvoiplink = getSIPVoIPLink();

        try {
            // remove accounts from broken configuration
            removeAccounts();
            restore_backup(pimpl_->path_);
            pimpl_->parseConfiguration();
        } catch (const YAML::Exception &e) {
            JAMI_ERR("%s", e.what());
            JAMI_WARN("Restoring backup failed");
        }
    }

    initAudioDriver();

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        if (pimpl_->audiodriver_) {
            pimpl_->toneCtrl_.setSampleRate(pimpl_->audiodriver_->getSampleRate());
            pimpl_->dtmfKey_.reset(new DTMF(getRingBufferPool().getInternalSamplingRate()));
        }
    }

    registerAccounts();
}

void
Manager::finish() noexcept
{
    bool expected = false;
    if (not pimpl_->finished_.compare_exchange_strong(expected, true))
        return;

    try {
        // Forbid call creation
        callFactory.forbid();

        // Hangup all remaining active calls
        JAMI_DBG("Hangup %zu remaining call(s)", callFactory.callCount());
        for (const auto call : callFactory.getAllCalls())
            hangupCall(call->getCallId());
        callFactory.clear();

        for (const auto &account : getAllAccounts<JamiAccount>()) {
            if (account->getRegistrationState() == RegistrationState::INITIALIZING)
                removeAccount(account->getAccountID(), true);
        }

        saveConfig();

        // Disconnect accounts, close link stacks and free allocated ressources
        unregisterAccounts();
        accountFactory.clear();

        {
            std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

            pimpl_->audiodriver_.reset();
        }

        pimpl_->ice_tf_.reset();

        // Flush remaining tasks (free lambda' with capture)
        pimpl_->scheduler_.stop();
        dht::ThreadPool::io().join();
        dht::ThreadPool::computation().join();

        pj_shutdown();

        if (!pimpl_->ioContext_->stopped()){
            pimpl_->ioContext_->reset(); // allow to finish
            pimpl_->ioContext_->stop();  // make thread stop
        }
        if (pimpl_->ioContextRunner_.joinable())
            pimpl_->ioContextRunner_.join();
    } catch (const VoipLinkException &err) {
        JAMI_ERR("%s", err.what());
    }
}

bool
Manager::isCurrentCall(const Call& call) const
{
    return pimpl_->currentCall_ == call.getCallId();
}

bool
Manager::hasCurrentCall() const
{
    return not pimpl_->currentCall_.empty();
}

std::shared_ptr<Call>
Manager::getCurrentCall() const
{
    return getCallFromCallID(pimpl_->currentCall_);
}

const std::string&
Manager::getCurrentCallId() const
{
    return pimpl_->currentCall_;
}

void
Manager::unregisterAccounts()
{
    for (const auto& account : getAllAccounts()) {
        if (account->isEnabled())
            account->doUnregister();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

std::string
Manager::outgoingCall(const std::string& account_id,
                          const std::string& to,
                          const std::string& conf_id,
                          const std::map<std::string, std::string>& volatileCallDetails)
{
    if (not conf_id.empty() and not isConference(conf_id)) {
        JAMI_ERR("outgoingCall() failed, invalid conference id");
        return {};
    }

    JAMI_DBG() << "try outgoing call to '" << to << "'"
               << " with account '" << account_id << "'";

    const auto& current_call_id(getCurrentCallId());
    std::string to_cleaned = hookPreference.getNumberAddPrefix() + trim(to);
    std::shared_ptr<Call> call;

    try {
        call = newOutgoingCall(to_cleaned, account_id, volatileCallDetails);
    } catch (const std::exception &e) {
        JAMI_ERR("%s", e.what());
        return {};
    }

    if (not call)
        return {};

    auto call_id = call->getCallId();

    stopTone();

    pimpl_->switchCall(call);
    call->setConfId(conf_id);

    return call_id;
}

//THREAD=Main : for outgoing Call
bool
Manager::answerCall(const std::string& call_id)
{
    bool result = true;

    auto call = getCallFromCallID(call_id);
    if (!call) {
        JAMI_ERR("Call %s is NULL", call_id.c_str());
        return false;
    }

    // If ring is ringing
    stopTone();

    // store the current call id
    const auto& current_call_id(getCurrentCallId());

    try {
        call->answer();
    } catch (const std::runtime_error &e) {
        JAMI_ERR("%s", e.what());
        result = false;
    }

    // if it was waiting, it's waiting no more
    pimpl_->removeWaitingCall(call_id);

    if (!result) {
        // do not switch to this call if it was not properly started
        return false;
    }

    // if we dragged this call into a conference already
    if (isConferenceParticipant(call_id))
        pimpl_->switchCall(call->getConfId());
    else
        pimpl_->switchCall(call);

    addAudio(*call);

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);

    return result;
}

void
Manager::checkAudio()
{
    // FIXME dirty, the manager should not need to be aware of local recorders
    if (getCallList().empty()
#ifdef ENABLE_VIDEO
        and not getVideoManager().audioPreview
#endif
        and not jami::LocalRecorderManager::instance().hasRunningRecorders()) {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        if (pimpl_->audiodriver_)
            pimpl_->audiodriver_->stopStream();
    }
}

//THREAD=Main
bool
Manager::hangupCall(const std::string& callId)
{
    // store the current call id
    const auto& currentCallId(getCurrentCallId());

    stopTone();

    /* We often get here when the call was hungup before being created */
    auto call = getCallFromCallID(callId);
    if (not call) {
        JAMI_WARN("Could not hang up non-existant call %s", callId.c_str());
        checkAudio();
        return false;
    }

    // Disconnect streams
    removeAudio(*call);

    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (not isConference(currentCallId) and isCurrentCall(*call))
            pimpl_->unsetCurrentCall();
    }

    try {
        call->hangup(0);
        checkAudio();
    } catch (const VoipLinkException &e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    return true;
}

bool
Manager::hangupConference(const std::string& id)
{
    JAMI_DBG("Hangup conference %s", id.c_str());
    if (auto conf = getConferenceFromID(id)) {
        ParticipantSet participants(conf->getParticipantList());
        for (const auto &item : participants)
            hangupCall(item);
        pimpl_->unsetCurrentCall();
        return true;
    }
    JAMI_ERR("No such conference %s", id.c_str());
    return false;
}

//THREAD=Main
bool
Manager::onHoldCall(const std::string& callId)
{
    bool result = true;

    stopTone();

    std::string current_call_id(getCurrentCallId());

    if (auto call = getCallFromCallID(callId)) {
        try {
            result = call->onhold();
            if (result)
                removeAudio(*call); // Unbind calls in main buffer
        } catch (const VoipLinkException &e) {
            JAMI_ERR("%s", e.what());
            result = false;
        }

    } else {
        JAMI_DBG("CallID %s doesn't exist in call onHold", callId.c_str());
        return false;
    }

    if (result) {
        // Remove call from the queue if it was still there
        pimpl_->removeWaitingCall(callId);

        // keeps current call id if the action is not holding this call
        // or a new outgoing call. This could happen in case of a conference
        if (current_call_id == callId)
            pimpl_->unsetCurrentCall();
    }

    return result;
}

//THREAD=Main
bool
Manager::offHoldCall(const std::string& callId)
{
    bool result = true;

    stopTone();

    std::shared_ptr<Call> call = getCallFromCallID(callId);
    if (!call)
        return false;

    try {
        result = call->offhold();
    } catch (const VoipLinkException &e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    if (result) {
        if (isConferenceParticipant(callId))
            pimpl_->switchCall(call->getConfId());
        else
            pimpl_->switchCall(call);

        addAudio(*call);
    }

    return result;
}

bool
Manager::muteMediaCall(const std::string& callId, const std::string& mediaType, bool is_muted)
{
    if (auto call = getCallFromCallID(callId)) {
        call->muteMedia(mediaType, is_muted);
        return true;
    } else {
        JAMI_DBG("CallID %s doesn't exist in call muting", callId.c_str());
        return false;
    }
}

//THREAD=Main
bool
Manager::transferCall(const std::string& callId, const std::string& to)
{
    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else if (not isConference(getCurrentCallId()))
        pimpl_->unsetCurrentCall();

    if (auto call = getCallFromCallID(callId))
        call->transfer(to);
    else
        return false;

    // remove waiting call in case we make transfer without even answer
    pimpl_->removeWaitingCall(callId);

    return true;
}

void
Manager::transferFailed()
{
    emitSignal<DRing::CallSignal::TransferFailed>();
}

void
Manager::transferSucceeded()
{
    emitSignal<DRing::CallSignal::TransferSucceeded>();
}

bool
Manager::attendedTransfer(const std::string& transferID,
                              const std::string& targetID)
{
    if (auto call = getCallFromCallID(transferID))
        return call->attendedTransfer(targetID);

    return false;
}

//THREAD=Main : Call:Incoming
bool
Manager::refuseCall(const std::string& id)
{
    auto call = getCallFromCallID(id);
    if (!call)
        return false;

    stopTone();

    call->refuse();

    checkAudio();

    pimpl_->removeWaitingCall(id);

    // Disconnect streams
    removeAudio(*call);

    return true;
}

void
Manager::removeConference(const std::string& conference_id)
{
    JAMI_DBG("Remove conference %s with %zu participants", conference_id.c_str(), pimpl_->conferenceMap_.size());
    auto iter = pimpl_->conferenceMap_.find(conference_id);
    if (iter == pimpl_->conferenceMap_.end()) {
        JAMI_ERR("Conference not found");
        return;
    }

    emitSignal<DRing::CallSignal::ConferenceRemoved>(conference_id);

    // We now need to bind the audio to the remain participant
    // Unbind main participant audio from conference
    getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);

    // bind main participant audio to remaining conference call
    ParticipantSet participants(iter->second->getParticipantList());
    auto iter_p = participants.begin();
    if (iter_p != participants.end())
        getRingBufferPool().bindCallID(*iter_p, RingBufferPool::DEFAULT_ID);

    // Then remove the conference from the conference map
    pimpl_->conferenceMap_.erase(iter);
    JAMI_DBG("Conference %s removed successfully", conference_id.c_str());
}

std::shared_ptr<Conference>
Manager::getConferenceFromCallID(const std::string& call_id) const
{
    if (auto call = getCallFromCallID(call_id))
        return getConferenceFromID(call->getConfId());
    return nullptr;
}

std::shared_ptr<Conference>
Manager::getConferenceFromID(const std::string& confID) const
{
    auto iter = pimpl_->conferenceMap_.find(confID);
    return iter == pimpl_->conferenceMap_.end() ? nullptr : iter->second;
}

bool
Manager::holdConference(const std::string& id)
{
    auto conf = getConferenceFromID(id);
    if (not conf)
        return false;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                 conf->getState() == Conference::ACTIVE_DETACHED_REC or
                 conf->getState() == Conference::HOLD_REC;

    for (const auto &item : conf->getParticipantList()) {
        pimpl_->switchCall(getCallFromCallID(item));
        onHoldCall(item);
    }

    conf->setState(isRec ? Conference::HOLD_REC : Conference::HOLD);

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

    return true;
}

bool
Manager::unHoldConference(const std::string& id)
{
    auto conf = getConferenceFromID(id);
    if (not conf)
        return false;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
        conf->getState() == Conference::ACTIVE_DETACHED_REC or
        conf->getState() == Conference::HOLD_REC;

    for (const auto &item : conf->getParticipantList()) {
        if (auto call = getCallFromCallID(item)) {
            // if one call is currently recording, the conference is in state recording
            isRec |= call->isRecording();

            pimpl_->switchCall(call);
            offHoldCall(item);
        }
    }

    conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

    return true;
}

bool
Manager::isConference(const std::string& id) const
{
    return pimpl_->conferenceMap_.find(id) != pimpl_->conferenceMap_.end();
}

bool
Manager::isConferenceParticipant(const std::string& call_id)
{
    auto call = getCallFromCallID(call_id);
    return call and not call->getConfId().empty();
}

bool
Manager::addParticipant(const std::string& callId,
                        const std::string& conferenceId)
{
    auto conf = getConferenceFromID(conferenceId);
    if (not conf) {
        JAMI_ERR("Conference id is not valid");
        return false;
    }

    auto call = getCallFromCallID(callId);
    if (!call) {
        JAMI_ERR("Call id %s is not valid", callId.c_str());
        return false;
    }

    // No-op if the call is already a conference participant
    if (call->getConfId() == conferenceId) {
        JAMI_WARN("Call %s already participant of conf %s", callId.c_str(), conferenceId.c_str());
        return true;
    }

    JAMI_DBG("Add participant %s to %s", callId.c_str(), conferenceId.c_str());

    // store the current call id (it will change in offHoldCall or in answerCall)
    auto current_call_id = getCurrentCallId();

    pimpl_->bindCallToConference(*call, *conf);

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    pimpl_->unsetCurrentCall();

    addMainParticipant(conferenceId);
    pimpl_->switchCall(conferenceId);
    addAudio(*call);

    return true;
}

bool
Manager::addMainParticipant(const std::string& conference_id)
{

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        auto conf = getConferenceFromID(conference_id);
        if (not conf)
            return false;

        for (const auto &item_p : conf->getParticipantList()) {
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
            JAMI_WARN("Invalid conference state while adding main participant");

        emitSignal<DRing::CallSignal::ConferenceChanged>(conference_id, conf->getStateStr());
    }

    pimpl_->switchCall(conference_id);
    return true;
}

std::shared_ptr<Call>
Manager::getCallFromCallID(const std::string& callID) const
{
    return callFactory.getCall(callID);
}

bool
Manager::joinParticipant(const std::string& callId1, const std::string& callId2)
{
    if (callId1 == callId2) {
        JAMI_ERR("Cannot join participant %s to itself", callId1.c_str());
        return false;
    }

    // Set corresponding conference ids for call 1
    auto call1 = getCallFromCallID(callId1);
    if (!call1) {
        JAMI_ERR("Could not find call %s", callId1.c_str());
        return false;
    }

    // Set corresponding conderence details
    auto call2 = getCallFromCallID(callId2);
    if (!call2) {
        JAMI_ERR("Could not find call %s", callId2.c_str());
        return false;
    }

    auto conf = std::make_shared<Conference>();

    // Bind calls according to their state
    pimpl_->bindCallToConference(*call1, *conf);
    pimpl_->bindCallToConference(*call2, *conf);

    // Switch current call id to this conference
    pimpl_->switchCall(conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    pimpl_->conferenceMap_.emplace(conf->getConfID(), conf);
    emitSignal<DRing::CallSignal::ConferenceCreated>(conf->getConfID());
    return true;
}

void
Manager::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        JAMI_ERR("Participant number must be higher or equal to 2");
        return;
    }

    auto conf = std::make_shared<Conference>();

    int successCounter = 0;

    for (const auto& numberaccount : participantList) {
        std::string tostr(numberaccount.substr(0, numberaccount.find(',')));
        std::string account(numberaccount.substr(numberaccount.find(',') + 1, numberaccount.size()));

        pimpl_->unsetCurrentCall();

        // Create call
        auto call_id = outgoingCall(account, tostr, conf->getConfID());
        if (call_id.empty())
            continue;

        // Manager methods may behave differently if the call id participates in a conference
        conf->add(call_id);
        successCounter++;
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        pimpl_->conferenceMap_[conf->getConfID()] = conf;
        emitSignal<DRing::CallSignal::ConferenceCreated>(conf->getConfID());
    }
}

bool
Manager::detachLocalParticipant()
{
    JAMI_DBG("Unbind local participant from conference");
    const auto& current_call_id = getCurrentCallId();

    if (not isConference(current_call_id)) {
        JAMI_ERR("Current call id (%s) is not a conference", current_call_id.c_str());
        return false;
    }

    auto conf = getConferenceFromID(current_call_id);
    if (not conf) {
        JAMI_ERR("Conference is NULL");
        return false;
    }

    getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);

    switch (conf->getState()) {
        case Conference::ACTIVE_ATTACHED:
            conf->setState(Conference::ACTIVE_DETACHED);
            break;
        case Conference::ACTIVE_ATTACHED_REC:
            conf->setState(Conference::ACTIVE_DETACHED_REC);
            break;
        default:
            JAMI_WARN("Undefined behavior, invalid conference state in detach participant");
    }

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

    pimpl_->unsetCurrentCall();
    return true;
}

bool
Manager::detachParticipant(const std::string& call_id)
{
    JAMI_DBG("Detach participant %s", call_id.c_str());

    auto call = getCallFromCallID(call_id);
    if (!call) {
        JAMI_ERR("Could not find call %s", call_id.c_str());
        return false;
    }

    auto conf = getConferenceFromCallID(call_id);
    if (!conf) {
        JAMI_ERR("Call is not conferencing, cannot detach");
        return false;
    }

    // Don't hold ringing calls when detaching them from conferences
    if (call->getStateStr() != "RINGING")
        onHoldCall(call_id);

    removeParticipant(call_id);
    return true;
}

void
Manager::removeParticipant(const std::string& call_id)
{
    JAMI_DBG("Remove participant %s", call_id.c_str());

    // this call is no longer a conference participant
    auto call = getCallFromCallID(call_id);
    if (!call) {
        JAMI_ERR("Call not found");
        return;
    }

    auto conf = getConferenceFromID(call->getConfId());
    if (not conf) {
        JAMI_ERR("No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    conf->remove(call_id);
    call->setConfId("");

    removeAudio(*call);

    emitSignal<DRing::CallSignal::ConferenceChanged>(conf->getConfID(), conf->getStateStr());

    pimpl_->processRemainingParticipants(*conf);
}

bool
Manager::joinConference(const std::string& conf_id1,
                            const std::string& conf_id2)
{
    auto conf = getConferenceFromID(conf_id1);
    if (not conf) {
        JAMI_ERR("Not a valid conference ID: %s", conf_id1.c_str());
        return false;
    }

    if (pimpl_->conferenceMap_.find(conf_id2) == pimpl_->conferenceMap_.end()) {
        JAMI_ERR("Not a valid conference ID: %s", conf_id2.c_str());
        return false;
    }

    ParticipantSet participants(conf->getParticipantList());

    // Detach and remove all participant from conf1 before add
    // ... to conf2
    for (const auto &p : participants) {
        JAMI_DBG("Detach participant %s", p.c_str());
        auto call = getCallFromCallID(p);
        if (!call) {
            JAMI_ERR("Could not find call %s", p.c_str());
            continue;
        }

        if (!getConferenceFromCallID(p)) {
            JAMI_ERR("Call is not conferencing, cannot detach");
            continue;
        }

        conf->remove(p);
        call->setConfId("");
        removeAudio(*call);
    }
    // Remove conf1
    pimpl_->base_.removeConference(conf_id1);

    for (const auto &p : participants)
        addParticipant(p, conf_id2);

    return true;
}

void
Manager::addAudio(Call& call)
{
    const auto& call_id = call.getCallId();

    if (isConferenceParticipant(call_id)) {
        JAMI_DBG("[conf:%s] Attach local audio", call_id.c_str());

        // bind to conference participant
        ConferenceMap::iterator iter = pimpl_->conferenceMap_.find(call_id);
        if (iter != pimpl_->conferenceMap_.end() and iter->second) {
            auto conf = iter->second;
            conf->bindParticipant(call_id);
        }
    } else {
        JAMI_DBG("[call:%s] Attach audio", call_id.c_str());

        // bind to main
        getRingBufferPool().bindCallID(call_id, RingBufferPool::DEFAULT_ID);

        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        if (!pimpl_->audiodriver_) {
            JAMI_ERR("Audio driver not initialized");
            return;
        }
        pimpl_->audiodriver_->flushUrgent();
        getRingBufferPool().flushAllBuffers();
    }
    startAudioDriverStream();
}

void
Manager::removeAudio(Call& call)
{
    const auto& call_id = call.getCallId();
    JAMI_DBG("[call:%s] Remove local audio", call_id.c_str());
    getRingBufferPool().unBindAll(call_id);
}

ScheduledExecutor&
Manager::scheduler()
{
    return pimpl_->scheduler_;
}

std::shared_ptr<asio::io_context>
Manager::ioContext() const
{
    return pimpl_->ioContext_;
}

void
Manager::addTask(std::function<bool()>&& task)
{
    pimpl_->scheduler_.scheduleAtFixedRate(std::move(task), std::chrono::milliseconds(30));
}

std::shared_ptr<Task>
Manager::scheduleTask(std::function<void()>&& task, std::chrono::steady_clock::time_point when)
{
    return pimpl_->scheduler_.schedule(std::move(task), when);
}

// Must be invoked periodically by a timer from the main event loop
void Manager::pollEvents()
{
}

//THREAD=Main

void
Manager::saveConfig(const std::shared_ptr<Account>& acc)
{
    if (auto ringAcc = std::dynamic_pointer_cast<JamiAccount>(acc))
        ringAcc->saveConfig();
    else
        saveConfig();
}

void
Manager::saveConfig()
{
    JAMI_DBG("Saving Configuration to XDG directory %s", pimpl_->path_.c_str());

    if (pimpl_->audiodriver_) {
        audioPreference.setVolumemic(pimpl_->audiodriver_->getCaptureGain());
        audioPreference.setVolumespkr(pimpl_->audiodriver_->getPlaybackGain());
        audioPreference.setCaptureMuted(pimpl_->audiodriver_->isCaptureMuted());
        audioPreference.setPlaybackMuted(pimpl_->audiodriver_->isPlaybackMuted());
    }

    try {
        YAML::Emitter out;

        // FIXME maybe move this into accountFactory?
        out << YAML::BeginMap << YAML::Key << "accounts";
        out << YAML::Value << YAML::BeginSeq;

        for (const auto& account : accountFactory.getAllAccounts()) {
            if (auto ringAccount = std::dynamic_pointer_cast<JamiAccount>(account)) {
                auto accountConfig = ringAccount->getPath() + DIR_SEPARATOR_STR + "config.yml";
                if (not fileutils::isFile(accountConfig)) {
                    saveConfig(ringAccount);
                }
            } else {
                account->serialize(out);
            }
        }
        out << YAML::EndSeq;

        // FIXME: this is a hack until we get rid of accountOrder
        preferences.verifyAccountOrder(getAccountList());
        preferences.serialize(out);
        voipPreferences.serialize(out);
        hookPreference.serialize(out);
        audioPreference.serialize(out);
#ifdef ENABLE_VIDEO
        videoPreferences.serialize(out);
#endif
        shortcutPreferences.serialize(out);

        std::lock_guard<std::mutex> lock(fileutils::getFileLock(pimpl_->path_));
        std::ofstream fout = fileutils::ofstream(pimpl_->path_);
        fout << out.c_str();
    } catch (const YAML::Exception &e) {
        JAMI_ERR("%s", e.what());
    } catch (const std::runtime_error &e) {
        JAMI_ERR("%s", e.what());
    }
}

//THREAD=Main | VoIPLink
void
Manager::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        JAMI_DBG("Do not have to play a tone...");
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        JAMI_DBG("Pulse length is not set...");
        return;
    }

    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    // fast return, no sound, so no dtmf
    if (not pimpl_->audiodriver_ or not pimpl_->dtmfKey_) {
        JAMI_DBG("No audio layer...");
        return;
    }

    pimpl_->audiodriver_->startStream();
    if (not pimpl_->audiodriver_->waitForStart(std::chrono::seconds(1))) {
        JAMI_ERR("Failed to start audio layer...");
        return;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    int size = (int)((pulselen * (float) pimpl_->audiodriver_->getSampleRate()) / 1000);
    pimpl_->dtmfBuf_.resize(size);

    // Handle dtmf
    pimpl_->dtmfKey_->startTone(code);

    // copy the sound
    if (pimpl_->dtmfKey_->generateDTMF(*pimpl_->dtmfBuf_.getChannel(0))) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();

        pimpl_->audiodriver_->putUrgent(pimpl_->dtmfBuf_);
    }

    // TODO Cache the DTMF
}

// Multi-thread
bool
Manager::incomingCallsWaiting()
{
    std::lock_guard<std::mutex> m(pimpl_->waitingCallsMutex_);
    return not pimpl_->waitingCalls_.empty();
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
void
Manager::incomingCall(Call &call, const std::string& accountId)
{
    stopTone();
    const std::string callID(call.getCallId());

    if (accountId.empty())
        call.setIPToIP(true);
    else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history.
        std::string peerNumber(call.getPeerNumber());

        const char SIP_PREFIX[] = "sip:";
        size_t startIndex = peerNumber.find(SIP_PREFIX);

        if (startIndex != std::string::npos)
            call.setPeerNumber(peerNumber.substr(startIndex + sizeof(SIP_PREFIX) - 1));
    }

    if (not hasCurrentCall()) {
        call.setState(Call::ConnectionState::RINGING);
#ifndef RING_UWP
        playRingtone(accountId);
#endif
    }

    pimpl_->addWaitingCall(callID);

    std::string number(call.getPeerNumber());

    std::string from("<" + number + ">");

    emitSignal<DRing::CallSignal::IncomingCall>(accountId, callID, call.getPeerDisplayName() + " " + from);

    auto currentCall = getCurrentCall();
    if (pimpl_->autoAnswer_) {
        runOnMainThread([this, callID]{ answerCall(callID); });
    } else if (currentCall) {
        // Test if already calling this person
        if (currentCall->getAccountId() == accountId
        && currentCall->getPeerNumber() == call.getPeerNumber()) {
            auto device_uid = currentCall->getAccount().getUsername();
            if (device_uid.find("ring:") == 0) {
                // NOTE: in case of a SIP call it's already ready to compare
                device_uid = device_uid.substr(5); // after ring:
            }
            auto answerToCall = false;
            auto downgradeToAudioOnly = currentCall->isAudioOnly() != call.isAudioOnly();
            if (downgradeToAudioOnly)
                // Accept the incoming audio only
                answerToCall = call.isAudioOnly();
            else
                // Accept the incoming call from the higher id number
                answerToCall = (device_uid.compare(call.getPeerNumber()) < 0);

            if (answerToCall) {
                auto currentCallID = currentCall->getCallId();
                runOnMainThread([this, currentCallID, callID] {
                    answerCall(callID);
                    hangupCall(currentCallID);
                });
            }
        }
    }
}

void
Manager::incomingMessage(const std::string& callID,
                         const std::string& from,
                         const std::map<std::string, std::string>& messages)
{
    if (isConferenceParticipant(callID)) {
        auto conf = getConferenceFromCallID(callID);
        if (not conf) {
            JAMI_ERR("no conference associated to ID %s", callID.c_str());
            return;
        }

        JAMI_DBG("Is a conference, send incoming message to everyone");
        //filter out vcards messages  as they could be resent by master as its own vcard
        // TODO. Implement a protocol to handle vcard messages
        bool sendToOtherParicipants = true;
        for (auto& message : messages) {
            if (message.first.find("x-ring/ring.profile.vcard") != std::string::npos) {
                sendToOtherParicipants = false;
            }
        }
        if (sendToOtherParicipants) {
            pimpl_->sendTextMessageToConference(*conf, messages, from);
        }

        // in case of a conference we must notify client using conference id
        emitSignal<DRing::CallSignal::IncomingMessage>(conf->getConfID(), from, messages);
    } else
        emitSignal<DRing::CallSignal::IncomingMessage>(callID, from, messages);
}

void
Manager::sendCallTextMessage(const std::string& callID,
                             const std::map<std::string, std::string>& messages,
                             const std::string& from,
                             bool /*isMixed TODO: use it */)
{
    if (auto conf = getConferenceFromID(callID)) {
        JAMI_DBG("Is a conference, send instant message to everyone");
        pimpl_->sendTextMessageToConference(*conf, messages, from);

    } else if (isConferenceParticipant(callID)) {
        auto conf = getConferenceFromCallID(callID);
        if (not conf) {
            JAMI_ERR("no conference associated to call ID %s", callID.c_str());
            return;
        }

        JAMI_DBG("Call is participant in a conference, send instant message to everyone");
        pimpl_->sendTextMessageToConference(*conf, messages, from);

    } else {
        auto call = getCallFromCallID(callID);
        if (not call) {
            JAMI_ERR("Failed to send message to %s: inexistant call ID", callID.c_str());
            return;
        }

        try {
            call->sendTextMessage(messages, from);
        } catch (const im::InstantMessageException& e) {
            JAMI_ERR("Failed to send message to call %s: %s", call->getCallId().c_str(), e.what());
        }
    }
}

//THREAD=VoIP CALL=Outgoing
void
Manager::peerAnsweredCall(Call& call)
{
    const auto& call_id = call.getCallId();
    JAMI_DBG("[call:%s] Peer answered", call_id.c_str());

    // The if statement is useful only if we sent two calls at the same time.
    if (isCurrentCall(call))
        stopTone();

    addAudio(call);

    if (pimpl_->audiodriver_) {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        getRingBufferPool().flushAllBuffers();
        pimpl_->audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording())
        toggleRecordingCall(call_id);
}

//THREAD=VoIP Call=Outgoing
void
Manager::peerRingingCall(Call& call)
{
    JAMI_DBG("[call:%s] Peer ringing", call.getCallId().c_str());

    if (isCurrentCall(call))
        ringback();
}

//THREAD=VoIP Call=Outgoing/Ingoing
void
Manager::peerHungupCall(Call& call)
{
    const auto& call_id = call.getCallId();
    JAMI_DBG("[call:%s] Peer hungup", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        removeParticipant(call_id);
    } else if (isCurrentCall(call)) {
        stopTone();
        pimpl_->unsetCurrentCall();
    }

    call.peerHungup();

    checkAudio();
    pimpl_->removeWaitingCall(call_id);
    if (not incomingCallsWaiting())
        stopTone();

    removeAudio(call);
}

//THREAD=VoIP
void
Manager::callBusy(Call& call)
{
    JAMI_DBG("[call:%s] Busy", call.getCallId().c_str());

    if (isCurrentCall(call)) {
        pimpl_->unsetCurrentCall();
    }

    checkAudio();
    pimpl_->removeWaitingCall(call.getCallId());
    if (not incomingCallsWaiting())
        stopTone();
}

//THREAD=VoIP
void
Manager::callFailure(Call& call)
{
    const auto& call_id = call.getCallId();
    JAMI_DBG("[call:%s] Failed", call.getCallId().c_str());

    if (isCurrentCall(call)) {
        pimpl_->unsetCurrentCall();
    }

    if (isConferenceParticipant(call_id)) {
        JAMI_DBG("Call %s participating in a conference failed", call_id.c_str());
        // remove this participant
        removeParticipant(call_id);
    }

    checkAudio();
    pimpl_->removeWaitingCall(call_id);
    if (not incomingCallsWaiting())
        stopTone();
    removeAudio(call);
}

/**
 * Multi Thread
 */
void
Manager::stopTone()
{
    if (not voipPreferences.getPlayTones())
        return;

    pimpl_->toneCtrl_.stop();
}

/**
 * Multi Thread
 */
void
Manager::playTone()
{
    pimpl_->playATone(Tone::TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void
Manager::playToneWithMessage()
{
    pimpl_->playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
Manager::congestion()
{
    pimpl_->playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void
Manager::ringback()
{
    pimpl_->playATone(Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void
Manager::playRingtone(const std::string& accountID)
{
    const auto account = getAccount(accountID);

    if (!account) {
        JAMI_WARN("Invalid account in ringtone");
        return;
    }

    if (!account->getRingtoneEnabled()) {
        ringback();
        return;
    }

    std::string ringchoice = account->getRingtonePath();
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    //for ios file located in main buindle
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef bundleURL = CFBundleCopyBundleURL(bundle);
    CFStringRef stringPath = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
    CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
    const char *buindlePath = CFStringGetCStringPtr(stringPath, encodingMethod);
    ringchoice = std::string(buindlePath) + DIR_SEPARATOR_STR + ringchoice;
#elif !defined(_WIN32)
    if (ringchoice.find(DIR_SEPARATOR_CH) == std::string::npos) {
        // check inside global share directory
        static const char * const RINGDIR = "ringtones";
        ringchoice = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR
        + RINGDIR + DIR_SEPARATOR_STR + ringchoice;
    }
#endif

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_) {
            JAMI_ERR("no audio layer in ringtone");
            return;
        }
        // start audio if not started AND flush all buffers (main and urgent)
        pimpl_->audiodriver_->startStream();
        pimpl_->toneCtrl_.setSampleRate(pimpl_->audiodriver_->getSampleRate());
    }

    if (not pimpl_->toneCtrl_.setAudioFile(ringchoice))
        ringback();
}

std::shared_ptr<AudioLoop>
Manager::getTelephoneTone()
{
    return pimpl_->toneCtrl_.getTelephoneTone();
}

std::shared_ptr<AudioLoop>
Manager::getTelephoneFile()
{
    return pimpl_->toneCtrl_.getTelephoneFile();
}

/**
 * Set input audio plugin
 */
void
Manager::setAudioPlugin(const std::string& audioPlugin)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    audioPreference.setAlsaPlugin(audioPlugin);

    bool wasStarted = pimpl_->audiodriver_->isStarted();

    // Recreate audio driver with new settings
    pimpl_->audiodriver_.reset();
    pimpl_->audiodriver_.reset(audioPreference.createAudioLayer());

    if (pimpl_->audiodriver_ and wasStarted)
        pimpl_->audiodriver_->startStream();
    else
        JAMI_ERR("No audio layer created, possibly built without audio support");

    saveConfig();
}

/**
 * Set audio output device
 */
void
Manager::setAudioDevice(int index, DeviceType type)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio driver not initialized");
        return ;
    }
    if (pimpl_->getCurrentDeviceIndex(type) == index) {
        JAMI_WARN("Audio device already selected ; doing nothing.");
        return;
    }

    const bool wasStarted = pimpl_->audiodriver_->isStarted();
    pimpl_->audiodriver_->updatePreference(audioPreference, index, type);

    // Recreate audio driver with new settings
    pimpl_->audiodriver_.reset(audioPreference.createAudioLayer());

    if (pimpl_->audiodriver_ and wasStarted)
        pimpl_->audiodriver_->startStream();

    saveConfig();
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string>
Manager::getAudioOutputDeviceList()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return {};
    }

    return pimpl_->audiodriver_->getPlaybackDeviceList();
}

/**
 * Get list of supported audio input device
 */
std::vector<std::string>
Manager::getAudioInputDeviceList()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return {};
    }

    return pimpl_->audiodriver_->getCaptureDeviceList();
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string>
Manager::getCurrentAudioDevicesIndex()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return {};
    }

    std::vector<std::string> v;

    std::stringstream ssi, sso, ssr;
    sso << pimpl_->audiodriver_->getIndexPlayback();
    v.push_back(sso.str());
    ssi << pimpl_->audiodriver_->getIndexCapture();
    v.push_back(ssi.str());
    ssr << pimpl_->audiodriver_->getIndexRingtone();
    v.push_back(ssr.str());

    return v;
}

bool
Manager::switchInput(const std::string& call_id, const std::string& res)
{
    if (auto conf = getConferenceFromID(call_id)) {
        conf->switchInput(res);
        return true;
    } else if (auto call = getCallFromCallID(call_id)) {
        call->switchInput(res);
        return true;
    }
    return false;
}

int
Manager::isRingtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        JAMI_WARN("Invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void
Manager::ringtoneEnabled(const std::string& id)
{
    const auto account = getAccount(id);

    if (!account) {
        JAMI_WARN("Invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled(false) : account->setRingtoneEnabled(true);
}

bool
Manager::getIsAlwaysRecording() const
{
    return audioPreference.getIsAlwaysRecording();
}

void
Manager::setIsAlwaysRecording(bool isAlwaysRec)
{
    audioPreference.setIsAlwaysRecording(isAlwaysRec);
    saveConfig();
}

bool
Manager::toggleRecordingCall(const std::string& id)
{
    std::shared_ptr<Recordable> rec;

    ConferenceMap::const_iterator it(pimpl_->conferenceMap_.find(id));
    if (it == pimpl_->conferenceMap_.end()) {
        JAMI_DBG("toggle recording for call %s", id.c_str());
        rec = getCallFromCallID(id);
    } else {
        JAMI_DBG("toggle recording for conference %s", id.c_str());
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
        JAMI_ERR("Could not find recordable instance %s", id.c_str());
        return false;
    }

    const bool result = rec->toggleRecording();
    emitSignal<DRing::CallSignal::RecordPlaybackFilepath>(id, rec->getPath());
    emitSignal<DRing::CallSignal::RecordingStateChanged>(id, result);
    return result;
}

bool
Manager::isRecording(const std::string& id)
{
    auto call = getCallFromCallID(id);
    return call and (static_cast<Recordable*>(call.get()))->isRecording();
}

bool
Manager::startRecordedFilePlayback(const std::string& filepath)
{
    JAMI_DBG("Start recorded file playback %s", filepath.c_str());

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_) {
            JAMI_ERR("No audio layer in start recorded file playback");
            return false;
        }

        pimpl_->audiodriver_->startStream();
        pimpl_->toneCtrl_.setSampleRate(pimpl_->audiodriver_->getSampleRate());
    }

    return pimpl_->toneCtrl_.setAudioFile(filepath);
}

void
Manager::recordingPlaybackSeek(const double value)
{
    pimpl_->toneCtrl_.seek(value);
}

void
Manager::stopRecordedFilePlayback()
{
    JAMI_DBG("Stop recorded file playback");

    checkAudio();
    pimpl_->toneCtrl_.stopAudioFile();
}

void
Manager::setHistoryLimit(int days)
{
    JAMI_DBG("Set history limit");
    preferences.setHistoryLimit(days);
    saveConfig();
}

int
Manager::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

void
Manager::setRingingTimeout(int timeout)
{
    JAMI_DBG("Set ringing timeout");
    preferences.setRingingTimeout(timeout);
    saveConfig();
}

int
Manager::getRingingTimeout() const
{
    return preferences.getRingingTimeout();
}

bool
Manager::setAudioManager(const std::string &api)
{
    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_)
            return false;

        if (api == audioPreference.getAudioApi()) {
            JAMI_DBG("Audio manager chosen already in use. No changes made. ");
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        bool wasStarted = pimpl_->audiodriver_->isStarted();
        audioPreference.setAudioApi(api);
        pimpl_->audiodriver_.reset(audioPreference.createAudioLayer());

        if (pimpl_->audiodriver_ and wasStarted)
            pimpl_->audiodriver_->startStream();
    }

    saveConfig();

    // ensure that we completed the transition (i.e. no fallback was used)
    return api == audioPreference.getAudioApi();
}

std::string
Manager::getAudioManager() const
{
    return audioPreference.getAudioApi();
}

int
Manager::getAudioInputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return 0;
    }

    return pimpl_->audiodriver_->getAudioDeviceIndex(name, DeviceType::CAPTURE);
}

int
Manager::getAudioOutputDeviceIndex(const std::string &name)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return 0;
    }

    return pimpl_->audiodriver_->getAudioDeviceIndex(name, DeviceType::PLAYBACK);
}

std::string
Manager::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getAlsaPlugin();
}

bool
Manager::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce();
}

void
Manager::setNoiseSuppressState(bool state)
{
    audioPreference.setNoiseReduce(state);
}

bool
Manager::isAGCEnabled() const
{
    return audioPreference.isAGCEnabled();
}

void
Manager::setAGCState(bool state)
{
    audioPreference.setAGCState(state);
}

/**
 * Initialization: Main Thread
 */
void
Manager::initAudioDriver()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
    pimpl_->audiodriver_.reset(audioPreference.createAudioLayer());
}

AudioFormat
Manager::hardwareAudioFormatChanged(AudioFormat format)
{
    return audioFormatUsed(format);
}

AudioFormat
Manager::audioFormatUsed(AudioFormat format)
{
    AudioFormat currentFormat = pimpl_->ringbufferpool_->getInternalAudioFormat();
    format.nb_channels = std::max(currentFormat.nb_channels, std::min(format.nb_channels, 2u)); // max 2 channels.
    format.sample_rate = std::max(currentFormat.sample_rate, format.sample_rate);

    if (currentFormat == format)
        return format;

    JAMI_DBG("Audio format changed: %s -> %s", currentFormat.toString().c_str(),
             format.toString().c_str());

    pimpl_->ringbufferpool_->setInternalAudioFormat(format);
    pimpl_->toneCtrl_.setSampleRate(format.sample_rate);
    pimpl_->dtmfKey_.reset(new DTMF(format.sample_rate));

    return format;
}

void
Manager::setAccountsOrder(const std::string& order)
{
    JAMI_DBG("Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder(order);

    saveConfig();

    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();
}

std::vector<std::string>
Manager::getAccountList() const
{
    // Concatenate all account pointers in a single map
    std::vector<std::string> v;
    v.reserve(accountCount());
    for (const auto &account : getAllAccounts()) {
        v.emplace_back(account->getAccountID());
    }

    return v;
}

std::map<std::string, std::string>
Manager::getAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getAccountDetails();
    } else {
        JAMI_ERR("Could not get account details on a non-existing accountID %s", accountID.c_str());
        // return an empty map since we can't throw an exception to D-Bus
        return std::map<std::string, std::string>();
    }
}

std::map<std::string, std::string>
Manager::getVolatileAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getVolatileAccountDetails();
    } else {
        JAMI_ERR("Could not get volatile account details on a non-existing accountID %s", accountID.c_str());
        return {};
    }
}

// method to reduce the if/else mess.
// Even better, switch to XML !

void
Manager::setAccountDetails(const std::string& accountID,
                               const std::map<std::string, std::string>& details)
{
    JAMI_DBG("Set account details for %s", accountID.c_str());

    const auto account = getAccount(accountID);

    if (account == nullptr) {
        JAMI_ERR("Could not find account %s", accountID.c_str());
        return;
    }

    // Ignore if nothing has changed
    if (details == account->getAccountDetails())
        return;

    // Unregister before modifying any account information
    // FIXME: inefficient api, don't pass details (not as ref nor copy)
    // let client requiests them we needed.
    account->doUnregister([&](bool /* transport_free */) {
        account->setAccountDetails(details);
        // Serialize configuration to disk once it is done
        if (auto ringAccount = std::dynamic_pointer_cast<JamiAccount>(account)) {
            saveConfig(ringAccount);
        } else {
            saveConfig();
        }

        if (account->isUsable())
            account->doRegister();
        else
            account->doUnregister();

        // Update account details to the client side
        emitSignal<DRing::ConfigurationSignal::AccountDetailsChanged>(accountID, details);
    });
}

std::map <std::string, std::string>
Manager::testAccountICEInitialization(const std::string& accountID)
{
    const auto account = getAccount(accountID);
    const auto transportOptions = account->getIceOptions();

    auto& iceTransportFactory = Manager::instance().getIceTransportFactory();
    std::shared_ptr<IceTransport> ice = iceTransportFactory.createTransport(
        accountID.c_str(), 4, true, account->getIceOptions()
    );

    std::map<std::string, std::string> result;

    if (ice->waitForInitialization(ICE_INIT_TIMEOUT) <= 0)
    {
        result["STATUS"] = std::to_string((int) DRing::Account::testAccountICEInitializationStatus::FAILURE);
        result["MESSAGE"] = ice->getLastErrMsg();
    }
    else
    {
        result["STATUS"] = std::to_string((int) DRing::Account::testAccountICEInitializationStatus::SUCCESS);
        result["MESSAGE"] = "";
    }

    return result;
}

std::string
Manager::getNewAccountId()
{
    std::string newAccountID;
    static std::uniform_int_distribution<uint64_t> rand_acc_id;

    const std::vector<std::string> accountList(getAccountList());

    do {
        std::ostringstream accId;
        accId << std::hex << rand_acc_id(pimpl_->rand_);
        newAccountID = accId.str();
    } while (std::find(accountList.begin(), accountList.end(), newAccountID)
             != accountList.end());

    return newAccountID;
}

std::string
Manager::addAccount(const std::map<std::string, std::string>& details, const std::string& accountId)
{
    /** @todo Deal with both the accountMap_ and the Configuration */
    auto newAccountID = accountId.empty() ? getNewAccountId() : accountId;

    // Get the type
    const char* accountType;
    if (details.find(Conf::CONFIG_ACCOUNT_TYPE) != details.end())
        accountType = (*details.find(Conf::CONFIG_ACCOUNT_TYPE)).second.c_str();
    else
        accountType = AccountFactory::DEFAULT_ACCOUNT_TYPE;

    JAMI_DBG("Adding account %s", newAccountID.c_str());

    auto newAccount = accountFactory.createAccount(accountType, newAccountID);
    if (!newAccount) {
        JAMI_ERR("Unknown %s param when calling addAccount(): %s",
              Conf::CONFIG_ACCOUNT_TYPE, accountType);
        return "";
    }

    newAccount->setAccountDetails(details);
    saveConfig(newAccount);
    newAccount->doRegister();

    preferences.addAccount(newAccountID);
    saveConfig();

    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();

    return newAccountID;
}

void Manager::removeAccount(const std::string& accountID, bool flush)
{
    // Get it down and dying
    if (const auto& remAccount = getAccount(accountID)) {
        remAccount->doUnregister();
        if (flush)
            remAccount->flush();
        accountFactory.removeAccount(*remAccount);
    }

    preferences.removeAccount(accountID);

    saveConfig();

    emitSignal<DRing::ConfigurationSignal::AccountsChanged>();
}

void
Manager::removeAccounts()
{
    for (const auto &acc : getAccountList())
        removeAccount(acc);
}

std::string
Manager::getNewCallID()
{
    std::ostringstream random_id;

    // generate something like s7ea037947eb9fb2f
    do {
        random_id.clear();
        random_id << std::uniform_int_distribution<uint64_t>(1, DRING_ID_MAX_VAL)(pimpl_->rand_);
    } while (callFactory.hasCall(random_id.str()));

    return random_id.str();
}

std::vector<std::string>
Manager::loadAccountOrder() const
{
    return split_string(preferences.getAccountOrder(), '/');
}

int
Manager::loadAccountMap(const YAML::Node& node)
{
    // build preferences
    preferences.unserialize(node);
    voipPreferences.unserialize(node);
    hookPreference.unserialize(node);
    audioPreference.unserialize(node);
    shortcutPreferences.unserialize(node);

    int errorCount = 0;
    try {
#ifdef ENABLE_VIDEO
        videoPreferences.unserialize(node);
#endif
    } catch (const YAML::Exception &e) {
        JAMI_ERR("%s: No video node in config file", e.what());
        ++errorCount;
    }

    const std::string accountOrder = preferences.getAccountOrder();

    // load saved preferences for IP2IP account from configuration file
    const auto &accountList = node["accounts"];

    for (auto &a : accountList) {
        pimpl_->loadAccount(a, errorCount);
    }

    auto accountBaseDir = fileutils::get_data_dir();
    auto dirs = fileutils::readDirectory(accountBaseDir);

    std::condition_variable cv;
    std::mutex lock;
    size_t remaining {0};
    std::unique_lock<std::mutex> l(lock);
    for (const auto& dir : dirs) {
        if (accountFactory.hasAccount<JamiAccount>(dir)) {
            continue;
        }
        remaining++;
        dht::ThreadPool::computation().run([
            this, dir,
            &cv, &remaining, &lock,
            configFile = accountBaseDir + DIR_SEPARATOR_STR + dir + DIR_SEPARATOR_STR + "config.yml"
        ] {
            if (fileutils::isFile(configFile)) {
                try {
                    if (auto a = accountFactory.createAccount(JamiAccount::ACCOUNT_TYPE, dir)) {
                        std::ifstream file = fileutils::ifstream(configFile);
                        YAML::Node parsedConfig = YAML::Load(file);
                        file.close();
                        a->unserialize(parsedConfig);
                    }
                } catch (const std::exception& e) {
                    JAMI_ERR("Can't import account %s: %s", dir.c_str(), e.what());
                }
            }
            {
                std::lock_guard<std::mutex> l(lock);
                remaining--;
            }
            cv.notify_one();
        });
    }
    cv.wait(l, [&remaining] {
        return remaining == 0;
    });

    return errorCount;
}

std::map<std::string, std::string>
Manager::getCallDetails(const std::string &callID)
{
    if (auto call = getCallFromCallID(callID)) {
        return call->getDetails();
    } else {
        JAMI_ERR("Call is NULL");
        // FIXME: is this even useful?
        return Call::getNullDetails();
    }
}

std::vector<std::string>
Manager::getCallList() const
{
    std::vector<std::string> results;
    for (const auto& call: callFactory.getAllCalls()) {
        if (!call->isSubcall())
            results.push_back(call->getCallId());
    }
    return results;
}

std::map<std::string, std::string>
Manager::getConferenceDetails(
    const std::string& confID) const
{
    std::map<std::string, std::string> conf_details;
    ConferenceMap::const_iterator iter_conf = pimpl_->conferenceMap_.find(confID);

    if (iter_conf != pimpl_->conferenceMap_.end()) {
        conf_details["CONFID"] = confID;
        conf_details["CONF_STATE"] = iter_conf->second->getStateStr();
    }

    return conf_details;
}

std::vector<std::string>
Manager::getConferenceList() const
{
    return map_utils::extractKeys(pimpl_->conferenceMap_);
}

std::vector<std::string>
Manager::getDisplayNames(const std::string& confID) const
{
    std::vector<std::string> v;
    ConferenceMap::const_iterator iter_conf = pimpl_->conferenceMap_.find(confID);

    if (iter_conf != pimpl_->conferenceMap_.end()) {
        return iter_conf->second->getDisplayNames();
    } else {
        JAMI_WARN("Did not find conference %s", confID.c_str());
    }

    return v;
}

std::vector<std::string>
Manager::getParticipantList(const std::string& confID) const
{
    auto iter_conf = pimpl_->conferenceMap_.find(confID);
    if (iter_conf != pimpl_->conferenceMap_.end()) {
        const ParticipantSet& participants(iter_conf->second->getParticipantList());
        return {participants.begin(), participants.end()};
    } else
        JAMI_WARN("Did not find conference %s", confID.c_str());

    return {};
}

std::string
Manager::getConferenceId(const std::string& callID)
{
    if (auto call = getCallFromCallID(callID))
        return call->getConfId();

    JAMI_ERR("Call is NULL");
    return "";
}

void
Manager::startAudioDriverStream()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
    if (!pimpl_->audiodriver_) {
        JAMI_ERR("Audio driver not initialized");
        return;
    }
    pimpl_->audiodriver_->startStream();
}

void
Manager::restartAudioDriverStream()
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
    if (!pimpl_->audiodriver_) {
        JAMI_ERR("Audio driver not initialized");
        return;
    }
    pimpl_->audiodriver_->stopStream();
    pimpl_->audiodriver_->startStream();
}

void
Manager::registerAccounts()
{
    auto allAccounts(getAccountList());

    for (auto &item : allAccounts) {
        const auto a = getAccount(item);

        if (!a)
            continue;

        a->loadConfig();

        if (a->isUsable())
            a->doRegister();
    }
}

void
Manager::sendRegister(const std::string& accountID, bool enable)
{
    const auto acc = getAccount(accountID);
    if (!acc)
        return;

    acc->setEnabled(enable);
    acc->loadConfig();

    saveConfig(acc);

    if (acc->isEnabled()) {
        acc->doRegister();
    } else
        acc->doUnregister();
}

uint64_t
Manager::sendTextMessage(const std::string& accountID, const std::string& to,
                         const std::map<std::string, std::string>& payloads)
{
    if (const auto acc = getAccount(accountID)) {
        try {
            return acc->sendTextMessage(to, payloads);
        } catch (const std::exception& e) {
            JAMI_ERR("Exception during text message sending: %s", e.what());
        }
    }
    return 0;
}

int
statusFromImStatus(im::MessageStatus status) {
    switch (status) {
    case im::MessageStatus::IDLE:
    case im::MessageStatus::SENDING:
        return static_cast<int>(DRing::Account::MessageStates::SENDING);
    case im::MessageStatus::SENT:
        return static_cast<int>(DRing::Account::MessageStates::SENT);
    case im::MessageStatus::READ:
        return static_cast<int>(DRing::Account::MessageStates::READ);
    case im::MessageStatus::FAILURE:
        return static_cast<int>(DRing::Account::MessageStates::FAILURE);
    default:
        return static_cast<int>(DRing::Account::MessageStates::UNKNOWN);
    }
}

int
Manager::getMessageStatus(uint64_t id) const
{
    const auto& allAccounts = accountFactory.getAllAccounts();
    for (const auto& acc : allAccounts) {
        auto status = acc->getMessageStatus(id);
        if (status != im::MessageStatus::UNKNOWN)
            return statusFromImStatus(status);
    }
    return static_cast<int>(DRing::Account::MessageStates::UNKNOWN);
}

int
Manager::getMessageStatus(const std::string& accountID, uint64_t id) const
{
    if (const auto acc = getAccount(accountID))
        return statusFromImStatus(acc->getMessageStatus(id));
    return static_cast<int>(DRing::Account::MessageStates::UNKNOWN);
}

void
Manager::setAccountActive(const std::string& accountID, bool active)
{
    const auto acc = getAccount(accountID);
    if (!acc || acc->isActive() == active)
        return;
    acc->setActive(active);
    if (acc->isEnabled()) {
        if (active)
            acc->doRegister();
        else
            acc->doUnregister();
    }
    emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(
        accountID,
        acc->getVolatileAccountDetails());
}

std::shared_ptr<AudioLayer>
Manager::getAudioDriver()
{
    return pimpl_->audiodriver_;
}

std::shared_ptr<Call>
Manager::newOutgoingCall(const std::string& toUrl,
                         const std::string& accountId,
                         const std::map<std::string, std::string>& volatileCallDetails)
{
    auto account = getAccount(accountId);
    if (!account or !account->isUsable()) {
        JAMI_WARN("Account is not usable for calling");
        return nullptr;
    }

    return account->newOutgoingCall(toUrl, volatileCallDetails);
}

#ifdef ENABLE_VIDEO
std::shared_ptr<video::SinkClient>
Manager::createSinkClient(const std::string& id, bool mixer)
{
    const auto& iter = pimpl_->sinkMap_.find(id);
    if (iter != std::end(pimpl_->sinkMap_)) {
        if (auto sink = iter->second.lock())
            return sink;
        pimpl_->sinkMap_.erase(iter); // remove expired weak_ptr
    }

    auto sink = std::make_shared<video::SinkClient>(id, mixer);
    pimpl_->sinkMap_.emplace(id, sink);
    return sink;
}

std::shared_ptr<video::SinkClient>
Manager::getSinkClient(const std::string& id)
{
    const auto& iter = pimpl_->sinkMap_.find(id);
    if (iter != std::end(pimpl_->sinkMap_))
        if (auto sink = iter->second.lock())
            return sink;
    return nullptr;
}
#endif // ENABLE_VIDEO

RingBufferPool&
Manager::getRingBufferPool()
{
    return *pimpl_->ringbufferpool_;
}

bool
Manager::hasAccount(const std::string& accountID)
{
    return accountFactory.hasAccount(accountID);
}

IceTransportFactory&
Manager::getIceTransportFactory()
{
    return *pimpl_->ice_tf_;
}

#ifdef ENABLE_VIDEO
VideoManager&
Manager::getVideoManager() const
{
    return *pimpl_->videoManager_;
}
#endif

std::vector<DRing::Message>
Manager::getLastMessages(const std::string& accountID, const uint64_t& base_timestamp)
{
    if (const auto acc = getAccount(accountID))
        return acc->getLastMessages(base_timestamp);
    return {};
}

std::map<std::string, std::string>
Manager::getNearbyPeers(const std::string& accountID)
{
    if (const auto acc = getAccount<JamiAccount>(accountID))
        return acc->getNearbyPeers();
    return {};
}

} // namespace jami
