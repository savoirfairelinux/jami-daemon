/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
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
#include "security/certstore.h"

#include <opendht/infohash.h>
#include <opendht/crypto.h>

#include <map>
#include <mutex>
#include <chrono>

namespace jami {

class ContactList {
public:
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using VerifyResult = dht::crypto::TrustList::VerifyResult;

    using OnContactAdded = std::function<void(const std::string&, bool)>;
    using OnContactRemoved = std::function<void(const std::string&, bool)>;
    using OnIncomingTrustRequest = std::function<void(const std::string&, const std::vector<uint8_t>&, bool)>;
    using OnDevicesChanged = std::function<void()>;

    struct OnChangeCallback {
        OnContactAdded contactAdded;
        OnContactRemoved contactRemoved;
        OnIncomingTrustRequest trustRequest;
        OnDevicesChanged devicesChanged;
    };

    ContactList(const std::shared_ptr<crypto::Certificate>& cert, const std::string& path, OnChangeCallback cb);
    ~ContactList();

    void load();
    void save();

    /* Contacts */
    std::map<std::string, std::string> getContactDetails(const dht::InfoHash&) const;
    void removeContact(const dht::InfoHash&, bool ban);
    void addContact(const dht::InfoHash&, bool confirmed = false);

    bool setCertificateStatus(const std::string& cert_id, const tls::TrustStore::PermissionStatus status);
    bool setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert, tls::TrustStore::PermissionStatus status, bool local = true);

    tls::TrustStore::PermissionStatus getCertificateStatus(const std::string& cert_id) const {
        return trust_.getCertificateStatus(cert_id);
    }

    std::vector<std::string> getCertificatesByStatus(tls::TrustStore::PermissionStatus status) const {
        return trust_.getCertificatesByStatus(status);
    }

    bool isAllowed(const crypto::Certificate& crt, bool allowPublic) {
        return trust_.isAllowed(crt, allowPublic);
    }

    VerifyResult isValidAccountDevice(const crypto::Certificate& crt) const {
        return accountTrust_.verify(crt);
    }

    const std::map<dht::InfoHash, Contact>& getContacts() const;
    void setContacts(const std::map<dht::InfoHash, Contact>&);
    void updateContact(const dht::InfoHash&, const Contact&);

    /* Contact requests */

    void onTrustRequest(const dht::InfoHash& peer_account, const dht::InfoHash& peer_device, time_t received, bool confirm, std::vector<uint8_t>&& payload);
    std::vector<std::map<std::string, std::string>> getTrustRequests() const;
    bool acceptTrustRequest(const dht::InfoHash& from);
    bool discardTrustRequest(const dht::InfoHash& from);

    /* Devices */
    const std::map<dht::InfoHash, KnownDevice>& getKnownDevices() const { return knownDevices_; }
    bool foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name = {}, const time_point& last_sync = time_point::min());
    bool removeAccountDevice(const dht::InfoHash& device);
    void setAccountDeviceName(const dht::InfoHash& device, const std::string& name);

    DeviceSync getSyncData() const;
    bool syncDevice(const dht::InfoHash& device, const time_point& syncDate);
    //void onSyncData(DeviceSync&& device);

private:
    mutable std::mutex lock;
    std::map<dht::InfoHash, Contact> contacts_;
    std::map<dht::InfoHash, TrustRequest> trustRequests_;
    std::map<dht::InfoHash, KnownDevice> knownDevices_;

    // Trust store with account main certificate as the only CA
    dht::crypto::TrustList accountTrust_;
    // Trust store for to match peer certificates
    tls::TrustStore trust_;
    std::string path_;

    OnChangeCallback callbacks_;

    void loadContacts();
    void saveContacts() const;

    void loadTrustRequests();
    void saveTrustRequests() const;

    void loadKnownDevices();
    void saveKnownDevices() const;
};

}
