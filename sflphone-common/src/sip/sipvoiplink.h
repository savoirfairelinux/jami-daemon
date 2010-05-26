/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef SIPVOIPLINK_H
#define SIPVOIPLINK_H

#include "voiplink.h"
#include "hooks/urlhook.h"


//////////////////////////////
/* PJSIP imports */
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath/stun_config.h>
///////////////////////////////

#include <map>
#include <sstream>

class EventThread;
class SIPCall;

#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2
#define RANDOM_SIP_PORT   rand() % 64000 + 1024

// To set the verbosity. From 0 (min) to 6 (max)
#define PJ_LOG_LEVEL 0

#define SipTransportMap std::map<std::string, pjsip_transport*>

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink : public VoIPLink
{
    public:

        /**
         * Singleton method. Enable to retrieve the unique static instance
         * @return SIPVoIPLink* A pointer on the object
         */
        static SIPVoIPLink* instance( const AccountID& id );

        /**
         * Destructor
         */
        ~SIPVoIPLink();

        /* Copy Constructor */
        SIPVoIPLink(const SIPVoIPLink& rh);

        /* Assignment Operator */
        SIPVoIPLink& operator=( const SIPVoIPLink& rh);

        /** 
         * Try to initiate the pjsip engine/thread and set config 
         * @return bool True if OK
         */
        bool init(void);

        /**
         * Shut the library and clean up
         */
        void terminate( void );

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        void getEvent(void);

        /**
         * Build and send SIP registration request
         * @return bool True on success
         *		  false otherwise
         */
        int sendRegister(AccountID id);

        /**
         * Build and send SIP unregistration request
         * @return bool True on success
         *		  false otherwise
         */
        int sendUnregister(AccountID id);

        /**
         * Place a new call
         * @param id  The call identifier
         * @param toUrl  The Sip address of the recipient of the call
         * @return Call* The current call
         */
        Call* newOutgoingCall(const CallID& id, const std::string& toUrl);

        /**
         * Answer the call
         * @param id The call identifier
         * @return int True on success
         */
        bool answer(const CallID& id);

        /**
         * Hang up the call
         * @param id The call identifier
         * @return bool True on success
         */
        bool hangup(const CallID& id);

        /**
         * Hang up the call
         * @param id The call identifier
         * @return bool True on success
         */
        bool peerHungup(const CallID& id);

        /**
         * Cancel the call
         * @param id The call identifier
         * @return bool True on success
         */
        bool cancel(const CallID& id);

        /**
         * Put the call on hold
         * @param id The call identifier
         * @return bool True on success
         */
        bool onhold(const CallID& id);

        /**
         * Put the call off hold
         * @param id The call identifier
         * @return bool True on success
         */
        bool offhold(const CallID& id);

        /**
         * Transfer the call
         * @param id The call identifier
         * @param to The recipient of the transfer
         * @return bool True on success
         */
        bool transfer(const CallID& id, const std::string& to);

        /** Handle the incoming refer msg, not finished yet */
        bool transferStep2(SIPCall* call);

        /**
         * Refuse the call
         * @param id The call identifier
         * @return bool True on success
         */
        bool refuse (const CallID& id);

        /**
         * Send DTMF refering to account configuration
         * @param id The call identifier
         * @param code  The char code
         * @return bool True on success
         */
        bool carryingDTMFdigits(const CallID& id, char code);

        /**
         * Send Dtmf using SIP INFO message
         */
        bool dtmfSipInfo(SIPCall *call, char code);

        /**
         * Send Dtmf over RTP
         */
        bool dtmfOverRtp(SIPCall* call, char code);

        /** 
         * Terminate every call not hangup | brutal | Protected by mutex 
         */
        void terminateSIPCall(); 
 
        /**
         * Terminate only one call
         */
       void terminateOneCall(const CallID& id);

        /**
         * Send an outgoing call invite
         * @param call  The current call
         * @return bool True if all is correct
         */
        bool SIPOutgoingInvite(SIPCall* call);

        /**
         * Start a SIP Call
         * @param call  The current call
         * @param subject Undocumented
         * @return true if all is correct
         */
        bool SIPStartCall(SIPCall* call, const std::string& subject);

        /**
         * Tell the user that the call was answered
         * @param
         */
        void SIPCallAnswered(SIPCall *call, pjsip_rx_data *rdata);

        /**
         * Handling 5XX/6XX error
         * @param 
         */
        void SIPCallServerFailure(SIPCall *call);

        /**
         * Peer close the connection
         * @param
         */
        void SIPCallClosed(SIPCall *call);

        /**
         * The call pointer was released
         * If the call was not cleared before, report an error
         * @param
         */
        void SIPCallReleased(SIPCall *call);

        /**
         * Handle a re-invite request by the remote peer.
         * A re-invite is an invite request inside a dialog.
         * When receiving a re-invite, we close the current rtp session and create a new one with the updated information
         */
        void handle_reinvite (SIPCall *call);

        /**
         * SIPCall accessor
         * @param id  The call identifier
         * @return SIPCall*	  A pointer on SIPCall object
         */
        SIPCall* getSIPCall(const CallID& id);

        /** when we init the listener, how many times we try to bind a port? */
        int _nbTryListenAddr;

        /** Increment the number of SIP account connected to this link */
        void incrementClients (void) { _clients++; }

        /** Decrement the number of SIP account connected to this link */
        void decrementClients (void);

 	/**
     	* Set Recording
     	* @param id The call identifier
     	*/
    	void setRecording(const CallID& id);
      
        /**
     	* Returning state (true recording)
     	* @param id The call identifier
     	*/
    	bool isRecording(const CallID& id);

        /**
         * Return the codec protocol used for this call 
         * @param id The call identifier
         */
         std::string getCurrentCodecName();
      
        int inv_session_reinvite (SIPCall *call, std::string direction="");
        
        bool new_ip_to_ip_call (const CallID& id, const std::string& to);

        std::string get_useragent_name (const AccountID& id);

        /** 
         * List all the interfaces on the system and return 
         * a vector list containing their IPV4 address.
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of IPV4 address available on all of the interfaces on
         * the system.
         */
        std::vector<std::string> getAllIpInterface(void);


       	/** 
         * List all the interfaces on the system and return 
         * a vector list containing their name (eth0, eth0:1 ...).
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of interface name available on all of the interfaces on
         * the system.
         */
        std::vector<std::string> getAllIpInterfaceByName(void);


	/** 
         * List all the interfaces on the system and return 
         * a vector list containing their name (eth0, eth0:1 ...).
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of interface name available on all of the interfaces on
         * the system.
         */
	std::string getInterfaceAddrFromName(std::string ifaceName);


	/**
	 * Initialize the transport selector
	 * @param transport		A transport associated with an account
	 * @param tp_sel		A pointer to receive the transport selector structure
	 *
	 * @return pj_status_t		PJ_SUCCESS if the structure was successfully initialized
	 */
	pj_status_t init_transport_selector (pjsip_transport *transport, pjsip_tpselector **tp_sel);

	/**
	 * Requests PJSIP library for local IP address, using pj_gethostbyname()
	 * @param addr*                 A string to be initialized
	 *
	 * @return bool                 True if addr successfully initialized
	 */
        bool loadSIPLocalIP (std::string *addr);


	/**
	 * This function unset the transport for a given account. It tests wether the 
	 * associated transport is used by other accounts. If not, it shutdown the transport
	 * putting its reference counter to zero. PJSIP assumes transport destruction since 
	 * this action can be delayed by ongoing SIP transactions.
	 */
	void shutdownSipTransport(const AccountID& accountID);

    private:
        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPVoIPLink(const AccountID& accountID);

        /* The singleton instance */
        static SIPVoIPLink* _instance;

        /**
         * Enable the SIP SRV resolver
         * @param endpt     The SIP endpoint
         * @param p_resv    Pointer to receive The DNS resolver instance
         *
         * @return pj_status_t  PJ_SUCCESS on success
         */
        pj_status_t enable_dns_srv_resolver (pjsip_endpoint *endpt, pj_dns_resolver ** p_resv);

        void busy_sleep(unsigned msec);

        /** 
         * Initialize the PJSIP library
         * Must be called before any other calls to the SIP layer
         *
         * @return bool True on success
         */
        bool pjsip_init();

        /**
         * Delete link-related stuff like calls
         */
        bool pjsip_shutdown(void);

        pj_status_t stunServerResolve (AccountID id);


	/**
	 * Function used to create a new sip transport or get an existing one from the map.
	 * The SIP transport is "acquired" according to account's current settings.
	 * This function should be called before registering an account
	 * @param accountID An account id for which transport is to be set
	 *
	 * @return bool True if the account is successfully created or successfully obtained 
	 * from the transport map
	 */
	bool acquireTransport(const AccountID& accountID);


	/**
	 * Create the default UDP transport according ot Ip2Ip profile settings
	 */
	bool createDefaultSipUdpTransport();


	/**
	 * Create the default TLS litener using IP2IP_PROFILE settings
	 */
	void createDefaultSipTlsListener();


	/**
	 * Create the default TLS litener according to account settings.
	 */
	void createTlsListener(const AccountID& accountID);


	/**
	 * General Sip transport creation method according to the 
	 * transport type specified in account settings
	 * @param id The account id for which a transport must
     * be created.
	 */
	bool createSipTransport(AccountID id);


	/**
	 * Method to store newly created UDP transport in internal transport map. 
	 * Transports are stored in order to retreive them in case
	 * several accounts would share the same port number for UDP transprt.
	 * @param key The transport's port number
	 * @param transport A pointer to the UDP transport
	 */
	bool addTransportToMap(std::string key, pjsip_transport* transport);

     /**
	 * Create SIP UDP transport from account's setting
	 * @param id The account id for which a transport must
     * be created.
	 * @return pj_status_t PJ_SUCCESS on success 
	 */
        int createUdpTransport (AccountID = "");

     /**
      * Create a TLS transport from the default TLS listener from
      * @param id The account id for which a transport must
      * be created.
      * @return pj_status_t PJ_SUCCESS on success
      */
     pj_status_t createTlsTransport(const AccountID& id,  std::string remoteAddr);

	/**
     * Create a UDP transport using stun server to resove public address
     * @param id The account id for which a transport must
     * be created.
     * @return pj_status_t PJ_SUCCESS on success
     */
	pj_status_t createAlternateUdpTransport (AccountID id);


	/** 
	 * UDP Transports are stored in this map in order to retreive them in case
	 * several accounts would share the same port number.
	 */
	SipTransportMap _transportMap;

	/** For registration use only */
	int _regPort;

	/** Threading object */
	EventThread* _evThread;
	ost::Mutex _mutexSIP;

    /* Number of SIP accounts connected to the link */
    int _clients;
        
    /*
     * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered. 
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address 
         * @return pj_str_t The extern (public) address
         */
        std::string findLocalAddressFromUri(const std::string& uri, pjsip_transport *transport);
        
        /* 
         * Does the same as findLocalAddressFromUri but returns a port.
         * @param uri The uri from which we want to discover the port to use
         * @param transport The transport to use to discover the port 
         * @return int The extern (public) port
         */
        int findLocalPortFromUri(const std::string& uri, pjsip_transport *transport);
};


#endif
