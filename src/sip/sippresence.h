/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Patrick Keroulas  <patrick.keroulas@savoirfairelinux.com>
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
 */

#ifndef SIPPRESENCE_H
#define SIPPRESENCE_H

#include <string>
#include <list>
#include <mutex>

#include "noncopyable.h"
#include "pjsip/sip_types.h"
#include "pjsip/sip_msg.h"
#include "pjsip/sip_multipart.h"
#include "pjsip-simple/publish.h"
#include "pjsip-simple/presence.h"
#include "pjsip-simple/rpid.h"
#include <pj/pool.h>

#define PRESENCE_FUNCTION_PUBLISH   0
#define PRESENCE_FUNCTION_SUBSCRIBE 1
#define PRESENCE_LOCK_FLAG          1
#define PRESENCE_CLIENT_LOCK_FLAG   2

/**
 * TODO Clean this:
 */
struct pj_caching_pool;

namespace jami {

struct pres_msg_data {
    /**
     * Additional message headers as linked list. Application can add
     * headers to the list by creating the header, either from the heap/pool
     * or from temporary local variable, and add the header using
     * linked list operation. See pjsip_apps.c for some sample codes.
     */
    pjsip_hdr    hdr_list;

    /**
     * MIME type of optional message body.
     */
    pj_str_t    content_type;

    /**
     * Optional message body to be added to the message, only when the
     * message doesn't have a body.
     */
    pj_str_t    msg_body;

    /**
     * Content type of the multipart body. If application wants to send
     * multipart message bodies, it puts the parts in \a parts and set
     * the content type in \a multipart_ctype. If the message already
     * contains a body, the body will be added to the multipart bodies.
     */
    pjsip_media_type  multipart_ctype;

    /**
     * List of multipart parts. If application wants to send multipart
     * message bodies, it puts the parts in \a parts and set the content
     * type in \a multipart_ctype. If the message already contains a body,
     * the body will be added to the multipart bodies.
     */
    pjsip_multipart_part multipart_parts;
};

class SIPAccount;
class PresSubClient;
class PresSubServer;


/**
 * @file sippresence.h
 * @brief A SIP Presence manages buddy subscription in both PBX and IP2IP contexts.
 */

class SIPPresence {

    public:

        /**
         * Constructor
         * @param acc the associated sipaccount
         */
        SIPPresence(SIPAccount * acc);
        /**
         * Destructor
         */
        ~SIPPresence();

        /**
         * Return associated sipaccount
         */
        SIPAccount * getAccount() const;
        /**
         * Return presence data.
         */
        pjsip_pres_status * getStatus();
        /**
         * Return presence module ID which is actually the same as the VOIP link
         */
        int getModId() const;
        /**
         *  Return a pool for generic functions.
         */
        pj_pool_t*  getPool() const;
        /**
         * Activate the module.
         * @param enable Flag
         */
        void enable(bool enabled);
        /**
         * Support the presence function publish/subscribe.
         * @param function Publish or subscribe to enable
         * @param enable Flag
         */
        void support(int function, bool enabled);
        /**
        * Fill xml document, the header and the body
        */
        void fillDoc(pjsip_tx_data *tdata, const pres_msg_data *msg_data);
        /**
         * Modify the presence data
         * @param status is basically "open" or "close"
         */
        void updateStatus(bool status, const std::string &note);
        /**
         * Send the presence data in a PUBLISH to the PBX or in a NOTIFY
         * to a remote subscriber (IP2IP)
         */
        void sendPresence(bool status, const std::string &note);
        /**
         * Send a signal to the client on DBus. The signal contain the status
         * of a remote user.
         */
        void reportPresSubClientNotification(const std::string& uri, pjsip_pres_status * status);
        /**
         * Send a SUBSCRIBE request to PBX/IP2IP
         * @param buddyUri  Remote user that we want to subscribe
         */
        void subscribeClient(const std::string& uri, bool flag);
        /**
         * Add a buddy in the buddy list.
         * @param b     PresSubClient pointer
         */
        void addPresSubClient(PresSubClient *b);
        /**
         * Remove a buddy from the list.
         * @param b     PresSubClient pointer
         */
        void removePresSubClient(PresSubClient *b);

        /**
        * IP2IP context.
        * Process new subscription based on client decision.
        * @param flag     client decision.
        * @param uri       uri of the remote subscriber
        */
        void approvePresSubServer(const std::string& uri, bool flag);
        /**
         * IP2IP context.
         * Add a server associated to a subscriber in the list.
         * @param s     PresenceSubcription pointer.
         */
        void addPresSubServer(PresSubServer *s);
        /**
         * IP2IP context.
         * Remove a server associated to a subscriber from the list.
         * @param s     PresenceSubcription pointer.
         */
        void removePresSubServer(PresSubServer *s);
        /**
         * IP2IP context.
         * Iterate through the subscriber list and send NOTIFY to each.
         */
        void notifyPresSubServer();

        bool isEnabled(){
            return enabled_;
        }

        bool isSupported(int function);

        const std::list< PresSubClient *>& getClientSubscriptions() const {
            return sub_client_list_;
        }

        bool isOnline(){
            return status_;
        }

        std::string getNote(){
            return note_;
        }

        void lock();
        bool tryLock();
        void unlock();

    private:
        NON_COPYABLE(SIPPresence);

        static pj_status_t publish(SIPPresence *pres);
        static void publish_cb(struct pjsip_publishc_cbparam *param);
        static pj_status_t send_publish(SIPPresence *pres);

        pjsip_publishc  *publish_sess_;  /**< Client publication session.*/
        pjsip_pres_status status_data_; /**< Presence Data to be published.*/

        pj_bool_t enabled_;
        pj_bool_t publish_supported_; /**< the server allow for status publishing */
        pj_bool_t subscribe_supported_; /**< the server allow for buddy subscription */

        bool status_; /**< Status received from the server*/
        std::string note_; /**< Note received  from the server*/
        SIPAccount * acc_; /**<  Associated SIP account. */
        std::list< PresSubServer *> sub_server_list_; /**< Subscribers list.*/
        std::list< PresSubClient *> sub_client_list_; /**< Subcribed buddy list.*/

        std::recursive_mutex mutex_;
        unsigned         mutex_nesting_level_ {0};
        pj_caching_pool  cp_;
        pj_pool_t       *pool_;
};

} // namespace jami

#endif
