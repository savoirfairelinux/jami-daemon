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

#include "logger.h"

#include <string>
#include <map>
#include <memory>
#include <mutex>

class Account;

/** AccountMap container.
 * Used to fetch an account from its ID
 */
typedef std::map<std::string, std::shared_ptr<Account> > AccountMap;

class AccountMapBase {
    public:
        typedef std::function<std::shared_ptr<Account>(const std::string&)> CreateAccountFunc;

        AccountMap accountMap = {};

        AccountMapBase(CreateAccountFunc&& f);

        std::shared_ptr<Account> createAccount(const std::string& accountID);

        bool hasAccount(const std::string& accountID) const;

        std::shared_ptr<Account> getAccount(const std::string& accountID) const;

    private:
        CreateAccountFunc create_ = nullptr;
};

class AccountFactory {
    public:
        static const std::string DEFAULT_TYPE;

        AccountFactory();

        bool isSupportedType(const std::string& accountType) const;

        bool hasAccount(const std::string& accountId);

        std::shared_ptr<Account> createAccount(const std::string& accountType,
                                               const std::string& accountId);

        void removeAccount(const std::string& accountId);

        AccountMap getAllAccounts() const;

        AccountMap getAllAccounts(const std::string& accountType) const;

        std::shared_ptr<Account> getAccount(const std::string& accountId) const;

        std::shared_ptr<Account> getAccount(const std::string& accountType,
                                            const std::string& accountId) const;

        std::shared_ptr<Account> getIP2IPAccount() const;

        bool empty() const;

        bool empty(const std::string& accountType) const;

        void initIP2IPAccount();

    private:
        mutable std::recursive_mutex mutex_ = {};
        std::map<std::string,  std::unique_ptr<AccountMapBase> > typedAccountMap_ = {};
        std::weak_ptr<Account> ip2ip_account_ = {}; //! cached pointer on IP2IP account
};

#endif // ACCOUNT_FACTORY_H
