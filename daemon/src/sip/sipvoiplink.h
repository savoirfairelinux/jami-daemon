/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "sfl_types.h"
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
class SIPAccountBase;
class SIPVoIPLink;

typedef std::map<std::string, std::shared_ptr<SIPCall> > SipCallMap;

extern decltype(getGlobalInstance<SIPVoIPLink>)& getSIPVoIPLink;

/**
 * @file sipvoiplink.h
 * @brief Specific VoIPLink for SIP (SIP core for incoming and outgoing events).
 *          This class is based on the singleton design pattern.
 *          One SIPVoIPLink can handle multiple SIP accounts, but all the SIP accounts have all the same SIPVoIPLink
 */

class SIPVoIPLink {
    public:
        /**
         * Set pjsip's log level based on the SIPLOGLEVEL environment variable.
         * SIPLOGLEVEL = 0 minimum logging
         * SIPLOGLEVEL = 6 maximum logging
         */
        static void setSipLogLevel();

#ifdef __ANDROID__
        static void setSipLogger();
#endif

        SIPVoIPLink();
        ~SIPVoIPLink();

        /**
         * Event listener. Each event send by the call manager is received and handled from here
         */
        void handleEvents();

        /* Returns a list of all callIDs */
        std::vector<std::string> getCallIDs();

        /**
         * Register a new keepalive registration timer to this endpoint
         */
        void registerKeepAliveTimer(pj_timer_entry& timer, pj_time_val& delay);

        /**
         * Abort currently registered timer
         */
        void cancelKeepAliveTimer(pj_timer_entry& timer);

        /**
         * Get the memory pool factory since each calls has its own memory pool
         */
        pj_caching_pool *getMemoryPoolFactory();

        /**
         * Create the default UDP transport according ot Ip2Ip profile settings
         */
        void createDefaultSipUdpTransport();

        static void loadIP2IPSettings();

    public:
        static void createSDPOffer(pjsip_inv_session *inv,
                                   pjmedia_sdp_session **p_offer);

        /**
         * Instance that maintain and manage transport (UDP, TLS)
         */
        std::unique_ptr<SipTransport> sipTransport;

#ifdef SFL_VIDEO
        static void enqueueKeyframeRequest(const std::string &callID);
#endif

        /**
         * Guess the account related to an incoming SIP call.
         */
        std::shared_ptr<SIPAccountBase>
        guessAccount(const std::string& userName,
                     const std::string& server,
                     const std::string& fromUri) const;

        int getModId();
        pjsip_endpoint * getEndpoint();
        pjsip_module * getMod();

        pj_caching_pool* getCachingPool() const {
           return cp_;
        }

        pj_pool_t* getPool() const;

    private:

        NON_COPYABLE(SIPVoIPLink);

#ifdef SFL_VIDEO
        void dequeKeyframeRequests();
        void requestKeyframe(const std::string &callID);
        std::mutex keyframeRequestsMutex_;
        std::queue<std::string> keyframeRequests_;
#endif

        static pj_caching_pool* cp_;

        friend class SIPTest;
};

#endif // SIPVOIPLINK_H_
