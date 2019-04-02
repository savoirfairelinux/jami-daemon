/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

namespace jami { namespace upnp {
class Controller;
}} // namespace jami::upnp

namespace YAML {
class Emitter;
class Node;
} // namespace YAML

namespace jami {

class Call;
class SystemCodecContainer;
struct IceTransportOptions;

class VoipLinkException : public std::runtime_error
{
    public:
        VoipLinkException(const std::string &str = "") :
            std::runtime_error("VoipLinkException occurred: " + str) {}
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
        void freeAccount();

        virtual void setAccountDetails(const std::map<std::string, std::string> &details);

        virtual std::map<std::string, std::string> getAccountDetails() const;

        virtual std::map<std::string, std::string> getVolatileAccountDetails() const;

        /**
         * Load the settings for this account.
         */
        virtual void loadConfig() = 0;

        virtual void serialize(YAML::Emitter &out);
        virtual void unserialize(const YAML::Node &node);

        /**
         * Get the account ID
         * @return constant account id
         */
        const std::string& getAccountID() const {
            return accountID_;
        }

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
        virtual std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails = {}) = 0;

        /* Note: we forbid incoming call creation from an instance of Account.
         * This is why no newIncomingCall() method exist here.
         */

        /**
         * If supported, send a text message from this account.
         * @return a token to query the message status
         */
        virtual uint64_t sendTextMessage(const std::string& to UNUSED,
                                     const std::map<std::string, std::string>& payloads UNUSED) { return 0; }

        virtual std::vector<DRing::Message> getLastMessages(const uint64_t& /*base_timestamp*/) {
            return {};
        }

        /**
         * Return the status corresponding to the token.
         */
        virtual im::MessageStatus getMessageStatus(uint64_t /*id*/) const {
            return im::MessageStatus::UNKNOWN;
        }

        virtual bool cancelMessage(uint64_t /*id*/) {
            return false;
        }

        std::vector<std::shared_ptr<Call>> getCalls();

        /**
         * Tell if the account is enable or not.
         * @return true if enabled, false otherwise
         */
        bool isEnabled() const noexcept {
            return enabled_;
        }

        void setEnabled(bool enable) noexcept {
            enabled_ = enable;
        }

        /**
         * Tell if the account is activated
         * (can currently be used).
         */
        bool isActive() const noexcept {
            return active_;
        }

        void setActive(bool active) noexcept {
            active_ = active;
        }

        bool isUsable() const noexcept {
            return enabled_ and active_;
        }

        bool isVideoEnabled() const noexcept {
            return videoEnabled_;
        }

        /**
         * Set the registration state of the specified link
         * @param state The registration state of underlying VoIPLink
         */
        virtual void setRegistrationState(RegistrationState state, unsigned detail_code=0, const std::string& detail_str={});

        /* They should be treated like macro definitions by the C++ compiler */
        std::string getUsername() const {
            return username_;
        }

        std::string getHostname() const {
            return hostname_;
        }
        void setHostname(const std::string &hostname) {
            hostname_ = hostname;
        }

        std::string getAlias() const {
            return alias_;
        }

        void setAlias(const std::string &alias) {
            alias_ = alias;
        }

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
        void setActiveCodecs(const std::vector<unsigned>& list);
        std::shared_ptr<AccountCodecInfo> searchCodecById(unsigned codecId, MediaType mediaType);
        std::vector<std::shared_ptr<AccountCodecInfo>> getActiveAccountCodecInfoList(MediaType mediaType) const;
        std::shared_ptr<AccountCodecInfo> searchCodecByPayload(unsigned payload, MediaType mediaType);

        std::string getRingtonePath() const {
            return ringtonePath_;
        }

        void setRingtonePath(const std::string &path) {
            ringtonePath_ = path;
        }

        bool getRingtoneEnabled() const {
            return ringtoneEnabled_;
        }
        void setRingtoneEnabled(bool enable) {
            ringtoneEnabled_ = enable;
        }

        std::string getDisplayName() const {
            return displayName_;
        }
        void setDisplayName(const std::string &name) {
            displayName_ = name;
        }

        std::string getMailBox() const {
            return mailBox_;
        }

        void setMailBox(const std::string &mb) {
            mailBox_ = mb;
        }

        void attachCall(const std::string& id);
        void detachCall(const std::string& id);

        static const char * const VIDEO_CODEC_ENABLED;
        static const char * const VIDEO_CODEC_NAME;
        static const char * const VIDEO_CODEC_PARAMETERS;
        static const char * const VIDEO_CODEC_BITRATE;

        /**
         * returns whether or not UPnP is enabled and active
         * ie: if it is able to make port mappings
         */
        bool getUPnPActive(std::chrono::seconds timeout = {}) const;

        /**
         * Get the UPnP IP (external router) address.
         * If use UPnP is set to false, the address will be empty.
         */
        IpAddr getUPnPIpAddress() const;

        virtual const IceTransportOptions getIceOptions() const noexcept;

        /**
         * Random generator engine
         * Logical account state shall never rely on the state of the random generator.
         */
        mutable std::mt19937_64 rand;

        /**
         * Inform the account that the network status has changed.
         */
        virtual void connectivityChanged() {};

    public: // virtual methods that has to be implemented by concrete classes
        /**
         * This method is called to request removal of possible account traces on the system,
         * like internal account setup files.
         */
        virtual void flush() { /* nothing to do here - overload */ };

    private:
        NON_COPYABLE(Account);

        /**
         * Helper function used to load the default codec order from the codec factory
         */
        void loadDefaultCodecs();

        /**
         * Set of call's ID attached to the account.
         */
        std::set<std::string> callIDSet_;

        void enableUpnp(bool state);

    protected:
        static void parseString(const std::map<std::string, std::string> &details, const char *key, std::string &s);
        static void parseBool(const std::map<std::string, std::string> &details, const char *key, bool &b);
        static void parsePath(const std::map<std::string, std::string> &details, const char *key, std::string &s, const std::string& base);

        template<class T>
        static inline void
        parseInt(const std::map<std::string, std::string>& details, const char* key, T& i) {
            const auto& iter = details.find(key);
            if (iter == details.end()) {
                JAMI_ERR("Couldn't find key \"%s\"", key);
                return;
            }
            i = atoi(iter->second.c_str());
        }

        friend class ConfigurationTest;

        // General configuration keys for accounts
        static const char * const ALL_CODECS_KEY;
        static const char * const RINGTONE_PATH_KEY;
        static const char * const RINGTONE_ENABLED_KEY;
        static const char * const VIDEO_ENABLED_KEY;
        static const char * const DISPLAY_NAME_KEY;
        static const char * const ALIAS_KEY;
        static const char * const TYPE_KEY;
        static const char * const ID_KEY;
        static const char * const USERNAME_KEY;
        static const char * const AUTHENTICATION_USERNAME_KEY;
        static const char * const PASSWORD_KEY;
        static const char * const HOSTNAME_KEY;
        static const char * const ACCOUNT_ENABLE_KEY;
        static const char * const ACCOUNT_AUTOANSWER_KEY;
        static const char * const ACCOUNT_ACTIVE_CALL_LIMIT_KEY;
        static const char * const MAILBOX_KEY;
        static const char * const USER_AGENT_KEY;
        static const char * const HAS_CUSTOM_USER_AGENT_KEY;
        static const char * const DEFAULT_USER_AGENT;
        static const char * const PRESENCE_MODULE_ENABLED_KEY;
        static const char * const UPNP_ENABLED_KEY;
        static const char * const PROXY_ENABLED_KEY;
        static const char * const PROXY_SERVER_KEY;
        static const char * const PROXY_PUSH_TOKEN_KEY;

        static std::string mapStateNumberToString(RegistrationState state);

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

        /**
         * The number of concurrent calls for the account
         * -1: Unlimited
         *  0: Do not disturb
         *  1: Single call
         *  +: Multi line
         */
        int activeCallLimit_ {-1} ;

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
         * Useragent used for registration
         */
        std::string userAgent_;

        //  true if user has overridden default
        bool hasCustomUserAgent_;

        /**
         * Account mail box
         */
        std::string mailBox_;

        /**
         * UPnP IGD controller and the mutex to access it
         */
        std::unique_ptr<jami::upnp::Controller> upnp_;
        mutable std::mutex upnp_mtx {};

        /**
         * private account codec searching functions
         */
        std::shared_ptr<AccountCodecInfo> searchCodecByName(std::string name, MediaType mediaType);
        std::vector<unsigned> getAccountCodecInfoIdList(MediaType mediaType) const;
        void setAllCodecsActive(MediaType mediaType, bool active);
};

static inline std::ostream&
operator<< (std::ostream& os, const Account& acc)
{
    os << "[Account " << acc.getAccountID() << "] ";
    return os;
}

} // namespace jami

#endif
