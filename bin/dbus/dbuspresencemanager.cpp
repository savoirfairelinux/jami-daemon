/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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

#include "dbuspresencemanager.h"
#include "presencemanager_interface.h"

DBusPresenceManager::DBusPresenceManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/PresenceManager")
{}

void
DBusPresenceManager::publish(const std::string& accountID, const bool& status, const std::string& note)
{
    libjami::publish(accountID, status, note);
}

void
DBusPresenceManager::answerServerRequest(const std::string& uri, const bool& flag)
{
    libjami::answerServerRequest(uri, flag);
}

void
DBusPresenceManager::subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag)
{
    libjami::subscribeBuddy(accountID, uri, flag);
}

auto
DBusPresenceManager::getSubscriptions(const std::string& accountID) -> decltype(libjami::getSubscriptions(accountID))
{
    return libjami::getSubscriptions(accountID);
}

void
DBusPresenceManager::setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    libjami::setSubscriptions(accountID, uris);
}
