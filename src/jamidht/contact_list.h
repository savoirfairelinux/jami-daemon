/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include "jami_contact.h"

#include <dhtnet/certstore.h>
#include <opendht/infohash.h>
#include <opendht/crypto.h>

#include <map>
#include <mutex>
#include <chrono>

namespace jami {

class ContactList
{
public:
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using VerifyResult = dht::crypto::TrustList::VerifyResult;

    using OnContactAdded = std::function<void(const std::string&, bool)>;
    using OnContactRemoved = std::function<void(const std::string&, bool)>;
    using OnIncomingTrustRequest = std::function<
        void(const std::string&, const std::string&, const std::vector<uint8_t>&, time_t)>;
    using OnAcceptConversation = std::function<void(const std::string&)>;
    using OnConfirmation = std::function<void(const std::string&, const std::string&)>;
    using OnDevicesChanged = std::function<void(const std::map<dht::PkId, KnownDevice>&)>;

    struct OnChangeCallback
    {
        OnContactAdded contactAdded;
        OnContactRemoved contactRemoved;
        OnIncomingTrustRequest trustRequest;
        OnDevicesChanged devicesChanged;
        OnAcceptConversation acceptConversation;
        OnConfirmation onConfirmation;
    };

    ContactList(const std::string& accountId,
                const std::shared_ptr<crypto::Certificate>& cert,
                const std::filesystem::path& path,
                OnChangeCallback cb);
    ~ContactList();

    const std::string& accountId() const { return accountId_; }

    void load();
    void save();

    /* Contacts */
    std::map<std::string, std::string> getContactDetails(const dht::InfoHash&) const;
    bool removeContact(const dht::InfoHash&, bool ban);
    bool removeContactConversation(const dht::InfoHash&);
    bool addContact(const dht::InfoHash&,
                    bool confirmed = false,
                    const std::string& conversationId = "");
    void updateConversation(const dht::InfoHash& h, const std::string& conversationId);

    bool setCertificateStatus(const std::string& cert_id,
                              const dhtnet::tls::TrustStore::PermissionStatus status);

    bool setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                              dhtnet::tls::TrustStore::PermissionStatus status,
                              bool local = true);

    dhtnet::tls::TrustStore::PermissionStatus getCertificateStatus(const std::string& cert_id) const
    {
        return trust_->getCertificateStatus(cert_id);
    }

    std::vector<std::string> getCertificatesByStatus(dhtnet::tls::TrustStore::PermissionStatus status) const
    {
        return trust_->getCertificatesByStatus(status);
    }

    bool isAllowed(const crypto::Certificate& crt, bool allowPublic)
    {
        return trust_->isAllowed(crt, allowPublic);
    }

    VerifyResult isValidAccountDevice(const crypto::Certificate& crt) const
    {
        return accountTrust_.verify(crt);
    }

    const std::map<dht::InfoHash, Contact>& getContacts() const;
    void setContacts(const std::map<dht::InfoHash, Contact>&);
    void updateContact(const dht::InfoHash&, const Contact&);

    /** Should be called only after updateContact */
    void saveContacts() const;

    const std::filesystem::path& path() const { return path_; }

    /* Contact requests */

    /** Inform of a new contact request. Returns true if the request should be immediatly accepted
     * (already a contact) */
    bool onTrustRequest(const dht::InfoHash& peer_account,
                        const std::shared_ptr<dht::crypto::PublicKey>& peer_device,
                        time_t received,
                        bool confirm,
                        const std::string& conversationId,
                        std::vector<uint8_t>&& payload);
    std::vector<std::map<std::string, std::string>> getTrustRequests() const;
    std::map<std::string, std::string> getTrustRequest(const dht::InfoHash& from) const;
    void acceptConversation(const std::string& convId); // ToDO this is a bit dirty imho
    bool acceptTrustRequest(const dht::InfoHash& from);
    bool discardTrustRequest(const dht::InfoHash& from);

    /** Should be called only after onTrustRequest */
    void saveTrustRequests() const;

    /* Devices */
    const std::map<dht::PkId, KnownDevice>& getKnownDevices() const { return knownDevices_; }
    void foundAccountDevice(const dht::PkId& device,
                            const std::string& name = {},
                            const time_point& last_sync = time_point::min());
    bool foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                            const std::string& name = {},
                            const time_point& last_sync = time_point::min());
    bool removeAccountDevice(const dht::PkId& device);
    void setAccountDeviceName(const dht::PkId& device, const std::string& name);
    std::string getAccountDeviceName(const dht::PkId& device) const;

    DeviceSync getSyncData() const;
    bool syncDevice(const dht::PkId& device, const time_point& syncDate);

private:
    mutable std::mutex lock;
    std::map<dht::InfoHash, Contact> contacts_;
    std::map<dht::InfoHash, TrustRequest> trustRequests_;
    std::map<dht::InfoHash, KnownDevice> knownDevicesLegacy_;

    std::map<dht::PkId, KnownDevice> knownDevices_;

    // Trust store with account main certificate as the only CA
    dht::crypto::TrustList accountTrust_;
    // Trust store for to match peer certificates
    std::unique_ptr<dhtnet::tls::TrustStore> trust_;
    std::filesystem::path path_;
    std::string accountUri_;

    OnChangeCallback callbacks_;
    std::string accountId_;

    void loadContacts();
    void loadTrustRequests();

    void loadKnownDevices();
    void saveKnownDevices() const;
};

} // namespace jami
