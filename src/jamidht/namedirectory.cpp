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
#include "config.h"
#include "namedirectory.h"

#include "logger.h"
#include "string_utils.h"
#include "thread_pool.h"
#include "fileutils.h"
#include "base64.h"

#include <opendht/crypto.h>
#include <msgpack.hpp>
#include <json/json.h>
#include <restbed>

/* for visual studio */
#include <ciso646>
#include <sstream>
#include <regex>
#include <fstream>

namespace jami {

constexpr const char* const QUERY_NAME {"/name/"};
constexpr const char* const QUERY_ADDR {"/addr/"};
constexpr const char* const HTTPS_PROTO {"https://"};
constexpr const char* const CACHE_DIRECTORY {"namecache"};
const std::string  HEX_PREFIX = "0x";

/** Parser for URIs.         ( protocol        )    ( username         ) ( hostname                            ) */
const std::regex URI_VALIDATOR {"^([a-zA-Z]+:(?://)?)?(?:([a-z0-9-_]{1,64})@)?([a-zA-Z0-9\\-._~%!$&'()*+,;=:\\[\\]]+)"};
const std::regex NAME_VALIDATOR {"^[a-zA-Z0-9-_]{3,32}$"};

constexpr size_t MAX_RESPONSE_SIZE {1024 * 1024};

void toLower(std::string& string)
{
    std::transform(string.begin(), string.end(), string.begin(), ::tolower);
}

void
NameDirectory::lookupUri(const std::string& uri, const std::string& default_server, LookupCallback cb)
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

NameDirectory::NameDirectory(const std::string& s)
   : serverHost_(s),
     cachePath_(fileutils::get_cache_dir()+DIR_SEPARATOR_STR+CACHE_DIRECTORY+DIR_SEPARATOR_STR+serverHost_)
{}

void
NameDirectory::load()
{
    loadCache();
}

NameDirectory& NameDirectory::instance(const std::string& server)
{
    const std::string& s = server.empty() ? DEFAULT_SERVER_HOST : server;
    static std::mutex instanceMtx {};

    std::lock_guard<std::mutex> lock(instanceMtx);
    static std::map<std::string, NameDirectory> instances {};
    auto r = instances.emplace(std::piecewise_construct,
                      std::forward_as_tuple(s),
                      std::forward_as_tuple(s));
    if (r.second)
        r.first->second.load();
    return r.first->second;
}

size_t getContentLength(restbed::Response& reply)
{
    size_t length = 0;
#ifdef RESTBED_OLD_API
    reply.get_header("Content-Length", length);
#else
    length = reply.get_header("Content-Length", 0);
#endif
    return length;
}

void NameDirectory::lookupAddress(const std::string& addr, LookupCallback cb)
{
    try {
        std::string cacheResult = nameCache(addr);
        if (not cacheResult.empty()) {
            cb(cacheResult, Response::found);
            return;
        }

        restbed::Uri uri(HTTPS_PROTO + serverHost_ + QUERY_ADDR + addr);
        auto req = std::make_shared<restbed::Request>(uri);
        req->set_header("Accept", "*/*");
        req->set_header("Host", serverHost_);

        JAMI_DBG("Address lookup for %s: %s", addr.c_str(), uri.to_string().c_str());

        auto ret = restbed::Http::async(req, [this,cb,addr](const std::shared_ptr<restbed::Request>&,
                                                 const std::shared_ptr<restbed::Response>& reply) {
            auto code = reply->get_status_code();
            if (code == 200) {
                size_t length = getContentLength(*reply);
                if (length > MAX_RESPONSE_SIZE) {
                    cb("", Response::error);
                    return;
                }
                restbed::Http::fetch(length, reply);
                std::string body;
                reply->get_body(body);

                Json::Value json;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(&body[0], &body[body.size()], &json, nullptr)) {
                    JAMI_ERR("Address lookup for %s: can't parse server response: %s", addr.c_str(), body.c_str());
                    cb("", Response::error);
                    return;
                }
                auto name = json["name"].asString();
                if (not name.empty()) {
                    JAMI_DBG("Found name for %s: %s", addr.c_str(), name.c_str());
                    {
                        std::lock_guard<std::mutex> l(lock_);
                        addrCache_.emplace(name, addr);
                        nameCache_.emplace(addr, name);
                    }
                    cb(name, Response::found);
                    saveCache();
                } else {
                    cb("", Response::notFound);
                }
            } else if (code >= 400 && code < 500) {
                cb("", Response::notFound);
            } else {
                cb("", Response::error);
            }
        }).share();

        // avoid blocking on future destruction
        ThreadPool::instance().run([ret](){ ret.get(); });
    } catch (const std::exception& e) {
        JAMI_ERR("Error when performing address lookup: %s", e.what());
        cb("", Response::error);
    }
}

bool
NameDirectory::verify(const std::string& name, const dht::crypto::PublicKey& pk, const std::string& signature)
{
    return pk.checkSignature(std::vector<uint8_t>(name.begin(), name.end()), base64::decode(signature));
}

void NameDirectory::lookupName(const std::string& n, LookupCallback cb)
{
    try {
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

        restbed::Uri uri(HTTPS_PROTO + serverHost_ + QUERY_NAME + name);
        JAMI_DBG("Name lookup for %s: %s", name.c_str(), uri.to_string().c_str());

        auto request = std::make_shared<restbed::Request>(std::move(uri));
        request->set_header("Accept", "*/*");
        request->set_header("Host", serverHost_);

        auto ret = restbed::Http::async(request, [this,cb,name](const std::shared_ptr<restbed::Request>&,
                                                     const std::shared_ptr<restbed::Response>& reply) {
            auto code = reply->get_status_code();
            if (code != 200)
                JAMI_DBG("Name lookup for %s: got reply code %d", name.c_str(), code);
            if (code >= 200 && code < 300) {
                size_t length = getContentLength(*reply);
                if (length > MAX_RESPONSE_SIZE) {
                    cb("", Response::error);
                    return;
                }
                restbed::Http::fetch(length, reply);
                std::string body;
                reply->get_body(body);

                Json::Value json;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(&body[0], &body[body.size()], &json, nullptr)) {
                    JAMI_ERR("Name lookup for %s: can't parse server response: %s", name.c_str(), body.c_str());
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
                        if(pk.getId().toString() != addr or not verify(name, pk, signature)) {
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
                    std::lock_guard<std::mutex> l(lock_);
                    addrCache_.emplace(name, addr);
                    nameCache_.emplace(addr, name);
                }
                cb(addr, Response::found);
                saveCache();
            } else if (code >= 400 && code < 500) {
                cb("", Response::notFound);
            } else {
                cb("", Response::error);
            }
        }).share();

        // avoid blocking on future destruction
        ThreadPool::instance().run([ret](){ ret.get(); });
    } catch (const std::exception& e) {
        JAMI_ERR("Error when performing name lookup: %s", e.what());
        cb("", Response::error);
    }
}

bool NameDirectory::validateName(const std::string& name) const
{
    return std::regex_match(name, NAME_VALIDATOR);
}
using Blob = std::vector<uint8_t>;
void NameDirectory::registerName(const std::string& addr, const std::string& n, const std::string& owner, RegistrationCallback cb, const std::string& signedname, const std::string& publickey)
{
    try {
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

        auto request = std::make_shared<restbed::Request>(restbed::Uri(HTTPS_PROTO + serverHost_ + QUERY_NAME + name));
        request->set_header("Accept", "*/*");
        request->set_header("Host", serverHost_);
        request->set_header("Content-Type", "application/json");
        request->set_method("POST");
        std::string body;
        {
            std::stringstream ss;
            ss << "{\"addr\":\"" << addr << "\",\"owner\":\"" << owner <<
                "\",\"signature\":\"" << signedname << "\",\"publickey\":\"" << base64::encode(jami::Blob(publickey.begin(), publickey.end()))  << "\"}";

            body = ss.str();
        }
        request->set_body(body);
        request->set_header("Content-Length", jami::to_string(body.size()));

        auto params = std::make_shared<restbed::Settings>();
        params->set_connection_timeout(std::chrono::seconds(120));

        JAMI_WARN("registerName: sending request %s %s", addr.c_str(), name.c_str());
        auto ret = restbed::Http::async(request,
         [this,cb,addr,name](const std::shared_ptr<restbed::Request>&,
             const std::shared_ptr<restbed::Response>& reply)
         {
            auto code = reply->get_status_code();
            JAMI_DBG("Got reply for registration of %s -> %s: code %d", name.c_str(), addr.c_str(), code);
            if (code >= 200 && code < 300) {
                size_t length = getContentLength(*reply);
                if (length > MAX_RESPONSE_SIZE) {
                    cb(RegistrationResponse::error);
                    return;
                }
                restbed::Http::fetch(length, reply);
                std::string body;
                reply->get_body(body);

                Json::Value json;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(&body[0], &body[body.size()], &json, nullptr)) {
                    cb(RegistrationResponse::error);
                    return;
                }
                auto success = json["success"].asBool();
                JAMI_DBG("Got reply for registration of %s -> %s: %s", name.c_str(), addr.c_str(), success ? "success" : "failure");
                if (success) {
                    std::lock_guard<std::mutex> l(lock_);
                    addrCache_.emplace(name, addr);
                    nameCache_.emplace(addr, name);
                }
                cb(success ? RegistrationResponse::success : RegistrationResponse::error);
            } else if(code == 400){
                cb(RegistrationResponse::incompleteRequest);
                JAMI_ERR("RegistrationResponse::incompleteRequest");
            } else if(code == 401){
                cb(RegistrationResponse::signatureVerificationFailed);
                JAMI_ERR("RegistrationResponse::signatureVerificationFailed");
            } else if (code == 403) {
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else if (code == 409) {
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else if (code > 400 && code < 500) {
                cb(RegistrationResponse::alreadyTaken);
                JAMI_ERR("RegistrationResponse::alreadyTaken");
            } else {
                cb(RegistrationResponse::error);
                JAMI_ERR("RegistrationResponse::error");
            }
        }, params).share();

    // avoid blocking on future destruction
        ThreadPool::instance().run([ret](){ ret.get(); });
    } catch (const std::exception& e) {
        JAMI_ERR("Error when performing name registration: %s", e.what());
        cb(RegistrationResponse::error);
    }
}

void
NameDirectory::saveCache()
{
    fileutils::recursive_mkdir(fileutils::get_cache_dir()+DIR_SEPARATOR_STR+CACHE_DIRECTORY);
    std::lock_guard<std::mutex> lock(fileutils::getFileLock(cachePath_));
    std::ofstream file(cachePath_, std::ios::trunc | std::ios::binary);
    {
        std::lock_guard<std::mutex> l(lock_);
        msgpack::pack(file, nameCache_);
    }
    JAMI_DBG("Saved %lu name-address mappings", (long unsigned)nameCache_.size());
}

void
NameDirectory::loadCache()
{
    msgpack::unpacker pac;

    // read file
    {
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(cachePath_));
        std::ifstream file(cachePath_);
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
    std::lock_guard<std::mutex> l(lock_);
    msgpack::object_handle oh;
    if (pac.next(oh))
        oh.get().convert(nameCache_);
    for (const auto& m : nameCache_)
        addrCache_.emplace(m.second, m.first);
    JAMI_DBG("Loaded %lu name-address mappings", (long unsigned)nameCache_.size());
}

}
