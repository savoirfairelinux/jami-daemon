/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sip/sipaccount.h"
#include "sip/sippresence.h"
#include "logger.h"
#include "pres_sub_server.h"
#include "connectivity/sip_utils.h"
#include "compiler_intrinsics.h"

namespace jami {

using sip_utils::CONST_PJ_STR;

/* Callback called when *server* subscription state has changed. */
void
PresSubServer::pres_evsub_on_srv_state(UNUSED pjsip_evsub* sub, UNUSED pjsip_event* event)
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
        JAMI_ERR("Unable to find account IP2IP");
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
PresSubServer::pres_on_rx_subscribe_request(pjsip_rx_data* rdata)
{
    /* Only hande incoming subscribe messages should be processed here.
     * Otherwise we return FALSE to let other modules handle it */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, pjsip_get_subscribe_method()) != 0)
        return PJ_FALSE;

    JAMI_ERR("PresSubServer::pres_evsub_on_srv_state() is deprecated and does nothing");
    return PJ_FALSE;
}

pjsip_module PresSubServer::mod_presence_server = {
    NULL,
    NULL,                                /* prev, next.        */
    CONST_PJ_STR("mod-presence-server"), /* Name.        */
    -1,                                  /* Id            */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,
    NULL,                          /* load()        */
    NULL,                          /* start()        */
    NULL,                          /* stop()        */
    NULL,                          /* unload()        */
    &pres_on_rx_subscribe_request, /* on_rx_request()    */
    NULL,                          /* on_rx_response()    */
    NULL,                          /* on_tx_request.    */
    NULL,                          /* on_tx_response()    */
    NULL,                          /* on_tsx_state()    */

};

PresSubServer::PresSubServer(SIPPresence* pres, pjsip_evsub* evsub, const char* remote, pjsip_dialog* d)
    : remote_(remote)
    , pres_(pres)
    , sub_(evsub)
    , dlg_(d)
    , expires_(-1)
    , approved_(false)
{}

PresSubServer::~PresSubServer()
{
    // TODO: check if evsub needs to be forced TERMINATED.
}

void
PresSubServer::setExpires(int ms)
{
    expires_ = ms;
}

int
PresSubServer::getExpires() const
{
    return expires_;
}

bool
PresSubServer::matches(const char* s) const
{
    // servers match if they have the same remote uri and the account ID.
    return (!(strcmp(remote_, s)));
}

void
PresSubServer::approve(bool flag)
{
    approved_ = flag;
    JAMI_DBG("Approve Presence_subscription_server for %s: %s.", remote_, flag ? "true" : "false");
    // attach the real status data
    pjsip_pres_set_status(sub_, pres_->getStatus());
}

void
PresSubServer::notify()
{
    /* Only send NOTIFY once subscription is active. Some subscriptions
     * may still be in NULL (when app is adding a new buddy while in the
     * on_incoming_subscribe() callback) or PENDING (when user approval is
     * being requested) state and we don't send NOTIFY to these subs until
     * the user accepted the request.
     */
    if ((pjsip_evsub_get_state(sub_) == PJSIP_EVSUB_STATE_ACTIVE) && (approved_)) {
        JAMI_DBG("Notifying %s.", remote_);

        pjsip_tx_data* tdata;
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
