/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
#include "accountcreator.h"
#include "sipaccount.h"
#ifdef USE_IAX
#include "iaxaccount.h"
#endif

AccountCreator::AccountCreator()
{
}


AccountCreator::~AccountCreator()
{
}

Account* 
AccountCreator::createAccount(AccountType type, AccountID accountID)
{
  switch(type) {
    case SIP_ACCOUNT:
      return new SIPAccount(accountID);
    break;

    case IAX_ACCOUNT:
#ifdef USE_IAX
      return new IAXAccount(accountID);
#endif
    break;
  }
  return 0;
}

