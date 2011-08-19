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

#include "config/config.h"
#include "voiplink.h"
#include "config/serializable.h"

class VoIPLink;

/**
 * @file account.h
 * @brief Interface to protocol account (SIPAccount, IAXAccount)
 * It can be enable on loading or activate after.
 * It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
 */

/** Contains all the state an Voip can be in */
typedef enum RegistrationState {
    Unregistered,
    Trying,
    Registered,
    Error,
    ErrorAuth ,
    ErrorNetwork ,
    ErrorHost,
    ErrorExistStun,
    ErrorConfStun,
    NumberOfState
} RegistrationState;

// Account identifier
#define ACCOUNT_ID                          "Account.id"

// Common account parameters
#define CONFIG_ACCOUNT_TYPE                 "Account.type"
#define CONFIG_ACCOUNT_ALIAS                "Account.alias"
#define CONFIG_ACCOUNT_MAILBOX	            "Account.mailbox"
#define CONFIG_ACCOUNT_ENABLE               "Account.enable"
#define CONFIG_ACCOUNT_RESOLVE_ONCE         "Account.resolveOnce"
#define CONFIG_ACCOUNT_REGISTRATION_EXPIRE  "Account.expire"
#define CONFIG_CREDENTIAL_NUMBER            "Credential.count"
#define ACCOUNT_DTMF_TYPE                   "Account.dtmfType"
#define CONFIG_RINGTONE_PATH                "Account.ringtonePath"
#define CONFIG_RINGTONE_ENABLED             "Account.ringtoneEnabled"

#define HOSTNAME                            "hostname"
#define USERNAME                            "username"
#define ROUTESET                            "routeset"
#define PASSWORD                            "password"
#define REALM                               "realm"
#define DEFAULT_REALM                       "*"
#define USERAGENT							"useragent"

#define LOCAL_INTERFACE                     "Account.localInterface"
#define PUBLISHED_SAMEAS_LOCAL              "Account.publishedSameAsLocal"
#define LOCAL_PORT                          "Account.localPort"
#define PUBLISHED_PORT                      "Account.publishedPort"
#define PUBLISHED_ADDRESS                   "Account.publishedAddress"

#define DISPLAY_NAME                        "Account.displayName"
#define DEFAULT_ADDRESS                     "0.0.0.0"

// SIP specific parameters
#define SIP_PROXY                           "SIP.proxy"
#define STUN_SERVER							"STUN.server"
#define STUN_ENABLE							"STUN.enable"

// SRTP specific parameters
#define SRTP_ENABLE                         "SRTP.enable"
#define SRTP_KEY_EXCHANGE                   "SRTP.keyExchange"
#define SRTP_ENCRYPTION_ALGO                "SRTP.encryptionAlgorithm"  // Provided by ccRTP,0=NULL,1=AESCM,2=AESF8 
#define SRTP_RTP_FALLBACK                   "SRTP.rtpFallback"
#define ZRTP_HELLO_HASH                     "ZRTP.helloHashEnable"
#define ZRTP_DISPLAY_SAS                    "ZRTP.displaySAS"
#define ZRTP_NOT_SUPP_WARNING               "ZRTP.notSuppWarning"
#define ZRTP_DISPLAY_SAS_ONCE               "ZRTP.displaySasOnce"

#define TLS_LISTENER_PORT                   "TLS.listenerPort"
#define TLS_ENABLE                          "TLS.enable"
#define TLS_CA_LIST_FILE                    "TLS.certificateListFile"
#define TLS_CERTIFICATE_FILE                "TLS.certificateFile"
#define TLS_PRIVATE_KEY_FILE                "TLS.privateKeyFile"
#define TLS_PASSWORD                        "TLS.password"
#define TLS_METHOD                          "TLS.method"
#define TLS_CIPHERS                         "TLS.ciphers"
#define TLS_SERVER_NAME                     "TLS.serverName"
#define TLS_VERIFY_SERVER                   "TLS.verifyServer"
#define TLS_VERIFY_CLIENT                   "TLS.verifyClient"
#define TLS_REQUIRE_CLIENT_CERTIFICATE      "TLS.requireClientCertificate"
#define TLS_NEGOTIATION_TIMEOUT_SEC         "TLS.negotiationTimeoutSec"
#define TLS_NEGOTIATION_TIMEOUT_MSEC        "TLS.negotiationTimemoutMsec"

#define REGISTRATION_STATUS                 "Status"
#define REGISTRATION_STATE_CODE             "Registration.code"
#define REGISTRATION_STATE_DESCRIPTION      "Registration.description"


// General configuration keys for accounts
static const char * const aliasKey = "alias";
static const char * const typeKey = "type";
static const char * const idKey = "id";
static const char * const usernameKey = "username";
static const char * const authenticationUsernameKey = "authenticationUsername";
static const char * const passwordKey = "password";
static const char * const hostnameKey = "hostname";
static const char * const accountEnableKey = "enable";
static const char * const mailboxKey = "mailbox";

static const char * const codecsKey = "codecs";  // 0/9/110/111/112/
static const char * const ringtonePathKey = "ringtonePath";
static const char * const ringtoneEnabledKey = "ringtoneEnabled";
static const char * const displayNameKey = "displayName";

class Account : public Serializable
{

    public:

        Account (const std::string& accountID, const std::string &type);

        /**
         * Virtual destructor
         */
        virtual ~Account();

        /**
         * Method called by the configuration engine to serialize instance's information
         * into configuration file.
         */
        virtual void serialize (Conf::YamlEmitter *emitter) = 0;

        /**
         * Method called by the configuration engine to restore instance internal state
         * from configuration file.
         */
        virtual void unserialize (Conf::MappingNode *map) = 0;

        virtual void setAccountDetails (std::map<std::string, std::string> details) = 0;

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
        VoIPLink* getVoIPLink() const {
            return link_;
        }

        virtual void setVoIPLink () = 0;

        /**
         * Register the underlying VoIPLink. Launch the event listener.
         * This should update the getRegistrationState() return value.
         */
        virtual int registerVoIPLink() = 0;

        /**
         * Unregister the underlying VoIPLink. Stop the event listener.
         * This should update the getRegistrationState() return value.
         */
        virtual int unregisterVoIPLink() = 0;

        /**
         * Tell if the account is enable or not.
         * @return true if enabled
         *	     false otherwise
         */
        bool isEnabled() const {
            return enabled_;
        }

        void setEnabled (bool enable) {
            enabled_ = enable;
        }

        /**
         * Get the registration state of the specified link
         * @return RegistrationState	The registration state of underlying VoIPLink
         */
        RegistrationState getRegistrationState() const {
            return registrationState_;
        }

        /**
         * Set the registration state of the specified link
         * @param state	The registration state of underlying VoIPLink
         */
        void setRegistrationState (const RegistrationState &state);

        /**
         * Set the latest up-to-date state code
         * for that account. These codes are
         * those used in SIP and IAX (eg. 200, 500 ...)
         * @param state The Code:Description state
         * @return void
         */
        void setRegistrationStateDetailed (std::pair<int, std::string> state) {
            registrationStateDetailed_ = state;
        }

        /**
         * Get the latest up-to-date state code
         * for that account. These codes are
         * those used in SIP and IAX (eg. 200, 500 ...)
         * @param void
         * @return std::pair<int, std::string> A Code:Description state
         */
        std::pair<int, std::string> getRegistrationStateDetailed (void) const {
            return registrationStateDetailed_;
        }

        /* They should be treated like macro definitions by the C++ compiler */
        std::string getUsername (void) const {
            return username_;
        }

        void setUsername (const std::string &username) {
            username_ = username;
        }

        std::string getHostname (void) const {
            return hostname_;
        }
        void setHostname (const std::string &hostname) {
            hostname_ = hostname;
        }

        std::string getAlias (void) const {
            return alias_;
        }
        void setAlias (const std::string &alias) {
            alias_ = alias;
        }

        std::string getType (void) const {
            return type_;
        }
        void setType (const std::string &type) {
            type_ = type;
        }

        /**
         * Accessor to data structures
         * @return CodecOrder& The list that reflects the user's choice
         */
        const CodecOrder& getActiveCodecs (void) const {
            return codecOrder_;
        }

        /**
         * Update both the codec order structure and the codec string used for
         * SDP offer and configuration respectively
         */
        void setActiveCodecs (const std::vector <std::string>& list);

        std::string getRingtonePath (void) const {
            return ringtonePath_;
        }
        void setRingtonePath (const std::string &path) {
            ringtonePath_ = path;
        }

        bool getRingtoneEnabled (void) const {
            return ringtoneEnabled_;
        }
        void setRingtoneEnabled (bool enable) {
            ringtoneEnabled_ = enable;
        }

        std::string getDisplayName (void) const {
            return displayName_;
        }
        void setDisplayName (const std::string &name) {
            displayName_ = name;
        }

        std::string getUserAgent (void) const {
            return userAgent_;
        }
        void setUseragent (const std::string &ua) {
            userAgent_ = ua;
        }

        std::string getMailBox (void) const {
            return mailBox_;
        }

        void setMailBox (const std::string &mb) {
            mailBox_ = mb;
        }

    private:
        // copy constructor
        Account (const Account& rh);

        // assignment operator
        Account& operator= (const Account& rh);

        /**
         * Helper function used to load the default codec order from the codec factory
         * setActiveCodecs is called to sync both _codecOrder and _codecStr
         */
        void loadDefaultCodecs (void);

    protected:

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
         * Voice over IP Link contains a listener thread and calls
         */
        VoIPLink* link_;

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

        /*
         * Details about the registration state.
         * This is a protocol Code:Description pair.
         */
        std::pair<int, std::string> registrationStateDetailed_;

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
