/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "account_schema.h"
#include "iaxvoiplink.h"
#include "iaxcall.h"
#include "logger.h"
#include "manager.h"
#include "call_factory.h"

#include "config/yamlparser.h"
#include <yaml-cpp/yaml.h>

constexpr const char * const IAXAccount::ACCOUNT_TYPE;

IAXAccount::IAXAccount(const std::string& accountID)
    : Account(accountID), link_(new IAXVoIPLink(*this))
{}

void IAXAccount::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    out << YAML::BeginMap;
    Account::serialize(out);
    out << YAML::Key << PASSWORD_KEY << YAML::Value << password_;
    out << YAML::EndMap;
}

void IAXAccount::unserialize(const YAML::Node &node)
{
    using namespace yaml_utils;
    Account::unserialize(node);
    parseValue(node, PASSWORD_KEY, password_);
}

void IAXAccount::setAccountDetails(const std::map<std::string, std::string> &details)
{
    // Account setting common to SIP and IAX
    Account::setAccountDetails(details);
    parseString(details, CONFIG_ACCOUNT_PASSWORD, password_);
}

std::map<std::string, std::string> IAXAccount::getAccountDetails() const
{
    std::map<std::string, std::string> a = Account::getAccountDetails();
    a[CONFIG_ACCOUNT_PASSWORD] = password_;
    return a;
}

void IAXAccount::doRegister()
{
    try {
        link_->init();
        sendRegister();
    } catch (const VoipLinkException &e) {
        ERROR("IAXAccount: %s", e.what());
    }
}

void
IAXAccount::doUnregister(std::function<void(bool)> cb)
{
    try {
        sendUnregister();
        link_->terminate();
    } catch (const VoipLinkException &e) {
        ERROR("IAXAccount: %s", e.what());
    }
    if (cb)
        cb(true);
}

void
IAXAccount::loadConfig()
{
    // If IAX is not supported, do not register this account
#if !HAVE_IAX
    enabled_ = false;
#endif
}

template <>
std::shared_ptr<IAXCall>
IAXAccount::newIncomingCall(const std::string& id)
{
    return Manager::instance().callFactory.newCall<IAXCall, IAXAccount>(*this, id, Call::INCOMING);
}

template <>
std::shared_ptr<IAXCall>
IAXAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    auto call = Manager::instance().callFactory.newCall<IAXCall, IAXAccount>(*this, id, Call::OUTGOING);

    call->setPeerNumber(toUrl);
    call->initRecFilename(toUrl);

    iaxOutgoingInvite(call.get());

    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);

    return call;
}

std::shared_ptr<Call>
IAXAccount::newOutgoingCall(const std::string& id, const std::string& toUrl)
{
    return newOutgoingCall<IAXCall>(id, toUrl);
}

void
IAXAccount::iaxOutgoingInvite(IAXCall* call)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);

    call->session = iax_session_new();

    std::string username(getUsername());
    std::string strNum(username + ":" + getPassword() + "@" + getHostname() + "/" + call->getPeerNumber());

    /** @todo Make preference dynamic, and configurable */
    const auto accountID = getAccountID();
    int audio_format_preferred = call->getFirstMatchingFormat(call->getSupportedFormat(accountID), accountID);
    int audio_format_capability = call->getSupportedFormat(accountID);

    iax_call(call->session, username.c_str(), username.c_str(), strNum.c_str(),
             NULL, 0, audio_format_preferred, audio_format_capability);
}

void
IAXAccount::sendRegister()
{
    if (not isEnabled()) {
        WARN("Account must be enabled to register, ignoring");
        return;
    }

    if (getHostname().empty())
        throw VoipLinkException("Account hostname is empty");

    if (getUsername().empty())
        throw VoipLinkException("Account username is empty");

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        regSession_.reset(iax_session_new());
    }

    if (regSession_) {
        {
            std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
            DEBUG("register IAXAccount %s", getHostname().c_str());
            iax_register(regSession_.get(), getHostname().data(),
                         getUsername().data(), getPassword().data(), 120);
        }

        nextRefreshStamp_ = time(NULL) + 10;
        setRegistrationState(RegistrationState::TRYING);
    }
}

void
IAXAccount::sendUnregister(std::function<void(bool)> cb)
{
    DEBUG("unregister IAXAccount %s", getHostname().c_str());
    destroyRegSession();

    nextRefreshStamp_ = 0;
    setRegistrationState(RegistrationState::UNREGISTERED);

    if (cb)
        cb(true);
}

void
IAXAccount::destroyRegSession()
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    regSession_.reset();
}

void
IAXAccount::checkRegister()
{
    if (nextRefreshStamp_ and nextRefreshStamp_ < time(NULL))
        sendRegister();
}

bool
IAXAccount::matchRegSession(const iax_session* session) const
{
    return regSession_.get() == session;
}
