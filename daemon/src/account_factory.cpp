/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

#include "account_factory.h"

#include "sip/sipvoiplink.h" // for SIPVoIPLink::loadIP2IPSettings
#include "sip/sipaccount.h"
#if HAVE_IAX
#include "iax/iaxaccount.h"
#endif

#include <functional>
#include <utility>

const std::string AccountFactory::DEFAULT_TYPE = "SIP";

std::shared_ptr<Account>
AccountMapBase::createAccount(const std::string& accountID) {
    auto account = create_(accountID);
    accountMap[accountID] = account;
    return account;
}

bool
AccountMapBase::hasAccount(const std::string& accountID) const {
    return accountMap.find(accountID) != accountMap.end();
}

std::shared_ptr<Account>
AccountMapBase::getAccount(const std::string& accountID) const {
    const auto& iter = accountMap.find(accountID);
    if (iter != accountMap.cend())
        return iter->second;
    return nullptr;
}

AccountFactory::AccountFactory()
{
    auto base = new AccountMapBase(
        [](const std::string& accountID) {
            auto account = std::make_shared<SIPAccount>(accountID, true);
            return std::dynamic_pointer_cast<Account>(account);
        });

    typedAccountMap_["SIP"] = std::unique_ptr<AccountMapBase>(base);

#if HAVE_IAX
    base = new AccountMapBase(
        [](const std::string& accountID) {
            auto account = std::make_shared<IAXAccount>(accountID);
            return std::dynamic_pointer_cast<Account>(account);
        });
    typedAccountMap_["IAX"] = std::unique_ptr<AccountMapBase>(base);
#endif

    ip2ip_account_ = createAccount("SIP", SIPAccount::IP2IP_PROFILE);
}

bool
AccountFactory::isSupportedType(const std::string& accountType) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    return typedAccountMap_.find(accountType) != typedAccountMap_.end();
}

bool
AccountFactory::hasAccount(const std::string& accountId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& item : typedAccountMap_) {
        if (item.second->hasAccount(accountId))
            return true;
    }

    return false;
}

void
AccountFactory::removeAccount(const std::string& accountId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (auto& item : typedAccountMap_)
        item.second->accountMap.erase(accountId);
}

std::shared_ptr<Account>
AccountFactory::createAccount(const std::string& accountType,
                              const std::string& accountId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!isSupportedType(accountType)) {
        ERROR("Not supported type %s", accountType.c_str());
        return nullptr;
    }

    if (hasAccount(accountId)) {
        ERROR("Existing account %s", accountId.c_str());
        return nullptr;
    }

    return typedAccountMap_.at(accountType)->createAccount(accountId);
}

AccountMap
AccountFactory::getAllAccounts() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AccountMap all_accounts;

    for (const auto& item : typedAccountMap_)
        all_accounts.insert(item.second->accountMap.cbegin(), item.second->accountMap.cend());

    return all_accounts;
}

AccountMap
AccountFactory::getAllAccounts(const std::string& accountType) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!isSupportedType(accountType)) {
        ERROR("Not supported type %s", accountType.c_str());
        return AccountMap();
    }

    return typedAccountMap_.at(accountType)->accountMap;
}

std::shared_ptr<Account>
AccountFactory::getAccount(const std::string& accountId) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& item : typedAccountMap_) {
        auto account = item.second->getAccount(accountId);
        if (account)
            return account;
    }

    return nullptr;
}

std::shared_ptr<Account>
AccountFactory::getAccount(const std::string& accountType,
                           const std::string& accountId) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!isSupportedType(accountType)) {
        ERROR("Not supported type %s", accountType.c_str());
        return nullptr;
    }

    return typedAccountMap_.at(accountType)->getAccount(accountId);
}

void AccountFactory::initIP2IPAccount()
{
    SIPVoIPLink::loadIP2IPSettings();
}
