/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "presencemanager_interface.h"

#include <cerrno>
#include <sstream>
#include <cstring>

#include "logger.h"
#include "manager.h"
#include "sip/sipaccount.h"
#include "sip/sippresence.h"
#include "sip/pres_sub_client.h"
#include "client/ring_signal.h"
#include "compiler_intrinsics.h"

namespace DRing {

using ring::SIPAccount;

constexpr static const char* STATUS_KEY     = "Status";
constexpr static const char* LINESTATUS_KEY = "LineStatus";
constexpr static const char* ONLINE_KEY     = "Online";
constexpr static const char* OFFLINE_KEY    = "Offline";

void
registerPresHandlers(const std::map<std::string,
                     std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

/**
 * Un/subscribe to buddySipUri for an accountID
 */
void
subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID)) {
        auto pres = sipaccount->getPresence();
        if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)) {
            RING_DBG("%subscribePresence (acc:%s, buddy:%s)",
                     flag ? "S" : "Uns", accountID.c_str(), uri.c_str());
            pres->subscribeClient(uri, flag);
        }
    } else
        RING_ERR("Could not find account %s", accountID.c_str());
}

/**
 * push a presence for a account
 * Notify for IP2IP account and publish for PBX account
 */
void
publish(const std::string& accountID, bool status, const std::string& note)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID)) {
        auto pres = sipaccount->getPresence();
        if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_PUBLISH)) {
            RING_DBG("Send Presence (acc:%s, status %s).", accountID.c_str(),
                     status ? "online" : "offline");
            pres->sendPresence(status, note);
        }
    } else
        RING_ERR("Could not find account %s.", accountID.c_str());
}

/**
 * Accept or not a PresSubServer request for IP2IP account
 */
void
answerServerRequest(UNUSED const std::string& uri, UNUSED bool flag)
{
#if 0 // DISABLED: removed IP2IP support, tuleap: #448
    auto account = ring::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount *>(account.get())) {
        RING_DBG("Approve presence (acc:IP2IP, serv:%s, flag:%s)", uri.c_str(),
                 flag ? "true" : "false");

        if (auto pres = sipaccount->getPresence())
            pres->approvePresSubServer(uri, flag);
        else
            RING_ERR("Presence not initialized");
    } else
        RING_ERR("Could not find account IP2IP");
#else
    RING_ERR("answerServerRequest() is deprecated and does nothing");
#endif
}

/**
 * Get all active subscriptions for "accountID"
 */
std::vector<std::map<std::string, std::string> >
getSubscriptions(const std::string& accountID)
{
    std::vector<std::map<std::string, std::string>> ret;

    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID)) {
        if (auto pres = sipaccount->getPresence()) {
            for (const auto& s : pres->getClientSubscriptions()) {
                ret.push_back({
                        {STATUS_KEY, s->isPresent() ? ONLINE_KEY : OFFLINE_KEY},
                        {LINESTATUS_KEY, s->getLineStatus()}
                    });
            }
        } else
            RING_ERR("Presence not initialized");
    } else
        RING_ERR("Could not find account %s.", accountID.c_str());

    return ret;
}

/**
 * Batch subscribing of URIs
 */
void
setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID)) {
        if (auto pres = sipaccount->getPresence()) {
            for (const auto &u : uris)
                pres->subscribeClient(u, true);
        } else
            RING_ERR("Presence not initialized");
    } else
        RING_ERR("Could not find account %s.", accountID.c_str());
}

} // namespace DRing
