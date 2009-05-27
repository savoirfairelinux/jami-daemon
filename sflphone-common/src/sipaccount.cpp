/*
 *  Copyright (C) 2006-2009 Savoir-Faire Linux inc.
 *  
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 , _bRegister(false)
 , _regc()
{
    /* SIPVoIPlink is used as a singleton, because we want to have only one link for all the SIP accounts created */
    /* So instead of creating a new instance, we just fetch the static instance, or create one if it is not yet */
    /* The SIP library initialization is done in the SIPVoIPLink constructor */
    /* The SIP voip link is now independant of the account ID as it can manage several SIP accounts */
    _link = SIPVoIPLink::instance("");
    
    /* Represents the number of SIP accounts connected the same link */
    dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();
    
}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    dynamic_cast<SIPVoIPLink*> (_link)->decrementClients();
    /* Delete accounts-related information */
    _regc = NULL;
    delete _cred; _cred = NULL;
}

int SIPAccount::registerVoIPLink()
{
    int status;

    /* Retrieve the account information */
    /* Stuff needed for SIP registration */
    setHostname(Manager::instance().getConfigString(_accountID, HOSTNAME));
    setUsername(Manager::instance().getConfigString(_accountID, USERNAME));
    setPassword(Manager::instance().getConfigString(_accountID, PASSWORD));

    /* Start registration */
    status = _link->sendRegister( _accountID );
    ASSERT( status , SUCCESS );

    return SUCCESS;
}

int SIPAccount::unregisterVoIPLink()
{
    _debug("unregister account %s\n" , getAccountID().c_str());
  
    if(_link->sendUnregister( _accountID )){
        setRegistrationInfo (NULL);
        return true;
    }
    else
        return false;
  
}

void SIPAccount::loadConfig() 
{
  // Account generic
  Account::loadConfig();
}

bool SIPAccount::fullMatch(const std::string& username, const std::string& hostname)
{
  return (userMatch (username) && hostnameMatch (hostname));
}

bool SIPAccount::userMatch(const std::string& username)
{
  _debug("username = %s , getUserName() = %s, == : %i\n", username.c_str(), getUsername().c_str() , username == getUsername());
  return (username == getUsername());
}

bool SIPAccount::hostnameMatch(const std::string& hostname)
{
  _debug("hostname = %s , getHostname() = %s, == : %i\n", hostname.c_str(), getHostname().c_str() , hostname == getHostname());
  return (hostname == getHostname());
}

