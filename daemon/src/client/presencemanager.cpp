/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "presencemanager.h"

#include <cerrno>
#include <sstream>
#include <cstring>

#include "logger.h"
#include "manager.h"
#include "sip/sipaccount.h"
#include "sip/sippresence.h"
#include "sip/pres_sub_client.h"

constexpr static const char* STATUS_KEY     = "Status";
constexpr static const char* LINESTATUS_KEY = "LineStatus";
constexpr static const char* ONLINE_KEY     = "Online";
constexpr static const char* OFFLINE_KEY    = "Offline";

void PresenceManager::registerEvHandlers(struct sflph_pres_ev_handlers* evHandlers)
{
    evHandlers_ = *evHandlers;
}

/**
 * Un/subscribe to buddySipUri for an accountID
 */
void
PresenceManager::subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag)
{
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    if (!sipaccount) {
        ERROR("Could not find account %s", accountID.c_str());
        return;
    }

    auto pres = sipaccount->getPresence();

    if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)) {
        DEBUG("%subscribePresence (acc:%s, buddy:%s)", flag ? "S" : "Uns",
              accountID.c_str(), uri.c_str());
        pres->subscribeClient(uri, flag);
    }
}

/**
 * push a presence for a account
 * Notify for IP2IP account and publish for PBX account
 */
void
PresenceManager::publish(const std::string& accountID, bool status, const std::string& note)
{
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    if (!sipaccount) {
        ERROR("Could not find account %s.", accountID.c_str());
        return;
    }

    auto pres = sipaccount->getPresence();

    if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_PUBLISH)) {
        DEBUG("Send Presence (acc:%s, status %s).", accountID.c_str(),
              status ? "online" : "offline");
        pres->sendPresence(status, note);
    }
}

/**
 * Accept or not a PresSubServer request for IP2IP account
 */
void
PresenceManager::answerServerRequest(const std::string& uri, bool flag)
{
    const auto account = Manager::instance().getIP2IPAccount();
    const auto sipaccount = static_cast<SIPAccount *>(account.get());

    if (!sipaccount) {
        ERROR("Could not find account IP2IP");
        return;
    }

    DEBUG("Approve presence (acc:IP2IP, serv:%s, flag:%s)", uri.c_str(),
          flag ? "true" : "false");

    auto pres = sipaccount->getPresence();

    if (!pres) {
        ERROR("Presence not initialized");
        return;
    }

    pres->approvePresSubServer(uri, flag);
}

/**
 * Get all active subscriptions for "accountID"
 */
std::vector<std::map<std::string, std::string> >
PresenceManager::getSubscriptions(const std::string& accountID)
{
    std::vector<std::map<std::string, std::string> > ret;
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    if (sipaccount) {
        const auto pres = sipaccount->getPresence();

        if (!pres) {
            ERROR("Presence not initialized");
            return ret;
        }

        for (const auto& s : pres->getClientSubscriptions()) {
            std::map<std::string, std::string> sub;
            sub[ STATUS_KEY     ] = s->isPresent() ? ONLINE_KEY : OFFLINE_KEY;
            sub[ LINESTATUS_KEY ] = s->getLineStatus();
            ret.push_back(sub);
        }
    }

    return ret;
}

/**
 * Batch subscribing of URIs
 */
void
PresenceManager::setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    const auto sipaccount = Manager::instance().getAccount<SIPAccount>(accountID);

    if (!sipaccount)
        return;

    auto pres = sipaccount->getPresence();

    if (!pres) {
        ERROR("Presence not initialized");
        return;
    }

    for (const auto &u : uris)
        pres->subscribeClient(u, true);
}

void PresenceManager::newServerSubscriptionRequest(const std::string& remote)
{
    if (evHandlers_.on_new_server_subscription_request) {
        evHandlers_.on_new_server_subscription_request(remote);
    }
}

void PresenceManager::serverError(const std::string& accountID, const std::string& error, const std::string& msg)
{
    if (evHandlers_.on_server_error) {
        evHandlers_.on_server_error(accountID, error, msg);
    }
}

void PresenceManager::newBuddyNotification(const std::string& accountID, const std::string& buddyUri,
                          bool status, const std::string& lineStatus)
{
    if (evHandlers_.on_new_buddy_notification) {
        evHandlers_.on_new_buddy_notification(accountID, buddyUri, status, lineStatus);
    }
}

void PresenceManager::subscriptionStateChanged(const std::string& accountID, const std::string& buddyUri,
                          bool state)
{
    if (evHandlers_.on_subscription_state_change) {
        evHandlers_.on_subscription_state_change(accountID, buddyUri, state);
    }
}
