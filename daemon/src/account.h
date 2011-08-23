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
#include "config/config.h"
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
    ErrorConfStun,
    NumberOfState
};

// Account identifier
static const char *const ACCOUNT_ID                          = "Account.id";

// Common account parameters
static const char *const CONFIG_ACCOUNT_TYPE                 = "Account.type";
static const char *const CONFIG_ACCOUNT_ALIAS                = "Account.alias";
static const char *const CONFIG_ACCOUNT_MAILBOX	             = "Account.mailbox";
static const char *const CONFIG_ACCOUNT_ENABLE               = "Account.enable";
static const char *const CONFIG_ACCOUNT_RESOLVE_ONCE         = "Account.resolveOnce";
static const char *const CONFIG_ACCOUNT_REGISTRATION_EXPIRE  = "Account.expire";
static const char *const CONFIG_CREDENTIAL_NUMBER            = "Credential.count";
static const char *const ACCOUNT_DTMF_TYPE                   = "Account.dtmfType";
static const char *const CONFIG_RINGTONE_PATH                = "Account.ringtonePath";
static const char *const CONFIG_RINGTONE_ENABLED             = "Account.ringtoneEnabled";

static const char *const HOSTNAME                            = "hostname";
static const char *const USERNAME                            = "username";
static const char *const ROUTESET                            = "routeset";
static const char *const PASSWORD                            = "password";
static const char *const REALM                               = "realm";
static const char *const DEFAULT_REALM                       = "*";
static const char *const USERAGENT							 = "useragent";

static const char *const LOCAL_INTERFACE                     = "Account.localInterface";
static const char *const PUBLISHED_SAMEAS_LOCAL              = "Account.publishedSameAsLocal";
static const char *const LOCAL_PORT                          = "Account.localPort";
static const char *const PUBLISHED_PORT                      = "Account.publishedPort";
static const char *const PUBLISHED_ADDRESS                   = "Account.publishedAddress";

static const char *const DISPLAY_NAME                        = "Account.displayName";
static const char *const DEFAULT_ADDRESS                     = "0.0.0.0";

// SIP specific parameters
static const char *const SIP_PROXY                           = "SIP.proxy";
static const char *const STUN_SERVER						 = "STUN.server";
static const char *const STUN_ENABLE						 = "STUN.enable";

// SRTP specific parameters
static const char *const SRTP_ENABLE                         = "SRTP.enable";
static const char *const SRTP_KEY_EXCHANGE                   = "SRTP.keyExchange";
static const char *const SRTP_ENCRYPTION_ALGO                = "SRTP.encryptionAlgorithm";  // Provided by ccRTP,0=NULL,1=AESCM,2=AESF8
static const char *const SRTP_RTP_FALLBACK                   = "SRTP.rtpFallback";
static const char *const ZRTP_HELLO_HASH                     = "ZRTP.helloHashEnable";
static const char *const ZRTP_DISPLAY_SAS                    = "ZRTP.displaySAS";
static const char *const ZRTP_NOT_SUPP_WARNING               = "ZRTP.notSuppWarning";
static const char *const ZRTP_DISPLAY_SAS_ONCE               = "ZRTP.displaySasOnce";

static const char *const TLS_LISTENER_PORT                   = "TLS.listenerPort";
static const char *const TLS_ENABLE                          = "TLS.enable";
static const char *const TLS_CA_LIST_FILE                    = "TLS.certificateListFile";
static const char *const TLS_CERTIFICATE_FILE                = "TLS.certificateFile";
static const char *const TLS_PRIVATE_KEY_FILE                = "TLS.privateKeyFile";
static const char *const TLS_PASSWORD                        = "TLS.password";
static const char *const TLS_METHOD                          = "TLS.method";
static const char *const TLS_CIPHERS                         = "TLS.ciphers";
static const char *const TLS_SERVER_NAME                     = "TLS.serverName";
static const char *const TLS_VERIFY_SERVER                   = "TLS.verifyServer";
static const char *const TLS_VERIFY_CLIENT                   = "TLS.verifyClient";
static const char *const TLS_REQUIRE_CLIENT_CERTIFICATE      = "TLS.requireClientCertificate";
static const char *const TLS_NEGOTIATION_TIMEOUT_SEC         = "TLS.negotiationTimeoutSec";
static const char *const TLS_NEGOTIATION_TIMEOUT_MSEC        = "TLS.negotiationTimemoutMsec";

static const char *const REGISTRATION_STATUS                 = "Status";
static const char *const REGISTRATION_STATE_CODE             = "Registration.code";
static const char *const REGISTRATION_STATE_DESCRIPTION      = "Registration.description";

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
static const char * const videocodecsKey = "videocodecs";
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
         * Set the registration state of the specified link
         * @param state	The registration state of underlying VoIPLink
         */
        void setRegistrationState (const RegistrationState &state);

        /* They should be treated like macro definitions by the C++ compiler */
        std::string getUsername (void) const {
            return username_;
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
         * @return std::vector<std::string>& The list that reflects the user's choice
         */
        const std::vector<std::string>& getActiveVideoCodecs (void) const {
            return videoCodecOrder_;
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
        void setActiveVideoCodecs (const std::vector <std::string>& list);

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
         * setActiveCodecs is called to sync both codecOrder_ and codecStr_
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

        /**
         * Vector containing the order of the codecs
         */
        CodecOrder codecOrder_;
      
        /**
         * Vector containing the order of the video codecs
         */
        std::vector<std::string> videoCodecOrder_;

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
