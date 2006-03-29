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
#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>

class VoIPLink;
typedef std::string AccountID;
#define AccountNULL ""

/**
	@author Yan Morin 
  Class account is an interface to protocol account (sipaccount, aixaccount)
  It can be enable on loading or activate after.
  It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
*/
class Account{
public:
    Account(const AccountID& accountID);

    ~Account();

  /**
   * Register the account
   * @return false is an error occurs
   */
  virtual bool registerAccount() = 0;

  /**
   * Unregister the account
   * @return false is an error occurs
   */
  virtual bool unregisterAccount() = 0;

  /**
   * Init the voiplink to run (event listener)
   * @return false is an error occurs
   */
  virtual bool init() = 0;

  /**
   * Stop the voiplink to run (event listener)
   * @return false is an error occurs
   */
  virtual bool terminate() = 0;

private:
  /**
   * Create a unique voIPLink() depending on the protocol
   * Multiple call to this function do nothing (if the voiplink pointer is 0)
   * @return false if an error occurs
   */
  virtual bool createVoIPLink() = 0;

  /**
   * Account ID are assign in constructor and shall not changed
   */
  AccountID _accountID;
  /**
   * Voice over IP Link contains a listener thread and calls
   */
  VoIPLink* _link;

  /**
   * Tells if the link should be start on loading or not
   * Modified by the configuration
   */
  bool _shouldInitOnStart;

  /**
   * Tells if the link is enabled or not
   * Modified by init/terminate
   */
  bool _enabled;

  /**
   * Tells if the link is registered or not
   * Modified by unregister/register
   */
  bool _registered;
};

#endif
