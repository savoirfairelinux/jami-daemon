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

SIPAccount::SIPAccount (const AccountID& accountID)
        : Account (accountID, "sip")
        , _cred (NULL)
        , _regc()
        , _bRegister (false)
        , _contact ("")
        , _resolveOnce (false)
{
    /* SIPVoIPlink is used as a singleton, because we want to have only one link for all the SIP accounts created */
    /* So instead of creating a new instance, we just fetch the static instance, or create one if it is not yet */
    /* The SIP library initialization is done in the SIPVoIPLink constructor */
    /* The SIP voip link is now independant of the account ID as it can manage several SIP accounts */
    _link = SIPVoIPLink::instance ("");

    /* Represents the number of SIP accounts connected the same link */
    dynamic_cast<SIPVoIPLink*> (_link)->incrementClients();

}

SIPAccount::~SIPAccount()
{
    /* One SIP account less connected to the sip voiplink */
    dynamic_cast<SIPVoIPLink*> (_link)->decrementClients();
    /* Delete accounts-related information */
    _regc = NULL;
    free(_cred);
    _cred = NULL;
}

int SIPAccount::registerVoIPLink()
{
    int status;

    /* Retrieve the account information */
    /* Stuff needed for SIP registration */

    if (Manager::instance().getConfigString (_accountID, HOSTNAME).length() >= PJ_MAX_HOSTNAME) {
        return !SUCCESS;
    }
    
    setHostname (Manager::instance().getConfigString (_accountID, HOSTNAME));
    setUsername (Manager::instance().getConfigString (_accountID, USERNAME));
    setPassword (Manager::instance().getConfigString (_accountID, PASSWORD));
    _authenticationUsername = Manager::instance().getConfigString (_accountID, AUTHENTICATION_USERNAME);
    _realm = Manager::instance().getConfigString (_accountID, REALM);
    
    int credentialCount = 0;
    credentialCount = Manager::instance().getConfigInt (_accountID, CONFIG_CREDENTIAL_NUMBER);
    credentialCount += 1;

    bool md5HashingEnabled = false;
    int dataType = 0;
    md5HashingEnabled = Manager::instance().getConfigBool(PREFERENCES, CONFIG_MD5HASH);
    std::string digest;
    if (md5HashingEnabled) {
        dataType = PJSIP_CRED_DATA_DIGEST;
    } else {
        dataType = PJSIP_CRED_DATA_PLAIN_PASSWD;
    }
        
    pjsip_cred_info * cred_info = (pjsip_cred_info *) malloc(sizeof(pjsip_cred_info)*(credentialCount));        
    if (cred_info == NULL) {
        _debug("Failed to set cred_info for account %s\n", _accountID.c_str());
        return !SUCCESS;
    }
    pj_bzero (cred_info, sizeof(pjsip_cred_info)*credentialCount);
    
    if (!_authenticationUsername.empty()) {
        cred_info[0].username = pj_str(strdup(_authenticationUsername.c_str())); 
    } else {
        cred_info[0].username = pj_str(strdup(_username.c_str()));
    }
    cred_info[0].data =  pj_str(strdup(_password.c_str()));
    cred_info[0].realm = pj_str(strdup(_realm.c_str()));
    cred_info[0].data_type = dataType;
    cred_info[0].scheme = pj_str("digest");
            
    int i;
    for (i = 1; i < credentialCount; i++) {
        std::string credentialIndex;
        std::stringstream streamOut;
        streamOut << i - 1;
        credentialIndex = streamOut.str();

        std::string section = std::string("Credential") + std::string(":") + _accountID + std::string(":") + credentialIndex;

        std::string username = Manager::instance().getConfigString(section, USERNAME);
        std::string password = Manager::instance().getConfigString(section, PASSWORD);
        std::string realm = Manager::instance().getConfigString(section, REALM);
        
        cred_info[i].username = pj_str(strdup(username.c_str()));
        cred_info[i].data = pj_str(strdup(password.c_str()));
        cred_info[i].realm = pj_str(strdup(realm.c_str()));
        cred_info[i].data_type = dataType;
        cred_info[i].scheme = pj_str("digest");
        
        _debug("Setting credential %d realm = %s passwd = %s username = %s data_type = %d\n", i, realm.c_str(), password.c_str(), username.c_str(), cred_info[i].data_type);
    }

    _credentialCount = credentialCount;
    _cred = cred_info;
    
    _resolveOnce = Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_RESOLVE_ONCE) == "1" ? true : false;

    if (Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE).empty()) {
        _registrationExpire = DFT_EXPIRE_VALUE;
    } else {
        _registrationExpire = Manager::instance().getConfigString (_accountID, CONFIG_ACCOUNT_REGISTRATION_EXPIRE);
    }
    
    /* Start registration */
    status = _link->sendRegister (_accountID);

    ASSERT (status , SUCCESS);

    return SUCCESS;
}

int SIPAccount::unregisterVoIPLink()
{
    _debug ("unregister account %s\n" , getAccountID().c_str());

    if (_link->sendUnregister (_accountID)) {
        setRegistrationInfo (NULL);
        return true;
    } else
        return false;

}

void SIPAccount::loadConfig()
{
    // Account generic
    Account::loadConfig();
}

bool SIPAccount::fullMatch (const std::string& username, const std::string& hostname)
{
    return (userMatch (username) && hostnameMatch (hostname));
}

bool SIPAccount::userMatch (const std::string& username)
{
    if(username.empty()) {
        return false;
    }
    return (username == getUsername());
}

bool SIPAccount::hostnameMatch (const std::string& hostname)
{
    return (hostname == getHostname());
}

