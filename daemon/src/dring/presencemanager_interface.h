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

#ifndef PRESENCEMANAGERI_H
#define PRESENCEMANAGERI_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <map>
#include <string>
#include <functional>


namespace ring {

/* presence events */
struct ring_pres_ev_handlers
{
    std::function<void (const std::string& /*remote*/)> on_new_server_subscription_request;
    std::function<void (const std::string& /*account_id*/, const std::string& /*error*/, const std::string& /*msg*/)> on_server_error;
    std::function<void (const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*status*/, const std::string& /*line_status*/)> on_new_buddy_notification;
    std::function<void (const std::string& /*account_id*/, const std::string& /*buddy_uri*/, int /*state*/)> on_subscription_state_change;
};

class PresenceManagerI
{
    public:
        void registerEvHandlers(struct ring_pres_ev_handlers* evHandlers);

    // Methods
    public:
        /* Presence subscription/Notification. */
        void publish(const std::string& accountID, bool status, const std::string& note);
        void answerServerRequest(const std::string& uri, bool flag);
        void subscribeBuddy(const std::string& accountID, const std::string& uri, bool flag);
        std::vector<std::map<std::string, std::string> > getSubscriptions(const std::string& accountID);
        void setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris);
};

} // namespace ring

#endif //PRESENCEMANAGERI_H
