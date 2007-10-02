/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
#include "iaxaccount.h"
#include "account.h"
#include "iaxvoiplink.h"
#include "manager.h"

IAXAccount::IAXAccount(const AccountID& accountID)
 : Account(accountID)
{
  _link = new IAXVoIPLink(accountID);
}


IAXAccount::~IAXAccount()
{
  delete _link;
  _link = NULL;
}

void
IAXAccount::registerVoIPLink()
{
  init();
  //unregisterAccount(); No need to unregister first.
  IAXVoIPLink* thislink = dynamic_cast<IAXVoIPLink*> (_link);
  if (thislink) {
    // Stuff needed for IAX registration
    thislink->setHost(Manager::instance().getConfigString(_accountID, IAX_HOST));
    thislink->setUser(Manager::instance().getConfigString(_accountID, IAX_USER));
    thislink->setPass(Manager::instance().getConfigString(_accountID, IAX_PASS));
  }

  _link->sendRegister();
}

void
IAXAccount::unregisterVoIPLink()
{
  _link->sendUnregister();
}

bool
IAXAccount::init()
{
  _link->init();
  return true;
}

bool
IAXAccount::terminate()
{
  _link->terminate();
  return true;
}

void 
IAXAccount::initConfig(Conf::ConfigTree& config)
{
  /*
  std::string section(_accountID);
  std::string type_str("string");
  std::string type_int("int");

  // Account generic
  Account::initConfig(config);

  // IAX specific
  config.verifyConfigTreeItem(section, CONFIG_ACCOUNT_TYPE, "IAX", type_str);
  config.verifyConfigTreeItem(section, IAX_FULL_NAME, "", type_str);
  config.verifyConfigTreeItem(section, IAX_HOST, "", type_str);
  config.verifyConfigTreeItem(section, IAX_USER, "", type_str);
  config.verifyConfigTreeItem(section, IAX_PASS, "", type_str);
  */
}

void
IAXAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();

  // IAX specific
  //none
}
