/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#pragma once

#include "account_manager.h"

namespace jami {

class ArchiveAccountManager : public AccountManager
{
public:
    using OnExportConfig = std::function<std::map<std::string, std::string>()>;

    ArchiveAccountManager(const std::filesystem::path& path,
                          OnExportConfig&& onExportConfig,
                          std::string archivePath,
                          const std::string& nameServer)
        : AccountManager(path, nameServer)
        , onExportConfig_(std::move(onExportConfig))
        , archivePath_(std::move(archivePath)) {}

    struct ArchiveAccountCredentials : AccountCredentials
    {
        in_port_t dhtPort;
        std::vector<std::string> dhtBootstrap;
        dht::crypto::Identity updateIdentity;
    };

    void initAuthentication(const std::string& accountId,
                            PrivateKey request,
                            std::string deviceName,
                            std::unique_ptr<AccountCredentials> credentials,
                            AuthSuccessCallback onSuccess,
                            AuthFailureCallback onFailure,
                            const OnChangeCallback& onChange) override;

    void startSync(const OnNewDeviceCb&, const OnDeviceAnnouncedCb& dcb = {}, bool publishPresence = true) override;

    bool changePassword(const std::string& password_old, const std::string& password_new) override;
    virtual std::vector<uint8_t> getPasswordKey(const std::string& /*password*/) override;

    void syncDevices() override;

    void addDevice(const std::string& password, AddDeviceCallback) override;
    bool revokeDevice(const std::string& device,
                      std::string_view scheme, const std::string& password,
                      RevokeDeviceCallback) override;
    bool exportArchive(const std::string& destinationPath, std::string_view scheme, const std::string& password);
    bool isPasswordValid(const std::string& password) override;

#if HAVE_RINGNS
    /*void lookupName(const std::string& name, LookupCallback cb) override;
    void lookupAddress(const std::string& address, LookupCallback cb) override;*/
    void registerName(const std::string& name,
                      std::string_view scheme, const std::string& password,
                      RegistrationCallback cb) override;
#endif

    /**
     * Change the validity of a certificate. If hash is empty, update all certificates
     */
    bool setValidity(std::string_view scheme, const std::string& password,
                     dht::crypto::Identity& device,
                     const dht::InfoHash& id,
                     int64_t validity);

private:
    struct DhtLoadContext;
    struct AuthContext
    {
        std::string accountId;
        PrivateKey key;
        CertRequest request;
        std::string deviceName;
        std::unique_ptr<ArchiveAccountCredentials> credentials;
        std::unique_ptr<DhtLoadContext> dhtContext;
        AuthSuccessCallback onSuccess;
        AuthFailureCallback onFailure;
    };

    void createAccount(AuthContext& ctx);
    void migrateAccount(AuthContext& ctx);

    std::pair<std::string, std::shared_ptr<dht::Value>> makeReceipt(
        const dht::crypto::Identity& id,
        const dht::crypto::Certificate& device,
        const std::string& ethAccount);
    void updateArchive(AccountArchive& content /*, const ContactList& syncData*/) const;
    void saveArchive(AccountArchive& content, std::string_view scheme, const std::string& pwd);
    AccountArchive readArchive(std::string_view scheme, const std::string& password) const;
    static std::pair<std::vector<uint8_t>, dht::InfoHash> computeKeys(const std::string& password,
                                                                      const std::string& pin,
                                                                      bool previous = false);
    bool updateCertificates(AccountArchive& archive, dht::crypto::Identity& device);
    static bool needsMigration(const dht::crypto::Identity& id);

    void loadFromFile(AuthContext& ctx);
    void loadFromDHT(const std::shared_ptr<AuthContext>& ctx);
    void onArchiveLoaded(AuthContext& ctx,
                         AccountArchive&& a);

    OnExportConfig onExportConfig_;
    std::string archivePath_;
};

} // namespace jami
