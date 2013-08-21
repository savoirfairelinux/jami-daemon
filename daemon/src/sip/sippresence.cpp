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
#include "sipaccount.h"
#include "sipbuddy.h"
#include "sippublish.h"
#include "sippresence.h"
#include "presence_subscription.h"
#include "sipvoiplink.h"



SIPPresence::SIPPresence(SIPAccount *acc)
    : pres_status_data()
    , online_status()
    , publish_sess()
    , publish_state()
    , publish_enabled(true)
    , acc_(acc)
    , serverSubscriptions_ ()
    , buddies_ ()
    , mutex_()
    , mutex_nesting_level_()
    , mutex_owner_()
    , pool_()
    , cp_()
{
    /* init default status */
    updateStatus("open","Available");

    /* init pool */
    pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
    pool_ = pj_pool_create(&cp_.factory, "pres", 1000, 1000, NULL);

    /* Create mutex */
    if(pj_mutex_create_recursive(pool_, "pres",&mutex_) != PJ_SUCCESS)
	ERROR("Unable to create mutex");
}


SIPPresence::~SIPPresence(){
    /* Flush the lists */
    std::list< PresenceSubscription *>::iterator serverIt;
    std::list< SIPBuddy *>::iterator buddyIt;
       for (buddyIt = buddies_.begin(); buddyIt != buddies_.end(); buddyIt++)
        delete *buddyIt;
    for (serverIt = serverSubscriptions_.begin(); serverIt != serverSubscriptions_.end(); serverIt++)
        delete *serverIt;
}

SIPAccount * SIPPresence::getAccount(){
    return acc_;
}

pjsip_pres_status * SIPPresence::getStatus(){
    return &pres_status_data;
}

int SIPPresence::getModId(){
    return  ((SIPVoIPLink*) (acc_->getVoIPLink()))->getModId();
}

pj_pool_t*  SIPPresence::getPool(){
    return pool_;
}

void SIPPresence::updateStatus(const std::string &status, const std::string &note){
    //char* pj_note  = (char*) pj_pool_alloc(pool_, "50");

    pjrpid_element rpid = {PJRPID_ELEMENT_TYPE_PERSON,
            pj_str("20"),
            PJRPID_ACTIVITY_UNKNOWN,
            pj_str((char *) note.c_str())};
            //pj_str(strdup(note.c_str()))}; /*TODO : del strdup*/

    /* fill activity if user not available. */
    if(note=="away")
        rpid.activity = PJRPID_ACTIVITY_AWAY;
    else if (note=="busy")
        rpid.activity = PJRPID_ACTIVITY_BUSY;
    else
        DEBUG("Presence : no activity");

    pj_bzero(&pres_status_data, sizeof(pres_status_data));
    pres_status_data.info_cnt = 1;
    pres_status_data.info[0].basic_open = (status == "open")? true: false;
    pres_status_data.info[0].id = pj_str("0"); /* todo: tuplie_id*/
    pj_memcpy(&pres_status_data.info[0].rpid, &rpid,sizeof(pjrpid_element));
    /* "contact" field is optionnal */
}

void SIPPresence::sendPresence(const std::string &status, const std::string &note){
    updateStatus(status,note);
    if (acc_->isIP2IP())
        notifyServerSubscription(); // to each subscribers
    else
        pres_publish(this); // to sipvoip server
}


void SIPPresence::reportBuddy(const std::string& buddySipUri, pjsip_pres_status * status){
    /* Update our info. See pjsua_buddy_get_info() for additionnal ideas*/
    const std::string basic(status->info[0].basic_open ? "open" : "closed");
    const std::string note(status->info[0].rpid.note.ptr,status->info[0].rpid.note.slen);
    DEBUG(" Received presenceStateChange for %s status=%s note=%s",buddySipUri.c_str(),basic.c_str(),note.c_str());
    /* report status to client signal */
    Manager::instance().getClient()->getCallManager()->newPresenceNotification(buddySipUri, basic, note);
}

void SIPPresence::subscribeBuddy(const std::string& buddySipUri){
    std::list< SIPBuddy *>::iterator buddyIt;
    for (buddyIt = buddies_.begin(); buddyIt != buddies_.end(); buddyIt++)
        if((*buddyIt)->getURI()==buddySipUri){
            DEBUG("-Buddy:%s exist in the list. Replace it.",buddySipUri.c_str());
            delete *buddyIt;
            removeBuddy(*buddyIt);
            break;
        }
    SIPBuddy *b = new SIPBuddy(buddySipUri, acc_);
    if(!(b->subscribe())){
        WARN("Failed to add buddy.");
        delete b;
    }
}

void SIPPresence::unsubscribeBuddy(const std::string& buddySipUri){
    std::list< SIPBuddy *>::iterator buddyIt;
    for (buddyIt = buddies_.begin(); buddyIt != buddies_.end(); buddyIt++)
        if((*buddyIt)->getURI()==buddySipUri){
            DEBUG("-Found buddy:%s in the buddy list.",buddySipUri.c_str());
            delete *buddyIt;
            removeBuddy(*buddyIt);
            return;
        }
}

void SIPPresence::addBuddy(SIPBuddy *b){
    DEBUG("-New buddy subscription added in the buddy list.");
    buddies_.push_back(b);
}
void SIPPresence::removeBuddy(SIPBuddy *b){
    DEBUG("-Buddy subscription removed from the buddy list.");
    buddies_.remove(b);
}


void SIPPresence::reportNewServerSubscription(PresenceSubscription *s){
    //newPresenceSubscription_ = s;
    Manager::instance().getClient()->getCallManager()->newPresenceSubscription(s->remote);
}

void SIPPresence::approveServerSubscription(const bool& flag, const std::string& uri){
    std::list< PresenceSubscription *>::iterator serverIt;
    for (serverIt = serverSubscriptions_.begin(); serverIt != serverSubscriptions_.end(); serverIt++){
         if((*serverIt)->matches((char *) uri.c_str())){
             DEBUG("-Approve subscription for %s.",(*serverIt)->remote);
             (*serverIt)->approve(flag);
             // return; // 'return' would prevent multiple-time subscribers from spam
         }
    }
}


void SIPPresence::addServerSubscription(PresenceSubscription *s) {
    DEBUG("-PresenceServer subscription added: %s.",s->remote);
    serverSubscriptions_.push_back(s);
}

void SIPPresence::removeServerSubscription(PresenceSubscription *s) {
    serverSubscriptions_.remove(s);
    DEBUG("-PresenceServer removed");
}

void SIPPresence::notifyServerSubscription() {
    std::list< PresenceSubscription *>::iterator serverIt;
    DEBUG("-Iterating through PresenceServers:");
    for (serverIt = serverSubscriptions_.begin(); serverIt != serverSubscriptions_.end(); serverIt++)
        (*serverIt)->notify();
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

bool SIPPresence::tryLock()
{
    pj_status_t status;
    status = pj_mutex_trylock(mutex_);
    if (status == PJ_SUCCESS) {
	mutex_owner_ = pj_thread_this();
	++mutex_nesting_level_;
    }
    return status;
}

bool SIPPresence::isLocked()
{
    return mutex_owner_ == pj_thread_this();
}

void SIPPresence::fillDoc(pjsip_tx_data *tdata, const pres_msg_data *msg_data)
{

    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
        const pj_str_t STR_USER_AGENT = pj_str("User-Agent");
        std::string useragent(acc_->getUserAgentName());
        pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
        pjsip_hdr *h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool, &STR_USER_AGENT, &pJuseragent);
        pjsip_msg_add_hdr(tdata->msg, h);

    /*pjsip_hdr hdr_list;
    pj_list_init(&hdr_list);
    std::string useragent(account->getUserAgentName());
    pj_str_t pJuseragent = pj_str((char*) useragent.c_str());
    const pj_str_t STR_USER_AGENT = { (char*) "User-Agent", 10 };
    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create(pool_, &STR_USER_AGENT, &pJuseragent);
    */
    }

    if(msg_data == NULL)
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
