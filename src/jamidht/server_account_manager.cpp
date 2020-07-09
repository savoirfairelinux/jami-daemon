/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *           Vsevolod Ivanov <vsevolod.ivanov@savoirfairelinux.com>
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
#include "base64.h"
#include "dring/account_const.h"

#include <opendht/http.h>
#include <opendht/log.h>
#include <opendht/thread_pool.h>

#include "manager.h"

#include <algorithm>

namespace jami {

using Request = dht::http::Request;

static const std::string PATH_LOGIN = "/api/login";
static const std::string PATH_AUTH =  "/api/auth";
static const std::string PATH_DEVICE = PATH_AUTH + "/device";
static const std::string PATH_DEVICES = PATH_AUTH + "/devices";
static const std::string PATH_SEARCH = PATH_AUTH + "/directory/search";

ServerAccountManager::ServerAccountManager(
    const std::string& path,
    OnAsync&& onAsync,
    const std::string& managerHostname,
    const std::string& nameServer)
: AccountManager(path, std::move(onAsync), nameServer)
, managerHostname_(managerHostname)
, logger_(std::make_shared<dht::Logger>(
    [](char const* m, va_list args) { Logger::vlog(LOG_ERR, nullptr, 0, true, m, args); },
    [](char const* m, va_list args) { Logger::vlog(LOG_WARNING, nullptr, 0, true, m, args); },
    [](char const* m, va_list args) { Logger::vlog(LOG_DEBUG, nullptr, 0, true, m, args); }))
{};

void
ServerAccountManager::setAuthHeaderFields(Request& request) const {
    request.set_header_field(restinio::http_field_t::authorization, "Bearer " + token_);
}

void
ServerAccountManager::initAuthentication(
    PrivateKey key,
    std::string deviceName,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    OnChangeCallback onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->key = key;
    ctx->request = buildRequest(key);
    ctx->deviceName = std::move(deviceName);
    ctx->credentials = dynamic_unique_cast<ServerAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);
    if (not ctx->credentials or ctx->credentials->username.empty()) {
        ctx->onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    onChange_ = std::move(onChange);
    const std::string url = managerHostname_ + PATH_DEVICE;
    JAMI_WARN("[Auth] authentication with: %s to %s", ctx->credentials->username.c_str(), url.c_str());

    dht::ThreadPool::computation().run([onAsync = onAsync_, ctx, url]{
        onAsync([=](AccountManager& accountManager){
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            Json::Value body;
            {
                std::stringstream ss;
                auto csr = ctx->request.get()->toString();
                body["csr"] = csr;
                body["deviceName"] = ctx->deviceName;
            }
            auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, body, [ctx, onAsync]
                                                (Json::Value json, const dht::http::Response& response){
                JAMI_DBG("[Auth] Got request callback with status code=%u", response.status_code);
                if (response.status_code == 0)
                    ctx->onFailure(AuthError::SERVER_ERROR, "Can't connect to server");
                else if (response.status_code >= 400 && response.status_code < 500)
                    ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
                else if (response.status_code < 200 || response.status_code > 299)
                    ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
                else {
                    do {
                        try {
                            JAMI_WARN("[Auth] Got server response: %s", response.body.c_str());
                            auto cert = std::make_shared<dht::crypto::Certificate>(json["certificateChain"].asString());
                            auto accountCert = cert->issuer;
                            if (not accountCert) {
                                JAMI_ERR("[Auth] Can't parse certificate: no issuer");
                                ctx->onFailure(AuthError::SERVER_ERROR, "Invalid certificate from server");
                                break;
                            }
                            auto receipt = json["deviceReceipt"].asString();
                            Json::Value receiptJson;
                            std::string err;
                            auto receiptReader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder{}.newCharReader());
                            if (!receiptReader->parse(receipt.data(), receipt.data() + receipt.size(), &receiptJson, &err)){
                                JAMI_ERR("[Auth] Can't parse receipt from server: %s", err.c_str());
                                ctx->onFailure(AuthError::SERVER_ERROR, "Can't parse receipt from server");
                                break;
                            }
                            onAsync([=] (AccountManager& accountManager) mutable
                            {
                                auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
                                auto receiptSignature = base64::decode(json["receiptSignature"].asString());

                                auto info = std::make_unique<AccountInfo>();
                                info->identity.first = ctx->key.get();
                                info->identity.second = cert;
                                info->deviceId = cert->getPublicKey().getId().toString();
                                info->accountId = accountCert->getId().toString();
                                info->contacts = std::make_unique<ContactList>(accountCert, this_.path_, this_.onChange_);
                                //info->contacts->setContacts(a.contacts);
                                if (ctx->deviceName.empty())
                                    ctx->deviceName = info->deviceId.substr(8);
                                info->contacts->foundAccountDevice(cert, ctx->deviceName, clock::now());
                                info->ethAccount = receiptJson["eth"].asString();
                                info->announce = parseAnnounce(receiptJson["announce"].asString(), info->accountId, info->deviceId);
                                if (not info->announce) {
                                    ctx->onFailure(AuthError::SERVER_ERROR, "Can't parse announce from server");
                                }
                                info->username = ctx->credentials->username;

                                this_.creds_ = std::move(ctx->credentials);
                                this_.info_ = std::move(info);
                                std::map<std::string, std::string> config;
                                if (json.isMember("nameServer")) {
                                    auto nameServer = json["nameServer"].asString();
                                    if (!nameServer.empty() && nameServer[0] == '/')
                                        nameServer = this_.managerHostname_ + nameServer;
                                    this_.nameDir_ = NameDirectory::instance(nameServer);
                                    config.emplace(DRing::Account::ConfProperties::RingNS::URI, std::move(nameServer));
                                }
                                if (json.isMember("displayName")) {
                                    config.emplace(DRing::Account::ConfProperties::DISPLAYNAME, json["displayName"].asString());
                                }
                                if (json.isMember("userPhoto")) {
                                    this_.info_->photo = json["userPhoto"].asString();
                                }

                                ctx->onSuccess(*this_.info_, std::move(config), std::move(receipt), std::move(receiptSignature));
                                this_.syncDevices();
                            });
                        }
                        catch (const std::exception& e) {
                            JAMI_ERR("Error when loading account: %s", e.what());
                            ctx->onFailure(AuthError::NETWORK, "");
                        }
                    } while (false);
                }

                onAsync([response](AccountManager& accountManager){
                    auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
                    this_.clearRequest(response.request);
                });
            }, this_.logger_);
            request->set_auth(ctx->credentials->username, ctx->credentials->password);
            this_.sendRequest(request);
        });
    });
}

void
ServerAccountManager::authenticateDevice() {
    const std::string url = managerHostname_ + PATH_LOGIN;
    JAMI_WARN("[Auth] getting a device token: %s", url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, Json::Value{Json::objectValue}, [onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([=] (AccountManager& accountManager) {
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200 && response.status_code < 300) {
                auto scopeStr = json["scope"].asString();
                auto scope = scopeStr == "DEVICE" ? TokenScope::Device
                          : (scopeStr == "USER"   ? TokenScope::User
                                                  : TokenScope::None);
                auto expires_in = json["expires_in"].asLargestUInt();
                auto expiration = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in);
                JAMI_WARN("[Auth] Got server response: %d %s", response.status_code, response.body.c_str());
                this_.setToken(json["access_token"].asString(), scope, expiration);
            } else {
                this_.authFailed(TokenScope::Device, response.status_code);
            }
            this_.clearRequest(response.request);
        });
    }, logger_);
    request->set_identity(info_->identity);
    // request->set_certificate_authority(info_->identity.second->issuer->issuer);
    sendRequest(request);
}

void
ServerAccountManager::sendRequest(const std::shared_ptr<dht::http::Request>& request) {
    request->set_header_field(restinio::http_field_t::user_agent, "Jami");
    {
        std::lock_guard<std::mutex> lock(requestLock_);
        requests_.emplace(request);
    }
    request->send();
}

void
ServerAccountManager::clearRequest(const std::weak_ptr<dht::http::Request>& request) {
    if (auto req = request.lock()) {
        std::lock_guard<std::mutex> lock(requestLock_);
        requests_.erase(req);
    }
}

void
ServerAccountManager::authFailed(TokenScope scope, int code)
{
    RequestQueue requests;
    {
        std::lock_guard<std::mutex> lock(tokenLock_);
        requests = std::move(getRequestQueue(scope));
    }
    JAMI_DBG("[Auth] Failed auth with scope %d, ending %zu pending requests", (int)scope, requests.size());
    while (not requests.empty()) {
        auto req = std::move(requests.front());
        requests.pop();
        req->terminate(code == 0 ? asio::error::not_connected : asio::error::access_denied);
    }
}

void
ServerAccountManager::authError(TokenScope scope) {
    {
        std::lock_guard<std::mutex> lock(tokenLock_);
        if (scope >= tokenScope_) {
            token_ = {};
            tokenScope_ = TokenScope::None;
        }
    }
    if (scope == TokenScope::Device)
        authenticateDevice();
}

void
ServerAccountManager::setToken(std::string token, TokenScope scope, std::chrono::steady_clock::time_point expiration)
{
    std::lock_guard<std::mutex> lock(tokenLock_);
    token_ = std::move(token);
    tokenScope_ = scope;
    tokenExpire_ = expiration;

    nameDir_.get().setToken(token_);
    if (not token_.empty()) {
        auto& reqQueue = getRequestQueue(scope);
        JAMI_DBG("[Auth] Got token with scope %d, handling %zu pending requests", (int)scope, reqQueue.size());
        while (not reqQueue.empty()) {
            auto req = std::move(reqQueue.front());
            reqQueue.pop();
            setAuthHeaderFields(*req);
            sendRequest(req);
        }
    }
}

void ServerAccountManager::sendDeviceRequest(const std::shared_ptr<dht::http::Request>& req)
{
    std::lock_guard<std::mutex> lock(tokenLock_);
    if (hasAuthorization(TokenScope::Device)) {
        setAuthHeaderFields(*req);
        sendRequest(req);
    } else {
        auto& rQueue = getRequestQueue(TokenScope::Device);
        if (rQueue.empty())
            authenticateDevice();
        rQueue.emplace(req);
    }
}

void ServerAccountManager::sendAccountRequest(const std::shared_ptr<dht::http::Request>& req)
{
    std::lock_guard<std::mutex> lock(tokenLock_);
    if (hasAuthorization(TokenScope::User)) {
        setAuthHeaderFields(*req);
        sendRequest(req);
    } else {
        pendingAccountRequests_.emplace(req);
    }
}

void
ServerAccountManager::syncDevices()
{
    const std::string url = managerHostname_ + PATH_DEVICES;
    JAMI_WARN("[Auth] syncDevices %s", url.c_str());
    sendDeviceRequest(std::make_shared<Request>(*Manager::instance().ioContext(), url, [onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([=] (AccountManager& accountManager) {
            JAMI_DBG("[Auth] Got request callback with status code=%u", response.status_code);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARN("[Auth] Got server response: %s", response.body.c_str());
                    if (not json.isArray()) {
                        JAMI_ERR("[Auth] Can't parse server response: not an array");
                        return;
                    }
                    for (unsigned i=0, n=json.size(); i<n; i++) {
                        const auto& e = json[i];
                        dht::InfoHash deviceId(e["deviceId"].asString());
                        if (deviceId) {
                            this_.info_->contacts->foundAccountDevice(deviceId, e["alias"].asString(), clock::now());
                        }
                    }
                }
                catch (const std::exception& e) {
                    JAMI_ERR("Error when loading device list: %s", e.what());
                }
            } else if (response.status_code == 401)
                this_.authError(TokenScope::Device);

            this_.clearRequest(response.request);
        });
    }, logger_));
}

bool
ServerAccountManager::revokeDevice(const std::string& password, const std::string& device, RevokeDeviceCallback cb)
{
    if (not info_){
        if (cb)
            cb(RevokeDeviceResult::ERROR_CREDENTIALS);
        return false;
    }
    const std::string url = managerHostname_ + PATH_DEVICE + "?deviceId=" + device;
    JAMI_WARN("[Revoke] Removing device of %s at %s", info_->username.c_str(), url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, [cb, onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([=] (AccountManager& accountManager) {
            JAMI_DBG("[Revoke] Got request callback with status code=%u", response.status_code);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARN("[Revoke] Got server response");
                    if (json["errorDetails"].empty()){
                        if (cb)
                            cb(RevokeDeviceResult::SUCCESS);
                        this_.syncDevices();
                    }
                }
                catch (const std::exception& e) {
                    JAMI_ERR("Error when loading device list: %s", e.what());
                }
            }
            else if (cb)
                cb(RevokeDeviceResult::ERROR_NETWORK);
            this_.clearRequest(response.request);
        });
    }, logger_);
    request->set_method(restinio::http_method_delete());
    request->set_auth(info_->username, password);
    JAMI_DBG("[Revoke] Sending revoke device '%s' to JAMS", device.c_str());
    sendRequest(request);
    return false;
}

void
ServerAccountManager::registerName(const std::string&, const std::string&, RegistrationCallback cb)
{
    cb(NameDirectory::RegistrationResponse::unsupported);
}

bool
ServerAccountManager::searchUser(const std::string& query, SearchCallback cb)
{
    //TODO escape url query
    const std::string url = managerHostname_ + PATH_SEARCH + "?queryString=" + query;
    JAMI_WARN("[Search] Searching user %s at %s", query.c_str(), url.c_str());
    sendDeviceRequest(std::make_shared<Request>(*Manager::instance().ioContext(), url, [cb, onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([=] (AccountManager& accountManager) {
            JAMI_DBG("[Search] Got request callback with status code=%u", response.status_code);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200  && response.status_code < 300) {
                try {
                    Json::Value::ArrayIndex rcount = json.size();
                    std::vector<std::map<std::string, std::string>> results;
                    results.reserve(rcount);
                    JAMI_WARN("[Search] Got server response: %s", response.body.c_str());
                    for (Json::Value::ArrayIndex i=0; i<rcount; i++) {
                        const auto& ruser = json[i];
                        std::map<std::string, std::string> user;
                        for (const auto& member : ruser.getMemberNames()) {
                            user[member] = ruser[member].asString();
                        }
                        results.emplace_back(std::move(user));
                    }
                    if (cb)
                        cb(results, SearchResponse::found);
                }
                catch (const std::exception& e) {
                    JAMI_ERR("[Search] Error during search: %s", e.what());
                }
            } else {
                if (response.status_code == 401)
                    this_.authError(TokenScope::Device);
                if (cb)
                    cb({}, SearchResponse::error);
            }
            this_.clearRequest(response.request);
        });
    }, logger_));
    return true;
}

}
