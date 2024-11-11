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

%header %{

#include "jami/jami.h"
#include "jami/presencemanager_interface.h"

class PresenceCallback {
public:
    virtual ~PresenceCallback(){}
    virtual void newServerSubscriptionRequest(const std::string& /*remote*/){}
    virtual void serverError(const std::string& /*account_id*/, const std::string& /*error*/, const std::string& /*msg*/){}
    virtual void newBuddyNotification(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*status*/, const std::string& /*line_status*/){}
    virtual void nearbyPeerNotification(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/, const std::string& /*displayname*/){}
    virtual void subscriptionStateChanged(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/){}
};
%}

%feature("director") PresenceCallback;

namespace libjami {

/* Presence subscription/Notification. */
void publish(const std::string& accountID, bool status, const std::string& note);
void answerServerRequest(const std::string& uri, bool flag);
void subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag);
std::vector<std::map<std::string, std::string>> getSubscriptions(const std::string& accountID);
void setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris);
}

class PresenceCallback {
public:
    virtual ~PresenceCallback(){}
    virtual void newServerSubscriptionRequest(const std::string& /*remote*/){}
    virtual void serverError(const std::string& /*account_id*/, const std::string& /*error*/, const std::string& /*msg*/){}
    virtual void newBuddyNotification(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*status*/, const std::string& /*line_status*/){}
    virtual void nearbyPeerNotification(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/, const std::string& /*displayname*/){}
    virtual void subscriptionStateChanged(const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/){}
};