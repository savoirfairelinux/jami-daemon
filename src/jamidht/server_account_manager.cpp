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
ServerAccountManager::setHeaderFields(Request& request){
    request.set_header_field(restinio::http_field_t::user_agent, "Jami");
}

void
ServerAccountManager::setDeviceAuthHeaderFields(Request& request) {
    request.set_header_field(restinio::http_field_t::authorization, "Bearer " + deviceToken_);
}

void
ServerAccountManager::initAuthentication(
    CertRequest csrRequest,
    std::string deviceName,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    OnChangeCallback onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->request = std::move(csrRequest);
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
        onAsync([&](AccountManager& accountManager){
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            Json::Value body;
            {
                std::stringstream ss;
                auto csr = ctx->request.get()->toString();
                /*string_replace(csr, "\n", "");
                string_replace(csr, "\r", "");*/
                body["csr"] = csr;
                body["deviceName"] = ctx->deviceName;
                /*ss << "{\"csr\":\"" << csr  << "\", \"deviceName\":\"" << ctx->deviceName  << "\"}";
                JAMI_WARN("[Auth] Sending request: %s", csr.c_str());
                request->set_body(ss.str());*/
            }
            auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, body, [ctx, onAsync]
                                                (Json::Value json, const dht::http::Response& response){
                JAMI_DBG("[Auth] Got request callback with status code=%u", response.status_code);
                if (response.status_code == 0)
                    ctx->onFailure(AuthError::SERVER_ERROR, "Can't connect to server");
                else if (response.status_code >= 400 && response.status_code < 500)
                    ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
                else if (response.status_code < 200 && response.status_code > 299)
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
                            onAsync([&] (AccountManager& accountManager) mutable
                            {
                                auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
                                auto receiptSignature = base64::decode(json["receiptSignature"].asString());

                                auto info = std::make_unique<AccountInfo>();
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
                                if (json.isMember("token")) {
                                    this_.setDeviceToken(json["token"].asString());
                                }

                                ctx->onSuccess(*this_.info_, std::move(config), std::move(receipt), std::move(receiptSignature));
                                this_.syncDevices();
                                //this_.authenticateDevice();
                            });
                        }
                        catch (const std::exception& e) {
                            JAMI_ERR("Error when loading account: %s", e.what());
                            ctx->onFailure(AuthError::NETWORK, "");
                        }
                    } while (false);
                }

                onAsync([&](AccountManager& accountManager){
                    auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
                    if (auto req = response.request.lock())
                        this_.requests_.erase(req);
                });
            }, this_.logger_);
            request->set_auth(ctx->credentials->username, ctx->credentials->password);
            this_.setHeaderFields(*request);
            this_.requests_.emplace(request);
            request->send();
        });
    });
}

void
ServerAccountManager::authenticateDevice() {
    const std::string url = managerHostname_ + PATH_LOGIN;
    JAMI_WARN("[Auth] getting a device token: %s", url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, Json::Value{Json::objectValue}, [onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([&] (AccountManager& accountManager) {
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200 && response.status_code < 300) {
                JAMI_WARN("[Auth] Got server response: %d %s", response.status_code, response.body.c_str());
                this_.setDeviceToken(json["access_token"].asString());
            }
            if (auto req = response.request.lock())
                this_.requests_.erase(req);
        });
    }, logger_);
    request->set_identity(info_->identity);
    request->set_certificate_authority(info_->identity.second->issuer->issuer);
    setHeaderFields(*request);
    requests_.emplace(request);
    request->send();
}

void
ServerAccountManager::setDeviceToken(std::string token) {
    deviceToken_ = std::move(token);
    nameDir_.get().setToken(deviceToken_);
    if (not deviceToken_.empty()) {
        JAMI_WARN("[Auth] Got device token, handling: %zu pending requests", pendingDeviceRequests_.size());
        while (not pendingDeviceRequests_.empty()) {
            auto req = std::move(pendingDeviceRequests_.front());
            pendingDeviceRequests_.pop();
            setDeviceAuthHeaderFields(*req);
            requests_.emplace(req);
            req->send();
        }
    }
}

void
ServerAccountManager::setAccountToken(std::string token) {
    accountToken_ = std::move(token);
    if (not deviceToken_.empty()) {
        while (not pendingAccountRequests_.empty()) {
            JAMI_WARN("[Auth] Got account token, handling: %zu pending requests", pendingAccountRequests_.size());
            auto req = std::move(pendingAccountRequests_.front());
            pendingAccountRequests_.pop();
            //setAccountAuthHeaderFields(*req);
            requests_.emplace(req);
            req->send();
        }
    }
}

void ServerAccountManager::sendDeviceRequest(const std::shared_ptr<dht::http::Request>& req)
{
    if (not deviceToken_.empty()) {
        setDeviceAuthHeaderFields(*req);
        requests_.emplace(req);
        req->send();
    } else {
        if (pendingDeviceRequests_.empty())
            authenticateDevice();
        pendingDeviceRequests_.emplace(req);
    }
}

void ServerAccountManager::sendAccountRequest(const std::shared_ptr<dht::http::Request>& req)
{
    if (not accountToken_.empty()) {
        requests_.emplace(req);
        req->send();
    } else {
        pendingAccountRequests_.emplace(req);
    }
}

void
ServerAccountManager::syncDevices()
{
    const std::string url = managerHostname_ + PATH_DEVICES;
    JAMI_WARN("[Auth] syncDevices %s", url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, [onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([&] (AccountManager& accountManager) {
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
            }
            if (auto req = response.request.lock())
                this_.requests_.erase(req);
        });
    }, logger_);
    setHeaderFields(*request);
    sendDeviceRequest(request);
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
        onAsync([&] (AccountManager& accountManager) {
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
            if (auto req = response.request.lock())
                this_.requests_.erase(req);
        });
    }, logger_);
    request->set_method(restinio::http_method_delete());
    request->set_auth(info_->username, password);
    setHeaderFields(*request);
    JAMI_DBG("[Revoke] Sending revoke device '%s' to JAMS", device.c_str());
    requests_.emplace(request);
    request->send();
    return false;
}

void
ServerAccountManager::registerName(const std::string&, const std::string&, RegistrationCallback cb)
{
    cb(NameDirectory::RegistrationResponse::unsupported);
}

bool
ServerAccountManager::searchName(const std::string& query, SearchCallback cb)
{
    if (not info_){
        if (cb)
            cb({}, SearchResponse::error);
        return false;
    }
    //TODO escape url query
    const std::string url = managerHostname_ + PATH_SEARCH + "?queryString=" + query;
    JAMI_WARN("[Search] Searching user %s at %s", query.c_str(), url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, [cb, onAsync = onAsync_] (Json::Value json, const dht::http::Response& response){
        onAsync([&] (AccountManager& accountManager) {
            JAMI_DBG("[Search] Got request callback with status code=%u", response.status_code);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (response.status_code >= 200  && response.status_code < 300) {
                try {
                    JAMI_WARN("[Search] Got server response: %s", response.body.c_str());
                    if (cb)
                        cb({}, SearchResponse::found);
                }
                catch (const std::exception& e) {
                    JAMI_ERR("[Search] Error during search: %s", e.what());
                }
            }
            else if (cb)
                cb({}, SearchResponse::error);
            if (auto req = response.request.lock())
                this_.requests_.erase(req);
        });
    }, logger_);
    setHeaderFields(*request);
    sendDeviceRequest(request);
    return true;
}

}
