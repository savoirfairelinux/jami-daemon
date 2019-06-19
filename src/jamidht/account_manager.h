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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "contact_list.h"
#include "logger.h"
#if HAVE_RINGNS
#include "namedirectory.h"
#endif

#include <opendht/crypto.h>

#include <functional>
#include <string>
#include <map>

namespace dht {
    class DhtRunner;
}

namespace jami {

class AccountArchive;

struct AccountInfo {
    dht::crypto::Identity identity;
    std::unique_ptr<ContactList> contacts;
    std::string accountId;
    std::string deviceId;
    std::shared_ptr<dht::Value> announce;
    std::string ethAccount;
};

class AccountManager {
public:
    using AsyncUser = std::function<void(AccountManager&)>;
    using OnAsync = std::function<void(AsyncUser&&)>;
    using OnChangeCallback = ContactList::OnChangeCallback;
    using OnExportConfig = std::function<std::map<std::string, std::string>()>;
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;

    AccountManager(
        const std::string& path,
        OnAsync&& onAsync,
        OnExportConfig&& onExportConfig,
        std::shared_ptr<dht::DhtRunner> dht)
    : path_(path), onAsync_(std::move(onAsync)), onExportConfig_(std::move(onExportConfig)), dht_(std::move(dht))  {};

    virtual ~AccountManager() = default;

    // Auth

    enum class AuthError {
        UNKNOWN,
        INVALID_ARGUMENTS,
        NETWORK
    };

    using AuthSuccessCallback = std::function<void(
        const AccountInfo& info,
        const std::map<std::string, std::string>& config,
        std::string&& receipt,
        std::vector<uint8_t>&& receipt_signature)>;

    using AuthFailureCallback = std::function<void(AuthError error, const std::string& message)>;
    using DeviceSyncCallback = std::function<void(DeviceSync&& syncData)>;
    using CertRequest = std::future<std::unique_ptr<dht::crypto::CertificateRequest>>;

    struct AccountCredentials {
        std::string scheme;
        std::string uri;
        std::string password;
        virtual ~AccountCredentials() {};
    };

    virtual void initAuthentication(
        CertRequest request,
        std::unique_ptr<AccountCredentials> credentials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        OnChangeCallback onChange) = 0;

    virtual bool changePassword(const std::string& password_old, const std::string& password_new) = 0;

    virtual void syncDevices() = 0;

    dht::crypto::Identity loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const;

    const AccountInfo* useIdentity(
        const dht::crypto::Identity& id,
        const std::string& receipt,
        const std::vector<uint8_t>& receiptSignature,
        OnChangeCallback&& onChange);

    virtual void startSync() {};

    const AccountInfo* getInfo() const {
        return info_.get()  ;
    }

    // Device management

    enum class AddDeviceResult {
        SUCCESS_SHOW_PIN = 0,
        ERROR_CREDENTIALS,
        ERROR_NETWORK,
    };
    using AddDeviceCallback = std::function<void(AddDeviceResult, std::string pin)>;

    enum class RevokeDeviceResult {
        SUCCESS = 0,
        ERROR_CREDENTIALS,
        ERROR_NETWORK,
    };
    using RevokeDeviceCallback = std::function<void(RevokeDeviceResult)>;

    virtual void addDevice(const std::string& /*password*/, AddDeviceCallback) {};
    virtual bool revokeDevice(const std::string& /*password*/, const std::string& /*device*/, RevokeDeviceCallback) { return false; };

    const std::map<dht::InfoHash, KnownDevice>& getKnownDevices() const;
    bool foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name = {}, const time_point& last_sync = time_point::min());
    //bool removeAccountDevice(const dht::InfoHash& device);
    void setAccountDeviceName(/*const dht::InfoHash& device,  */const std::string& name);


    void forEachDevice(const dht::InfoHash& to,
                        std::function<void(const dht::InfoHash&)>&& op,
                        std::function<void(bool)>&& end = {});

    /**
     * Inform that a potential peer device have been found.
     * Returns true only if the device certificate is a valid device certificate.
     * In that case (true is returned) the account_id parameter is set to the peer account ID.
     */
    static bool foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, dht::InfoHash& account_id);


    // Contact requests

    std::vector<std::map<std::string, std::string>> getTrustRequests() const;
    bool acceptTrustRequest(const std::string& from);
    bool discardTrustRequest(const std::string& from);

    void sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload);
    void sendTrustRequestConfirm(const dht::InfoHash& to);

    // Contact

    /**
     * Add contact to the account contact list.
     * Set confirmed if we know the contact also added us.
     */
    void addContact(const std::string& uri, bool confirmed = false);
    void removeContact(const std::string& uri, bool banned = true);
    std::vector<std::map<std::string, std::string>> getContacts() const;

    /** Obtain details about one account contact in serializable form. */
    std::map<std::string, std::string> getContactDetails(const std::string& uri) const;

    virtual bool findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {}) = 0;
    bool setCertificateStatus(const std::string& cert_id, tls::TrustStore::PermissionStatus status);
    std::vector<std::string> getCertificatesByStatus(tls::TrustStore::PermissionStatus status);
    tls::TrustStore::PermissionStatus getCertificateStatus(const std::string& cert_id) const;
    bool isAllowed(const crypto::Certificate& crt, bool allowPublic);

    // Name resolver
#if HAVE_RINGNS
    using LookupCallback = NameDirectory::LookupCallback;
    using RegistrationCallback = NameDirectory::RegistrationCallback;

    virtual void lookupName(const std::string& name, LookupCallback cb) = 0;
    virtual void lookupAddress(const std::string& address, LookupCallback cb) = 0;
    virtual void registerName(const std::string& password, const std::string& name, RegistrationCallback cb) = 0;
#endif

protected:
    std::string path_;
    OnAsync onAsync_;
    OnExportConfig onExportConfig_;
    OnChangeCallback onChange_;
    std::unique_ptr<AccountInfo> info_;
    std::shared_ptr<dht::DhtRunner> dht_;
};


class ArchiveAccountManager : public AccountManager {
public:
    ArchiveAccountManager(
        const std::string& path,
        std::shared_ptr<dht::DhtRunner> dht,
        OnAsync&& onAsync,
        OnExportConfig&& onExportConfig,
        std::string archivePath,
        const std::string& nameServer)
     : AccountManager(path, std::move(onAsync), std::move(onExportConfig), std::move(dht)), archivePath_(std::move(archivePath)), nameDir_(NameDirectory::instance(nameServer)) {};

    struct ArchiveAccountCredentials : AccountCredentials {
        std::string archivePath;
        in_port_t dhtPort;
        std::vector<std::string> dhtBootstrap;
        dht::crypto::Identity updateIdentity;
    };

    void initAuthentication(
        CertRequest request,
        std::unique_ptr<AccountCredentials> credentials,
        AuthSuccessCallback onSuccess,
        AuthFailureCallback onFailure,
        OnChangeCallback onChange) override;

    void startSync() override;

    bool changePassword(const std::string& password_old, const std::string& password_new) override;

    void syncDevices() override;
    void onSyncData(DeviceSync&& device);

    bool findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {}) override;

    void addDevice(const std::string& password, AddDeviceCallback) override;
    bool revokeDevice(const std::string& password, const std::string& device, RevokeDeviceCallback) override;
    bool exportArchive(const std::string& destinationPath, const std::string& password);

#if HAVE_RINGNS
    void lookupName(const std::string& name, LookupCallback cb) override;
    void lookupAddress(const std::string& address, LookupCallback cb) override;
    void registerName(const std::string& password, const std::string& name, RegistrationCallback cb) override;
#endif

private:
    struct DhtLoadContext;
    struct AuthContext {
        CertRequest request;
        //std::unique_ptr<dht::crypto::CertificateRequest> request;
        std::unique_ptr<ArchiveAccountCredentials> credentials;
        std::unique_ptr<DhtLoadContext> dhtContext;
        AuthSuccessCallback onSuccess;
        AuthFailureCallback onFailure;
    };

    void createAccount(const std::shared_ptr<AuthContext>& ctx);
    std::pair<std::string, std::shared_ptr<dht::Value>> makeReceipt(const dht::crypto::Identity& id, const dht::crypto::Certificate& device, const std::string& ethAccount);
    void updateArchive(AccountArchive& content/*, const ContactList& syncData*/) const;
    void saveArchive(AccountArchive& content, const std::string& pwd);
    AccountArchive readArchive(const std::string& pwd) const;
    static std::pair<std::vector<uint8_t>, dht::InfoHash> computeKeys(const std::string& password, const std::string& pin, bool previous=false);
    bool updateCertificates(AccountArchive& archive, dht::crypto::Identity& device);
    static bool needsMigration(const dht::crypto::Identity& id);

    void loadFromFile(const std::shared_ptr<AuthContext>& ctx);
    void loadFromDHT(const std::shared_ptr<AuthContext>& ctx);
    void onArchiveLoaded(AuthContext& ctx, AccountArchive&& a);

    std::string archivePath_;
    std::reference_wrapper<NameDirectory> nameDir_;
};

class ServerAccountManager : public AccountManager {


};

}
