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
#include "aixaccount.h"

AIXAccount::AIXAccount(const AccountID& accountID)
 : Account(accountID)
{
}


AIXAccount::~AIXAccount()
{
}

/* virtual Account function implementation */
bool
AIXAccount::createVoIPLink()
{
  return false;
}

bool
AIXAccount::registerAccount()
{
  return false;
}

bool
AIXAccount::unregisterAccount()
{
  return false;
}

bool
AIXAccount::init()
{
  return false;
}

bool
AIXAccount::terminate()
{
  return false;
}

void 
AIXAccount::initConfig(Conf::ConfigTree& config)
{
  std::string section(_accountID);
  std::string type_str("string");
  std::string type_int("int");
  
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_TYPE, "AIX", type_str));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem(CONFIG_ACCOUNT_ENABLE, "1", type_int));
  config.addConfigTreeItem(section, Conf::ConfigTreeItem("AIX.Proxy", "", type_str));

}

