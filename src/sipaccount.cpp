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
#include "sipaccount.h"
#include "sipvoiplink.h"
#include "manager.h"


SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
{
  _link = new SIPVoIPLink(accountID);
}


SIPAccount::~SIPAccount()
{
  delete _link;
}

bool
SIPAccount::registerVoIPLink()
{
  if (_link) {
    init(); // init if not enable
    unregisterVoIPLink();
    SIPVoIPLink* tmplink = dynamic_cast<SIPVoIPLink*> (_link);
    if (tmplink) {
      // Stuff needed for SIP registration.
      tmplink->setProxy   (Manager::instance().getConfigString(_accountID,SIP_PROXY));
      tmplink->setUserPart(Manager::instance().getConfigString(_accountID,SIP_USER_PART));
      tmplink->setAuthName(Manager::instance().getConfigString(_accountID,SIP_AUTH_NAME));
      tmplink->setPassword(Manager::instance().getConfigString(_accountID,SIP_PASSWORD));
    }
    _registered = _link->sendRegister();
  }
  return _registered;
}

bool
SIPAccount::unregisterVoIPLink()
{
  if (_link && _registered) {
    _registered = _link->sendUnregister();
  }
  return !_registered;
}

bool
SIPAccount::init()
{
  if (_link && !_enabled) {
    _link->setFullName(Manager::instance().getConfigString(_accountID,SIP_FULL_NAME));
    _link->setHostName(Manager::instance().getConfigString(_accountID,SIP_HOST_PART));
    int useStun = Manager::instance().getConfigInt(_accountID,SIP_USE_STUN);
    
    SIPVoIPLink* tmplink = dynamic_cast<SIPVoIPLink*> (_link);
    if (tmplink) {
      tmplink->setStunServer(Manager::instance().getConfigString(_accountID,SIP_STUN_SERVER));
      tmplink->setUseStun( useStun!=0 ? true : false);
    }
    _link->init();
    _enabled = true;
    return true;
  }
  return false;
}

bool
SIPAccount::terminate()
{
  if (_link && _enabled) {
    _link->terminate();
    _enabled = false;
    return true;
  }
  return false;
}

void 
SIPAccount::initConfig(Conf::ConfigTree& config)
{
  /*
  std::string section(_accountID);
  std::string type_str("string");
  std::string type_int("int");

  // Account generic
  Account::initConfig(config);

  // SIP specific
  config.verifyConfigTreeItem(section, CONFIG_ACCOUNT_TYPE, "SIP", type_str);
  config.verifyConfigTreeItem(section, SIP_FULL_NAME, "", type_str);
  config.verifyConfigTreeItem(section, SIP_USER_PART, "", type_str);
  config.verifyConfigTreeItem(section, SIP_HOST_PART, "", type_str);
  config.verifyConfigTreeItem(section, SIP_AUTH_NAME, "", type_str);
  config.verifyConfigTreeItem(section, SIP_PASSWORD, "", type_str);
  config.verifyConfigTreeItem(section, SIP_PROXY, "", type_str);
  config.verifyConfigTreeItem(section, SIP_STUN_SERVER, "stun.fwdnet.net:3478", type_str);
  config.verifyConfigTreeItem(section, SIP_USE_STUN, "0", type_int);
  */
}

void
SIPAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();

  // SIP specific
  //none
}
