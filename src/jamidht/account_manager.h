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

#include "contact_list.h"
#include "logger.h"

#include <opendht/crypto.h>

#include <functional>
#include <string>
#include <map>

namespace jami {

class JamiAccount;
class AccountArchive;

class AccountManager {
public:
    using AsyncUser = std::function<void(AccountManager*)>;
    using OnAsync = std::function<void(AsyncUser&&)>;

    AccountManager(JamiAccount& account, OnAsync&& onAsync) : account_(account), onAsync_(std::move(onAsync))  {};
    virtual ~AccountManager() = default;

    using AuthSuccessCallback = std::function<void(
        const std::shared_ptr<dht::crypto::Certificate>& device,
        const std::map<std::string, std::string>& config,
        std::unique_ptr<ContactList>&& contacts,
        const std::string& receipt,
        const std::vector<uint8_t>& receipt_signature)>;

    using AuthFailureCallback = std::function<void(int error, const std::string& message)>;

    using DeviceSyncCallback = std::function<void(DeviceSync&& syncData)>;

    struct AccountCredentials {
        std::string scheme;
        std::string uri;
        std::string password;
        virtual ~AccountCredentials() {};
    };

    virtual void initAuthentication(
        std::unique_ptr<dht::crypto::CertificateRequest> request,
        std::unique_ptr<AccountCredentials> credientials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        DeviceSyncCallback onSync) = 0;

    virtual bool changePassword(const std::string& password_old, const std::string& password_new) = 0;

    virtual void syncDevice(std::unique_ptr<DeviceSync>&&) = 0;

    dht::crypto::Identity loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const;

protected:
    JamiAccount& getAccount() { return account_.get(); }
    const JamiAccount& getAccount() const { return account_.get(); }
    OnAsync onAsync_;

private:
    std::reference_wrapper<JamiAccount> account_;
};


class ArchiveAccountManager : public AccountManager {
public:
    ArchiveAccountManager(JamiAccount& account, OnAsync&& onAsync, std::string archivePath)
     : AccountManager(account, std::move(onAsync)), archivePath_(std::move(archivePath)) {};

    struct ArchiveAccountCredentials : AccountCredentials {
        std::string archivePath;
    };

    void initAuthentication(
        std::unique_ptr<dht::crypto::CertificateRequest> request,
        std::unique_ptr<AccountCredentials> credientials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        DeviceSyncCallback onSync) override;

    bool changePassword(const std::string& password_old, const std::string& password_new) override;

    void syncDevice(std::unique_ptr<DeviceSync>&&) override {
        JAMI_WARN("syncDevice");
    }

private:
    std::string makeReceipt(const dht::crypto::Identity& id, const dht::crypto::Certificate& device, const std::string& ethAccount);
    void updateArchive(AccountArchive& content/*, const ContactList& syncData*/) const;
    void saveArchive(AccountArchive& content, const std::string& pwd);
    AccountArchive readArchive(const std::string& pwd) const;

    std::string archivePath_;
};

class ServerAccountManager : public AccountManager {

};

}
