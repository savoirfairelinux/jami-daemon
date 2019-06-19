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
#include "contact_list.h"
#include "logger.h"
#include "jamiaccount.h"
#include "fileutils.h"

#include "account_const.h"

#include <fstream>

namespace jami {

ContactList::ContactList(const std::shared_ptr<crypto::Certificate>& cert, const std::string& path, OnChangeCallback cb)
 : path_(path), callbacks_(std::move(cb))
{
    if (cert)
        accountTrust_.add(*cert);
}

ContactList::~ContactList() {}

void
ContactList::load()
{
    loadContacts();
    loadTrustRequests();
    loadKnownDevices();
}

void
ContactList::save()
{
    saveContacts();
    saveTrustRequests();
    saveKnownDevices();
}

bool
ContactList::setCertificateStatus(const std::string& cert_id, const tls::TrustStore::PermissionStatus status)
{
    if (contacts_.find(dht::InfoHash(cert_id)) != contacts_.end()) {
        JAMI_DBG("Can't set certificate status for existing contacts %s", cert_id.c_str());
        return false;
    }
    return trust_.setCertificateStatus(cert_id, status);
}

bool
ContactList::addContact(const dht::InfoHash& h, bool confirmed)
{
    JAMI_WARN("[Contacts] addContact: %s", h.to_c_str());
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (c->second.isActive() and c->second.confirmed == confirmed)
        return false;
    c->second.added = std::time(nullptr);
    c->second.confirmed |= confirmed;
    auto hStr = h.toString();
    trust_.setCertificateStatus(hStr, tls::TrustStore::PermissionStatus::ALLOWED);
    saveContacts();
    callbacks_.contactAdded(hStr, c->second.confirmed);
    //emitSignal<DRing::ConfigurationSignal::ContactAdded>(account_.get().getAccountID(), hStr, c->second.confirmed);
    //syncDevices();
    return true;
}

bool
ContactList::removeContact(const dht::InfoHash& h, bool ban)
{
    JAMI_WARN("[Contacts] removeContact: %s", h.to_c_str());
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact{}).first;
    else if (not c->second.isActive() and c->second.banned == ban)
        return false;
    c->second.removed = std::time(nullptr);
    c->second.banned = ban;
    auto uri = h.toString();
    trust_.setCertificateStatus(uri, ban ? tls::TrustStore::PermissionStatus::BANNED
                                         : tls::TrustStore::PermissionStatus::UNDEFINED);
    if (ban and trustRequests_.erase(h) > 0)
        saveTrustRequests();
    saveContacts();
    callbacks_.contactRemoved(uri, ban);
    //emitSignal<DRing::ConfigurationSignal::ContactRemoved>(account_.get().getAccountID(), uri, ban);
    //syncDevices();
    return true;
}

std::map<std::string, std::string>
ContactList::getContactDetails(const dht::InfoHash& h) const
{
    const auto c = contacts_.find(h);
    if (c == std::end(contacts_)) {
        JAMI_WARN("[Contacts] contact '%s' not found", h.to_c_str());
        return {};
    }

    auto details = c->second.toMap();
    if (not details.empty())
        details["id"] = c->first.toString();

    return details;
}

const std::map<dht::InfoHash, Contact>&
ContactList::getContacts() const
{
    return contacts_;
}

void
ContactList::setContacts(const std::map<dht::InfoHash, Contact>& contacts)
{
    contacts_ = contacts;
    saveContacts();
}

void
ContactList::updateContact(const dht::InfoHash& id, const Contact& contact)
{
    if (not id) {
        JAMI_ERR("[Contacts] updateContact: invalid contact ID");
        return;
    }
    bool stateChanged {false};
    auto c = contacts_.find(id);
    if (c == contacts_.end()) {
        JAMI_DBG("[Contacts] new contact: %s", id.toString().c_str());
        c = contacts_.emplace(id, contact).first;
        stateChanged = c->second.isActive() or c->second.isBanned();
    } else {
        JAMI_DBG("[Contacts] updated contact: %s", id.toString().c_str());
        stateChanged = c->second.update(contact);
    }
    if (stateChanged) {
        if (c->second.isActive()) {
            trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::ALLOWED);
            callbacks_.contactAdded(id.toString(), c->second.confirmed);
        } else {
            if (c->second.banned)
                trust_.setCertificateStatus(id.toString(), tls::TrustStore::PermissionStatus::BANNED);
            callbacks_.contactRemoved(id.toString(), c->second.banned);
        }
    }
}

void
ContactList::loadContacts()
{
    decltype(contacts_) contacts;
    try {
        // read file
        auto file = fileutils::loadFile("contacts", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(contacts);
    } catch (const std::exception& e) {
        JAMI_WARN("[Contacts] error loading contacts: %s", e.what());
        return;
    }

    for (auto& peer : contacts)
        updateContact(peer.first, peer.second);
}

void
ContactList::saveContacts() const
{
    std::ofstream file(path_+DIR_SEPARATOR_STR "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

void
ContactList::saveTrustRequests() const
{
    std::ofstream file(path_+DIR_SEPARATOR_STR "incomingTrustRequests", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, trustRequests_);
}

void
ContactList::loadTrustRequests()
{
    std::map<dht::InfoHash, TrustRequest> requests;
    try {
        // read file
        auto file = fileutils::loadFile("incomingTrustRequests", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(requests);
    } catch (const std::exception& e) {
        JAMI_WARN("[Contacts] error loading trust requests: %s", e.what());
        return;
    }

    for (auto& tr : requests)
        onTrustRequest(tr.first, tr.second.device, tr.second.received, false, std::move(tr.second.payload));
}

bool
ContactList::onTrustRequest(const dht::InfoHash& peer_account, const dht::InfoHash& peer_device, time_t received, bool confirm, std::vector<uint8_t>&& payload)
{
    bool accept = false;
     // Check existing contact
    auto contact = contacts_.find(peer_account);
    if (contact != contacts_.end()) {
        // Banned contact: discard request
        if (contact->second.isBanned())
            return false;
        // Send confirmation
        if (not confirm)
            accept = true;
        // Contact exists, update confirmation status
        if (not contact->second.confirmed) {
            contact->second.confirmed = true;
            callbacks_.contactAdded(peer_account.toString(), true);
            saveContacts();
            //syncDevices();
        }
    } else {
        auto req = trustRequests_.find(peer_account);
        if (req == trustRequests_.end()) {
            // Add trust request
            req = trustRequests_.emplace(peer_account, TrustRequest{
                peer_device, received, std::move(payload)
            }).first;
        } else {
            // Update trust request
            if (received < req->second.received) {
                req->second.device = peer_device;
                req->second.received = received;
                req->second.payload = std::move(payload);
            } else {
                JAMI_DBG("[Contacts] Ignoring outdated trust request from %s", peer_account.toString().c_str());
            }
        }
        saveTrustRequests();
        callbacks_.trustRequest(req->first.toString(), req->second.payload, received);
    }
    return accept;
}


/* trust requests */

std::vector<std::map<std::string, std::string>>
ContactList::getTrustRequests() const
{
    using Map = std::map<std::string, std::string>;
    std::vector<Map> ret;
    ret.reserve(trustRequests_.size());
    for (const auto& r : trustRequests_) {
        ret.emplace_back(Map {
            {DRing::Account::TrustRequest::FROM, r.first.toString()},
            {DRing::Account::TrustRequest::RECEIVED, std::to_string(r.second.received)},
            {DRing::Account::TrustRequest::PAYLOAD, std::string(r.second.payload.begin(), r.second.payload.end())}
        });
    }
    return ret;
}

bool
ContactList::acceptTrustRequest(const dht::InfoHash& from)
{
    // The contact sent us a TR so we are in its contact list
    addContact(from, true);

    auto i = trustRequests_.find(from);
    if (i == trustRequests_.end())
        return false;

    // Clear trust request
    auto treq = std::move(i->second);
    trustRequests_.erase(i);
    saveTrustRequests();

    // Send confirmation
    //account_.get().sendTrustRequestConfirm(from);
    return true;
}

bool
ContactList::discardTrustRequest(const dht::InfoHash& from)
{
    if (trustRequests_.erase(from) > 0) {
        saveTrustRequests();
        return true;
    }
    return false;
}

void
ContactList::loadKnownDevices()
{
    std::map<dht::InfoHash, std::pair<std::string, uint64_t>> knownDevices;
    try {
        // read file
        auto file = fileutils::loadFile("knownDevicesNames", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*)file.data(), file.size());
        oh.get().convert(knownDevices);
    } catch (const std::exception& e) {
        JAMI_WARN("[Contacts] error loading devices: %s", e.what());
        return;
    }

    for (const auto& d : knownDevices) {
        JAMI_DBG("[Contacts] loading known account device %s %s",
                                                                    d.second.first.c_str(),
                                                                    d.first.toString().c_str());
        if (auto crt = tls::CertificateStore::instance().getCertificate(d.first.toString())) {
            if (not foundAccountDevice(crt, d.second.first, clock::from_time_t(d.second.second)))
                JAMI_WARN("[Contacts] can't add device %s", d.first.toString().c_str());
        }
        else {
            JAMI_WARN("[Contacts] can't find certificate for device %s", d.first.toString().c_str());
        }
    }
}

void
ContactList::saveKnownDevices() const
{
    std::ofstream file(path_+DIR_SEPARATOR_STR "knownDevicesNames", std::ios::trunc | std::ios::binary);

    std::map<dht::InfoHash, std::pair<std::string, uint64_t>> devices;
    for (const auto& id : knownDevices_)
        devices.emplace(id.first, std::make_pair(id.second.name, clock::to_time_t(id.second.last_sync)));

    msgpack::pack(file, devices);
}

bool
ContactList::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name, const time_point& updated)
{
    if (not crt)
        return false;

    // match certificate chain
    if (not accountTrust_.verify(*crt)) {
        JAMI_WARN("[Contacts] Found invalid account device: %s", crt->getId().toString().c_str());
        return false;
    }

    // insert device
    auto it = knownDevices_.emplace(crt->getId(), KnownDevice{crt, name, updated});
    if (it.second) {
        JAMI_DBG("[Contacts] Found account device: %s %s",
                                                              name.c_str(),
                                                              crt->getId().toString().c_str());
        tls::CertificateStore::instance().pinCertificate(crt);
        saveKnownDevices();
        callbacks_.devicesChanged();
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_DBG("[Contacts] updating device name: %s %s",
                                                                  name.c_str(),
                                                                  crt->getId().toString().c_str());
            it.first->second.name = name;
            saveKnownDevices();
            callbacks_.devicesChanged();
        }
    }
    return true;
}

bool
ContactList::removeAccountDevice(const dht::InfoHash& device)
{
    if (knownDevices_.erase(device) > 0) {
        saveKnownDevices();
        return true;
    }
    return false;
}

void
ContactList::setAccountDeviceName(const dht::InfoHash& device, const std::string& name)
{
    auto dev = knownDevices_.find(device);
    if (dev != knownDevices_.end()) {
        if (dev->second.name != name) {
            dev->second.name = name;
            saveKnownDevices();
        }
    }
}

DeviceSync
ContactList::getSyncData() const
{
    DeviceSync sync_data;
    sync_data.date = clock::now().time_since_epoch().count();
    //sync_data.device_name = ringDeviceName_;
    sync_data.peers = getContacts();

    static constexpr size_t MAX_TRUST_REQUESTS = 20;
    if (trustRequests_.size() <= MAX_TRUST_REQUESTS)
        for (const auto& req : trustRequests_)
            sync_data.trust_requests.emplace(req.first, TrustRequest{req.second.device, req.second.received, {}});
    else {
        size_t inserted = 0;
        auto req = trustRequests_.lower_bound(dht::InfoHash::getRandom());
        while (inserted++ < MAX_TRUST_REQUESTS) {
            if (req == trustRequests_.end())
                req = trustRequests_.begin();
            sync_data.trust_requests.emplace(req->first, TrustRequest{req->second.device, req->second.received, {}});
            ++req;
        }
    }

    for (const auto& dev : knownDevices_) {
        sync_data.devices_known.emplace(dev.first, dev.second.name);
    }
    return sync_data;
}

bool
ContactList::syncDevice(const dht::InfoHash& device, const time_point& syncDate)
{
    auto it = knownDevices_.find(device);
    if (it == knownDevices_.end()) {
        JAMI_WARN("[Contacts] dropping sync data from unknown device");
        return false;
    }
    if (it->second.last_sync >= syncDate) {
        JAMI_DBG("[Contacts] dropping outdated sync data");
        return false;
    }
    it->second.last_sync = syncDate;
    return true;
}

/*
void
ContactList::onSyncData(DeviceSync&& sync)
{
    auto it = knownDevices_.find(sync.from);
    if (it == knownDevices_.end()) {
        JAMI_WARN("[Contacts] dropping sync data from unknown device");
        return;
    }
    auto sync_date = clock::time_point(clock::duration(sync.date));
    if (it->second.last_sync >= sync_date) {
        JAMI_DBG("[Contacts] dropping outdated sync data");
        return;
    }

    // Sync known devices
    JAMI_DBG("[Contacts] received device sync data (%lu devices, %lu contacts)", sync.devices_known.size(), sync.peers.size());
    for (const auto& d : sync.devices_known) {
        account_.get().findCertificate(d.first, [this,d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (not crt)
                return;
            //std::lock_guard<std::mutex> lock(deviceListMutex_);
            foundAccountDevice(crt, d.second);
        });
    }
    saveKnownDevices();

    // Sync contacts
    for (const auto& peer : sync.peers)
        updateContact(peer.first, peer.second);
    saveContacts();

    // Sync trust requests
    for (const auto& tr : sync.trust_requests)
        onTrustRequest(tr.first, tr.second.device, tr.second.received, false, {});

    it->second.last_sync = sync_date;
}
*/
}
