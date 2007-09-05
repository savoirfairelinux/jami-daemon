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
#include "iaxaccount.h"
#include "account.h"
#include "iaxvoiplink.h"
#include "manager.h"

#define IAX_FULL_NAME  "IAX.fullName"
#define IAX_HOST       "IAX.host"
#define IAX_USER       "IAX.user"
#define IAX_PASS       "IAX.pass"

IAXAccount::IAXAccount(const AccountID& accountID)
 : Account(accountID)
{
  createVoIPLink();
}


IAXAccount::~IAXAccount()
{
}

/* virtual Account function implementation */
bool
IAXAccount::createVoIPLink()
{
  if (!_link) {
    _link = new IAXVoIPLink(_accountID);
  }
  return (_link != 0 ? true : false);
}

bool
IAXAccount::registerAccount()
{
  if (_link && !_registered) {
    init();
    //unregisterAccount(); No need to unregister first.
    IAXVoIPLink* tmplink = dynamic_cast<IAXVoIPLink*> (_link);
    if (tmplink) {
      // Stuff needed for IAX registration
      tmplink->setHost(Manager::instance().getConfigString(_accountID,IAX_HOST));
      tmplink->setUser(Manager::instance().getConfigString(_accountID,IAX_USER));
      tmplink->setPass(Manager::instance().getConfigString(_accountID,IAX_PASS));
    }
    _registered = _link->setRegister();
  }
  return _registered;
}

bool
IAXAccount::unregisterAccount()
{
  if (_link && _registered) {
    _registered = _link->setUnregister();
  }
  return !_registered;
}

bool
IAXAccount::init()
{
  if (_link && !_enabled) {
    _link->init();
    _enabled = true;
    return true;
  }
  return false;
}

bool
IAXAccount::terminate()
{
  if (_link && _enabled) {
    _link->terminate();
    _enabled = false;
    return true;
  }
  return false;
}

void 
IAXAccount::initConfig(Conf::ConfigTree& config)
{
  std::string section(_accountID);
  std::string type_str("string");
  std::string type_int("int");

  // Account generic
  Account::initConfig(config);

  // IAX specific
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_TYPE, "IAX", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(IAX_FULL_NAME, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(IAX_HOST, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(IAX_USER, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(IAX_PASS, "", type_str));
}

void
IAXAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();

  // IAX specific
  //none
}
