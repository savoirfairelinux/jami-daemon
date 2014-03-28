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
#include <pjsip/sip_module.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_endpoint.h>
#include <string>
#include <sstream>
#include <pj/pool.h>
#include <pjsip/sip_ua_layer.h>
#include <pjsip-simple/evsub.h>
#include <unistd.h>

#include "array_size.h"
#include "pres_sub_client.h"
#include "client/presencemanager.h"
#include "client/configurationmanager.h"
#include "sipaccount.h"
#include "sippresence.h"
#include "sipvoiplink.h"
#include "sip_utils.h"
#include "manager.h"

#include "logger.h"

#define PRES_TIMER 300 // 5min

int PresSubClient::modId_ = 0; // used to extract data structure from event_subscription

void
PresSubClient::pres_client_timer_cb(pj_timer_heap_t * /*th*/, pj_timer_entry *entry)
{
    PresSubClient *c = (PresSubClient *) entry->user_data;
    DEBUG("timeout for %s", c->getURI().c_str());
}

/* Callback called when *client* subscription state has changed. */
void
PresSubClient::pres_client_evsub_on_state(pjsip_evsub *sub, pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    PresSubClient *pres_client = (PresSubClient *) pjsip_evsub_get_mod_data(sub, modId_);
    /* No need to pres->lock() here since the client has a locked dialog*/

    if (!pres_client) {
        WARN("pres_client not found");
        return;
    }

    DEBUG("Subscription for pres_client '%s' is '%s'", pres_client->getURI().c_str(),
            pjsip_evsub_get_state_name(sub) ? pjsip_evsub_get_state_name(sub) : "null");

    pjsip_evsub_state state = pjsip_evsub_get_state(sub);

    SIPPresence * pres = pres_client->getPresence();

    if (state == PJSIP_EVSUB_STATE_ACCEPTED) {
        pres_client->enable(true);
        Manager::instance().getClient()->getPresenceManager()->subscriptionStateChanged(
                pres->getAccount()->getAccountID(),
                pres_client->getURI().c_str(),
                PJ_TRUE);

        pres->getAccount()->supportPresence(PRESENCE_FUNCTION_SUBSCRIBE, true);

    } else if (state == PJSIP_EVSUB_STATE_TERMINATED) {
        int resub_delay = -1;
        pj_strdup_with_null(pres_client->pool_, &pres_client->term_reason_, pjsip_evsub_get_termination_reason(sub));

        Manager::instance().getClient()->getPresenceManager()->subscriptionStateChanged(
                pres->getAccount()->getAccountID(),
                pres_client->getURI().c_str(),
                PJ_FALSE);

        pres_client->term_code_ = 200;

        /* Determine whether to resubscribe automatically */
        if (event && event->type == PJSIP_EVENT_TSX_STATE) {
            const pjsip_transaction *tsx = event->body.tsx_state.tsx;

            if (pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method) == 0) {
                pres_client->term_code_ = tsx->status_code;
                std::ostringstream os;
                os << pres_client->term_code_;
                const std::string error = os.str() + "/" +
                    std::string(pres_client->term_reason_.ptr,
                            pres_client->term_reason_.slen);

                std::string msg;
                bool subscribe_allowed = PJ_FALSE;

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
                        if (pres_client->dlg_->remote.contact)
                            resub_delay = 500;
                        msg = "Bad subscribe refresh.";
                        subscribe_allowed = PJ_TRUE;
                        break;

                    case PJSIP_SC_NOT_FOUND:
                        msg = "Subscribe context not set for this buddy.";
                        subscribe_allowed = PJ_TRUE;
                        break;

                    case PJSIP_SC_FORBIDDEN:
                        msg = "Subscribe not allowed for this buddy.";
                        subscribe_allowed = PJ_TRUE;
                        break;

                    case PJSIP_SC_PRECONDITION_FAILURE:
                        msg = "Wrong server.";
                        break;
                }

                /*  report error:
                 *  1) send a signal through DBus
                 *  2) change the support field in the account schema if the pres_sub's server
                 *  is the same as the account's server
                 */
                Manager::instance().getClient()->getPresenceManager()->serverError(
                        pres_client->getPresence()->getAccount()->getAccountID(),
                        error,
                        msg);

                std::string account_host = std::string(pj_gethostname()->ptr, pj_gethostname()->slen);
                std::string sub_host = sip_utils::getHostFromUri(pres_client->getURI());

                if (not subscribe_allowed and account_host == sub_host)
                    pres_client->getPresence()->getAccount()->supportPresence(PRESENCE_FUNCTION_SUBSCRIBE, false);

            } else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method) == 0) {
                if (pres_client->isTermReason("deactivated") || pres_client->isTermReason("timeout")) {
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
                } else if (pres_client->isTermReason("probation") || pres_client->isTermReason("giveup")) {
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
                    pj_str_t sub_state = CONST_PJ_STR("Subscription-State");
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
            resub_delay = PRES_TIMER * 1000;
        }

        pres_client->sub_ = sub;
        pres_client->rescheduleTimer(PJ_TRUE, resub_delay);

    } else { //state==ACTIVE ......
        //This will clear the last termination code/reason
        pres_client->term_code_ = 0;
        pres_client->term_reason_.ptr = NULL;
    }

    /* Clear subscription */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        pjsip_evsub_terminate(pres_client->sub_, PJ_FALSE); // = NULL;
        pres_client->status_.info_cnt = 0;
        pres_client->dlg_ = NULL;
        pres_client->rescheduleTimer(PJ_FALSE, 0);
        pjsip_evsub_set_mod_data(sub, modId_, NULL);

        pres_client->enable(false);
    }
}

/* Callback when transaction state has changed. */
    void
PresSubClient::pres_client_evsub_on_tsx_state(pjsip_evsub *sub, pjsip_transaction *tsx, pjsip_event *event)
{

    PresSubClient *pres_client;
    pjsip_contact_hdr *contact_hdr;

    pres_client = (PresSubClient *) pjsip_evsub_get_mod_data(sub, modId_);
    /* No need to pres->lock() here since the client has a locked dialog*/

    if (!pres_client) {
        WARN("Couldn't find pres_client.");
        return;
    }

    /* We only use this to update pres_client's Contact, when it's not
     * set.
     */
    if (pres_client->contact_.slen != 0) {
        /* Contact already set */
        return;
    }

    /* Only care about 2xx response to outgoing SUBSCRIBE */
    if (tsx->status_code / 100 != 2 || tsx->role != PJSIP_UAC_ROLE || event->type != PJSIP_EVENT_RX_MSG
            || pjsip_method_cmp(&tsx->method, pjsip_get_subscribe_method()) != 0) {
        return;
    }

    /* Find contact header. */
    contact_hdr = (pjsip_contact_hdr*) pjsip_msg_find_hdr(event->body.rx_msg.rdata->msg_info.msg, PJSIP_H_CONTACT,
                  NULL);

    if (!contact_hdr || !contact_hdr->uri) {
        return;
    }

    pres_client->contact_.ptr = (char*) pj_pool_alloc(pres_client->pool_, PJSIP_MAX_URL_SIZE);
    pres_client->contact_.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, contact_hdr->uri, pres_client->contact_.ptr,
                                PJSIP_MAX_URL_SIZE);

    if (pres_client->contact_.slen < 0)
        pres_client->contact_.slen = 0;

}

/* Callback called when we receive NOTIFY */
void
PresSubClient::pres_client_evsub_on_rx_notify(pjsip_evsub *sub, pjsip_rx_data *rdata, int *p_st_code, pj_str_t **p_st_text, pjsip_hdr *res_hdr, pjsip_msg_body **p_body)
{

    PresSubClient *pres_client = (PresSubClient *) pjsip_evsub_get_mod_data(sub, modId_);

    if (!pres_client) {
        WARN("Couldn't find pres_client from ev_sub.");
        return;
    }
    /* No need to pres->lock() here since the client has a locked dialog*/

    pjsip_pres_get_status(sub, &pres_client->status_);
    pres_client->reportPresence();

    /* The default is to send 200 response to NOTIFY.
     * Just leave it there..
     */
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(p_st_code);
    PJ_UNUSED_ARG(p_st_text);
    PJ_UNUSED_ARG(res_hdr);
    PJ_UNUSED_ARG(p_body);
}

PresSubClient::PresSubClient(const std::string& uri, SIPPresence *pres) :
    pres_(pres),
    uri_{0, 0},
    contact_{0, 0},
    display_(),
    dlg_(NULL),
    monitored_(false),
    name_(),
    cp_(),
    pool_(0),
    status_(),
    sub_(NULL),
    term_code_(0),
    term_reason_(),
    timer_(),
    user_data_(NULL),
    lock_count_(0),
    lock_flag_(0)
{
    pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
    pool_ = pj_pool_create(&cp_.factory, "Pres_sub_client", 512, 512, NULL);
    uri_ = pj_strdup3(pool_, uri.c_str());
    contact_ = pj_strdup3(pool_, pres_->getAccount()->getFromUri().c_str());
}

PresSubClient::~PresSubClient()
{
    DEBUG("Destroying pres_client object with uri %.*s", uri_.slen, uri_.ptr);
    rescheduleTimer(PJ_FALSE, 0);
    unsubscribe();
    pj_pool_release(pool_);
}

bool PresSubClient::isSubscribed()
{
    return monitored_;
}

std::string PresSubClient::getURI()
{
    std::string res(uri_.ptr, uri_.slen);
    return res;
}

SIPPresence * PresSubClient::getPresence()
{
    return pres_;
}

bool PresSubClient::isPresent()
{
    return status_.info[0].basic_open;
}

std::string PresSubClient::getLineStatus()
{
    return std::string(status_.info[0].rpid.note.ptr, status_.info[0].rpid.note.slen);
}

bool PresSubClient::isTermReason(const std::string &reason)
{
    const std::string myReason(term_reason_.ptr, term_reason_.slen);
    return not myReason.compare(reason);
}

void PresSubClient::rescheduleTimer(bool reschedule, unsigned msec)
{
    SIPAccount * acc = pres_->getAccount();

    if (timer_.id) {
        pjsip_endpt_cancel_timer(((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint(), &timer_);
        timer_.id = PJ_FALSE;
    }

    if (reschedule) {
        pj_time_val delay;

        WARN("pres_client  %.*s will resubscribe in %u ms (reason: %.*s)",
             uri_.slen, uri_.ptr, msec, (int) term_reason_.slen, term_reason_.ptr);
        pj_timer_entry_init(&timer_, 0, this, &pres_client_timer_cb);
        delay.sec = 0;
        delay.msec = msec;
        pj_time_val_normalize(&delay);

        if (pjsip_endpt_schedule_timer(((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint(), &timer_, &delay) == PJ_SUCCESS) {
            timer_.id = PJ_TRUE;
        }
    }
}

void PresSubClient::enable(bool flag)
{
    DEBUG("pres_client %s is %s monitored.",getURI().c_str(), flag? "":"NOT");
    if (flag and not monitored_)
        pres_->addPresSubClient(this);
    monitored_ = flag;
}

void PresSubClient::reportPresence()
{
    /* callback*/
    pres_->reportPresSubClientNotification(getURI(), &status_);
}

bool PresSubClient::lock()
{
    unsigned i;

    for(i=0; i<50; i++)
    {
        if (not pres_->tryLock()){
            pj_thread_sleep(i/10);
            continue;
        }
        lock_flag_ = PRESENCE_LOCK_FLAG;

        if (dlg_ == NULL)
            return true;

        if (pjsip_dlg_try_inc_lock(dlg_) != PJ_SUCCESS) {
	    lock_flag_ = 0;
	    pres_->unlock();
	    pj_thread_sleep(i/10);
	    continue;
	}

        lock_flag_ = PRESENCE_CLIENT_LOCK_FLAG;
	pres_->unlock();
    }

    if (lock_flag_ == 0)
    {
        DEBUG("pres_client failed to lock : timeout");
        return false;
    }
    return true;
}

void PresSubClient::unlock()
{
    if (lock_flag_ & PRESENCE_CLIENT_LOCK_FLAG)
	pjsip_dlg_dec_lock(dlg_);

    if (lock_flag_ & PRESENCE_LOCK_FLAG)
	pres_->unlock();
}

bool PresSubClient::unsubscribe()
{
    if (not lock())
        return false;

    monitored_ = false;

    pjsip_tx_data *tdata;
    pj_status_t retStatus;

    if (sub_ == NULL or dlg_ == NULL) {
        WARN("PresSubClient already unsubscribed.");
        unlock();
        return false;
    }

    if (pjsip_evsub_get_state(sub_) == PJSIP_EVSUB_STATE_TERMINATED) {
        WARN("pres_client already unsubscribed sub=TERMINATED.");
        sub_ = NULL;
        unlock();
        return false;
    }

    /* Unsubscribe means send a subscribe with timeout=0s*/
    WARN("pres_client %.*s: unsubscribing..", uri_.slen, uri_.ptr);
    retStatus = pjsip_pres_initiate(sub_, 0, &tdata);

    if (retStatus == PJ_SUCCESS) {
        pres_->fillDoc(tdata, NULL);
        retStatus = pjsip_pres_send_request(sub_, tdata);
    }

    if (retStatus != PJ_SUCCESS and sub_) {
        pjsip_pres_terminate(sub_, PJ_FALSE);
        sub_ = NULL;
        WARN("Unable to unsubscribe presence", retStatus);
        unlock();
        return false;
    }

    //pjsip_evsub_set_mod_data(sub_, modId_, NULL);   // Not interested with further events

    unlock();
    return true;
}


bool PresSubClient::subscribe()
{

    if (sub_ and dlg_) { //do not bother if already subscribed
        pjsip_evsub_terminate(sub_, PJ_FALSE);
        DEBUG("PreseSubClient %.*s: already subscribed. Refresh it.", uri_.slen, uri_.ptr);
    }

    //subscribe
    pjsip_evsub_user pres_callback;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Event subscription callback. */
    pj_bzero(&pres_callback, sizeof(pres_callback));
    pres_callback.on_evsub_state = &pres_client_evsub_on_state;
    pres_callback.on_tsx_state = &pres_client_evsub_on_tsx_state;
    pres_callback.on_rx_notify = &pres_client_evsub_on_rx_notify;

    SIPAccount * acc = pres_->getAccount();
    DEBUG("PresSubClient %.*s: subscribing ", uri_.slen, uri_.ptr);


    /* Create UAC dialog */
    pj_str_t from = pj_strdup3(pool_, acc->getFromUri().c_str());
    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &from, &contact_, &uri_, NULL, &dlg_);

    if (status != PJ_SUCCESS) {
        ERROR("Unable to create dialog \n");
        return false;
    }

    /* Add credential for auth. */
    if (acc->hasCredentials() and pjsip_auth_clt_set_credentials(&dlg_->auth_sess, acc->getCredentialCount(), acc->getCredInfo()) != PJ_SUCCESS) {
        ERROR("Could not initialize credentials for subscribe session authentication");
    }

    /* Increment the dialog's lock otherwise when presence session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(dlg_);

    status = pjsip_pres_create_uac(dlg_, &pres_callback, PJSIP_EVSUB_NO_EVENT_ID, &sub_);

    if (status != PJ_SUCCESS) {
        sub_ = NULL;
        WARN("Unable to create presence client", status);

        /* This should destroy the dialog since there's no session
         * referencing it
         */
        if (dlg_) {
            pjsip_dlg_dec_lock(dlg_);
        }

        return false;
    }

    /* Add credential for authentication */
    if (acc->hasCredentials() and pjsip_auth_clt_set_credentials(&dlg_->auth_sess, acc->getCredentialCount(), acc->getCredInfo()) != PJ_SUCCESS) {
        ERROR("Could not initialize credentials for invite session authentication");
        return false;
    }

    /* Set route-set */
    pjsip_regc *regc = acc->getRegistrationInfo();
    if (regc and acc->hasServiceRoute())
        pjsip_regc_set_route_set(regc, sip_utils::createRouteSet(acc->getServiceRoute(), pres_->getPool()));

    // attach the client data to the sub
    pjsip_evsub_set_mod_data(sub_, modId_, this);

    status = pjsip_pres_initiate(sub_, -1, &tdata);
    if (status != PJ_SUCCESS) {
        if (dlg_)
            pjsip_dlg_dec_lock(dlg_);
        if (sub_)
            pjsip_pres_terminate(sub_, PJ_FALSE);
        sub_ = NULL;
        WARN("Unable to create initial SUBSCRIBE", status);
        return false;
    }

//    pjsua_process_msg_data(tdata, NULL);

    status = pjsip_pres_send_request(sub_, tdata);

    if (status != PJ_SUCCESS) {
        if (dlg_)
            pjsip_dlg_dec_lock(dlg_);
        if (sub_)
            pjsip_pres_terminate(sub_, PJ_FALSE);
        sub_ = NULL;
        WARN("Unable to send initial SUBSCRIBE", status);
        return false;
    }

    pjsip_dlg_dec_lock(dlg_);
    return true;
}

bool PresSubClient::match(PresSubClient *b)
{
    return (b->getURI() == getURI());
}
