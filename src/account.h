/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 */

#ifndef ACCOUNT_H
#define ACCOUNT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configurationmanager_interface.h"
#include "noncopyable.h"
#include "config/serializable.h"
#include "registration_states.h"
#include "im/message_engine.h"
#include "ip_utils.h"
#include "media_codec.h"
#include "media/media_attribute.h"
#include "logger.h"
#include "compiler_intrinsics.h" // include the "UNUSED" macro

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <random>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <chrono>

namespace YAML {
class Emitter;
class Node;
} // namespace YAML

namespace Json {
class Value;
}

namespace jami {
static constexpr uint64_t DRING_ID_MAX_VAL = 9007199254740992;

namespace upnp {
class Controller;
} // namespace upnp

class Call;
class SystemCodecContainer;
struct IceTransportOptions;

class VoipLinkException : public std::runtime_error
{
public:
    VoipLinkException(const std::string& str = "")
        : std::runtime_error("VoipLinkException occurred: " + str)
    {}
};

/**
 * @file account.h
 * @brief Interface to protocol account (ex: SIPAccount)
 * It can be enable on loading or activate after.
 * It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
 */

class Account : public Serializable, public std::enable_shared_from_this<Account>
{
public:
    Account(const std::string& accountID);

    /**
     * Virtual destructor
     */
    virtual ~Account();

    /**
     * Free all ressources related to this account.
     *   ***Current calls using this account are HANG-UP***
     */
    void hangupCalls();

    virtual void setAccountDetails(const std::map<std::string, std::string>& details);

    virtual std::map<std::string, std::string> getAccountDetails() const;

    virtual std::map<std::string, std::string> getVolatileAccountDetails() const;

    virtual std::string getFromUri() const = 0;

    /**
     * Load the settings for this account.
     */
    virtual void loadConfig() = 0;

    virtual void serialize(YAML::Emitter& out) const;
    virtual void unserialize(const YAML::Node& node);

    /**
     * Get the account ID
     * @return constant account id
     */
    const std::string& getAccountID() const { return accountID_; }

    virtual const char* getAccountType() const = 0;

    /**
     * Returns true if this is the IP2IP account
     */
    virtual bool isIP2IP() const { return false; }

    /**
     * Register the account.
     * This should update the getRegistrationState() return value.
     */
    virtual void doRegister() = 0;

    /**
     * Unregister the account.
     * This should update the getRegistrationState() return value.
     */
    virtual void doUnregister(std::function<void(bool)> cb = std::function<void(bool)>()) = 0;

    RegistrationState getRegistrationState() const { return registrationState_; }

    /**
     * Create a new outgoing call.
     *
     * @param toUrl The address to call
     * @return std::shared_ptr<Call> A pointer on the created call
     */
    virtual std::shared_ptr<Call> newOutgoingCall(
        std::string_view toUrl, const std::map<std::string, std::string>& volatileCallDetails = {})
        = 0;

    /**
     * Create a new outgoing call.
     *
     * @param toUrl The address to call
     * @param mediaList A list of media
     * @return The created call
     */
    virtual std::shared_ptr<Call> newOutgoingCall(std::string_view toUrl,
                                                  const std::vector<MediaAttribute>& mediaList)
        = 0;

    /**
     * If supported, send a text message from this account.
     * @return a token to query the message status
     */
    virtual uint64_t sendTextMessage(const std::string& /*to*/,
                                     const std::map<std::string, std::string>& /*payloads*/)
    {
        return 0;
    }

    virtual void sendInstantMessage(const std::string& /*convId*/,
                                    const std::map<std::string, std::string>& /*msg*/)
    {}

    virtual void setIsComposing(const std::string& /*conversationUri*/, bool /*isWriting*/) {};

    virtual void onIsComposing(const std::string& /*conversationId*/,
                               const std::string& /*peer*/,
                               bool /*isWriting*/);

    virtual bool setMessageDisplayed(const std::string& /*conversationUri*/,
                                     const std::string& /*messageId*/,
                                     int /*status*/)
    {
        return false;
    };

    virtual std::vector<DRing::Message> getLastMessages(const uint64_t& /*base_timestamp*/)
    {
        return {};
    }

    virtual std::map<std::string, std::string> getNearbyPeers() const { return {}; }

    /**
     * Return the status corresponding to the token.
     */
    virtual im::MessageStatus getMessageStatus(uint64_t /*id*/) const
    {
        return im::MessageStatus::UNKNOWN;
    }

    virtual bool cancelMessage(uint64_t /*id*/) { return false; }

    /**
     * Tell if the account is enable or not.
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const noexcept { return enabled_; }

    void setEnabled(bool enable) noexcept { enabled_ = enable; }

    /**
     * Tell if the account is activated
     * (can currently be used).
     */
    bool isActive() const noexcept { return active_; }

    void setActive(bool active) noexcept { active_ = active; }

    bool isUsable() const noexcept { return enabled_ and active_; }

    bool isVideoEnabled() const noexcept { return videoEnabled_; }

    /**
     * Set the registration state of the specified link
     * @param state The registration state of underlying VoIPLink
     */
    virtual void setRegistrationState(RegistrationState state,
                                      unsigned detail_code = 0,
                                      const std::string& detail_str = {});

    /* They should be treated like macro definitions by the C++ compiler */
    const std::string& getUsername() const { return username_; }

    const std::string& getHostname() const { return hostname_; }
    void setHostname(const std::string& hostname) { hostname_ = hostname; }

    const std::string& getAlias() const { return alias_; }

    void setAlias(const std::string& alias) { alias_ = alias; }

    static std::vector<unsigned> getDefaultCodecsId();
    static std::map<std::string, std::string> getDefaultCodecDetails(const unsigned& codecId);

    /* Accessor to data structures
     * @return The list that reflects the user's choice
     */
    std::vector<unsigned> getActiveCodecs(MediaType mediaType = MEDIA_ALL) const;
    bool hasActiveCodec(MediaType mediaType) const;

    /**
     * Update both the codec order structure and the codec string used for
     * SDP offer and configuration respectively
     */
    virtual void setActiveCodecs(const std::vector<unsigned>& list);
    std::shared_ptr<AccountCodecInfo> searchCodecById(unsigned codecId, MediaType mediaType);
    std::vector<std::shared_ptr<AccountCodecInfo>> getActiveAccountCodecInfoList(
        MediaType mediaType) const;
    std::shared_ptr<AccountCodecInfo> searchCodecByPayload(unsigned payload, MediaType mediaType);

    std::string getRingtonePath() const { return ringtonePath_; }

    void setRingtonePath(const std::string& path) { ringtonePath_ = path; }

    bool getRingtoneEnabled() const { return ringtoneEnabled_; }
    void setRingtoneEnabled(bool enable) { ringtoneEnabled_ = enable; }

    std::string getDisplayName() const { return displayName_; }
    void setDisplayName(const std::string& name) { displayName_ = name; }

    std::string getMailBox() const { return mailBox_; }

    void setMailBox(const std::string& mb) { mailBox_ = mb; }

    bool isRendezVous() const { return isRendezVous_; }

    void attachCall(const std::string& id);
    void detachCall(const std::string& id);

    static const char* const VIDEO_CODEC_ENABLED;
    static const char* const VIDEO_CODEC_NAME;
    static const char* const VIDEO_CODEC_PARAMETERS;
    static const char* const VIDEO_CODEC_BITRATE;

    /**
     * returns whether or not UPnP is enabled and active
     * ie: if it is able to make port mappings
     */
    bool getUPnPActive() const;

    /**
     * Get the UPnP IP (external router) address.
     * If use UPnP is set to false, the address will be empty.
     */
    IpAddr getUPnPIpAddress() const;

    /**
     * Random generator engine
     * Logical account state shall never rely on the state of the random generator.
     */
    mutable std::mt19937_64 rand;

    /**
     * Inform the account that the network status has changed.
     */
    virtual void connectivityChanged() {};

    virtual void onNewGitCommit(const std::string& /*peer*/,
                                const std::string& /*deviceId*/,
                                const std::string& /*conversationId*/,
                                const std::string& /*commitId*/) {};

    virtual void onAskForTransfer(const std::string& /*peer*/,
                                  const std::string& /*deviceId*/,
                                  const std::string& /*conversationId*/,
                                  const std::string& /*fileId*/,
                                  size_t /*start*/,
                                  size_t /*end*/) {};

    // Invites
    virtual void onConversationRequest(const std::string& /*from*/, const Json::Value&) {};
    virtual void onNeedConversationRequest(const std::string& /*from*/,
                                           const std::string& /*conversationId*/) {};
    virtual void checkIfRemoveForCompat(const std::string& /*peerUri*/) {};

    /**
     * Helper function used to load the default codec order from the codec factory
     */
    void loadDefaultCodecs();

    void setCodecActive(unsigned codecId);

    void setCodecInactive(unsigned codecId);

    std::vector<unsigned> convertIdToAVId(const std::vector<unsigned>& list);

    /**
     * Get the user-agent
     */
    const std::string& getUserAgentName();

    std::set<std::string> getDefaultModerators() const { return defaultModerators_; }

    void addDefaultModerator(const std::string& peerURI);
    void removeDefaultModerator(const std::string& peerURI);

    bool isLocalModeratorsEnabled() const { return localModeratorsEnabled_; }
    void enableLocalModerators(bool isModEnabled) { localModeratorsEnabled_ = isModEnabled; }
    bool isAllModerators() const { return allModeratorsEnabled_; }
    void setAllModerators(bool isAllModeratorEnabled)
    {
        allModeratorsEnabled_ = isAllModeratorEnabled;
    }

    // Enable/disable multi-stream feature.
    // Multi-stream feature changes the callflow of the re-invite process. All
    // clients must support this feature before it can be enabled by default.
    // These two internal APIs allow controlling the callflow accordingly. They
    // should be removed once the multi-stream feature is fully supported.
    bool isMultiStreamEnabled() const { return multiStreamEnabled_; }
    void enableMultiStream(bool enable) { multiStreamEnabled_ = enable; }

public:
    // virtual methods that has to be implemented by concrete classes
    /**
     * This method is called to request removal of possible account traces on the system,
     * like internal account setup files.
     */
    virtual void flush() {/* nothing to do here - overload */};

private:
    NON_COPYABLE(Account);

    /**
     * Set of call's ID attached to the account.
     */
    std::mutex callIDSetMtx_;
    std::set<std::string> callIDSet_;

protected:
    void updateUpnpController();

    static void parseString(const std::map<std::string, std::string>& details,
                            const char* key,
                            std::string& s);
    static void parseBool(const std::map<std::string, std::string>& details,
                          const char* key,
                          bool& b);
    static void parsePath(const std::map<std::string, std::string>& details,
                          const char* key,
                          std::string& s,
                          const std::string& base);

    template<class T>
    static inline void parseInt(const std::map<std::string, std::string>& details,
                                const char* key,
                                T& i)
    {
        const auto& iter = details.find(key);
        if (iter == details.end()) {
            JAMI_ERR("Couldn't find key \"%s\"", key);
            return;
        }
        i = atoi(iter->second.c_str());
    }

    friend class ConfigurationTest;

    // General configuration keys for accounts
    static const char* const ALL_CODECS_KEY;
    static const char* const RINGTONE_PATH_KEY;
    static const char* const RINGTONE_ENABLED_KEY;
    static const char* const VIDEO_ENABLED_KEY;
    static const char* const DISPLAY_NAME_KEY;
    static const char* const ALIAS_KEY;
    static const char* const TYPE_KEY;
    static const char* const ID_KEY;
    static const char* const USERNAME_KEY;
    static const char* const AUTHENTICATION_USERNAME_KEY;
    static const char* const PASSWORD_KEY;
    static const char* const HOSTNAME_KEY;
    static const char* const ACCOUNT_ENABLE_KEY;
    static const char* const ACCOUNT_AUTOANSWER_KEY;
    static const char* const ACCOUNT_ISRENDEZVOUS_KEY;
    static const char* const ACCOUNT_ACTIVE_CALL_LIMIT_KEY;
    static const char* const MAILBOX_KEY;
    static const char* const USER_AGENT_KEY;
    static const char* const HAS_CUSTOM_USER_AGENT_KEY;
    static const char* const PRESENCE_MODULE_ENABLED_KEY;
    static const char* const UPNP_ENABLED_KEY;
    static const char* const PROXY_ENABLED_KEY;
    static const char* const PROXY_SERVER_KEY;
    static const char* const PROXY_PUSH_TOKEN_KEY;
    static const char* const ACTIVE_CODEC_KEY;
    static const char* const DEFAULT_MODERATORS_KEY;
    static const char* const LOCAL_MODERATORS_ENABLED_KEY;
    static const char* const ALL_MODERATORS_ENABLED_KEY;

    static const std::string DEFAULT_USER_AGENT;

    static std::string mapStateNumberToString(RegistrationState state);

    /**
     * Build the user-agent string
     */
    static std::string setDefaultUserAgent();

    /**
     * Account ID are assign in constructor and shall not changed
     */
    const std::string accountID_;

    mutable std::mutex configurationMutex_ {};

    /**
     * Account login information: username
     */
    std::string username_;

    /**
     * Account login information: hostname
     */
    std::string hostname_;

    /**
     * Account login information: Alias
     */
    std::string alias_;

    /**
     * Tells if the account is enabled.
     * This implies the link will be initialized on startup.
     * Modified by the configuration (key: ENABLED)
     */
    bool enabled_;

    /**
     * Tells if the account is active now.
     * This allows doRegister to be called.
     * When an account is unactivated, doUnregister must be called.
     */
    bool active_ {true};

    /* If true, automatically answer calls to this account */
    bool autoAnswerEnabled_;

    /* If true mix calls into a conference */
    bool isRendezVous_;

    /**
     * The number of concurrent calls for the account
     * -1: Unlimited
     *  0: Do not disturb
     *  1: Single call
     *  +: Multi line
     */
    int activeCallLimit_ {-1};

    /*
     * The general, protocol neutral registration
     * state of the account
     */
    RegistrationState registrationState_;

    /**
     * Vector containing all system codecs (with default parameters)
     */
    std::shared_ptr<SystemCodecContainer> systemCodecContainer_;
    /**
     * Vector containing all account codecs (set of system codecs with custom parameters)
     */
    std::vector<std::shared_ptr<AccountCodecInfo>> accountCodecInfoList_;

    /**
     * Ringtone .au file used for this account
     */
    std::string ringtonePath_;

    /**
     * Allows user to temporarily disable video calling
     */

    bool videoEnabled_ = true;

    /**
     * Play ringtone when receiving a call
     */
    bool ringtoneEnabled_;

    /**
     * Display name when calling
     */
    std::string displayName_;

    /**
     * User-agent used for registration
     */
    std::string customUserAgent_;

    //  true if user has overridden default
    bool hasCustomUserAgent_;

    /**
     * Account mail box
     */
    std::string mailBox_;

    /**
     * UPnP IGD controller and the mutex to access it
     */
    bool upnpEnabled_;
    mutable std::mutex upnp_mtx {};
    std::shared_ptr<jami::upnp::Controller> upnpCtrl_;

    std::set<std::string> defaultModerators_ {};
    bool localModeratorsEnabled_;
    bool allModeratorsEnabled_;

    bool multiStreamEnabled_;

    /**
     * private account codec searching functions
     */
    std::shared_ptr<AccountCodecInfo> searchCodecByName(const std::string& name,
                                                        MediaType mediaType);
    std::vector<unsigned> getAccountCodecInfoIdList(MediaType mediaType) const;
    void setAllCodecsActive(MediaType mediaType, bool active);
    void sortCodec();
};

static inline std::ostream&
operator<<(std::ostream& os, const Account& acc)
{
    os << "[Account " << acc.getAccountID() << "] ";
    return os;
}

} // namespace jami

#endif
