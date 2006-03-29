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
#define SIP_FULL_NAME      "SIP.fullName"
#define SIP_USER_PART      "SIP.userPart"
#define SIP_AUTH_USER_NAME "SIP.username"
#define SIP_PASSWORD       "SIP.password"
#define SIP_HOST_PART      "SIP.hostPart"
#define SIP_PROXY          "SIP.proxy"
#define SIP_AUTO_REGISTER  "SIP.autoregister"
#define SIP_STUN_SERVER    "STUN.STUNserver"
#define SIP_USE_STUN       "STUN.useStun"


SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
{
}


SIPAccount::~SIPAccount()
{
}

/* virtual Account function implementation */
bool
SIPAccount::createVoIPLink()
{
  return false;
}

bool
SIPAccount::registerAccount()
{
  return false;
}

bool
SIPAccount::unregisterAccount()
{
  return false;
}

bool
SIPAccount::init()
{
  return false;
}

bool
SIPAccount::terminate()
{
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
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_FULL_NAME, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_USER_PART, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_AUTH_USER_NAME, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_PROXY, "", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_AUTO_REGISTER, "1", type_int));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_STUN_SERVER, "stun.fwdnet.net:3478", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(SIP_USE_STUN, "0", type_int));
}

