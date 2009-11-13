/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include "managerimpl.h"

#include "account.h"
#include "dbus/callmanager.h"
#include "user_cfg.h"
#include "global.h"
#include "sip/sipaccount.h"

#include "audio/audiolayer.h"
#include "sip/sipvoiplink.h"
#include "manager.h"
#include "dbus/configurationmanager.h"

#include "conference.h"

#include <errno.h>
#include <cstdlib>

//THREAD=Main
int
ManagerImpl::initRegisterAccounts()
{
    int status;
    bool flag = true;
    AccountMap::iterator iter;

    _debugInit ("Initiate VoIP Links Registration");
    iter = _accountMap.begin();

    /* Loop on the account map previously loaded */

    while (iter != _accountMap.end()) {
        if (iter->second) {
            iter->second->loadConfig();
            /* If the account is set as enabled, try to register */

            if (iter->second->isEnabled()) {
                status = iter->second->registerVoIPLink();

                if (status != SUCCESS) {
                    flag = false;
                }
            }
        }

        iter++;
    }

    // calls the client notification here in case of errors at startup...
    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    ASSERT (flag, true);

    return SUCCESS;
}

void ManagerImpl::restartPJSIP (void)
{
    SIPVoIPLink *siplink;
    siplink = dynamic_cast<SIPVoIPLink*> (getSIPAccountLink ());

    this->unregisterCurSIPAccounts();
    /* Terminate and initialize the PJSIP library */

    if (siplink) {
        siplink->terminate ();
        siplink = SIPVoIPLink::instance ("");
        siplink->init ();
    }

    /* Then register all enabled SIP accounts */
    this->registerCurSIPAccounts (siplink);
}

int
ManagerImpl::registerAccounts()
{
    int status;
    bool flag = true;
    AccountMap::iterator iter;

    _debugInit ("Initiate VoIP Links Registration");
    iter = _accountMap.begin();

    /* Loop on the account map previously loaded */

    while (iter != _accountMap.end()) {
        if (iter->second) {

            if (iter->second->isEnabled()) {

		_debug("Register account %s", iter->first.c_str());
		
                status = iter->second->registerVoIPLink();

                if (status != SUCCESS) {
                    flag = false;
                }
            }
        }

        iter++;
    }

    // calls the client notification here in case of errors at startup...
    if (_audiodriver -> getErrorMessage() != -1)
        notifyErrClient (_audiodriver -> getErrorMessage());

    ASSERT (flag, true);

    return SUCCESS;
}

VoIPLink* ManagerImpl::getAccountLink (const AccountID& accountID)
{
    if (accountID!=AccountNULL) {
        Account* acc = getAccount (accountID);

        if (acc) {
            return acc->getVoIPLink();
        }

        return 0;
    } else
        return SIPVoIPLink::instance ("");
}

VoIPLink* ManagerImpl::getSIPAccountLink()
{
    /* We are looking for the first SIP account we met because all the SIP accounts have the same voiplink */
    Account *account;
    AccountMap::iterator iter;

    for (iter = _accountMap.begin(); iter != _accountMap.end(); ++iter) {
        account = iter->second;

        if (account->getType() == "sip") {
            return account->getVoIPLink();
        }
    }

    return NULL;
}

pjsip_regc
*getSipRegcFromID (const AccountID& id UNUSED)
{
    /*SIPAccount *tmp = dynamic_cast<SIPAccount *>getAccount(id);
    if(tmp != NULL)
      return tmp->getSipRegc();
    else*/
    return NULL;
}

void ManagerImpl::unregisterCurSIPAccounts()
{
    Account *current;

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {
        current = iter->second;

        if (current) {
            if (current->isEnabled() && current->getType() == "sip") {
                current->unregisterVoIPLink();
            }
        }

        iter++;
    }
}

void ManagerImpl::registerCurSIPAccounts (VoIPLink *link)
{

    Account *current;

    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {
        current = iter->second;

        if (current) {
            if (current->isEnabled() && current->getType() == "sip") {
                //current->setVoIPLink(link);
                current->registerVoIPLink();
            }
        }

        current = NULL;

        iter++;
    }
}

void
ManagerImpl::sendRegister (const std::string& accountID , const int32_t& enable)
{

    // Update the active field
    setConfig (accountID, CONFIG_ACCOUNT_ENABLE, (enable == 1) ? TRUE_STR:FALSE_STR);

    Account* acc = getAccount (accountID);
    acc->loadConfig();

    // Test on the freshly updated value

    if (acc->isEnabled()) {
        // Verify we aren't already registered, then register
        _debug ("Send register for account %s" , accountID.c_str());
        acc->registerVoIPLink();
    } else {
        // Verify we are already registered, then unregister
        _debug ("Send unregister for account %s" , accountID.c_str());
        acc->unregisterVoIPLink();
    }

}
