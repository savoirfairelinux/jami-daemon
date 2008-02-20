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
#include "accountcreator.h"
#include "sipaccount.h"
#ifdef HAVE_IAX2
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
#ifdef HAVE_IAX2
    case IAX_ACCOUNT:
      return new IAXAccount(accountID);
    break;
#endif
  }
  return 0;
}

