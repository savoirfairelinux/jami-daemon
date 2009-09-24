/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

class EventThread;
class SIPCall;

#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2
#define RANDOM_SIP_PORT   rand() % 64000 + 1024

// To set the verbosity. From 0 (min) to 6 (max)
#define PJ_LOG_LEVEL 6 

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
         * Send DTMF
         * @param id The call identifier
         * @param code  The char code
         * @return bool True on success
         */
        bool carryingDTMFdigits(const CallID& id, char code);

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

        std::string get_useragent_name (void);

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
		 * Initialize the transport selector
		 * @param transport		A transport associated with an account
		 * @param tp_sel		A pointer to receive the transport selector structure
		 *
		 * @return pj_status_t		PJ_SUCCESS if the structure was successfully initialized
		 */
		pj_status_t init_transport_selector (pjsip_transport *transport, pjsip_tpselector **tp_sel);

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
         * Delete link-related stuuf like calls
         */
        bool pjsip_shutdown(void);

        pj_status_t stunServerResolve (AccountID id);

        /** Create SIP UDP Listener */
        int createUDPServer (AccountID = "");

        /**
         * Try to create a new TLS transport
         * with the settings defined in the corresponding
         * SIPAccount with id "id". If creatation fails
         * for whatever reason, it will try to start
         * it again on a randomly chosen port.
         *
         * A better idea would be to list all the transports
         * registered to the transport manager in order to find
         * an available port. Note that creation might also fail
         * for other reason than just a wrong port.
         * 
         * @param id The account id for which a tranport must
         * be created.
         * @return pj_status_t PJ_SUCCESS on success
         */
        pj_status_t createTlsTransportRetryOnFailure(AccountID id);

        /**
         * Try to create a TLS transport with the settings
         * defined in the corresponding SIPAccount with id
         * "id". 
         * @param id The account id for which a transport must
         * be created.
         * @return pj_status_t PJ_SUCCESS on success 
         */
        pj_status_t createTlsTransport(AccountID id);

		pj_status_t createAlternateUdpTransport (AccountID id);
        
        bool loadSIPLocalIP();

        std::string getLocalIP() {return _localExternAddress;}

        /* Flag to check if the STUN server is valid or not */
        bool validStunServer;

        /** The current STUN server address */
        std::string _stunServer;

        /** Local Extern Address is the IP address seen by peers for SIP listener */
        std::string _localExternAddress;

        /** Local Extern Port is the port seen by peers for SIP listener */
        unsigned int _localExternPort;
        
        /** For registration use only */
        int _regPort;

        /** Do we use stun? */
        bool _useStun;

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
