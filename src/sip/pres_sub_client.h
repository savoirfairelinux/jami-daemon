/*
 *  Copyright (C) 2012, 2013 LOTES TM LLC
 *  Author : Andrey Loukhnov <aol.nnov@gmail.com>
 *  Author : Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
 *  This file is a part of pult5-voip
 *
 *  pult5-voip is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  pult5-voip is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this programm. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PRES_SUB_CLIENT_H
#define PRES_SUB_CLIENT_H

#include <pjsip-simple/presence.h>
#include <pj/timer.h>
#include <pj/pool.h>
#include <string>

#include <pjsip-simple/evsub.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transport.h>
#include "noncopyable.h"

namespace jami {

class SIPPresence;

class PresSubClient {

    public:
        /**
         * Constructor
         * @param uri   SIP uri of remote user that we want to subscribe,
         */
        PresSubClient(const std::string &uri, SIPPresence *pres);
        /**
         * Destructor.
         * Process the the unsubscription before the destruction.
         */
        ~PresSubClient();
        /**
         * Compare with another pres_client's uris.
         * @param b     Other pres_client pointer
         */
        bool match(PresSubClient *b);
        /**
         * Enable the monitoring and report signal to the client.
         * The PBX server must approve and maintain the subrciption before the pres_client is added in the pres_client list.
         * @param flag  State of subscription. True if active.
         */
        void enable(bool flag);
        /**
         * Get associated parent presence_module
         */
        SIPPresence * getPresence();
        /**
         * Data lock function
         */
        bool lock();
        /**
         * Data unlock function
         */
        void unlock();
        /**
         * Send a SUBCRIBE to the PXB or directly to a pres_client in the IP2IP context.
         */
        bool subscribe();
        /**
         * Send a SUBCRIBE to the PXB or directly to a pres_client in the IP2IP context but
         * the 0s timeout make the dialog expire immediately.
         */
        bool unsubscribe();
        /**
         * Return  the monitor variable.
         */
        bool isSubscribed();
        /**
         * Return the pres_client URI
         */
        std::string getURI();

        /**
         * Is the buddy present
         */
        bool isPresent();

        /**
         * A message from the URIs
         */
        std::string getLineStatus();


        /**
         * TODO: explain this:
         */
        void incLock() {
            lock_count_++;
        }
        void decLock() {
            lock_count_--;
        }

    private:

        NON_COPYABLE(PresSubClient);

        /**
         * Transaction functions of event subscription client side.
         */
        static void pres_client_evsub_on_state(pjsip_evsub *sub, pjsip_event *event);
        static void pres_client_evsub_on_tsx_state(pjsip_evsub *sub,
                pjsip_transaction *tsx,
                pjsip_event *event);
        static void pres_client_evsub_on_rx_notify(pjsip_evsub *sub,
                pjsip_rx_data *rdata,
                int *p_st_code,
                pj_str_t **p_st_text,
                pjsip_hdr *res_hdr,
                pjsip_msg_body **p_body);
        static void pres_client_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry);

        /**
         * Plan a retry or a renew a subscription.
         * @param reschedule    Allow for reschedule.
         * @param msec          Delay value in milliseconds.
         */
        void rescheduleTimer(bool reschedule, unsigned msec);
        /**
         * Callback after a presence notification was received.
         * Tranfert info to the SIP account.
         */
        void reportPresence();
        /**
         * Process the un/subscribe request transmission.
         */
        pj_status_t updateSubscription();
        /*
         * Compare the reason of a transaction end with the given string.
         */
        bool isTermReason(const std::string &);
        /**
         * return the code after a transaction is terminated.
         */
        unsigned getTermCode();

        SIPPresence      *pres_;        /**< Associated SIPPresence pointer */
        pj_str_t         uri_;          /**< pres_client URI. */
        pj_str_t         contact_;      /**< Contact learned from subscrp. */
        pj_str_t         display_;      /**< pres_client display name. */
        pjsip_dialog    *dlg_;          /**< The underlying dialog. */
        pj_bool_t        monitored_;      /**< Should we monitor? */
        pj_str_t         name_;         /**< pres_client name. */
        pj_caching_pool  cp_;
        pj_pool_t       *pool_;         /**< Pool for this pres_client. */
        pjsip_pres_status status_;  /**< pres_client presence status. */
        pjsip_evsub     *sub_;          /**< pres_client presence subscription */
        unsigned         term_code_;    /**< Subscription termination code */
        pj_str_t         term_reason_;  /**< Subscription termination reason */
        pj_timer_entry   timer_;        /**< Resubscription timer */
        void            *user_data_;        /**< Application data. */
        int lock_count_;
        int lock_flag_;
        static int modId_; // used to extract data structure from event_subscription
};

} // namespace jami

#endif    /*  PRES_SUB_CLIENT_H */
