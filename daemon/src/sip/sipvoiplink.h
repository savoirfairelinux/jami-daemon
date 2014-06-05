/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "voiplink.h"
#include "sipaccount.h"
#include "siptransport.h"

#include <pjsip.h>
#include <pjlib.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjnath/stun_config.h>

#ifdef SFL_VIDEO
#include <queue>
#endif
#include <map>
#include <mutex>
#include <memory>

class SIPCall;
class SIPAccount;

typedef std::map<std::string, std::shared_ptr<SIPCall> > SipCallMap;
/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink : public VoIPLink {

    public:

        /**
         * Singleton method. Enable to retrieve the unique static instance
         * @return SIPVoIPLink* A pointer on the object
         */
        static SIPVoIPLink& instance();

        /**
         * Destroy the singleton instance
         */
        static void destroy();

        /**
         * Set pjsip's log level based on the SIPLOGLEVEL environment variable.
         * SIPLOGLEVEL = 0 minimum logging
         * SIPLOGLEVEL = 6 maximum logging
         */
        static void setSipLogLevel();

#ifdef __ANDROID__
        static void setSipLogger();
#endif

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        virtual bool handleEvents();

        /* Returns a list of all callIDs */
        std::vector<std::string>
        getCallIDs();

        /**
         * Return the internal account map for this VOIP link
         */
        AccountMap &
        getAccounts() { return sipAccountMap_; }

        virtual std::vector<std::shared_ptr<Call> > getCalls(const std::string &account_id) const;

        /**
         * Build and send SIP registration request
         */
        virtual void sendRegister(Account& a);

        /**
         * Build and send SIP unregistration request
         * @param destroy_transport If true, attempt to destroy the transport.
         */
        virtual void sendUnregister(Account& a, std::function<void(bool)> cb = std::function<void(bool)>());

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
        virtual std::shared_ptr<Call> newOutgoingCall(const std::string& id, const std::string& toUrl, const std::string &account_id);

        /**
         * Start a new SIP call using the IP2IP profile
         * @param The call id
         * @param The target sip uri
         */
        std::shared_ptr<Call> SIPNewIpToIpCall(const std::string& id, const std::string& to);

        /**
         * Place a call using the currently selected account
         * @param The call id
         * @param The target sip uri
         */
        std::shared_ptr<Call> newRegisteredAccountCall(const std::string& id,
                                                       const std::string& toUrl,
                                                       const std::string &account_id);

        /**
         * Answer the call
         * @param c The call
         */
        virtual void answer(Call *c);

        /**
         * Hang up the call
         * @param id The call identifier
         */
        virtual void hangup(const std::string& id, int reason);

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

        /**
         * Transfer method used for both type of transfer
         */
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
         * Get the memory pool factory since each calls has its own memory pool
         */
        pj_caching_pool *getMemoryPoolFactory();

        /**
         * Retrive useragent name from account
         */
        std::string getUseragentName(SIPAccount *) const;

        /**
         * Send a SIP message to a call identified by its callid
         *
         * @param The Id of the call to send the message to
         * @param The actual message to be transmitted
         * @param The sender of this message (could be another participant of a conference)
         */
#if HAVE_INSTANT_MESSAGING
        void sendTextMessage(const std::string& callID,
                             const std::string& message,
                             const std::string& from);
#endif
        void clearSipCallMap();
        void addSipCall(std::shared_ptr<SIPCall>& call);

        std::shared_ptr<SIPCall> getSipCall(const std::string& id);

        /**
         * A non-blocking SIPCall accessor
         *
         * Will return NULL if the callMapMutex could not be locked
         *
         * @param id  The call identifier
         * @return SIPCall* A pointer to the SIPCall object
         */
        std::shared_ptr<SIPCall> tryGetSIPCall(const std::string &id);

        void removeSipCall(const std::string &id);

        /**
         * Create the default UDP transport according ot Ip2Ip profile settings
         */
        void createDefaultSipUdpTransport();

    public:
        void loadIP2IPSettings();

        /**
         * Instance that maintain and manage transport (UDP, TLS)
         */
        std::unique_ptr<SipTransport> sipTransport;

#ifdef SFL_VIDEO
        static void enqueueKeyframeRequest(const std::string &callID);
#endif

        std::string
        guessAccountIdFromNameAndServer(const std::string &userName,
                                        const std::string &server) const;
        int getModId();
        pjsip_endpoint * getEndpoint();
        pjsip_module * getMod();
    private:

        NON_COPYABLE(SIPVoIPLink);

        SIPVoIPLink();
        ~SIPVoIPLink();

        /**
         * Contains a list of all SIP account
         */
        AccountMap sipAccountMap_;

        mutable std::mutex sipCallMapMutex_;
        SipCallMap sipCallMap_;

        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(std::shared_ptr<SIPCall>& call);

#ifdef SFL_VIDEO
        void dequeKeyframeRequests();
        void requestKeyframe(const std::string &callID);
        std::mutex keyframeRequestsMutex_;
        std::queue<std::string> keyframeRequests_;
#endif

        static SIPVoIPLink * instance_;

        friend class SIPTest;
};

#endif // SIPVOIPLINK_H_
