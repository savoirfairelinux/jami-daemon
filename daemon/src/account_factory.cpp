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
#if HAVE_DHT
#include "dht/dhtaccount.h"
#endif

#include "sip/sipvoiplink.h" // for SIPVoIPLink::loadIP2IPSettings

#include <stdexcept>

const char* const AccountFactory::DEFAULT_ACCOUNT_TYPE = SIPAccount::ACCOUNT_TYPE;

AccountFactory::AccountFactory()
{
    auto sipfunc = [](const std::string& id){ return std::make_shared<SIPAccount>(id, true); };
    generators_.insert(std::make_pair(SIPAccount::ACCOUNT_TYPE, sipfunc));
    SFL_DBG("registered %s account", SIPAccount::ACCOUNT_TYPE);
#if HAVE_IAX
    auto iaxfunc = [](const std::string& id){ return std::make_shared<IAXAccount>(id); };
    generators_.insert(std::make_pair(IAXAccount::ACCOUNT_TYPE, iaxfunc));
    SFL_DBG("registered %s account", IAXAccount::ACCOUNT_TYPE);
#endif
#if HAVE_DHT
    auto dhtfunc = [](const std::string& id){ return std::make_shared<DHTAccount>(id, false); };
    generators_.insert(std::make_pair(DHTAccount::ACCOUNT_TYPE, dhtfunc));
    DEBUG("registered %s account", DHTAccount::ACCOUNT_TYPE);
#endif
}

std::shared_ptr<Account>
AccountFactory::createAccount(const char* const accountType,
                              const std::string& id)
{
     if (hasAccount(id)) {
         SFL_ERR("Existing account %s", id.c_str());
         return nullptr;
     }

     std::shared_ptr<Account> account;
     {
         const auto& it = generators_.find(accountType);
         if (it != generators_.cend())
             account = it->second(id);
     }

     {
         std::lock_guard<std::recursive_mutex> lock(mutex_);
         accountMaps_[accountType].insert(std::make_pair(id, account));
     }

     return account;
 }

bool
AccountFactory::isSupportedType(const char* const name) const
{
    return generators_.find(name) != generators_.cend();
}

void
AccountFactory::removeAccount(Account& account)
{
    const auto account_type = account.getAccountType();

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto& id = account.getAccountID();
    SFL_DBG("Removing account %s", id.c_str());
    auto& map = accountMaps_.at(account.getAccountType());
    map.erase(id);
    SFL_DBG("Remaining %u %s account(s)", map.size(), account_type);
}

void
AccountFactory::removeAccount(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (auto account = getAccount(id)) {
        removeAccount(*account);
    } else
        SFL_ERR("No account with ID %s", id.c_str());
}

template <> bool
AccountFactory::hasAccount(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);

    for (const auto& item : accountMaps_) {
        const auto& map = item.second;
        if (map.find(id) != map.cend())
            return true;
    }

    return false;
}

template <> void
AccountFactory::clear()
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    accountMaps_.clear();
}

template <>
std::vector<std::shared_ptr<Account> >
AccountFactory::getAllAccounts() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::shared_ptr<Account> > v;

    for (const auto& itemmap : accountMaps_) {
        const auto& map = itemmap.second;
        for (const auto item : map)
            v.push_back(item.second);
    }

    v.shrink_to_fit();
    return v;
}

template <>
std::shared_ptr<Account>
AccountFactory::getAccount(const std::string& id) const
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

template <>
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

template <>
std::size_t
AccountFactory::accountCount() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::size_t count = 0;

    for (const auto& it : accountMaps_)
        count += it.second.size();

    return count;
}

std::shared_ptr<Account>
AccountFactory::getIP2IPAccount() const
{
    return ip2ip_account_.lock();
}

void AccountFactory::initIP2IPAccount()
{
    // cache this often used account using a weak_ptr
    ip2ip_account_ = createAccount(SIPAccount::ACCOUNT_TYPE,
                                   SIPAccount::IP2IP_PROFILE);
}
