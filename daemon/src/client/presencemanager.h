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

#include <vector>
#include <string>

#include "sflphone.h"

class PresenceManager
{
    public:
        PresenceManager();
        void registerEvHandlers(struct sflph_pres_ev_handlers* evHandlers);

    // Methods
    public:
        /* Presence subscription/Notification. */
        void publish(const std::string& accountID, const bool& status, const std::string& note);
        void answerServerRequest(const std::string& uri, const bool& flag);
        void subscribeBuddy(const std::string& accountID, const std::string& uri, const bool& flag);
        std::vector<std::map<std::string, std::string> > getSubscriptions(const std::string& accountID);
        void setSubscriptions(const std::string& accountID, const std::vector<std::string>& uris);

    // Signals
    public:
        void newServerSubscriptionRequest(const std::string& remote);
        void serverError(const std::string& accountID, const std::string& error, const std::string& msg);
        void newBuddyNotification(const std::string& accountID, const std::string& buddyUri,
                                  const bool& status, const std::string& lineStatus);
        void subscriptionStateChanged(const std::string& accountID, const std::string& buddyUri,
                                  const bool& state);

    private:
        // Event handlers; needed by the library API
        struct sflph_pres_ev_handlers evHandlers_;
};

#endif //PRESENCEINT_H
