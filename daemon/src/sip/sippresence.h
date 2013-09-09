/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef SIPPRESENCE_H
#define SIPPRESENCE_H

#include <vector>
#include <string>
#include <list>

#include "noncopyable.h"
#include "pjsip/sip_types.h"
#include "pjsip/sip_msg.h"
#include "pjsip/sip_multipart.h"
#include "pjsip-simple/publish.h"
#include "pjsip-simple/presence.h"
#include "pjsip-simple/rpid.h"
#include <pj/pool.h>

struct pres_msg_data
{
    /**
     * Additional message headers as linked list. Application can add
     * headers to the list by creating the header, either from the heap/pool
     * or from temporary local variable, and add the header using
     * linked list operation. See pjsip_apps.c for some sample codes.
     */
    pjsip_hdr	hdr_list;

    /**
     * MIME type of optional message body.
     */
    pj_str_t	content_type;

    /**
     * Optional message body to be added to the message, only when the
     * message doesn't have a body.
     */
    pj_str_t	msg_body;

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


//extern void pres_process_msg_data(pjsip_tx_data *tdata, const pres_msg_data *msg_data);


class SIPAccount;
class PresSubClient;
class PresSubServer;
/**
 * TODO Clean this:
 */
struct pj_caching_pool;


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
     * Activate the module (PUBLISH/SUBSCRIBE)
     */
    void enable(const bool& flag);
     /**
     * Fill xml document, the header and the body
     */
    void fillDoc(pjsip_tx_data *tdata, const pres_msg_data *msg_data);
    /**
     * Modify the presence data
     * @param status is basically "open" or "close"
     */
    void updateStatus(const  bool& status, const std::string &note);
    /**
     * Send the presence data in a PUBLISH to the PBX or in a NOTIFY
     * to a remote subscriber (IP2IP)
     */
    void sendPresence(const bool& status, const std::string &note);
    /**
     * Send a signal to the client on DBus. The signal contain the status
     * of a remote user.
     */
    void reportPresSubClientNotification(const std::string& uri, pjsip_pres_status * status);
    /**
     * Send a SUBSCRIBE request to PBX/IP2IP
     * @param buddyUri  Remote user that we want to subscribe
     */
    void subscribeClient(const std::string& uri, const bool& flag);
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
     * Report new Subscription to the client, waiting for approval.
     * @param s     PresenceSubcription pointer.
     */
    void reportnewServerSubscriptionRequest(PresSubServer *s);
     /**
     * IP2IP context.
     * Process new subscription based on client decision.
     * @param flag     client decision.
     * @param uri       uri of the remote subscriber
     */
    void approvePresSubServer(const std::string& uri, const bool& flag);
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

    bool isEnabled() const {
        return enabled;
    }

    const std::list< PresSubClient *> getClientSubscriptions() {
        return pres_sub_client_list_;
    }

    /**
     * Lock methods
     */
    void lock();
    void unlock();
    bool tryLock();
    bool isLocked();

    pjsip_pres_status pres_status_data; /**< Presence Data.*/
    pjsip_publishc  *publish_sess;  /**< Client publication session.*/
    pj_bool_t   enabled; /**< Allow for status publish,*/

private:
    NON_COPYABLE(SIPPresence);

    SIPAccount * acc_; /**<  Associated SIP account. */
    std::list< PresSubServer *> pres_sub_server_list_; /**< Subscribers list.*/
    std::list< PresSubClient *> pres_sub_client_list_; /**< Subcribed buddy list.*/

    pj_mutex_t	*mutex_;	    /**< Mutex protection for this data	*/
    unsigned	mutex_nesting_level_; /**< Mutex nesting level.	*/
    pj_thread_t	*mutex_owner_; /**< Mutex owner.			*/
    pj_caching_pool     cp_;	    /**< Global pool factory.		*/
    pj_pool_t	*pool_;	    /**< pjsua's private pool.		*/


};

#endif
