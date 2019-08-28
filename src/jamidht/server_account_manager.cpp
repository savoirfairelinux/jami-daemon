/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "server_account_manager.h"

#include <opendht/http.h>

namespace jami {

static const std::string PATH_AUTH = "/api/auth/device/register";

void
ServerAccountManager::initAuthentication(
    CertRequest csrRequest,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    OnChangeCallback onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->request = std::move(request);
    ctx->credentials = dynamic_unique_cast<ArchiveAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);

    auto request = std::make_shared<Request>(httpContext_, resolver_, logger_);
    auto reqid = request->id();
    request->set_connection_type(restinio::http_connection_header_t::keep_alive);
    request->set_target(PATH_AUTH);
    request->set_method(restinio::http_method_post());
    std::string body;
    {
        std::stringstream ss;
        ss << "{\"csrRequest\":\"" << csrRequest.toString()  << "\"}";
        body = ss.str();
    }

    const std::string host = std::string(HTTPS_PROTO) + ":" + serverHost_;
    request->set_header_field(restinio::http_field_t::host, host);
    request->set_header_field(restinio::http_field_t::user_agent, "Jami");
    request->set_header_field(restinio::http_field_t::accept, "application/json");
    request->set_header_field(restinio::http_field_t::content_type, "application/json");

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

}

}
