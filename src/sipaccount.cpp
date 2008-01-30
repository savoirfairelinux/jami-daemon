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

SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
{
  _link = new SIPVoIPLink(accountID);
}


SIPAccount::~SIPAccount()
{
  delete _link;
  _link = NULL;
}

void
SIPAccount::registerVoIPLink()
{
  _link->setFullName(Manager::instance().getConfigString(_accountID,SIP_FULL_NAME));
  _link->setHostName(Manager::instance().getConfigString(_accountID,SIP_HOST_PART));
  int useStun = Manager::instance().getConfigInt(_accountID,SIP_USE_STUN);
  
  SIPVoIPLink* thislink = dynamic_cast<SIPVoIPLink*> (_link);
  thislink->setStunServer(Manager::instance().getConfigString(_accountID,DFT_STUN_SERVER));
  thislink->setUseStun( useStun!=0 ? true : false);

  _link->init();

  // Stuff needed for SIP registration.
  thislink->setProxy   (Manager::instance().getConfigString(_accountID,SIP_PROXY));
  thislink->setUserPart(Manager::instance().getConfigString(_accountID,SIP_USER_PART));
  thislink->setAuthName(Manager::instance().getConfigString(_accountID,SIP_AUTH_NAME));
  thislink->setPassword(Manager::instance().getConfigString(_accountID,SIP_PASSWORD));

  _link->sendRegister();
}

void
SIPAccount::unregisterVoIPLink()
{
  _link->sendUnregister();
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
