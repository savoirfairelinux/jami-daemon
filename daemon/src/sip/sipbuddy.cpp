/*
 *  Copyright (C) 2012, 2013 LOTES TM LLC
 *  Author : Andrey Loukhnov <aol.nnov@gmail.com>
 *
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify pult5-voip, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, LOTES-TM LLC
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <pj/log.h>
#include <pj/rand.h>
#include <pj/log.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_endpoint.h>
#include <string>
#include <pj/pool.h>
#include <pjsip/sip_ua_layer.h>
#include <pjsip-simple/evsub.h>

#include "sipbuddy.h"
#include "sipaccount.h"
#include "sipvoiplink.h"

#define PJSUA_BUDDY_SUB_TERM_REASON_LEN 32
#define PJSUA_PRES_TIMER 300
#define THIS_FILE "sipbuddy.cpp"

#include "logger.h"
//extern pjsip_module mod_ua_;
//extern pjsip_endpoint *endpt_;

int modId;
static void buddy_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry) {
    (void) th;
    SIPBuddy *b = (SIPBuddy *) entry->user_data;
    b->updatePresence();
}

/* Callback called when *client* subscription state has changed. */
static void sflphoned_evsub_on_state(pjsip_evsub *sub, pjsip_event *event) {
    SIPBuddy *buddy;

    PJ_UNUSED_ARG(event);

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */

    buddy = (SIPBuddy *) pjsip_evsub_get_mod_data(sub, modId);
    if (buddy) {
        buddy->incLock();
        PJ_LOG(4,
                (THIS_FILE, "Presence subscription to '%s' is '%s'", buddy->getURI().c_str(), pjsip_evsub_get_state_name(sub)?pjsip_evsub_get_state_name(sub):"null"));
//	pj_log_push_indent();

        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            int resub_delay = -1;

//            const pj_str_t *pjTermReason = pjsip_evsub_get_termination_reason(sub);
//            std::string termReason(pjTermReason->ptr,
//                    pjTermReason->slen > PJSUA_BUDDY_SUB_TERM_REASON_LEN?
//                        PJSUA_BUDDY_SUB_TERM_REASON_LEN:
//                        pjTermReason->slen
//            );
            pj_strdup_with_null(buddy->pool, &buddy->term_reason, pjsip_evsub_get_termination_reason(sub));
//            buddy->setTermReason(termReason);
//            buddy->setTermCode(200);
            buddy->term_code = 200;

            /* Determine whether to resubscribe automatically */
            if (event && event->type == PJSIP_EVENT_TSX_STATE) {
                const pjsip_transaction *tsx = event->body.tsx_state.tsx;
                if (pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method) == 0) {
//		    buddy->setTermCode(tsx->status_code);
                    buddy->term_code = tsx->status_code;
                    switch (tsx->status_code) {
                        case PJSIP_SC_CALL_TSX_DOES_NOT_EXIST:
                            /* 481: we refreshed too late? resubscribe
                             * immediately.
                             */
                            /* But this must only happen when the 481 is received
                             * on subscription refresh request. We MUST NOT try to
                             * resubscribe automatically if the 481 is received
                             * on the initial SUBSCRIBE (if server returns this
                             * response for some reason).
                             */
                            if (buddy->dlg->remote.contact)
                                resub_delay = 500;
                            break;
                    }
                } else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method) == 0) {
                    if (buddy->isTermReason("deactivated") || buddy->isTermReason("timeout")) {
                        /* deactivated: The subscription has been terminated,
                         * but the subscriber SHOULD retry immediately with
                         * a new subscription.
                         */
                        /* timeout: The subscription has been terminated
                         * because it was not refreshed before it expired.
                         * Clients MAY re-subscribe immediately. The
                         * "retry-after" parameter has no semantics for
                         * "timeout".
                         */
                        resub_delay = 500;
                    } else if (buddy->isTermReason("probation") || buddy->isTermReason("giveup")) {
                        /* probation: The subscription has been terminated,
                         * but the client SHOULD retry at some later time.
                         * If a "retry-after" parameter is also present, the
                         * client SHOULD wait at least the number of seconds
                         * specified by that parameter before attempting to re-
                         * subscribe.
                         */
                        /* giveup: The subscription has been terminated because
                         * the notifier could not obtain authorization in a
                         * timely fashion.  If a "retry-after" parameter is
                         * also present, the client SHOULD wait at least the
                         * number of seconds specified by that parameter before
                         * attempting to re-subscribe; otherwise, the client
                         * MAY retry immediately, but will likely get put back
                         * into pending state.
                         */
                        const pjsip_sub_state_hdr *sub_hdr;
                        pj_str_t sub_state = {
                                "Subscription-State",
                                18 };
                        const pjsip_msg *msg;

                        msg = event->body.tsx_state.src.rdata->msg_info.msg;
                        sub_hdr = (const pjsip_sub_state_hdr*) pjsip_msg_find_hdr_by_name(msg, &sub_state, NULL);
                        if (sub_hdr && sub_hdr->retry_after > 0)
                            resub_delay = sub_hdr->retry_after * 1000;
                    }

                }
            }

            /* For other cases of subscription termination, if resubscribe
             * timer is not set, schedule with default expiration (plus minus
             * some random value, to avoid sending SUBSCRIBEs all at once)
             */
            if (resub_delay == -1) {
//		pj_assert(PJSUA_PRES_TIMER >= 3);
                resub_delay = PJSUA_PRES_TIMER * 1000;// - 2500 + (pj_rand() % 5000);
            }
            buddy->sub = sub;
            buddy->rescheduleTimer(PJ_TRUE, resub_delay);
        }/* else {
             This will clear the last termination code/reason
            buddy->term_code = 0;
            buddy->term_reason.ptr = NULL;
        }*/

        /* Clear subscription */
       /* if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_terminate(buddy->sub, PJ_FALSE); // = NULL;
            buddy->status.info_cnt = 0;
            buddy->dlg = NULL;
            buddy->rescheduleTimer(PJ_FALSE, 0);
//            pjsip_evsub_set_mod_data(sub, modId, NULL);
        }*/

//	pj_log_pop_indent();
        buddy->decLock();
    }
}

/* Callback when transaction state has changed. */
static void sflphoned_evsub_on_tsx_state(pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event) {

    SIPBuddy *buddy;
    pjsip_contact_hdr *contact_hdr;

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    buddy = (SIPBuddy *) pjsip_evsub_get_mod_data(sub, modId);
    if (!buddy) {
        return;
    }
    buddy->incLock();

    /* We only use this to update buddy's Contact, when it's not
     * set.
     */
    if (buddy->contact.slen != 0) {
        /* Contact already set */
        buddy->decLock();
        return;
    }

    /* Only care about 2xx response to outgoing SUBSCRIBE */
    if (tsx->status_code / 100 != 2 || tsx->role != PJSIP_UAC_ROLE || event->type != PJSIP_EVENT_RX_MSG
            || pjsip_method_cmp(&tsx->method, pjsip_get_subscribe_method()) != 0) {
        buddy->decLock();
        return;
    }

    /* Find contact header. */
    contact_hdr = (pjsip_contact_hdr*) pjsip_msg_find_hdr(event->body.rx_msg.rdata->msg_info.msg, PJSIP_H_CONTACT,
            NULL);
    if (!contact_hdr || !contact_hdr->uri) {
        buddy->decLock();
        return;
    }

    buddy->contact.ptr = (char*) pj_pool_alloc(buddy->pool, PJSIP_MAX_URL_SIZE);
    buddy->contact.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, contact_hdr->uri, buddy->contact.ptr,
            PJSIP_MAX_URL_SIZE);
    if (buddy->contact.slen < 0)
        buddy->contact.slen = 0;

    buddy->decLock();
}

/* Callback called when we receive NOTIFY */
static void sflphoned_evsub_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text,
        pjsip_hdr *res_hdr, pjsip_msg_body **p_body) {
    SIPBuddy *buddy;

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    buddy = (SIPBuddy *) pjsip_evsub_get_mod_data(sub, modId);
    if (!buddy) {
        return;
    }
    buddy->incLock();
    /* Update our info. */
    pjsip_pres_get_status(sub, &buddy->status);
    if (buddy->status.info[0] != NULL ) {
      std::string basic(buddy->status.info[0].basic_open ? "open" : "closed");
      //ebail : TODO Call here the callback for presence changement
      ERROR("\n-----------------\n presenceStateChange for %s status=%s note=%s \n-----------------\n", buddy->getURI().c_str(),basic.c_str(),buddy->status.info[0].rpid.note.ptr);
    }

    /* The default is to send 200 response to NOTIFY.
     * Just leave it there..
     */
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(p_st_code);
    PJ_UNUSED_ARG(p_st_text);
    PJ_UNUSED_ARG(res_hdr);
    PJ_UNUSED_ARG(p_body);

    buddy->decLock();
}

SIPBuddy::SIPBuddy(const std::string& uri_, SIPAccount *acc_) :
        acc(acc_),
        uri(pj_str(strdup(uri_.c_str()))),
        //buddy_id(-1)
        contact(pj_str(strdup(acc->getFromUri().c_str()))),
        display(),
        dlg(NULL),
//        host ()
        monitor(false),
        name(),
        cp_(),
        pool(0),
//        port(0)
        status(),
        sub(NULL),
        term_code(0),
        term_reason(),
        timer(),
        user_data(NULL),
        lock_count(0) {
    pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create(&cp_.factory, "buddy", 512, 512, NULL);
}

SIPBuddy::~SIPBuddy() {
    while(lock_count >0) {
        usleep(200);
    }
    PJ_LOG(4, ("Destroying buddy object with uri %s", uri.ptr));
    monitor = false;
    rescheduleTimer(PJ_FALSE, 0);
    updatePresence();

    pj_pool_release(pool);
}

bool SIPBuddy::isSubscribed() {
    return this->monitor;
}

std::string SIPBuddy::getURI() {
    std::string buddyURI(uri.ptr, uri.slen);
    return buddyURI;
}

bool SIPBuddy::isTermReason(std::string reason) {
    std::string myReason(term_reason.ptr, term_reason.slen);
    return !myReason.compare(reason);
}

void SIPBuddy::rescheduleTimer(bool reschedule, unsigned msec) {
    if (timer.id) {
        //	pjsua_cancel_timer(&timer);
        pjsip_endpt_cancel_timer(((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint(), &timer);
        timer.id = PJ_FALSE;
    }

    if (reschedule) {
        pj_time_val delay;

        PJ_LOG(4,
                (THIS_FILE, "Resubscribing buddy  %.*s in %u ms (reason: %.*s)", uri.slen, uri.ptr, msec, (int) term_reason.slen, term_reason.ptr));
        monitor = PJ_TRUE;
        pj_timer_entry_init(&timer, 0, this, &buddy_timer_cb);
        delay.sec = 0;
        delay.msec = msec;
        pj_time_val_normalize(&delay);

        //	if (pjsua_schedule_timer(&timer, &delay)==PJ_SUCCESS)

        if (pjsip_endpt_schedule_timer(((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint(), &timer, &delay) == PJ_SUCCESS) {
//            timer.id = PJ_TRUE;
        }
    }
}

pj_status_t SIPBuddy::updatePresence() {

    if (!monitor) {
        /* unsubscribe */
        pjsip_tx_data *tdata;
        pj_status_t retStatus;

        if (sub == NULL) {
            return PJ_SUCCESS;
        }

        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_terminate(sub, PJ_FALSE); // = NULL;
            return PJ_SUCCESS;
        }

        PJ_LOG(5, (THIS_FILE, "Buddy %s: unsubscribing..", uri.ptr));

        retStatus = pjsip_pres_initiate(sub, 300, &tdata);
        if (retStatus == PJ_SUCCESS) {
            //	pjsua_process_msg_data(tdata, NULL);
            if (/*pjsua_var.ua_cfg.user_agent.slen &&*/
            tdata->msg->type == PJSIP_REQUEST_MSG) {
                const pj_str_t STR_USER_AGENT = {
                        "User-Agent",
                        10 };
                pj_str_t ua = pj_str(strdup(acc->getUserAgentName().c_str()));
                pjsip_hdr *h;
                h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool, &STR_USER_AGENT, &ua);
                pjsip_msg_add_hdr(tdata->msg, h);
            }
            retStatus = pjsip_pres_send_request(sub, tdata);
        }

        if (retStatus != PJ_SUCCESS && sub) {
            pjsip_pres_terminate(sub, PJ_FALSE);
            pjsip_evsub_terminate(sub, PJ_FALSE); // = NULL;
            PJ_LOG(4, (THIS_FILE, "Unable to unsubscribe presence", status));
        }
//        pjsip_evsub_set_mod_data(sub, modId, NULL);
        return PJ_SUCCESS;
    }

#if 0
    if (sub && sub->dlg) { //do not bother if already subscribed
//        PJ_LOG(4, (THIS_FILE, "Buddy %s: already subscribed", uri.ptr));
//        return PJ_SUCCESS;
        pjsip_evsub_terminate(sub, PJ_FALSE);
    }
#endif

    //subscribe
    pjsip_evsub_user pres_callback;
//    pj_pool_t *tmp_pool = NULL;

    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Event subscription callback. */
    pj_bzero(&pres_callback, sizeof(pres_callback));
    pres_callback.on_evsub_state = &sflphoned_evsub_on_state;
    pres_callback.on_tsx_state = &sflphoned_evsub_on_tsx_state;
    pres_callback.on_rx_notify = &sflphoned_evsub_on_rx_notify;

    PJ_LOG(4, (THIS_FILE, "Buddy %s: subscribing presence,using account %s..", uri.ptr, acc->getAccountID().c_str()));

    /* Generate suitable Contact header unless one is already set in
     * the account
     */
#if 0
    if (acc->contact.slen) {
        contact = acc->contact;
    } else {
        tmp_pool = pjsua_pool_create("tmpbuddy", 512, 256);

        status = pjsua_acc_create_uac_contact(tmp_pool, &contact,
                acc_id, &buddy->uri);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_FILE, "Unable to generate Contact header",
                    status);
            pj_pool_release(tmp_pool);
            pj_log_pop_indent();
            return;
        }
    }
#endif
    /* Create UAC dialog */
    pj_str_t from = pj_str(strdup(acc->getFromUri().c_str()));
    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &from, &contact, &uri, NULL, &dlg);
    if (status != PJ_SUCCESS) {
        //pjsua_perror(THIS_FILE, "Unable to create dialog",
         //       status);
        ERROR("Unable to create dialog \n");
        //if (tmp_pool) pj_pool_release(tmp_pool);
        //pj_log_pop_indent();
        return PJ_FALSE;
    }
    //ELOI add credential for auth - otherwise subscription was failing
    if (acc->hasCredentials() and pjsip_auth_clt_set_credentials(&dlg->auth_sess, acc->getCredentialCount(), acc->getCredInfo()) != PJ_SUCCESS) {
      ERROR("Could not initialize credentials for invite session authentication");
    }
    // E.B add credential for auth

    /* Increment the dialog's lock otherwise when presence session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(dlg);

    status = pjsip_pres_create_uac(dlg, &pres_callback, PJSIP_EVSUB_NO_EVENT_ID, &sub);
    if (status != PJ_SUCCESS) {
        pjsip_evsub_terminate(sub, PJ_FALSE); // = NULL;
        PJ_LOG(4, (THIS_FILE, "Unable to create presence client", status));
        /* This should destroy the dialog since there's no session
         * referencing it
         */
        if (dlg) {
            pjsip_dlg_dec_lock(dlg);
        }
//        if (tmp_pool) pj_pool_release(tmp_pool);
//        pj_log_pop_indent();
        return PJ_SUCCESS;
    }
#if 0
    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
        pjsip_tpselector tp_sel;

        pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
        pjsip_dlg_set_transport(buddy->dlg, &tp_sel);
    }

    /* Set route-set */
    if (!pj_list_empty(&acc->route_set)) {
        pjsip_dlg_set_route_set(buddy->dlg, &acc->route_set);
    }

    /* Set credentials */
    if (acc->cred_cnt) {
        pjsip_auth_clt_set_credentials(&buddy->dlg->auth_sess,
                acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&buddy->dlg->auth_sess, &acc->cfg.auth_pref);
#endif
    modId = ((SIPVoIPLink*) acc->getVoIPLink())->getModId();
    pjsip_evsub_set_mod_data(sub, modId, this);

    status = pjsip_pres_initiate(sub, -1, &tdata);
    if (status != PJ_SUCCESS) {
        if (dlg)
            pjsip_dlg_dec_lock(dlg);
        if (sub) {
            pjsip_pres_terminate(sub, PJ_FALSE);
        }
        pjsip_evsub_terminate(sub, PJ_FALSE); // = NULL;
        PJ_LOG(4, (THIS_FILE, "Unable to create initial SUBSCRIBE", status));
//        if (tmp_pool) pj_pool_release(tmp_pool);
//        pj_log_pop_indent();
        return PJ_SUCCESS;
    }

//    pjsua_process_msg_data(tdata, NULL);

    status = pjsip_pres_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
        if (dlg)
            pjsip_dlg_dec_lock(dlg);
        if (sub) {
            pjsip_pres_terminate(sub, PJ_FALSE);
            sub = NULL;
        }

        PJ_LOG(4, (THIS_FILE, "Unable to send initial SUBSCRIBE", status));
//        if (tmp_pool) pj_pool_release(tmp_pool);
//        pj_log_pop_indent();
        return PJ_SUCCESS;
    }

    pjsip_dlg_dec_lock(dlg);
//    if (tmp_pool) pj_pool_release(tmp_pool);
    return PJ_SUCCESS;
}

void SIPBuddy::subscribe() {
    monitor = true;
    updatePresence();
}

void SIPBuddy::unsubscribe() {
    monitor = false;
    updatePresence();
}
