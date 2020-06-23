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
#pragma once

#include "account_manager.h"

#include <queue>
#include <set>

namespace jami {

class ServerAccountManager : public AccountManager {
public:
    ServerAccountManager(
        const std::string& path,
        OnAsync&& onAsync,
        const std::string& managerHostname,
        const std::string& nameServer);

    struct ServerAccountCredentials : AccountCredentials {
        std::string username;
        std::shared_ptr<dht::crypto::Certificate> ca;
    };

    void initAuthentication(
        CertRequest request,
        std::string deviceName,
        std::unique_ptr<AccountCredentials> credentials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        OnChangeCallback onChange) override;

    bool changePassword(const std::string& /*password_old*/, const std::string& /*password_new*/) override {
        return false;
    }

    void syncDevices() override;

    bool revokeDevice(const std::string& password, const std::string& device, RevokeDeviceCallback cb) override;

    void registerName(const std::string& password, const std::string& name, RegistrationCallback cb) override;

private:
    struct AuthContext {
        CertRequest request;
        std::string deviceName;
        std::unique_ptr<ServerAccountCredentials> credentials;
        AuthSuccessCallback onSuccess;
        AuthFailureCallback onFailure;
    };

    const std::string managerHostname_;
    std::shared_ptr<dht::Logger> logger_;
    std::set<std::shared_ptr<dht::http::Request>> requests_;
    std::unique_ptr<ServerAccountCredentials> creds_;
    std::string deviceToken_ {};
    std::string accountToken_ {};
    std::queue<std::shared_ptr<dht::http::Request>> pendingDeviceRequests_;
    std::queue<std::shared_ptr<dht::http::Request>> pendingAccountRequests_;

    void setHeaderFields(dht::http::Request& request);
    void setDeviceAuthHeaderFields(dht::http::Request& request);

    void sendDeviceRequest(const std::shared_ptr<dht::http::Request>& req);
    void sendAccountRequest(const std::shared_ptr<dht::http::Request>& req);

    void authenticateDevice();
    void authenticateAccount();

    void setDeviceToken(std::string token);
    void setAccountToken(std::string token);
};

}
