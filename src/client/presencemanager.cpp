/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Simon Désaulniers <simon.desaulniers@savoirfairelinux.com>
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

#include "jamidht/jamiaccount.h"

namespace libjami {

using jami::SIPAccount;

void
registerPresHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

/**
 * Un/subscribe to buddySipUri for an accountID
 */
void
subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID)) {
        auto pres = sipaccount->getPresence();
        if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_SUBSCRIBE)) {
            JAMI_DBG("%subscribePresence (acc:%s, buddy:%s)",
                     flag ? "S" : "Uns",
                     accountID.c_str(),
                     uri.c_str());
            pres->subscribeClient(uri, flag);
        }
    } else if (auto ringaccount = jami::Manager::instance().getAccount<jami::JamiAccount>(
                   accountID)) {
        ringaccount->trackBuddyPresence(uri, flag);
    } else
        JAMI_ERR("Could not find account %s", accountID.c_str());
}

/**
 * push a presence for a account
 * Notify for IP2IP account and publish for PBX account
 */
void
publish(const std::string& accountID, bool status, const std::string& note)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID)) {
        auto pres = sipaccount->getPresence();
        if (pres and pres->isEnabled() and pres->isSupported(PRESENCE_FUNCTION_PUBLISH)) {
            JAMI_DBG("Send Presence (acc:%s, status %s).",
                     accountID.c_str(),
                     status ? "online" : "offline");
            pres->sendPresence(status, note);
        }
    } else
        JAMI_ERR("Could not find account %s.", accountID.c_str());
}

/**
 * Accept or not a PresSubServer request for IP2IP account
 */
void
answerServerRequest(UNUSED const std::string& uri, UNUSED bool flag)
{
#if 0 // DISABLED: removed IP2IP support, tuleap: #448
    auto account = jami::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount *>(account.get())) {
        JAMI_DBG("Approve presence (acc:IP2IP, serv:%s, flag:%s)", uri.c_str(),
                 flag ? "true" : "false");

        if (auto pres = sipaccount->getPresence())
            pres->approvePresSubServer(uri, flag);
        else
            JAMI_ERR("Presence not initialized");
    } else
        JAMI_ERR("Could not find account IP2IP");
#else
    JAMI_ERR("answerServerRequest() is deprecated and does nothing");
#endif
}

/**
 * Get all active subscriptions for "accountID"
 */
std::vector<std::map<std::string, std::string>>
getSubscriptions(const std::string& accountID)
{
    std::vector<std::map<std::string, std::string>> ret;

    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID)) {
        if (auto pres = sipaccount->getPresence()) {
            const auto& subs = pres->getClientSubscriptions();
            ret.reserve(subs.size());
            for (const auto& s : subs) {
                ret.push_back(
                    {{libjami::Presence::BUDDY_KEY, std::string(s->getURI())},
                     {libjami::Presence::STATUS_KEY, s->isPresent() ? libjami::Presence::ONLINE_KEY : libjami::Presence::OFFLINE_KEY},
                     {libjami::Presence::LINESTATUS_KEY, std::string(s->getLineStatus())}});
            }
        } else
            JAMI_ERR("Presence not initialized");
    } else if (auto ringaccount = jami::Manager::instance().getAccount<jami::JamiAccount>(
                   accountID)) {
        const auto& trackedBuddies = ringaccount->getTrackedBuddyPresence();
        ret.reserve(trackedBuddies.size());
        for (const auto& tracked_id : trackedBuddies) {
            ret.push_back(
                {{libjami::Presence::BUDDY_KEY, tracked_id.first},
                 {libjami::Presence::STATUS_KEY,
                  tracked_id.second ? libjami::Presence::ONLINE_KEY : libjami::Presence::OFFLINE_KEY}});
        }
    } else
        JAMI_ERR("Could not find account %s.", accountID.c_str());

    return ret;
}

/**
 * Batch subscribing of URIs
 */
void
setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID)) {
        if (auto pres = sipaccount->getPresence()) {
            for (const auto& u : uris)
                pres->subscribeClient(u, true);
        } else
            JAMI_ERR("Presence not initialized");
    } else if (auto ringaccount = jami::Manager::instance().getAccount<jami::JamiAccount>(
                   accountID)) {
        for (const auto& u : uris)
            ringaccount->trackBuddyPresence(u, true);
    } else
        JAMI_ERR("Could not find account %s.", accountID.c_str());
}

} // namespace libjami
