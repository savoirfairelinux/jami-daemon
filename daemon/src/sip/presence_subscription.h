/*
 * File: PresenceSubscription.h
 * Author: aol
 *
 * Created on April 24, 2012, 10:13 AM
 */

#ifndef SERVERPRESENCESUB_H
#define	SERVERPRESENCESUB_H

#include <string>
#include "logger.h"
#include <pjsip-simple/evsub.h>
#include"pjsip-simple/presence.h"

#include "manager.h"
#include "sipvoip_pres.h"


class PresenceSubscription {
public:
    PresenceSubscription(pjsip_evsub *evsub, char *r, std::string acc_Id, pjsip_dialog *d):
        sub(evsub)
        , remote(r)
        , accId(acc_Id)
        , dlg(d)
        , expires (-1) {};

    char            *remote;    /**< Remote URI.			    */
    std::string	    accId;	/**< Account ID.			    */

    void setExpires(int ms) {
        expires = ms;
    }

    int getExpires(){
        return expires;
    }

    bool matches(PresenceSubscription * s){
        // servers match if they have the same remote uri and the account ID.
      return ((!(strcmp(remote,s->remote))) && (accId==s->accId));
    }

    bool isActive(){
        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE )
            return true;
        return false;
    }

    /**
     * Send the tirst notification.
     * FIXME : pjsip_pres_notify crash because the header can't be cloned
     * So far, the first notify is sent in sipvoip_pres.c instead.
     */
    void init(){
        pjsip_tx_data *tdata = NULL;
        pres_msg_data msg_data;
        pj_str_t reason = pj_str("OK");
        pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;

        pjsip_pres_set_status(sub, pres_get_data());
        if (expires == 0)
            ev_state = PJSIP_EVSUB_STATE_TERMINATED;

        /* Create and send the the first NOTIFY to active subscription: */
        pj_str_t stateStr = pj_str("");
        pj_status_t status = pjsip_pres_notify(sub, ev_state, &stateStr, &reason, &tdata);
        if (status == PJ_SUCCESS) {
            pres_process_msg_data(tdata, &msg_data);
            status = pjsip_pres_send_request(sub, tdata);
        }

        if (status != PJ_SUCCESS) {
            WARN("Unable to create/send NOTIFY %d", status);
            pjsip_pres_terminate(sub, PJ_FALSE);
        }
    }

    void notify() {
         /* Only send NOTIFY once subscription is active. Some subscriptions
         * may still be in NULL (when app is adding a new buddy while in the
         * on_incoming_subscribe() callback) or PENDING (when user approval is
         * being requested) state and we don't send NOTIFY to these subs until
         * the user accepted the request.
         */
        if (isActive()) {
            WARN("Notifying %s.", remote);

            pjsip_tx_data *tdata;
            pjsip_pres_set_status(sub, pres_get_data());

            if (pjsip_pres_current_notify(sub, &tdata) == PJ_SUCCESS) {
                // add msg header and send
                pres_process_msg_data(tdata, NULL);
                pjsip_pres_send_request(sub, tdata);
            }
            else{
                WARN("Unable to create/send NOTIFY");
                pjsip_pres_terminate(sub, PJ_FALSE);
            }
        }
    }

    friend void pres_evsub_on_srv_state( pjsip_evsub *sub, pjsip_event *event);
    friend pj_bool_t pres_on_rx_subscribe_request(pjsip_rx_data *rdata);

private:
    NON_COPYABLE(PresenceSubscription);
    pjsip_evsub	    *sub;	    /**< The evsub.			    */
    pjsip_dialog    *dlg;	    /**< Dialog.			    */
    int		     expires;	    /**< "expires" value in the request.    */
};


#endif	/* SERVERPRESENCESUB_H */
