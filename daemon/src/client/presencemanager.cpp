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

#include "logger.h"
#include "sip/sipaccount.h"
#include "manager.h"
#include "sip/sippresence.h"
#include "sip/pres_sub_client.h"

constexpr static const char* STATUS_KEY     = "Status";
constexpr static const char* LINESTATUS_KEY = "LineStatus";
constexpr static const char* ONLINE_KEY     = "Online";
constexpr static const char* OFFLINE_KEY    = "Offline";

/**
 * Un/subscribe to buddySipUri for an accountID
 */
void
PresenceManager::subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag)
{
    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (!sipaccount) {
        ERROR("Could not find account %s",accountID.c_str());
        return;
    }

    SIPPresence *pres = sipaccount->getPresence();
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
PresenceManager::publish(const std::string& accountID, const bool& status, const std::string& note)
{
    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (!sipaccount) {
        ERROR("Could not find account %s.",accountID.c_str());
        return;
    }

    SIPPresence *pres = sipaccount->getPresence();
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
PresenceManager::answerServerRequest(const std::string& uri, const bool& flag)
{
    SIPAccount *sipaccount = Manager::instance().getIP2IPAccount();
    if (!sipaccount) {
        ERROR("Could not find account IP2IP");
        return;
    }

    DEBUG("Approve presence (acc:IP2IP, serv:%s, flag:%s)", uri.c_str(),
          flag ? "true" : "false");
    sipaccount->getPresence()->approvePresSubServer(uri, flag);
}

/**
 * Get all active subscriptions for "accountID"
 */
std::vector<std::map<std::string, std::string> >
PresenceManager::getSubscriptions(const std::string& accountID)
{
    std::vector<std::map<std::string, std::string> > ret;
    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (sipaccount) {
        for (auto s : sipaccount->getPresence()->getClientSubscriptions()) {
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
    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (!sipaccount)
        return;

    for (const auto &u : uris)
        sipaccount->getPresence()->subscribeClient(u, true);
}
