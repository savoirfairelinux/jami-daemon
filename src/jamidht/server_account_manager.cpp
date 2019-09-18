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
#include "base64.h"
#include "dring/account_const.h"

#include <opendht/http.h>
#include <opendht/log.h>

#include "manager.h"

#include <algorithm>

namespace jami {

using Request = dht::http::Request;

static const std::string PATH_AUTH = "/api/auth";
static const std::string PATH_DEVICE_REGISTER = PATH_AUTH + "/device/register";
static const std::string PATH_DEVICES = PATH_AUTH + "/devices";

constexpr const char* const HTTPS_PROTO {"https"};

ServerAccountManager::ServerAccountManager(
    const std::string& path,
    std::shared_ptr<dht::DhtRunner> dht,
    OnAsync&& onAsync,
    const std::string& managerHostname,
    const std::string& nameServer)
: AccountManager(path, std::move(onAsync), std::move(dht), nameServer)
, managerHostname_(managerHostname)
, logger_(std::make_shared<dht::Logger>(
    [](char const* m, va_list args) { Logger::vlog(LOG_ERR, nullptr, 0, true, m, args); }, 
    [](char const* m, va_list args) { Logger::vlog(LOG_WARNING, nullptr, 0, true, m, args); },
    [](char const* m, va_list args) { Logger::vlog(LOG_DEBUG, nullptr, 0, true, m, args); }))
{};

void
ServerAccountManager::setHeaderFields(Request& request){
    request.set_header_field(restinio::http_field_t::host, request.get_url().host + ":" + request.get_url().service);
    request.set_header_field(restinio::http_field_t::user_agent, "Jami");
    request.set_header_field(restinio::http_field_t::accept, "application/json");
    request.set_header_field(restinio::http_field_t::content_type, "application/json");
}

void
ServerAccountManager::initAuthentication(
    CertRequest csrRequest,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    OnChangeCallback onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->request = std::move(csrRequest);
    ctx->credentials = dynamic_unique_cast<ServerAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);
    if (not ctx->credentials or ctx->credentials->username.empty()) {
        onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    onChange_ = std::move(onChange);

    const std::string url = managerHostname_ + PATH_DEVICE_REGISTER;
    JAMI_WARN("[Auth] authentication with: %s to %s", ctx->credentials->username.c_str(), url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, logger_);
    auto reqid = request->id();
    restinio::http_request_header_t header;
    header.method(restinio::http_method_post());
    request->set_header(header);
    request->set_target(PATH_DEVICE_REGISTER);
    request->set_auth(ctx->credentials->username, ctx->credentials->password);
    {
        std::stringstream ss;
        auto csr = ctx->request.get()->toString();
        string_replace(csr, "\n", "\\n");
        string_replace(csr, "\r", "\\r");
        ss << "{\"csrRequest\":\"" << csr  << "\"}";
        JAMI_WARN("[Auth] Sending request: %s", csr.c_str());
        request->set_body(ss.str());
    }
    setHeaderFields(*request);
    request->set_header_field(restinio::http_field_t::expect, "100-continue");
    request->add_on_state_change_callback([reqid, ctx, onAsync = onAsync_]
                                          (Request::State state, const dht::http::Response& response){

        JAMI_ERR("[Auth] Got server response: %d", (int)state);
        if (state != Request::State::DONE)
            return;
        if (response.status_code >= 400 && response.status_code < 500)
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

                auto certStr = json["x509Certificate"].asString();
                string_replace(certStr, "\\n", "\n");
                string_replace(certStr, "\\r", "\r");
                auto cert = std::make_shared<dht::crypto::Certificate>(certStr);

                auto accountCert = cert->issuer;
                if (not accountCert) {
                    JAMI_ERR("[Auth] Can't parse certificate: no issuer");
                    ctx->onFailure(AuthError::SERVER_ERROR, "Invalid certificate from server");
                    return;
                }
                auto receipt = json["receipt"].asString();
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
                    info->contacts = std::make_unique<ContactList>(accountCert, this_.path_, this_.onChange_);
                    //info->contacts->setContacts(a.contacts);
                    info->accountId = accountCert->getId().toString();
                    info->deviceId = cert->getPublicKey().getId().toString();
                    info->ethAccount = receiptJson["eth"].asString();
                    info->announce = parseAnnounce(receiptJson["announce"].asString(), info->accountId, info->deviceId);
                    info->archivePath = {};
                    info->archiveHasPassword = ctx->credentials && !ctx->credentials->password.empty();
                    if (not info->announce) {
                        ctx->onFailure(AuthError::SERVER_ERROR, "Can't parse announce from server");
                    }

                    this_.creds_ = std::move(ctx->credentials);
                    this_.info_ = std::move(info);
                    //auto info = useIdentity();
                    std::map<std::string, std::string> config;
                    if (json.isMember("nameServer")) {
                        config.emplace(DRing::Account::ConfProperties::RingNS::ACCOUNT, json["nameServer"].asString());
                    }
                    if (json.isMember("displayName")) {
                        config.emplace(DRing::Account::ConfProperties::DISPLAYNAME, json["displayName"].asString());
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
    const std::string url = managerHostname_ + PATH_DEVICES;
    JAMI_WARN("[Auth] syncDevices with: %s to %s", creds_->username.c_str(), url.c_str());
    auto request = std::make_shared<Request>(*Manager::instance().ioContext(), url, logger_);
    auto reqid = request->id();
    restinio::http_request_header_t header;
    header.method(restinio::http_method_get());
    request->set_header(header);
    request->set_target(PATH_DEVICES);
    request->set_auth(creds_->username, creds_->password);
    setHeaderFields(*request);
    request->add_on_state_change_callback([reqid, onAsync = onAsync_]
                                            (Request::State state, const dht::http::Response& response){
        onAsync([reqid, state, response] (AccountManager& accountManager) {
            JAMI_ERR("[Auth] Got server response: %d", (int)state);
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

void
ServerAccountManager::registerName(const std::string& password, const std::string& name, RegistrationCallback cb)
{
    cb(NameDirectory::RegistrationResponse::unsupported);
}

}
