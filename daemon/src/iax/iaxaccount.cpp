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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iaxaccount.h"
#include "iaxvoiplink.h"
#include "logger.h"
#include "manager.h"
#include "config/yamlnode.h"
#include "config/yamlemitter.h"

IAXAccount::IAXAccount(const std::string& accountID)
    : Account(accountID, "iax2"), password_(), link_(accountID)
{}

void IAXAccount::serialize(Conf::YamlEmitter &emitter)
{
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

    accountmap.setKeyValue(ALIAS_KEY, &alias);
    accountmap.setKeyValue(TYPE_KEY, &type);
    accountmap.setKeyValue(ID_KEY, &id);
    accountmap.setKeyValue(USERNAME_KEY, &username);
    accountmap.setKeyValue(PASSWORD_KEY, &password);
    accountmap.setKeyValue(HOSTNAME_KEY, &hostname);
    accountmap.setKeyValue(ACCOUNT_ENABLE_KEY, &enable);
    accountmap.setKeyValue(MAILBOX_KEY, &mailbox);

    accountmap.setKeyValue(DISPLAY_NAME_KEY, &displayName);
    accountmap.setKeyValue(CODECS_KEY, &codecs);

    try {
        emitter.serializeAccount(&accountmap);
    } catch (const Conf::YamlEmitterException &e) {
        ERROR("ConfigTree: %s", e.what());
    }
}

void IAXAccount::unserialize(const Conf::MappingNode &map)
{
    map.getValue(ALIAS_KEY, &alias_);
    map.getValue(TYPE_KEY,  &type_);
    map.getValue(USERNAME_KEY, &username_);
    map.getValue(PASSWORD_KEY, &password_);
    map.getValue(HOSTNAME_KEY, &hostname_);
    map.getValue(ACCOUNT_ENABLE_KEY, &enabled_);
    map.getValue(MAILBOX_KEY, &mailBox_);
    map.getValue(CODECS_KEY, &codecStr_);

    // Update codec list which one is used for SDP offer
    setActiveCodecs(ManagerImpl::split_string(codecStr_));
    map.getValue(DISPLAY_NAME_KEY, &displayName_);
}

void IAXAccount::setAccountDetails(std::map<std::string, std::string> details)
{
    // Account setting common to SIP and IAX
    alias_ = details[CONFIG_ACCOUNT_ALIAS];
    type_ = details[CONFIG_ACCOUNT_TYPE];
    username_ = details[CONFIG_ACCOUNT_USERNAME];
    hostname_ = details[CONFIG_ACCOUNT_HOSTNAME];
    password_ = details[CONFIG_ACCOUNT_PASSWORD];
    enabled_ = details[CONFIG_ACCOUNT_ENABLE] == "true";
    mailBox_ = details[CONFIG_ACCOUNT_MAILBOX];
    displayName_ = details[CONFIG_DISPLAY_NAME];
    userAgent_ = details[CONFIG_ACCOUNT_USERAGENT];
}

std::map<std::string, std::string> IAXAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[CONFIG_ACCOUNT_ID] = accountID_;
    a[CONFIG_ACCOUNT_ALIAS] = alias_;
    a[CONFIG_ACCOUNT_ENABLE] = enabled_ ? "true" : "false";
    a[CONFIG_ACCOUNT_TYPE] = type_;
    a[CONFIG_ACCOUNT_HOSTNAME] = hostname_;
    a[CONFIG_ACCOUNT_USERNAME] = username_;
    a[CONFIG_ACCOUNT_PASSWORD] = password_;
    a[CONFIG_ACCOUNT_MAILBOX] = mailBox_;

    RegistrationState state(registrationState_);

    a[CONFIG_ACCOUNT_REGISTRATION_STATUS] = mapStateNumberToString(state);
    a[CONFIG_ACCOUNT_USERAGENT] = userAgent_;

    return a;
}

void IAXAccount::registerVoIPLink()
{
    try {
        link_.init();
        link_.sendRegister(this);
    } catch (const VoipLinkException &e) {
        ERROR("IAXAccount: %s", e.what());
    }
}

void
IAXAccount::unregisterVoIPLink()
{
    try {
        link_.sendUnregister(this);
        link_.terminate();
    } catch (const VoipLinkException &e) {
        ERROR("IAXAccount: %s", e.what());
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

VoIPLink* IAXAccount::getVoIPLink()
{
    return &link_;
}
