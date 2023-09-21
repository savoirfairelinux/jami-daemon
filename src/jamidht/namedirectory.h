/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *          Vsevolod Ivanov <vsevolod.ivanov@savoirfairelinux.com>
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

#include <asio/io_context.hpp>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>
#include <thread>
#include <filesystem>

namespace dht {
class Executor;
namespace crypto {
struct PublicKey;
}
namespace http {
class Request;
struct Response;
class Resolver;
} // namespace http
namespace log {
struct Logger;
}
using Logger = log::Logger;
} // namespace dht

namespace jami {

class Task;

class NameDirectory
{
public:
    enum class Response : int { found = 0, invalidResponse, notFound, error };
    enum class RegistrationResponse : int {
        success = 0,
        invalidName,
        invalidCredentials,
        alreadyTaken,
        error,
        incompleteRequest,
        signatureVerificationFailed,
        unsupported
    };

    using LookupCallback = std::function<void(const std::string& result, Response response)>;
    using SearchResult = std::vector<std::map<std::string, std::string>>;
    using SearchCallback = std::function<void(const SearchResult& result, Response response)>;
    using RegistrationCallback = std::function<void(RegistrationResponse response)>;

    NameDirectory(const std::string& serverUrl, std::shared_ptr<dht::Logger> l = {});
    ~NameDirectory();
    void load();

    static NameDirectory& instance(const std::string& serverUrl,
                                   std::shared_ptr<dht::Logger> l = {});
    static NameDirectory& instance();

    void setToken(std::string token) { serverToken_ = std::move(token); }

    static void lookupUri(std::string_view uri,
                          const std::string& default_server,
                          LookupCallback cb);

    void lookupAddress(const std::string& addr, LookupCallback cb);
    void lookupName(const std::string& name, LookupCallback cb);
    bool searchName(const std::string& /*name*/, SearchCallback /*cb*/) { return false; }

    void registerName(const std::string& addr,
                      const std::string& name,
                      const std::string& owner,
                      RegistrationCallback cb,
                      const std::string& signedname,
                      const std::string& publickey);

private:
    NON_COPYABLE(NameDirectory);
    NameDirectory(NameDirectory&&) = delete;
    NameDirectory& operator=(NameDirectory&&) = delete;

    std::string serverUrl_;
    std::string serverToken_;
    std::filesystem::path cachePath_;

    std::mutex cacheLock_ {};
    std::shared_ptr<dht::Logger> logger_;

    /*
     * ASIO I/O Context for sockets in httpClient_.
     * Note: Each context is used in one thread only.
     */
    std::shared_ptr<asio::io_context> httpContext_;
    std::shared_ptr<dht::http::Resolver> resolver_;
    std::mutex requestsMtx_ {};
    std::set<std::shared_ptr<dht::http::Request>> requests_;

    std::map<std::string, std::string> nameCache_ {};
    std::map<std::string, std::string> addrCache_ {};

    std::weak_ptr<Task> saveTask_;

    void setHeaderFields(dht::http::Request& request);

    std::string nameCache(const std::string& addr)
    {
        std::lock_guard<std::mutex> l(cacheLock_);
        auto cacheRes = nameCache_.find(addr);
        return cacheRes != nameCache_.end() ? cacheRes->second : std::string {};
    }
    std::string addrCache(const std::string& name)
    {
        std::lock_guard<std::mutex> l(cacheLock_);
        auto cacheRes = addrCache_.find(name);
        return cacheRes != addrCache_.end() ? cacheRes->second : std::string {};
    }

    bool validateName(const std::string& name) const;
    static bool verify(const std::string& name,
                       const dht::crypto::PublicKey& publickey,
                       const std::string& signature);

    void scheduleCacheSave();
    void saveCache();
    void loadCache();
};
} // namespace jami
