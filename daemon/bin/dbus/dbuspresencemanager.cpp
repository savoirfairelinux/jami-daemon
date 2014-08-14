/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "sflphone.h"

#include "dbuspresencemanager.h"

DBusPresenceManager::DBusPresenceManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/org/sflphone/SFLphone/PresenceManager")
{
}

void DBusPresenceManager::publish(const std::string& accountID, const bool& status, const std::string& note)
{
    sflph_pres_publish(accountID, status, note);
}

void DBusPresenceManager::answerServerRequest(const std::string& uri, const bool& flag)
{
    sflph_pres_answer_server_request(uri, flag);
}

void DBusPresenceManager::subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag)
{
    sflph_pres_subscribe_buddy(accountID, uri, flag);
}

std::vector<std::map<std::string, std::string> > DBusPresenceManager::getSubscriptions(const std::string& accountID)
{
    return sflph_pres_get_subscriptions(accountID);
}

void DBusPresenceManager::setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris)
{
    sflph_pres_set_subscriptions(accountID, uris);
}
