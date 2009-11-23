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

#include "account.h"
#include "manager.h"

Account::Account (const AccountID& accountID, std::string type) :
        _accountID (accountID)
        , _link (NULL)
        , _enabled (false)
        , _type (type)
{
    setRegistrationState (Unregistered);
}

Account::~Account()
{
}

void Account::loadConfig()
{
    std::string p;

    p =  Manager::instance().getConfigString (_accountID , CONFIG_ACCOUNT_TYPE);
#ifdef USE_IAX
    _enabled = (Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_ENABLE) == "true") ? true : false;
#else

    if (p.c_str() == "IAX")
        _enabled = false;
    else
        _enabled = (Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_ENABLE) == "true") ? true : false;

#endif
}

void Account::setRegistrationState (RegistrationState state)
{

    if (state != _registrationState) {
        _debug ("Account::setRegistrationState");
        _registrationState = state;

        // Notify the client
        Manager::instance().connectionStatusNotification();
    }
}
