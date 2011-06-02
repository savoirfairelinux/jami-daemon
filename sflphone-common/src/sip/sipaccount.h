/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include "sipvoiplink.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_types.h"
#include "config/serializable.h"
#include <exception>
#include <map>

enum DtmfType { OVERRTP, SIPINFO};

#define OVERRTPSTR "overrtp"
#define SIPINFOSTR "sipinfo"

// SIP specific configuration keys
const Conf::Key expireKey ("expire");
const Conf::Key interfaceKey ("interface");
const Conf::Key portKey ("port");
const Conf::Key publishAddrKey ("publishAddr");
const Conf::Key publishPortKey ("publishPort");
const Conf::Key sameasLocalKey ("sameasLocal");
const Conf::Key resolveOnceKey ("resolveOnce");
const Conf::Key dtmfTypeKey ("dtmfType");
const Conf::Key serviceRouteKey ("serviceRoute");

// TODO: write an object to store credential which implement serializable
const Conf::Key srtpKey ("srtp");
const Conf::Key srtpEnableKey ("enable");
const Conf::Key keyExchangeKey ("keyExchange");
const Conf::Key rtpFallbackKey ("rtpFallback");

// TODO: wirte an object to store zrtp params wich implement serializable
const Conf::Key zrtpKey ("zrtp");
const Conf::Key displaySasKey ("displaySas");
const Conf::Key displaySasOnceKey ("displaySasOnce");
const Conf::Key helloHashEnabledKey ("helloHashEnabled");
const Conf::Key notSuppWarningKey ("notSuppWarning");

// TODO: write an object to store tls params which implement serializable
const Conf::Key tlsKey ("tls");
const Conf::Key tlsPortKey ("tlsPort");
const Conf::Key certificateKey ("certificate");
const Conf::Key calistKey ("calist");
const Conf::Key ciphersKey ("ciphers");
const Conf::Key tlsEnableKey ("enable");
const Conf::Key methodKey ("method");
const Conf::Key timeoutKey ("timeout");
const Conf::Key tlsPasswordKey ("password");
const Conf::Key privateKeyKey ("privateKey");
const Conf::Key requireCertifKey ("requireCertif");
const Conf::Key serverKey ("server");
const Conf::Key verifyClientKey ("verifyClient");
const Conf::Key verifyServerKey ("verifyServer");

const Conf::Key stunEnabledKey ("stunEnabled");
const Conf::Key stunServerKey ("stunServer");

const Conf::Key credKey ("credential");
const Conf::Key credentialCountKey ("count");

class SIPVoIPLink;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object (SIPCall/SIPVoIPLink)
 */

class SipAccountException : public std::exception
{
    public:
        SipAccountException (const std::string& str="") throw() : errstr (str) {}

        virtual ~SipAccountException() throw() {}

        virtual const char *what() const throw() {
            std::string expt ("SipAccountException occured: ");
            expt.append (errstr);

            return expt.c_str();
        }
    private:
        std::string errstr;

};

class CredentialItem
{
    public:

        std::string username;
        std::string password;
        std::string realm;
};


class Credentials : public Serializable
{
    public:

        Credentials();

        ~Credentials();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        int getCredentialCount (void) const {
            return credentialCount;
        }
        void setCredentialCount (int count) {
            credentialCount = count;
        }

        void setNewCredential (const std::string &username,
                               const std::string &password,
                               const std::string &realm);
        const CredentialItem *getCredential (int index) const;

    private:

        int credentialCount;

        CredentialItem credentialArray[10];

};


class SIPAccount : public Account
{
    public:
        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPAccount (const AccountID& accountID);

        /* Copy Constructor */
        SIPAccount (const SIPAccount& rh);

        /* Assignment Operator */
        SIPAccount& operator= (const SIPAccount& rh);

        /**
         * Virtual destructor
         */
        virtual ~SIPAccount();

        virtual void serialize (Conf::YamlEmitter *emitter);

        virtual void unserialize (Conf::MappingNode *map);

        virtual void setAccountDetails (const std::map<std::string, std::string>& details);

        virtual std::map<std::string, std::string> getAccountDetails() const;

        /**
         * Set route header to appears in sip messages for this account
         */
        void setRouteSet (const std::string &route) {
            _routeSet = route;
        }

        /**
         * Get route header to appear in sip messages for this account
         */
        std::string getRouteSet (void) const {
            return _routeSet;
        }

        /**
         * Special setVoIPLink which increment SipVoIPLink's number of client.
         */
        // void setVoIPLink(VoIPLink *link);
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

        void setCredInfo (pjsip_cred_info *cred) {
            _cred = cred;
        }
        pjsip_cred_info *getCredInfo() const {
            return _cred;
        }

        const std::string& getAuthenticationUsername (void) const {
            return _authenticationUsername;
        }
        void setAuthenticationUsername (const std::string& username) {
            _authenticationUsername = username;
        }

        bool isResolveOnce (void) const {
            return _resolveOnce;
        }
        void setResolveOnce (bool reslv) {
            _resolveOnce = reslv;
        }


        /**
         * A client sendings a REGISTER request MAY suggest an expiration
         * interval that indicates how long the client would like the
         * registration to be valid.
         *
         * @return A string describing the expiration value.
         */
        const std::string& getRegistrationExpire (void) const {
            return _registrationExpire;
        }

        /**
         * Setting the Expiration Interval of Contact Addresses.
         *
         * @param A string describing the expiration value.
         */
        void setRegistrationExpire (const std::string &expr) {
            _registrationExpire = expr;
        }

        bool fullMatch (const std::string& username, const std::string& hostname) const;
        bool userMatch (const std::string& username) const;
        bool hostnameMatch (const std::string& hostname) const;

        /* Registration flag */
        bool isRegister() const {
            return _bRegister;
        }
        void setRegister (bool result) {
            _bRegister = result;
        }

        /**
         * Get the registration stucture that is used
         * for PJSIP in the registration process.
         * Settings are loaded from configuration file.
         * @param void
         * @return pjsip_regc* A pointer to the registration structure
         */
        pjsip_regc* getRegistrationInfo (void) const {
            return _regc;
        }

        /**
         * Set the registration structure that is used
         * for PJSIP in the registration process;
         * @pram A pointer to the new registration structure
         * @return void
         */
        void setRegistrationInfo (pjsip_regc *regc) {
            _regc = regc;
        }

        /**
         * Get the number of credentials defined for
         * this account.
         * @param none
         * @return int The number of credentials set for this account.
         */
        int getCredentialCount (void) const {
            return credentials.getCredentialCount() + 1;
        }
        void setCredentialCount (int count) {
            return credentials.setCredentialCount (count);
        }

        /**
         * @return pjsip_tls_setting structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * TLS transport.
         */
        pjsip_tls_setting * getTlsSetting (void) const {
            return _tlsSetting;
        }

        /**
         * @return pj_str_t , filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * an alternate UDP transport.
         */
        std::string getStunServer (void) const {
            return _stunServer;
        }
        void setStunServer (const std::string &srv) {
            _stunServer = srv;
        }

        pj_str_t getStunServerName (void) const {
            return _stunServerName;
        }

        /**
         * @return pj_uint8_t structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize
         * an alternate UDP transport.
         */
        pj_uint16_t getStunPort (void) const {
            return _stunPort;
        }
        void setStunPort (pj_uint16_t port) {
            _stunPort = port;
        }

        /**
         * @return bool Tells if current transport for that
         * account is set to TLS.
         */
        bool isTlsEnabled (void) const {
            return (_transportType == PJSIP_TRANSPORT_TLS) ? true: false;
        }

        /**
         * @return bool Tells if current transport for that
         * account is set to OTHER.
         */
        bool isStunEnabled (void) const {
            return _stunEnabled;
        }

        /**
         * Set wether or not stun is enabled for this account
         */
        void setStunEnabled (bool enabl) {
            _stunEnabled = enabl;
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
         * It is expected that "port" is present in the internal _hostname.
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
            _interface = interface;
        }

        /**
         * Get the local interface name on which this account is bound.
         */
        std::string getLocalInterface (void) const {
            return _interface;
        }

        /**
         * Get a flag which determine the usage in sip headers of either the local
         * IP address and port (_localAddress and _localPort) or to an address set
         * manually (_publishedAddress and _publishedPort).
         */
        bool getPublishedSameasLocal() const {
            return _publishedSameasLocal;
        }

        /**
         * Set a flag which determine the usage in sip headers of either the local
         * IP address and port (_localAddress and _localPort) or to an address set
         * manually (_publishedAddress and _publishedPort).
         */
        void setPublishedSameasLocal (bool published) {
            _publishedSameasLocal = published;
        }

        /**
         * Get the port on which the transport/listener should use, or is
         * actually using.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getLocalPort (void) const {
            return (pj_uint16_t) _localPort;
        }

        /**
         * Set the new port on which this account is running over.
         * @pram port The port used by this account.
         */
        void setLocalPort (pj_uint16_t port) {
            _localPort = port;
        }

        /**
         * Get the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @return pj_uint16 The port used for that account
         */
        pj_uint16_t getPublishedPort (void) const {
            return (pj_uint16_t) _publishedPort;
        }

        /**
         * Set the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @pram port The port used by this account.
         */
        void setPublishedPort (pj_uint16_t port) {
            _publishedPort = port;
        }

        /**
             * Get the local port for TLS listener.
             * @return pj_uint16 The port used for that account
             */
        pj_uint16_t getTlsListenerPort (void) const {
            return (pj_uint16_t) _tlsListenerPort;
        }

        /**
         * Set the local port for TLS listener.
         * @pram port The port used for TLS listener.
         */
        void setTlsListenerPort (pj_uint16_t port) {
            _tlsListenerPort = port;
        }

        /**
         * Get the public IP address set by the user for this account.
         * If this setting is not provided, the local bound adddress
         * will be used.
         * @return std::string The public IPV4 address formatted in the standard dot notation.
         */
        std::string getPublishedAddress (void) const {
            return _publishedIpAddress;
        }

        /**
         * Set the public IP address to be used in Contact header.
         * @param The public IPV4 address in the standard dot notation.
         * @return void
         */
        void setPublishedAddress (const std::string& publishedIpAddress) {
            _publishedIpAddress = publishedIpAddress;
        }

        std::string getServiceRoute (void) const {
            return _serviceRoute;
        }

        void setServiceRoute (const std::string &route) {
            _serviceRoute = route;
        }

        /**
         * Get the chosen transport type.
         * @return pjsip_transport_type_e Transport type chosen by the user for this account.
         */
        pjsip_transport_type_e getTransportType (void) const {
            return _transportType;
        }

        pjsip_transport* getAccountTransport (void) const {
            return _transport;
        }

        void setAccountTransport (pjsip_transport *transport) {
            _transport = transport;
        }

        std::string getTransportMapKey (void) const;

        DtmfType getDtmfType (void) const {
            return _dtmfType;
        }
        void setDtmfType (DtmfType type) {
            _dtmfType = type;
        }

        bool getSrtpEnable (void) const {
            return _srtpEnabled;
        }
        void setSrtpEnable (bool enabl) {
            _srtpEnabled = enabl;
        }

        std::string getSrtpKeyExchange (void) const {
            return _srtpKeyExchange;
        }
        void setSrtpKeyExchange (const std::string &key) {
            _srtpKeyExchange = key;
        }

        bool getSrtpFallback (void) const {
            return _srtpFallback;
        }
        void setSrtpFallback (bool fallback) {
            _srtpFallback = fallback;
        }

        bool getZrtpDisplaySas (void) const {
            return _zrtpDisplaySas;
        }
        void setZrtpDisplaySas (bool sas) {
            _zrtpDisplaySas = sas;
        }

        bool getZrtpDiaplaySasOnce (void) const {
            return _zrtpDisplaySasOnce;
        }
        void setZrtpDiaplaySasOnce (bool sasonce) {
            _zrtpDisplaySasOnce = sasonce;
        }

        bool getZrtpNotSuppWarning (void) const {
            return _zrtpNotSuppWarning;
        }
        void setZrtpNotSuppWarning (bool warning) {
            _zrtpNotSuppWarning = warning;
        }

        bool getZrtpHelloHash (void) const {
            return _zrtpHelloHash;
        }
        void setZrtpHelloHash (bool hellohash) {
            _zrtpHelloHash = hellohash;
        }
        // void setSrtpKeyExchange

        std::string getRealm (void) const {
            return _realm;
        }
        void setRealm (const std::string &r) {
            _realm = r;
        }

        std::string getTlsEnable (void) const {
            return _tlsEnable;
        }
        void setTlsEnable (const std::string &enabl) {
            _tlsEnable = enabl;
        }

        std::string getTlsCaListFile (void) const {
            return _tlsCaListFile;
        }
        void setTlsCaListFile (const std::string &calist) {
            _tlsCaListFile = calist;
        }

        std::string getTlsCertificateFile (void) const {
            return _tlsCertificateFile;
        }
        void setTlsCertificateFile (const std::string &cert) {
            _tlsCertificateFile = cert;
        }

        std::string getTlsPrivateKeyFile (void) const {
            return _tlsPrivateKeyFile;
        }
        void setTlsPrivateKeyFile (const std::string &priv) {
            _tlsPrivateKeyFile = priv;
        }

        std::string getTlsPassword (void) const {
            return _tlsPassword;
        }
        void setTlsPassword (const std::string &pass) {
            _tlsPassword = pass;
        }

        std::string getTlsMethod (void) const {
            return _tlsMethod;
        }
        void setTlsMethod (const std::string &meth) {
            _tlsMethod = meth;
        }

        std::string getTlsCiphers (void) const {
            return _tlsCiphers;
        }
        void setTlsCiphers (const std::string &cipher) {
            _tlsCiphers = cipher;
        }

        std::string getTlsServerName (void) const {
            return _tlsServerName;
        }
        void setTlsServerName (const std::string &name) {
            _tlsServerName = name;
        }

        bool getTlsVerifyServer (void) const {
            return _tlsVerifyServer;
        }
        void setTlsVerifyServer (bool verif) {
            _tlsVerifyServer = verif;
        }

        bool getTlsVerifyClient (void) const {
            return _tlsVerifyClient;
        }
        void setTlsVerifyClient (bool verif) {
            _tlsVerifyClient = verif;
        }

        bool getTlsRequireClientCertificate (void) const {
            return _tlsRequireClientCertificate;
        }
        void setTlsRequireClientCertificate (bool require) {
            _tlsRequireClientCertificate = require;
        }

        std::string getTlsNegotiationTimeoutSec (void) const {
            return _tlsNegotiationTimeoutSec;
        }
        void setTlsNegotiationTimeoutSec (const std::string &timeout) {
            _tlsNegotiationTimeoutSec = timeout;
        }

        std::string getTlsNegotiationTimeoutMsec (void) const {
            return _tlsNegotiationTimeoutMsec;
        }
        void setTlsNegotiationTimeoutMsec (const std::string &timeout) {
            _tlsNegotiationTimeoutMsec = timeout;
        }

    private:

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

        /*
         * Initializes set of additional credentials, if supplied by the user.
         */
        int initCredential (void);

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
        std::string _routeSet;

        /**
         * Private pjsip memory pool for accounts
         */
        pj_pool_t *_pool;


        // The pjsip client registration information
        pjsip_regc *_regc;
        // To check if the account is registered
        bool _bRegister;

        // Network settings
        std::string _registrationExpire;

        // interface name on which this account is bound
        std::string _interface;

        // Flag which determine if _localIpAddress or _publishedIpAddress is used in
        // sip headers
        bool _publishedSameasLocal;

        std::string _publishedIpAddress;

        pj_uint16_t _localPort;
        pj_uint16_t _publishedPort;

        std::string _serviceRoute;

        /**
         * The global TLS listener port which can be configured through the IP2IP_PROFILE
         */
        pj_uint16_t _tlsListenerPort;

        pjsip_transport_type_e _transportType;

        pjsip_transport* _transport;

        // Special hack that is not here to stay
        // See #1852
        bool _resolveOnce;

        //Credential information
        pjsip_cred_info *_cred;
        std::string _realm;
        std::string _authenticationUsername;
        Credentials credentials;

        // The TLS settings, if tls is chosen as
        // a sip transport.
        pjsip_tls_setting * _tlsSetting;

        // The STUN server name, if applicable for internal use only
        pj_str_t _stunServerName;

        // The STUN server port, if applicable
        pj_uint16_t _stunPort;

        DtmfType _dtmfType;

        std::string _tlsEnable;
        std::string _tlsPortStr;
        std::string _tlsCaListFile;
        std::string _tlsCertificateFile;
        std::string _tlsPrivateKeyFile;
        std::string _tlsPassword;
        std::string _tlsMethod;
        std::string _tlsCiphers;
        std::string _tlsServerName;
        bool _tlsVerifyServer;
        bool _tlsVerifyClient;
        bool _tlsRequireClientCertificate;
        std::string _tlsNegotiationTimeoutSec;
        std::string _tlsNegotiationTimeoutMsec;

        std::string _stunServer;
        bool _stunEnabled;

        bool _srtpEnabled;
        std::string _srtpKeyExchange;
        bool _srtpFallback;

        bool _zrtpDisplaySas;
        bool _zrtpDisplaySasOnce;
        bool _zrtpHelloHash;
        bool _zrtpNotSuppWarning;


};

#endif
