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
#ifndef ACCOUNTCREATOR_H
#define ACCOUNTCREATOR_H

#include "account.h"

class Account;

/**
 * @file accountcreator.h
 * @brief Create protocol-specific account
 */
class AccountCreator{
public:
  ~AccountCreator();
  /**
   * Public account type
   */
  enum AccountType {SIP_ACCOUNT, IAX_ACCOUNT };
  
  /**
   * Create a new account or null
   * @param type type of the account
   * @param accountID   accountID (must be unique for each account)
   */
  static Account* createAccount(AccountType type, AccountID accountID);

private:
  /** Hidden constructor */
  AccountCreator();
};

#endif
