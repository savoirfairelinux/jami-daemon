/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#include "sip/sipcall.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"

#include "logger.h"
#include "account_schema.h"

#include "fileutils.h"
#include "gittransport.h"
#include "map_utils.h"
#include "account.h"
#include "string_utils.h"
#include "jamidht/jamiaccount.h"
#include "account.h"
#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include "call_factory.h"

#include "connectivity/sip_utils.h"
#include "sip/sipvoiplink.h"
#include "sip/sipaccount_config.h"

#include "im/instant_messaging.h"

#include "config/yamlparser.h"

#if HAVE_ALSA
#include "audio/alsa/alsalayer.h"
#endif

#include "media/localrecordermanager.h"
#include "audio/sound/tonelist.h"
#include "audio/sound/dtmf.h"
#include "audio/ringbufferpool.h"

#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#include "plugin/streamdata.h"
#endif

#include "client/videomanager.h"

#include "conference.h"

#include "client/ring_signal.h"
#include "jami/call_const.h"
#include "jami/account_const.h"

#include "libav_utils.h"
#ifdef ENABLE_VIDEO
#include "video/video_scaler.h"
#include "video/sinkclient.h"
#include "video/video_base.h"
#include "media/video/video_mixer.h"
#endif
#include "audio/tonecontrol.h"

#include "data_transfer.h"
#include "jami/media_const.h"

#include <dhtnet/ice_transport_factory.h>
#include <dhtnet/ice_transport.h>
#include <dhtnet/upnp/upnp_context.h>

#include <libavutil/ffversion.h>

#include <opendht/thread_pool.h>

#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>

#include <git2.h>

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

#ifndef JAMI_DATADIR
#error "Define the JAMI_DATADIR macro as the data installation prefix of the package"
#endif

namespace jami {

/** To store uniquely a list of Call ids */
using CallIDSet = std::set<std::string>;

static constexpr const char* PACKAGE_OLD = "ring";

std::atomic_bool Manager::initialized = {false};

#if TARGET_OS_IOS
bool Manager::isIOSExtension = {false};
#endif

bool Manager::syncOnRegister = {true};

static void
copy_over(const std::filesystem::path& srcPath, const std::filesystem::path& destPath)
{
    std::ifstream src(srcPath);
    std::ofstream dest(destPath);
    dest << src.rdbuf();
    src.close();
    dest.close();
}

// Creates a backup of the file at "path" with a .bak suffix appended
static void
make_backup(const std::filesystem::path& path)
{
    auto backup_path = path;
    backup_path.replace_extension(".bak");
    copy_over(path, backup_path);
}

// Restore last backup of the configuration file
static void
restore_backup(const std::filesystem::path& path)
{
    auto backup_path = path;
    backup_path.replace_extension(".bak");
    copy_over(backup_path, path);
}

void
check_rename(const std::filesystem::path& old_dir, const std::filesystem::path& new_dir)
{
    if (old_dir == new_dir or not std::filesystem::is_directory(old_dir))
        return;

    std::error_code ec;
    if (not std::filesystem::is_directory(new_dir)) {
        JAMI_WARNING("Migrating {} to {}", old_dir, new_dir);
        std::filesystem::rename(old_dir, new_dir, ec);
        if (ec)
            JAMI_ERROR("Failed to rename {} to {}: {}", old_dir, new_dir, ec.message());
    } else {
        for (const auto& file_iterator : std::filesystem::directory_iterator(old_dir, ec)) {
            const auto& file_path = file_iterator.path();
            auto new_path = new_dir / file_path.filename();
            if (file_iterator.is_directory()
                and std::filesystem::is_directory(new_path)) {
                check_rename(file_path, new_path);
            } else {
                JAMI_WARNING("Migrating {} to {}", old_dir, new_path);
                std::filesystem::rename(file_path, new_path, ec);
                if (ec)
                    JAMI_ERROR("Failed to rename {} to {}: {}", file_path, new_path, ec.message());
            }
        }
        std::filesystem::remove_all(old_dir, ec);
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
    int level = 0;
    if (auto envvar = getenv(DHTLOGLEVEL)) {
        level = to_int<int>(envvar, 0);
        level = std::clamp(level, 0, 3);
        JAMI_DBG("DHTLOGLEVEL=%u", level);
    }
    Manager::instance().dhtLogLevel = level;
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
        level = to_int<int>(envvar, 0);

        // From 0 (min) to 6 (max)
        level = std::max(0, std::min(level, 6));
    }

    pj_log_set_level(level);
    pj_log_set_log_func([](int level, const char* data, int /*len*/) {
        if (level < 2)
            JAMI_ERR() << data;
        else if (level < 4)
            JAMI_WARN() << data;
        else
            JAMI_DBG() << data;
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
    char* envvar = getenv("RING_TLS_LOGLEVEL");
    int level = RING_TLS_LOGLEVEL;

    if (envvar != nullptr) {
        level = to_int<int>(envvar);

        // From 0 (min) to 9 (max)
        level = std::max(0, std::min(level, 9));
    }

    gnutls_global_set_log_level(level);
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
    void playATone(Tone::ToneId toneId);

    int getCurrentDeviceIndex(AudioDeviceType type);

    /**
     * Process remaining participant given a conference and the current call id.
     * Mainly called when a participant is detached or hagned up
     * @param current call id
     * @param conference pointer
     */
    void processRemainingParticipants(Conference& conf);

    /**
     * Create config directory in home user and return configuration file path
     */
    std::filesystem::path retrieveConfigPath() const;

    void unsetCurrentCall();

    void switchCall(const std::string& id);

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

    void loadAccount(const YAML::Node& item, int& errorCount);

    void sendTextMessageToConference(const Conference& conf,
                                     const std::map<std::string, std::string>& messages,
                                     const std::string& from) const noexcept;

    void bindCallToConference(const std::shared_ptr<SIPCall>& call, Conference& conf);

    void addMainParticipant(Conference& conf);

    bool hangupConference(Conference& conf);

    template<class T>
    std::shared_ptr<T> findAccount(const std::function<bool(const std::shared_ptr<T>&)>&);

    void initAudioDriver();

    void processIncomingCall(const std::string& accountId, Call& incomCall);
    static void stripSipPrefix(Call& incomCall);

    Manager& base_; // pimpl back-pointer

    std::shared_ptr<asio::io_context> ioContext_;
    std::thread ioContextRunner_;

    std::shared_ptr<dhtnet::upnp::UPnPContext> upnpContext_;

    /** Main scheduler */
    ScheduledExecutor scheduler_ {"manager"};

    std::atomic_bool autoAnswer_ {false};

    /** Application wide tone controller */
    ToneControl toneCtrl_;
    std::unique_ptr<AudioDeviceGuard> toneDeviceGuard_;

    /** Current Call ID */
    std::string currentCall_;

    /** Protected current call access */
    std::mutex currentCallMutex_;

    /** Protected sinks access */
    std::mutex sinksMutex_;

    /** Audio layer */
    std::shared_ptr<AudioLayer> audiodriver_ {nullptr};
    std::array<std::atomic_uint, 3> audioStreamUsers_ {};

    // Main thread
    std::unique_ptr<DTMF> dtmfKey_;

    /** Buffer to generate DTMF */
    std::shared_ptr<AudioFrame> dtmfBuf_;

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
    std::filesystem::path path_;

    /**
     * Instance of the RingBufferPool for the whole application
     *
     * In order to send signal to other parts of the application, one must pass through the
     * RingBufferMananger. Audio instances must be registered into the RingBufferMananger and bound
     * together via the Manager.
     *
     */
    std::unique_ptr<RingBufferPool> ringbufferpool_;

    std::atomic_bool finished_ {false};

    /* ICE support */
    std::shared_ptr<dhtnet::IceTransportFactory> ice_tf_;

    /* Sink ID mapping */
    std::map<std::string, std::weak_ptr<video::SinkClient>> sinkMap_;

    std::unique_ptr<VideoManager> videoManager_;

    std::unique_ptr<SIPVoIPLink> sipLink_;
#ifdef ENABLE_PLUGIN
    /* Jami Plugin Manager */
    JamiPluginManager jami_plugin_manager;
#endif

    std::mutex gitTransportsMtx_ {};
    std::map<git_smart_subtransport*, std::unique_ptr<P2PSubTransport>> gitTransports_ {};
};

Manager::ManagerPimpl::ManagerPimpl(Manager& base)
    : base_(base)
    , ioContext_(std::make_shared<asio::io_context>())
    , upnpContext_(std::make_shared<dhtnet::upnp::UPnPContext>(nullptr, Logger::dhtLogger()))
    , toneCtrl_(base.preferences)
    , dtmfBuf_(std::make_shared<AudioFrame>())
    , ringbufferpool_(new RingBufferPool)
#ifdef ENABLE_VIDEO
    , videoManager_(new VideoManager)
#endif
{
    jami::libav_utils::av_init();

    ioContextRunner_ = std::thread([context = ioContext_]() {
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
        std::ifstream file(path_);
        YAML::Node parsedFile = YAML::Load(file);
        file.close();
        const int error_count = base_.loadAccountMap(parsedFile);

        if (error_count > 0) {
            JAMI_WARN("Errors while parsing %s", path_.c_str());
            result = false;
        }
    } catch (const YAML::BadFile& e) {
        JAMI_WARN("Could not open configuration file");
        result = false;
    }

    return result;
}

/**
 * Multi Thread
 */
void
Manager::ManagerPimpl::playATone(Tone::ToneId toneId)
{
    if (not base_.voipPreferences.getPlayTones())
        return;

    std::lock_guard<std::mutex> lock(audioLayerMutex_);
    if (not audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return;
    }

    auto oldGuard = std::move(toneDeviceGuard_);
    toneDeviceGuard_ = base_.startAudioStream(AudioDeviceType::PLAYBACK);
    audiodriver_->flushUrgent();
    toneCtrl_.play(toneId);
}

int
Manager::ManagerPimpl::getCurrentDeviceIndex(AudioDeviceType type)
{
    if (not audiodriver_)
        return -1;
    switch (type) {
    case AudioDeviceType::PLAYBACK:
        return audiodriver_->getIndexPlayback();
    case AudioDeviceType::RINGTONE:
        return audiodriver_->getIndexRingtone();
    case AudioDeviceType::CAPTURE:
        return audiodriver_->getIndexCapture();
    default:
        return -1;
    }
}

void
Manager::ManagerPimpl::processRemainingParticipants(Conference& conf)
{
    const std::string current_callId(base_.getCurrentCallId());
    std::set<std::string> participants(conf.getCallIds());
    const size_t n = participants.size();
    JAMI_DBG("Process remaining %zu participant(s) from conference %s", n, conf.getConfId().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto& p : participants) {
            if (auto call = base_.getCallFromCallID(p)) {
                auto medias = call->getAudioStreams();
                for (const auto& media : medias) {
                    JAMI_DEBUG("[call:{}] Remove local audio {}", p, media.first);
                    base_.getRingBufferPool().flush(media.first);
                }
            }
        }

        base_.getRingBufferPool().flush(RingBufferPool::DEFAULT_ID);
    } else if (n == 1) {
        // this call is the last participant, hence
        // the conference is over
        auto p = participants.begin();
        if (auto call = base_.getCallFromCallID(*p)) {
            // if we are not listening to this conference and not a rendez-vous
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account) {
                JAMI_ERR("No account detected");
                return;
            }

            // Stay in a conference if 1 participants for swarm and rendezvous
            if (account->isRendezVous())
                return;

            if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account))
                if (acc->convModule()->isHosting("", conf.getConfId()))
                    return;

            // Else go in 1:1
            if (current_callId != conf.getConfId())
                base_.onHoldCall(account->getAccountID(), call->getCallId());
            else
                switchCall(call->getCallId());
        }

        JAMI_DBG("No remaining participants, remove conference");
        if (auto account = base_.getAccount(conf.getAccountId()))
            account->removeConference(conf.getConfId());
    } else {
        JAMI_DBG("No remaining participants, remove conference");
        if (auto account = base_.getAccount(conf.getAccountId()))
            account->removeConference(conf.getConfId());
        unsetCurrentCall();
    }
}

/**
 * Initialization: Main Thread
 */
std::filesystem::path
Manager::ManagerPimpl::retrieveConfigPath() const
{
    // TODO: Migrate config file name from dring.yml to jami.yml.
    return fileutils::get_config_dir() / "dring.yml";
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
Manager::ManagerPimpl::addWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    // Enable incoming call beep if needed.
    if (audiodriver_ and waitingCalls_.empty() and not currentCall_.empty())
        audiodriver_->playIncomingCallNotification(true);
    waitingCalls_.insert(id);
}

void
Manager::ManagerPimpl::removeWaitingCall(const std::string& id)
{
    std::lock_guard<std::mutex> m(waitingCallsMutex_);
    waitingCalls_.erase(id);
    if (audiodriver_ and waitingCalls_.empty())
        audiodriver_->playIncomingCallNotification(false);
}

void
Manager::ManagerPimpl::loadAccount(const YAML::Node& node, int& errorCount)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseValueOptional;

    std::string accountid;
    parseValue(node, "id", accountid);

    std::string accountType(ACCOUNT_TYPE_SIP);
    parseValueOptional(node, "type", accountType);

    if (!accountid.empty()) {
        if (auto a = base_.accountFactory.createAccount(accountType, accountid)) {
            auto config = a->buildConfig();
            config->unserialize(node);
            a->setConfig(std::move(config));
        } else {
            JAMI_ERROR("Failed to create account of type \"{:s}\"", accountType);
            ++errorCount;
        }
    }
}

// THREAD=VoIP
void
Manager::ManagerPimpl::sendTextMessageToConference(const Conference& conf,
                                                   const std::map<std::string, std::string>& messages,
                                                   const std::string& from) const noexcept
{
    std::set<std::string> participants(conf.getCallIds());
    for (const auto& callId : participants) {
        try {
            auto call = base_.getCallFromCallID(callId);
            if (not call)
                throw std::runtime_error("no associated call");
            call->sendTextMessage(messages, from);
        } catch (const std::exception& e) {
            JAMI_ERR("Failed to send message to conference participant %s: %s",
                     callId.c_str(),
                     e.what());
        }
    }
}

void
Manager::ManagerPimpl::bindCallToConference(const std::shared_ptr<SIPCall>& call, Conference& conf)
{
    const auto& callId = call->getCallId();
    const auto& confId = conf.getConfId();
    const auto& state = call->getStateStr();

    // ensure that calls are only in one conference at a time
    if (call->isConferenceParticipant())
        base_.detachParticipant(callId);

    JAMI_DEBUG("[call:{:s}] bind to conference {:s} (callState={:s})",
             callId,
             confId,
             state);

    auto medias = call->getAudioStreams();
    for (const auto& media : medias) {
        JAMI_DEBUG("[call:{}] Remove local audio {}", callId, media.first);
        base_.getRingBufferPool().unBindAll(media.first);
    }

    conf.bindCall(call);

    if (state == "HOLD") {
        conf.bindCallId(callId);
        base_.offHoldCall(call->getAccountId(), callId);
    } else if (state == "INCOMING") {
        conf.bindCallId(callId);
        base_.answerCall(*call);
    } else if (state == "CURRENT") {
        conf.bindCallId(callId);
    } else if (state == "INACTIVE") {
        conf.bindCallId(callId);
        base_.answerCall(*call);
    } else
        JAMI_WARNING("[call:{:s}] call state {:s} not recognized for conference",
                  callId,
                  state);
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
    : rand_(dht::crypto::getSeededRandomEngine<std::mt19937_64>())
    , preferences()
    , voipPreferences()
    , audioPreference()
#ifdef ENABLE_PLUGIN
    , pluginPreferences()
#endif
#ifdef ENABLE_VIDEO
    , videoPreferences()
#endif
    , callFactory(rand_)
    , accountFactory()
{
#if defined _MSC_VER
    gnutls_global_init();
#endif
    pimpl_ = std::make_unique<ManagerPimpl>(*this);
}

Manager::~Manager() {}

void
Manager::setAutoAnswer(bool enable)
{
    pimpl_->autoAnswer_ = enable;
}

void
Manager::init(const std::filesystem::path& config_file, libjami::InitFlag flags)
{
    // FIXME: this is no good
    initialized = true;

    git_libgit2_init();
    auto res = git_transport_register("git", p2p_transport_cb, nullptr);
    if (res < 0) {
        const git_error* error = giterr_last();
        JAMI_ERR("Unable to initialize git transport %s", error ? error->message : "(unknown)");
    }

#ifndef WIN32
    // Set the max number of open files.
    struct rlimit nofiles;
    if (getrlimit(RLIMIT_NOFILE, &nofiles) == 0) {
        if (nofiles.rlim_cur < nofiles.rlim_max && nofiles.rlim_cur <= 1024u) {
            nofiles.rlim_cur = std::min<rlim_t>(nofiles.rlim_max, 8192u);
            setrlimit(RLIMIT_NOFILE, &nofiles);
        }
    }
#endif

#define PJSIP_TRY(ret) \
    do { \
        if ((ret) != PJ_SUCCESS) \
            throw std::runtime_error(#ret " failed"); \
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
    JAMI_DBG("Using FFmpeg version %s", av_version_info());
    int git2_major = 0, git2_minor = 0, git2_rev = 0;
    if (git_libgit2_version(&git2_major, &git2_minor, &git2_rev) == 0) {
        JAMI_DBG("Using Libgit2 version %d.%d.%d", git2_major, git2_minor, git2_rev);
    }

    setDhtLogLevel();

    // Manager can restart without being recreated (Unit tests)
    // So only create the SipLink once
    pimpl_->sipLink_ = std::make_unique<SIPVoIPLink>();

    check_rename(fileutils::get_cache_dir(PACKAGE_OLD).string(),
                 fileutils::get_cache_dir().string());
    check_rename(fileutils::get_data_dir(PACKAGE_OLD).string(), fileutils::get_data_dir().string());
    check_rename(fileutils::get_config_dir(PACKAGE_OLD).string(),
                 fileutils::get_config_dir().string());

    pimpl_->ice_tf_ = std::make_shared<dhtnet::IceTransportFactory>(Logger::dhtLogger());

    pimpl_->path_ = config_file.empty() ? pimpl_->retrieveConfigPath() : config_file;
    JAMI_DBG("Configuration file path: %s", pimpl_->path_.c_str());

    bool no_errors = true;

    // manager can restart without being recreated (Unit tests)
    pimpl_->finished_ = false;

    try {
        no_errors = pimpl_->parseConfiguration();
    } catch (const YAML::Exception& e) {
        JAMI_ERR("%s", e.what());
        no_errors = false;
    }

    // always back up last error-free configuration
    if (no_errors) {
        make_backup(pimpl_->path_);
    } else {
        // restore previous configuration
        JAMI_WARN("Restoring last working configuration");

        try {
            // remove accounts from broken configuration
            removeAccounts();
            restore_backup(pimpl_->path_);
            pimpl_->parseConfiguration();
        } catch (const YAML::Exception& e) {
            JAMI_ERR("%s", e.what());
            JAMI_WARN("Restoring backup failed");
        }
    }

    if (!(flags & libjami::LIBJAMI_FLAG_NO_LOCAL_AUDIO)) {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        pimpl_->initAudioDriver();
        if (pimpl_->audiodriver_) {
            auto format = pimpl_->audiodriver_->getFormat();
            pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
            pimpl_->dtmfKey_.reset(
                new DTMF(getRingBufferPool().getInternalSamplingRate(),
                         getRingBufferPool().getInternalAudioFormat().sampleFormat));
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
        // Terminate UPNP context
        upnpContext()->shutdown();

        // Forbid call creation
        callFactory.forbid();

        // Hangup all remaining active calls
        JAMI_DBG("Hangup %zu remaining call(s)", callFactory.callCount());
        for (const auto& call : callFactory.getAllCalls())
            hangupCall(call->getAccountId(), call->getCallId());
        callFactory.clear();

        for (const auto& account : getAllAccounts<JamiAccount>()) {
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

        JAMI_DBG("Stopping schedulers and worker threads");

        // Flush remaining tasks (free lambda' with capture)
        pimpl_->scheduler_.stop();
        dht::ThreadPool::io().join();
        dht::ThreadPool::computation().join();

        // IceTransportFactory should be stopped after the io pool
        // as some ICE are destroyed in a ioPool (see ConnectionManager)
        // Also, it must be called before pj_shutdown to avoid any problem
        pimpl_->ice_tf_.reset();

        // NOTE: sipLink_->shutdown() is needed because this will perform
        // sipTransportBroker->shutdown(); which will call Manager::instance().sipVoIPLink()
        // so the pointer MUST NOT be resetted at this point
        if (pimpl_->sipLink_) {
            pimpl_->sipLink_->shutdown();
            pimpl_->sipLink_.reset();
        }

        pj_shutdown();
        pimpl_->gitTransports_.clear();
        git_libgit2_shutdown();

        if (!pimpl_->ioContext_->stopped()) {
            pimpl_->ioContext_->reset(); // allow to finish
            pimpl_->ioContext_->stop();  // make thread stop
        }
        if (pimpl_->ioContextRunner_.joinable())
            pimpl_->ioContextRunner_.join();

#if defined _MSC_VER
        gnutls_global_deinit();
#endif

    } catch (const VoipLinkException& err) {
        JAMI_ERR("%s", err.what());
    }
}

void
Manager::monitor(bool continuous)
{
    Logger::setMonitorLog(true);
    JAMI_DBG("############## START MONITORING ##############");
    JAMI_DBG("Using PJSIP version %s for %s", pj_get_version(), PJ_OS_NAME);
    JAMI_DBG("Using GnuTLS version %s", gnutls_check_version(nullptr));
    JAMI_DBG("Using OpenDHT version %s", dht::version());

#ifdef __linux__
#if defined(__ANDROID__)
#else
    auto opened_files
        = dhtnet::fileutils::readDirectory("/proc/" + std::to_string(getpid()) + "/fd").size();
    JAMI_DBG("Opened files: %lu", opened_files);
#endif
#endif

    for (const auto& call : callFactory.getAllCalls())
        call->monitor();
    for (const auto& account : getAllAccounts())
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account))
            acc->monitor();
    JAMI_DBG("############## END MONITORING ##############");
    Logger::setMonitorLog(continuous);
}

std::vector<std::map<std::string, std::string>>
Manager::getConnectionList(const std::string& accountId, const std::string& conversationId)
{
    std::vector<std::map<std::string, std::string>> connectionsList;

    if (accountId.empty()) {
        for (const auto& account : getAllAccounts<JamiAccount>()) {
            if (account->getRegistrationState() != RegistrationState::INITIALIZING) {
                const auto& cnl = account->getConnectionList(conversationId);
                connectionsList.insert(connectionsList.end(), cnl.begin(), cnl.end());
            }
        }
    } else {
        auto account = getAccount(accountId);
        if (account) {
            if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account)) {
                if (acc->getRegistrationState() != RegistrationState::INITIALIZING) {
                    const auto& cnl = acc->getConnectionList(conversationId);
                    connectionsList.insert(connectionsList.end(), cnl.begin(), cnl.end());
                }
            }
        }
    }

    return connectionsList;
}

std::vector<std::map<std::string, std::string>>
Manager::getChannelList(const std::string& accountId, const std::string& connectionId)
{
    // if account id is empty, return all channels
    // else return only for specific accountid
    std::vector<std::map<std::string, std::string>> channelsList;

    if (accountId.empty()) {
        for (const auto& account : getAllAccounts<JamiAccount>()) {
            if (account->getRegistrationState() != RegistrationState::INITIALIZING) {
                // add to channelsList all channels for this account
                const auto& cnl = account->getChannelList(connectionId);
                channelsList.insert(channelsList.end(), cnl.begin(), cnl.end());
            }
        }

    }

    else {
        // get the jamiaccount for this accountid and return its channels
        auto account = getAccount(accountId);
        if (account) {
            if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account)) {
                if (acc->getRegistrationState() != RegistrationState::INITIALIZING) {
                    const auto& cnl = acc->getChannelList(connectionId);
                    channelsList.insert(channelsList.end(), cnl.begin(), cnl.end());
                }
            }
        }
    }

    return channelsList;
}

bool
Manager::isCurrentCall(const Call& call) const
{
    return pimpl_->currentCall_ == call.getCallId();
}

bool
Manager::hasCurrentCall() const
{
    for (const auto& call : callFactory.getAllCalls()) {
        if (!call->isSubcall() && call->getStateStr() == libjami::Call::StateEvent::CURRENT)
            return true;
    }
    return false;
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
        if (account->isEnabled()) {
            if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account)) {
                // Note: shutdown the connections as doUnregister will not do it (because the
                // account is enabled)
                acc->shutdownConnections();
            }
            account->doUnregister();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

std::string
Manager::outgoingCall(const std::string& account_id,
                      const std::string& to,
                      const std::vector<libjami::MediaMap>& mediaList)
{
    JAMI_DBG() << "try outgoing call to '" << to << "'"
               << " with account '" << account_id << "'";

    std::shared_ptr<Call> call;

    try {
        call = newOutgoingCall(trim(to), account_id, mediaList);
    } catch (const std::exception& e) {
        JAMI_ERR("%s", e.what());
        return {};
    }

    if (not call)
        return {};

    stopTone();

    pimpl_->switchCall(call->getCallId());

    return call->getCallId();
}

// THREAD=Main : for outgoing Call
bool
Manager::answerCall(const std::string& accountId,
                    const std::string& callId,
                    const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return answerCall(*call, mediaList);
        }
    }
    return false;
}

bool
Manager::answerCall(Call& call, const std::vector<libjami::MediaMap>& mediaList)
{
    JAMI_INFO("Answer call %s", call.getCallId().c_str());

    if (call.getConnectionState() != Call::ConnectionState::RINGING) {
        // The call is already answered
        return true;
    }

    // If ringing
    stopTone();
    pimpl_->removeWaitingCall(call.getCallId());

    try {
        call.answer(mediaList);
    } catch (const std::runtime_error& e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    // if we dragged this call into a conference already
    if (auto conf = call.getConference())
        pimpl_->switchCall(conf->getConfId());
    else
        pimpl_->switchCall(call.getCallId());

    addAudio(call);

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording()) {
        auto recResult = call.toggleRecording();
        emitSignal<libjami::CallSignal::RecordPlaybackFilepath>(call.getCallId(), call.getPath());
        emitSignal<libjami::CallSignal::RecordingStateChanged>(call.getCallId(), recResult);
    }
    return true;
}

// THREAD=Main
bool
Manager::hangupCall(const std::string& accountId, const std::string& callId)
{
    auto account = getAccount(accountId);
    if (not account)
        return false;
    // store the current call id
    stopTone();
    pimpl_->removeWaitingCall(callId);

    /* We often get here when the call was hungup before being created */
    auto call = account->getCall(callId);
    if (not call) {
        JAMI_WARN("Could not hang up non-existant call %s", callId.c_str());
        return false;
    }

    // Disconnect streams
    removeAudio(call);

    if (call->isConferenceParticipant()) {
        removeCall(call);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (isCurrentCall(*call))
            pimpl_->unsetCurrentCall();
    }

    try {
        call->hangup(0);
    } catch (const VoipLinkException& e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    return true;
}

bool
Manager::hangupConference(const std::string& accountId, const std::string& confId)
{
    if (auto account = getAccount(accountId)) {
        if (auto conference = account->getConference(confId)) {
            return pimpl_->hangupConference(*conference);
        } else {
            JAMI_ERR("No such conference %s", confId.c_str());
        }
    }
    return false;
}

// THREAD=Main
bool
Manager::onHoldCall(const std::string&, const std::string& callId)
{
    bool result = true;

    stopTone();

    std::string current_callId(getCurrentCallId());

    if (auto call = getCallFromCallID(callId)) {
        try {
            result = call->onhold([=](bool ok) {
                if (!ok) {
                    JAMI_ERR("hold failed for call %s", callId.c_str());
                    return;
                }
                removeAudio(call); // Unbind calls in main buffer
                // Remove call from the queue if it was still there
                pimpl_->removeWaitingCall(callId);

                // keeps current call id if the action is not holding this call
                // or a new outgoing call. This could happen in case of a conference
                if (current_callId == callId)
                    pimpl_->unsetCurrentCall();
            });
        } catch (const VoipLinkException& e) {
            JAMI_ERR("%s", e.what());
            result = false;
        }
    } else {
        JAMI_DBG("CallID %s doesn't exist in call onHold", callId.c_str());
        return false;
    }

    return result;
}

// THREAD=Main
bool
Manager::offHoldCall(const std::string&, const std::string& callId)
{
    bool result = true;

    stopTone();

    std::shared_ptr<Call> call = getCallFromCallID(callId);
    if (!call)
        return false;

    try {
        result = call->offhold([=](bool ok) {
            if (!ok) {
                JAMI_ERR("off hold failed for call %s", callId.c_str());
                return;
            }

            if (auto conf = call->getConference())
                pimpl_->switchCall(conf->getConfId());
            else
                pimpl_->switchCall(call->getCallId());

            addAudio(*call);
        });
    } catch (const VoipLinkException& e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    return result;
}

// THREAD=Main
bool
Manager::transferCall(const std::string& accountId, const std::string& callId, const std::string& to)
{
    auto account = getAccount(accountId);
    if (not account)
        return false;
    if (auto call = account->getCall(callId)) {
        if (call->isConferenceParticipant())
            removeCall(call);
        call->transfer(to);
    } else
        return false;

    // remove waiting call in case we make transfer without even answer
    pimpl_->removeWaitingCall(callId);

    return true;
}

void
Manager::transferFailed()
{
    emitSignal<libjami::CallSignal::TransferFailed>();
}

void
Manager::transferSucceeded()
{
    emitSignal<libjami::CallSignal::TransferSucceeded>();
}

// THREAD=Main : Call:Incoming
bool
Manager::refuseCall(const std::string& accountId, const std::string& id)
{
    if (auto account = getAccount(accountId)) {
        if (auto call = account->getCall(id)) {
            stopTone();
            call->refuse();
            pimpl_->removeWaitingCall(id);
            removeAudio(call);
            return true;
        }
    }
    return false;
}

bool
Manager::holdConference(const std::string& accountId, const std::string& confId)
{
    JAMI_INFO("Hold conference %s", confId.c_str());

    if (const auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->detachLocal();
            emitSignal<libjami::CallSignal::ConferenceChanged>(accountId,
                                                               conf->getConfId(),
                                                               conf->getStateStr());
            return true;
        }
    }
    return false;
}

bool
Manager::unHoldConference(const std::string& accountId, const std::string& confId)
{
    JAMI_DBG("[conf:%s] un-holding conference", confId.c_str());

    if (const auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            // Unhold conf only if it was in hold state otherwise...
            // all participants are restarted
            if (conf->getState() == Conference::State::HOLD) {
                for (const auto& item : conf->getCallIds())
                    offHoldCall(accountId, item);

                pimpl_->switchCall(confId);
                conf->setState(Conference::State::ACTIVE_ATTACHED);
                emitSignal<libjami::CallSignal::ConferenceChanged>(accountId,
                                                                   conf->getConfId(),
                                                                   conf->getStateStr());
                return true;
            } else if (conf->getState() == Conference::State::ACTIVE_DETACHED) {
                pimpl_->addMainParticipant(*conf);
            }
        }
    }
    return false;
}

bool
Manager::addParticipant(const std::string& accountId,
                        const std::string& callId,
                        const std::string& account2Id,
                        const std::string& conferenceId)
{
    auto account = getAccount(accountId);
    auto account2 = getAccount(account2Id);
    if (account && account2) {
        auto call = std::dynamic_pointer_cast<SIPCall>(account->getCall(callId));
        auto conf = account2->getConference(conferenceId);
        if (!call or !conf)
            return false;
        auto callConf = call->getConference();
        if (callConf != conf)
            return addParticipant(call, *conf);
    }
    return false;
}

bool
Manager::addParticipant(const std::shared_ptr<SIPCall>& call, Conference& conference)
{
    // No-op if the call is already a conference participant
    /*if (call.getConfId() == conference.getConfId()) {
        JAMI_WARN("Call %s already participant of conf %s", call.getCallId().c_str(),
    conference.getConfId().c_str()); return true;
    }*/

    JAMI_DEBUG("Add participant {:s} to conference {:s}",
             call->getCallId(),
             conference.getConfId());

    // store the current call id (it will change in offHoldCall or in answerCall)
    pimpl_->bindCallToConference(call, conference);

    // Don't attach current user yet
    if (conference.getState() == Conference::State::ACTIVE_DETACHED)
        return true;

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // to conference
    pimpl_->unsetCurrentCall();
    pimpl_->addMainParticipant(conference);
    pimpl_->switchCall(conference.getConfId());
    addAudio(*call);

    return true;
}

void
Manager::ManagerPimpl::addMainParticipant(Conference& conf)
{
    conf.attachLocal();
    emitSignal<libjami::CallSignal::ConferenceChanged>(conf.getAccountId(),
                                                       conf.getConfId(),
                                                       conf.getStateStr());
    switchCall(conf.getConfId());
}

bool
Manager::ManagerPimpl::hangupConference(Conference& conference)
{
    JAMI_DBG("Hangup conference %s", conference.getConfId().c_str());
    std::set<std::string> participants(conference.getCallIds());
    for (const auto& callId : participants) {
        if (auto call = base_.getCallFromCallID(callId))
            base_.hangupCall(call->getAccountId(), callId);
    }
    unsetCurrentCall();
    return true;
}

bool
Manager::addMainParticipant(const std::string& accountId, const std::string& conferenceId)
{
    JAMI_INFO("Add main participant to conference %s", conferenceId.c_str());

    if (auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(conferenceId)) {
            pimpl_->addMainParticipant(*conf);
            JAMI_DBG("Successfully added main participant to conference %s", conferenceId.c_str());
            return true;
        } else
            JAMI_WARN("Failed to add main participant to conference %s", conferenceId.c_str());
    }
    return false;
}

std::shared_ptr<Call>
Manager::getCallFromCallID(const std::string& callID) const
{
    return callFactory.getCall(callID);
}

bool
Manager::joinParticipant(const std::string& accountId,
                         const std::string& callId1,
                         const std::string& account2Id,
                         const std::string& callId2,
                         bool attached)
{
    JAMI_INFO("JoinParticipant(%s, %s, %i)", callId1.c_str(), callId2.c_str(), attached);
    auto account = getAccount(accountId);
    auto account2 = getAccount(account2Id);
    if (not account or not account2) {
        return false;
    }

    JAMI_INFO("Creating conference for participants %s and %s. Attach host [%s]",
              callId1.c_str(),
              callId2.c_str(),
              attached ? "YES" : "NO");

    if (callId1 == callId2) {
        JAMI_ERROR("Cannot join participant {:s} to itself", callId1);
        return false;
    }

    // Set corresponding conference ids for call 1
    auto call1 = std::dynamic_pointer_cast<SIPCall>(account->getCall(callId1));
    if (!call1) {
        JAMI_ERROR("Could not find call {:s}", callId1);
        return false;
    }

    // Set corresponding conference details
    auto call2 = std::dynamic_pointer_cast<SIPCall>(account2->getCall(callId2));
    if (!call2) {
        JAMI_ERROR("Could not find call {:s}", callId2);
        return false;
    }

    auto mediaAttr = call1->getMediaAttributeList();
    if (mediaAttr.empty())
        mediaAttr = call2->getMediaAttributeList();
    auto conf = std::make_shared<Conference>(account, "", true, mediaAttr);
    conf->initLayout();
    account->attach(conf);
    emitSignal<libjami::CallSignal::ConferenceCreated>(account->getAccountID(), conf->getConfId());

    // Bind calls according to their state
    pimpl_->bindCallToConference(call1, *conf);
    pimpl_->bindCallToConference(call2, *conf);

    // Switch current call id to this conference
    if (attached) {
        pimpl_->switchCall(conf->getConfId());
        conf->setState(Conference::State::ACTIVE_ATTACHED);
    } else {
        conf->detachLocal();
    }
    emitSignal<libjami::CallSignal::ConferenceChanged>(account->getAccountID(),
                                                       conf->getConfId(),
                                                       conf->getStateStr());

    return true;
}

void
Manager::createConfFromParticipantList(const std::string& accountId,
                                       const std::vector<std::string>& participantList)
{
    auto account = getAccount(accountId);
    if (not account) {
        JAMI_WARN("Can't find account");
        return;
    }

    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        JAMI_ERR("Participant number must be higher or equal to 2");
        return;
    }

    auto conf = std::make_shared<Conference>(account);
    conf->initLayout();

    unsigned successCounter = 0;
    for (const auto& numberaccount : participantList) {
        std::string tostr(numberaccount.substr(0, numberaccount.find(',')));
        std::string account(numberaccount.substr(numberaccount.find(',') + 1, numberaccount.size()));

        pimpl_->unsetCurrentCall();

        // Create call
        auto callId = outgoingCall(account, tostr, {});
        if (!getCallFromCallID(callId))
            continue;

        // Manager methods may behave differently if the call id participates in a conference
        conf->bindCall(std::dynamic_pointer_cast<SIPCall>(getCallFromCallID(callId)));
        successCounter++;
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        account->attach(conf);
        emitSignal<libjami::CallSignal::ConferenceCreated>(accountId, conf->getConfId());
    }
}

bool
Manager::detachLocal(const std::shared_ptr<Conference>& conf)
{
    if (not conf)
        return false;

    JAMI_INFO("Detach local participant from conference %s", conf->getConfId().c_str());
    conf->detachLocal();
    emitSignal<libjami::CallSignal::ConferenceChanged>(conf->getAccountId(),
                                                       conf->getConfId(),
                                                       conf->getStateStr());
    pimpl_->unsetCurrentCall();
    return true;
}

bool
Manager::detachParticipant(const std::string& callId)
{
    JAMI_DEBUG("Detach participant {:s}", callId);

    auto call = getCallFromCallID(callId);
    if (!call) {
        JAMI_ERROR("Could not find call {:s}", callId);
        return false;
    }

    // Don't hold ringing calls when detaching them from conferences
    if (call->getStateStr() != "RINGING")
        onHoldCall(call->getAccountId(), callId);

    removeCall(call);
    return true;
}

void
Manager::removeCall(const std::shared_ptr<Call>& call)
{
    JAMI_DEBUG("Remove participant {:s}", call->getCallId());

    auto conf = call->getConference();
    if (not conf) {
        JAMI_ERROR("No conference, cannot remove participant");
        return;
    }

    conf->removeCall(std::dynamic_pointer_cast<SIPCall>(call));

    removeAudio(call);

    emitSignal<libjami::CallSignal::ConferenceChanged>(conf->getAccountId(),
                                                       conf->getConfId(),
                                                       conf->getStateStr());

    pimpl_->processRemainingParticipants(*conf);
}

bool
Manager::joinConference(const std::string& accountId,
                        const std::string& confId1,
                        const std::string& account2Id,
                        const std::string& confId2)
{
    auto account = getAccount(accountId);
    auto account2 = getAccount(account2Id);
    if (not account) {
        JAMI_ERR("Can't find account: %s", accountId.c_str());
        return false;
    }
    if (not account2) {
        JAMI_ERR("Can't find account: %s", account2Id.c_str());
        return false;
    }

    auto conf = account->getConference(confId1);
    if (not conf) {
        JAMI_ERR("Not a valid conference ID: %s", confId1.c_str());
        return false;
    }

    auto conf2 = account2->getConference(confId2);
    if (not conf2) {
        JAMI_ERR("Not a valid conference ID: %s", confId2.c_str());
        return false;
    }

    std::set<std::string> participants(conf->getCallIds());

    std::vector<std::shared_ptr<Call>> calls;
    calls.reserve(participants.size());

    // Detach and remove all participant from conf1 before add
    // ... to conf2
    for (const auto& p : participants) {
        JAMI_DBG("Detach participant %s", p.c_str());
        if (auto call = account->getCall(p)) {
            conf->removeCall(std::dynamic_pointer_cast<SIPCall>(call));
            removeAudio(call);
            calls.emplace_back(std::move(call));
        } else {
            JAMI_ERR("Could not find call %s", p.c_str());
        }
    }
    // Remove conf1
    account->removeConference(confId1);

    for (const auto& c : calls)
        addParticipant(std::dynamic_pointer_cast<SIPCall>(c), *conf2);

    return true;
}

void
Manager::addAudio(Call& call)
{
    const auto& callId = call.getCallId();
    JAMI_LOG("Add audio to call {}", callId);

    if (call.isConferenceParticipant()) {
        JAMI_DEBUG("[conf:{}] Attach local audio", callId);

        // bind to conference participant
        /*auto iter = pimpl_->conferenceMap_.find(callId);
        if (iter != pimpl_->conferenceMap_.end() and iter->second) {
            iter->second->bindParticipant(callId);
        }*/
    } else {
        JAMI_DEBUG("[call:{}] Attach audio", callId);

        // bind to main
        auto medias = call.getAudioStreams();
        for (const auto& media : medias) {
            JAMI_DEBUG("[call:{}] Attach audio", media.first);
            getRingBufferPool().bindRingbuffers(media.first,
                                            RingBufferPool::DEFAULT_ID);
        }
        auto oldGuard = std::move(call.audioGuard);
        call.audioGuard = startAudioStream(AudioDeviceType::PLAYBACK);

        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        if (!pimpl_->audiodriver_) {
            JAMI_ERROR("Audio driver not initialized");
            return;
        }
        pimpl_->audiodriver_->flushUrgent();
        getRingBufferPool().flushAllBuffers();
    }
}

void
Manager::removeAudio(const std::shared_ptr<Call>& call)
{
    const auto& callId = call->getCallId();
    auto medias = call->getAudioStreams();
    for (const auto& media : medias) {
        JAMI_DEBUG("[call:{}] Remove local audio {}", callId, media.first);
        getRingBufferPool().unBindAll(media.first);
    }
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

std::shared_ptr<dhtnet::upnp::UPnPContext>
Manager::upnpContext() const
{
    return pimpl_->upnpContext_;
}

std::shared_ptr<Task>
Manager::scheduleTask(std::function<void()>&& task,
                      std::chrono::steady_clock::time_point when,
                      const char* filename,
                      uint32_t linum)
{
    return pimpl_->scheduler_.schedule(std::move(task), when, filename, linum);
}

std::shared_ptr<Task>
Manager::scheduleTaskIn(std::function<void()>&& task,
                        std::chrono::steady_clock::duration timeout,
                        const char* filename,
                        uint32_t linum)
{
    return pimpl_->scheduler_.scheduleIn(std::move(task), timeout, filename, linum);
}

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
            if (auto jamiAccount = std::dynamic_pointer_cast<JamiAccount>(account)) {
                auto accountConfig = jamiAccount->getPath() / "config.yml";
                if (not std::filesystem::is_regular_file(accountConfig)) {
                    saveConfig(jamiAccount);
                }
            } else {
                account->config().serialize(out);
            }
        }
        out << YAML::EndSeq;

        // FIXME: this is a hack until we get rid of accountOrder
        preferences.verifyAccountOrder(getAccountList());
        preferences.serialize(out);
        voipPreferences.serialize(out);
        audioPreference.serialize(out);
#ifdef ENABLE_VIDEO
        videoPreferences.serialize(out);
#endif
#ifdef ENABLE_PLUGIN
        pluginPreferences.serialize(out);
#endif

        std::lock_guard<std::mutex> lock(dhtnet::fileutils::getFileLock(pimpl_->path_));
        std::ofstream fout(pimpl_->path_);
        fout.write(out.c_str(), out.size());
    } catch (const YAML::Exception& e) {
        JAMI_ERR("%s", e.what());
    } catch (const std::runtime_error& e) {
        JAMI_ERR("%s", e.what());
    }
}

// THREAD=Main | VoIPLink
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

    std::shared_ptr<AudioDeviceGuard> audioGuard = startAudioStream(AudioDeviceType::PLAYBACK);
    if (not pimpl_->audiodriver_->waitForStart(std::chrono::seconds(1))) {
        JAMI_ERR("Failed to start audio layer...");
        return;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    unsigned size = (unsigned) ((pulselen * (long) pimpl_->audiodriver_->getSampleRate()) / 1000ul);
    if (!pimpl_->dtmfBuf_ or pimpl_->dtmfBuf_->getFrameSize() != size)
        pimpl_->dtmfBuf_ = std::make_shared<AudioFrame>(pimpl_->audiodriver_->getFormat(), size);

    // Handle dtmf
    pimpl_->dtmfKey_->startTone(code);

    // copy the sound
    if (pimpl_->dtmfKey_->generateDTMF(pimpl_->dtmfBuf_->pointer())) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();

        pimpl_->audiodriver_->putUrgent(pimpl_->dtmfBuf_);
    }

    scheduler().scheduleIn([audioGuard] { JAMI_WARN("End of dtmf"); },
                           std::chrono::milliseconds(pulselen));

    // TODO Cache the DTMF
}

// Multi-thread
bool
Manager::incomingCallsWaiting()
{
    std::lock_guard<std::mutex> m(pimpl_->waitingCallsMutex_);
    return not pimpl_->waitingCalls_.empty();
}

void
Manager::incomingCall(const std::string& accountId, Call& call)
{
    if (not accountId.empty()) {
        pimpl_->stripSipPrefix(call);
    }

    std::string from("<" + call.getPeerNumber() + ">");

    auto const& account = getAccount(accountId);
    if (not account) {
        JAMI_ERR("Incoming call %s on unknown account %s",
                 call.getCallId().c_str(),
                 accountId.c_str());
        return;
    }

    // Process the call.
    pimpl_->processIncomingCall(accountId, call);
}

void
Manager::incomingMessage(const std::string& accountId,
                         const std::string& callId,
                         const std::string& from,
                         const std::map<std::string, std::string>& messages)
{
    auto account = getAccount(accountId);
    if (not account) {
        return;
    }
    if (auto call = account->getCall(callId)) {
        if (call->isConferenceParticipant()) {
            if (auto conf = call->getConference()) {
                JAMI_DBG("Is a conference, send incoming message to everyone");
                // filter out vcards messages  as they could be resent by master as its own vcard
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
                emitSignal<libjami::CallSignal::IncomingMessage>(accountId,
                                                                 conf->getConfId(),
                                                                 from,
                                                                 messages);
            } else {
                JAMI_ERR("no conference associated to ID %s", callId.c_str());
            }
        } else {
            emitSignal<libjami::CallSignal::IncomingMessage>(accountId, callId, from, messages);
        }
    }
}

void
Manager::sendCallTextMessage(const std::string& accountId,
                             const std::string& callID,
                             const std::map<std::string, std::string>& messages,
                             const std::string& from,
                             bool /*isMixed TODO: use it */)
{
    auto account = getAccount(accountId);
    if (not account) {
        return;
    }

    if (auto conf = account->getConference(callID)) {
        JAMI_DBG("Is a conference, send instant message to everyone");
        pimpl_->sendTextMessageToConference(*conf, messages, from);
    } else if (auto call = account->getCall(callID)) {
        if (call->isConferenceParticipant()) {
            if (auto conf = call->getConference()) {
                JAMI_DBG("Call is participant in a conference, send instant message to everyone");
                pimpl_->sendTextMessageToConference(*conf, messages, from);
            } else {
                JAMI_ERR("no conference associated to call ID %s", callID.c_str());
            }
        } else {
            try {
                call->sendTextMessage(messages, from);
            } catch (const im::InstantMessageException& e) {
                JAMI_ERR("Failed to send message to call %s: %s",
                         call->getCallId().c_str(),
                         e.what());
            }
        }
    } else {
        JAMI_ERR("Failed to send message to %s: inexistent call ID", callID.c_str());
    }
}

// THREAD=VoIP CALL=Outgoing
void
Manager::peerAnsweredCall(Call& call)
{
    const auto& callId = call.getCallId();
    JAMI_DBG("[call:%s] Peer answered", callId.c_str());

    // The if statement is useful only if we sent two calls at the same time.
    if (isCurrentCall(call))
        stopTone();

    addAudio(call);

    if (pimpl_->audiodriver_) {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        getRingBufferPool().flushAllBuffers();
        pimpl_->audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording()) {
        auto result = call.toggleRecording();
        emitSignal<libjami::CallSignal::RecordPlaybackFilepath>(callId, call.getPath());
        emitSignal<libjami::CallSignal::RecordingStateChanged>(callId, result);
    }
}

// THREAD=VoIP Call=Outgoing
void
Manager::peerRingingCall(Call& call)
{
    JAMI_DBG("[call:%s] Peer ringing", call.getCallId().c_str());

    if (!hasCurrentCall())
        ringback();
}

// THREAD=VoIP Call=Outgoing/Ingoing
void
Manager::peerHungupCall(const std::shared_ptr<Call>& call)
{
    const auto& callId = call->getCallId();
    JAMI_DEBUG("[call:{}] Peer hung up", callId);

    if (call->isConferenceParticipant()) {
        removeCall(call);
    } else if (isCurrentCall(*call)) {
        stopTone();
        pimpl_->unsetCurrentCall();
    }

    call->peerHungup();

    pimpl_->removeWaitingCall(callId);
    if (not incomingCallsWaiting())
        stopTone();

    removeAudio(call);
}

// THREAD=VoIP
void
Manager::callBusy(Call& call)
{
    JAMI_DBG("[call:%s] Busy", call.getCallId().c_str());

    if (isCurrentCall(call)) {
        pimpl_->unsetCurrentCall();
    }

    pimpl_->removeWaitingCall(call.getCallId());
    if (not incomingCallsWaiting())
        stopTone();
}

// THREAD=VoIP
void
Manager::callFailure(const std::shared_ptr<Call>& call)
{
    JAMI_DEBUG("[call:{}] {} failed",
             call->getCallId(),
             call->isSubcall() ? "Sub-call" : "Parent call");

    if (isCurrentCall(*call)) {
        pimpl_->unsetCurrentCall();
    }

    if (call->isConferenceParticipant()) {
        JAMI_DEBUG("[call {}] Participating in a conference. Remove", call->getCallId());
        // remove this participant
        removeCall(call);
    }

    pimpl_->removeWaitingCall(call->getCallId());
    if (not call->isSubcall() && not incomingCallsWaiting())
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
    pimpl_->toneDeviceGuard_.reset();
}

/**
 * Multi Thread
 */
void
Manager::playTone()
{
    pimpl_->playATone(Tone::ToneId::DIALTONE);
}

/**
 * Multi Thread
 */
void
Manager::playToneWithMessage()
{
    pimpl_->playATone(Tone::ToneId::CONGESTION);
}

/**
 * Multi Thread
 */
void
Manager::congestion()
{
    pimpl_->playATone(Tone::ToneId::CONGESTION);
}

/**
 * Multi Thread
 */
void
Manager::ringback()
{
    pimpl_->playATone(Tone::ToneId::RINGTONE);
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

    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_) {
            JAMI_ERR("no audio layer in ringtone");
            return;
        }
        // start audio if not started AND flush all buffers (main and urgent)
        auto oldGuard = std::move(pimpl_->toneDeviceGuard_);
        pimpl_->toneDeviceGuard_ = startAudioStream(AudioDeviceType::RINGTONE);
        auto format = pimpl_->audiodriver_->getFormat();
        pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
    }

    if (not pimpl_->toneCtrl_.setAudioFile(account->getRingtonePath().string()))
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
    {
        std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);
        audioPreference.setAlsaPlugin(audioPlugin);
        pimpl_->audiodriver_.reset();
        pimpl_->initAudioDriver();
    }
    // Recreate audio driver with new settings
    saveConfig();
}

/**
 * Set audio output device
 */
void
Manager::setAudioDevice(int index, AudioDeviceType type)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio driver not initialized");
        return;
    }
    if (pimpl_->getCurrentDeviceIndex(type) == index) {
        JAMI_WARN("Audio device already selected ; doing nothing.");
        return;
    }

    pimpl_->audiodriver_->updatePreference(audioPreference, index, type);

    // Recreate audio driver with new settings
    pimpl_->audiodriver_.reset();
    pimpl_->initAudioDriver();
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

    return {std::to_string(pimpl_->audiodriver_->getIndexPlayback()),
            std::to_string(pimpl_->audiodriver_->getIndexCapture()),
            std::to_string(pimpl_->audiodriver_->getIndexRingtone())};
}

void
Manager::startAudio()
{
    if (!pimpl_->audiodriver_)
        pimpl_->audiodriver_.reset(pimpl_->base_.audioPreference.createAudioLayer());
    constexpr std::array<AudioDeviceType, 3> TYPES {AudioDeviceType::CAPTURE,
                                                    AudioDeviceType::PLAYBACK,
                                                    AudioDeviceType::RINGTONE};
    for (const auto& type : TYPES)
        if (pimpl_->audioStreamUsers_[(unsigned) type])
            pimpl_->audiodriver_->startStream(type);
}

AudioDeviceGuard::AudioDeviceGuard(Manager& manager, AudioDeviceType type)
    : manager_(manager)
    , type_(type)
{
    auto streamId = (unsigned) type;
    if (streamId >= manager_.pimpl_->audioStreamUsers_.size())
        throw std::invalid_argument("Invalid audio device type");
    if (manager_.pimpl_->audioStreamUsers_[streamId]++ == 0) {
        if (auto layer = manager_.getAudioDriver())
            layer->startStream(type);
    }
}

AudioDeviceGuard::~AudioDeviceGuard()
{
    auto streamId = (unsigned) type_;
    if (--manager_.pimpl_->audioStreamUsers_[streamId] == 0) {
        if (auto layer = manager_.getAudioDriver())
            layer->stopStream(type_);
    }
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
Manager::toggleRecordingCall(const std::string& accountId, const std::string& id)
{
    bool result = false;
    if (auto account = getAccount(accountId)) {
        std::shared_ptr<Recordable> rec;
        if (auto conf = account->getConference(id)) {
            JAMI_DBG("toggle recording for conference %s", id.c_str());
            rec = conf;
        } else if (auto call = account->getCall(id)) {
            JAMI_DBG("toggle recording for call %s", id.c_str());
            rec = call;
        } else {
            JAMI_ERR("Could not find recordable instance %s", id.c_str());
            return false;
        }
        result = rec->toggleRecording();
        emitSignal<libjami::CallSignal::RecordPlaybackFilepath>(id, rec->getPath());
        emitSignal<libjami::CallSignal::RecordingStateChanged>(id, result);
    }
    return result;
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

        auto oldGuard = std::move(pimpl_->toneDeviceGuard_);
        pimpl_->toneDeviceGuard_ = startAudioStream(AudioDeviceType::RINGTONE);
        auto format = pimpl_->audiodriver_->getFormat();
        pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
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

    pimpl_->toneCtrl_.stopAudioFile();
    pimpl_->toneDeviceGuard_.reset();
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
Manager::setAudioManager(const std::string& api)
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
        audioPreference.setAudioApi(api);
        pimpl_->audiodriver_.reset();
        pimpl_->initAudioDriver();
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
Manager::getAudioInputDeviceIndex(const std::string& name)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return 0;
    }

    return pimpl_->audiodriver_->getAudioDeviceIndex(name, AudioDeviceType::CAPTURE);
}

int
Manager::getAudioOutputDeviceIndex(const std::string& name)
{
    std::lock_guard<std::mutex> lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERR("Audio layer not initialized");
        return 0;
    }

    return pimpl_->audiodriver_->getAudioDeviceIndex(name, AudioDeviceType::PLAYBACK);
}

std::string
Manager::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getAlsaPlugin();
}

std::string
Manager::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce();
}

void
Manager::setNoiseSuppressState(const std::string& state)
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
Manager::ManagerPimpl::initAudioDriver()
{
    audiodriver_.reset(base_.audioPreference.createAudioLayer());
    constexpr std::array<AudioDeviceType, 3> TYPES {AudioDeviceType::CAPTURE,
                                                    AudioDeviceType::PLAYBACK,
                                                    AudioDeviceType::RINGTONE};
    for (const auto& type : TYPES)
        if (audioStreamUsers_[(unsigned) type])
            audiodriver_->startStream(type);
}

// Internal helper method
void
Manager::ManagerPimpl::stripSipPrefix(Call& incomCall)
{
    // strip sip: which is not required and bring confusion with ip to ip calls
    // when placing new call from history.
    std::string peerNumber(incomCall.getPeerNumber());

    const char SIP_PREFIX[] = "sip:";
    size_t startIndex = peerNumber.find(SIP_PREFIX);

    if (startIndex != std::string::npos)
        incomCall.setPeerNumber(peerNumber.substr(startIndex + sizeof(SIP_PREFIX) - 1));
}

// Internal helper method
void
Manager::ManagerPimpl::processIncomingCall(const std::string& accountId, Call& incomCall)
{
    base_.stopTone();

    auto incomCallId = incomCall.getCallId();
    auto currentCall = base_.getCurrentCall();

    auto w = incomCall.getAccount();
    auto account = w.lock();
    if (!account) {
        JAMI_ERR("No account detected");
        return;
    }

    auto username = incomCall.toUsername();
    if (username.find('/') != std::string::npos) {
        // Avoid to do heavy stuff in SIPVoIPLink's transaction_request_cb
        dht::ThreadPool::io().run([account, incomCallId, username]() {
            if (auto jamiAccount = std::dynamic_pointer_cast<JamiAccount>(account))
                jamiAccount->handleIncomingConversationCall(incomCallId, username);
        });
        return;
    }

    auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(
        incomCall.getMediaAttributeList());

    if (mediaList.empty())
        JAMI_WARN("Incoming call %s has an empty media list", incomCallId.c_str());

    JAMI_INFO("Incoming call %s on account %s with %lu media",
              incomCallId.c_str(),
              accountId.c_str(),
              mediaList.size());

    emitSignal<libjami::CallSignal::IncomingCallWithMedia>(accountId,
                                                           incomCallId,
                                                           incomCall.getPeerNumber(),
                                                           mediaList);

    if (not base_.hasCurrentCall()) {
        incomCall.setState(Call::ConnectionState::RINGING);
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        if (not account->isRendezVous())
            base_.playRingtone(accountId);
#endif
    }

    addWaitingCall(incomCallId);

    if (account->isRendezVous()) {
        dht::ThreadPool::io().run([this, account, incomCall = incomCall.shared_from_this()] {
            base_.answerCall(*incomCall);

            for (const auto& callId : account->getCallList()) {
                if (auto call = account->getCall(callId)) {
                    if (call->getState() != Call::CallState::ACTIVE)
                        continue;
                    if (call != incomCall) {
                        if (auto conf = call->getConference()) {
                            base_.addParticipant(std::dynamic_pointer_cast<SIPCall>(incomCall), *conf);
                        } else {
                            base_.joinParticipant(account->getAccountID(),
                                                  incomCall->getCallId(),
                                                  account->getAccountID(),
                                                  call->getCallId(),
                                                  false);
                        }
                        return;
                    }
                }
            }

            // First call
            auto conf = std::make_shared<Conference>(account, "", false);
            conf->initLayout();
            account->attach(conf);
            emitSignal<libjami::CallSignal::ConferenceCreated>(account->getAccountID(),
                                                               conf->getConfId());

            // Bind calls according to their state
            bindCallToConference(std::dynamic_pointer_cast<SIPCall>(incomCall), *conf);
            conf->detachLocal();
            emitSignal<libjami::CallSignal::ConferenceChanged>(account->getAccountID(),
                                                               conf->getConfId(),
                                                               conf->getStateStr());
        });
    } else if (autoAnswer_ || account->isAutoAnswerEnabled()) {
        dht::ThreadPool::io().run(
            [this, incomCall = incomCall.shared_from_this()] { base_.answerCall(*incomCall); });
    } else if (currentCall && currentCall->getCallId() != incomCallId) {
        // Test if already calling this person
        auto peerNumber = incomCall.getPeerNumber();
        auto currentPeerNumber = currentCall->getPeerNumber();
        string_replace(peerNumber, "@ring.dht", "");
        string_replace(currentPeerNumber, "@ring.dht", "");
        if (currentCall->getAccountId() == account->getAccountID()
            && currentPeerNumber == peerNumber) {
            auto answerToCall = false;
            auto downgradeToAudioOnly = currentCall->isAudioOnly() != incomCall.isAudioOnly();
            if (downgradeToAudioOnly)
                // Accept the incoming audio only
                answerToCall = incomCall.isAudioOnly();
            else
                // Accept the incoming call from the higher id number
                answerToCall = (account->getUsername().compare(peerNumber) < 0);

            if (answerToCall) {
                runOnMainThread([accountId = currentCall->getAccountId(),
                                 currentCallID = currentCall->getCallId(),
                                 incomCall = incomCall.shared_from_this()] {
                    auto& mgr = Manager::instance();
                    mgr.answerCall(*incomCall);
                    mgr.hangupCall(accountId, currentCallID);
                });
            }
        }
    }
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
    format.nb_channels = std::max(currentFormat.nb_channels,
                                  std::min(format.nb_channels, 2u)); // max 2 channels.
    format.sample_rate = std::max(currentFormat.sample_rate, format.sample_rate);

    if (currentFormat == format)
        return format;

    JAMI_DEBUG("Audio format changed: {} -> {}", currentFormat.toString(), format.toString());

    pimpl_->ringbufferpool_->setInternalAudioFormat(format);
    pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
    pimpl_->dtmfKey_.reset(new DTMF(format.sample_rate, format.sampleFormat));

    return format;
}

void
Manager::setAccountsOrder(const std::string& order)
{
    JAMI_DBG("Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder(order);

    saveConfig();

    emitSignal<libjami::ConfigurationSignal::AccountsChanged>();
}

std::vector<std::string>
Manager::getAccountList() const
{
    // Concatenate all account pointers in a single map
    std::vector<std::string> v;
    v.reserve(accountCount());
    for (const auto& account : getAllAccounts()) {
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
        return {};
    }
}

std::map<std::string, std::string>
Manager::getVolatileAccountDetails(const std::string& accountID) const
{
    const auto account = getAccount(accountID);

    if (account) {
        return account->getVolatileAccountDetails();
    } else {
        JAMI_ERR("Could not get volatile account details on a non-existing accountID %s",
                 accountID.c_str());
        return {};
    }
}

void
Manager::setAccountDetails(const std::string& accountID,
                           const std::map<std::string, std::string>& details)
{
    JAMI_DBG("Set account details for %s", accountID.c_str());

    auto account = getAccount(accountID);
    if (not account) {
        JAMI_ERR("Could not find account %s", accountID.c_str());
        return;
    }

    // Ignore if nothing has changed
    if (details == account->getAccountDetails())
        return;

    // Unregister before modifying any account information
    account->doUnregister([&](bool /* transport_free */) {
        account->setAccountDetails(details);

        if (account->isUsable())
            account->doRegister();
        else
            account->doUnregister();

        // Update account details to the client side
        emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(accountID, details);
    });
}

std::string
Manager::getNewAccountId()
{
    std::string random_id;
    do {
        random_id = to_hex_string(std::uniform_int_distribution<uint64_t>()(rand_));
    } while (getAccount(random_id));
    return random_id;
}

std::string
Manager::addAccount(const std::map<std::string, std::string>& details, const std::string& accountId)
{
    /** @todo Deal with both the accountMap_ and the Configuration */
    auto newAccountID = accountId.empty() ? getNewAccountId() : accountId;

    // Get the type
    std::string_view accountType;
    auto typeIt = details.find(Conf::CONFIG_ACCOUNT_TYPE);
    if (typeIt != details.end())
        accountType = typeIt->second;
    else
        accountType = AccountFactory::DEFAULT_ACCOUNT_TYPE;

    JAMI_DEBUG("Adding account {:s} with type {}", newAccountID, accountType);

    auto newAccount = accountFactory.createAccount(accountType, newAccountID);
    if (!newAccount) {
        JAMI_ERROR("Unknown {:s} param when calling addAccount(): {:s}",
                   Conf::CONFIG_ACCOUNT_TYPE,
                   accountType);
        return "";
    }

    newAccount->setAccountDetails(details);
    saveConfig(newAccount);
    newAccount->doRegister();

    preferences.addAccount(newAccountID);
    saveConfig();

    emitSignal<libjami::ConfigurationSignal::AccountsChanged>();

    return newAccountID;
}

void
Manager::removeAccount(const std::string& accountID, bool flush)
{
    // Get it down and dying
    if (const auto& remAccount = getAccount(accountID)) {
        // Force stopping connection before doUnregister as it will
        // wait for dht threads to finish
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(remAccount)) {
            acc->hangupCalls();
            acc->shutdownConnections();
        }
        remAccount->doUnregister();
        if (flush)
            remAccount->flush();
        accountFactory.removeAccount(*remAccount);
    }

    preferences.removeAccount(accountID);

    saveConfig();

    emitSignal<libjami::ConfigurationSignal::AccountsChanged>();
}

void
Manager::removeAccounts()
{
    for (const auto& acc : getAccountList())
        removeAccount(acc);
}

std::vector<std::string_view>
Manager::loadAccountOrder() const
{
    return split_string(preferences.getAccountOrder(), '/');
}

int
Manager::loadAccountMap(const YAML::Node& node)
{
    int errorCount = 0;
    try {
        // build preferences
        preferences.unserialize(node);
        voipPreferences.unserialize(node);
        audioPreference.unserialize(node);
#ifdef ENABLE_VIDEO
        videoPreferences.unserialize(node);
#endif
#ifdef ENABLE_PLUGIN
        pluginPreferences.unserialize(node);
#endif
    } catch (const YAML::Exception& e) {
        JAMI_ERR("Preferences node unserialize YAML exception: %s", e.what());
        ++errorCount;
    } catch (const std::exception& e) {
        JAMI_ERR("Preferences node unserialize standard exception: %s", e.what());
        ++errorCount;
    } catch (...) {
        JAMI_ERR("Preferences node unserialize unknown exception");
        ++errorCount;
    }

    const std::string accountOrder = preferences.getAccountOrder();

    // load saved preferences for IP2IP account from configuration file
    const auto& accountList = node["accounts"];

    for (auto& a : accountList) {
        pimpl_->loadAccount(a, errorCount);
    }

    auto accountBaseDir = fileutils::get_data_dir();
    auto dirs = dhtnet::fileutils::readDirectory(accountBaseDir);

    std::condition_variable cv;
    std::mutex lock;
    size_t remaining {0};
    std::unique_lock<std::mutex> l(lock);
    for (const auto& dir : dirs) {
        if (accountFactory.hasAccount<JamiAccount>(dir)) {
            continue;
        }
        remaining++;
        dht::ThreadPool::computation().run(
            [this, dir, &cv, &remaining, &lock, configFile = accountBaseDir / dir / "config.yml"] {
                if (std::filesystem::is_regular_file(configFile)) {
                    try {
                        auto configNode = YAML::LoadFile(configFile.string());
                        if (auto a = accountFactory.createAccount(JamiAccount::ACCOUNT_TYPE, dir)) {
                            auto config = a->buildConfig();
                            config->unserialize(configNode);
                            a->setConfig(std::move(config));
                        }
                    } catch (const std::exception& e) {
                        JAMI_ERR("Can't import account %s: %s", dir.c_str(), e.what());
                    }
                }
                std::lock_guard<std::mutex> l(lock);
                remaining--;
                cv.notify_one();
            });
    }
    cv.wait(l, [&remaining] { return remaining == 0; });

#ifdef ENABLE_PLUGIN
    if (pluginPreferences.getPluginsEnabled()) {
        std::vector<std::string> loadedPlugins = pluginPreferences.getLoadedPlugins();
        for (const std::string& plugin : loadedPlugins) {
            jami::Manager::instance().getJamiPluginManager().loadPlugin(plugin);
        }
    }
#endif

    return errorCount;
}

std::vector<std::string>
Manager::getCallList() const
{
    std::vector<std::string> results;
    for (const auto& call : callFactory.getAllCalls()) {
        if (!call->isSubcall())
            results.push_back(call->getCallId());
    }
    return results;
}

void
Manager::registerAccounts()
{
    auto allAccounts(getAccountList());

    for (auto& item : allAccounts) {
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
    saveConfig(acc);

    if (acc->isEnabled()) {
        acc->doRegister();
    } else
        acc->doUnregister();
}

bool
Manager::isPasswordValid(const std::string& accountID, const std::string& password)
{
    const auto acc = getAccount<JamiAccount>(accountID);
    if (!acc)
        return false;
    return acc->isPasswordValid(password);
}

uint64_t
Manager::sendTextMessage(const std::string& accountID,
                         const std::string& to,
                         const std::map<std::string, std::string>& payloads,
                         bool fromPlugin,
                         bool onlyConnected)
{
    if (const auto acc = getAccount(accountID)) {
        try {
#ifdef ENABLE_PLUGIN // modifies send message
            auto& pluginChatManager = getJamiPluginManager().getChatServicesManager();
            if (pluginChatManager.hasHandlers()) {
                auto cm = std::make_shared<JamiMessage>(accountID, to, false, payloads, fromPlugin);
                pluginChatManager.publishMessage(cm);
                return acc->sendTextMessage(cm->peerId, "", cm->data, 0, onlyConnected);
            } else
#endif // ENABLE_PLUGIN
                return acc->sendTextMessage(to, "", payloads, 0, onlyConnected);
        } catch (const std::exception& e) {
            JAMI_ERR("Exception during text message sending: %s", e.what());
        }
    }
    return 0;
}

int
statusFromImStatus(im::MessageStatus status)
{
    switch (status) {
    case im::MessageStatus::IDLE:
    case im::MessageStatus::SENDING:
        return static_cast<int>(libjami::Account::MessageStates::SENDING);
    case im::MessageStatus::SENT:
        return static_cast<int>(libjami::Account::MessageStates::SENT);
    case im::MessageStatus::DISPLAYED:
        return static_cast<int>(libjami::Account::MessageStates::DISPLAYED);
    case im::MessageStatus::FAILURE:
        return static_cast<int>(libjami::Account::MessageStates::FAILURE);
    default:
        return static_cast<int>(libjami::Account::MessageStates::UNKNOWN);
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
    return static_cast<int>(libjami::Account::MessageStates::UNKNOWN);
}

int
Manager::getMessageStatus(const std::string& accountID, uint64_t id) const
{
    if (const auto acc = getAccount(accountID))
        return statusFromImStatus(acc->getMessageStatus(id));
    return static_cast<int>(libjami::Account::MessageStates::UNKNOWN);
}

void
Manager::setAccountActive(const std::string& accountID, bool active, bool shutdownConnections)
{
    const auto acc = getAccount(accountID);
    if (!acc || acc->isActive() == active)
        return;
    acc->setActive(active);
    if (acc->isEnabled()) {
        if (active) {
            acc->doRegister();
        } else {
            acc->doUnregister();
            if (shutdownConnections) {
                if (auto jamiAcc = std::dynamic_pointer_cast<JamiAccount>(acc)) {
                    jamiAcc->shutdownConnections();
                }
            }
        }
    }
    emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        accountID, acc->getVolatileAccountDetails());
}

std::shared_ptr<AudioLayer>
Manager::getAudioDriver()
{
    return pimpl_->audiodriver_;
}

std::shared_ptr<Call>
Manager::newOutgoingCall(std::string_view toUrl,
                         const std::string& accountId,
                         const std::vector<libjami::MediaMap>& mediaList)
{
    auto account = getAccount(accountId);
    if (not account) {
        JAMI_WARN("No account matches ID %s", accountId.c_str());
        return {};
    }

    if (not account->isUsable()) {
        JAMI_WARN("Account %s is not usable", accountId.c_str());
        return {};
    }

    return account->newOutgoingCall(toUrl, mediaList);
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

void
Manager::createSinkClients(
    const std::string& callId,
    const ConfInfo& infos,
    const std::vector<std::shared_ptr<video::VideoFrameActiveWriter>>& videoStreams,
    std::map<std::string, std::shared_ptr<video::SinkClient>>& sinksMap,
    const std::string& accountId)
{
    std::lock_guard<std::mutex> lk(pimpl_->sinksMutex_);
    std::set<std::string> sinkIdsList {};

    // create video sinks
    for (const auto& [key, callInfo] : infos.callInfo_) {
        auto& uri = key.first; auto& device = key.second;
        for (const auto& [streamId, streamInfo] : callInfo.streams) {
            auto sinkId = streamId;
            if (sinkId.empty()) {
                sinkId = callId;
                sinkId += uri + device;
            }
            if (streamInfo.w && streamInfo.h && !streamInfo.videoMuted) {
                auto currentSink = getSinkClient(sinkId);
                if (!accountId.empty() &&
                    currentSink &&
                    uri == getAccount(accountId)->getUsername() &&
                    device == getAccount<JamiAccount>(accountId)->currentDeviceId()) {
                    // This is a local sink that must already exist
                    continue;
                }

                if (currentSink) {
                    // If sink exists, update it
                    currentSink->setCrop(streamInfo.x, streamInfo.y, streamInfo.w, streamInfo.h);
                    sinkIdsList.emplace(sinkId);
                    continue;
                }
                auto newSink = createSinkClient(sinkId);
                newSink->start();
                newSink->setCrop(streamInfo.x, streamInfo.y, streamInfo.w, streamInfo.h);
                newSink->setFrameSize(streamInfo.w, streamInfo.h);

                for (auto& videoStream : videoStreams)
                    videoStream->attach(newSink.get());

                sinksMap.emplace(sinkId, newSink);
                sinkIdsList.emplace(sinkId);
            } else {
                sinkIdsList.erase(sinkId);
            }
        }
    }

    // remove any non used video sink
    for (auto it = sinksMap.begin(); it != sinksMap.end();) {
        if (sinkIdsList.find(it->first) == sinkIdsList.end()) {
            for (auto& videoStream : videoStreams)
                videoStream->detach(it->second.get());
            it->second->stop();
            it = sinksMap.erase(it);
        } else {
            it++;
        }
    }
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

const std::shared_ptr<dhtnet::IceTransportFactory>&
Manager::getIceTransportFactory()
{
    return pimpl_->ice_tf_;
}

VideoManager&
Manager::getVideoManager() const
{
    return *pimpl_->videoManager_;
}

std::vector<libjami::Message>
Manager::getLastMessages(const std::string& accountID, const uint64_t& base_timestamp)
{
    if (const auto acc = getAccount(accountID))
        return acc->getLastMessages(base_timestamp);
    return {};
}

SIPVoIPLink&
Manager::sipVoIPLink() const
{
    return *pimpl_->sipLink_;
}

#ifdef ENABLE_PLUGIN
JamiPluginManager&
Manager::getJamiPluginManager() const
{
    return pimpl_->jami_plugin_manager;
}
#endif

std::shared_ptr<dhtnet::ChannelSocket>
Manager::gitSocket(std::string_view accountId,
                   std::string_view deviceId,
                   std::string_view conversationId)
{
    if (const auto acc = getAccount<JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->gitSocket(deviceId, conversationId);
    return nullptr;
}

std::map<std::string, std::string>
Manager::getNearbyPeers(const std::string& accountID)
{
    if (const auto acc = getAccount<JamiAccount>(accountID))
        return acc->getNearbyPeers();
    return {};
}

void
Manager::setDefaultModerator(const std::string& accountID, const std::string& peerURI, bool state)
{
    auto acc = getAccount(accountID);
    if (!acc) {
        JAMI_ERR("Fail to change default moderator, account %s not found", accountID.c_str());
        return;
    }

    if (state)
        acc->addDefaultModerator(peerURI);
    else
        acc->removeDefaultModerator(peerURI);
    saveConfig(acc);
}

std::vector<std::string>
Manager::getDefaultModerators(const std::string& accountID)
{
    auto acc = getAccount(accountID);
    if (!acc) {
        JAMI_ERR("Fail to get default moderators, account %s not found", accountID.c_str());
        return {};
    }

    auto set = acc->getDefaultModerators();
    return std::vector<std::string>(set.begin(), set.end());
}

void
Manager::enableLocalModerators(const std::string& accountID, bool isModEnabled)
{
    if (auto acc = getAccount(accountID))
        acc->editConfig(
            [&](AccountConfig& config) { config.localModeratorsEnabled = isModEnabled; });
}

bool
Manager::isLocalModeratorsEnabled(const std::string& accountID)
{
    auto acc = getAccount(accountID);
    if (!acc) {
        JAMI_ERR("Fail to get local moderators, account %s not found", accountID.c_str());
        return true; // Default value
    }
    return acc->isLocalModeratorsEnabled();
}

void
Manager::setAllModerators(const std::string& accountID, bool allModerators)
{
    if (auto acc = getAccount(accountID))
        acc->editConfig([&](AccountConfig& config) { config.allModeratorsEnabled = allModerators; });
}

bool
Manager::isAllModerators(const std::string& accountID)
{
    auto acc = getAccount(accountID);
    if (!acc) {
        JAMI_ERR("Fail to get all moderators, account %s not found", accountID.c_str());
        return true; // Default value
    }
    return acc->isAllModerators();
}

void
Manager::insertGitTransport(git_smart_subtransport* tr, std::unique_ptr<P2PSubTransport>&& sub)
{
    std::lock_guard<std::mutex> lk(pimpl_->gitTransportsMtx_);
    pimpl_->gitTransports_[tr] = std::move(sub);
}

void
Manager::eraseGitTransport(git_smart_subtransport* tr)
{
    std::lock_guard<std::mutex> lk(pimpl_->gitTransportsMtx_);
    pimpl_->gitTransports_.erase(tr);
}

dhtnet::tls::CertificateStore&
Manager::certStore(const std::string& accountId) const
{
    if (const auto& account = getAccount<JamiAccount>(accountId)) {
        return account->certStore();
    }
    throw std::runtime_error("No account found");
}

} // namespace jami
