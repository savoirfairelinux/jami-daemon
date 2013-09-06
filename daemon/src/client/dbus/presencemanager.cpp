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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "presencemanager.h"

#include <cerrno>
#include <sstream>

#include "logger.h"
#include "sip/sipaccount.h"
#include "manager.h"
#include "sip/sippresence.h"

namespace {
    const char* SERVER_PATH = "/org/sflphone/SFLphone/PresenceManager";
}

PresenceManager::PresenceManager(DBus::Connection& connection) :
    DBus::ObjectAdaptor(connection, SERVER_PATH)
{}

/**
 * Un/subscribe to buddySipUri for an accountID
 */
void
PresenceManager::subscribePresSubClient(const std::string& accountID, const std::string& uri, const bool& flag)
{

    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (!sipaccount)
        ERROR("Could not find account %s",accountID.c_str());
    else{
        DEBUG("%subscribePresence (acc:%s, buddy:%s)",flag? "S":"Uns", accountID.c_str(), uri.c_str());
        sipaccount->getPresence()->subscribePresSubClient(uri,flag);
    }
}

/**
 * push a presence for a account
 * Notify for IP2IP account and publish for PBX account
 */
void
PresenceManager::sendPresence(const std::string& accountID, const bool& status, const std::string& note)
{
    SIPAccount *sipaccount = Manager::instance().getSipAccount(accountID);
    if (!sipaccount)
        ERROR("Could not find account %s.",accountID.c_str());
    else{
        DEBUG("Send Presence (acc:%s, status %s).",accountID.c_str(),status? "online":"offline");
        sipaccount->getPresence()->sendPresence(status, note);
    }
}

/**
 * Accept or not a PresSubServer request for IP2IP account
 */
void
PresenceManager::approvePresSubServer(const std::string& uri, const bool& flag)
{
    SIPAccount *sipaccount = Manager::instance().getIP2IPAccount();
    if (!sipaccount)
        ERROR("Could not find account IP2IP");
    else{
        DEBUG("Approve presence (acc:IP2IP, serv:%s, flag:%s)", uri.c_str(), flag? "true":"false");
        sipaccount->getPresence()->approvePresSubServer(uri, flag);
    }
}
