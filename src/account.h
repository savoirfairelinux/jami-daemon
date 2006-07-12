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
#include "config/config.h"

class VoIPLink;

typedef std::string AccountID;
#define AccountNULL ""
#define CONFIG_ACCOUNT_TYPE   "Account.type"
#define CONFIG_ACCOUNT_ENABLE "Account.enable"
#define CONFIG_ACCOUNT_AUTO_REGISTER  "Account.autoregister"


/**
 * Class account is an interface to protocol account (sipaccount, aixaccount)
 * It can be enable on loading or activate after.
 * It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
 * @author Yan Morin 
 */
class Account{
public:
    Account(const AccountID& accountID);

    virtual ~Account();

  /**
   * Load the default properties for the account
   */
  virtual void initConfig(Conf::ConfigTree& config) = 0;
  virtual void loadConfig() = 0;

  /**
   * Get the account ID
   * @return constant account id
   */
  inline const AccountID& getAccountID() { return _accountID; }

  /**
   * Get the voiplink pointer
   * @return the pointer or 0
   */
  inline VoIPLink* getVoIPLink() { return _link; }

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

  /**
   * Tell if we should init the account on start
   * @return true if we must init the link
   */
  bool shouldInitOnStart() {return _shouldInitOnStart; }

  /**
   * Tell if we should init the account on start
   * @return true if we must init the link
   */
  bool shouldRegisterOnStart() {return _shouldRegisterOnStart; }

  /**
   * Tell if the account is enable or not
   */
  bool isEnabled() { return _enabled; }

private:
  /**
   * Create a unique voIPLink() depending on the protocol
   * Multiple call to this function do nothing (if the voiplink pointer is 0)
   * @return false if an error occurs
   */
  virtual bool createVoIPLink() = 0;

protected:
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
   * Tells if we should register automatically on startup
   * Modified by the configuration
   */
  bool _shouldRegisterOnStart;

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
