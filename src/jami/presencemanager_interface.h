/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#ifndef LIBJAMI_PRESENCEMANAGERI_H
#define LIBJAMI_PRESENCEMANAGERI_H

#include "def.h"

#include <vector>
#include <map>
#include <string>
#include <memory>

#include "jami.h"
#include "presence_const.h"

namespace libjami {

[[deprecated("Replaced by registerSignalHandlers")]] LIBJAMI_PUBLIC void registerPresHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* Presence subscription/Notification. */
LIBJAMI_PUBLIC void publish(const std::string& accountID, bool status, const std::string& note);
LIBJAMI_PUBLIC void answerServerRequest(const std::string& uri, bool flag);
LIBJAMI_PUBLIC void subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getSubscriptions(
    const std::string& accountID);
LIBJAMI_PUBLIC void setSubscriptions(const std::string& accountID,
                                   const std::vector<std::string>& uris);

// Presence signal type definitions
struct LIBJAMI_PUBLIC PresenceSignal
{
    struct LIBJAMI_PUBLIC NewServerSubscriptionRequest
    {
        constexpr static const char* name = "NewServerSubscriptionRequest";
        using cb_type = void(const std::string& /*remote*/);
    };
    struct LIBJAMI_PUBLIC ServerError
    {
        constexpr static const char* name = "ServerError";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*error*/,
                             const std::string& /*msg*/);
    };
    struct LIBJAMI_PUBLIC NewBuddyNotification
    {
        constexpr static const char* name = "NewBuddyNotification";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*buddy_uri*/,
                             int /*status*/,
                             const std::string& /*line_status*/);
    };
    struct LIBJAMI_PUBLIC NearbyPeerNotification
    {
        constexpr static const char* name = "NearbyPeerNotification";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*buddy_uri*/,
                             int /*state*/,
                             const std::string& /*displayname*/);
    };
    struct LIBJAMI_PUBLIC SubscriptionStateChanged
    {
        constexpr static const char* name = "SubscriptionStateChanged";
        using cb_type = void(const std::string& /*account_id*/,
                             const std::string& /*buddy_uri*/,
                             int /*state*/);
    };
};

} // namespace libjami

#endif // PRESENCEMANAGERI_H
