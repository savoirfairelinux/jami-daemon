/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

template <class T> using AccountMap = std::map<std::string, std::shared_ptr<T> >;

class AccountFactory {
    public:
        static const char* const DEFAULT_ACCOUNT_TYPE;

        AccountFactory();

        bool isSupportedType(const char* const accountType) const;

        std::shared_ptr<Account> createAccount(const char* const accountType,
                                               const std::string& id);

        void removeAccount(Account& account);

        void removeAccount(const std::string& id);

        template <class T=Account>
        bool hasAccount(const std::string& id) const {
            std::lock_guard<std::recursive_mutex> lk(mutex_);

            const auto map = getMap_<T>();
            return map and map->find(id) != map->cend();
        }

        template <class T=Account>
        void clear() {
            std::lock_guard<std::recursive_mutex> lk(mutex_);

            auto map = getMap_<T>();
            if (!map) return;

            map->clear();
        }

        template <class T=Account>
        bool empty() const {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            const auto map = getMap_<T>();
            return map and map->empty();
        }

        template <class T=Account>
        std::size_t accountCount() const {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            const auto map = getMap_<T>();
            if (!map) return 0;

            return map->size();
        }

        template <class T=Account>
        std::shared_ptr<T>
        getAccount(const std::string& id) const {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            const auto map = getMap_<T>();
            if (!map) return nullptr;

            const auto& it = map->find(id);
            if (it == map->cend())
                return nullptr;

            return std::static_pointer_cast<T>(it->second);
        }

        template <class T=Account>
        std::vector<std::shared_ptr<T> > getAllAccounts() const {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            std::vector<std::shared_ptr<T> > v;

            if (const auto map = getMap_<T>()) {
                v.reserve(map->size());
                for (const auto& it : *map)
                    v.push_back(std::static_pointer_cast<T>(it.second));
            }
            return v;
        }

    private:
        mutable std::recursive_mutex mutex_ {};
        std::map<std::string, std::function<std::shared_ptr<Account>(const std::string&)> > generators_ {};
        std::map<std::string, AccountMap<Account> > accountMaps_ {};

        template <class T>
        const AccountMap<Account>* getMap_() const {
            const auto& itermap = accountMaps_.find(T::ACCOUNT_TYPE);

            if (itermap != accountMaps_.cend())
                return &itermap->second;

            return nullptr;
        }
};

template <>
bool
AccountFactory::hasAccount(const std::string& id) const;

template <>
void
AccountFactory::clear();

template <>
std::vector<std::shared_ptr<Account> >
AccountFactory::getAllAccounts() const;

template <>
std::shared_ptr<Account>
AccountFactory::getAccount(const std::string& accountId) const;

template <>
bool
AccountFactory::empty() const;

template <>
std::size_t
AccountFactory::accountCount() const;

} // namespace jami
