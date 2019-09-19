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
#pragma once

#include "account_manager.h"

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
        std::unique_ptr<AccountCredentials> credentials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        OnChangeCallback onChange) override;

    bool changePassword(const std::string& password_old, const std::string& password_new) {
        return false;
    }

    void syncDevices();

    bool findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {}) {
        return false;
    }
/*
    void lookupName(const std::string& name, LookupCallback cb) {

    }

    void lookupAddress(const std::string& address, LookupCallback cb) {

    }*/

    void registerName(const std::string& password, const std::string& name, RegistrationCallback cb);

private:
    struct AuthContext {
        CertRequest request;
        //std::unique_ptr<dht::crypto::CertificateRequest> request;
        std::unique_ptr<ServerAccountCredentials> credentials;
        //std::unique_ptr<DhtLoadContext> dhtContext;
        AuthSuccessCallback onSuccess;
        AuthFailureCallback onFailure;
    };

    const std::string managerHostname_;
    std::shared_ptr<dht::Logger> logger_;
    std::map<unsigned int /*id*/, std::shared_ptr<dht::http::Request>> requests_;
    std::unique_ptr<ServerAccountCredentials> creds_;

    void setHeaderFields(dht::http::Request& request);
};

}
