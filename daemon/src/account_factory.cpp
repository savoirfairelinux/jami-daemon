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

#include "sip/sipaccount.h"
#if HAVE_IAX
#include "iax/iaxaccount.h"
#endif

#include "sip/sipvoiplink.h" // for SIPVoIPLink::loadIP2IPSettings

#include <stdexcept>

AccountFactory::AccountFactory() : generators_()
{
    auto sip_gen = new AccountGenerator<SIPAccount>(SIP_ACCOUNT_TYPE);
    generators_.push_back(std::unique_ptr<AccountGeneratorBase>(sip_gen));

#if HAVE_IAX
    auto iax_gen = new AccountGenerator<IAXAccount>(IAX_ACCOUNT_TYPE);
    generators_.push_back(std::unique_ptr<AccountGeneratorBase>(iax_gen));
#endif
}

const AccountGeneratorBase&
AccountFactory::getGenerator(const std::string& name) const
{
    for (const auto& gen : generators_)
        if (gen->getTypename() == name)
            return *gen;

    throw std::invalid_argument("Unknown Account generator");
}

AccountGeneratorBase&
AccountFactory::getGenerator(const std::string& name)
{
    for (const auto& gen : generators_)
        if (gen->getTypename() == name)
            return *gen;

    throw std::invalid_argument("Unknown Account generator");
}

bool
AccountFactory::isSupportedType(const std::string& name) const
{
    for (const auto& gen : generators_)
        if (gen->getTypename() == name)
            return true;

    return false;
}

bool
AccountFactory::hasAccount(const std::string& accountId) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& gen : generators_)
        if (gen->hasAccount(accountId))
            return true;

    return false;
}

std::shared_ptr<Account>
AccountFactory::createAccount(const std::string& accountType,
                              const std::string& accountId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Could be in any generator
    if (hasAccount(accountId)) {
        ERROR("Existing account %s", accountId.c_str());
        return nullptr;
    }

    return getGenerator(accountType).createAccount(accountId);
}

void
AccountFactory::removeAccount(const std::string& accountId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (auto& gen : generators_)
        gen->accountMap.erase(accountId);
}

void
AccountFactory::removeAccount(std::shared_ptr<Account> account)
{
    removeAccount(account->getAccountID());
}

AccountMap
AccountFactory::getAllAccounts() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AccountMap all_accounts;

    for (const auto& gen : generators_)
        all_accounts.insert(gen->accountMap.cbegin(),
                            gen->accountMap.cend());

    return all_accounts;
}

AccountMap
AccountFactory::getAllAccounts(const std::string& accountType) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    return getGenerator(accountType).accountMap;
}

std::shared_ptr<Account>
AccountFactory::getAccount(const std::string& accountId) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& gen : generators_) {
        auto account = gen->getAccount(accountId);
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

    return getGenerator(accountType).getAccount(accountId);
}

bool
AccountFactory::empty() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& gen : generators_)
        if (!gen->accountMap.empty())
            return false;

    return true;
}

bool
AccountFactory::empty(const std::string& accountType) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    return getGenerator(accountType).accountMap.empty();
}

int
AccountFactory::accountCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int count = 0;

    for (const auto& gen : generators_)
        count += gen->accountMap.size();

    return count;
}

int
AccountFactory::accountCount(const std::string& accountType) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    return getGenerator(accountType).accountMap.size();
}

std::shared_ptr<Account>
AccountFactory::getIP2IPAccount() const
{
    return ip2ip_account_.lock();
}

void AccountFactory::initIP2IPAccount()
{
    // cache this often used account using a weak_ptr
    ip2ip_account_ = createAccount(SIP_ACCOUNT_TYPE, SIPAccount::IP2IP_PROFILE);
    SIPVoIPLink::loadIP2IPSettings();
}
