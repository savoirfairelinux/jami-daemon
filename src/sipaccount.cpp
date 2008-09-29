
#include "voiplink.h"

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
#include "sipaccount.h"
#include "sipvoiplink.h"
#include "manager.h"
#include "user_cfg.h"
#include "useragent.h"

SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
{
  _cred = NULL;
  _link = new SIPVoIPLink(accountID);
}


SIPAccount::~SIPAccount()
{
  delete _link;
  _link = NULL;
  delete _cred;
  _cred = NULL;
}

void
SIPAccount::registerVoIPLink()
{
  _link->setHostName(Manager::instance().getConfigString(_accountID,SIP_HOST));
  int useStun = Manager::instance().getConfigInt(_accountID,SIP_USE_STUN);
  
  SIPVoIPLink* thislink = dynamic_cast<SIPVoIPLink*> (_link);
  thislink->setStunServer(Manager::instance().getConfigString(_accountID,SIP_STUN_SERVER));
  thislink->setUseStun( useStun!=0 ? true : false);
    
  //SIPVoIPLink* thislink = dynamic_cast<SIPVoIPLink*> (_link);
  _link->init();
  
  // Stuff needed for SIP registration.
  thislink->setProxy   (Manager::instance().getConfigString(_accountID,SIP_PROXY));
  thislink->setAuthName(Manager::instance().getConfigString(_accountID,SIP_USER));
  thislink->setPassword(Manager::instance().getConfigString(_accountID,SIP_PASSWORD));
  thislink->setSipServer(Manager::instance().getConfigString(_accountID,SIP_HOST));
  _link->sendRegister();
}

void
SIPAccount::unregisterVoIPLink()
{
  _debug("SIPAccount: unregister account %s\n" , getAccountID().c_str());
  _link->sendUnregister();
  _debug("Terminate SIP account\n");
  _link->terminate();
}

void
SIPAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();

  // SIP specific
  //none
}

bool 
SIPAccount::fullMatch(const std::string& userName, const std::string& server)
{
  return (userName == _userName && server == _server);
}

bool 
SIPAccount::userMatch(const std::string& userName)
{
  return (userName == _userName);
}

