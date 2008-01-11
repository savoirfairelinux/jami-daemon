/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include "voiplink.h"
#include "manager.h"

Account::Account(const AccountID& accountID) : _accountID(accountID)
{
  _link = NULL;

  _enabled = false;
}


Account::~Account()
{
  // _link should be destroyed WHERE IT'S CREATED
  //delete _link;
  //_link = NULL;
}


void
Account::loadConfig() 
{
  _enabled = Manager::instance().getConfigInt(_accountID, CONFIG_ACCOUNT_ENABLE) ? true : false;
}

