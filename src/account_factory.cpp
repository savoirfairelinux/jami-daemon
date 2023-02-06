/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "account_factory.h"

#include "sip/sipaccount.h"
#include "jamidht/jamiaccount.h"

#include <stdexcept>

namespace jami {

const std::string_view AccountFactory::DEFAULT_ACCOUNT_TYPE = SIPAccount::ACCOUNT_TYPE;

AccountFactory::AccountFactory()
{
    generators_.emplace(SIPAccount::ACCOUNT_TYPE, [](const std::string& id) {
        return std::make_shared<SIPAccount>(id, true);
    });
    generators_.emplace(JamiAccount::ACCOUNT_TYPE, [](const std::string& id) {
        return std::make_shared<JamiAccount>(id);
    });
}

std::shared_ptr<Account>
AccountFactory::createAccount(std::string_view accountType, const std::string& id)
{
    if (hasAccount(id)) {
        JAMI_ERROR("Existing account {}", id);
        return nullptr;
    }

    const auto& it = generators_.find(accountType);
    if (it == generators_.cend())
        return {};

    std::shared_ptr<Account> account = it->second(id);
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto m = accountMaps_.find(accountType);
        if (m == accountMaps_.end())
            m = accountMaps_.emplace(std::string(accountType), AccountMap<Account>{}).first;
        m->second.emplace(id, account);
    }
    return account;
}

bool
AccountFactory::isSupportedType(std::string_view name) const
{
    return generators_.find(name) != generators_.cend();
}

void
AccountFactory::removeAccount(Account& account)
{
    std::string_view account_type = account.getAccountType();
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto& id = account.getAccountID();
    JAMI_DEBUG("Removing account {:s}", id);
    auto m = accountMaps_.find(account_type);
    if (m != accountMaps_.end()) {
        m->second.erase(id);
        JAMI_DEBUG("Remaining {:d} {:s} account(s)", m->second.size(), account_type);
    }
}

void
AccountFactory::removeAccount(std::string_view id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (auto account = getAccount(id)) {
        removeAccount(*account);
    } else
        JAMI_ERROR("No account with ID {:s}", id);
}

template<>
bool
AccountFactory::hasAccount(std::string_view id) const
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);

    for (const auto& item : accountMaps_) {
        const auto& map = item.second;
        if (map.find(id) != map.cend())
            return true;
    }

    return false;
}

template<>
void
AccountFactory::clear()
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    accountMaps_.clear();
}

template<>
std::vector<std::shared_ptr<Account>>
AccountFactory::getAllAccounts() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::shared_ptr<Account>> v;

    for (const auto& itemmap : accountMaps_) {
        const auto& map = itemmap.second;
        v.reserve(v.size() + map.size());
        for (const auto& item : map)
            v.push_back(item.second);
    }

    return v;
}

template<>
std::shared_ptr<Account>
AccountFactory::getAccount(std::string_view id) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& item : accountMaps_) {
        const auto& map = item.second;
        const auto& iter = map.find(id);
        if (iter != map.cend())
            return iter->second;
    }

    return nullptr;
}

template<>
bool
AccountFactory::empty() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& item : accountMaps_) {
        const auto& map = item.second;
        if (!map.empty())
            return false;
    }

    return true;
}

template<>
std::size_t
AccountFactory::accountCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::size_t count = 0;

    for (const auto& it : accountMaps_)
        count += it.second.size();

    return count;
}

} // namespace jami
