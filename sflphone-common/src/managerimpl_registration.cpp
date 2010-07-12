/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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

int
ManagerImpl::registerAccounts()
{
    int status;
    bool flag = true;
    AccountMap::iterator iter;

    _debugInit ("Manager: Initiate VoIP Links Registration");
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

//THREAD=Main
int
ManagerImpl::initRegisterAccounts()
{
    int status;
    bool flag = true;
    AccountMap::iterator iter;

    _debugInit ("Manager: Initiate VoIP Links Registration");
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
    _debug ("ManagerImpl::restartPJSIP\n");
    VoIPLink *link = getSIPAccountLink();
    SIPVoIPLink *siplink = NULL;

    if (link) {
        siplink = dynamic_cast<SIPVoIPLink*> (getSIPAccountLink ());
    }

    _debug ("ManagerImpl::unregister sip account\n");

    this->unregisterCurSIPAccounts();
    /* Terminate and initialize the PJSIP library */

    if (siplink) {
        _debug ("ManagerImpl::Terminate sip\n");
        siplink->terminate ();
        siplink = SIPVoIPLink::instance ("");
        _debug ("ManagerImpl::Init new sip\n");
        siplink->init ();
    }

    _debug ("ManagerImpl::register sip account\n");

    /* Then register all enabled SIP accounts */
    this->registerCurSIPAccounts ();
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
    AccountMap::iterator iter = _accountMap.begin();

    while (iter != _accountMap.end()) {

        account = iter->second;

        if (account->getType() == "sip") {
            return account->getVoIPLink();
        }

        ++iter;
    }

    return NULL;
}

pjsip_regc *getSipRegcFromID (const AccountID& id UNUSED)
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

void ManagerImpl::registerCurSIPAccounts (void)
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
    Account* acc = getAccount (accountID);

    if(enable == 1)
      acc->setEnabled(true);
    else
      acc->setEnabled(false);

    acc->loadConfig();

    // Test on the freshly updated value
    if (acc->isEnabled()) {
        // Verify we aren't already registered, then register
        _debug ("Send register for account %s\n" , accountID.c_str());
        acc->registerVoIPLink();
    } else {
        // Verify we are already registered, then unregister
        _debug ("Send unregister for account %s\n" , accountID.c_str());
        acc->unregisterVoIPLink();
    }

}

