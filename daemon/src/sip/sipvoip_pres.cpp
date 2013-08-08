/*
 *  Copyright (C) 2012б 2013 LOTES TM LLC
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

#ifdef have_config_h
#include "config.h"
#endif

#include <string>
#include <bits/stringfwd.h>
#include <bits/basic_string.h>

#include "pjsip/sip_dialog.h"
#include "pjsip/sip_msg.h"
#include "pjsip/sip_ua_layer.h"
#include "pjsip/sip_transaction.h"

#include"pjsip-simple/evsub.h"
#include"pjsip-simple/presence.h"
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/compat/string.h>

#include "sipvoip_pres.h"
#include "logger.h"
#include "sipaccount.h"
#include "sipcall.h"
#include "sipvoiplink.h"
#include "manager.h"

pjsip_pres_status pres_status_data;

pjsip_pres_status * pres_get_data(){
    return &pres_status_data;
}


void pres_update(const std::string &status, const std::string &note){
    pjrpid_element rpid = {PJRPID_ELEMENT_TYPE_PERSON,
            pj_str("20"),
            PJRPID_ACTIVITY_UNKNOWN,
            pj_str(strdup(note.c_str()))};

    /* fill activity if user not available. */
    if(note=="Away")
        rpid.activity = PJRPID_ACTIVITY_AWAY;
    else if (note=="Busy")
        rpid.activity = PJRPID_ACTIVITY_BUSY;
    else
        WARN("Presence : unkown activity");

    pj_bzero(&pres_status_data, sizeof(pres_status_data));
    pres_status_data.info_cnt = 1;
    pres_status_data.info[0].basic_open = (status == "open")? true: false;
    pres_status_data.info[0].id = pj_str("0"); /* TODO: tuplie_id*/
    pj_memcpy(&pres_status_data.info[0].rpid, &rpid,sizeof(pjrpid_element));
    /* "contact" field is optionnal */
}

void pres_process_msg_data(pjsip_tx_data *tdata, const pres_msg_data *msg_data){
    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
        const pj_str_t STR_USER_AGENT = {"User-Agent", 10 };
        pjsip_hdr *h;
        pj_str_t ua = pj_str("SFLPhone");
        h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool, &STR_USER_AGENT, &ua);
        pjsip_msg_add_hdr(tdata->msg, h);
    }

    if(msg_data == NULL)
        return;

    const pjsip_hdr *hdr;
    hdr = msg_data->hdr_list.next;
    while (hdr && hdr != &msg_data->hdr_list) {
        pjsip_hdr *new_hdr;
        new_hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr);
        WARN("adding header", new_hdr->name.ptr);
        pjsip_msg_add_hdr(tdata->msg, new_hdr);
        hdr = hdr->next;
    }

    if (msg_data->content_type.slen && msg_data->msg_body.slen) {
        pjsip_msg_body *body;
        pj_str_t type = pj_str("application");
        pj_str_t subtype = pj_str("pidf+xml");
        body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &msg_data->msg_body);
        tdata->msg->body = body;
    }
}


/* Callback called when *server* subscription state has changed. */
void pres_evsub_on_srv_state(pjsip_evsub *sub, pjsip_event *event) {
    pjsip_rx_data *rdata = event->body.rx_msg.rdata;
    if(!rdata) {
        DEBUG("no rdata in presence");
        return;
    }
    std::string accountId = "IP2IP";/* ebail : this code is only used for IP2IP accounts */
    SIPAccount *acc = Manager::instance().getSipAccount(accountId);
    PJ_UNUSED_ARG(event);
    PresenceSubscription *presenceSub;
    presenceSub = (PresenceSubscription *) pjsip_evsub_get_mod_data(sub,
            ((SIPVoIPLink*) (acc->getVoIPLink()))->getModId() /*my_mod_pres.id*/);
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
            pjsip_evsub_set_mod_data(sub, ((SIPVoIPLink*) (acc->getVoIPLink()))->getModId(), NULL);
            acc->removeServerSubscription(presenceSub);
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
    PresenceSubscription *presenceSub = new PresenceSubscription(sub, remote, accountId, dlg);

    if (status < 1)
        pj_ansi_strcpy(remote, "<-- url is too long-->");
    else
        remote[status] = '\0';

    int modId = ((SIPVoIPLink*) (acc->getVoIPLink()))->getModId();
    pjsip_evsub_set_mod_data(sub, modId/*my_mod_pres.id*/, presenceSub);

    /* Add server subscription to the list: */
    acc->addServerSubscription(presenceSub);

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

    pjsip_pres_set_status(sub, &pres_status_data);

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
