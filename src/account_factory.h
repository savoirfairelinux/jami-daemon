/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <utility>
#include <functional>
#include <ciso646>

namespace jami {

class Account;
class AccountGeneratorBase;

template<class T>
using AccountMap = std::map<std::string, std::shared_ptr<T>, std::less<>>;

class AccountFactory
{
public:
    static const std::string_view DEFAULT_ACCOUNT_TYPE;// = SIPAccount::ACCOUNT_TYPE;

    AccountFactory();

    bool isSupportedType(std::string_view accountType) const;

    std::shared_ptr<Account> createAccount(std::string_view accountType, const std::string& id);

    void removeAccount(Account& account);

    void removeAccount(std::string_view id);

    template<class T = Account>
    bool hasAccount(std::string_view id) const
    {
        std::lock_guard lk(mutex_);

        const auto map = getMap_<T>();
        return map and map->find(id) != map->cend();
    }

    template<class T = Account>
    void clear()
    {
        std::lock_guard lk(mutex_);

        auto map = getMap_<T>();
        if (!map)
            return;

        map->clear();
    }

    template<class T = Account>
    bool empty() const
    {
        std::lock_guard lock(mutex_);

        const auto map = getMap_<T>();
        return map and map->empty();
    }

    template<class T = Account>
    std::size_t accountCount() const
    {
        std::lock_guard lock(mutex_);

        const auto map = getMap_<T>();
        if (!map)
            return 0;

        return map->size();
    }

    template<class T = Account>
    std::shared_ptr<T> getAccount(std::string_view id) const
    {
        std::lock_guard lock(mutex_);

        const auto map = getMap_<T>();
        if (!map)
            return nullptr;

        const auto& it = map->find(id);
        if (it == map->cend())
            return nullptr;

        return std::static_pointer_cast<T>(it->second);
    }

    template<class T = Account>
    std::vector<std::shared_ptr<T>> getAllAccounts() const
    {
        std::lock_guard lock(mutex_);
        std::vector<std::shared_ptr<T>> v;

        if (const auto map = getMap_<T>()) {
            v.reserve(map->size());
            for (const auto& it : *map)
                v.emplace_back(std::static_pointer_cast<T>(it.second));
        }
        return v;
    }

private:
    mutable std::recursive_mutex mutex_ {};
    std::map<std::string, std::function<std::shared_ptr<Account>(const std::string&)>, std::less<>> generators_ {};
    std::map<std::string, AccountMap<Account>, std::less<>> accountMaps_ {};

    template<class T>
    const AccountMap<Account>* getMap_() const
    {
        const auto& itermap = accountMaps_.find(T::ACCOUNT_TYPE);
        if (itermap != accountMaps_.cend())
            return &itermap->second;
        return nullptr;
    }
};

template<>
bool AccountFactory::hasAccount(std::string_view id) const;

template<>
void AccountFactory::clear();

template<>
std::vector<std::shared_ptr<Account>> AccountFactory::getAllAccounts() const;

template<>
std::shared_ptr<Account> AccountFactory::getAccount(std::string_view accountId) const;

template<>
bool AccountFactory::empty() const;

template<>
std::size_t AccountFactory::accountCount() const;

} // namespace jami
