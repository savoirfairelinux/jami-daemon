/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "config.h"

#include "iaxaccount.h"
#include "iaxvoiplink.h"
#include "manager.h"

IAXAccount::IAXAccount(const std::string& accountID)
    : Account(accountID, "iax2")
{
    link_ = new IAXVoIPLink(accountID);
}


IAXAccount::~IAXAccount()
{
    delete link_;
}

void IAXAccount::serialize(Conf::YamlEmitter *emitter)
{
    if (emitter == NULL) {
        _error("IAXAccount: Error: emitter is NULL in serialize");
        return;
    }

    Conf::MappingNode accountmap(NULL);

    Conf::ScalarNode id(accountID_);
    Conf::ScalarNode username(username_);
    Conf::ScalarNode password(password_);
    Conf::ScalarNode alias(alias_);
    Conf::ScalarNode hostname(hostname_);
    Conf::ScalarNode enable(enabled_);
    Conf::ScalarNode type(type_);
    Conf::ScalarNode mailbox(mailBox_);

    Conf::ScalarNode codecs(codecStr_);
    Conf::ScalarNode displayName(displayName_);

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

    try {
        emitter->serializeAccount(&accountmap);
    } catch (const Conf::YamlEmitterException &e) {
        _error("ConfigTree: %s", e.what());
    }
}

void IAXAccount::unserialize(Conf::MappingNode *map)
{
    if (map == NULL) {
        _error("IAXAccount: Error: Map is NULL in unserialize");
        return;
    }

    map->getValue(aliasKey, &alias_);
    map->getValue(typeKey,  &type_);
    map->getValue(usernameKey, &username_);
    map->getValue(passwordKey, &password_);
    map->getValue(hostnameKey, &hostname_);
    map->getValue(accountEnableKey, &enabled_);
    map->getValue(mailboxKey, &mailBox_);
    map->getValue(codecsKey, &codecStr_);

    // Update codec list which one is used for SDP offer
    setActiveCodecs(ManagerImpl::unserialize(codecStr_));
    map->getValue(displayNameKey, &displayName_);
}

void IAXAccount::setAccountDetails(std::map<std::string, std::string> details)
{
    // Account setting common to SIP and IAX
    alias_ = details[CONFIG_ACCOUNT_ALIAS];
    type_ = details[CONFIG_ACCOUNT_TYPE];
    username_ = details[USERNAME];
    hostname_ = details[HOSTNAME];
    password_ = details[PASSWORD];
    enabled_ = details[CONFIG_ACCOUNT_ENABLE] == "true";
    mailBox_ = details[CONFIG_ACCOUNT_MAILBOX];
    displayName_ = details[DISPLAY_NAME];
    userAgent_ = details[USERAGENT];
}

std::map<std::string, std::string> IAXAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[ACCOUNT_ID] = accountID_;
    a[CONFIG_ACCOUNT_ALIAS] = alias_;
    a[CONFIG_ACCOUNT_ENABLE] = enabled_ ? "true" : "false";
    a[CONFIG_ACCOUNT_TYPE] = type_;
    a[HOSTNAME] = hostname_;
    a[USERNAME] = username_;
    a[PASSWORD] = password_;
    a[CONFIG_ACCOUNT_MAILBOX] = mailBox_;

    RegistrationState state(registrationState_);

    a[REGISTRATION_STATUS] = mapStateNumberToString(state);
    a[USERAGENT] = userAgent_;

    return a;
}

void IAXAccount::registerVoIPLink()
{
    try {
        link_->init();
        link_->sendRegister(this);
    } catch (const VoipLinkException &e) {
        _error("IAXAccount: %s", e.what());
    }
}

void
IAXAccount::unregisterVoIPLink()
{
    try {
        link_->sendUnregister(this);
        dynamic_cast<IAXVoIPLink*>(link_)->terminate();
    } catch (const VoipLinkException &e) {
        _error("IAXAccount: %s", e.what());
    }
}

void
IAXAccount::loadConfig()
{
    // If IAX is not supported, do not register this account
#if !HAVE_IAX
    enabled_ = false;
#endif
}
