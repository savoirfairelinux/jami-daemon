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


/* Callback called when *server* subscription state has changed. */
void pres_evsub_on_srv_state(pjsip_evsub *sub, pjsip_event *event) {
    pjsip_rx_data *rdata = event->body.rx_msg.rdata;
    if(!rdata) {
        ERROR("no rdata in presence");
        return;
    }
    std::string accountId = "IP2IP";/* ebail : this code is only used for IP2IP accounts */
    SIPAccount *acc = Manager::instance().getSipAccount(accountId);
    PJ_UNUSED_ARG(event);
    PresenceSubscription *presenceSub;
//    PJSUA_LOCK(); /* ebail : FIXME figure out if locking is necessary or not */
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
//	    pj_list_erase(uapres);
            acc->removeServerSubscription(presenceSub);
        }
    }
//    PJSUA_UNLOCK(); /* ebail : FIXME figure out if unlocking is necessary or not */
}

pj_bool_t my_pres_on_rx_request(pjsip_rx_data *rdata) {

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;
    pj_str_t *str = &method->name;
    std::string request(str->ptr, str->slen);
    DEBUG("my_pres_on_rx_request for %s ", request.c_str());
    pj_str_t contact;
    pj_status_t status;
    pjsip_dialog *dlg;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_pres_status pres_status;
    pjsip_tx_data *tdata;
    pjsip_expires_hdr *expires_hdr;
//    int expires;
    pjsip_status_code st_code;
    pj_str_t reason;
    pjsua_msg_data msg_data;
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
        //	PJSUA_UNLOCK(); /* ebail : FIXME figure out if unlocking is necessary or not */
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

//	PJSUA_UNLOCK(); /* ebail : FIXME figure out if unlocking is necessary or not */
        return PJ_TRUE;
    }

    /* Attach our data to the subscription: */
/*  ebail : FIXME check if ths code is useful */
//    uapres = PJ_POOL_ALLOC_T(dlg->pool, pjsua_srv_pres);
//    uapres->sub = sub;
//    uapres->
    char* remote = (char*) pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
/*  ebail : FIXME check if ths code is useful */
//    uapres->acc_id = acc_id;
//    uapres->dlg = dlg;
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri, remote, PJSIP_MAX_URL_SIZE);

    pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, dlg->local.info->uri, contact.ptr, PJSIP_MAX_URL_SIZE);
    PresenceSubscription *presenceSub = new PresenceSubscription(sub, remote, accountId, dlg);

    if (status < 1)
        pj_ansi_strcpy(remote, "<-- url is too long-->");
    else
        remote[status] = '\0';

//    pjsip_evsub_add_header(sub, &acc->cfg.sub_hdr_list);
    int modId = ((SIPVoIPLink*) (acc->getVoIPLink()))->getModId();
    pjsip_evsub_set_mod_data(sub, modId/*my_mod_pres.id*/, presenceSub);
    //DEBUG("***************** presenceSub.name:%s",sub->mod_name);
    acc->addServerSubscription(presenceSub);
    /* Add server subscription to the list: */
//    pj_list_push_back(&pjsua_var.acc[acc_id].pres_srv_list, uapres);
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
//	pj_list_erase(uapres);
        pjsip_pres_terminate(sub, PJ_FALSE);
//	PJSUA_UNLOCK();
        return PJ_FALSE;
    }
//TODO: handle rejection case pjsua_pers.c:956

    /* If code is 200, send NOTIFY now */
#if 0
    if (st_code == 200) {
        pjsua_pres_notify(acc_id, uapres, PJSIP_EVSUB_STATE_ACTIVE,
                NULL, NULL, PJ_TRUE, &msg_data);
    }
#endif
    pj_bzero(&pres_status, sizeof(pres_status));
    pres_status.info_cnt = 1;
    pres_status.info[0].basic_open = true;
    /*std::string currState = ((SIPVoIPLink*) acc->getVoIPLink())->getChannelState();
    pres_status.info[0].chanState = pj_str(strdup(currState.c_str()));*/

    std::string contactWithAngles =  acc->getFromUri();
    contactWithAngles.erase(contactWithAngles.find('>'));

    int semicolon = contactWithAngles.find_first_of(":");
    std::string contactWithoutAngles = contactWithAngles.substr(semicolon + 1);
    pj_str_t contt = pj_str(strdup(contactWithoutAngles.c_str()));
//    acc->get

    pj_memcpy(&pres_status.info[0].contact, &contt, sizeof(pj_str_t));

//    pres_status.info[0].id = pj_str("000005"); //TODO: tuple id

    pjsip_pres_set_status(sub, &pres_status);

    ev_state = PJSIP_EVSUB_STATE_ACTIVE;
    if (presenceSub->expires == 0)
        ev_state = PJSIP_EVSUB_STATE_TERMINATED;

    /* Create and send the NOTIFY to active subscription: */
    pj_str_t stateStr = pj_str("");
    tdata = NULL;
    status = pjsip_pres_notify(sub, ev_state, &stateStr, &reason, &tdata);
    if (status == PJ_SUCCESS) {
        /* Force removal of message body if msg_body==FALSE */

        //pjsua_process_msg_data(tdata, msg_data);
        if (tdata->msg->type == PJSIP_REQUEST_MSG) {
            const pj_str_t STR_USER_AGENT = {
                    "User-Agent",
                    10 };
            pjsip_hdr *h;
            pj_str_t ua = pj_str("SFLPhone");
            h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool, &STR_USER_AGENT, &ua);
            pjsip_msg_add_hdr(tdata->msg, h);
        }

        const pjsip_hdr *hdr;
        hdr = msg_data.hdr_list.next;
        while (hdr && hdr != &msg_data.hdr_list) {
            pjsip_hdr *new_hdr;

            new_hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr);
            WARN("adding header", new_hdr->name.ptr);
            pjsip_msg_add_hdr(tdata->msg, new_hdr);

            hdr = hdr->next;
        }
        if (msg_data.content_type.slen && msg_data.msg_body.slen) {
//            pjsip_media_type ctype;
            pjsip_msg_body *body;
            pj_str_t type = pj_str("application");
            pj_str_t subtype = pj_str("pidf+xml");

//            pjsua_parse_media_type(tdata->pool, &msg_data->content_type, &ctype);
            body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &msg_data.msg_body);
            tdata->msg->body = body;
        }
        // process_msg_data
        status = pjsip_pres_send_request(sub, tdata);
    }

    if (status != PJ_SUCCESS) {
        WARN("Unable to create/send NOTIFY %d", status);
//	pj_list_erase(srv_pres);
        pjsip_pres_terminate(sub, PJ_FALSE);
//	PJSUA_UNLOCK();
        return status;
    }

    return PJ_TRUE;
}
