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


#include "logger.h"
#include "manager.h"
#include "client/client.h"
#include "client/callmanager.h"
#include "client/presencemanager.h"
#include "sipaccount.h"
#include "sippublish.h"
#include "sippresence.h"
#include "pres_sub_server.h"
#include "pres_sub_client.h"
#include "sipvoiplink.h"

#define MAX_N_PRES_SUB_SERVER 20
#define MAX_N_PRES_SUB_CLIENT 20

SIPPresence::SIPPresence(SIPAccount *acc)
    : publish_sess()
    , pres_status_data_()
    , enabled_(true)
    , acc_(acc)
    , pres_sub_server_list_()  //IP2IP context
    , pres_sub_client_list_()
    , mutex_()
    , mutex_nesting_level_()
    , mutex_owner_()
    , cp_()
    , pool_()
{
    /* init default status */
    updateStatus(true, "Available");

    /* init pool */
    pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
    pool_ = pj_pool_create(&cp_.factory, "pres", 1000, 1000, NULL);

    /* Create mutex */
    if (pj_mutex_create_recursive(pool_, "pres", &mutex_) != PJ_SUCCESS)
        ERROR("Unable to create mutex");
}


SIPPresence::~SIPPresence()
{
    /* Flush the lists */
    for (const auto &c : pres_sub_client_list_)
        removePresSubClient(c) ;

    for (const auto &s : pres_sub_server_list_)
        removePresSubServer(s);
}

SIPAccount * SIPPresence::getAccount() const
{
    return acc_;
}

pjsip_pres_status * SIPPresence::getStatus()
{
    return &pres_status_data_;
}

int SIPPresence::getModId() const
{
    return ((SIPVoIPLink*)(acc_->getVoIPLink()))->getModId();
}

pj_pool_t*  SIPPresence::getPool() const
{
    return pool_;
}

void SIPPresence::enable(bool flag)
{
    enabled_ = flag;
}

void SIPPresence::updateStatus(bool status, const std::string &note)
{
    //char* pj_note  = (char*) pj_pool_alloc(pool_, "50");

    pjrpid_element rpid = {
        PJRPID_ELEMENT_TYPE_PERSON,
        pj_str("20"),
        PJRPID_ACTIVITY_UNKNOWN,
        pj_str((char *) note.c_str())
    };

    /* fill activity if user not available. */
    if (note == "away")
        rpid.activity = PJRPID_ACTIVITY_AWAY;
    else if (note == "busy")
        rpid.activity = PJRPID_ACTIVITY_BUSY;
    else // TODO: is there any other possibilities
        DEBUG("Presence : no activity");

    pj_bzero(&pres_status_data_, sizeof(pres_status_data_));
    pres_status_data_.info_cnt = 1;
    pres_status_data_.info[0].basic_open = status;
    pres_status_data_.info[0].id = pj_str("0"); /* todo: tuplie_id*/
    pj_memcpy(&pres_status_data_.info[0].rpid, &rpid, sizeof(pjrpid_element));
    /* "contact" field is optionnal */
}

void SIPPresence::sendPresence(bool status, const std::string &note)
{
    updateStatus(status, note);

    if (not enabled_)
        return;

    if (acc_->isIP2IP())
        notifyPresSubServer(); // to each subscribers
    else
        pres_publish(this); // to the PBX server
}


void SIPPresence::reportPresSubClientNotification(const std::string& uri, pjsip_pres_status * status)
{
    /* Update our info. See pjsua_buddy_get_info() for additionnal ideas*/
    const std::string basic(status->info[0].basic_open ? "open" : "closed");
    const std::string note(status->info[0].rpid.note.ptr, status->info[0].rpid.note.slen);
    DEBUG(" Received status of PresSubClient  %s: status=%s note=%s", uri.c_str(), (status->info[0].basic_open ? "open" : "closed"), note.c_str());
    /* report status to client signal */
    Manager::instance().getClient()->getPresenceManager()->newBuddySubscription(uri, status->info[0].basic_open, note);
}

void SIPPresence::subscribeClient(const std::string& uri, bool flag)
{
    /* Check if the buddy was already subscribed */
    for (const auto &c : pres_sub_client_list_)
        if (c->getURI() == uri) {
            DEBUG("-PresSubClient:%s exists in the list. Replace it.", uri.c_str());
            delete c;
            removePresSubClient(c);
            break;
        }

    if (pres_sub_client_list_.size() >= MAX_N_PRES_SUB_CLIENT) {
        WARN("Can't add PresSubClient, max number reached.");
        return;
    }

    if (flag) {
        PresSubClient *c = new PresSubClient(uri, this);

        if (!(c->subscribe())) {
            WARN("Failed send subscribe.");
            delete c;
        }

        // the buddy has to be accepted before being added in the list
    }
}

void SIPPresence::addPresSubClient(PresSubClient *c)
{
    if (pres_sub_client_list_.size() < MAX_N_PRES_SUB_CLIENT) {
        pres_sub_client_list_.push_back(c);
        DEBUG("New Presence_subscription_client client added in the list[l=%i].", pres_sub_client_list_.size());
    } else {
        WARN("Max Presence_subscription_client is reach.");
        // let the client alive //delete c;
    }
}

void SIPPresence::removePresSubClient(PresSubClient *c)
{
    DEBUG("Presence_subscription_client removed from the buddy list.");
    pres_sub_client_list_.remove(c);
}


void SIPPresence::reportnewServerSubscriptionRequest(PresSubServer *s)
{
    Manager::instance().getClient()->getPresenceManager()->newServerSubscriptionRequest(s->remote);
}

void SIPPresence::approvePresSubServer(const std::string& uri, bool flag)
{
    for (const auto &s : pres_sub_server_list_)
        if (s->matches((char *) uri.c_str())) {
            DEBUG("Approve Presence_subscription_server for %s: %s.", s->remote, flag ? "true" : "false");
            s->approve(flag);
            // return; // 'return' would prevent multiple-time subscribers from spam
        }
}


void SIPPresence::addPresSubServer(PresSubServer *s)
{
    if (pres_sub_server_list_.size() < MAX_N_PRES_SUB_SERVER) {
        DEBUG("Presence_subscription_server added: %s.", s->remote);
        pres_sub_server_list_.push_back(s);
    } else {
        WARN("Max Presence_subscription_server is reach.");
        // let de server alive // delete s;
    }
}

void SIPPresence::removePresSubServer(PresSubServer *s)
{
    pres_sub_server_list_.remove(s);
    DEBUG("Presence_subscription_server removed");
}

void SIPPresence::notifyPresSubServer()
{
    DEBUG("Iterating through Presence_subscription_server:");

    for (const auto &s : pres_sub_server_list_)
        s->notify();
}

void SIPPresence::lock()
{
    pj_mutex_lock(mutex_);
    mutex_owner_ = pj_thread_this();
    ++mutex_nesting_level_;
}

void SIPPresence::unlock()
{
    if (--mutex_nesting_level_ == 0)
        mutex_owner_ = NULL;

    pj_mutex_unlock(mutex_);
}

void SIPPresence::fillDoc(pjsip_tx_data *tdata, const pres_msg_data *msg_data)
{

    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
        const pj_str_t STR_USER_AGENT = pj_str("User-Agent");
        std::string useragent(acc_->getUserAgentName());
        pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
        pjsip_hdr *h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool, &STR_USER_AGENT, &pJuseragent);
        pjsip_msg_add_hdr(tdata->msg, h);
    }

    if (msg_data == NULL)
        return;

    const pjsip_hdr *hdr;
    hdr = msg_data->hdr_list.next;

    while (hdr && hdr != &msg_data->hdr_list) {
        pjsip_hdr *new_hdr;
        new_hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr);
        DEBUG("adding header", new_hdr->name.ptr);
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
