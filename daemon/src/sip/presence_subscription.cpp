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


#include "pjsip/sip_multipart.h"

#include "sipvoiplink.h"
#include "manager.h"
#include "sippresence.h"
#include "logger.h"
#include "presence_subscription.h"

/* Callback called when *server* subscription state has changed. */
void pres_evsub_on_srv_state(pjsip_evsub *sub, pjsip_event *event) {
    pjsip_rx_data *rdata = event->body.rx_msg.rdata;
    if(!rdata) {
        DEBUG("no rdata in presence");
        return;
    }
    /*std::string accountId = "IP2IP";
    SIPAccount *acc = Manager::instance().getSipAccount(accountId);*/
    PJ_UNUSED_ARG(event);
    SIPPresence * pres = Manager::instance().getSipAccount("IP2IP")->getPresence();

    PresenceSubscription *presenceSub = (PresenceSubscription *) pjsip_evsub_get_mod_data(sub,pres->getModId());
    WARN("Presence server subscription to %s is %s", presenceSub->remote, pjsip_evsub_get_state_name(sub));

    if (presenceSub) {
        pjsip_evsub_state state;

        state = pjsip_evsub_get_state(sub);

        /*  ebail : FIXME check if ths code is usefull */
#if 0
        if (false pjsua_var.ua_cfg.cb.on_srv_subscribe_state) {
            pj_str_t from;

            from = server->dlg->remote.info_str;
            (*pjsua_var.ua_cfg.cb.on_srv_subscribe_state)(uapres->acc_id,
                    uapres, &from,
                    state, event);
        }
#endif

        if (state == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data(sub, pres->getModId(), NULL);
            pres->removeServerSubscription(presenceSub);
        }
    }
}

pj_bool_t pres_on_rx_subscribe_request(pjsip_rx_data *rdata) {

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;
    pj_str_t *str = &method->name;
    std::string request(str->ptr, str->slen);
    DEBUG("pres_on_rx_subscribe_request for %s.", request.c_str());
    pj_str_t contact;
    pj_status_t status;
    pjsip_dialog *dlg;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_tx_data *tdata;
    pjsip_expires_hdr *expires_hdr;
    pjsip_status_code st_code;
    pj_str_t reason;
    pres_msg_data msg_data;
    pjsip_evsub_state ev_state;

    /* ebail this code only hande incoming subscribe messages. Otherwise we return FALSE to let other modules handle it */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, pjsip_get_subscribe_method()) != 0){
        return PJ_FALSE;
    }

    std::string name(rdata->msg_info.to->name.ptr, rdata->msg_info.to->name.slen);
    std::string server(rdata->msg_info.from->name.ptr, rdata->msg_info.from->name.slen);

    std::string accountId = "IP2IP"; /* ebail : this code is only used for IP2IP accounts */
    SIPAccount *acc = (SIPAccount *) Manager::instance().getSipAccount(accountId);
    pjsip_endpoint *endpt = ((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint();
    SIPPresence * pres = acc->getPresence();

    contact = pj_str(strdup(acc->getContactHeader().c_str()));

    /* Create UAS dialog: */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, &contact, &dlg);
    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        WARN("Unable to create UAS dialog for subscription: %s [status=%d]", errmsg, status);
        pjsip_endpt_respond_stateless(endpt, rdata, 400, NULL, NULL, NULL);
        return PJ_TRUE;
    }

    /* Init callback: */
    pj_bzero(&pres_cb, sizeof(pres_cb));
    pres_cb.on_evsub_state = &pres_evsub_on_srv_state;

    /* Create server presence subscription: */
    status = pjsip_pres_create_uas(dlg, &pres_cb, rdata, &sub);
    if (status != PJ_SUCCESS) {
        int code = PJSIP_ERRNO_TO_SIP_STATUS(status);
        pjsip_tx_data *tdata;

        WARN("Unable to create server subscription %d", status);

        if (code == 599 || code > 699 || code < 300) {
            code = 400;
        }

        status = pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata);
        if (status == PJ_SUCCESS) {
            status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
        }

        return PJ_FALSE;
    }

    /* Attach our data to the subscription: */
    char* remote = (char*) pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri, remote, PJSIP_MAX_URL_SIZE);
    pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, dlg->local.info->uri, contact.ptr, PJSIP_MAX_URL_SIZE);
    PresenceSubscription *presenceSub = new PresenceSubscription(pres, sub, remote, dlg);

    if (status < 1)
        pj_ansi_strcpy(remote, "<-- url is too long-->");
    else
        remote[status] = '\0';

    pjsip_evsub_set_mod_data(sub, pres->getModId(), presenceSub);

    /* Add server subscription to the list: */
    pres->addServerSubscription(presenceSub);

    /* Capture the value of Expires header. */
    expires_hdr = (pjsip_expires_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES, NULL);
    if (expires_hdr)
        presenceSub->setExpires(expires_hdr->ivalue);
    else
        presenceSub->setExpires(-1);

    st_code = (pjsip_status_code) 200;
    reason = pj_str("OK");
    pj_bzero(&msg_data, sizeof(msg_data));
    pj_list_init(&msg_data.hdr_list);
    pjsip_media_type_init(&msg_data.multipart_ctype, NULL, NULL);
    pj_list_init(&msg_data.multipart_parts);

    /* Create and send 2xx response to the SUBSCRIBE request: */
    status = pjsip_pres_accept(sub, rdata, st_code, &msg_data.hdr_list);
    if (status != PJ_SUCCESS) {
        WARN("Unable to accept presence subscription %d", status);
        pjsip_pres_terminate(sub, PJ_FALSE);
        return PJ_FALSE;
    }
//TODO: handle rejection case pjsua_pers.c:956


// -------------------------------------------------------------------------------V
// presenceSub->init();
//    return PJ_TRUE;
   /*Send notify immediatly*/

    pjsip_pres_set_status(sub, pres->getStatus());

    ev_state = PJSIP_EVSUB_STATE_ACTIVE;
    if (presenceSub->getExpires() == 0)
        ev_state = PJSIP_EVSUB_STATE_TERMINATED;

    /* Create and send the the first NOTIFY to active subscription: */
    pj_str_t stateStr = pj_str("");
    tdata = NULL;
    status = pjsip_pres_notify(sub, ev_state, &stateStr, &reason, &tdata);
    if (status == PJ_SUCCESS) {
        pres_process_msg_data(tdata, &msg_data);
        status = pjsip_pres_send_request(sub, tdata);
    }

    if (status != PJ_SUCCESS) {
        WARN("Unable to create/send NOTIFY %d", status);
        pjsip_pres_terminate(sub, PJ_FALSE);
        return status;
    }
    return PJ_TRUE;
}












PresenceSubscription::PresenceSubscription(SIPPresence * pres, pjsip_evsub *evsub, char *r,pjsip_dialog *d)
    :sub(evsub)
    , pres_(pres)
    , remote(r)
    , dlg(d)
    , expires (-1)
{

}


void PresenceSubscription::setExpires(int ms) {
    expires = ms;
}

int PresenceSubscription::getExpires(){
    return expires;
}

/*SIPPresence *  PresenceSubscription::getPresence(){
    return pres_;
}*/

bool PresenceSubscription::matches(PresenceSubscription * s){
    // servers match if they have the same remote uri and the account ID.
  return (!(strcmp(remote,s->remote))) ;
}

bool PresenceSubscription::isActive(){
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE )
        return true;
    return false;
}

/**
 * Send the tirst notification.
 * FIXME : pjsip_pres_notify crash because the header can't be cloned
 * So far, the first notify is sent in sipvoip_pres.c instead.
 */
void PresenceSubscription::init(){
    pjsip_tx_data *tdata = NULL;
    pres_msg_data msg_data;
    pj_str_t reason = pj_str("OK");
    pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;

    pjsip_pres_set_status(sub, pres_->getStatus());
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

void PresenceSubscription::notify() {
     /* Only send NOTIFY once subscription is active. Some subscriptions
     * may still be in NULL (when app is adding a new buddy while in the
     * on_incoming_subscribe() callback) or PENDING (when user approval is
     * being requested) state and we don't send NOTIFY to these subs until
     * the user accepted the request.
     */
    if (isActive()) {
        DEBUG("Notifying %s.", remote);

        pjsip_tx_data *tdata;
        pjsip_pres_set_status(sub, pres_->getStatus());

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
