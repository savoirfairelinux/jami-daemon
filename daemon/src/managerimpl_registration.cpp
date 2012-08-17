/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "managerimpl.h"

#include "account.h"
#include "dbus/callmanager.h"
#include "global.h"
#include "logger.h"

#include "audio/audiolayer.h"
#include "sip/sipvoiplink.h"
#include "manager.h"
#include "dbus/configurationmanager.h"

#include <cstdlib>

void
ManagerImpl::registerAccounts()
{
    AccountMap concatenatedMap;
    fillConcatAccountMap(concatenatedMap);

    for (AccountMap::iterator iter = concatenatedMap.begin(); iter != concatenatedMap.end(); ++iter) {
        Account *a = iter->second;

        if (!a)
            continue;

        a->loadConfig();

        if (a->isEnabled())
            a->registerVoIPLink();
    }
}


VoIPLink* ManagerImpl::getAccountLink(const std::string& accountID)
{
    Account *account = getAccount(accountID);
    if(account == NULL) {
        ERROR("Could not find account for voip link, returning sip voip");
        return SIPVoIPLink::instance();
    }

    if (not accountID.empty())
        return account->getVoIPLink();
    else {
        ERROR("Account id is empty for voip link, returning sip voip");
        return SIPVoIPLink::instance();
    }
}


void
ManagerImpl::sendRegister(const std::string& accountID, bool enable)
{
    Account* acc = getAccount(accountID);

    acc->setEnabled(enable);
    acc->loadConfig();

    Manager::instance().saveConfig();

    if (acc->isEnabled())
        acc->registerVoIPLink();
    else
        acc->unregisterVoIPLink();
}
