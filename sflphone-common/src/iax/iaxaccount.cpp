/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

void IAXAccount::serialize(Conf::YamlEmitter *emitter) 
{
  _debug("IaxAccount: serialize %s", _accountID.c_str());

  Conf::MappingNode accountmap(NULL);

  Conf::ScalarNode id(Account::_accountID);
  Conf::ScalarNode username(Account::_username);
  Conf::ScalarNode password(Account::_password);
  Conf::ScalarNode alias(Account::_alias);
  Conf::ScalarNode hostname(Account::_hostname);
  Conf::ScalarNode enable(_enabled ? "true" : "false");
  Conf::ScalarNode type(Account::_type);
  Conf::ScalarNode mailbox("97");

  Conf::ScalarNode codecs(_codecStr);
  Conf::ScalarNode displayName(_displayName);

  accountmap.setKeyValue(aliasKey, &alias);
  accountmap.setKeyValue(typeKey, &type);
  accountmap.setKeyValue(idKey, &id);
  accountmap.setKeyValue(usernameKey, &username);
  accountmap.setKeyValue(passwordKey, &password);
  accountmap.setKeyValue(hostnameKey, &hostname);
  accountmap.setKeyValue(accountEnableKey, &enable);
  accountmap.setKeyValue(mailboxKey, &mailbox);

  accountmap.setKeyValue(displayNameKey, &displayName);
  accountmap.setKeyValue(codecsKey, &codecs);

  try{
    emitter->serializeAccount(&accountmap);
  }
  catch (Conf::YamlEmitterException &e) {
    _error("ConfigTree: %s", e.what());
  }
}

void IAXAccount::unserialize(Conf::MappingNode *map)
{
  Conf::ScalarNode *val;

  _debug("IaxAccount: Unserialize");

  val = (Conf::ScalarNode *)(map->getValue(aliasKey));
  if(val) { _alias = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(typeKey));
  if(val) { _type = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(idKey));
  if(val) { _accountID = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(usernameKey));
  if(val) { _username = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(passwordKey));
  if(val) { _password = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(hostnameKey));
  if(val) { _hostname = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(accountEnableKey));
  if(val) { _enabled = (val->getValue().compare("true") == 0) ? true : false; val = NULL; }
  //  val = (Conf::ScalarNode *)(map->getValue(mailboxKey));

  val = (Conf::ScalarNode *)(map->getValue(codecsKey));
  if(val) { _codecStr = val->getValue(); val = NULL; }
  val = (Conf::ScalarNode *)(map->getValue(displayNameKey));
  if(val) { _displayName = val->getValue(); val = NULL; }

}

void IAXAccount::setAccountDetails(const std::map<std::string, std::string>& details)
{
  std::map<std::string, std::string> map_cpy;
  std::map<std::string, std::string>::iterator iter;

  _debug("IaxAccount: Set account details: %s", _accountID.c_str());

  // Work on a copy
  map_cpy = details;

  std::string alias;
  std::string type;
  std::string hostname;
  std::string username;
  std::string password;
  std::string mailbox;
  std::string accountEnable;

  std::string ua_name;

  // Account setting common to SIP and IAX
  find_in_map(CONFIG_ACCOUNT_ALIAS, alias)
  find_in_map(CONFIG_ACCOUNT_TYPE, type)
  find_in_map(HOSTNAME, hostname)
  find_in_map(USERNAME, username)
  find_in_map(PASSWORD, password)
  find_in_map(CONFIG_ACCOUNT_MAILBOX, mailbox);
  find_in_map(CONFIG_ACCOUNT_ENABLE, accountEnable);

  setAlias(alias);
  setType(type);
  setUsername(username);
  setHostname(hostname);
  setPassword(password);
  setEnabled((accountEnable.compare("true") == 0) ? true : false);

  std::string displayName;
  find_in_map(DISPLAY_NAME, displayName)
  setDisplayName(displayName);
  
  find_in_map(USERAGENT, ua_name)
  setUseragent(ua_name);

}

std::map<std::string, std::string> IAXAccount::getAccountDetails()
{
  std::map<std::string, std::string> a;

  _debug("IaxAccount: get account details  %s", _accountID.c_str());

  a.insert(std::pair<std::string, std::string>(ACCOUNT_ID, _accountID));
  a.insert(std::pair<std::string, std::string>(CONFIG_ACCOUNT_ALIAS, getAlias()));
  a.insert(std::pair<std::string, std::string>(CONFIG_ACCOUNT_ENABLE, isEnabled() ? "true" : "false"));
  a.insert(std::pair<std::string, std::string>(CONFIG_ACCOUNT_TYPE, getType()));
  a.insert(std::pair<std::string, std::string>(HOSTNAME, getHostname()));
  a.insert(std::pair<std::string, std::string>(USERNAME, getUsername()));
  a.insert(std::pair<std::string, std::string>(PASSWORD, getPassword()));

  RegistrationState state = Unregistered;
  std::string registrationStateCode;
  std::string registrationStateDescription;

  state = getRegistrationState();
  int code = getRegistrationStateDetailed().first;
  std::stringstream out; out << code;
  registrationStateCode = out.str();
  registrationStateDescription = getRegistrationStateDetailed().second;

  a.insert(std::pair<std::string, std::string>(REGISTRATION_STATUS, Manager::instance().mapStateNumberToString(state)));
  a.insert(std::pair<std::string, std::string>(REGISTRATION_STATE_CODE, registrationStateCode));
  a.insert(std::pair<std::string, std::string>(REGISTRATION_STATE_DESCRIPTION, registrationStateDescription));
  a.insert(std::pair<std::string, std::string>(USERAGENT, getUseragent()));  

  return a;
}


void IAXAccount::setVoIPLink()
{

}

int IAXAccount::registerVoIPLink()
{
    _link->init();

    // Stuff needed for IAX registration
    setHostname (Manager::instance().getConfigString (_accountID, HOSTNAME));
    setUsername (Manager::instance().getConfigString (_accountID, USERNAME));
    setPassword (Manager::instance().getConfigString (_accountID, PASSWORD));

    _link->sendRegister (_accountID);

    return SUCCESS;
}

int
IAXAccount::unregisterVoIPLink()
{
    _link->sendUnregister (_accountID);
    _link->terminate();

    return SUCCESS;
}

void
IAXAccount::loadConfig()
{
    // Account generic
    Account::loadConfig();
}
