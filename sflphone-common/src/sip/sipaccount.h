/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
const Conf::Key expireKey("expire");
const Conf::Key interfaceKey("interface");
const Conf::Key portKey("port");
const Conf::Key publishAddrKey("publishAddr");
const Conf::Key publishPortKey("publishPort");
const Conf::Key sameasLocalKey("sameasLocal");
const Conf::Key resolveOnceKey("resolveOnce");
const Conf::Key dtmfTypeKey("dtmfType");
const Conf::Key serviceRouteKey("serviceRoute");

// TODO: write an object to store credential which implement serializable
const Conf::Key srtpKey("srtp");
const Conf::Key srtpEnableKey("enable");
const Conf::Key keyExchangeKey("keyExchange");
const Conf::Key rtpFallbackKey("rtpFallback");

// TODO: wirte an object to store zrtp params wich implement serializable
const Conf::Key zrtpKey("zrtp");
const Conf::Key displaySasKey("displaySas");
const Conf::Key displaySasOnceKey("displaySasOnce");
const Conf::Key helloHashEnabledKey("helloHashEnabled");
const Conf::Key notSuppWarningKey("notSuppWarning");

// TODO: write an object to store tls params which implement serializable
const Conf::Key tlsKey("tls");
const Conf::Key tlsPortKey("tlsPort");
const Conf::Key certificateKey("certificate");
const Conf::Key calistKey("calist");
const Conf::Key ciphersKey("ciphers");
const Conf::Key tlsEnableKey("enable");
const Conf::Key methodKey("method");
const Conf::Key timeoutKey("timeout");
const Conf::Key tlsPasswordKey("password");
const Conf::Key privateKeyKey("privateKey");
const Conf::Key requireCertifKey("requireCertif");
const Conf::Key serverKey("server");
const Conf::Key verifyClientKey("verifyClient");
const Conf::Key verifyServerKey("verifyServer");

const Conf::Key stunEnabledKey("stunEnabled");
const Conf::Key stunServerKey("stunServer");

const Conf::Key credKey("credential");
const Conf::Key credentialCountKey("count");

class SIPVoIPLink;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object (SIPCall/SIPVoIPLink)
 */

class SipAccountException : public std::exception
{
 public:
  SipAccountException(const std::string& str="") throw() : errstr(str) {}

  virtual ~SipAccountException() throw() {}

  virtual const char *what() const throw() {
    std::string expt("SipAccountException occured: ");
    expt.append(errstr);

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

  virtual void serialize(Conf::YamlEmitter *emitter);

  virtual void unserialize(Conf::MappingNode *map);

  int getCredentialCount(void) { return credentialCount; }
  void setCredentialCount(int count) { credentialCount = count; }

  void setNewCredential(std::string username, std::string password, std::string realm);
  CredentialItem *getCredential(int index);

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
        SIPAccount(const AccountID& accountID);

        /* Copy Constructor */
        SIPAccount(const SIPAccount& rh);

        /* Assignment Operator */
        SIPAccount& operator=( const SIPAccount& rh);

        /**
         * Virtual destructor
         */
        virtual ~SIPAccount();

	virtual void serialize(Conf::YamlEmitter *emitter);

	virtual void unserialize(Conf::MappingNode *map);

	virtual void setAccountDetails(const std::map<std::string, std::string>& details);

	virtual std::map<std::string, std::string> getAccountDetails();

	/**
	 * Set route header to appears in sip messages for this account
	 */ 
	void setRouteSet(std::string route) { _routeSet = route; }

	/**
	 * Get route header to appear in sip messages for this account
	 */ 
	std::string getRouteSet(void) { return _routeSet; }

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

        inline void setCredInfo(pjsip_cred_info *cred) {_cred = cred;}
        inline pjsip_cred_info *getCredInfo() {return _cred;}
              
        inline std::string& getAuthenticationUsername(void) { return _authenticationUsername; }
        inline void setAuthenticationUsername(const std::string& username) { _authenticationUsername = username; }
        
        inline bool isResolveOnce(void) { return _resolveOnce; }
	void setResolveOnce(bool reslv) { _resolveOnce = reslv; }
        

	/**
	 * A client sendings a REGISTER request MAY suggest an expiration
	 * interval that indicates how long the client would like the
	 * registration to be valid.
	 *
	 * @return A string describing the expiration value.
	 */
	inline std::string& getRegistrationExpire(void) { return _registrationExpire; }

	/**
	 * Setting the Expiration Interval of Contact Addresses.
	 *
	 * @param A string describing the expiration value.
	 */ 
	inline void setRegistrationExpire(std::string expr) { _registrationExpire = expr; }

        bool fullMatch(const std::string& username, const std::string& hostname);
        bool userMatch(const std::string& username);
        bool hostnameMatch(const std::string& hostname);
        
        /* Registration flag */
        bool isRegister() {return _bRegister;}
        void setRegister(bool result) {_bRegister = result;}        
        
        /**
         * Get the registration stucture that is used 
         * for PJSIP in the registration process.
         * Settings are loaded from configuration file.
         * @param void
         * @return pjsip_regc* A pointer to the registration structure
         */
        pjsip_regc* getRegistrationInfo( void ) { return _regc; }
        
        /** 
         * Set the registration structure that is used
         * for PJSIP in the registration process;
         * @pram A pointer to the new registration structure
         * @return void
         */
        void setRegistrationInfo( pjsip_regc *regc ) { _regc = regc; }

        /**
         * Get the number of credentials defined for 
         * this account.
         * @param none
         * @return int The number of credentials set for this account.
         */
        inline int getCredentialCount(void) { return credentials.getCredentialCount() + 1; }
	inline void setCredentialCount(int count) { return credentials.setCredentialCount(count); }
                
        /**
         * @return pjsip_tls_setting structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize 
         * TLS transport.
         */
        inline pjsip_tls_setting * getTlsSetting(void) { return _tlsSetting; }
		
		/**
         * @return pj_str_t , filled from the configuration
         * file, that can be used directly by PJSIP to initialize 
         * an alternate UDP transport.
         */
        inline std::string getStunServer(void) { return _stunServer; }
	inline void setStunServer (std::string srv) { _stunServer = srv; }

	inline pj_str_t getStunServerName(void) { return _stunServerName; }

		/**
         * @return pj_uint8_t structure, filled from the configuration
         * file, that can be used directly by PJSIP to initialize 
         * an alternate UDP transport.
         */
        inline pj_uint16_t getStunPort (void) { return _stunPort; }
	inline void setStunPort (pj_uint16_t port) { _stunPort = port; }
        
        /**
         * @return bool Tells if current transport for that 
         * account is set to TLS.
         */
        inline bool isTlsEnabled(void) { return (_transportType == PJSIP_TRANSPORT_TLS) ? true: false; }
		
	/**
         * @return bool Tells if current transport for that 
         * account is set to OTHER.
         */
        inline bool isStunEnabled(void) { return (_transportType == PJSIP_TRANSPORT_START_OTHER) ? true: false; }
	inline void setStunEnabled(bool enabl) { _stunEnabled = enabl; }
                
        /*
         * @return pj_str_t "From" uri based on account information.
         * From RFC3261: "The To header field first and foremost specifies the desired
         * logical" recipient of the request, or the address-of-record of the
         * user or resource that is the target of this request. [...]  As such, it is
         * very important that the From URI not contain IP addresses or the FQDN
         * of the host on which the UA is running, since these are not logical
         * names."
         */
        std::string getFromUri(void);
        
        /*
         * This method adds the correct scheme, hostname and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         * It is expected that "port" is present in the internal _hostname.
         *
         * @return pj_str_t "To" uri based on @param username
         * @param username A string formatted as : "username"
         */
        std::string getToUri(const std::string& username);

        /*
         * In the current version of SFLPhone, "srv" uri is obtained in the preformated 
         * way: hostname:port. This method adds the correct scheme and append
         * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
         *
         * @return pj_str_t "server" uri based on @param hostPort
         * @param hostPort A string formatted as : "hostname:port"
         */
        std::string getServerUri(void);
               
        /**
         * @param port Optional port. Otherwise set to the port defined for that account.
         * @param hostname Optional local address. Otherwise set to the hostname defined for that account.
         * @return pj_str_t The contact header based on account information
         */
        std::string getContactHeader(const std::string& address, const std::string& port);

	/**
	 * Set the interface name on which this account is bound, "default" means 
	 * that the account is bound to the ANY interafec (0.0.0.0). This method should be
	 * when binding the account to a new sip transport only.
	 */
	inline void setLocalInterface(const std::string& interface) {_interface = interface;}

	/**
	 * Get the local interface name on which this account is bound.
	 */
	inline std::string getLocalInterface(void) { return _interface; }

	/**
	 * Get a flag which determine the usage in sip headers of either the local 
	 * IP address and port (_localAddress and _localPort) or to an address set 
	 * manually (_publishedAddress and _publishedPort). 
	 */ 
	bool getPublishedSameasLocal(){ return _publishedSameasLocal; }

	/**
	 * Set a flag which determine the usage in sip headers of either the local 
	 * IP address and port (_localAddress and _localPort) or to an address set 
	 * manually (_publishedAddress and _publishedPort). 
	 */ 
	void setPublishedSameasLocal(bool published){ _publishedSameasLocal = published; }

        /**
         * Get the port on which the transport/listener should use, or is
         * actually using.
         * @return pj_uint16 The port used for that account
         */   
        inline pj_uint16_t getLocalPort(void) { return (pj_uint16_t) _localPort; }
        
        /** 
         * Set the new port on which this account is running over.
         * @pram port The port used by this account.
         */
        inline void setLocalPort(pj_uint16_t port) { _localPort = port; }
                
        /**
         * Get the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @return pj_uint16 The port used for that account
         */   
        inline pj_uint16_t getPublishedPort(void) { return (pj_uint16_t) _publishedPort; }
        
        /** 
         * Set the published port, which is the port to be advertised as the port
         * for the chosen SIP transport.
         * @pram port The port used by this account.
         */
        inline void setPublishedPort(pj_uint16_t port) { _publishedPort = port; }

	/**
         * Get the local port for TLS listener.
         * @return pj_uint16 The port used for that account
         */   
        inline pj_uint16_t getTlsListenerPort(void) { return (pj_uint16_t) _tlsListenerPort; }
        
        /** 
         * Set the local port for TLS listener.
         * @pram port The port used for TLS listener.
         */
        inline void setTlsListenerPort(pj_uint16_t port) { _tlsListenerPort = port; }
                
        /**
         * Get the public IP address set by the user for this account.
         * If this setting is not provided, the local bound adddress
         * will be used.
         * @return std::string The public IPV4 address formatted in the standard dot notation.
         */
        inline std::string getPublishedAddress(void) { return _publishedIpAddress; }
        
        /**
         * Set the public IP address to be used in Contact header.
         * @param The public IPV4 address in the standard dot notation.
         * @return void
         */
        inline void setPublishedAddress(const std::string& publishedIpAddress) { _publishedIpAddress = publishedIpAddress; }

	inline std::string getServiceRoute(void) { return _serviceRoute; }

	inline void setServiceRoute(std::string route) { _serviceRoute = route; }
        
        /**
         * Get the chosen transport type.
         * @return pjsip_transport_type_e Transport type chosen by the user for this account.
         */
        inline pjsip_transport_type_e getTransportType(void) { return _transportType; }
        
        inline pjsip_transport* getAccountTransport (void) { return _transport; }

        inline void setAccountTransport (pjsip_transport *transport) { _transport = transport; }

        std::string getTransportMapKey(void);

        DtmfType getDtmfType(void) { return _dtmfType; }
        void setDtmfType(DtmfType type) { _dtmfType = type; }

	bool getSrtpEnable(void) { return _srtpEnabled; }
	void setSrtpEnable(bool enabl) { _srtpEnabled = enabl; }

	std::string getSrtpKeyExchange(void) { return _srtpKeyExchange; }
	void setSrtpKeyExchange(std::string key) { _srtpKeyExchange = key; }

	bool getSrtpFallback(void) { return _srtpFallback; }
	void setSrtpFallback(bool fallback) { _srtpFallback = fallback; }
	
	bool getZrtpDisplaySas(void) { return _zrtpDisplaySas; }
	void setZrtpDisplaySas(bool sas) { _zrtpDisplaySas = sas; }

	bool getZrtpDiaplaySasOnce(void) { return _zrtpDisplaySasOnce; }
	void setZrtpDiaplaySasOnce(bool sasonce) { _zrtpDisplaySasOnce = sasonce; }

	bool getZrtpNotSuppWarning(void) { return _zrtpNotSuppWarning; }
	void setZrtpNotSuppWarning(bool warning) { _zrtpNotSuppWarning = warning; }

	bool getZrtpHelloHash(void) { return _zrtpHelloHash; }
	void setZrtpHelloHash(bool hellohash) { _zrtpHelloHash = hellohash; }
	// void setSrtpKeyExchange

	std::string getRealm(void) { return _realm; }
	void setRealm(std::string r) { _realm = r; }

	std::string getTlsEnable(void) {return _tlsEnable; }
	void setTlsEnable(std::string enabl) { _tlsEnable = enabl; }

	std::string getTlsCaListFile(void) { return _tlsCaListFile; }
	void setTlsCaListFile(std::string calist) { _tlsCaListFile = calist; }
 
	std::string getTlsCertificateFile(void) { return _tlsCertificateFile; }
	void setTlsCertificateFile(std::string cert) { _tlsCertificateFile = cert; }

	std::string getTlsPrivateKeyFile(void) { return _tlsPrivateKeyFile; }
	void setTlsPrivateKeyFile(std::string priv) { _tlsPrivateKeyFile = priv; }

	std::string getTlsPassword(void) { return _tlsPassword; }
	void setTlsPassword(std::string pass) { _tlsPassword = pass; }

	std::string getTlsMethod(void) { return _tlsMethod; }
	void setTlsMethod(std::string meth) { _tlsMethod = meth; }

	std::string getTlsCiphers(void) { return _tlsCiphers; }
	void setTlsCiphers(std::string cipher) { _tlsCiphers = cipher; }

	std::string getTlsServerName(void) { return _tlsServerName; }
	void setTlsServerName(std::string name) { _tlsServerName = name; }

	bool getTlsVerifyServer(void) { return _tlsVerifyServer; }
	void setTlsVerifyServer(bool verif) { _tlsVerifyServer = verif; }

	bool getTlsVerifyClient(void) { return _tlsVerifyClient; }
	void setTlsVerifyClient(bool verif) { _tlsVerifyClient = verif; }

	bool getTlsRequireClientCertificate(void) { return _tlsRequireClientCertificate; }
	void setTlsRequireClientCertificate(bool require) { _tlsRequireClientCertificate = require; }

	std::string getTlsNegotiationTimeoutSec(void) { return _tlsNegotiationTimeoutSec; }
	void setTlsNegotiationTimeoutSec(std::string timeout) { _tlsNegotiationTimeoutSec = timeout; }

	std::string getTlsNegotiationTimeoutMsec(void) { return _tlsNegotiationTimeoutMsec; }
	void setTlsNegotiationTimeoutMsec(std::string timeout) { _tlsNegotiationTimeoutMsec = timeout; }

  private: 

        /* Maps a string description of the SSL method 
         * to the corresponding enum value in pjsip_ssl_method.
         * @param method The string representation 
         * @return pjsip_ssl_method The corresponding value in the enum
         */
        pjsip_ssl_method sslMethodStringToPjEnum(const std::string& method);
    
        /*
         * Initializes tls settings from configuration file.
         *
         */  
        void initTlsConfiguration(void);  

	/*
	 * Initializes STUN config from the config file
	 */
	void initStunConfiguration (void);
 
        /*
         * Initializes set of additional credentials, if supplied by the user.
         */
        int initCredential(void);       
        
        /**
         * If username is not provided, as it happens for Direct ip calls, 
         * fetch the hostname of the machine on which the program is running
         * onto.
         * @return std::string The machine hostname as returned by pj_gethostname()
         */
        std::string getMachineName(void);
        
        /**
         * If username is not provided, as it happens for Direct ip calls, 
         * fetch the Real Name field of the user that is currently 
         * running this program. 
         * @return std::string The login name under which SFLPhone is running.
         */ 
        std::string getLoginName(void);

	std::string _routeSet;
              

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

	bool _tlsEnabled;
	bool _stunEnabled;

	// std::string _routeset;

	// std::string _realm;
	// std::string _tlsListenerPort;
	// std::string _routeSet;
	// std::string _dtmfType;


	bool _srtpEnabled;
	std::string _srtpKeyExchange;
	bool _srtpFallback;

	bool _zrtpDisplaySas;
	bool _zrtpDisplaySasOnce;
	bool _zrtpHelloHash;
	bool _zrtpNotSuppWarning;


};

#endif
