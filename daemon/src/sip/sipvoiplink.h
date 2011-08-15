/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include <map>
#include <exception>

//////////////////////////////
/* PJSIP imports */
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath/stun_config.h>
///////////////////////////////

#include "voiplink.h"

namespace sfl {
    class InstantMessaging;
}

class EventThread;
class SIPCall;

#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2
#define RANDOM_SIP_PORT   rand() % 64000 + 1024

// To set the verbosity. From 0 (min) to 6 (max)
#define PJ_LOG_LEVEL 0

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink : public VoIPLink
{
	typedef std::map<std::string, pjsip_transport*>  SipTransportMap;

	public:

	    /* Copy Constructor */
        SIPVoIPLink (const SIPVoIPLink& rh);

        /**
         * Destructor
         */
        ~SIPVoIPLink();

        /* Assignment Operator */
        SIPVoIPLink& operator= (const SIPVoIPLink& rh);

        /**
         * Singleton method. Enable to retrieve the unique static instance
         * @return SIPVoIPLink* A pointer on the object
         */
        static SIPVoIPLink* instance ();

        /**
         * Increment the number of SIP account connected to this link
         */
        void incrementClients (void) {
            _clients++;
        }

        /**
         * Decrement the number of SIP account connected to this link
         */
        void decrementClients (void);

        /**
         * Try to initiate the pjsip engine/thread and set config
         * @return bool True if OK
         */
        virtual bool init (void);

        /**
         * Shut the library and clean up
         */
        virtual void terminate (void);

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual void getEvent (void);

        /**
         * Build and send SIP registration request
         * @return bool True on success
         *		  false otherwise
         */
        virtual void sendRegister (std::string id) throw(VoipLinkException);

        /**
         * Build and send SIP unregistration request
         * @return bool True on success
         *		  false otherwise
         */
        virtual void sendUnregister (std::string id) throw(VoipLinkException);

        /**
         * Place a new call
         * @param id  The call identifier
         * @param toUrl  The Sip address of the recipient of the call
         * @return Call* The current call
         */
        virtual Call* newOutgoingCall (const std::string& id, const std::string& toUrl) throw (VoipLinkException);

        /**
         * Answer the call
         * @param id The call identifier
         * @return True on success
         */
        virtual bool answer (const std::string& id) throw (VoipLinkException);

        /**
         * Hang up the call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool hangup (const std::string& id) throw (VoipLinkException);

        /**
         * Hang up the call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool peerHungup (const std::string& id) throw (VoipLinkException);

        /**
         * Cancel the call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool cancel (const std::string& id) throw (VoipLinkException);

        /**
         * Put the call on hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool onhold (const std::string& id) throw (VoipLinkException);

        /**
         * Put the call off hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool offhold (const std::string& id) throw (VoipLinkException);

        /**
         * Transfer the call
         * @param id The call identifier
         * @param to The recipient of the transfer
         * @return bool True on success
         */
        virtual bool transfer (const std::string& id, const std::string& to) throw (VoipLinkException);

        /**
         * Attended transfer
         * @param The transfered call id
         * @param The target call id
         * @return True on success
         */
        virtual bool attendedTransfer(const std::string&, const std::string&);

        /**
         * Handle the incoming refer msg, not finished yet
         */
        bool transferStep2 (SIPCall* call);

        /**
         * Refuse the call
         * @param id The call identifier
         * @return bool True on success
         */
        virtual bool refuse (const std::string& id);

        /**
         * Send DTMF refering to account configuration
         * @param id The call identifier
         * @param code  The char code
         * @return bool True on success
         */
        virtual bool carryingDTMFdigits (const std::string& id, char code);

        /**
         * Send Dtmf using SIP INFO message
         */
        bool dtmfSipInfo (SIPCall *call, char code);

        /**
         * Send Dtmf over RTP
         */
        bool dtmfOverRtp (SIPCall* call, char code);

        /**
         * Send an outgoing call invite
         * @param call  The current call
         * @return bool True if all is correct
         */
        bool SIPOutgoingInvite (SIPCall* call);

        /**
         * Start a SIP Call
         * @param call  The current call
         * @param subject Undocumented
         * @return true if all is correct
         */
        bool SIPStartCall (SIPCall* call, const std::string& subject);

        /**
         * Start a new SIP call using the IP2IP profile
         * @param The call id
         * @param The target sip uri
         */
        bool SIPNewIpToIpCall (const std::string& id, const std::string& to);

        /**
         * Tell the user that the call was answered
         * @param
         */
        void SIPCallAnswered (SIPCall *call, pjsip_rx_data *rdata);

        /**
         * Handling 5XX/6XX error
         * @param
         */
        void SIPCallServerFailure (SIPCall *call);

        /**
         * Peer close the connection
         * @param
         */
        void SIPCallClosed (SIPCall *call);

        /**
         * The call pointer was released
         * If the call was not cleared before, report an error
         * @param sip call
         */
        void SIPCallReleased (SIPCall *call);

        /**
         * Handle a re-invite request by the remote peer.
         * A re-invite is an invite request inside a dialog.
         * When receiving a re-invite, we updated information
         * concerning medias
         * @param sip call
         */
        void SIPHandleReinvite (SIPCall *call);

        pj_caching_pool *getMemoryPoolFactory();

        /**
         * SIPCall accessor
         * @param id  The call identifier
         * @return SIPCall*	  A pointer on SIPCall object
         */
        SIPCall* getSIPCall (const std::string& id);

        /**
         * Return the codec protocol used for this call
         * @param id The call identifier
         */
        std::string getCurrentCodecName(const std::string& id);

        /**
         * Retrive useragent name from account
         */
        std::string getUseragentName (const std::string& id);

        /**
         * List all the interfaces on the system and return
         * a vector list containing their IPV4 address.
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of IPV4 address available on all of the interfaces on
         * the system.
         */
        std::vector<std::string> getAllIpInterface (void);

        /**
        * List all the interfaces on the system and return
        * a vector list containing their name (eth0, eth0:1 ...).
        * @param void
        * @return std::vector<std::string> A std::string vector
        * of interface name available on all of the interfaces on
        * the system.
        */
        std::vector<std::string> getAllIpInterfaceByName (void);

        /**
         * List all the interfaces on the system and return
         * a vector list containing their name (eth0, eth0:1 ...).
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of interface name available on all of the interfaces on
         * the system.
         */
        std::string getInterfaceAddrFromName (std::string ifaceName);

        /**
         * Initialize the transport selector
         * @param transport		A transport associated with an account
         * @param tp_sel		A pointer to receive the transport selector structure
         *
         * @return pj_status_t		PJ_SUCCESS if the structure was successfully initialized
         */
        pj_status_t initTransportSelector (pjsip_transport *, pjsip_tpselector **, pj_pool_t *);

        /**
         * Requests PJSIP library for local IP address, using pj_gethostbyname()
         * @param addr*                 A string to be initialized
         *
         * @return bool                 True if addr successfully initialized
         */
        bool loadSIPLocalIP (std::string *addr);

        /**
         * Helper function for creating a route set from information
         * stored in configuration file.
         */
        pjsip_route_hdr *createRouteSet(Account *account, pj_pool_t *pool);

        /**
         * Wrapper around getaddrinfo
         */
        bool dnsResolution(pjsip_tx_data *tdata);

        /**
         * This function unset the transport for a given account. It tests wether the
         * associated transport is used by other accounts. If not, it shutdown the transport
         * putting its reference counter to zero. PJSIP assumes transport destruction since
         * this action can be delayed by ongoing SIP transactions.
         */
        void shutdownSipTransport (const std::string& accountID);

        /**
         * Send a SIP message to a call identified by its callid
         *
         * @param The InstantMessaging module which contains formating, parsing and sending method
         * @param The Id of the call to send the message to
         * @param The actual message to be transmitted
         * @param The sender of this message (could be another participant of a conference)
         *
         * @return True if the message is sent without error, false elsewhere
         */
        bool sendTextMessage (sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from);

        /**
         * Retrieve display name from the
         */
        static std::string parseDisplayName(char *);

        /**
         * Remove the "sip:" or "sips:" from input uri
         */
        static void stripSipUriPrefix(std::string&);

        /**
         * when we init the listener, how many times we try to bind a port?
         */
        int _nbTryListenAddr;

    private:
        SIPVoIPLink ();

        /* The singleton instance */
        static SIPVoIPLink* _instance;

        void busySleep (unsigned msec);

        /**
         * Initialize the PJSIP library
         * Must be called before any other calls to the SIP layer
         *
         * @return bool True on success
         */
        bool pjsipInit();

        /**
         * Delete link-related stuff like calls
         */
        bool pjsipShutdown (void);

        /**
         * Resolve public address for this account
         */
        pj_status_t stunServerResolve (std::string id);


        /**
         * Function used to create a new sip transport or get an existing one from the map.
         * The SIP transport is "acquired" according to account's current settings.
         * This function should be called before registering an account
         * @param accountID An account id for which transport is to be set
         *
         * @return bool True if the account is successfully created or successfully obtained
         * from the transport map
         */
        bool acquireTransport (const std::string& accountID);


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
        void createTlsListener (const std::string& accountID);


        /**
         * General Sip transport creation method according to the
         * transport type specified in account settings
         * @param id The account id for which a transport must
         * be created.
         */
        bool createSipTransport (std::string id);


        /**
         * Method to store newly created UDP transport in internal transport map.
         * Transports are stored in order to retreive them in case
         * several accounts would share the same port number for UDP transprt.
         * @param key The transport's port number
         * @param transport A pointer to the UDP transport
         */
        bool addTransportToMap (std::string key, pjsip_transport* transport);

        /**
        * Create SIP UDP transport from account's setting
        * @param id The account id for which a transport must
        * be created.
        * @return pj_status_t PJ_SUCCESS on success
        */
        int createUdpTransport (std::string = "");

        /**
         * Create a TLS transport from the default TLS listener from
         * @param id The account id for which a transport must
         * be created.
         * @return pj_status_t PJ_SUCCESS on success
         */
        pj_status_t createTlsTransport (const std::string& id,  std::string remoteAddr);

        /**
         * Create a UDP transport using stun server to resove public address
         * @param id The account id for which a transport must
         * be created.
         * @return pj_status_t PJ_SUCCESS on success
         */
        pj_status_t createAlternateUdpTransport (std::string id);

        /**
         * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered.
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address
         * @return pj_str_t The extern (public) address
         */
        std::string findLocalAddressFromUri (const std::string& uri, pjsip_transport *transport);

        /*
         * Does the same as findLocalAddressFromUri but returns a port.
         * @param uri The uri from which we want to discover the port to use
         * @param transport The transport to use to discover the port
         * @return int The extern (public) port
         */
        int findLocalPortFromUri (const std::string& uri, pjsip_transport *transport);

        /**
         * UDP Transports are stored in this map in order to retreive them in case
         * several accounts would share the same port number.
         */
        SipTransportMap _transportMap;

        /**
         * For registration use only
         */
        int _regPort;

        /**
         * Threading object
         */
        EventThread* _evThread;

        /**
         * Global mutex for the sip voiplink
         */
        ost::Mutex _mutexSIP;

        /**
         * Number of SIP accounts connected to the link
         */
        int _clients;


        friend class SIPTest;
};


#endif
