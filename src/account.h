/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "noncopyable.h"
#include "config/serializable.h"
#include "registration_states.h"
#include "ip_utils.h"
#include "media_codec.h"
#include "logger.h"

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

namespace ring { namespace upnp {
class Controller;
}} // namespace ring::upnp

namespace YAML {
class Emitter;
class Node;
} // namespace YAML

namespace ring {

class Call;
class SystemCodecContainer;
class IceTransportOptions;

class VoipLinkException : public std::runtime_error
{
    public:
        VoipLinkException(const std::string &str = "") :
            std::runtime_error("VoipLinkException occured: " + str) {}
};

/**
 * @file account.h
 * @brief Interface to protocol account (SIPAccount, IAXAccount)
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
         *   ***Current calls using this account are HUNG-UP***
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

        /**
         * Create a new outgoing call.
         *
         * @param toUrl The address to call
         * @return std::shared_ptr<Call> A pointer on the created call
         */
        virtual std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl) = 0;

        /* Note: we forbid incoming call creation from an instance of Account.
         * This is why no newIncomingCall() method exist here.
         */

        /**
         * If supported, send a text message from this account.
         */
        virtual void sendTextMessage(const std::string& /* to */, const std::string& /* message */) {};

        std::vector<std::shared_ptr<Call>> getCalls();

        /**
         * Tell if the account is enable or not.
         * @return true if enabled
         *	     false otherwise
         */
        bool isEnabled() const {
            return enabled_;
        }

        bool isVideoEnabled() const {
            return videoEnabled_;
        }

        void setEnabled(bool enable) {
            enabled_ = enable;
        }

        /**
         * Set the registration state of the specified link
         * @param state	The registration state of underlying VoIPLink
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
        std::vector<unsigned> getActiveCodecs() const;

        /**
         * Update both the codec order structure and the codec string used for
         * SDP offer and configuration respectively
         */
        void setActiveCodecs(const std::vector<unsigned>& list);
        std::shared_ptr<AccountCodecInfo> searchCodecById(unsigned codecId, MediaType mediaType);
        std::vector<unsigned> getActiveAccountCodecInfoIdList(MediaType mediaType) const;
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

    protected:
        static void parseString(const std::map<std::string, std::string> &details, const char *key, std::string &s);
        static void parseBool(const std::map<std::string, std::string> &details, const char *key, bool &b);

        template<class T>
        static inline void
        parseInt(const std::map<std::string, std::string>& details, const char* key, T i) {
            const auto& iter = details.find(key);
            if (iter == details.end()) {
                RING_ERR("Couldn't find key \"%s\"", key);
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
        static const char * const MAILBOX_KEY;
        static const char * const USER_AGENT_KEY;
        static const char * const HAS_CUSTOM_USER_AGENT_KEY;
        static const char * const DEFAULT_USER_AGENT;
        static const char * const PRESENCE_MODULE_ENABLED_KEY;
        static const char * const UPNP_ENABLED_KEY;

        static std::string mapStateNumberToString(RegistrationState state);

        /**
         * Account ID are assign in constructor and shall not changed
         */
        const std::string accountID_;

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
         * Tells if the link is enabled, active.
         * This implies the link will be initialized on startup.
         * Modified by the configuration (key: ENABLED)
         */
        bool enabled_;

        /* If true, automatically answer calls to this account */
        bool autoAnswerEnabled_;

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
         * List of audio and video codecs obtained when parsing configuration and used
         * to generate codec order list
         */
        std::string allCodecStr_;

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
         * Random generator engine
         * Logical account state shall never rely on the state of the random generator.
         */
        mutable std::mt19937_64 rand_;

        /**
         * UPnP IGD controller and the mutex to access it
         */
        std::unique_ptr<ring::upnp::Controller> upnp_;
        mutable std::mutex upnp_mtx {};

        /**
         * flag which determines if this account is set to use UPnP.
         */
        std::atomic_bool upnpEnabled_ {false};

        /**
         * private account codec searching functions
         */
        std::shared_ptr<AccountCodecInfo> searchCodecByName(std::string name, MediaType mediaType);
        std::vector<unsigned> getAccountCodecInfoIdList(MediaType mediaType) const;
        void desactivateAllMedia(MediaType mediaType);

};

} // namespace ring

#endif
