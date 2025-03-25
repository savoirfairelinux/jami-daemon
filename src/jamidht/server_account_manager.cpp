/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "server_account_manager.h"
#include "base64.h"
#include "jami/account_const.h"
#include "fileutils.h"

#include <opendht/http.h>
#include <opendht/log.h>
#include <opendht/thread_pool.h>

#include "conversation_module.h"
#include "jamiaccount.h"
#include "manager.h"

#include <algorithm>
#include <string_view>

using namespace std::literals;

namespace jami {

using Request = dht::http::Request;

#define JAMI_PATH_LOGIN "/api/login"
#define JAMI_PATH_AUTH  "/api/auth"
constexpr std::string_view PATH_DEVICE = JAMI_PATH_AUTH "/device";
constexpr std::string_view PATH_DEVICES = JAMI_PATH_AUTH "/devices";
constexpr std::string_view PATH_SEARCH = JAMI_PATH_AUTH "/directory/search";
constexpr std::string_view PATH_CONTACTS = JAMI_PATH_AUTH "/contacts";
constexpr std::string_view PATH_CONVERSATIONS = JAMI_PATH_AUTH "/conversations";
constexpr std::string_view PATH_CONVERSATIONS_REQUESTS = JAMI_PATH_AUTH "/conversationRequests";
constexpr std::string_view PATH_BLUEPRINT = JAMI_PATH_AUTH "/policyData";

ServerAccountManager::ServerAccountManager(const std::string& accountId,
                                           const std::filesystem::path& path,
                                           const std::string& managerHostname,
                                           const std::string& nameServer)
    : AccountManager(accountId, path, nameServer)
    , managerHostname_(managerHostname)
    , logger_(Logger::dhtLogger()) {}

void
ServerAccountManager::setAuthHeaderFields(Request& request) const
{
    request.set_header_field(restinio::http_field_t::authorization, "Bearer " + token_);
}

void
ServerAccountManager::initAuthentication(PrivateKey key,
                                         std::string deviceName,
                                         std::unique_ptr<AccountCredentials> credentials,
                                         AuthSuccessCallback onSuccess,
                                         AuthFailureCallback onFailure,
                                         const OnChangeCallback& onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->accountId = accountId_;
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
    JAMI_WARNING("[Account {}] [Auth] Authentication with: {} to {}", accountId_, ctx->credentials->username, url);

    dht::ThreadPool::computation().run([ctx, url, w=weak_from_this()] {
        auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
        if (not this_) return;
        Json::Value body;
        {
            auto csr = ctx->request.get()->toString();
            body["csr"] = csr;
            body["deviceName"] = ctx->deviceName;
        }
        auto request = std::make_shared<Request>(
            *Manager::instance().ioContext(),
            url,
            body,
            [ctx, w](Json::Value json, const dht::http::Response& response) {
                auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
                JAMI_DEBUG("[Auth] Got request callback with status code={}",
                            response.status_code);
                if (response.status_code == 0 || this_ == nullptr)
                    ctx->onFailure(AuthError::SERVER_ERROR, "Unable to connect to server");
                else if (response.status_code >= 400 && response.status_code < 500)
                    ctx->onFailure(AuthError::INVALID_ARGUMENTS, "Invalid credentials provided!");
                else if (response.status_code < 200 || response.status_code > 299)
                    ctx->onFailure(AuthError::INVALID_ARGUMENTS, "");
                else {
                    do {
                        try {
                            JAMI_WARNING("[Account {}] [Auth] Got server response: {} bytes", this_->accountId_, response.body.size());
                            auto cert = std::make_shared<dht::crypto::Certificate>(
                                json["certificateChain"].asString());
                            auto accountCert = cert->issuer;
                            if (not accountCert) {
                                JAMI_ERR("[Auth] Unable to parse certificate: no issuer");
                                ctx->onFailure(AuthError::SERVER_ERROR,
                                                "Invalid certificate from server");
                                break;
                            }
                            auto receipt = json["deviceReceipt"].asString();
                            Json::Value receiptJson;
                            std::string err;
                            auto receiptReader = std::unique_ptr<Json::CharReader>(
                                Json::CharReaderBuilder {}.newCharReader());
                            if (!receiptReader->parse(receipt.data(),
                                                        receipt.data() + receipt.size(),
                                                        &receiptJson,
                                                        &err)) {
                                JAMI_ERR("[Auth] Unable to parse receipt from server: %s",
                                            err.c_str());
                                ctx->onFailure(AuthError::SERVER_ERROR,
                                                "Unable to parse receipt from server");
                                break;
                            }
                            auto receiptSignature = base64::decode(
                                json["receiptSignature"].asString());

                            auto info = std::make_unique<AccountInfo>();
                            info->identity.first = ctx->key.get();
                            info->identity.second = cert;
                            info->devicePk = cert->getSharedPublicKey();
                            info->deviceId = info->devicePk->getLongId().toString();
                            info->accountId = accountCert->getId().toString();
                            info->contacts = std::make_unique<ContactList>(ctx->accountId,
                                                                            accountCert,
                                                                            this_->path_,
                                                                            this_->onChange_);
                            // info->contacts->setContacts(a.contacts);
                            if (ctx->deviceName.empty())
                                ctx->deviceName = info->deviceId.substr(8);
                            info->contacts->foundAccountDevice(cert,
                                                                ctx->deviceName,
                                                                clock::now());
                            info->ethAccount = receiptJson["eth"].asString();
                            info->announce
                                = parseAnnounce(receiptJson["announce"].asString(),
                                                info->accountId,
                                                info->devicePk->getId().toString(),
                                                info->devicePk->getLongId().toString());
                            if (not info->announce) {
                                ctx->onFailure(AuthError::SERVER_ERROR,
                                                "Unable to parse announce from server");
                                return;
                            }
                            info->username = ctx->credentials->username;

                            this_->creds_ = std::move(ctx->credentials);
                            this_->info_ = std::move(info);
                            std::map<std::string, std::string> config;
                            for (auto itr = json.begin(); itr != json.end(); ++itr) {
                                const auto& name = itr.name();
                                if (name == "nameServer"sv) {
                                    auto nameServer = json["nameServer"].asString();
                                    if (!nameServer.empty() && nameServer[0] == '/')
                                        nameServer = this_->managerHostname_ + nameServer;
                                    this_->nameDir_ = NameDirectory::instance(nameServer);
                                    config
                                        .emplace(libjami::Account::ConfProperties::RingNS::URI,
                                                    std::move(nameServer));
                                } else if (name == "userPhoto"sv) {
                                    this_->info_->photo = json["userPhoto"].asString();
                                } else {
                                    config.emplace(name, itr->asString());
                                }
                            }

                            ctx->onSuccess(*this_->info_,
                                            std::move(config),
                                            std::move(receipt),
                                            std::move(receiptSignature));
                        } catch (const std::exception& e) {
                            JAMI_ERROR("[Account {}] [Auth] Error when loading account: {}", this_->accountId_, e.what());
                            ctx->onFailure(AuthError::NETWORK, "");
                        }
                    } while (false);
                }

                if (auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock()))
                    this_->clearRequest(response.request);
            },
            this_->logger_);
        request->set_auth(ctx->credentials->username, ctx->credentials->password);
        this_->sendRequest(request);
    });
}

void
ServerAccountManager::onAuthEnded(const Json::Value& json,
                                  const dht::http::Response& response,
                                  TokenScope expectedScope)
{
    if (response.status_code >= 200 && response.status_code < 300) {
        auto scopeStr = json["scope"].asString();
        auto scope = scopeStr == "DEVICE"sv
                         ? TokenScope::Device
                         : (scopeStr == "USER"sv ? TokenScope::User : TokenScope::None);
        auto expires_in = json["expires_in"].asLargestUInt();
        auto expiration = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in);
        JAMI_WARNING("[Account {}] [Auth] Got server response: {} ({} bytes)", accountId_, response.status_code, response.body.size());
        setToken(json["access_token"].asString(), scope, expiration);
    } else {
        JAMI_WARNING("[Account {}] [Auth] Got server response: {} ({} bytes)", accountId_, response.status_code, response.body.size());
        authFailed(expectedScope, response.status_code);
    }
    clearRequest(response.request);
}

void
ServerAccountManager::authenticateDevice()
{
    if (not info_) {
        authFailed(TokenScope::Device, 0);
    }
    const std::string url = managerHostname_ + JAMI_PATH_LOGIN;
    JAMI_WARNING("[Account {}] [Auth] Getting a device token: {}", accountId_, url);
    auto request = std::make_shared<Request>(
        *Manager::instance().ioContext(),
        url,
        Json::Value {Json::objectValue},
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            if (auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock()))
                this_->onAuthEnded(json, response, TokenScope::Device);
        },
        logger_);
    request->set_identity(info_->identity);
    // request->set_certificate_authority(info_->identity.second->issuer->issuer);
    sendRequest(request);
}

void
ServerAccountManager::authenticateAccount(const std::string& username, const std::string& password)
{
    const std::string url = managerHostname_ + JAMI_PATH_LOGIN;
    JAMI_WARNING("[Account {}] [Auth] Getting a user token: {}", accountId_, url);
    auto request = std::make_shared<Request>(
        *Manager::instance().ioContext(),
        url,
        Json::Value {Json::objectValue},
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            if (auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock()))
                this_->onAuthEnded(json, response, TokenScope::User);
        },
        logger_);
    request->set_auth(username, password);
    sendRequest(request);
}

void
ServerAccountManager::sendRequest(const std::shared_ptr<dht::http::Request>& request)
{
    request->set_header_field(restinio::http_field_t::user_agent, userAgent());
    {
        std::lock_guard lock(requestLock_);
        requests_.emplace(request);
    }
    request->send();
}

void
ServerAccountManager::clearRequest(const std::weak_ptr<dht::http::Request>& request)
{
    Manager::instance().ioContext()->post([w=weak_from_this(), request] {
        if (auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock())) {
            if (auto req = request.lock()) {
                std::lock_guard lock(this_->requestLock_);
                this_->requests_.erase(req);
            }
        }
    });
}

void
ServerAccountManager::authFailed(TokenScope scope, int code)
{
    RequestQueue requests;
    {
        std::lock_guard lock(tokenLock_);
        requests = std::move(getRequestQueue(scope));
    }
    JAMI_DEBUG("[Auth] Failed auth with scope {}, ending {} pending requests",
             (int) scope,
             requests.size());
    if (code == 401) {
        // NOTE: we do not login every time to the server but retrieve a device token to use the account
        // If authentificate device fails with 401
        // it means that the device is revoked
        if (onNeedsMigration_)
            onNeedsMigration_();
    }
    while (not requests.empty()) {
        auto req = std::move(requests.front());
        requests.pop();
        req->terminate(code == 0 ? asio::error::not_connected : asio::error::access_denied);
    }
}

void
ServerAccountManager::authError(TokenScope scope)
{
    {
        std::lock_guard lock(tokenLock_);
        if (scope <= tokenScope_) {
            token_ = {};
            tokenScope_ = TokenScope::None;
        }
    }
    if (scope == TokenScope::Device)
        authenticateDevice();
}

void
ServerAccountManager::setToken(std::string token,
                               TokenScope scope,
                               std::chrono::steady_clock::time_point expiration)
{
    std::lock_guard lock(tokenLock_);
    token_ = std::move(token);
    tokenScope_ = scope;
    tokenExpire_ = expiration;

    nameDir_.get().setToken(token_);
    if (not token_.empty() and scope != TokenScope::None) {
        auto& reqQueue = getRequestQueue(scope);
        JAMI_DEBUG("[Account {}] [Auth] Got token with scope {}, handling {} pending requests",
                 accountId_,
                 (int) scope,
                 reqQueue.size());
        while (not reqQueue.empty()) {
            auto req = std::move(reqQueue.front());
            reqQueue.pop();
            setAuthHeaderFields(*req);
            sendRequest(req);
        }
    }
}

void
ServerAccountManager::sendDeviceRequest(const std::shared_ptr<dht::http::Request>& req)
{
    std::lock_guard lock(tokenLock_);
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

void
ServerAccountManager::sendAccountRequest(const std::shared_ptr<dht::http::Request>& req,
                                         const std::string& pwd)
{
    std::lock_guard lock(tokenLock_);
    if (hasAuthorization(TokenScope::User)) {
        setAuthHeaderFields(*req);
        sendRequest(req);
    } else {
        auto& rQueue = getRequestQueue(TokenScope::User);
        if (rQueue.empty())
            authenticateAccount(info_->username, pwd);
        rQueue.emplace(req);
    }
}

void
ServerAccountManager::syncDevices()
{
    const std::string urlDevices = managerHostname_ + PATH_DEVICES;
    const std::string urlContacts = managerHostname_ + PATH_CONTACTS;
    const std::string urlConversations = managerHostname_ + PATH_CONVERSATIONS;
    const std::string urlConversationsRequests = managerHostname_ + PATH_CONVERSATIONS_REQUESTS;

    JAMI_WARNING("[Account {}] [Auth] Sync conversations {}", accountId_, urlConversations);
    Json::Value jsonConversations(Json::arrayValue);
    for (const auto& [key, convInfo] : ConversationModule::convInfos(accountId_)) {
        jsonConversations.append(convInfo.toJson());
    }
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        urlConversations,
        jsonConversations,
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            JAMI_DEBUG("[Account {}] [Auth] Got conversation sync request callback with status code={}",
                         this_->accountId_,
                         response.status_code);
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARNING("[Account {}] [Auth] Got server response: {} bytes", this_->accountId_, response.body.size());
                    if (not json.isArray()) {
                        JAMI_ERROR("[Account {}] [Auth] Unable to parse server response: not an array", this_->accountId_);
                    } else {
                        SyncMsg convs;
                        for (unsigned i = 0, n = json.size(); i < n; i++) {
                            const auto& e = json[i];
                            auto ci = ConvInfo(e);
                            convs.c[ci.id] = std::move(ci);
                        }
                        dht::ThreadPool::io().run([accountId=this_->accountId_, convs] {
                            if (auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
                                acc->convModule()->onSyncData(convs, "", "");
                            }
                        });
                    }
                } catch (const std::exception& e) {
                    JAMI_ERROR("[Account {}] Error when iterating conversation list: {}", this_->accountId_, e.what());
                }
            } else if (response.status_code == 401)
                this_->authError(TokenScope::Device);

            this_->clearRequest(response.request);
        },
        logger_));

    JAMI_WARNING("[Account {}] [Auth] Sync conversations requests {}", accountId_, urlConversationsRequests);
    Json::Value jsonConversationsRequests(Json::arrayValue);
    for (const auto& [key, convRequest] : ConversationModule::convRequests(accountId_)) {
        auto jsonConversation = convRequest.toJson();
        jsonConversationsRequests.append(std::move(jsonConversation));
    }
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        urlConversationsRequests,
        jsonConversationsRequests,
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            JAMI_DEBUG("[Account {}] [Auth] Got conversations requests sync request callback with status code={}",
                            this_->accountId_, response.status_code);
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARNING("[Account {}] [Auth] Got server response: {}", this_->accountId_, response.body);
                    if (not json.isArray()) {
                        JAMI_ERROR("[Account {}] [Auth] Unable to parse server response: not an array", this_->accountId_);
                    } else {
                        SyncMsg convReqs;
                        for (unsigned i = 0, n = json.size(); i < n; i++) {
                            const auto& e = json[i];
                            auto cr = ConversationRequest(e);
                            convReqs.cr[cr.conversationId] = std::move(cr);
                        }
                        dht::ThreadPool::io().run([accountId=this_->accountId_, convReqs] {
                            if (auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
                                acc->convModule()->onSyncData(convReqs, "", "");
                            }
                        });
                    }
                } catch (const std::exception& e) {
                    JAMI_ERROR("[Account {}] Error when iterating conversations requests list: {}", this_->accountId_, e.what());
                }
            } else if (response.status_code == 401)
                this_->authError(TokenScope::Device);

            this_->clearRequest(response.request);
        },
        logger_));

    JAMI_WARNING("[Account {}] [Auth] syncContacts {}", accountId_, urlContacts);
    Json::Value jsonContacts(Json::arrayValue);
    for (const auto& contact : info_->contacts->getContacts()) {
        auto jsonContact = contact.second.toJson();
        jsonContact["uri"] = contact.first.toString();
        jsonContacts.append(std::move(jsonContact));
    }
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        urlContacts,
        jsonContacts,
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            JAMI_DEBUG("[Account {}] [Auth] Got contact sync request callback with status code={}",
                this_->accountId_, response.status_code);
            if (!this_) return;
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARNING("[Account {}] [Auth] Got server response: {} bytes", this_->accountId_, response.body.size());
                    if (not json.isArray()) {
                        JAMI_ERROR("[Auth] Unable to parse server response: not an array");
                    } else {
                        for (unsigned i = 0, n = json.size(); i < n; i++) {
                            const auto& e = json[i];
                            Contact contact(e);
                            this_->info_->contacts
                                ->updateContact(dht::InfoHash {e["uri"].asString()}, contact);
                        }
                        this_->info_->contacts->saveContacts();
                    }
                } catch (const std::exception& e) {
                    JAMI_ERROR("Error when iterating contact list: {}", e.what());
                }
            } else if (response.status_code == 401)
                this_->authError(TokenScope::Device);

            this_->clearRequest(response.request);
        },
        logger_));

    JAMI_WARNING("[Account {}] [Auth] syncDevices {}", accountId_, urlDevices);
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        urlDevices,
        [w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            JAMI_DEBUG("[Account {}] [Auth] Got request callback with status code={}", this_->accountId_, response.status_code);
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_LOG("[Account {}] [Auth] Got server response: {} bytes", this_->accountId_, response.body.size());
                    if (not json.isArray()) {
                        JAMI_ERROR("[Auth] Unable to parse server response: not an array");
                    } else {
                        for (unsigned i = 0, n = json.size(); i < n; i++) {
                            const auto& e = json[i];
                            const bool revoked = e["revoked"].asBool();
                            dht::PkId deviceId(e["deviceId"].asString());
                            if(!deviceId){
                                continue;
                            }
                            if (!revoked) {
                                this_->info_->contacts->foundAccountDevice(deviceId,
                                                                            e["alias"].asString(),
                                                                            clock::now());
                            }
                            else {
                                this_->info_->contacts->removeAccountDevice(deviceId);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    JAMI_ERROR("Error when iterating device list: {}", e.what());
                }
            } else if (response.status_code == 401)
                this_->authError(TokenScope::Device);

            this_->clearRequest(response.request);
        },
        logger_));
}

void
ServerAccountManager::syncBlueprintConfig(SyncBlueprintCallback onSuccess)
{
    auto syncBlueprintCallback = std::make_shared<SyncBlueprintCallback>(onSuccess);
    const std::string urlBlueprints = managerHostname_ + PATH_BLUEPRINT;
    JAMI_DEBUG("[Auth] Synchronize blueprint configuration {}", urlBlueprints);
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        urlBlueprints,
        [syncBlueprintCallback, w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            JAMI_DEBUG("[Auth] Got sync request callback with status code={}", response.status_code);
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    std::map<std::string, std::string> config;
                    for (auto itr = json.begin(); itr != json.end(); ++itr) {
                        const auto& name = itr.name();
                        config.emplace(name, itr->asString());
                    }
                    (*syncBlueprintCallback)(config);
                } catch (const std::exception& e) {
                    JAMI_ERROR("Error when iterating blueprint config json: {}", e.what());
                }
            } else if (response.status_code == 401)
                this_->authError(TokenScope::Device);
            this_->clearRequest(response.request);
        },
        logger_));
}

bool
ServerAccountManager::revokeDevice(const std::string& device,
                                   std::string_view scheme, const std::string& password,
                                   RevokeDeviceCallback cb)
{
    if (not info_ || scheme != fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD) {
        if (cb)
            cb(RevokeDeviceResult::ERROR_CREDENTIALS);
        return false;
    }
    const std::string url = managerHostname_ + PATH_DEVICE + "/" + device;
    JAMI_WARNING("[Revoke] Revoking device of {} at {}", info_->username, url);
    auto request = std::make_shared<Request>(
        *Manager::instance().ioContext(),
        url,
        [cb, w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            JAMI_DEBUG("[Revoke] Got request callback with status code={}", response.status_code);
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    JAMI_WARNING("[Revoke] Got server response");
                    if (json["errorDetails"].empty()) {
                        if (cb)
                            cb(RevokeDeviceResult::SUCCESS);
                        this_->syncDevices(); // this will remove the devices from the known devices
                    }
                } catch (const std::exception& e) {
                    JAMI_ERROR("Error when loading device list: {}", e.what());
                }
            } else if (cb)
                cb(RevokeDeviceResult::ERROR_NETWORK);
            this_->clearRequest(response.request);
        },
        logger_);
    request->set_method(restinio::http_method_delete());
    sendAccountRequest(request, password);
    return true;
}

void
ServerAccountManager::registerName(const std::string& name, std::string_view scheme, const std::string&, RegistrationCallback cb)
{
    cb(NameDirectory::RegistrationResponse::unsupported, name);
}

bool
ServerAccountManager::searchUser(const std::string& query, SearchCallback cb)
{
    const std::string url = managerHostname_ + PATH_SEARCH + "?queryString=" + query;
    JAMI_WARNING("[Search] Searching user {} at {}", query, url);
    sendDeviceRequest(std::make_shared<Request>(
        *Manager::instance().ioContext(),
        url,
        [cb, w=weak_from_this()](Json::Value json, const dht::http::Response& response) {
            JAMI_DEBUG("[Search] Got request callback with status code={}", response.status_code);
            auto this_ = std::static_pointer_cast<ServerAccountManager>(w.lock());
            if (!this_) return;
            if (response.status_code >= 200 && response.status_code < 300) {
                try {
                    const auto& profiles = json["profiles"];
                    Json::Value::ArrayIndex rcount = profiles.size();
                    std::vector<std::map<std::string, std::string>> results;
                    results.reserve(rcount);
                    JAMI_WARNING("[Search] Got server response: {} bytes", response.body.size());
                    for (Json::Value::ArrayIndex i = 0; i < rcount; i++) {
                        const auto& ruser = profiles[i];
                        std::map<std::string, std::string> user;
                        for (const auto& member : ruser.getMemberNames()) {
                            const auto& rmember = ruser[member];
                            if (rmember.isString())
                                user[member] = rmember.asString();
                        }
                        results.emplace_back(std::move(user));
                    }
                    if (cb)
                        cb(results, SearchResponse::found);
                } catch (const std::exception& e) {
                    JAMI_ERROR("[Search] Error during search: {}", e.what());
                }
            } else {
                if (response.status_code == 401)
                    this_->authError(TokenScope::Device);
                if (cb)
                    cb({}, SearchResponse::error);
            }
            this_->clearRequest(response.request);
        },
        logger_));
    return true;
}

} // namespace jami
