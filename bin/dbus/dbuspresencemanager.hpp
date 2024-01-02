/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#pragma once

#include "dbuspresencemanager.adaptor.h"
#include <presencemanager_interface.h>

class DBusPresenceManager : public sdbus::AdaptorInterfaces<cx::ring::Ring::PresenceManager_adaptor>
{
public:
    DBusPresenceManager(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/PresenceManager")
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusPresenceManager()
    {
        unregisterAdaptor();
    }

    void
    publish(const std::string& accountID, const bool& status, const std::string& note)
    {
        libjami::publish(accountID, status, note);
    }

    void
    answerServerRequest(const std::string& uri, const bool& flag)
    {
        libjami::answerServerRequest(uri, flag);
    }

    void
    subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag)
    {
        libjami::subscribeBuddy(accountID, uri, flag);
    }

    auto
    getSubscriptions(const std::string& accountID) -> decltype(libjami::getSubscriptions(accountID))
    {
        return libjami::getSubscriptions(accountID);
    }

    void
    setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
    {
        libjami::setSubscriptions(accountID, uris);
    }

private:

    void
    registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using libjami::PresenceSignal;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        const std::map<std::string, SharedCallback> presEvHandlers = {
            exportable_serialized_callback<PresenceSignal::NewServerSubscriptionRequest>(
                std::bind(&DBusPresenceManager::emitNewServerSubscriptionRequest, this, _1)),
            exportable_serialized_callback<PresenceSignal::ServerError>(
                std::bind(&DBusPresenceManager::emitServerError, this, _1, _2, _3)),
            exportable_serialized_callback<PresenceSignal::NewBuddyNotification>(
                std::bind(&DBusPresenceManager::emitNewBuddyNotification, this, _1, _2, _3, _4)),
            exportable_serialized_callback<PresenceSignal::NearbyPeerNotification>(
                std::bind(&DBusPresenceManager::emitNearbyPeerNotification, this, _1, _2, _3, _4)),
            exportable_serialized_callback<PresenceSignal::SubscriptionStateChanged>(
                std::bind(&DBusPresenceManager::emitSubscriptionStateChanged, this, _1, _2, _3)),
        };

        libjami::registerSignalHandlers(presEvHandlers);
    }
};
