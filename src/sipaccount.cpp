/*
 *  Copyright (C) 2006-2009 Savoir-Faire Linux inc.
 *  
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#include "manager.h"
#include "user_cfg.h"

SIPAccount::SIPAccount(const AccountID& accountID)
 : Account(accountID)
 , _userName("")
 , _server("")
 , _cred(NULL)
 , _contact("")
{
  _link = new SIPVoIPLink(accountID);
   //_link = SIPVoIPLink::instance( accountID ); 
}


SIPAccount::~SIPAccount()
{
  delete _link;
  _link = NULL;
  delete _cred;
  _cred = NULL;
}

int SIPAccount::registerVoIPLink()
{

    int status, useStun;
    SIPVoIPLink *thislink;

    /* Retrieve the account information */
    _link->setHostname(Manager::instance().getConfigString(_accountID,HOSTNAME));
    useStun = Manager::instance().getConfigInt(_accountID,SIP_USE_STUN);
    thislink = dynamic_cast<SIPVoIPLink*> (_link);
    thislink->setStunServer(Manager::instance().getConfigString(_accountID,SIP_STUN_SERVER));
    thislink->setUseStun( useStun!=0 ? true : false);
    
    _link->init();
  
    // Stuff needed for SIP registration.
    thislink->setUsername(Manager::instance().getConfigString(_accountID, USERNAME));
    thislink->setPassword(Manager::instance().getConfigString(_accountID, PASSWORD));
    thislink->setHostname(Manager::instance().getConfigString(_accountID, HOSTNAME));

    // Start registration
    status = _link->sendRegister();
    ASSERT( status , SUCCESS );

    return SUCCESS;
}

int
SIPAccount::unregisterVoIPLink()
{
  _debug("SIPAccount: unregister account %s\n" , getAccountID().c_str());
  _link->sendUnregister();
  
  return SUCCESS;
}

void
SIPAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();
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

