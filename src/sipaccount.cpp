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
#include "sipaccount.h"
#include "sipvoiplink.h"
#include "manager.h"

#define SIP_FULL_NAME      "SIP.fullName"
#define SIP_USER_PART      "SIP.userPart"
#define SIP_AUTH_NAME      "SIP.username"
#define SIP_PASSWORD       "SIP.password"
#define SIP_HOST_PART      "SIP.hostPart"
#define SIP_PROXY          "SIP.proxy"
#define SIP_STUN_SERVER    "STUN.STUNserver"
#define SIP_USE_STUN       "STUN.useStun"


SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
{
  createVoIPLink();
}


SIPAccount::~SIPAccount()
{
}

/* virtual Account function implementation */
bool
SIPAccount::createVoIPLink()
{
  if (!_link) {
    _link = new SIPVoIPLink(_accountID);
  }
  return (_link != 0 ? true : false);
}

bool
SIPAccount::registerAccount()
{
  if (_link) {
    init(); // init if not enable
    unregisterAccount();
    SIPVoIPLink* tmplink = dynamic_cast<SIPVoIPLink*> (_link);
    if (tmplink) {
      tmplink->setProxy(Manager::instance().getConfigString(_accountID,SIP_PROXY));
      tmplink->setUserPart(Manager::instance().getConfigString(_accountID,SIP_USER_PART));
      tmplink->setAuthName(Manager::instance().getConfigString(_accountID,SIP_AUTH_NAME));
      tmplink->setPassword(Manager::instance().getConfigString(_accountID,SIP_PASSWORD));
    }
    _registered = _link->setRegister();
  }
  return _registered;
}

bool
SIPAccount::unregisterAccount()
{
  if (_link && _registered) {
    _registered = _link->setUnregister();
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
  std::string section(_accountID);
  std::string type_str("string");
  std::string type_int("int");
  
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_TYPE, "SIP", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_ENABLE,"1", type_int));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_AUTO_REGISTER, "1", type_int));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_FULL_NAME, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_USER_PART, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_HOST_PART, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_AUTH_NAME, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_PASSWORD, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_PROXY, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_STUN_SERVER, "stun.fwdnet.net:3478", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_USE_STUN, "0", type_int));
}

void
SIPAccount::loadConfig() 
{
  _shouldInitOnStart = Manager::instance().getConfigInt(_accountID, CONFIG_ACCOUNT_ENABLE) ? true : false;
  _shouldRegisterOnStart = Manager::instance().getConfigInt(_accountID, CONFIG_ACCOUNT_AUTO_REGISTER) ? true : false;
}
