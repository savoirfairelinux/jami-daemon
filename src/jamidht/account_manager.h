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
#include <optional>
#include <functional>
#include <map>
#include <string>
#include <filesystem>

#include <dhtnet/multiplexed_socket.h>

namespace dht {
class DhtRunner;
}

namespace jami {

using DeviceId = dht::PkId;
struct AccountArchive;
class AuthChannelHandler;

struct AccountInfo
{
    dht::crypto::Identity identity;
    std::unique_ptr<ContactList> contacts;
    std::string accountId;
    std::string deviceId;
    std::shared_ptr<dht::crypto::PublicKey> devicePk;
    std::shared_ptr<dht::Value> announce;
    std::string ethAccount;
    std::string username;
    std::string photo;
};

template<typename To, typename From>
std::unique_ptr<To>
dynamic_unique_cast(std::unique_ptr<From>&& p)
{
    if (auto cast = dynamic_cast<To*>(p.get())) {
        std::unique_ptr<To> result(cast);
        p.release();
        return result;
    }
    return {};
}

class AccountManager : public std::enable_shared_from_this<AccountManager>
{
public:
    using OnChangeCallback = ContactList::OnChangeCallback;
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using OnNewDeviceCb = std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>;
    using OnDeviceAnnouncedCb = std::function<void()>;

    AccountManager(const std::string& accountId,
                   const std::filesystem::path& path,
                   const std::string& nameServer)
        : accountId_(accountId)
        , path_(path)
        , nameDir_(NameDirectory::instance(nameServer)) {};

    virtual ~AccountManager();

    constexpr static const char* const DHT_TYPE_NS = "cx.ring";

    // Auth

    enum class AuthError { UNKNOWN, INVALID_ARGUMENTS, SERVER_ERROR, NETWORK };

    using AuthSuccessCallback = std::function<void(const AccountInfo& info,
                                                   const std::map<std::string, std::string>& config,
                                                   std::string&& receipt,
                                                   std::vector<uint8_t>&& receipt_signature)>;

    using AuthFailureCallback = std::function<void(AuthError error, const std::string& message)>;
    using DeviceSyncCallback = std::function<void(DeviceSync&& syncData)>;
    using CertRequest = std::future<std::unique_ptr<dht::crypto::CertificateRequest>>;
    using PrivateKey = std::shared_future<std::shared_ptr<dht::crypto::PrivateKey>>;

    CertRequest buildRequest(PrivateKey fDeviceKey);

    struct AccountCredentials
    {
        std::string scheme;
        std::string uri;
        std::string password_scheme;
        std::string password;
        virtual ~AccountCredentials() {};
    };

    virtual void initAuthentication(PrivateKey request,
                                    std::string deviceName,
                                    std::unique_ptr<AccountCredentials> credentials,
                                    AuthSuccessCallback onSuccess,
                                    AuthFailureCallback onFailure,
                                    const OnChangeCallback& onChange)
        = 0;

    virtual bool changePassword(const std::string& password_old, const std::string& password_new) = 0;

    virtual void syncDevices() = 0;
    virtual void onSyncData(DeviceSync&& device, bool checkDevice = true);

    virtual bool isPasswordValid(const std::string& /*password*/) { return false; };
    virtual std::vector<uint8_t> getPasswordKey(const std::string& /*password*/) { return {}; };

    dht::crypto::Identity loadIdentity(const std::string& crt_path,
                                       const std::string& key_path,
                                       const std::string& key_pwd) const;

    const AccountInfo* useIdentity(const dht::crypto::Identity& id,
                                   const std::string& receipt,
                                   const std::vector<uint8_t>& receiptSignature,
                                   const std::string& username,
                                   const OnChangeCallback& onChange);

    void setDht(const std::shared_ptr<dht::DhtRunner>& dht) { dht_ = dht; }

    virtual void startSync(const OnNewDeviceCb& cb,
                           const OnDeviceAnnouncedCb& dcb,
                           bool publishPresence = true);

    const AccountInfo* getInfo() const { return info_.get(); }

    void reloadContacts();

    // Device management

    enum class AddDeviceError { INVALID_URI = -1, ALREADY_LINKING = -2, GENERIC = -3 };

    enum class RevokeDeviceResult {
        SUCCESS = 0,
        ERROR_CREDENTIALS,
        ERROR_NETWORK,
    };

    using RevokeDeviceCallback = std::function<void(RevokeDeviceResult)>;

    /**
     * Initiates the process of adding a new device to the account
     * @param uri The URI provided by the new device to be added
     * @param auth_scheme The auth scheme (currently only "password" is expected)
     * @param chanel
     * @return A positive operation ID if successful, or a negative value indicating an AddDeviceError:
     *         - INVALID_URI (-1): The provided URI is invalid
     *         - ALREADY_LINKING (-2): A device linking operation is already in progress
     *         - GENERIC (-3): A generic error occurred during the process
     */
    virtual int32_t addDevice(const std::string& /*uri*/,
                              std::string_view /*auth_scheme*/,
                              AuthChannelHandler*)
    {
        return 0;
    };
    virtual bool cancelAddDevice(uint32_t /*token*/) { return false; };
    virtual bool confirmAddDevice(uint32_t /*token*/) { return false; };
    virtual bool revokeDevice(const std::string& /*device*/,
                              std::string_view /*scheme*/,
                              const std::string& /*password*/,
                              RevokeDeviceCallback)
    {
        return false;
    };

    const std::map<dht::PkId, KnownDevice>& getKnownDevices() const;
    bool foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                            const std::string& name = {},
                            const time_point& last_sync = time_point::min());
    // bool removeAccountDevice(const dht::InfoHash& device);
    void setAccountDeviceName(/*const dht::InfoHash& device,  */ const std::string& name);
    std::string getAccountDeviceName() const;

    void forEachDevice(const dht::InfoHash& to,
                       std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)>&& op,
                       std::function<void(bool)>&& end = {});

    using PeerCertificateCb = std::function<void(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                                 const dht::InfoHash& peer_account)>;
    void onPeerMessage(const dht::crypto::PublicKey& peer_device,
                       bool allowPublic,
                       PeerCertificateCb&& cb);
    bool onPeerCertificate(const std::shared_ptr<dht::crypto::Certificate>& crt,
                           bool allowPublic,
                           dht::InfoHash& account_id);

    /**
     * Inform that a potential peer device have been found.
     * Returns true only if the device certificate is a valid device certificate.
     * In that case (true is returned) the account_id parameter is set to the peer account ID.
     */
    static bool foundPeerDevice(const std::string& accoundId,
                                const std::shared_ptr<dht::crypto::Certificate>& crt,
                                dht::InfoHash& account_id);

    // Contact requests

    std::vector<std::map<std::string, std::string>> getTrustRequests() const;
    // Note: includeConversation used for compatibility test, do not use if not in test env.
    bool acceptTrustRequest(const std::string& from, bool includeConversation = true);
    bool discardTrustRequest(const std::string& from);

    void sendTrustRequest(const std::string& to,
                          const std::string& convId,
                          const std::vector<uint8_t>& payload);
    void sendTrustRequestConfirm(const dht::InfoHash& to,
                                 const std::string& conversationId); // TODO ideally no convId here

    // Contact

    /**
     * Add contact to the account contact list.
     * Set confirmed if we know the contact also added us.
     */
    bool addContact(const dht::InfoHash& uri,
                    bool confirmed = false,
                    const std::string& conversationId = "");
    void removeContact(const std::string& uri, bool banned = true);
    void removeContactConversation(const std::string& uri); // for non swarm contacts
    void updateContactConversation(const std::string& uri, const std::string& convId);
    std::vector<std::map<std::string, std::string>> getContacts(bool includeRemoved = false) const;

    /** Obtain details about one account contact in serializable form. */
    std::map<std::string, std::string> getContactDetails(const std::string& uri) const;
    std::optional<Contact> getContactInfo(const std::string& uri) const;

    virtual bool findCertificate(
        const dht::InfoHash& h,
        std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {});

    virtual bool findCertificate(
        const dht::PkId& h,
        std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {});

    bool setCertificateStatus(const std::string& cert_id,
                              dhtnet::tls::TrustStore::PermissionStatus status);
    bool setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                              dhtnet::tls::TrustStore::PermissionStatus status,
                              bool local = true);
    std::vector<std::string> getCertificatesByStatus(
        dhtnet::tls::TrustStore::PermissionStatus status);
    dhtnet::tls::TrustStore::PermissionStatus getCertificateStatus(const std::string& cert_id) const;
    bool isAllowed(const crypto::Certificate& crt, bool allowPublic = false);

    static std::shared_ptr<dht::Value> parseAnnounce(const std::string& announceBase64,
                                                     const std::string& accountId,
                                                     const std::string& deviceSha1,
                                                     const std::string& deviceSha256);

    // Name resolver
    using LookupCallback = NameDirectory::LookupCallback;
    using SearchResult = NameDirectory::SearchResult;
    using SearchCallback = NameDirectory::SearchCallback;
    using RegistrationCallback = NameDirectory::RegistrationCallback;
    using SearchResponse = NameDirectory::Response;

    virtual void lookupUri(const std::string& name,
                           const std::string& defaultServer,
                           LookupCallback cb);
    virtual void lookupAddress(const std::string& address, LookupCallback cb);
    virtual bool searchUser(const std::string& /*query*/, SearchCallback /*cb*/) { return false; }
    virtual void registerName(const std::string& name,
                              std::string_view scheme,
                              const std::string& password,
                              RegistrationCallback cb)
        = 0;

    dhtnet::tls::CertificateStore& certStore() const;

protected:
    const std::string accountId_;
    const std::filesystem::path path_;
    OnChangeCallback onChange_;
    std::unique_ptr<AccountInfo> info_;
    std::shared_ptr<dht::DhtRunner> dht_;
    std::reference_wrapper<NameDirectory> nameDir_;
};

} // namespace jami
