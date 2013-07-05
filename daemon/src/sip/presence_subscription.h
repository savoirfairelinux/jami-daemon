/*
 * File: PresenceSubscription.h
 * Author: aol
 *
 * Created on April 24, 2012, 10:13 AM
 */

#ifndef SERVERPRESENCESUB_H
#define	SERVERPRESENCESUB_H

#include "logger.h"
#include <pjsip-simple/evsub.h>
#include"pjsip-simple/presence.h"


class PresenceSubscription {
public:
    PresenceSubscription(pjsip_evsub *evsub, char *r, std::string acc_Id, pjsip_dialog *d):
        sub(evsub)
        , remote(r)
        , accId(acc_Id)
        , dlg(d)
        , expires (-1) {};

    void setExpires(int ms) {
        expires = ms;
    }

    inline void notify(const std::string &newPresenceState, const std::string &newChannelState) {
        DEBUG("notifying %s", remote);

        pjsip_pres_status pres_status;
        pjsip_tx_data *tdata;

        pjsip_pres_get_status(sub, &pres_status);

        /* Only send NOTIFY once subscription is active. Some subscriptions
         * may still be in NULL (when app is adding a new buddy while in the
         * on_incoming_subscribe() callback) or PENDING (when user approval is
         * being requested) state and we don't send NOTIFY to these subs until
         * the user accepted the request.
         */
        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE
                /* && (pres_status.info[0].basic_open != getStatus()) */) {

            pres_status.info[0].basic_open = newPresenceState == "open"? true: false;

            pjsip_pres_set_status(sub, &pres_status);

            if (pjsip_pres_current_notify(sub, &tdata) == PJ_SUCCESS) {
                if (tdata->msg->type == PJSIP_REQUEST_MSG) {
                    const pj_str_t STR_USER_AGENT = {"User-Agent", 10};
                    pjsip_hdr *h;
                    pj_str_t ua = pj_str("SFLphone");
                    h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool,
                            &STR_USER_AGENT, &ua);
                    pjsip_msg_add_hdr(tdata->msg, h);
                }
                pjsip_pres_send_request(sub, tdata);
            }
        }
    }

    friend void pres_evsub_on_srv_state( pjsip_evsub *sub, pjsip_event *event);
    friend pj_bool_t my_pres_on_rx_request(pjsip_rx_data *rdata);

private:
    NON_COPYABLE(PresenceSubscription);
    pjsip_evsub	    *sub;	    /**< The evsub.			    */
    char            *remote;	    /**< Remote URI.			    */
    std::string	    accId;	    /**< Account ID.			    */
    pjsip_dialog    *dlg;	    /**< Dialog.			    */
    int		     expires;	    /**< "expires" value in the request.    */
};

#endif	/* SERVERPRESENCESUB_H */
