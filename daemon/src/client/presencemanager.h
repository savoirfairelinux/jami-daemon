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

#ifndef PRESENCEINT_H
#define PRESENCEINT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_DBUS

#include "dbus/dbus_cpp.h"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"
#include "dbus/presencemanager-glue.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"
#pragma GCC diagnostic warning "-Weffc++"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#else
// these includes normally come with DBus C++
#include <vector>
#include <map>
#include <string>
#endif // HAVE_DBUS

class PresenceManager
#if HAVE_DBUS
    : public org::sflphone::SFLphone::PresenceManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
#endif
{
    public:
#if HAVE_DBUS
        PresenceManager(DBus::Connection& connection);
#else
        PresenceManager();
#endif

    /* the following signals must be implemented manually for any
     * platform or configuration that does not supply dbus */
#if !HAVE_DBUS
    void newServerSubscriptionRequest(const std::string& remote);
    void serverError(const std::string& accountID, const std::string& error, const std::string& msg);
    void newBuddyNotification(const std::string& accountID, const std::string& buddyUri,
                              const bool& status, const std::string& lineStatus);
    void subscriptionStateChanged(const std::string& accountID, const std::string& buddyUri,
                              const bool& state);
#endif // !HAVE_DBUS

    /* Presence subscription/Notification. */
    void publish(const std::string& accountID, const bool& status, const std::string& note);
    void answerServerRequest(const std::string& uri, const bool& flag);
    void subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag);
    std::vector<std::map<std::string, std::string> > getSubscriptions(const std::string& accountID);
    void setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris);

};

#endif //CONFIGURATIONMANAGER_H
