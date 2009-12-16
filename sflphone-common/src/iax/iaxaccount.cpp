/*
 *  Copyright (C) 2006-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
 */

#include "iaxaccount.h"
#include "iaxvoiplink.h"
#include "manager.h"

IAXAccount::IAXAccount (const AccountID& accountID)
        : Account (accountID, "iax2")
{
    _link = new IAXVoIPLink (accountID);
}


IAXAccount::~IAXAccount()
{
    delete _link;
    _link = NULL;
}

void IAXAccount::setVoIPLink()
{

}

int IAXAccount::registerVoIPLink()
{
    _link->init();

    // Stuff needed for IAX registration
    setHostname (Manager::instance().getConfigString (_accountID, HOSTNAME));
    setUsername (Manager::instance().getConfigString (_accountID, USERNAME));
    setPassword (Manager::instance().getConfigString (_accountID, PASSWORD));

    _link->sendRegister (_accountID);

    return SUCCESS;
}

int
IAXAccount::unregisterVoIPLink()
{
    _link->sendUnregister (_accountID);
    _link->terminate();

    return SUCCESS;
}

void
IAXAccount::loadConfig()
{
    // Account generic
    Account::loadConfig();
}
