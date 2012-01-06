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

#ifndef SIPVOIPLINK_H_
#define SIPVOIPLINK_H_

#include <map>

//////////////////////////////
/* PJSIP imports */
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath/stun_config.h>
///////////////////////////////

#include "sipaccount.h"
#include "voiplink.h"

namespace sfl {
class InstantMessaging;
}

class EventThread;
class SIPCall;
class SIPAccount;

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink : public VoIPLink {
    public:
        ~SIPVoIPLink();

        /**
         * Singleton method. Enable to retrieve the unique static instance
         * @return SIPVoIPLink* A pointer on the object
         */
        static SIPVoIPLink* instance();

        /**
         * Try to initiate the pjsip engine/thread and set config
         */
        virtual void init();

        /**
         * Shut the library and clean up
         */
        virtual void terminate();

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual void getEvent();

        /**
         * Build and send SIP registration request
         */
        virtual void sendRegister(Account *a);

        /**
         * Build and send SIP unregistration request
         */
        virtual void sendUnregister(Account *a);

        /**
         * Register a new keepalive registration timer to this endpoint
         */
        void registerKeepAliveTimer(pj_timer_entry& timer, pj_time_val& delay);

        /**
         * Abort currently registered timer
         */
        void cancelKeepAliveTimer(pj_timer_entry& timer); 

        /**
         * Place a new call
         * @param id  The call identifier
         * @param toUrl  The Sip address of the recipient of the call
         * @return Call* The current call
         */
        virtual Call* newOutgoingCall(const std::string& id, const std::string& toUrl);

        /**
         * Answer the call
         * @param c The call
         */
        virtual void answer(Call *c);

        /**
         * Hang up the call
         * @param id The call identifier
         */
        virtual void hangup(const std::string& id);

        /**
         * Hang up the call
         * @param id The call identifier
         */
        virtual void peerHungup(const std::string& id);

        /**
         * Put the call on hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual void onhold(const std::string& id);

        /**
         * Put the call off hold
         * @param id The call identifier
         * @return bool True on success
         */
        virtual void offhold(const std::string& id);

        bool transferCommon(SIPCall *call, pj_str_t *dst);

        /**
         * Transfer the call
         * @param id The call identifier
         * @param to The recipient of the transfer
         */
        virtual void transfer(const std::string& id, const std::string& to);

        /**
         * Attended transfer
         * @param The transfered call id
         * @param The target call id
         * @return True on success
         */
        virtual bool attendedTransfer(const std::string&, const std::string&);

        /**
         * Refuse the call
         * @param id The call identifier
         */
        virtual void refuse(const std::string& id);

        /**
         * Send DTMF refering to account configuration
         * @param id The call identifier
         * @param code  The char code
         */
        virtual void carryingDTMFdigits(const std::string& id, char code);

        /**
         * Start a new SIP call using the IP2IP profile
         * @param The call id
         * @param The target sip uri
         */
        bool SIPNewIpToIpCall(const std::string& id, const std::string& to);

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

        pj_caching_pool *getMemoryPoolFactory();

        /**
         * SIPCall accessor
         * @param id  The call identifier
         * @return SIPCall*	  A pointer on SIPCall object
         */
        SIPCall* getSIPCall(const std::string& id);

        /**
         * Return the codec protocol used for this call
         * @param c The call identifier
         */
        std::string getCurrentCodecName(Call *c) const;

        /**
         * Retrive useragent name from account
         */
        std::string getUseragentName(SIPAccount *) const;

        /**
         * List all the interfaces on the system and return
         * a vector list containing their IPV4 address.
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of IPV4 address available on all of the interfaces on
         * the system.
         */
        static std::vector<std::string> getAllIpInterface();

        /**
        * List all the interfaces on the system and return
        * a vector list containing their name (eth0, eth0:1 ...).
        * @param void
        * @return std::vector<std::string> A std::string vector
        * of interface name available on all of the interfaces on
        * the system.
        */
        static std::vector<std::string> getAllIpInterfaceByName();

        /**
         * List all the interfaces on the system and return
         * a vector list containing their name (eth0, eth0:1 ...).
         * @param void
         * @return std::vector<std::string> A std::string vector
         * of interface name available on all of the interfaces on
         * the system.
         */
        static std::string getInterfaceAddrFromName(const std::string &ifaceName);

        /**
         * Initialize the transport selector
         * @param transport		A transport associated with an account
         *
         * @return          	A pointer to the transport selector structure
         */
        pjsip_tpselector *initTransportSelector(pjsip_transport *, pj_pool_t *);

        /**
         * This function unset the transport for a given account.
         */
        void shutdownSipTransport(SIPAccount *account);

        /**
         * Send a SIP message to a call identified by its callid
         *
         * @param The InstantMessaging module which contains formating, parsing and sending method
         * @param The Id of the call to send the message to
         * @param The actual message to be transmitted
         * @param The sender of this message (could be another participant of a conference)
         */
        void sendTextMessage(sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from);

        /**
         * Create the default UDP transport according ot Ip2Ip profile settings
         */
        void createDefaultSipUdpTransport();

    private:
        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(SIPCall* call);

        void dtmfSend(SIPCall *call, char code, DtmfType type);

        NON_COPYABLE(SIPVoIPLink);

        SIPVoIPLink();

        /**
         * Resolve public address for this account
         */
        pj_status_t stunServerResolve(SIPAccount *);

        /**
         * Create the default TLS listener.
         */
        void createTlsListener(SIPAccount*, pjsip_tpfactory **listener);

        /**
         * General Sip transport creation method according to the
         * transport type specified in account settings
         * @param account The account for which a transport must be created.
         */
        void createSipTransport(SIPAccount *account);

        /**
        * Create SIP UDP transport from account's setting
        * @param account The account for which a transport must be created.
        */
        void createUdpTransport(SIPAccount *account);

        /**
         * Create a TLS transport from the default TLS listener from
         * @param account The account for which a transport must be created.
         */
        void createTlsTransport(SIPAccount *, std::string remoteAddr);

        /**
         * Create a UDP transport using stun server to resove public address
         * @param account The account for which a transport must be created.
         */
        void createStunTransport(SIPAccount *account);

        /**
         * Get the correct address to use (ie advertised) from
         * a uri. The corresponding transport that should be used
         * with that uri will be discovered.
         *
         * @param uri The uri from which we want to discover the address to use
         * @param transport The transport to use to discover the address
         */
        void findLocalAddressFromUri(const std::string& uri, pjsip_transport *transport, std::string &address, std::string &port);

        /**
         * UDP Transports are stored in this map in order to retreive them in case
         * several accounts would share the same port number.
         */
        std::map<pj_uint16_t, pjsip_transport*> transportMap_;

        /**
         * Threading object
         */
        EventThread *evThread_;

        friend class SIPTest;
};

#endif // SIPVOIPLINK_H_
