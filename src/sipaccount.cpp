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
 : Account(accountID, "sip")
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
    /* Stuff needed for SIP registration */
    setHostname(Manager::instance().getConfigString(_accountID,HOSTNAME));
    setUsername(Manager::instance().getConfigString(_accountID, USERNAME));
    setPassword(Manager::instance().getConfigString(_accountID, PASSWORD));
    /* Retrieve STUN stuff */
    /* STUN configuration is attached to a voiplink because it is applied to every accounts (PJSIP limitation)*/
    thislink = dynamic_cast<SIPVoIPLink*> (_link);
    if (thislink) {
        useStun = Manager::instance().getConfigInt(_accountID,SIP_USE_STUN);
        thislink->setStunServer(Manager::instance().getConfigString(_accountID,SIP_STUN_SERVER));
        thislink->setUseStun( useStun!=0 ? true : false);
    }
    /* Link initialization */
    _link->init();

    /* Start registration */
    status = _link->sendRegister( _accountID );
    ASSERT( status , SUCCESS );

    return SUCCESS;
}

int SIPAccount::unregisterVoIPLink()
{
  _debug("SIPAccount: unregister account %s\n" , getAccountID().c_str());
  return _link->sendUnregister();
}

void SIPAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();
}

bool SIPAccount::fullMatch(const std::string& username, const std::string& hostname)
{
  return (username == getUsername() && hostname == getHostname());
}

bool SIPAccount::userMatch(const std::string& username)
{
  return (username == getUsername());
}

