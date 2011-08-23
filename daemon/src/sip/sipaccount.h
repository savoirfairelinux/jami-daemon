/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#ifndef SIPACCOUNT_H
#define SIPACCOUNT_H

#include <sstream>

#include "account.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_types.h"
#include "pjsip-ua/sip_regc.h"
#include <vector>
#include <map>

namespace Conf {
    class YamlEmitter;
    class MappingNode;
}
enum DtmfType { OVERRTP, SIPINFO};

#define OVERRTPSTR "overrtp"
#define SIPINFOSTR "sipinfo"

// SIP specific configuration keys
static const char *const expireKey = "expire";
static const char *const interfaceKey = "interface";
static const char *const portKey = "port";
static const char *const publishAddrKey = "publishAddr";
static const char *const publishPortKey = "publishPort";
static const char *const sameasLocalKey = "sameasLocal";
static const char *const resolveOnceKey = "resolveOnce";
static const char *const dtmfTypeKey = "dtmfType";
static const char *const serviceRouteKey = "serviceRoute";

// TODO: write an object to store credential which implement serializable
static const char *const srtpKey = "srtp";
static const char *const srtpEnableKey = "enable";
static const char *const keyExchangeKey = "keyExchange";
static const char *const rtpFallbackKey = "rtpFallback";

// TODO: wirte an object to store zrtp params wich implement serializable
static const char *const zrtpKey = "zrtp";
static const char *const displaySasKey = "displaySas";
static const char *const displaySasOnceKey = "displaySasOnce";
static const char *const helloHashEnabledKey = "helloHashEnabled";
static const char *const notSuppWarningKey = "notSuppWarning";

// TODO: write an object to store tls params which implement serializable
static const char *const tlsKey = "tls";
static const char *const tlsPortKey = "tlsPort";
static const char *const certificateKey = "certificate";
static const char *const calistKey = "calist";
static const char *const ciphersKey = "ciphers";
static const char *const tlsEnableKey = "enable";
static const char *const methodKey = "method";
static const char *const timeoutKey = "timeout";
static const char *const tlsPasswordKey = "password";
static const char *const privateKeyKey = "privateKey";
static const char *const requireCertifKey = "requireCertif";
static const char *const serverKey = "server";
static const char *const verifyClientKey = "verifyClient";
static const char *const verifyServerKey = "verifyServer";

static const char *const stunEnabledKey = "stunEnabled";
static const char *const stunServerKey = "stunServer";

static const char *const credKey = "credential";

class SIPVoIPLink;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object = SIPCall/SIPVoIPLink)
 */

class SIPAccount : public Account
{
    public:
        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPAccount (const std::string& accountID);

        /**
         * Virtual destructor
         */
        virtual ~SIPAccount();
        std::string getUserAgentName() const;
        void setRegistrationStateDetailed (const std::pair<int, std::string> &details) { registrationStateDetailed_ = details; }

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        virtual void setAccountDetails (std::map<std::string, std::string> details);

        virtual std::map<std::string, std::string> getAccountDetails() const;
        std::map<std::string, std::string> getIp2IpDetails (void) const;
        std::map<std::string, std::string> getTlsSettings (void) const;

        /**
         * Special setVoIPLink which increment SipVoIPLink's number of client.
         */
        void setVoIPLink();

        /**
         * Actually useless, since config loading is done in init()
         */
        void loadConfig();

        /**
         * Initialize the SIP voip link with the account parameters and send registration
         */
        int registerVoIPLink();

        /**
         * Send unregistration and clean all related stuff ( calls , thread )
         */
        int unregisterVoIPLink();

        pjsip_cred_info *getCredInfo() const {
            return cred_;
        }

        /**
         * Get the number of credentials defined for
         * this account.
         * @param none
         * @return int The number of credentials set for this account.
         */
        unsigned getCredentialCount (void) const {
            return credentials_.size();
        }

        void setCredentials (const std::vector<std::map<std::string, std::string> >& details);
        const std::vector<std::map<std::string, std::string> > &getCredentials (void);

        bool isResolveOnce (void) const {
            return resolveOnce_;
        }

        /**
         * A client sendings a REGISTER request MAY suggest an expiration
         * interval that indicates how long the client would like the
         * registration to be valid.
         *
         * @return A string describing the expiration value.
         */
        const std::string& getRegistrationExpire (void) const {
            return registrationExpire_;
        }

        /**
         * Setting the Expiration Interval of Contact Addresses.
         *
         * @param A string describing the expiration value.
         */
        void setRegistrationExpire (const std::string &expr) {
            registrationExpire_ = expr;
        }

        bool fullMatch (const std::string& username, const std::string& hostname) const;
        bool userMatch (const std::string& username) const;
        bool hostnameMatch (const std::string& hostname) const;

        /* Registration flag */
        bool isRegister() const {
            return bRegister_;
        }
        void setRegister (bool result) {
            bRegister_ = result;
        }

        /**
         * Get the registration stucture that is used
         * for PJSIP in the registration process.
         * Settings are loaded from configuration file.
         * @param void
         * @return pjsip_regc* A pointer to the registration structure
         */
        pjsip_regc* getRegistrationInfo (void) const {
            return regc_;
        }

        /**
         * Set the registration structure that is used
         * for PJSIP in the registration process;
         * @pram A pointer to the new registration structure
         * @return void
         */
        void setRegistrationInfo (pjsip_regc *regc) {
            regc_ = regc;
        }

        /**
         * @return pjsip_tls_setting structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * TLS transport.
         */
        pjsip_tls_setting * getTlsSetting (void) const {
            return tlsSetting_;
        }

        /**
         * @return pj_str_t , filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * an alternate UDP transport.
         */
        std::string getStunServer (void) const {
            return stunServer_;
        }
        void setStunServer (const std::string &srv) {
            stunServer_ = srv;
        }

        pj_str_t getStunServerName (void) const {
            return stunServerName_;
        }

        /**
         * @return pj_uint8_t structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * an alternate UDP transport.
         */
        pj_uint16_t getStunPort (void) const {
            return stunPort_;
        }
        void setStunPort (pj_uint16_t port) {
            stunPort_ = port;
        }

        /**
         * @return bool Tells if current transport for that
         * account is set to TLS.
         */
        bool isTlsEnabled (void) const {
            return transportType_ == PJSIP_TRANSPORT_TLS;
        }

        /**
         * @return bool Tells if current transport for that
         * account is set to OTHER.
         */
        bool isStunEnabled (void) const {
            return stunEnabled_;
        }

        /**
         * Set wether or not stun is enabled for this account
         */
        void setStunEnabled (bool enabl) {
            stunEnabled_ = enabl;
        }

        /*
         * @return pj_str_t "From" uri based on account information.
         * From RFC3261: "The To header field first and foremost specifies the desired
         * logical" recipient of the request, or the address-of-record of the
         * user or resource that is the target of this request. [...]  As such, it is
         * very important that the From URI not contain IP addresses or the FQDN
         * of the host on which the UA is running, since these are not logical
         * names."
         */
        std::string getFromUri (void) const;

        /*
         * This method adds the correct scheme, hostname and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         * It is expected that "port" is present in the internal hostname_.
         *
         * @return pj_str_t "To" uri based on @param username
         * @param username A string formatted as : "username"
         */
        std::string getToUri (const std::string& username) const;

        /*
         * In the current version of SFLPhone, "srv" uri is obtained in the preformated
         * way: hostname:port. This method adds the correct scheme and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         *
         * @return pj_str_t "server" uri based on @param hostPort
         * @param hostPort A string formatted as : "hostname:port"
         */
        std::string getServerUri (void) const;

        /**
         * @param port Optional port. Otherwise set to the port defined for that account.
         * @param hostname Optional local address. Otherwise set to the hostname defined for that account.
         * @return pj_str_t The contact header based on account information
         */
        std::string getContactHeader (const std::string& address, const std::string& port) const;

        /**
         * Set the interface name on which this account is bound, "default" means
         * that the account is bound to the ANY interafec (0.0.0.0). This method should be
         * when binding the account to a new sip transport only.
         */
        void setLocalInterface (const std::string& interface) {
            interface_ = interface;
        }

        /**
         * Get the local interface name on which this account is bound.
         */
        std::string getLocalInterface (void) const {
            return interface_;
        }

        /**
         * Get a flag which determine the usage in sip headers of either the local
         * IP address and port (_localAddress and localPort_) or to an address set
         * manually (_publishedAddress and publishedPort_).
         */
        bool getPublishedSameasLocal() const {
            return publishedSameasLocal_;
        }

        /**
         * Set a flag which determine the usage in sip headers of either the local
         * IP address and port (_localAddress and localPort_) or to an address set
         * manually (_publishedAddress and publishedPort_).
         */
        void setPublishedSameasLocal (bool published) {
            publishedSameasLocal_ = published;
        }

        /**
         * Get the port on which the transport/listener should use, or is
         * actually using.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getLocalPort (void) const {
            return (pj_uint16_t) localPort_;
        }

        /**
         * Set the new port on which this account is running over.
         * @pram port The port used by this account.
         */
        void setLocalPort (pj_uint16_t port) {
            localPort_ = port;
        }

        /**
         * Get the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getPublishedPort (void) const {
            return (pj_uint16_t) publishedPort_;
        }

        /**
         * Set the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @pram port The port used by this account.
         */
        void setPublishedPort (pj_uint16_t port) {
            publishedPort_ = port;
        }

        /**
             * Get the local port for TLS listener.
             * @return pj_uint16 The port used for that account
             */
        pj_uint16_t getTlsListenerPort (void) const {
            return (pj_uint16_t) tlsListenerPort_;
        }

        /**
         * Set the local port for TLS listener.
         * @pram port The port used for TLS listener.
         */
        void setTlsListenerPort (pj_uint16_t port) {
            tlsListenerPort_ = port;
        }

        /**
         * Get the public IP address set by the user for this account.
         * If this setting is not provided, the local bound adddress
         * will be used.
         * @return std::string The public IPV4 address formatted in the standard dot notation.
         */
        std::string getPublishedAddress (void) const {
            return publishedIpAddress_;
        }

        /**
         * Set the public IP address to be used in Contact header.
         * @param The public IPV4 address in the standard dot notation.
         * @return void
         */
        void setPublishedAddress (const std::string& publishedIpAddress) {
            publishedIpAddress_ = publishedIpAddress;
        }

        std::string getServiceRoute (void) const {
            return serviceRoute_;
        }

        void setServiceRoute (const std::string &route) {
            serviceRoute_ = route;
        }

        pjsip_transport* getAccountTransport (void) const {
            return transport_;
        }

        void setAccountTransport (pjsip_transport *transport) {
        	transport_ = transport;
        }

        DtmfType getDtmfType (void) const {
            return dtmfType_;
        }
        void setDtmfType (DtmfType type) {
            dtmfType_ = type;
        }

        bool getSrtpEnable (void) const {
            return srtpEnabled_;
        }
        void setSrtpEnable (bool enabl) {
            srtpEnabled_ = enabl;
        }

        std::string getSrtpKeyExchange (void) const {
            return srtpKeyExchange_;
        }
        void setSrtpKeyExchange (const std::string &key) {
            srtpKeyExchange_ = key;
        }

        bool getSrtpFallback (void) const {
            return srtpFallback_;
        }
        void setSrtpFallback (bool fallback) {
            srtpFallback_ = fallback;
        }

        void setZrtpDisplaySas (bool sas) {
            zrtpDisplaySas_ = sas;
        }

        void setZrtpDiaplaySasOnce (bool sasonce) {
            zrtpDisplaySasOnce_ = sasonce;
        }

        void setZrtpNotSuppWarning (bool warning) {
            zrtpNotSuppWarning_ = warning;
        }

        bool getZrtpHelloHash (void) const {
            return zrtpHelloHash_;
        }
        void setZrtpHelloHash (bool hellohash) {
            zrtpHelloHash_ = hellohash;
        }

        void setTlsEnable (const std::string &enabl) {
            tlsEnable_ = enabl;
        }

        void setTlsCaListFile (const std::string &calist) {
            tlsCaListFile_ = calist;
        }

        void setTlsCertificateFile (const std::string &cert) {
            tlsCertificateFile_ = cert;
        }

        void setTlsPrivateKeyFile (const std::string &priv) {
            tlsPrivateKeyFile_ = priv;
        }

        void setTlsPassword (const std::string &pass) {
            tlsPassword_ = pass;
        }

        void setTlsMethod (const std::string &meth) {
            tlsMethod_ = meth;
        }

        void setTlsCiphers (const std::string &cipher) {
            tlsCiphers_ = cipher;
        }

        void setTlsServerName (const std::string &name) {
            tlsServerName_ = name;
        }

        void setTlsVerifyServer (bool verif) {
            tlsVerifyServer_ = verif;
        }

        void setTlsVerifyClient (bool verif) {
            tlsVerifyClient_ = verif;
        }

        void setTlsRequireClientCertificate (bool require) {
            tlsRequireClientCertificate_ = require;
        }

        void setTlsNegotiationTimeoutSec (const std::string &timeout) {
            tlsNegotiationTimeoutSec_ = timeout;
        }

        void setTlsNegotiationTimeoutMsec (const std::string &timeout) {
            tlsNegotiationTimeoutMsec_ = timeout;
        }

    private:

        std::vector< std::map<std::string, std::string > > credentials_;

        /**
         * Call specific memory pool initialization size (based on empirical data)
         */
        static const int ACCOUNT_MEMPOOL_INIT_SIZE;

        /**
         * Call specific memory pool incrementation size
         */
        static const int ACCOUNT_MEMPOOL_INC_SIZE;

        /* Maps a string description of the SSL method
         * to the corresponding enum value in pjsip_ssl_method.
         * @param method The string representation
         * @return pjsip_ssl_method The corresponding value in the enum
         */
        pjsip_ssl_method sslMethodStringToPjEnum (const std::string& method);

        /*
         * Initializes tls settings from configuration file.
         *
         */
        void initTlsConfiguration (void);

        /*
         * Initializes STUN config from the config file
         */
        void initStunConfiguration (void);

        /**
         * If username is not provided, as it happens for Direct ip calls,
         * fetch the hostname of the machine on which the program is running
         * onto.
         * @return std::string The machine hostname as returned by pj_gethostname()
         */
        std::string getMachineName (void) const;

        /**
         * If username is not provided, as it happens for Direct ip calls,
         * fetch the Real Name field of the user that is currently
         * running this program.
         * @return std::string The login name under which SFLPhone is running.
         */
        std::string getLoginName (void) const;

        /**
         * List of routes (proxies) used for registration and calls
         */
        std::string routeSet_;

        /**
         * Private pjsip memory pool for accounts
         */
        pj_pool_t *pool_;


        // The pjsip client registration information
        pjsip_regc *regc_;
        // To check if the account is registered
        bool bRegister_;

        // Network settings
        std::string registrationExpire_;

        // interface name on which this account is bound
        std::string interface_;

        // Flag which determine if localIpAddress_ or publishedIpAddress_ is used in
        // sip headers
        bool publishedSameasLocal_;

        std::string publishedIpAddress_;

        pj_uint16_t localPort_;
        pj_uint16_t publishedPort_;

        std::string serviceRoute_;

        /**
         * The global TLS listener port which can be configured through the IP2IP_PROFILE
         */
        pj_uint16_t tlsListenerPort_;

        pjsip_transport_type_e transportType_;

        pjsip_transport* transport_;

        // Special hack that is not here to stay
        // See #1852
        bool resolveOnce_;

        //Credential information
        pjsip_cred_info *cred_;

        // The TLS settings, if tls is chosen as
        // a sip transport.
        pjsip_tls_setting * tlsSetting_;

        // The STUN server name, if applicable for internal use only
        pj_str_t stunServerName_;

        // The STUN server port, if applicable
        pj_uint16_t stunPort_;

        DtmfType dtmfType_;

        std::string tlsEnable_;
        int tlsPort_;
        std::string tlsCaListFile_;
        std::string tlsCertificateFile_;
        std::string tlsPrivateKeyFile_;
        std::string tlsPassword_;
        std::string tlsMethod_;
        std::string tlsCiphers_;
        std::string tlsServerName_;
        bool tlsVerifyServer_;
        bool tlsVerifyClient_;
        bool tlsRequireClientCertificate_;
        std::string tlsNegotiationTimeoutSec_;
        std::string tlsNegotiationTimeoutMsec_;

        std::string stunServer_;
        bool stunEnabled_;

        bool srtpEnabled_;
        std::string srtpKeyExchange_;
        bool srtpFallback_;

        bool zrtpDisplaySas_;
        bool zrtpDisplaySasOnce_;
        bool zrtpHelloHash_;
        bool zrtpNotSuppWarning_;
        /*
        * Details about the registration state.
        * This is a protocol Code:Description pair.
        */
        std::pair<int, std::string> registrationStateDetailed_;
};

#endif
