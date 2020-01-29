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

#include "manager.h"

#include <algorithm>

namespace jami {

using Request = dht::http::Request;

static const std::string PATH_AUTH = "/api/auth";
static const std::string PATH_DEVICE = PATH_AUTH + "/device";

constexpr const char* const HTTPS_PROTO {"https"};

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
    request.set_header_field(restinio::http_field_t::accept, "application/json");
    request.set_header_field(restinio::http_field_t::content_type, "application/json");
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
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, logger_);
    auto reqid = request->id();
    request->set_method(restinio::http_method_post());
    request->set_auth(ctx->credentials->username, ctx->credentials->password);
    {
        std::stringstream ss;
        auto csr = ctx->request.get()->toString();
        string_replace(csr, "\n", "\\n");
        string_replace(csr, "\r", "\\r");
        ss << "{\"csr\":\"" << csr  << "\", \"deviceName\":\"" << ctx->deviceName  << "\"}";
        JAMI_WARN("[Auth] Sending request: %s", csr.c_str());
        request->set_body(ss.str());
    }
    setHeaderFields(*request);
    request->add_on_state_change_callback([reqid, ctx, onAsync = onAsync_]
                                          (Request::State state, const dht::http::Response& response){
        JAMI_DBG("[Auth] Got request callback with state=%d", (int)state);
        if (state != Request::State::DONE)
            return;
        if (response.status_code == 0) {
            ctx->onFailure(AuthError::SERVER_ERROR, "Invalid server provided");
            return;
        } else if (response.status_code >= 400 && response.status_code < 500)
            ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
        else if (response.status_code < 200 || response.status_code > 299)
            ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
        else {
            try {
                Json::Value json;
                std::string err;
                Json::CharReaderBuilder rbuilder;
                auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                    JAMI_ERR("[Auth] Can't parse server response: %s", err.c_str());
                    ctx->onFailure(AuthError::SERVER_ERROR, "Can't parse server response");
                    return;
                }
                JAMI_WARN("[Auth] Got server response: %s", response.body.c_str());

                auto certStr = json["certificateChain"].asString();
                string_replace(certStr, "\\n", "\n");
                string_replace(certStr, "\\r", "\r");
                auto cert = std::make_shared<dht::crypto::Certificate>(certStr);

                auto accountCert = cert->issuer;
                if (not accountCert) {
                    JAMI_ERR("[Auth] Can't parse certificate: no issuer");
                    ctx->onFailure(AuthError::SERVER_ERROR, "Invalid certificate from server");
                    return;
                }
                auto receipt = json["deviceReceipt"].asString();
                Json::Value receiptJson;
                auto receiptReader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                if (!receiptReader->parse(receipt.data(), receipt.data() + receipt.size(), &receiptJson, &err)){
                    JAMI_ERR("[Auth] Can't parse receipt from server: %s", err.c_str());
                    ctx->onFailure(AuthError::SERVER_ERROR, "Can't parse receipt from server");
                    return;
                }
                onAsync([reqid, ctx,
                            json=std::move(json),
                            receiptJson=std::move(receiptJson),
                            cert,
                            accountCert,
                            receipt=std::move(receipt)] (AccountManager& accountManager) mutable
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
                        const std::string photo = "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";
                        emitSignal<DRing::ConfigurationSignal::AvatarReceived>(this_.info_->accountId, photo);
                    }

                    ctx->onSuccess(*this_.info_, std::move(config), std::move(receipt), std::move(receiptSignature));
                    this_.syncDevices();
                    this_.requests_.erase(reqid);
                });
            }
            catch (const std::exception& e) {
                JAMI_ERR("Error when loading account: %s", e.what());
                ctx->onFailure(AuthError::NETWORK, "");
            }
        }
    });
    request->send();
    requests_[reqid] = std::move(request);
}

void
ServerAccountManager::syncDevices()
{
    if (not creds_)
        return;
    const std::string url = managerHostname_ + PATH_DEVICE + "?username=" + creds_->username;
    JAMI_WARN("[Auth] syncDevices with: %s to %s", creds_->username.c_str(), url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, logger_);
    auto reqid = request->id();
    request->set_method(restinio::http_method_get());
    request->set_auth(creds_->username, creds_->password);
    setHeaderFields(*request);
    request->add_on_state_change_callback([reqid, onAsync = onAsync_]
                                            (Request::State state, const dht::http::Response& response){
        onAsync([reqid, state, response] (AccountManager& accountManager) {
            JAMI_DBG("[Auth] Got request callback with state=%d", (int)state);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (state != Request::State::DONE)
                return;
            if (response.status_code >= 200 || response.status_code < 300) {
                try {
                    Json::Value json;
                    std::string err;
                    Json::CharReaderBuilder rbuilder;
                    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                    if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                        JAMI_ERR("[Auth] Can't parse server response: %s", err.c_str());
                        return;
                    }
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
            this_.requests_.erase(reqid);
        });
    });
    request->send();
    requests_[reqid] = std::move(request);
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
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, logger_);
    auto reqid = request->id();
    request->set_method(restinio::http_method_delete());
    request->set_auth(info_->username, password);
    setHeaderFields(*request);
    request->add_on_state_change_callback([reqid, cb, onAsync = onAsync_]
                                          (Request::State state, const dht::http::Response& response){
        onAsync([reqid, cb, state, response] (AccountManager& accountManager) {
            JAMI_DBG("[Revoke] Got request callback with state=%d", (int) state);
            auto& this_ = *static_cast<ServerAccountManager*>(&accountManager);
            if (state != Request::State::DONE)
                return;
            if (response.status_code >= 200 || response.status_code < 300) {
                if (response.body.empty())
                    return;
                try {
                    Json::Value json;
                    std::string err;
                    Json::CharReaderBuilder rbuilder;
                    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
                    if (!reader->parse(response.body.data(), response.body.data() + response.body.size(), &json, &err)){
                        JAMI_ERR("[Revoke] Can't parse server response: %s", err.c_str());
                    }
                    JAMI_WARN("[Revoke] Got server response: %s", response.body.c_str());
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
            this_.requests_.erase(reqid);
        });
    });
    JAMI_DBG("[Revoke] Sending revoke device '%s' to JAMS", device.c_str());
    request->send();
    requests_[reqid] = std::move(request);
    return false;
}

void
ServerAccountManager::registerName(const std::string& password, const std::string& name, RegistrationCallback cb)
{
    cb(NameDirectory::RegistrationResponse::unsupported);
}

}
