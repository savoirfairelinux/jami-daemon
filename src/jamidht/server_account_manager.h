/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#pragma once

#include "account_manager.h"

#include <queue>
#include <set>
#include <chrono>

namespace jami {

class ServerAccountManager : public AccountManager
{
public:
    ServerAccountManager(const std::string& path,
                         const std::string& managerHostname,
                         const std::string& nameServer);

    struct ServerAccountCredentials : AccountCredentials
    {
        std::string username;
        std::shared_ptr<dht::crypto::Certificate> ca;
    };

    void initAuthentication(const std::string& accountId,
                            PrivateKey request,
                            std::string deviceName,
                            std::unique_ptr<AccountCredentials> credentials,
                            AuthSuccessCallback onSuccess,
                            AuthFailureCallback onFailure,
                            const OnChangeCallback& onChange) override;

    bool changePassword(const std::string& /*password_old*/,
                        const std::string& /*password_new*/) override
    {
        return false;
    }

    void syncDevices() override;

    bool revokeDevice(const std::string& password,
                      const std::string& device,
                      RevokeDeviceCallback cb) override;

    bool searchUser(const std::string& query, SearchCallback cb) override;
    void registerName(const std::string& password,
                      const std::string& name,
                      RegistrationCallback cb) override;

private:
    struct AuthContext
    {
        std::string accountId;
        PrivateKey key;
        CertRequest request;
        std::string deviceName;
        std::unique_ptr<ServerAccountCredentials> credentials;
        AuthSuccessCallback onSuccess;
        AuthFailureCallback onFailure;
    };

    const std::string managerHostname_;
    std::shared_ptr<dht::Logger> logger_;

    std::mutex requestLock_;
    std::set<std::shared_ptr<dht::http::Request>> requests_;
    std::unique_ptr<ServerAccountCredentials> creds_;

    void sendRequest(const std::shared_ptr<dht::http::Request>& request);
    void clearRequest(const std::weak_ptr<dht::http::Request>& request);

    enum class TokenScope : unsigned { None = 0, Device, User, Admin };
    std::mutex tokenLock_;
    std::string token_ {};
    TokenScope tokenScope_ {};
    std::chrono::steady_clock::time_point tokenExpire_ {
        std::chrono::steady_clock::time_point::min()};
    unsigned authErrorCount {0};

    using RequestQueue = std::queue<std::shared_ptr<dht::http::Request>>;
    RequestQueue pendingDeviceRequests_;
    RequestQueue pendingAccountRequests_;
    RequestQueue& getRequestQueue(TokenScope scope)
    {
        return scope == TokenScope::Device ? pendingDeviceRequests_ : pendingAccountRequests_;
    }
    bool hasAuthorization(TokenScope scope) const
    {
        return not token_.empty() and tokenScope_ >= scope
               and tokenExpire_ >= std::chrono::steady_clock::now();
    }
    void setAuthHeaderFields(dht::http::Request& request) const;

    void sendDeviceRequest(const std::shared_ptr<dht::http::Request>& req);
    void sendAccountRequest(const std::shared_ptr<dht::http::Request>& req, const std::string& password);

    void authenticateDevice();
    void authenticateAccount(const std::string& username, const std::string& password);
    void authFailed(TokenScope scope, int code);
    void authError(TokenScope scope);
    void onAuthEnded(const Json::Value& json, const dht::http::Response& response, TokenScope scope);

    void setToken(std::string token,
                  TokenScope scope,
                  std::chrono::steady_clock::time_point expiration);
};

} // namespace jami
