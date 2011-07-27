/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "iaxaccount.h"
#include "iaxvoiplink.h"
#include "manager.h"

IAXAccount::IAXAccount (const AccountID& accountID)
    : Account (accountID, "iax2")
{
    _link = new IAXVoIPLink (accountID);
}


IAXAccount::~IAXAccount()
{
    delete _link;
    _link = NULL;
}

void IAXAccount::serialize (Conf::YamlEmitter *emitter)
{
	if(emitter == NULL) {
		_error("IAXAccount: Error: emitter is NULL in serialize");
		return;
	}

    Conf::MappingNode accountmap (NULL);

    Conf::ScalarNode id (Account::_accountID);
    Conf::ScalarNode username (Account::_username);
    Conf::ScalarNode password (Account::_password);
    Conf::ScalarNode alias (Account::_alias);
    Conf::ScalarNode hostname (Account::_hostname);
    Conf::ScalarNode enable (_enabled);
    Conf::ScalarNode type (Account::_type);
    Conf::ScalarNode mailbox (_mailBox);

    Conf::ScalarNode codecs (_codecStr);
    Conf::ScalarNode displayName (_displayName);

    accountmap.setKeyValue (aliasKey, &alias);
    accountmap.setKeyValue (typeKey, &type);
    accountmap.setKeyValue (idKey, &id);
    accountmap.setKeyValue (usernameKey, &username);
    accountmap.setKeyValue (passwordKey, &password);
    accountmap.setKeyValue (hostnameKey, &hostname);
    accountmap.setKeyValue (accountEnableKey, &enable);
    accountmap.setKeyValue (mailboxKey, &mailbox);

    accountmap.setKeyValue (displayNameKey, &displayName);
    accountmap.setKeyValue (codecsKey, &codecs);

    try {
        emitter->serializeAccount (&accountmap);
    } catch (Conf::YamlEmitterException &e) {
        _error ("ConfigTree: %s", e.what());
    }
}

void IAXAccount::unserialize (Conf::MappingNode *map)
{
    if(map == NULL) {
    	_error("IAXAccount: Error: Map is NULL in unserialize");
    	return;
    }

    map->getValue(aliasKey, &_alias);
    map->getValue(typeKey,  &_type);
    map->getValue(idKey,    &_accountID);
    map->getValue(usernameKey, &_username);
    map->getValue(passwordKey, &_password);
    map->getValue(hostnameKey, &_hostname);
    map->getValue(accountEnableKey, &_enabled);
    map->getValue(mailboxKey, &_mailBox);
    map->getValue (codecsKey, &_codecStr);

    // Update codec list which one is used for SDP offer
    setActiveCodecs (Manager::instance ().unserialize (_codecStr));
    map->getValue (displayNameKey, &_displayName);
}

void IAXAccount::setAccountDetails (std::map<std::string, std::string> details)
{
    // Account setting common to SIP and IAX
    setAlias (details[CONFIG_ACCOUNT_ALIAS]);
    setType (details[CONFIG_ACCOUNT_TYPE]);
    setUsername (details[USERNAME]);
    setHostname (details[HOSTNAME]);
    setPassword (details[PASSWORD]);
    setEnabled ( (details[CONFIG_ACCOUNT_ENABLE].compare ("true") == 0));
    setMailBox (details[CONFIG_ACCOUNT_MAILBOX]);

    setDisplayName (details[DISPLAY_NAME]);
    setUseragent (details[USERAGENT]);
}

std::map<std::string, std::string> IAXAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a.insert (std::pair<std::string, std::string> (ACCOUNT_ID, _accountID));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ALIAS, getAlias()));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_ENABLE, isEnabled() ? "true" : "false"));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_TYPE, getType()));
    a.insert (std::pair<std::string, std::string> (HOSTNAME, getHostname()));
    a.insert (std::pair<std::string, std::string> (USERNAME, getUsername()));
    a.insert (std::pair<std::string, std::string> (PASSWORD, getPassword()));
    a.insert (std::pair<std::string, std::string> (CONFIG_ACCOUNT_MAILBOX, getMailBox()));

    RegistrationState state = Unregistered;
    std::string registrationStateCode;
    std::string registrationStateDescription;

    state = getRegistrationState();
    int code = getRegistrationStateDetailed().first;
    std::stringstream out;
    out << code;
    registrationStateCode = out.str();
    registrationStateDescription = getRegistrationStateDetailed().second;

    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATUS, Manager::instance().mapStateNumberToString (state)));
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_CODE, registrationStateCode));
    a.insert (std::pair<std::string, std::string> (REGISTRATION_STATE_DESCRIPTION, registrationStateDescription));
    a.insert (std::pair<std::string, std::string> (USERAGENT, getUseragent()));

    return a;
}


void IAXAccount::setVoIPLink()
{

}

int IAXAccount::registerVoIPLink()
{
	try {

        _link->init();

        // Stuff needed for IAX registration
        setHostname (_hostname);
        setUsername (_username);
        setPassword (_password);

        _link->sendRegister (_accountID);
	}
	catch(VoipLinkException &e) {
		_error("IAXAccount: %s", e.what());
	}

    return 0;
}

int
IAXAccount::unregisterVoIPLink()
{
	try {
        _link->sendUnregister (_accountID);
        _link->terminate();

        return 0;
	}
	catch(VoipLinkException &e) {
		_error("IAXAccount: %s", e.what());
	}

	return 0;
}

void
IAXAccount::loadConfig()
{
    // Account generic
    Account::loadConfig();
}
