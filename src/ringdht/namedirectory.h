/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "noncopyable.h"

#include <functional>
#include <map>
#include <string>
#include <mutex>

namespace ring {

class NameDirectory
{
public:
    enum class Response : int { found = 0, invalidName, notFound, error };
    enum class RegistrationResponse : int { success = 0,
                                            invalidName,
                                            alreadyTaken,
                                            error,
                                            incompleteRequest,
                                            signatureVerificationFailed
                                        };

    using LookupCallback = std::function<void(const std::string& result, Response response)>;
    using RegistrationCallback = std::function<void(RegistrationResponse response)>;

    NameDirectory() {}
    NameDirectory(const std::string& s);
    void load();

    static NameDirectory& instance(const std::string& server);
    static NameDirectory& instance() { return instance(DEFAULT_SERVER_HOST); }

    static void lookupUri(const std::string& uri, const std::string& default_server, LookupCallback cb);

    void lookupAddress(const std::string& addr, LookupCallback cb);
    void lookupName(const std::string& name, LookupCallback cb);

    void registerName(const std::string& addr, const std::string& name, const std::string& owner, RegistrationCallback cb, const std::string& signedname, const std::string& publickey);

    const std::string& getServer() const {
        return serverHost_;
    }

private:
    NON_COPYABLE(NameDirectory);
    NameDirectory(NameDirectory&&) = delete;
    constexpr static const char* const DEFAULT_SERVER_HOST = "ns.jami.net";

    std::mutex lock_ {};

    const std::string serverHost_ {DEFAULT_SERVER_HOST};
    const std::string cachePath_;

    std::map<std::string, std::string> nameCache_ {};
    std::map<std::string, std::string> addrCache_ {};

    std::string nameCache(const std::string& addr) {
        std::lock_guard<std::mutex> l(lock_);
        auto cacheRes = nameCache_.find(addr);
        return cacheRes != nameCache_.end() ? cacheRes->second : std::string{};
    }
    std::string addrCache(const std::string& name) {
        std::lock_guard<std::mutex> l(lock_);
        auto cacheRes = addrCache_.find(name);
        return cacheRes != addrCache_.end() ? cacheRes->second : std::string{};
    }

    bool validateName(const std::string& name) const;

    void saveCache();
    void loadCache();
};
}