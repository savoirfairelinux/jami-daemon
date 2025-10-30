/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"

#include "logger.h"
#include "account_schema.h"

#include "fileutils.h"
#include "gittransport.h"
#include "jami.h"
#include "media_attribute.h"
#include "account.h"
#include "string_utils.h"
#include "jamidht/jamiaccount.h"
#include "account.h"
#include <opendht/rng.h>

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

#include "client/jami_signal.h"
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

bool Manager::autoLoad = {true};

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
            if (file_iterator.is_directory() and std::filesystem::is_directory(new_path)) {
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
 * Set OpenDHT's log level based on the JAMI_LOG_DHT environment variable.
 * JAMI_LOG_DHT = 0 minimum logging (=disable)
 * JAMI_LOG_DHT = 1 logging enabled
 */
static unsigned
setDhtLogLevel()
{
    unsigned level = 0;
    if (auto envvar = getenv("JAMI_LOG_DHT")) {
        level = to_int<unsigned>(envvar, 0);
        level = std::clamp(level, 0u, 1u);
    }
    return level;
}

/**
 * Set pjsip's log level based on the JAMI_LOG_SIP environment variable.
 * JAMI_LOG_SIP = 0 minimum logging
 * JAMI_LOG_SIP = 6 maximum logging
 */
static void
setSipLogLevel()
{
    int level = 0;
    if (auto envvar = getenv("JAMI_LOG_SIP")) {
        level = to_int<int>(envvar, 0);
        level = std::clamp(level, 0, 6);
    }

    pj_log_set_level(level);
    pj_log_set_log_func([](int level, const char* data, int len) {
        auto msg = std::string_view(data, len);
        if (level < 2)
            JAMI_XERR("{}", msg);
        else if (level < 4)
            JAMI_XWARN("{}", msg);
        else
            JAMI_XDBG("{}", msg);
    });
}

/**
 * Set gnutls's log level based on the JAMI_LOG_TLS environment variable.
 * JAMI_LOG_TLS = 0 minimum logging (default)
 * JAMI_LOG_TLS = 9 maximum logging
 */
static void
setGnuTlsLogLevel()
{
    int level = 0;
    if (auto envvar = getenv("JAMI_LOG_TLS")) {
        level = to_int<int>(envvar, 0);
        level = std::clamp(level, 0, 9);
    }

    gnutls_global_set_log_level(level);
    gnutls_global_set_log_function([](int level, const char* msg) { JAMI_XDBG("[{:d}]GnuTLS: {:s}", level, msg); });
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
     * Process the remaining participants in a conference with the current call ID.
     * Called when participants have been disconnected or have ended the call.
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
    void cleanupAccountStorage(const std::string& accountId);

    void sendTextMessageToConference(const Conference& conf,
                                     const std::map<std::string, std::string>& messages,
                                     const std::string& from) const noexcept;

    void bindCallToConference(Call& call, Conference& conf);

    void addMainParticipant(Conference& conf);

    bool endConference(Conference& conf);

    template<class T>
    std::shared_ptr<T> findAccount(const std::function<bool(const std::shared_ptr<T>&)>&);

    void initAudioDriver();

    void processIncomingCall(const std::string& accountId, Call& incomCall);
    static void stripSipPrefix(Call& incomCall);

    Manager& base_; // pimpl back-pointer

    std::shared_ptr<asio::io_context> ioContext_;
    std::thread ioContextRunner_;

    std::shared_ptr<dhtnet::upnp::UPnPContext> upnpContext_;

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

    /* Audio device users */
    std::mutex audioDeviceUsersMutex_ {};
    std::map<std::string, unsigned> audioDeviceUsers_ {};

    // Main thread
    std::unique_ptr<DTMF> dtmfKey_;

    /** Buffer to generate DTMF */
    std::shared_ptr<AudioFrame> dtmfBuf_;

    std::shared_ptr<asio::steady_timer> dtmfTimer_;

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
     * Protect waiting call list, access by many VoIP/audio threads
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
    std::unique_ptr<JamiPluginManager> jami_plugin_manager;
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
    , videoManager_(nullptr)
#endif
{
    jami::libav_utils::av_init();
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
            JAMI_WARNING("[config] Error while parsing {}", path_);
            result = false;
        }
    } catch (const YAML::BadFile& e) {
        JAMI_WARNING("[config] Unable to open configuration file");
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

    std::lock_guard lock(audioLayerMutex_);
    if (not audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
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
    const std::string currentCallId(base_.getCurrentCallId());
    CallIdSet subcalls(conf.getSubCalls());
    const size_t n = subcalls.size();
    JAMI_DEBUG("[conf:{}] Processing {} remaining participant(s)", conf.getConfId(), conf.getConferenceInfos().size());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (const auto& p : subcalls) {
            if (auto call = base_.getCallFromCallID(p)) {
                auto medias = call->getAudioStreams();
                for (const auto& media : medias) {
                    JAMI_DEBUG("[call:{}] Remove local audio {}", p, media.first);
                    base_.getRingBufferPool().flush(media.first);
                }
            }
        }

        base_.getRingBufferPool().flush(RingBufferPool::DEFAULT_ID);
    } else {
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(conf.getAccount())) {
            // Stay in a conference if 1 participants for swarm and rendezvous
            if (auto cm = acc->convModule(true)) {
                if (acc->isRendezVous() || cm->isHosting("", conf.getConfId())) {
                    // Check if attached
                    if (conf.getState() == Conference::State::ACTIVE_CONNECTED) {
                        return;
                    }
                }
            }
        }
        if (n == 1) {
            // this call is the last participant (non swarm-call), hence
            // the conference is over
            auto p = subcalls.begin();
            if (auto call = base_.getCallFromCallID(*p)) {
                // if we are not listening to this conference and not a rendez-vous
                auto w = call->getAccount();
                auto account = w.lock();
                if (!account) {
                    JAMI_ERROR("[conf:{}] Account no longer available", conf.getConfId());
                    return;
                }
                if (currentCallId != conf.getConfId())
                    base_.holdCall(account->getAccountID(), call->getCallId());
                else
                    switchCall(call->getCallId());
            }

            JAMI_DEBUG("[conf:{}] Only one participant left, removing conference", conf.getConfId());
            if (auto account = conf.getAccount())
                account->removeConference(conf.getConfId());
        } else {
            JAMI_DEBUG("[conf:{}] No remaining participants, removing conference", conf.getConfId());
            if (auto account = conf.getAccount())
                account->removeConference(conf.getConfId());
            unsetCurrentCall();
        }
    }
}

/**
 * Initialization: Main Thread
 */
std::filesystem::path
Manager::ManagerPimpl::retrieveConfigPath() const
{
    // TODO: Migrate config filename from dring.yml to jami.yml.
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
    std::lock_guard m(currentCallMutex_);
    JAMI_DBG("----- Switch current call ID to '%s' -----", not id.empty() ? id.c_str() : "none");
    currentCall_ = id;
}

void
Manager::ManagerPimpl::addWaitingCall(const std::string& id)
{
    std::lock_guard m(waitingCallsMutex_);
    // Enable incoming call beep if needed.
    if (audiodriver_ and waitingCalls_.empty() and not currentCall_.empty())
        audiodriver_->playIncomingCallNotification(true);
    waitingCalls_.insert(id);
}

void
Manager::ManagerPimpl::removeWaitingCall(const std::string& id)
{
    std::lock_guard m(waitingCallsMutex_);
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

    if (accountid.empty())
        return;

    if (base_.preferences.isAccountPending(accountid)) {
        JAMI_INFO("[account:%s] Removing pending account from disk", accountid.c_str());
        base_.removeAccount(accountid, true);
        cleanupAccountStorage(accountid);
        return;
    }

    if (auto a = base_.accountFactory.createAccount(accountType, accountid)) {
        auto config = a->buildConfig();
        config->unserialize(node);
        a->setConfig(std::move(config));
        return;
    }

    JAMI_ERROR("Failed to create account of type \"{:s}\"", accountType);
    ++errorCount;
}

void
Manager::ManagerPimpl::cleanupAccountStorage(const std::string& accountId)
{
    const auto cachePath = fileutils::get_cache_dir() / accountId;
    const auto dataPath = cachePath / "values";
    const auto idPath = fileutils::get_data_dir() / accountId;
    dhtnet::fileutils::removeAll(dataPath);
    dhtnet::fileutils::removeAll(cachePath);
    dhtnet::fileutils::removeAll(idPath, true);
}

// THREAD=VoIP
void
Manager::ManagerPimpl::sendTextMessageToConference(const Conference& conf,
                                                   const std::map<std::string, std::string>& messages,
                                                   const std::string& from) const noexcept
{
    CallIdSet subcalls(conf.getSubCalls());
    for (const auto& callId : subcalls) {
        try {
            auto call = base_.getCallFromCallID(callId);
            if (not call)
                throw std::runtime_error("No associated call");
            call->sendTextMessage(messages, from);
        } catch (const std::exception& e) {
            JAMI_ERROR("[conf:{}] Failed to send message to participant {}: {}", conf.getConfId(), callId, e.what());
        }
    }
}

void
Manager::bindCallToConference(Call& call, Conference& conf)
{
    pimpl_->bindCallToConference(call, conf);
}

void
Manager::ManagerPimpl::bindCallToConference(Call& call, Conference& conf)
{
    const auto& callId = call.getCallId();
    const auto& confId = conf.getConfId();
    const auto& state = call.getStateStr();

    // ensure that calls are only in one conference at a time
    if (call.isConferenceParticipant())
        base_.disconnectParticipant(callId);

    JAMI_DEBUG("[call:{}] Bind to conference {} (callState={})", callId, confId, state);

    auto medias = call.getAudioStreams();
    for (const auto& media : medias) {
        JAMI_DEBUG("[call:{}] Remove local audio {}", callId, media.first);
        base_.getRingBufferPool().unBindAll(media.first);
    }

    conf.addSubCall(callId);

    if (state == "HOLD") {
        base_.resumeCall(call.getAccountId(), callId);
    } else if (state == "INCOMING") {
        base_.acceptCall(call);
    } else if (state == "CURRENT") {
    } else if (state == "INACTIVE") {
        base_.acceptCall(call);
    } else
        JAMI_WARNING("[call:{}] Call state {} unrecognized for conference", callId, state);
}

//==============================================================================

Manager&
Manager::instance()
{
    // Meyers singleton
    static Manager instance;

    // This will give a warning that can be ignored the first time instance()
    // is called… subsequent warnings are more serious
    if (not Manager::initialized)
        JAMI_WARNING("Manager accessed before initialization");

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
        JAMI_ERROR("Unable to initialize git transport: {}", error ? error->message : "(unknown)");
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
    dhtLogLevel = setDhtLogLevel();
    pimpl_->upnpContext_->setMappingLabel("JAMI-" + fileutils::getOrCreateLocalDeviceId());

    JAMI_LOG("Using PJSIP version: {:s} for {:s}", pj_get_version(), PJ_OS_NAME);
    JAMI_LOG("Using GnuTLS version: {:s}", gnutls_check_version(nullptr));
    JAMI_LOG("Using OpenDHT version: {:s}", dht::version());
    JAMI_LOG("Using FFmpeg version: {:s}", av_version_info());
    int git2_major = 0, git2_minor = 0, git2_rev = 0;
    if (git_libgit2_version(&git2_major, &git2_minor, &git2_rev) == 0) {
        JAMI_LOG("Using libgit2 version: {:d}.{:d}.{:d}", git2_major, git2_minor, git2_rev);
    }

    // Manager can restart without being recreated (Unit tests)
    // So only create the SipLink once
    pimpl_->sipLink_ = std::make_unique<SIPVoIPLink>();

    check_rename(fileutils::get_cache_dir(PACKAGE_OLD), fileutils::get_cache_dir());
    check_rename(fileutils::get_data_dir(PACKAGE_OLD), fileutils::get_data_dir());
    check_rename(fileutils::get_config_dir(PACKAGE_OLD), fileutils::get_config_dir());

    pimpl_->ice_tf_ = std::make_shared<dhtnet::IceTransportFactory>(Logger::dhtLogger());

    pimpl_->path_ = config_file.empty() ? pimpl_->retrieveConfigPath() : config_file;
    JAMI_LOG("Configuration file path: {}", pimpl_->path_);

#ifdef ENABLE_PLUGIN
    pimpl_->jami_plugin_manager = std::make_unique<JamiPluginManager>();
#endif

    bool no_errors = true;

    // manager can restart without being recreated (Unit tests)
    pimpl_->finished_ = false;

    // Create video manager
    if (!(flags & libjami::LIBJAMI_FLAG_NO_LOCAL_VIDEO)) {
        pimpl_->videoManager_.reset(new VideoManager);
    }

    if (libjami::LIBJAMI_FLAG_NO_AUTOLOAD & flags) {
        autoLoad = false;
        JAMI_DEBUG("LIBJAMI_FLAG_NO_AUTOLOAD is set, accounts will neither be loaded nor backed up");
    } else {
        try {
            no_errors = pimpl_->parseConfiguration();
        } catch (const YAML::Exception& e) {
            JAMI_ERROR("[config] Failed to parse configuration: {}", e.what());
            no_errors = false;
        }

        // always back up last error-free configuration
        if (no_errors) {
            make_backup(pimpl_->path_);
        } else {
            // restore previous configuration
            JAMI_WARNING("Restoring last working configuration");

            try {
                // remove accounts from broken configuration
                removeAccounts();
                restore_backup(pimpl_->path_);
                pimpl_->parseConfiguration();
            } catch (const YAML::Exception& e) {
                JAMI_ERROR("{}", e.what());
                JAMI_WARNING("Restoring backup failed");
            }
        }
    }

    if (!(flags & libjami::LIBJAMI_FLAG_NO_LOCAL_AUDIO)) {
        std::lock_guard lock(pimpl_->audioLayerMutex_);
        pimpl_->initAudioDriver();
        if (pimpl_->audiodriver_) {
            auto format = pimpl_->audiodriver_->getFormat();
            pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
            pimpl_->dtmfKey_.reset(new DTMF(getRingBufferPool().getInternalSamplingRate(),
                                            getRingBufferPool().getInternalAudioFormat().sampleFormat));
        }
    }

    // Start ASIO event loop
    pimpl_->ioContextRunner_ = std::thread([context = pimpl_->ioContext_]() {
        try {
            auto work = asio::make_work_guard(*context);
            context->run();
        } catch (const std::exception& ex) {
            JAMI_ERROR("[io] Unexpected io_context thread exception: {}", ex.what());
        }
    });

    if (libjami::LIBJAMI_FLAG_NO_AUTOLOAD & flags) {
        JAMI_DEBUG("LIBJAMI_FLAG_NO_AUTOLOAD is set, accounts and conversations will not be loaded");
        return;
    } else {
        registerAccounts();
    }
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

        // End all remaining active calls
        JAMI_DBG("End %zu remaining call(s)", callFactory.callCount());
        for (const auto& call : callFactory.getAllCalls())
            endCall(call->getAccountId(), call->getCallId());
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
            std::lock_guard lock(pimpl_->audioLayerMutex_);
            pimpl_->audiodriver_.reset();
        }

        JAMI_DEBUG("Stopping schedulers and worker threads");

        // Flush remaining tasks (free lambda' with capture)
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
            pimpl_->ioContext_->stop(); // make thread stop
        }
        if (pimpl_->ioContextRunner_.joinable())
            pimpl_->ioContextRunner_.join();

#if defined _MSC_VER
        gnutls_global_deinit();
#endif

    } catch (const VoipLinkException& err) {
        JAMI_ERROR("[voip] {}", err.what());
    }
}

void
Manager::monitor(bool continuous)
{
    Logger::setMonitorLog(true);
    JAMI_DEBUG("############## START MONITORING ##############");
    JAMI_DEBUG("Using PJSIP version: {} for {}", pj_get_version(), PJ_OS_NAME);
    JAMI_DEBUG("Using GnuTLS version: {}", gnutls_check_version(nullptr));
    JAMI_DEBUG("Using OpenDHT version: {}", dht::version());

#ifdef __linux__
#if defined(__ANDROID__)
#else
    auto opened_files = dhtnet::fileutils::readDirectory("/proc/" + std::to_string(getpid()) + "/fd").size();
    JAMI_DEBUG("Opened files: {}", opened_files);
#endif
#endif

    for (const auto& call : callFactory.getAllCalls())
        call->monitor();
    for (const auto& account : getAllAccounts())
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account))
            acc->monitor();
    JAMI_DEBUG("############## END MONITORING ##############");
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
            account->doUnregister(true);
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
    JAMI_DBG() << "Attempt outgoing call to '" << to << "'" << " with account '" << account_id << "'";

    std::shared_ptr<Call> call;

    try {
        call = newOutgoingCall(trim(to), account_id, mediaList);
    } catch (const std::exception& e) {
        JAMI_ERROR("{}", e.what());
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
Manager::acceptCall(const std::string& accountId,
                    const std::string& callId,
                    const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return acceptCall(*call, mediaList);
        }
    }
    return false;
}

bool
Manager::acceptCall(Call& call, const std::vector<libjami::MediaMap>& mediaList)
{
    JAMI_LOG("Answer call {}", call.getCallId());

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
        JAMI_ERROR("[call:{}] Failed to answer: {}", call.getCallId(), e.what());
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
Manager::endCall(const std::string& accountId, const std::string& callId)
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
        JAMI_WARN("Unable to end the nonexistent call %s", callId.c_str());
        return false;
    }

    // Disconnect streams
    removeAudio(*call);

    if (call->isConferenceParticipant()) {
        removeParticipant(*call);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (isCurrentCall(*call))
            pimpl_->unsetCurrentCall();
    }

    try {
        call->end(0);
    } catch (const VoipLinkException& e) {
        JAMI_ERROR("[call:{}] Failed to end call: {}", call->getCallId(), e.what());
        return false;
    }

    return true;
}

bool
Manager::endConference(const std::string& accountId, const std::string& confId)
{
    if (auto account = getAccount(accountId)) {
        if (auto conference = account->getConference(confId)) {
            return pimpl_->endConference(*conference);
        } else {
            JAMI_ERROR("[conf:{}] Conference not found", confId);
        }
    }
    return false;
}

// THREAD=Main
bool
Manager::holdCall(const std::string&, const std::string& callId)
{
    bool result = true;

    stopTone();

    std::string current_callId(getCurrentCallId());

    if (auto call = getCallFromCallID(callId)) {
        try {
            result = call->hold([=](bool ok) {
                if (!ok) {
                    JAMI_ERROR("CallID {} holdCall failed", callId);
                    return;
                }
                removeAudio(*call); // Unbind calls in main buffer
                // Remove call from the queue if it was still there
                pimpl_->removeWaitingCall(callId);

                // Keeps current call ID if the action does not hold this call
                // or a new outgoing call. This could happen in case of a conference
                if (current_callId == callId)
                    pimpl_->unsetCurrentCall();
            });
        } catch (const VoipLinkException& e) {
            JAMI_ERROR("[call:{}] Failed to hold: {}", callId, e.what());
            result = false;
        }
    } else {
        JAMI_LOG("CallID {} doesn't exist in call holdCall", callId);
        return false;
    }

    return result;
}

// THREAD=Main
bool
Manager::resumeCall(const std::string&, const std::string& callId)
{
    bool result = true;

    stopTone();

    std::shared_ptr<Call> call = getCallFromCallID(callId);
    if (!call)
        return false;

    try {
        result = call->resume([=](bool ok) {
            if (!ok) {
                JAMI_ERROR("CallID {} resumeCall failed", callId);
                return;
            }

            if (auto conf = call->getConference())
                pimpl_->switchCall(conf->getConfId());
            else
                pimpl_->switchCall(call->getCallId());

            addAudio(*call);
        });
    } catch (const VoipLinkException& e) {
        JAMI_ERROR("[call] Failed to resume: {}", e.what());
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
            removeParticipant(*call);
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
Manager::declineCall(const std::string& accountId, const std::string& id)
{
    if (auto account = getAccount(accountId)) {
        if (auto call = account->getCall(id)) {
            stopTone();
            call->decline();
            pimpl_->removeWaitingCall(id);
            removeAudio(*call);
            return true;
        }
    }
    return false;
}

bool
Manager::holdConference(const std::string& accountId, const std::string& confId)
{
    JAMI_LOG("[conf:{}] Hold conference", confId);

    if (const auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->disconnectHost();
            emitSignal<libjami::CallSignal::ConferenceChanged>(accountId, conf->getConfId(), conf->getStateStr());
            return true;
        }
    }
    return false;
}

bool
Manager::resumeConference(const std::string& accountId, const std::string& confId)
{
    JAMI_DEBUG("[conf:{}] Resume conference", confId);

    if (const auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            // Resume conf only if it was in hold state otherwise…
            // all participants are restarted
            if (conf->getState() == Conference::State::HOLD) {
                for (const auto& item : conf->getSubCalls())
                    resumeCall(accountId, item);

                pimpl_->switchCall(confId);
                conf->setState(Conference::State::ACTIVE_CONNECTED);
                emitSignal<libjami::CallSignal::ConferenceChanged>(accountId, conf->getConfId(), conf->getStateStr());
                return true;
            } else if (conf->getState() == Conference::State::ACTIVE_DISCONNECTED) {
                pimpl_->addMainParticipant(*conf);
            }
        }
    }
    return false;
}

bool
Manager::addSubCall(const std::string& accountId,
                    const std::string& callId,
                    const std::string& account2Id,
                    const std::string& conferenceId)
{
    auto account = getAccount(accountId);
    auto account2 = getAccount(account2Id);
    if (account && account2) {
        auto call = account->getCall(callId);
        auto conf = account2->getConference(conferenceId);
        if (!call or !conf)
            return false;
        auto callConf = call->getConference();
        if (callConf != conf)
            return addSubCall(*call, *conf);
    }
    return false;
}

bool
Manager::addSubCall(Call& call, Conference& conference)
{
    JAMI_DEBUG("[conf:{}] Adding participant {}", conference.getConfId(), call.getCallId());

    // Store the current call ID (it will change in resumeCall or in acceptCall)
    pimpl_->bindCallToConference(call, conference);

    // Don't attach current user yet
    if (conference.getState() == Conference::State::ACTIVE_DISCONNECTED) {
        return true;
    }

    // TODO: remove this ugly hack → There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // to conference
    pimpl_->unsetCurrentCall();
    pimpl_->addMainParticipant(conference);
    pimpl_->switchCall(conference.getConfId());
    addAudio(call);

    return true;
}

void
Manager::ManagerPimpl::addMainParticipant(Conference& conf)
{
    JAMI_DEBUG("[conf:{}] Adding main participant", conf.getConfId());
    conf.connectHost(conf.getLastMediaList());
    emitSignal<libjami::CallSignal::ConferenceChanged>(conf.getAccountId(), conf.getConfId(), conf.getStateStr());
    switchCall(conf.getConfId());
}

bool
Manager::ManagerPimpl::endConference(Conference& conference)
{
    JAMI_DEBUG("[conf:{}] Ending conference", conference.getConfId());
    CallIdSet subcalls(conference.getSubCalls());
    conference.disconnectHost();
    if (subcalls.empty()) {
        if (auto account = conference.getAccount())
            account->removeConference(conference.getConfId());
    }
    for (const auto& callId : subcalls) {
        if (auto call = base_.getCallFromCallID(callId))
            base_.endCall(call->getAccountId(), callId);
    }
    unsetCurrentCall();
    return true;
}

bool
Manager::addMainParticipant(const std::string& accountId, const std::string& conferenceId)
{
    JAMI_LOG("[conf:{}] Adding main participant", conferenceId);

    if (auto account = getAccount(accountId)) {
        if (auto conf = account->getConference(conferenceId)) {
            pimpl_->addMainParticipant(*conf);
            return true;
        } else
            JAMI_WARNING("[conf:{}] Failed to add main participant (conference not found)", conferenceId);
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
    JAMI_DEBUG("Joining participants {} and {}, attached={}", callId1, callId2, attached);
    auto account = getAccount(accountId);
    auto account2 = getAccount(account2Id);
    if (not account or not account2) {
        return false;
    }

    JAMI_LOG("Creating conference for participants {} and {}, host attached: {}", callId1, callId2, attached);

    if (callId1 == callId2) {
        JAMI_ERROR("Unable to join participant {} to itself", callId1);
        return false;
    }

    // Set corresponding conference ids for call 1
    auto call1 = account->getCall(callId1);
    if (!call1) {
        JAMI_ERROR("Unable to find call {}", callId1);
        return false;
    }

    // Set corresponding conference details
    auto call2 = account2->getCall(callId2);
    if (!call2) {
        JAMI_ERROR("Unable to find call {}", callId2);
        return false;
    }

    auto mediaAttr = call1->getMediaAttributeList();
    if (mediaAttr.empty()) {
        JAMI_WARNING("[call:{}] No media attribute found, using media attribute from call [{}]", callId1, callId2);
        mediaAttr = call2->getMediaAttributeList();
    }

    // Filter out secondary audio streams that are muted: these are SDP
    // negotiation artifacts (the host answered a participant's extra audio
    // offer with a muted slot) and do not represent real host audio sources.
    {
        bool audioFound = false;
        mediaAttr.erase(std::remove_if(mediaAttr.begin(),
                                       mediaAttr.end(),
                                       [&audioFound](const MediaAttribute& attr) {
                                           if (attr.type_ == MediaType::MEDIA_AUDIO) {
                                               if (audioFound && attr.muted_)
                                                   return true; // remove secondary audio streams
                                               audioFound = true;
                                           }
                                           return false;
                                       }),
                        mediaAttr.end());
    }

    JAMI_DEBUG("[call:{}] Media attributes for conference:", callId1);
    for (const auto& media : mediaAttr) {
        JAMI_DEBUG("- {}", media.toString(true));
    }

    auto conf = std::make_shared<Conference>(account);
    conf->connectHost(MediaAttribute::mediaAttributesToMediaMaps(mediaAttr));
    account->attach(conf);
    emitSignal<libjami::CallSignal::ConferenceCreated>(account->getAccountID(), "", conf->getConfId());

    // Bind calls according to their state
    pimpl_->bindCallToConference(*call1, *conf);
    pimpl_->bindCallToConference(*call2, *conf);

    // Switch current call id to this conference
    if (attached) {
        pimpl_->switchCall(conf->getConfId());
        conf->setState(Conference::State::ACTIVE_CONNECTED);
    } else {
        conf->disconnectHost();
    }
    emitSignal<libjami::CallSignal::ConferenceChanged>(account->getAccountID(), conf->getConfId(), conf->getStateStr());

    return true;
}

void
Manager::createConfFromParticipantList(const std::string& accountId, const std::vector<std::string>& participantList)
{
    auto account = getAccount(accountId);
    if (not account) {
        JAMI_WARNING("[account:{}] Account not found", accountId);
        return;
    }

    // we must have at least 2 participant for a conference
    if (participantList.size() <= 1) {
        JAMI_ERROR("[conf] Participant number must be greater than or equal to 2");
        return;
    }

    auto conf = std::make_shared<Conference>(account);
    // attach host with empty medialist
    // which will result in a default list set by initSourcesForHost
    conf->connectHost({});

    unsigned successCounter = 0;
    for (const auto& numberaccount : participantList) {
        std::string tostr(numberaccount.substr(0, numberaccount.find(',')));
        std::string account(numberaccount.substr(numberaccount.find(',') + 1, numberaccount.size()));

        pimpl_->unsetCurrentCall();

        // Create call
        auto callId = outgoingCall(account, tostr, {});
        if (callId.empty())
            continue;

        // Manager methods may behave differently if the call id participates in a conference
        conf->addSubCall(callId);
        successCounter++;
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        account->attach(conf);
        emitSignal<libjami::CallSignal::ConferenceCreated>(accountId, "", conf->getConfId());
    }
}

bool
Manager::disconnectHost(const std::shared_ptr<Conference>& conf)
{
    if (not conf)
        return false;

    JAMI_LOG("[conf:{}] Disconnecting host", conf->getConfId());
    conf->disconnectHost();
    emitSignal<libjami::CallSignal::ConferenceChanged>(conf->getAccountId(), conf->getConfId(), conf->getStateStr());
    pimpl_->unsetCurrentCall();
    return true;
}

bool
Manager::disconnectParticipant(const std::string& callId)
{
    JAMI_DEBUG("Disconnecting participant {}", callId);

    auto call = getCallFromCallID(callId);
    if (!call) {
        JAMI_ERROR("Unable to find call {}", callId);
        return false;
    }

    // Don't hold ringing calls when disconnecting them from conferences
    if (call->getStateStr() != "RINGING")
        holdCall(call->getAccountId(), callId);

    removeParticipant(*call);
    return true;
}

void
Manager::removeParticipant(Call& call)
{
    JAMI_DEBUG("Removing participant {}", call.getCallId());

    auto conf = call.getConference();
    if (not conf) {
        JAMI_ERROR("[call:{}] No conference associated, unable to remove participant", call.getCallId());
        return;
    }

    conf->removeSubCall(call.getCallId());

    removeAudio(call);

    emitSignal<libjami::CallSignal::ConferenceChanged>(conf->getAccountId(), conf->getConfId(), conf->getStateStr());

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
        JAMI_ERROR("Unable to find account: {}", accountId);
        return false;
    }
    if (not account2) {
        JAMI_ERROR("Unable to find account: {}", account2Id);
        return false;
    }

    auto conf = account->getConference(confId1);
    if (not conf) {
        JAMI_ERROR("[conf:{}] Invalid conference ID", confId1);
        return false;
    }

    auto conf2 = account2->getConference(confId2);
    if (not conf2) {
        JAMI_ERROR("[conf:{}] Invalid conference ID", confId2);
        return false;
    }

    CallIdSet subcalls(conf->getSubCalls());

    std::vector<std::shared_ptr<Call>> calls;
    calls.reserve(subcalls.size());

    // Disconnect and remove all participants from conf1 before adding
    // ... to conf2
    for (const auto& callId : subcalls) {
        JAMI_DEBUG("Disconnect participant {}", callId);
        if (auto call = account->getCall(callId)) {
            conf->removeSubCall(callId);
            removeAudio(*call);
            calls.emplace_back(std::move(call));
        } else {
            JAMI_ERROR("Unable to find call {}", callId);
        }
    }
    // Remove conf1
    account->removeConference(confId1);

    for (const auto& c : calls)
        addSubCall(*c, *conf2);

    return true;
}

void
Manager::addAudio(Call& call)
{
    if (call.isConferenceParticipant())
        return;
    const auto& callId = call.getCallId();
    JAMI_LOG("Add audio to call {}", callId);

    // bind to main
    auto medias = call.getAudioStreams();
    for (const auto& media : medias) {
        JAMI_DEBUG("[call:{}] Attach audio stream {}", callId, media.first);
        getRingBufferPool().bindRingBuffers(media.first, RingBufferPool::DEFAULT_ID);
    }
    auto oldGuard = std::move(call.audioGuard);
    call.audioGuard = startAudioStream(AudioDeviceType::PLAYBACK);

    std::lock_guard lock(pimpl_->audioLayerMutex_);
    if (!pimpl_->audiodriver_) {
        JAMI_ERROR("Uninitialized audio driver");
        return;
    }
    pimpl_->audiodriver_->flushUrgent();
    getRingBufferPool().flushAllBuffers();
}

void
Manager::removeAudio(Call& call)
{
    const auto& callId = call.getCallId();
    auto medias = call.getAudioStreams();
    for (const auto& media : medias) {
        JAMI_DEBUG("[call:{}] Remove local audio {}", callId, media.first);
        getRingBufferPool().unBindAll(media.first);
    }
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

void
Manager::saveConfig(const std::shared_ptr<Account>& acc)
{
    if (auto account = std::dynamic_pointer_cast<JamiAccount>(acc))
        account->saveConfig();
    else
        saveConfig();
}

void
Manager::saveConfig()
{
    JAMI_LOG("Saving configuration to '{}'", pimpl_->path_);

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

        std::lock_guard lock(dhtnet::fileutils::getFileLock(pimpl_->path_));
        std::ofstream fout(pimpl_->path_);
        fout.write(out.c_str(), out.size());
    } catch (const YAML::Exception& e) {
        JAMI_ERROR("[config] YAML error: {}", e.what());
    } catch (const std::runtime_error& e) {
        JAMI_ERROR("[config] {}", e.what());
    }
}

// THREAD=Main | VoIPLink
void
Manager::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        return;
    }

    std::lock_guard lock(pimpl_->audioLayerMutex_);

    // fast return, no sound, so no dtmf
    if (not pimpl_->audiodriver_ or not pimpl_->dtmfKey_) {
        return;
    }

    std::shared_ptr<AudioDeviceGuard> audioGuard = startAudioStream(AudioDeviceType::PLAYBACK);
    if (not pimpl_->audiodriver_->waitForStart(std::chrono::seconds(1))) {
        JAMI_ERROR("[audio] Failed to start audio layer for DTMF");
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
        // put the size in bytes…
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();

        pimpl_->audiodriver_->putUrgent(pimpl_->dtmfBuf_);
    }

    auto dtmfTimer = std::make_unique<asio::steady_timer>(*pimpl_->ioContext_, std::chrono::milliseconds(pulselen));
    dtmfTimer->async_wait([this, audioGuard, t = dtmfTimer.get()](const asio::error_code& ec) {
        if (ec)
            return;
        JAMI_DBG("End of dtmf");
        std::lock_guard lock(pimpl_->audioLayerMutex_);
        if (pimpl_->dtmfTimer_.get() == t)
            pimpl_->dtmfTimer_.reset();
    });
    if (pimpl_->dtmfTimer_)
        pimpl_->dtmfTimer_->cancel();
    pimpl_->dtmfTimer_ = std::move(dtmfTimer);
}

// Multi-thread
bool
Manager::incomingCallsWaiting()
{
    std::lock_guard m(pimpl_->waitingCallsMutex_);
    return not pimpl_->waitingCalls_.empty();
}

void
Manager::incomingCall(const std::string& accountId, Call& call)
{
    if (not accountId.empty()) {
        pimpl_->stripSipPrefix(call);
    }

    auto const& account = getAccount(accountId);
    if (not account) {
        JAMI_ERROR("Incoming call {} on unknown account {}", call.getCallId(), accountId);
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
                emitSignal<libjami::CallSignal::IncomingMessage>(accountId, conf->getConfId(), from, messages);
            } else {
                JAMI_ERROR("[call:{}] No conference associated to call", callId);
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
        pimpl_->sendTextMessageToConference(*conf, messages, from);
    } else if (auto call = account->getCall(callID)) {
        if (call->isConferenceParticipant()) {
            if (auto conf = call->getConference()) {
                pimpl_->sendTextMessageToConference(*conf, messages, from);
            } else {
                JAMI_ERROR("[call:{}] No conference associated to call", callID);
            }
        } else {
            try {
                call->sendTextMessage(messages, from);
            } catch (const im::InstantMessageException& e) {
                JAMI_ERR("Failed to send message to call %s: %s", call->getCallId().c_str(), e.what());
            }
        }
    } else {
        JAMI_ERR("Failed to send message to %s: nonexistent call ID", callID.c_str());
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
        std::lock_guard lock(pimpl_->audioLayerMutex_);
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
    JAMI_LOG("[call:{}] Peer ringing", call.getCallId());
    if (!hasCurrentCall())
        ringback();
}

// THREAD=VoIP Call=Outgoing/Ingoing
void
Manager::peerHungupCall(Call& call)
{
    const auto& callId = call.getCallId();
    JAMI_LOG("[call:{}] Peer hung up", callId);

    if (call.isConferenceParticipant()) {
        removeParticipant(call);
    } else if (isCurrentCall(call)) {
        stopTone();
        pimpl_->unsetCurrentCall();
    }

    call.peerHungup();

    pimpl_->removeWaitingCall(callId);
    if (not incomingCallsWaiting())
        stopTone();

    removeAudio(call);
}

// THREAD=VoIP
void
Manager::callBusy(Call& call)
{
    JAMI_LOG("[call:{}] Busy", call.getCallId());

    if (isCurrentCall(call)) {
        pimpl_->unsetCurrentCall();
    }

    pimpl_->removeWaitingCall(call.getCallId());
    if (not incomingCallsWaiting())
        stopTone();
}

// THREAD=VoIP
void
Manager::callFailure(Call& call)
{
    JAMI_LOG("[call:{}] {} failed", call.getCallId(), call.isSubcall() ? "Sub-call" : "Parent call");

    if (isCurrentCall(call)) {
        pimpl_->unsetCurrentCall();
    }

    if (call.isConferenceParticipant()) {
        JAMI_LOG("[call:{}] Participating in conference, removing participant", call.getCallId());
        // remove this participant
        removeParticipant(call);
    }

    pimpl_->removeWaitingCall(call.getCallId());
    if (not call.isSubcall() && not incomingCallsWaiting())
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
        JAMI_WARNING("[account:{}] Invalid account for ringtone", accountID);
        return;
    }

    if (!account->getRingtoneEnabled()) {
        ringback();
        return;
    }

    {
        std::lock_guard lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_) {
            JAMI_ERROR("[audio] No audio layer for ringtone");
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
        std::lock_guard lock(pimpl_->audioLayerMutex_);
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
    std::lock_guard lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio driver");
        return;
    }
    if (pimpl_->getCurrentDeviceIndex(type) == index) {
        JAMI_DEBUG("[audio] Audio device already selected, doing nothing");
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
    std::lock_guard lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
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
    std::lock_guard lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
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
    std::lock_guard lock(pimpl_->audioLayerMutex_);
    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
        return {};
    }

    return {std::to_string(pimpl_->audiodriver_->getIndexPlayback()),
            std::to_string(pimpl_->audiodriver_->getIndexCapture()),
            std::to_string(pimpl_->audiodriver_->getIndexRingtone())};
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

AudioDeviceGuard::AudioDeviceGuard(Manager& manager, const std::string& captureDevice)
    : manager_(manager)
    , type_(AudioDeviceType::CAPTURE)
    , captureDevice_(captureDevice)
{
    std::lock_guard lock(manager_.pimpl_->audioDeviceUsersMutex_);
    auto& users = manager_.pimpl_->audioDeviceUsers_[captureDevice];
    if (users++ == 0) {
        if (auto layer = manager_.getAudioDriver()) {
            layer->startCaptureStream(captureDevice);
        }
    }
}

AudioDeviceGuard::~AudioDeviceGuard()
{
    if (captureDevice_.empty()) {
        auto streamId = (unsigned) type_;
        if (--manager_.pimpl_->audioStreamUsers_[streamId] == 0) {
            if (auto layer = manager_.getAudioDriver())
                layer->stopStream(type_);
        }
    } else {
        std::lock_guard lock(manager_.pimpl_->audioDeviceUsersMutex_);
        auto it = manager_.pimpl_->audioDeviceUsers_.find(captureDevice_);
        if (it != manager_.pimpl_->audioDeviceUsers_.end()) {
            if (--it->second == 0) {
                if (auto layer = manager_.getAudioDriver())
                    layer->stopCaptureStream(captureDevice_);
                manager_.pimpl_->audioDeviceUsers_.erase(it);
            }
        }
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
            JAMI_DEBUG("[conf:{}] Toggling recording", id);
            rec = conf;
        } else if (auto call = account->getCall(id)) {
            JAMI_DEBUG("[call:{}] Toggling recording", id);
            rec = call;
        } else {
            JAMI_ERROR("Unable to find recordable instance {}", id);
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
    JAMI_DEBUG("[audio] Start recorded file playback: {}", filepath);

    {
        std::lock_guard lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_) {
            JAMI_ERROR("[audio] No audio layer for recorded file playback");
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
    JAMI_DEBUG("[audio] Stop recorded file playback");

    pimpl_->toneCtrl_.stopAudioFile();
    pimpl_->toneDeviceGuard_.reset();
}

void
Manager::setHistoryLimit(int days)
{
    JAMI_DEBUG("[config] Set history limit to {} days", days);
    preferences.setHistoryLimit(days);
    saveConfig();
}

int
Manager::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

void
Manager::setRingingTimeout(std::chrono::seconds timeout)
{
    JAMI_DEBUG("[config] Set ringing timeout to {} seconds", timeout);
    preferences.setRingingTimeout(timeout);
    saveConfig();
}

std::chrono::seconds
Manager::getRingingTimeout() const
{
    return preferences.getRingingTimeout();
}

bool
Manager::setAudioManager(const std::string& api)
{
    {
        std::lock_guard lock(pimpl_->audioLayerMutex_);

        if (not pimpl_->audiodriver_)
            return false;

        if (api == audioPreference.getAudioApi()) {
            JAMI_DEBUG("[audio] Audio manager '{}' already in use", api);
            return true;
        }
    }

    {
        std::lock_guard lock(pimpl_->audioLayerMutex_);
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
    std::lock_guard lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
        return 0;
    }

    return pimpl_->audiodriver_->getAudioDeviceIndex(name, AudioDeviceType::CAPTURE);
}

int
Manager::getAudioOutputDeviceIndex(const std::string& name)
{
    std::lock_guard lock(pimpl_->audioLayerMutex_);

    if (not pimpl_->audiodriver_) {
        JAMI_ERROR("[audio] Uninitialized audio layer");
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

std::string
Manager::getEchoCancellationState() const
{
    return audioPreference.getEchoCanceller();
}

void
Manager::setEchoCancellationState(const std::string& state)
{
    audioPreference.setEchoCancel(state);
}

bool
Manager::getVoiceActivityDetectionState() const
{
    return audioPreference.getVadEnabled();
}

void
Manager::setVoiceActivityDetectionState(bool state)
{
    audioPreference.setVad(state);
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
    // strip sip: which is not required and causes confusion with IP-to-IP calls
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

    auto account = incomCall.getAccount().lock();
    if (!account) {
        JAMI_ERROR("[call:{}] No account detected", incomCallId);
        return;
    }

    auto username = incomCall.toUsername();
    if (account->getAccountType() == ACCOUNT_TYPE_JAMI && username.find('/') != std::string::npos) {
        // Avoid to do heavy stuff in SIPVoIPLink's transaction_request_cb
        dht::ThreadPool::io().run([account, incomCallId, username]() {
            if (auto jamiAccount = std::dynamic_pointer_cast<JamiAccount>(account))
                jamiAccount->handleIncomingConversationCall(incomCallId, username);
        });
        return;
    }

    auto const& mediaList = MediaAttribute::mediaAttributesToMediaMaps(incomCall.getMediaAttributeList());

    if (mediaList.empty())
        JAMI_WARNING("Incoming call {} has an empty media list", incomCallId);

    JAMI_DEBUG("Incoming call {} on account {} with {} media", incomCallId, accountId, mediaList.size());

    emitSignal<libjami::CallSignal::IncomingCall>(accountId, incomCallId, incomCall.getPeerNumber(), mediaList);

    if (not base_.hasCurrentCall()) {
        incomCall.setState(Call::ConnectionState::RINGING);
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        if (not account->isRendezVous())
            base_.playRingtone(accountId);
#endif
    } else {
        if (account->isDenySecondCallEnabled()) {
            base_.declineCall(account->getAccountID(), incomCallId);
            return;
        }
    }

    addWaitingCall(incomCallId);

    if (account->isRendezVous()) {
        dht::ThreadPool::io().run([this, account, incomCall = incomCall.shared_from_this()] {
            base_.acceptCall(*incomCall);

            for (const auto& callId : account->getCallList()) {
                if (auto call = account->getCall(callId)) {
                    if (call->getState() != Call::CallState::ACTIVE)
                        continue;
                    if (call != incomCall) {
                        if (auto conf = call->getConference()) {
                            base_.addSubCall(*incomCall, *conf);
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
            auto conf = std::make_shared<Conference>(account);
            account->attach(conf);
            emitSignal<libjami::CallSignal::ConferenceCreated>(account->getAccountID(), "", conf->getConfId());

            // Bind calls according to their state
            bindCallToConference(*incomCall, *conf);
            conf->disconnectHost();
            emitSignal<libjami::CallSignal::ConferenceChanged>(account->getAccountID(),
                                                               conf->getConfId(),
                                                               conf->getStateStr());
        });
    } else if (autoAnswer_ || account->isAutoAnswerEnabled()) {
        dht::ThreadPool::io().run([this, incomCall = incomCall.shared_from_this()] { base_.acceptCall(*incomCall); });
    } else if (currentCall && currentCall->getCallId() != incomCallId) {
        // Test if already calling this person
        auto peerNumber = incomCall.getPeerNumber();
        auto currentPeerNumber = currentCall->getPeerNumber();
        string_replace(peerNumber, "@ring.dht", "");
        string_replace(currentPeerNumber, "@ring.dht", "");
        if (currentCall->getAccountId() == account->getAccountID() && currentPeerNumber == peerNumber) {
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
                    mgr.acceptCall(*incomCall);
                    mgr.endCall(accountId, currentCallID);
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
    if (currentFormat == format)
        return format;

    JAMI_DEBUG("Audio format changed: {} → {}", currentFormat.toString(), format.toString());

    pimpl_->ringbufferpool_->setInternalAudioFormat(format);
    pimpl_->toneCtrl_.setSampleRate(format.sample_rate, format.sampleFormat);
    pimpl_->dtmfKey_.reset(new DTMF(format.sample_rate, format.sampleFormat));

    return format;
}

void
Manager::setAccountsOrder(const std::string& order)
{
    JAMI_LOG("Set accounts order: {}", order);
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
        JAMI_ERROR("[account:{}] Unable to get account details on nonexistent account", accountID);
        // return an empty map since unable to throw an exception to D-Bus
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
        JAMI_ERROR("[account:{}] Unable to get volatile account details on nonexistent account", accountID);
        return {};
    }
}

void
Manager::setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    JAMI_DEBUG("[account:{}] Set account details", accountID);

    auto account = getAccount(accountID);
    if (not account) {
        JAMI_ERROR("[account:{}] Unable to find account", accountID);
        return;
    }

    // Ignore if nothing has changed
    if (details == account->getAccountDetails())
        return;

    // Unregister before modifying any account information
    account->doUnregister();

    account->setAccountDetails(details);

    if (account->isUsable())
        account->doRegister();
    else
        account->doUnregister();

    // Update account details to the client side
    emitSignal<libjami::ConfigurationSignal::AccountDetailsChanged>(accountID, details);
}

std::mt19937_64
Manager::getSeededRandomEngine()
{
    std::lock_guard l(randMutex_);
    return dht::crypto::getDerivedRandomEngine(rand_);
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
        JAMI_ERROR("Unknown {:s} param when calling addAccount(): {:s}", Conf::CONFIG_ACCOUNT_TYPE, accountType);
        return "";
    }

    newAccount->setAccountDetails(details);
    saveConfig(newAccount);
    newAccount->doRegister();

    preferences.addAccount(newAccountID);
    markAccountPending(newAccountID);

    emitSignal<libjami::ConfigurationSignal::AccountsChanged>();

    return newAccountID;
}

void
Manager::markAccountPending(const std::string& accountId)
{
    if (preferences.addPendingAccountId(accountId))
        saveConfig();
}

void
Manager::markAccountReady(const std::string& accountId)
{
    if (preferences.removePendingAccountId(accountId))
        saveConfig();
}

void
Manager::removeAccount(const std::string& accountID, bool flush)
{
    // Get it down and dying
    if (const auto& remAccount = getAccount(accountID)) {
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(remAccount)) {
            acc->endCalls();
        }
        remAccount->doUnregister(true);
        if (flush)
            remAccount->flush();
        accountFactory.removeAccount(*remAccount);
    }

    preferences.removeAccount(accountID);
    preferences.removePendingAccountId(accountID);

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
        JAMI_ERROR("[config] Preferences unserialize YAML exception: {}", e.what());
        ++errorCount;
    } catch (const std::exception& e) {
        JAMI_ERROR("[config] Preferences unserialize exception: {}", e.what());
        ++errorCount;
    } catch (...) {
        JAMI_ERROR("[config] Preferences unserialize unknown exception");
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
    std::unique_lock l(lock);
    for (const auto& dir : dirs) {
        if (accountFactory.hasAccount<JamiAccount>(dir)) {
            continue;
        }

        if (preferences.isAccountPending(dir)) {
            JAMI_INFO("[account:%s] Removing pending account from disk", dir.c_str());
            removeAccount(dir, true);
            pimpl_->cleanupAccountStorage(dir);
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
                        JAMI_ERROR("[account:{}] Unable to import account: {}", dir, e.what());
                    }
                }
                std::lock_guard l(lock);
                remaining--;
                cv.notify_one();
            });
    }
    cv.wait(l, [&remaining] { return remaining == 0; });

#ifdef ENABLE_PLUGIN
    if (pluginPreferences.getPluginsEnabled()) {
        jami::Manager::instance().getJamiPluginManager().loadPlugins();
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
    for (auto& a : getAllAccounts()) {
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
            JAMI_ERROR("[account:{}] Exception during text message sending: {}", accountID, e.what());
        }
    }
    return 0;
}

int
Manager::getMessageStatus(uint64_t) const
{
    JAMI_ERROR("Deprecated method. Please use status from message");
    return 0;
}

int
Manager::getMessageStatus(const std::string&, uint64_t) const
{
    JAMI_ERROR("Deprecated method. Please use status from message");
    return 0;
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
            acc->doUnregister(shutdownConnections);
        }
    }
    emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(accountID, acc->getVolatileAccountDetails());
}

void
Manager::loadAccountAndConversation(const std::string& accountId, bool loadAll, const std::string& convId)
{
    auto account = getAccount(accountId);
    if (!account && !autoLoad) {
        /*
         With the LIBJAMI_FLAG_NO_AUTOLOAD flag active, accounts are not
         automatically created during manager initialization, nor are
         their configurations set or backed up. This is because account
         creation triggers the initialization of the certStore. There why
         account creation now occurs here in response to a received notification.
         */
        auto accountBaseDir = fileutils::get_data_dir();
        auto configFile = accountBaseDir / accountId / "config.yml";
        try {
            if ((account = accountFactory.createAccount(JamiAccount::ACCOUNT_TYPE, accountId))) {
                account->enableAutoLoadConversations(false);
                auto configNode = YAML::LoadFile(configFile.string());
                auto config = account->buildConfig();
                config->unserialize(configNode);
                account->setConfig(std::move(config));
            }
        } catch (const std::runtime_error& e) {
            JAMI_WARNING("[account:{}] Failed to load account: {}", accountId, e.what());
            return;
        }
    }

    if (!account) {
        JAMI_WARNING("[account:{}] Unable to load account", accountId);
        return;
    }

    if (auto jamiAcc = std::dynamic_pointer_cast<JamiAccount>(account)) {
        jamiAcc->setActive(true);
        jamiAcc->reloadContacts();
        if (jamiAcc->isUsable())
            jamiAcc->doRegister();
        if (auto convModule = jamiAcc->convModule()) {
            convModule->reloadRequests();
            if (loadAll) {
                convModule->loadConversations();
            } else if (!convId.empty()) {
                jamiAcc->loadConversation(convId);
            }
        }
    }
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
        JAMI_WARNING("[account:{}] No account matches ID", accountId);
        return {};
    }

    if (not account->isUsable()) {
        JAMI_WARNING("[account:{}] Account is unusable", accountId);
        return {};
    }

    return account->newOutgoingCall(toUrl, mediaList);
}

#ifdef ENABLE_VIDEO
std::shared_ptr<video::SinkClient>
Manager::createSinkClient(const std::string& id, bool mixer)
{
    std::lock_guard lk(pimpl_->sinksMutex_);
    auto& sinkRef = pimpl_->sinkMap_[id];
    if (auto sink = sinkRef.lock())
        return sink;
    auto sink = std::make_shared<video::SinkClient>(id, mixer);
    sinkRef = sink;
    return sink;
}

void
Manager::createSinkClients(const std::string& callId,
                           const ConfInfo& infos,
                           const std::vector<std::shared_ptr<video::VideoFrameActiveWriter>>& videoStreams,
                           std::map<std::string, std::shared_ptr<video::SinkClient>>& sinksMap,
                           const std::string& accountId)
{
    auto account = accountId.empty() ? nullptr : getAccount<JamiAccount>(accountId);

    std::set<std::string> sinkIdsList {};
    std::vector<std::pair<std::shared_ptr<video::SinkClient>, std::pair<int, int>>> newSinks;

    // create video sinks
    std::unique_lock lk(pimpl_->sinksMutex_);
    for (const auto& participant : infos) {
        std::string sinkId = participant.sinkId;
        if (sinkId.empty()) {
            sinkId = callId;
            sinkId += string_remove_suffix(participant.uri, '@') + participant.device;
        }
        if (participant.w && participant.h && !participant.videoMuted) {
            auto& currentSinkW = pimpl_->sinkMap_[sinkId];
            if (account && string_remove_suffix(participant.uri, '@') == account->getUsername()
                && participant.device == account->currentDeviceId()) {
                // This is a local sink that must already exist
                continue;
            }
            if (auto currentSink = currentSinkW.lock()) {
                // If sink exists, update it
                currentSink->setCrop(participant.x, participant.y, participant.w, participant.h);
                sinkIdsList.emplace(sinkId);
                continue;
            }
            auto newSink = std::make_shared<video::SinkClient>(sinkId, false);
            currentSinkW = newSink;
            newSink->setCrop(participant.x, participant.y, participant.w, participant.h);
            newSinks.emplace_back(newSink, std::make_pair(participant.w, participant.h));
            sinksMap.emplace(sinkId, std::move(newSink));
            sinkIdsList.emplace(sinkId);
        } else {
            sinkIdsList.erase(sinkId);
        }
    }
    lk.unlock();

    // remove unused video sinks
    for (auto it = sinksMap.begin(); it != sinksMap.end();) {
        if (sinkIdsList.find(it->first) == sinkIdsList.end()) {
            for (auto& videoStream : videoStreams)
                videoStream->disconnect(it->second.get());
            it->second->stop();
            it = sinksMap.erase(it);
        } else {
            it++;
        }
    }

    // create new video sinks
    for (const auto& [sink, size] : newSinks) {
        sink->start();
        sink->setFrameSize(size.first, size.second);
        for (auto& videoStream : videoStreams)
            videoStream->attach(sink.get());
    }
}

std::shared_ptr<video::SinkClient>
Manager::getSinkClient(const std::string& id)
{
    std::lock_guard lk(pimpl_->sinksMutex_);
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

VideoManager*
Manager::getVideoManager() const
{
    return pimpl_->videoManager_.get();
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
    return *pimpl_->jami_plugin_manager;
}
#endif

std::shared_ptr<dhtnet::ChannelSocket>
Manager::gitSocket(std::string_view accountId, std::string_view deviceId, std::string_view conversationId)
{
    if (const auto acc = getAccount<JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
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
        JAMI_ERROR("[account:{}] Failed to change default moderator: account not found", accountID);
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
        JAMI_ERROR("[account:{}] Failed to get default moderators: account not found", accountID);
        return {};
    }

    auto set = acc->getDefaultModerators();
    return std::vector<std::string>(set.begin(), set.end());
}

void
Manager::enableLocalModerators(const std::string& accountID, bool isModEnabled)
{
    if (auto acc = getAccount(accountID))
        acc->editConfig([&](AccountConfig& config) { config.localModeratorsEnabled = isModEnabled; });
}

bool
Manager::isLocalModeratorsEnabled(const std::string& accountID)
{
    auto acc = getAccount(accountID);
    if (!acc) {
        JAMI_ERROR("[account:{}] Failed to get local moderators: account not found", accountID);
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
        JAMI_ERROR("[account:{}] Failed to get all moderators: account not found", accountID);
        return true; // Default value
    }
    return acc->isAllModerators();
}

void
Manager::insertGitTransport(git_smart_subtransport* tr, std::unique_ptr<P2PSubTransport>&& sub)
{
    std::lock_guard lk(pimpl_->gitTransportsMtx_);
    pimpl_->gitTransports_[tr] = std::move(sub);
}

void
Manager::eraseGitTransport(git_smart_subtransport* tr)
{
    std::lock_guard lk(pimpl_->gitTransportsMtx_);
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
