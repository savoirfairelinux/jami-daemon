/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
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
#include "config.h"
#include "namedirectory.h"

//#include <asio/ssl.hpp>

#include "logger.h"
#include "string_utils.h"
#include "fileutils.h"
#include "base64.h"
#include "scheduled_executor.h"

#include <asio.hpp>

#include "manager.h"
#include <opendht/thread_pool.h>
#include <opendht/crypto.h>
#include <opendht/utils.h>
#include <opendht/http.h>
#include <opendht/log_enable.h>

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
constexpr const char* const CACHE_DIRECTORY {"namecache"};

constexpr const char* const HTTPS_URI {"https://"};
static const std::string HTTPS_PROTO {"https"};

const std::string  HEX_PREFIX = "0x";
constexpr std::chrono::seconds SAVE_INTERVAL {5};

/** Parser for URIs.         ( protocol        )    ( username         ) ( hostname                            ) */
const std::regex URI_VALIDATOR {"^([a-zA-Z]+:(?://)?)?(?:([a-z0-9-_]{1,64})@)?([a-zA-Z0-9\\-._~%!$&'()*+,;=:\\[\\]]+)"};
const std::regex NAME_VALIDATOR {"^[a-zA-Z0-9-_]{3,32}$"};

constexpr size_t MAX_RESPONSE_SIZE {1024 * 1024};

using Request = dht::http::Request;

void
toLower(std::string& string)
{
    std::transform(string.begin(), string.end(), string.begin(), ::tolower);
}

void
NameDirectory::lookupUri(const std::string& uri, const std::string& default_server,
                         LookupCallback cb)
{
    std::smatch pieces_match;
    if (std::regex_match(uri, pieces_match, URI_VALIDATOR)) {
        if (pieces_match.size() == 4) {
            if (pieces_match[2].length() == 0)
                instance(default_server).lookupName(pieces_match[3], cb);
            else
                instance(pieces_match[3].str()).lookupName(pieces_match[2], cb);
            return;
        }
    }
    JAMI_ERR("Can't parse URI: %s", uri.c_str());
    cb("", Response::invalidResponse);
}

NameDirectory::NameDirectory(const std::string& s, std::shared_ptr<dht::Logger> l)
   : serverHost_(s)
   , cachePath_(fileutils::get_cache_dir() + DIR_SEPARATOR_STR + CACHE_DIRECTORY + DIR_SEPARATOR_STR + serverHost_)
   , logger_(l)
   , httpContext_(Manager::instance().ioContext())
   , resolver_(std::make_shared<dht::http::Resolver>(*httpContext_, serverHost_, HTTPS_PROTO, true, logger_))
{}

void
NameDirectory::load()
{
    loadCache();
}

NameDirectory&
NameDirectory::instance(const std::string& server, std::shared_ptr<dht::Logger> l)
{
    const std::string& s = server.empty() ? DEFAULT_SERVER_HOST : server;
    static std::mutex instanceMtx {};

    std::lock_guard<std::mutex> lock(instanceMtx);
    static std::map<std::string, NameDirectory> instances {};
    auto r = instances.emplace(std::piecewise_construct,
                      std::forward_as_tuple(s),
                      std::forward_as_tuple(s, l));
    if (r.second)
        r.first->second.load();
    return r.first->second;
}

void
NameDirectory::setHeaderFields(Request& request){
    const std::string host = HTTPS_PROTO + ":" + serverHost_;
    request.set_header_field(restinio::http_field_t::host, host);
    request.set_header_field(restinio::http_field_t::user_agent, "JamiDHT");
    request.set_header_field(restinio::http_field_t::accept, "*/*");
    request.set_header_field(restinio::http_field_t::content_type, "application/json");
}

void
NameDirectory::lookupAddress(const std::string& addr, LookupCallback cb)
{
    std::string cacheResult = nameCache(addr);
    if (not cacheResult.empty()) {
        cb(cacheResult, Response::found);
        return;
    }
    auto request = std::make_shared<Request>(*httpContext_, resolver_, logger_);
    auto reqid = request->id();
    try {
        request->set_connection_type(restinio::http_connection_header_t::keep_alive);
        request->set_target(QUERY_ADDR + addr);
        request->set_method(restinio::http_method_get());
        setHeaderFields(*request);

        const std::string uri = HTTPS_URI + serverHost_ + QUERY_ADDR + addr;
        JAMI_DBG("Address lookup for %s: %s", addr.c_str(), uri.c_str());
        request->add_on_state_change_callback([this, cb, reqid, addr]
                                              (Request::State state, const dht::http::Response& response){
            if (state != Request::State::DONE)
                return;
            if (response.status_code >= 400 && response.status_code < 500){
                cb("", Response::notFound);
            }
            else if (response.status_code != 200){
                JAMI_ERR("Adress lookup for %s failed with code=%i", addr.c_str(), response.status_code);
                cb("", Response::error);
            }
            try {
                Json::Value json;
                std::string err;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                    JAMI_DBG("Address lookup for %s: can't parse server response: %s", addr.c_str(), response.body.c_str());
                    cb("", Response::error);
                    return;
                }
                auto name = json["name"].asString();
                if (name.empty()){
                    cb(name, Response::notFound);
                    return;
                }
                JAMI_DBG("Found name for %s: %s", addr.c_str(), name.c_str());
                {
                    std::lock_guard<std::mutex> l(cacheLock_);
                    addrCache_.emplace(name, addr);
                    nameCache_.emplace(addr, name);
                }
                cb(name, Response::found);
                scheduleCacheSave();
            }
            catch (const std::exception& e) {
                JAMI_ERR("Error when performing address lookup: %s", e.what());
                cb("", Response::error);
            }
            requests_.erase(reqid);
        });
        request->send();
        requests_[reqid] = request;
    }
    catch (const std::exception &e){
        JAMI_ERR("Error when performing address lookup: %s", e.what());
        requests_.erase(reqid);
    }
}

bool
NameDirectory::verify(const std::string& name, const dht::crypto::PublicKey& pk,
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
        cb(name, Response::invalidResponse);
        return;
    }
    toLower(name);
    std::string cacheResult = addrCache(name);
    if (not cacheResult.empty()) {
        cb(cacheResult, Response::found);
        return;
    }
    auto request = std::make_shared<Request>(*httpContext_, resolver_, logger_);
    auto reqid = request->id();
    try {
        request->set_connection_type(restinio::http_connection_header_t::keep_alive);
        request->set_target(QUERY_NAME + name);
        request->set_method(restinio::http_method_get());
        setHeaderFields(*request);

        const std::string uri = HTTPS_URI + serverHost_ + QUERY_NAME + name;
        JAMI_DBG("Name lookup for %s: %s", name.c_str(), uri.c_str());

        request->add_on_state_change_callback([this, reqid, name, cb]
                                              (Request::State state, const dht::http::Response& response){
            if (state != Request::State::DONE)
                return;
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
                    if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                        JAMI_ERR("Name lookup for %s: can't parse server response: %s",
                                name.c_str(), response.body.c_str());
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
                    if (not publickey.empty() and not signature.empty()){
                        try {
                            auto pk = dht::crypto::PublicKey(base64::decode(publickey));
                            if (pk.getId().toString() != addr or
                                not verify(name, pk, signature))
                            {
                                cb("", Response::invalidResponse);
                                return;
                            }
                        } catch (const std::exception& e) {
                            cb("", Response::invalidResponse);
                            return;
                        }
                    }
                    JAMI_DBG("Found address for %s: %s", name.c_str(), addr.c_str());
                    {
                        std::lock_guard<std::mutex> l(cacheLock_);
                        addrCache_.emplace(name, addr);
                        nameCache_.emplace(addr, name);
                    }
                    cb(addr, Response::found);
                    scheduleCacheSave();
                }
                catch (const std::exception& e) {
                    JAMI_ERR("Error when performing name lookup: %s", e.what());
                    cb("", Response::error);
                }
            }
            requests_.erase(reqid);
        });
        request->send();
        requests_[reqid] = request;
    }
    catch (const std::exception &e){
        JAMI_ERR("Name lookup for %s failed: %s", name.c_str(), e.what());
        requests_.erase(reqid);
    }
}

bool
NameDirectory::validateName(const std::string& name) const
{
    return std::regex_match(name, NAME_VALIDATOR);
}

using Blob = std::vector<uint8_t>;
void NameDirectory::registerName(const std::string& addr, const std::string& n,
                                 const std::string& owner, RegistrationCallback cb,
                                 const std::string& signedname, const std::string& publickey)
{
    std::string name {n};
    if (not validateName(name)) {
        cb(RegistrationResponse::invalidName);
        return;
    }
    toLower(name);
    auto cacheResult = addrCache(name);
    if (not cacheResult.empty()) {
        if (cacheResult == addr)
            cb(RegistrationResponse::success);
        else
            cb(RegistrationResponse::alreadyTaken);
        return;
    }
    std::string body;
    {
        std::stringstream ss;
        ss << "{\"addr\":\"" << addr << "\",\"owner\":\"" << owner <<
            "\",\"signature\":\"" << signedname <<
            "\",\"publickey\":\"" << base64::encode(
                    jami::Blob(publickey.begin(), publickey.end()))  << "\"}";
        body = ss.str();
    }
    auto request = std::make_shared<Request>(*httpContext_, resolver_, logger_);
    auto reqid = request->id();
    try {
        request->set_connection_type(restinio::http_connection_header_t::keep_alive);
        request->set_target(QUERY_NAME + name);
        request->set_method(restinio::http_method_post());
        setHeaderFields(*request);
        request->set_body(body);

        JAMI_WARN("RegisterName: sending request %s %s", addr.c_str(), name.c_str());

        request->add_on_state_change_callback([this, reqid, name, addr, cb]
                                              (Request::State state, const dht::http::Response& response){
            if (state != Request::State::DONE)
                return;
            if (response.status_code == 400){
                cb(RegistrationResponse::incompleteRequest);
                JAMI_ERR("RegistrationResponse::incompleteRequest");
            } else if (response.status_code == 401){
                cb(RegistrationResponse::signatureVerificationFailed);
                JAMI_ERR("RegistrationResponse::signatureVerificationFailed");
            } else if (response.status_code == 403){
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else if (response.status_code == 409){
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else if (response.status_code > 400 && response.status_code < 500){
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else if (response.status_code < 200 || response.status_code > 299){
                cb(RegistrationResponse::error);
                JAMI_ERR("RegistrationResponse::error");
            } else {
                Json::Value json;
                std::string err;
                Json::CharReaderBuilder rbuilder;
                
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                    cb(RegistrationResponse::error);
                    return;
                }
                auto success = json["success"].asBool();
                JAMI_DBG("Got reply for registration of %s %s: %s",
                            name.c_str(), addr.c_str(), success ? "success" : "failure");
                if (success){
                    std::lock_guard<std::mutex> l(cacheLock_);
                    addrCache_.emplace(name, addr);
                    nameCache_.emplace(addr, name);
                }
                cb(success ? RegistrationResponse::success : RegistrationResponse::error);
            }
            requests_.erase(reqid);
        });
        request->send();
        requests_[reqid] = request;
    }
    catch (const std::exception &e){
        JAMI_ERR("Error when performing name registration: %s", e.what());
        cb(RegistrationResponse::error);
        requests_.erase(reqid);
    }
}

void
NameDirectory::scheduleCacheSave()
{
    JAMI_DBG("Scheduling cache save to %s", cachePath_.c_str());
    std::weak_ptr<Task> task = Manager::instance().scheduler().scheduleIn([this]{
        dht::ThreadPool::io().run([this] {
            saveCache();
        });
    }, SAVE_INTERVAL);
    std::swap(saveTask_, task);
    if (auto old = task.lock())
        old->cancel();
}

void
NameDirectory::saveCache()
{
    fileutils::recursive_mkdir(fileutils::get_cache_dir()+DIR_SEPARATOR_STR+CACHE_DIRECTORY);
    std::lock_guard<std::mutex> lock(fileutils::getFileLock(cachePath_));
    std::ofstream file = fileutils::ofstream(cachePath_, std::ios::trunc | std::ios::binary);
    {
        std::lock_guard<std::mutex> l(cacheLock_);
        msgpack::pack(file, nameCache_);
    }
    JAMI_DBG("Saved %lu name-address mappings to %s",
            (long unsigned) nameCache_.size(), cachePath_.c_str());
}

void
NameDirectory::loadCache()
{
    msgpack::unpacker pac;

    // read file
    {
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(cachePath_));
        std::ifstream file = fileutils::ifstream(cachePath_);
        if (!file.is_open()) {
            JAMI_DBG("Could not load %s", cachePath_.c_str());
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
    std::lock_guard<std::mutex> l(cacheLock_);
    msgpack::object_handle oh;
    if (pac.next(oh))
        oh.get().convert(nameCache_);
    for (const auto& m : nameCache_)
        addrCache_.emplace(m.second, m.first);
    JAMI_DBG("Loaded %lu name-address mappings", (long unsigned) nameCache_.size());
}

}
