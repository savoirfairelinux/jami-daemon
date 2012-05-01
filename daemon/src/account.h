/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>

#include "global.h"
#include "noncopyable.h"
#include "config/sfl_config.h"
#include "config/serializable.h"

class VoIPLink;

/**
 * @file account.h
 * @brief Interface to protocol account (SIPAccount, IAXAccount)
 * It can be enable on loading or activate after.
 * It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
 */

/** Contains all the state an Voip can be in */
enum RegistrationState {
    Unregistered,
    Trying,
    Registered,
    Error,
    ErrorAuth ,
    ErrorNetwork ,
    ErrorHost,
    ErrorExistStun,
    ErrorNotAcceptable,
    NumberOfStates
};

// Account identifier
static const char *const CONFIG_ACCOUNT_ID                   = "Account.id";

// Common account parameters
static const char *const CONFIG_ACCOUNT_TYPE                 = "Account.type";
static const char *const CONFIG_ACCOUNT_ALIAS                = "Account.alias";
static const char *const CONFIG_ACCOUNT_MAILBOX	             = "Account.mailbox";
static const char *const CONFIG_ACCOUNT_ENABLE               = "Account.enable";
static const char *const CONFIG_ACCOUNT_REGISTRATION_EXPIRE  = "Account.registrationExpire";
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATUS = "Account.registrationStatus";
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATE_CODE = "Account.registrationCode";
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATE_DESC = "Account.registrationDescription";
static const char *const CONFIG_CREDENTIAL_NUMBER            = "Credential.count";
static const char *const CONFIG_ACCOUNT_DTMF_TYPE            = "Account.dtmfType";
static const char *const CONFIG_RINGTONE_PATH                = "Account.ringtonePath";
static const char *const CONFIG_RINGTONE_ENABLED             = "Account.ringtoneEnabled";

static const char *const CONFIG_ACCOUNT_HOSTNAME             = "Account.hostname";
static const char *const CONFIG_ACCOUNT_USERNAME             = "Account.username";
static const char *const CONFIG_ACCOUNT_ROUTESET             = "Account.routeset";
static const char *const CONFIG_ACCOUNT_PASSWORD             = "Account.password";
static const char *const CONFIG_ACCOUNT_REALM                = "Account.realm";
static const char *const CONFIG_ACCOUNT_DEFAULT_REALM        = "*";
static const char *const CONFIG_ACCOUNT_USERAGENT            = "Account.useragent";

static const char *const CONFIG_LOCAL_INTERFACE              = "Account.localInterface";
static const char *const CONFIG_PUBLISHED_SAMEAS_LOCAL       = "Account.publishedSameAsLocal";
static const char *const CONFIG_LOCAL_PORT                   = "Account.localPort";
static const char *const CONFIG_PUBLISHED_PORT               = "Account.publishedPort";
static const char *const CONFIG_PUBLISHED_ADDRESS            = "Account.publishedAddress";

static const char *const CONFIG_DISPLAY_NAME                 = "Account.displayName";
static const char *const CONFIG_DEFAULT_ADDRESS              = "0.0.0.0";

// SIP specific parameters
static const char *const CONFIG_SIP_PROXY                    = "SIP.proxy";
static const char *const CONFIG_STUN_SERVER                  = "STUN.server";
static const char *const CONFIG_STUN_ENABLE                  = "STUN.enable";

// SRTP specific parameters
static const char *const CONFIG_SRTP_ENABLE                  = "SRTP.enable";
static const char *const CONFIG_SRTP_KEY_EXCHANGE            = "SRTP.keyExchange";
static const char *const CONFIG_SRTP_ENCRYPTION_ALGO         = "SRTP.encryptionAlgorithm";  // Provided by ccRTP,0=NULL,1=AESCM,2=AESF8
static const char *const CONFIG_SRTP_RTP_FALLBACK            = "SRTP.rtpFallback";
static const char *const CONFIG_ZRTP_HELLO_HASH              = "ZRTP.helloHashEnable";
static const char *const CONFIG_ZRTP_DISPLAY_SAS             = "ZRTP.displaySAS";
static const char *const CONFIG_ZRTP_NOT_SUPP_WARNING        = "ZRTP.notSuppWarning";
static const char *const CONFIG_ZRTP_DISPLAY_SAS_ONCE        = "ZRTP.displaySasOnce";

static const char *const CONFIG_TLS_LISTENER_PORT            = "TLS.listenerPort";
static const char *const CONFIG_TLS_ENABLE                   = "TLS.enable";
static const char *const CONFIG_TLS_CA_LIST_FILE             = "TLS.certificateListFile";
static const char *const CONFIG_TLS_CERTIFICATE_FILE         = "TLS.certificateFile";
static const char *const CONFIG_TLS_PRIVATE_KEY_FILE         = "TLS.privateKeyFile";
static const char *const CONFIG_TLS_PASSWORD                 = "TLS.password";
static const char *const CONFIG_TLS_METHOD                   = "TLS.method";
static const char *const CONFIG_TLS_CIPHERS                  = "TLS.ciphers";
static const char *const CONFIG_TLS_SERVER_NAME              = "TLS.serverName";
static const char *const CONFIG_TLS_VERIFY_SERVER            = "TLS.verifyServer";
static const char *const CONFIG_TLS_VERIFY_CLIENT            = "TLS.verifyClient";
static const char *const CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE = "TLS.requireClientCertificate";
static const char *const CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC  = "TLS.negotiationTimeoutSec";
static const char *const CONFIG_TLS_NEGOTIATION_TIMEOUT_MSEC = "TLS.negotiationTimemoutMsec";

// General configuration keys for accounts
static const char * const ALIAS_KEY = "alias";
static const char * const TYPE_KEY = "type";
static const char * const ID_KEY = "id";
static const char * const USERNAME_KEY = "username";
static const char * const AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
static const char * const PASSWORD_KEY = "password";
static const char * const HOSTNAME_KEY = "hostname";
static const char * const ACCOUNT_ENABLE_KEY = "enable";
static const char * const MAILBOX_KEY = "mailbox";

static const char * const CODECS_KEY = "codecs";  // 0/9/110/111/112/
static const char * const RINGTONE_PATH_KEY = "ringtonePath";
static const char * const RINGTONE_ENABLED_KEY = "ringtoneEnabled";
static const char * const DISPLAY_NAME_KEY = "displayName";

class Account : public Serializable {

    public:

        Account(const std::string& accountID, const std::string &type);

        /**
         * Virtual destructor
         */
        virtual ~Account();

        virtual void setAccountDetails(std::map<std::string, std::string> details) = 0;

        virtual std::map<std::string, std::string> getAccountDetails() const = 0;

        /**
         * Load the settings for this account.
         */
        virtual void loadConfig() = 0;

        /**
         * Get the account ID
         * @return constant account id
         */
        std::string getAccountID() const {
            return accountID_;
        }

        /**
         * Get the voiplink pointer
         * @return VoIPLink* the pointer or 0
         */
        virtual VoIPLink* getVoIPLink() = 0;

        /**
         * Register the underlying VoIPLink. Launch the event listener.
         * This should update the getRegistrationState() return value.
         */
        virtual void registerVoIPLink() = 0;

        /**
         * Unregister the underlying VoIPLink. Stop the event listener.
         * This should update the getRegistrationState() return value.
         */
        virtual void unregisterVoIPLink() = 0;

        /**
         * Tell if the account is enable or not.
         * @return true if enabled
         *	     false otherwise
         */
        bool isEnabled() const {
            return enabled_;
        }

        void setEnabled(bool enable) {
            enabled_ = enable;
        }

        /**
         * Set the registration state of the specified link
         * @param state	The registration state of underlying VoIPLink
         */
        void setRegistrationState(const RegistrationState &state);

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

        /**
         * Accessor to data structures
         * @return CodecOrder& The list that reflects the user's choice
         */
        const CodecOrder& getActiveCodecs() const {
            return codecOrder_;
        }

        /**
         * Update both the codec order structure and the codec string used for
         * SDP offer and configuration respectively
         */
        void setActiveCodecs(const std::vector <std::string>& list);

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

    private:
        NON_COPYABLE(Account);

        /**
         * Helper function used to load the default codec order from the codec factory
         * setActiveCodecs is called to sync both codecOrder_ and codecStr_
         */
        void loadDefaultCodecs();

    protected:
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

        /*
         * The account type
         * IAX2 or SIP
         */
        std::string type_;

        /*
         * The general, protocol neutral registration
         * state of the account
         */
        RegistrationState registrationState_;

        /**
         * Vector containing the order of the codecs
         */
        CodecOrder codecOrder_;

        /**
         * List of codec obtained when parsing configuration and used
         * to generate codec order list
         */
        std::string codecStr_;

        /**
         * Ringtone .au file used for this account
         */
        std::string ringtonePath_;

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

        /**
             * Account mail box
         */
        std::string mailBox_;

};

#endif
