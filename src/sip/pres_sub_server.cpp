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


#include "pjsip/sip_multipart.h"

#include "sipaccount.h"
#include "sipvoiplink.h"
#include "manager.h"
#include "sippresence.h"
#include "logger.h"
#include "pres_sub_server.h"
#include "client/ring_signal.h"
#include "sip_utils.h"
#include "compiler_intrinsics.h"

namespace jami {

using sip_utils::CONST_PJ_STR;

/* Callback called when *server* subscription state has changed. */
void
PresSubServer::pres_evsub_on_srv_state(UNUSED pjsip_evsub *sub, UNUSED pjsip_event *event)
{
    JAMI_ERR("PresSubServer::pres_evsub_on_srv_state() is deprecated and does nothing");
    return;

#if 0 // DISABLED: removed IP2IP support, tuleap: #448
    pjsip_rx_data *rdata = event->body.rx_msg.rdata;

    if (!rdata) {
        JAMI_DBG("Presence_subscription_server estate has changed but no rdata.");
        return;
    }

    auto account = Manager::instance().getIP2IPAccount();
    auto sipaccount = static_cast<SIPAccount *>(account.get());
    if (!sipaccount) {
        JAMI_ERR("Could not find account IP2IP");
        return;
    }

    auto pres = sipaccount->getPresence();

    if (!pres) {
        JAMI_ERR("Presence not initialized");
        return;
    }

    pres->lock();
    PresSubServer *presSubServer = static_cast<PresSubServer *>(pjsip_evsub_get_mod_data(sub, pres->getModId()));

    if (presSubServer) {
        JAMI_DBG("Presence_subscription_server to %s is %s",
              presSubServer->remote_, pjsip_evsub_get_state_name(sub));
        pjsip_evsub_state state;

        state = pjsip_evsub_get_state(sub);

        if (state == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data(sub, pres->getModId(), NULL);
            pres->removePresSubServer(presSubServer);
        }

        /* TODO check if other cases should be handled*/
    }

    pres->unlock();
#endif
}

pj_bool_t
PresSubServer::pres_on_rx_subscribe_request(pjsip_rx_data *rdata)
{

    pjsip_method *method = &rdata->msg_info.msg->line.req.method;
    pj_str_t *str = &method->name;
    std::string request(str->ptr, str->slen);
//    pj_str_t contact;
#if 0 // DISABLED: removed IP2IP support, tuleap: #448
    pj_status_t status;
    pjsip_dialog *dlg;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_expires_hdr *expires_hdr;
    pjsip_status_code st_code;
    pj_str_t reason;
    pres_msg_data msg_data;
    pjsip_evsub_state ev_state;
#endif

    /* Only hande incoming subscribe messages should be processed here.
     * Otherwise we return FALSE to let other modules handle it */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, pjsip_get_subscribe_method()) != 0)
        return PJ_FALSE;

    JAMI_ERR("PresSubServer::pres_evsub_on_srv_state() is deprecated and does nothing");
    return PJ_FALSE;

#if 0 // DISABLED: removed IP2IP support, tuleap: #448
    /* debug msg */
    std::string name(rdata->msg_info.to->name.ptr, rdata->msg_info.to->name.slen);
    std::string server(rdata->msg_info.from->name.ptr, rdata->msg_info.from->name.slen);
    JAMI_DBG("Incoming pres_on_rx_subscribe_request for %s, name:%s, server:%s."
          , request.c_str()
          , name.c_str()
          , server.c_str());

    /* get parents*/
    auto account = Manager::instance().getIP2IPAccount();
    auto sipaccount = static_cast<SIPAccount *>(account.get());
    if (!sipaccount) {
        JAMI_ERR("Could not find account IP2IP");
        return PJ_FALSE;
    }

    pjsip_endpoint *endpt = getSIPVoIPLink()->getEndpoint();
    SIPPresence * pres = sipaccount->getPresence();
    pres->lock();

    /* Create UAS dialog: */
    const pj_str_t contact(sipaccount->getContactHeader());
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, &contact, &dlg);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        JAMI_WARN("Unable to create UAS dialog for subscription: %s [status=%d]", errmsg, status);
        pres->unlock();
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

        JAMI_WARN("Unable to create server subscription %d", status);

        if (code == 599 || code > 699 || code < 300) {
            code = 400;
        }

        status = pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata);

        if (status == PJ_SUCCESS) {
            pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
        }

        pres->unlock();
        return PJ_FALSE;
    }

    /* Attach our data to the subscription: */
    char* remote = (char*) pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri, remote, PJSIP_MAX_URL_SIZE);

    if (status < 1)
        pj_ansi_strcpy(remote, "<-- url is too long-->");
    else
        remote[status] = '\0';

    //pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, dlg->local.info->uri, contact.ptr, PJSIP_MAX_URL_SIZE);

    /* Create a new PresSubServer server and wait for client approve */
    PresSubServer *presSubServer = new PresSubServer(pres, sub, remote, dlg);
    pjsip_evsub_set_mod_data(sub, pres->getModId(), presSubServer);
    // Notify the client.
    emitSignal<DRing::PresenceSignal::NewServerSubscriptionRequest>(presSubServer->remote_);
    pres->addPresSubServer(presSubServer);

    /* Capture the value of Expires header. */
    expires_hdr = (pjsip_expires_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES, NULL);

    if (expires_hdr)
        presSubServer->setExpires(expires_hdr->ivalue);
    else
        presSubServer->setExpires(-1);

    st_code = (pjsip_status_code) 200;
    reason = CONST_PJ_STR("OK");
    pj_bzero(&msg_data, sizeof(msg_data));
    pj_list_init(&msg_data.hdr_list);
    pjsip_media_type_init(&msg_data.multipart_ctype, NULL, NULL);
    pj_list_init(&msg_data.multipart_parts);

    /* Create and send 2xx response to the SUBSCRIBE request: */
    status = pjsip_pres_accept(sub, rdata, st_code, &msg_data.hdr_list);

    if (status != PJ_SUCCESS) {
        JAMI_WARN("Unable to accept presence subscription %d", status);
        pjsip_pres_terminate(sub, PJ_FALSE);
        pres->unlock();
        return PJ_FALSE;
    }

    // Unsubscribe case
    ev_state = PJSIP_EVSUB_STATE_ACTIVE;

    if (presSubServer->getExpires() == 0) {
        // PJSIP_EVSUB_STATE_TERMINATED
        pres->unlock();
        return PJ_TRUE;
    }

    /*Send notify immediately. Replace real status with fake.*/

    // pjsip_pres_set_status(sub, pres->getStatus()); // real status

    // fake temporary status
    pjrpid_element rpid = {
        PJRPID_ELEMENT_TYPE_PERSON,
        CONST_PJ_STR("20"),
        PJRPID_ACTIVITY_UNKNOWN,
        CONST_PJ_STR("") // empty note by default
    };
    pjsip_pres_status fake_status_data;
    pj_bzero(&fake_status_data, sizeof(pjsip_pres_status));
    fake_status_data.info_cnt = 1;
    fake_status_data.info[0].basic_open = false;
    fake_status_data.info[0].id = CONST_PJ_STR("0"); /* todo: tuplie_id*/
    pj_memcpy(&fake_status_data.info[0].rpid, &rpid, sizeof(pjrpid_element));
    pjsip_pres_set_status(sub, &fake_status_data);

    /* Create and send the the first NOTIFY to active subscription: */
    pj_str_t stateStr = CONST_PJ_STR("");
    pjsip_tx_data *tdata = NULL;
    status = pjsip_pres_notify(sub, ev_state, &stateStr, &reason, &tdata);

    if (status == PJ_SUCCESS) {
        pres->fillDoc(tdata, &msg_data);
        status = pjsip_pres_send_request(sub, tdata);
    }

    if (status != PJ_SUCCESS) {
        JAMI_WARN("Unable to create/send NOTIFY %d", status);
        pjsip_pres_terminate(sub, PJ_FALSE);
        pres->unlock();
        return status;
    }

    pres->unlock();
    return PJ_TRUE;
#endif
}

pjsip_module PresSubServer::mod_presence_server = {
    NULL, NULL, /* prev, next.        */
    CONST_PJ_STR("mod-presence-server"), /* Name.        */
    -1, /* Id            */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,
    NULL, /* load()        */
    NULL, /* start()        */
    NULL, /* stop()        */
    NULL, /* unload()        */
    &pres_on_rx_subscribe_request, /* on_rx_request()    */
    NULL, /* on_rx_response()    */
    NULL, /* on_tx_request.    */
    NULL, /* on_tx_response()    */
    NULL, /* on_tsx_state()    */

};



PresSubServer::PresSubServer(SIPPresence * pres, pjsip_evsub *evsub, const char *remote, pjsip_dialog *d)
    : remote_(remote)
    , pres_(pres)
    , sub_(evsub)
    , dlg_(d)
    , expires_(-1)
    , approved_(false)
{}

PresSubServer::~PresSubServer()
{
    //TODO: check if evsub needs to be forced TERMINATED.
}

void PresSubServer::setExpires(int ms)
{
    expires_ = ms;
}

int PresSubServer::getExpires() const
{
    return expires_;
}

bool PresSubServer::matches(const char *s) const
{
    // servers match if they have the same remote uri and the account ID.
    return (!(strcmp(remote_, s))) ;
}

void PresSubServer::approve(bool flag)
{
    approved_ = flag;
    JAMI_DBG("Approve Presence_subscription_server for %s: %s.", remote_, flag ? "true" : "false");
    // attach the real status data
    pjsip_pres_set_status(sub_, pres_->getStatus());
}


void PresSubServer::notify()
{
    /* Only send NOTIFY once subscription is active. Some subscriptions
    * may still be in NULL (when app is adding a new buddy while in the
    * on_incoming_subscribe() callback) or PENDING (when user approval is
    * being requested) state and we don't send NOTIFY to these subs until
    * the user accepted the request.
    */
    if ((pjsip_evsub_get_state(sub_) == PJSIP_EVSUB_STATE_ACTIVE) && (approved_)) {
        JAMI_DBG("Notifying %s.", remote_);

        pjsip_tx_data *tdata;
        pjsip_pres_set_status(sub_, pres_->getStatus());

        if (pjsip_pres_current_notify(sub_, &tdata) == PJ_SUCCESS) {
            // add msg header and send
            pres_->fillDoc(tdata, NULL);
            pjsip_pres_send_request(sub_, tdata);
        } else {
            JAMI_WARN("Unable to create/send NOTIFY");
            pjsip_pres_terminate(sub_, PJ_FALSE);
        }
    }
}

} // namespace jami
