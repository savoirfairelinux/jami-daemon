/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
#include "namedirectory.h"

#include "logger.h"

#include <json/json.h>
#include <restbed>

/* for visual studio */
#include <ciso646>

namespace ring {

constexpr const char* const QUERY_NAME {"/name/"};
constexpr const char* const QUERY_ADDR {"/addr/"};

NameDirectory& NameDirectory::instance()
{
   static auto instance = new NameDirectory;
   return *instance;
}

void NameDirectory::addrLookup(const std::string& addr, LookupCallback cb)
{
    RING_DBG("Address lookup for %s", addr.c_str());
    auto cacheRes = nameCache_.find(addr);
    if (cacheRes != nameCache_.end()) {
        cb(cacheRes->second, NameDirectory::Response::found);
        return;
    }

    auto req = std::make_shared<restbed::Request>(restbed::Uri("http://" + server_ + QUERY_ADDR + addr));
    req->set_header("Accept", "*/*");
    req->set_header("Host", server_);

    restbed::Http::async(req, [this,cb,addr](const std::shared_ptr<restbed::Request>,
                                             const std::shared_ptr<restbed::Response> reply) {
        if (reply->get_status_code() == 200) {
            size_t length = 0;
            length = reply->get_header("Content-Length", length);
            restbed::Http::fetch(length, reply);
            std::string body;
            reply->get_body(body);

            Json::Value json;
            Json::Reader reader;
            if (!reader.parse(body, json)) {
                RING_ERR("Address lookup for %s: can't parse server response: %s", addr.c_str(), body.c_str());
                cb("", NameDirectory::Response::notFound);
                return;
            }
            auto name = json["name"].asString();
            if (not name.empty()) {
                RING_DBG("Found name for %s: %s", addr.c_str(), name.c_str());
                nameCache_.emplace(addr, name);
                cb(name, NameDirectory::Response::found);
            } else {
                cb("", NameDirectory::Response::notFound);
            }
        } else {
            cb("", NameDirectory::Response::error);
        }
    });
}

void NameDirectory::nameLookup(const std::string& name, LookupCallback cb)
{
    auto cacheRes = addrCache_.find(name);
    if (cacheRes != addrCache_.end()) {
        cb(cacheRes->second, NameDirectory::Response::found);
        return;
    }

    auto request = std::make_shared<restbed::Request>(restbed::Uri("http://" + server_ + QUERY_NAME + name));
    request->set_header("Accept", "*/*");
    request->set_header("Host", server_);

    restbed::Http::async(request, [this,cb,name](const std::shared_ptr<restbed::Request>,
                                                 const std::shared_ptr<restbed::Response> reply) {
        auto code = reply->get_status_code();
        if (code >= 200 && code < 300) {
            size_t length = 0;
            length = reply->get_header("Content-Length", length);
            restbed::Http::fetch(length, reply);
            std::string body;
            reply->get_body(body);

            Json::Value json;
            Json::Reader reader;
            if (!reader.parse(body, json)) {
                cb("", NameDirectory::Response::notFound);
                return;
            }
            auto addr = json["addr"].asString();
            if (not name.empty()) {
                RING_DBG("Found address for %s: %s", name.c_str(), addr.c_str());
                addrCache_.emplace(name, addr);
                cb(addr, NameDirectory::Response::found);
            }
        } else if (code >= 400 && code < 500) {
            cb("", NameDirectory::Response::notFound);
        } else {
            std::string error;
            reply->get_body(error);
            cb(error, NameDirectory::Response::error);
        }
    });
}

void NameDirectory::registerName(const std::string& addr, const std::string& name, RegistrationCallback cb)
{
    auto cacheRes = addrCache_.find(name);
    if (cacheRes != addrCache_.end()) {
        if (cacheRes->second == addr)
            cb(RegistrationResponse::success);
        else
            cb(RegistrationResponse::alreadyTaken);
        return;
    }

    auto request = std::make_shared<restbed::Request>(restbed::Uri("http://" + server_ + QUERY_NAME + name));
    request->set_header("Accept", "*/*");
    request->set_header("Host", server_);
    request->set_header("Content-Type", "application/json");
    request->set_method("POST");

    std::stringstream ss;
    ss << "{\"address\":\"" << addr << "\",\"owner\":\"\"}";
    request->set_body(ss.str());

    restbed::Http::async(request, [this,cb,addr,name](const std::shared_ptr<restbed::Request>,
                                                 const std::shared_ptr<restbed::Response> reply) {
        auto code = reply->get_status_code();
        if (code >= 200 && code < 300) {
            size_t length = 0;
            length = reply->get_header("Content-Length", length);
            restbed::Http::fetch(length, reply);
            std::string body;
            reply->get_body(body);

            Json::Value json;
            Json::Reader reader;
            if (!reader.parse(body, json)) {
                cb(RegistrationResponse::error);
                return;
            }
            auto success = json["success"].asBool();
            RING_DBG("Got reply for registration of %s -> %s: %s", name.c_str(), addr.c_str(), success ? "success" : "failure");
            if (success) {
                addrCache_.emplace(name, addr);
                nameCache_.emplace(addr, name);
            }
            cb(success ? RegistrationResponse::success : RegistrationResponse::error);
        } else if (code >= 400 && code < 500) {
            cb(RegistrationResponse::alreadyTaken);
        } else {
            std::string error;
            reply->get_body(error);
            cb(RegistrationResponse::error);
        }
    });
}

}
