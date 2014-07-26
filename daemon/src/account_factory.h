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

#ifndef ACCOUNT_FACTORY_H
#define ACCOUNT_FACTORY_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <utility>

class Account;
class AccountGeneratorBase;

/** Account container.
 * Used to store accounts using ID as key.
 */
typedef std::map<std::string, std::shared_ptr<Account> > AccountMap;

class AccountGeneratorBase {
    public:
        AccountMap accountMap = {};

        AccountGeneratorBase(const std::string& tname) : tname_(tname) {}

        virtual ~AccountGeneratorBase() {};

        virtual std::shared_ptr<Account>
        createAccount(const std::string& accountID) = 0;

        bool hasAccount(const std::string& accountID) const {
            return accountMap.find(accountID) != accountMap.end();
        }

        std::shared_ptr<Account>
        getAccount(const std::string& accountID) const {
            const auto& iter = accountMap.find(accountID);
            if (iter != accountMap.cend())
                return iter->second;
            return nullptr;
        }

        const std::string& getTypename() const {
            return tname_;
        }

        void clear() {
            accountMap.clear();
        }

    private:
        const std::string tname_;
};

template <typename T>
class AccountGenerator : public AccountGeneratorBase {
    public:
        AccountGenerator<T>(const std::string& tname)
        : AccountGeneratorBase(tname) {}

        virtual std::shared_ptr<Account>
        createAccount(const std::string& accountID) {
            auto account = std::make_shared<T>(accountID);
            accountMap.insert(std::make_pair(accountID, account));
            return account;
        }
};

class AccountFactory {
    public:
        static const char* const DEFAULT_ACCOUNT_TYPE;

        AccountFactory();

        bool isSupportedType(const std::string& accountType) const;

        std::shared_ptr<Account> createAccount(const std::string& accountType,
                                               const std::string& accountId);

        bool hasAccount(const std::string& accountId) const;

        void removeAccount(const std::string& accountId);
        void removeAccount(std::shared_ptr<Account> account);

        void clear();

        AccountMap getAllAccounts() const;
        AccountMap getAllAccounts(const std::string& accountType) const;

        std::shared_ptr<Account> getAccount(const std::string& accountId) const;
        std::shared_ptr<Account> getAccount(const std::string& accountType,
                                            const std::string& accountId) const;

        bool empty() const;
        bool empty(const std::string& accountType) const;

        int accountCount() const;
        int accountCount(const std::string& accountType) const;

        std::shared_ptr<Account> getIP2IPAccount() const;

        void initIP2IPAccount();

    private:
        const AccountGeneratorBase& getGenerator(const std::string& name) const;
        AccountGeneratorBase& getGenerator(const std::string& name);

        mutable std::recursive_mutex mutex_ = {};
        std::vector<std::unique_ptr<AccountGeneratorBase> > generators_;
        std::weak_ptr<Account> ip2ip_account_ = {}; //! cached pointer on IP2IP account
};

#endif // ACCOUNT_FACTORY_H
