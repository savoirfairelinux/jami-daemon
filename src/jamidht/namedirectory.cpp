/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "namedirectory.h"

#include "logger.h"
#include "string_utils.h"
#include "fileutils.h"
#include "base64.h"
#include "scheduled_executor.h"

#include <asio.hpp>

#include "manager.h"
#include <opendht/crypto.h>
#include <opendht/utils.h>
#include <opendht/http.h>
#include <opendht/logger.h>
#include <opendht/thread_pool.h>

#include <cstddef>
#include <msgpack.hpp>
#include <json/json.h>

/* for visual studio */
#include <ciso646>
#include <sstream>
#include <regex>
#include <fstream>

namespace jami {

constexpr const char* const QUERY_NAME {"/name/"};
constexpr const char* const QUERY_ADDR {"/addr/"};
constexpr auto CACHE_DIRECTORY {"namecache"sv};
constexpr const char DEFAULT_SERVER_HOST[] = "https://ns.jami.net";

const std::string HEX_PREFIX = "0x";
constexpr std::chrono::seconds SAVE_INTERVAL {5};

/** Parser for URIs.         ( protocol        )    ( username         ) ( hostname ) */
const std::regex URI_VALIDATOR {
    "^([a-zA-Z]+:(?://)?)?(?:([a-z0-9-_]{1,64})@)?([a-zA-Z0-9\\-._~%!$&'()*+,;=:\\[\\]]+)"};
const std::regex NAME_VALIDATOR {"^[a-zA-Z0-9-_]{3,32}$"};

constexpr size_t MAX_RESPONSE_SIZE {1024ul * 1024};

using Request = dht::http::Request;

void
toLower(std::string& string)
{
    std::transform(string.begin(), string.end(), string.begin(), ::tolower);
}

NameDirectory&
NameDirectory::instance()
{
    return instance(DEFAULT_SERVER_HOST);
}

void
NameDirectory::lookupUri(std::string_view uri, const std::string& default_server, LookupCallback cb)
{
    const std::string& default_ns = default_server.empty() ? DEFAULT_SERVER_HOST : default_server;
    std::svmatch pieces_match;
    if (std::regex_match(uri, pieces_match, URI_VALIDATOR)) {
        if (pieces_match.size() == 4) {
            if (pieces_match[2].length() == 0)
                instance(default_ns).lookupName(pieces_match[3], std::move(cb));
            else
                instance(pieces_match[3].str()).lookupName(pieces_match[2], std::move(cb));
            return;
        }
    }
    JAMI_ERROR("Unable to parse URI: {}", uri);
    cb("", Response::invalidResponse);
}

NameDirectory::NameDirectory(const std::string& serverUrl, std::shared_ptr<dht::Logger> l)
    : serverUrl_(serverUrl)
    , logger_(std::move(l))
    , httpContext_(Manager::instance().ioContext())
{
    if (!serverUrl_.empty() && serverUrl_.back() == '/')
        serverUrl_.pop_back();
    resolver_ = std::make_shared<dht::http::Resolver>(*httpContext_, serverUrl, logger_);
    cachePath_ = fileutils::get_cache_dir() / CACHE_DIRECTORY / resolver_->get_url().host;
}

NameDirectory::~NameDirectory()
{
    decltype(requests_) requests;
    {
        std::lock_guard lk(requestsMtx_);
        requests = std::move(requests_);
    }
    for (auto& req : requests)
        req->cancel();
}

void
NameDirectory::load()
{
    loadCache();
}

NameDirectory&
NameDirectory::instance(const std::string& serverUrl, std::shared_ptr<dht::Logger> l)
{
    const std::string& s = serverUrl.empty() ? DEFAULT_SERVER_HOST : serverUrl;
    static std::mutex instanceMtx {};

    std::lock_guard lock(instanceMtx);
    static std::map<std::string, NameDirectory> instances {};
    auto it = instances.find(s);
    if (it != instances.end())
        return it->second;
    auto r = instances.emplace(std::piecewise_construct,
                               std::forward_as_tuple(s),
                               std::forward_as_tuple(s, l));
    if (r.second)
        r.first->second.load();
    return r.first->second;
}

void
NameDirectory::setHeaderFields(Request& request)
{
    request.set_header_field(restinio::http_field_t::user_agent, "JamiDHT");
    request.set_header_field(restinio::http_field_t::accept, "*/*");
    request.set_header_field(restinio::http_field_t::content_type, "application/json");
}

void
NameDirectory::lookupAddress(const std::string& addr, LookupCallback cb)
{
    auto cacheResult = nameCache(addr);
    if (not cacheResult.empty()) {
        cb(cacheResult, Response::found);
        return;
    }
    auto request = std::make_shared<Request>(*httpContext_,
                                             resolver_,
                                             serverUrl_ + QUERY_ADDR + addr);
    try {
        request->set_method(restinio::http_method_get());
        setHeaderFields(*request);
        request->add_on_done_callback(
            [this, cb = std::move(cb), addr](const dht::http::Response& response) {
                if (response.status_code >= 400 && response.status_code < 500) {
                    auto cacheResult = nameCache(addr);
                    if (not cacheResult.empty())
                        cb(cacheResult, Response::found);
                    else
                        cb("", Response::notFound);
                } else if (response.status_code != 200) {
                    JAMI_ERROR("Address lookup for {} failed with code={}",
                             addr, response.status_code);
                    cb("", Response::error);
                } else {
                    try {
                        Json::Value json;
                        std::string err;
                        Json::CharReaderBuilder rbuilder;
                        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                        if (!reader->parse(response.body.data(),
                                           response.body.data() + response.body.size(),
                                           &json,
                                           &err)) {
                            JAMI_DBG("Address lookup for %s: Unable to parse server response: %s",
                                     addr.c_str(),
                                     response.body.c_str());
                            cb("", Response::error);
                            return;
                        }
                        auto name = json["name"].asString();
                        if (name.empty()) {
                            cb(name, Response::notFound);
                            return;
                        }
                        JAMI_DEBUG("Found name for {}: {}", addr, name);
                        {
                            std::lock_guard l(cacheLock_);
                            addrCache_.emplace(name, addr);
                            nameCache_.emplace(addr, name);
                        }
                        cb(name, Response::found);
                        scheduleCacheSave();
                    } catch (const std::exception& e) {
                        JAMI_ERROR("Error when performing address lookup: {}", e.what());
                        cb("", Response::error);
                    }
                }
                std::lock_guard lk(requestsMtx_);
                if (auto req = response.request.lock())
                    requests_.erase(req);
            });
        {
            std::lock_guard lk(requestsMtx_);
            requests_.emplace(request);
        }
        request->send();
    } catch (const std::exception& e) {
        JAMI_ERROR("Error when performing address lookup: {}", e.what());
        std::lock_guard lk(requestsMtx_);
        if (request)
            requests_.erase(request);
    }
}

bool
NameDirectory::verify(const std::string& name,
                      const dht::crypto::PublicKey& pk,
                      const std::string& signature)
{
    return pk.checkSignature(std::vector<uint8_t>(name.begin(), name.end()),
                             base64::decode(signature));
}

void
NameDirectory::lookupName(const std::string& n, LookupCallback cb)
{
    std::string name {n};
    if (not validateName(name)) {
        cb("", Response::invalidResponse);
        return;
    }
    toLower(name);
    std::string cacheResult = addrCache(name);
    if (not cacheResult.empty()) {
        cb(cacheResult, Response::found);
        return;
    }
    auto request = std::make_shared<Request>(*httpContext_,
                                             resolver_,
                                             serverUrl_ + QUERY_NAME + name);
    try {
        request->set_method(restinio::http_method_get());
        setHeaderFields(*request);
        request->add_on_done_callback([this, name, cb = std::move(cb)](
                                          const dht::http::Response& response) {
            if (response.status_code >= 400 && response.status_code < 500)
                cb("", Response::notFound);
            else if (response.status_code < 200 || response.status_code > 299)
                cb("", Response::error);
            else {
                try {
                    Json::Value json;
                    std::string err;
                    Json::CharReaderBuilder rbuilder;
                    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                    if (!reader->parse(response.body.data(),
                                       response.body.data() + response.body.size(),
                                       &json,
                                       &err)) {
                        JAMI_ERROR("Name lookup for {}: Unable to parse server response: {}",
                                 name, response.body);
                        cb("", Response::error);
                        return;
                    }
                    auto addr = json["addr"].asString();
                    auto publickey = json["publickey"].asString();
                    auto signature = json["signature"].asString();

                    if (!addr.compare(0, HEX_PREFIX.size(), HEX_PREFIX))
                        addr = addr.substr(HEX_PREFIX.size());
                    if (addr.empty()) {
                        cb("", Response::notFound);
                        return;
                    }
                    if (not publickey.empty() and not signature.empty()) {
                        try {
                            auto pk = dht::crypto::PublicKey(base64::decode(publickey));
                            if (pk.getId().toString() != addr or not verify(name, pk, signature)) {
                                cb("", Response::invalidResponse);
                                return;
                            }
                        } catch (const std::exception& e) {
                            cb("", Response::invalidResponse);
                            return;
                        }
                    }
                    JAMI_DEBUG("Found address for {}: {}", name, addr);
                    {
                        std::lock_guard l(cacheLock_);
                        addrCache_.emplace(name, addr);
                        nameCache_.emplace(addr, name);
                    }
                    cb(addr, Response::found);
                    scheduleCacheSave();
                } catch (const std::exception& e) {
                    JAMI_ERROR("Error when performing name lookup: {}", e.what());
                    cb("", Response::error);
                }
            }
            if (auto req = response.request.lock())
                requests_.erase(req);
        });
        {
            std::lock_guard lk(requestsMtx_);
            requests_.emplace(request);
        }
        request->send();
    } catch (const std::exception& e) {
        JAMI_ERROR("Name lookup for {} failed: {}", name, e.what());
        std::lock_guard lk(requestsMtx_);
        if (request)
            requests_.erase(request);
    }
}

bool
NameDirectory::validateName(const std::string& name) const
{
    return std::regex_match(name, NAME_VALIDATOR);
}

using Blob = std::vector<uint8_t>;
void
NameDirectory::registerName(const std::string& addr,
                            const std::string& n,
                            const std::string& owner,
                            RegistrationCallback cb,
                            const std::string& signedname,
                            const std::string& publickey)
{
    std::string name {n};
    if (not validateName(name)) {
        cb(RegistrationResponse::invalidName, name);
        return;
    }
    toLower(name);
    auto cacheResult = addrCache(name);
    if (not cacheResult.empty()) {
        if (cacheResult == addr)
            cb(RegistrationResponse::success, name);
        else
            cb(RegistrationResponse::alreadyTaken, name);
        return;
    }
    {
        std::lock_guard l(cacheLock_);
        if (not pendingRegistrations_.emplace(addr, name).second) {
            JAMI_WARNING("RegisterName: already registering name {} {}", addr, name);
            cb(RegistrationResponse::error, name);
            return;
        }
    }
    std::string body = fmt::format("{{\"addr\":\"{}\",\"owner\":\"{}\",\"signature\":\"{}\",\"publickey\":\"{}\"}}",
        addr,
        owner,
        signedname,
        base64::encode(publickey));
    auto request = std::make_shared<Request>(*httpContext_,
                                             resolver_,
                                             serverUrl_ + QUERY_NAME + name);
    try {
        request->set_method(restinio::http_method_post());
        setHeaderFields(*request);
        request->set_body(body);

        JAMI_WARNING("RegisterName: sending request {} {}", addr, name);

        request->add_on_done_callback(
            [this, name, addr, cb = std::move(cb)](const dht::http::Response& response) {
                {
                    std::lock_guard l(cacheLock_);
                    pendingRegistrations_.erase(name);
                }
                if (response.status_code == 400) {
                    cb(RegistrationResponse::incompleteRequest, name);
                    JAMI_ERROR("RegistrationResponse::incompleteRequest");
                } else if (response.status_code == 401) {
                    cb(RegistrationResponse::signatureVerificationFailed, name);
                    JAMI_ERROR("RegistrationResponse::signatureVerificationFailed");
                } else if (response.status_code == 403) {
                    cb(RegistrationResponse::alreadyTaken, name);
                    JAMI_ERROR("RegistrationResponse::alreadyTaken");
                } else if (response.status_code == 409) {
                    cb(RegistrationResponse::alreadyTaken, name);
                    JAMI_ERROR("RegistrationResponse::alreadyTaken");
                } else if (response.status_code > 400 && response.status_code < 500) {
                    cb(RegistrationResponse::alreadyTaken, name);
                    JAMI_ERROR("RegistrationResponse::alreadyTaken");
                } else if (response.status_code < 200 || response.status_code > 299) {
                    cb(RegistrationResponse::error, name);
                    JAMI_ERROR("RegistrationResponse::error");
                } else {
                    Json::Value json;
                    std::string err;
                    Json::CharReaderBuilder rbuilder;

                    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                    if (!reader->parse(response.body.data(),
                                       response.body.data() + response.body.size(),
                                       &json,
                                       &err)) {
                        cb(RegistrationResponse::error, name);
                        return;
                    }
                    auto success = json["success"].asBool();
                    JAMI_DEBUG("Got reply for registration of {} {}: {}",
                             name, addr, success ? "success" : "failure");
                    if (success) {
                        std::lock_guard l(cacheLock_);
                        addrCache_.emplace(name, addr);
                        nameCache_.emplace(addr, name);
                    }
                    cb(success ? RegistrationResponse::success : RegistrationResponse::error, name);
                }
                std::lock_guard lk(requestsMtx_);
                if (auto req = response.request.lock())
                    requests_.erase(req);
            });
        {
            std::lock_guard lk(requestsMtx_);
            requests_.emplace(request);
        }
        request->send();
    } catch (const std::exception& e) {
        JAMI_ERROR("Error when performing name registration: {}", e.what());
        cb(RegistrationResponse::error, name);
        {
            std::lock_guard l(cacheLock_);
            pendingRegistrations_.erase(name);
        }
        std::lock_guard lk(requestsMtx_);
        if (request)
            requests_.erase(request);
    }
}

void
NameDirectory::scheduleCacheSave()
{
    // JAMI_DBG("Scheduling cache save to %s", cachePath_.c_str());
    std::weak_ptr<Task> task = Manager::instance().scheduler().scheduleIn(
        [this] { dht::ThreadPool::io().run([this] { saveCache(); }); }, SAVE_INTERVAL);
    std::swap(saveTask_, task);
    if (auto old = task.lock())
        old->cancel();
}

void
NameDirectory::saveCache()
{
    dhtnet::fileutils::recursive_mkdir(fileutils::get_cache_dir() / CACHE_DIRECTORY);
    std::lock_guard lock(dhtnet::fileutils::getFileLock(cachePath_));
    std::ofstream file(cachePath_, std::ios::trunc | std::ios::binary);
    {
        std::lock_guard l(cacheLock_);
        msgpack::pack(file, nameCache_);
    }
    JAMI_DEBUG("Saved {:d} name-address mappings to {}",
             nameCache_.size(), cachePath_);
}

void
NameDirectory::loadCache()
{
    msgpack::unpacker pac;

    // read file
    {
        std::lock_guard lock(dhtnet::fileutils::getFileLock(cachePath_));
        std::ifstream file(cachePath_);
        if (!file.is_open()) {
            JAMI_DEBUG("Unable to load %s", cachePath_);
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            pac.reserve_buffer(line.size());
            memcpy(pac.buffer(), line.data(), line.size());
            pac.buffer_consumed(line.size());
        }
    }

    // load values
    std::lock_guard l(cacheLock_);
    msgpack::object_handle oh;
    if (pac.next(oh))
        oh.get().convert(nameCache_);
    for (const auto& m : nameCache_)
        addrCache_.emplace(m.second, m.first);
    JAMI_DEBUG("Loaded {:d} name-address mappings", nameCache_.size());
}

} // namespace jami
