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
#include "contact_list.h"
#include "logger.h"
#include "jamiaccount.h"
#include "fileutils.h"

#include "manager.h"
#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#endif

#include "account_const.h"

#include <fstream>
#include <gnutls/ocsp.h>

namespace jami {

ContactList::ContactList(const std::string& accountId,
                         const std::shared_ptr<crypto::Certificate>& cert,
                         const std::filesystem::path& path,
                         OnChangeCallback cb)
    : accountId_(accountId)
    , path_(path)
    , callbacks_(std::move(cb))
{
    if (cert) {
        trust_ = std::make_unique<dhtnet::tls::TrustStore>(jami::Manager::instance().certStore(accountId_));
        accountTrust_.add(*cert);
    }
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
ContactList::setCertificateStatus(const std::string& cert_id,
                                  const dhtnet::tls::TrustStore::PermissionStatus status)
{
    if (contacts_.find(dht::InfoHash(cert_id)) != contacts_.end()) {
        JAMI_LOG("[Account {}] [Contacts] Unable to set certificate status for existing contacts {}", accountId_, cert_id);
        return false;
    }
    return trust_->setCertificateStatus(cert_id, status);
}

bool
ContactList::setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                            dhtnet::tls::TrustStore::PermissionStatus status,
                            bool local)
{
    return trust_->setCertificateStatus(cert, status, local);
}

bool
ContactList::addContact(const dht::InfoHash& h, bool confirmed, const std::string& conversationId)
{
    JAMI_WARNING("[Account {}] [Contacts] addContact: {}, conversation: {}", accountId_, h, conversationId);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact {}).first;
    else if (c->second.isActive() and c->second.confirmed == confirmed && c->second.conversationId == conversationId)
        return false;
    c->second.added = std::time(nullptr);
    // NOTE: because we can re-add a contact after removing it
    // we should reset removed (as not removed anymore). This fix isActive()
    // if addContact is called just after removeContact during the same second
    c->second.removed = 0;
    c->second.conversationId = conversationId;
    c->second.confirmed |= confirmed;
    auto hStr = h.toString();
    trust_->setCertificateStatus(hStr, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    saveContacts();
    callbacks_.contactAdded(hStr, c->second.confirmed);
    return true;
}

void
ContactList::updateConversation(const dht::InfoHash& h, const std::string& conversationId)
{
    auto c = contacts_.find(h);
    if (c != contacts_.end() && c->second.conversationId != conversationId) {
        c->second.conversationId = conversationId;
        saveContacts();
    }
}

bool
ContactList::removeContact(const dht::InfoHash& h, bool ban)
{
    std::unique_lock lk(mutex_);
    JAMI_WARNING("[Account {}] [Contacts] removeContact: {} (banned: {})", accountId_, h, ban);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact {}).first;
    c->second.removed = std::time(nullptr);
    c->second.confirmed = false;
    c->second.banned = ban;
    auto uri = h.toString();
    trust_->setCertificateStatus(uri,
                                ban ? dhtnet::tls::TrustStore::PermissionStatus::BANNED
                                    : dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
    if (trustRequests_.erase(h) > 0)
        saveTrustRequests();
    saveContacts();
    lk.unlock();
#ifdef ENABLE_PLUGIN
    auto filename = path_.filename().string();
    jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .cleanChatSubjects(filename, uri);
#endif
    callbacks_.contactRemoved(uri, ban);
    return true;
}

bool
ContactList::removeContactConversation(const dht::InfoHash& h)
{
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        return false;
    c->second.conversationId = "";
    saveContacts();
    return true;
}

std::map<std::string, std::string>
ContactList::getContactDetails(const dht::InfoHash& h) const
{
    const auto c = contacts_.find(h);
    if (c == std::end(contacts_)) {
        JAMI_WARNING("[Account {}] [Contacts] Contact '{}' not found", accountId_, h.to_view());
        return {};
    }

    auto details = c->second.toMap();
    if (not details.empty())
        details["id"] = c->first.toString();

    return details;
}

std::optional<Contact>
ContactList::getContactInfo(const dht::InfoHash& h) const
{
    const auto c = contacts_.find(h);
    if (c == std::end(contacts_)) {
        JAMI_WARNING("[Account {}] [Contacts] Contact '{}' not found", accountId_, h.to_view());
        return {};
    }
    return c->second;
}

const std::map<dht::InfoHash, Contact>&
ContactList::getContacts() const
{
    return contacts_;
}

void
ContactList::setContacts(const std::map<dht::InfoHash, Contact>& contacts)
{
    JAMI_LOG("[Account {}] [Contacts] replacing contact list (old: {} new: {})", accountId_, contacts_.size(), contacts.size());
    contacts_ = contacts;
    saveContacts();
    // Set contacts is used when creating a new device, so just announce new contacts
    for (auto& peer : contacts)
        if (peer.second.isActive())
            callbacks_.contactAdded(peer.first.toString(), peer.second.confirmed);
}

void
ContactList::updateContact(const dht::InfoHash& id, const Contact& contact, bool emit)
{
    if (not id) {
        JAMI_ERROR("[Account {}] [Contacts] updateContact: invalid contact ID", accountId_);
        return;
    }
    bool stateChanged {false};
    auto c = contacts_.find(id);
    if (c == contacts_.end()) {
        // JAMI_DBG("[Contacts] New contact: %s", id.toString().c_str());
        c = contacts_.emplace(id, contact).first;
        stateChanged = c->second.isActive() or c->second.isBanned();
    } else {
        // JAMI_DBG("[Contacts] Updated contact: %s", id.toString().c_str());
        stateChanged = c->second.update(contact);
    }
    if (stateChanged) {
        {
            std::lock_guard lk(mutex_);
            if (trustRequests_.erase(id) > 0)
                saveTrustRequests();
        }
        if (c->second.isActive()) {
            trust_->setCertificateStatus(id.toString(), dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
            if (emit)
                callbacks_.contactAdded(id.toString(), c->second.confirmed);
        } else {
            if (c->second.banned)
                trust_->setCertificateStatus(id.toString(),
                                            dhtnet::tls::TrustStore::PermissionStatus::BANNED);
            if (emit)
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
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(contacts);
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] [Contacts] Error loading contacts: {}", accountId_, e.what());
        return;
    }

    JAMI_WARNING("[Account {}] [Contacts] Loaded {} contacts", accountId_, contacts.size());
    for (auto& peer : contacts)
        updateContact(peer.first, peer.second, false);
}

void
ContactList::saveContacts() const
{
    JAMI_LOG("[Account {}] [Contacts] saving {} contacts", accountId_, contacts_.size());
    std::ofstream file(path_ / "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

void
ContactList::saveTrustRequests() const
{
    // mutex_ MUST BE locked
    std::ofstream file(path_ / "incomingTrustRequests",
                       std::ios::trunc | std::ios::binary);
    msgpack::pack(file, trustRequests_);
}

void
ContactList::loadTrustRequests()
{
    if (!std::filesystem::is_regular_file(fileutils::getFullPath(path_, "incomingTrustRequests")))
        return;
    std::map<dht::InfoHash, TrustRequest> requests;
    try {
        // read file
        auto file = fileutils::loadFile("incomingTrustRequests", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(requests);
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] [Contacts] Error loading trust requests: {}", accountId_, e.what());
        return;
    }

    JAMI_WARNING("[Account {}] [Contacts] Loaded {} contact requests", accountId_, requests.size());
    for (auto& tr : requests)
        onTrustRequest(tr.first,
                       tr.second.device,
                       tr.second.received,
                       false,
                       tr.second.conversationId,
                       std::move(tr.second.payload));
}

bool
ContactList::onTrustRequest(const dht::InfoHash& peer_account,
                            const std::shared_ptr<dht::crypto::PublicKey>& peer_device,
                            time_t received,
                            bool confirm,
                            const std::string& conversationId,
                            std::vector<uint8_t>&& payload)
{
    bool accept = false;
    // Check existing contact
    std::unique_lock lk(mutex_);
    auto contact = contacts_.find(peer_account);
    bool active = false;
    if (contact != contacts_.end()) {
        // Banned contact: discard request
        if (contact->second.isBanned())
            return false;

        if (contact->second.isActive()) {
            active = true;
            // Send confirmation
            if (not confirm)
                accept = true;
            if (not contact->second.confirmed) {
                contact->second.confirmed = true;
                callbacks_.contactAdded(peer_account.toString(), true);
            }
        }
    }
    if (not active) {
        auto req = trustRequests_.find(peer_account);
        if (req == trustRequests_.end()) {
            // Add trust request
            req = trustRequests_
                      .emplace(peer_account,
                               TrustRequest {peer_device, conversationId, received, payload})
                      .first;
        } else {
            // Update trust request
            if (received > req->second.received) {
                req->second.device = peer_device;
                req->second.conversationId = conversationId;
                req->second.received = received;
                req->second.payload = payload;
            } else {
                JAMI_LOG("[Account {}] [Contacts] Ignoring outdated trust request from {}",
                         accountId_,
                         peer_account);
            }
        }
        saveTrustRequests();
    }
    lk.unlock();
    // Note: call JamiAccount's callback to build ConversationRequest anyway
    if (!confirm)
        callbacks_.trustRequest(peer_account.toString(),
                                conversationId,
                                std::move(payload),
                                received);
    else if (active) {
        // Only notify if confirmed + not removed
        callbacks_.onConfirmation(peer_account.toString(), conversationId);
    }
    return accept;
}

/* trust requests */

std::vector<std::map<std::string, std::string>>
ContactList::getTrustRequests() const
{
    using Map = std::map<std::string, std::string>;
    std::vector<Map> ret;
    std::lock_guard lk(mutex_);
    ret.reserve(trustRequests_.size());
    for (const auto& r : trustRequests_) {
        ret.emplace_back(
            Map {{libjami::Account::TrustRequest::FROM, r.first.toString()},
                 {libjami::Account::TrustRequest::RECEIVED, std::to_string(r.second.received)},
                 {libjami::Account::TrustRequest::CONVERSATIONID, r.second.conversationId},
                 {libjami::Account::TrustRequest::PAYLOAD,
                  std::string(r.second.payload.begin(), r.second.payload.end())}});
    }
    return ret;
}

std::map<std::string, std::string>
ContactList::getTrustRequest(const dht::InfoHash& from) const
{
    using Map = std::map<std::string, std::string>;
    std::lock_guard lk(mutex_);
    auto r = trustRequests_.find(from);
    if (r == trustRequests_.end())
        return {};
    return Map {{libjami::Account::TrustRequest::FROM, r->first.toString()},
                {libjami::Account::TrustRequest::RECEIVED, std::to_string(r->second.received)},
                {libjami::Account::TrustRequest::CONVERSATIONID, r->second.conversationId},
                {libjami::Account::TrustRequest::PAYLOAD,
                 std::string(r->second.payload.begin(), r->second.payload.end())}};
}

bool
ContactList::acceptTrustRequest(const dht::InfoHash& from)
{
    // The contact sent us a TR so we are in its contact list
    std::unique_lock lk(mutex_);
    auto i = trustRequests_.find(from);
    if (i == trustRequests_.end())
        return false;
    auto convId =  i->second.conversationId;
    // Clear trust request
    trustRequests_.erase(i);
    saveTrustRequests();
    lk.unlock();
    addContact(from, true, convId);
    return true;
}

void
ContactList::acceptConversation(const std::string& convId, const std::string& deviceId)
{
    if (callbacks_.acceptConversation)
        callbacks_.acceptConversation(convId, deviceId);
}

bool
ContactList::discardTrustRequest(const dht::InfoHash& from)
{
    std::lock_guard lk(mutex_);
    if (trustRequests_.erase(from) > 0) {
        saveTrustRequests();
        return true;
    }
    return false;
}

void
ContactList::loadKnownDevices()
{
    auto& certStore = jami::Manager::instance().certStore(accountId_);
    try {
        // read file
        auto file = fileutils::loadFile("knownDevices", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());

        std::map<dht::PkId, std::pair<std::string, uint64_t>> knownDevices;
        oh.get().convert(knownDevices);
        for (const auto& d : knownDevices) {
            if (auto crt = certStore.getCertificate(d.first.toString())) {
                if (not foundAccountDevice(crt, d.second.first, clock::from_time_t(d.second.second), false))
                    JAMI_WARNING("[Account {}] [Contacts] Unable to add device {}", accountId_, d.first);
            } else {
                JAMI_WARNING("[Account {}] [Contacts] Unable to find certificate for device {}", accountId_,
                          d.first);
            }
        }
        if (not knownDevices.empty()) {
            callbacks_.devicesChanged(knownDevices_);
        }
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] [Contacts] Error loading devices: {}", accountId_, e.what());
        return;
    }
}

void
ContactList::saveKnownDevices() const
{
    std::ofstream file(path_ / "knownDevices", std::ios::trunc | std::ios::binary);

    std::map<dht::PkId, std::pair<std::string, uint64_t>> devices;
    for (const auto& id : knownDevices_) {
        devices.emplace(id.first,
                        std::make_pair(id.second.name, clock::to_time_t(id.second.last_sync)));
    }

    msgpack::pack(file, devices);
}

void
ContactList::foundAccountDevice(const dht::PkId& device,
                                const std::string& name,
                                const time_point& updated)
{
    // insert device
    auto it = knownDevices_.emplace(device, KnownDevice {{}, name, updated});
    if (it.second) {
        JAMI_LOG("[Account {}] [Contacts] Found account device: {} {}", accountId_, name, device);
        saveKnownDevices();
        callbacks_.devicesChanged(knownDevices_);
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_LOG("[Account {}] [Contacts] Updating device name: {} {}", accountId_,
                     name, device);
            it.first->second.name = name;
            saveKnownDevices();
            callbacks_.devicesChanged(knownDevices_);
        }
    }
}

bool
ContactList::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                const std::string& name,
                                const time_point& updated,
                                bool notify)
{
    if (not crt)
        return false;

    auto id = crt->getLongId();

    // match certificate chain
    auto verifyResult = accountTrust_.verify(*crt);
    if (not verifyResult) {
        JAMI_WARNING("[Account {}] [Contacts] Found invalid account device: {:s}: {:s}",
                  accountId_, id, verifyResult.toString());
        return false;
    }

    // insert device
    auto it = knownDevices_.emplace(id, KnownDevice {crt, name, updated});
    if (it.second) {
        JAMI_LOG("[Account {}] [Contacts] Found account device: {} {}", accountId_, name, id);
        jami::Manager::instance().certStore(accountId_).pinCertificate(crt);
        if (crt->ocspResponse) {
            unsigned int status = crt->ocspResponse->getCertificateStatus();
            if (status == GNUTLS_OCSP_CERT_REVOKED) {
                JAMI_ERROR("[Account {}] Certificate {} has revoked OCSP status", accountId_, id);
                trust_->setCertificateStatus(crt, dhtnet::tls::TrustStore::PermissionStatus::BANNED, false);
            }
        }
        if (notify) {
            saveKnownDevices();
            callbacks_.devicesChanged(knownDevices_);
        }
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_LOG("[Account {}] [Contacts] updating device name: {} {}", accountId_, name, id);
            it.first->second.name = name;
            if (notify) {
                saveKnownDevices();
                callbacks_.devicesChanged(knownDevices_);
            }
        }
    }
    return true;
}

bool
ContactList::removeAccountDevice(const dht::PkId& device)
{
    if (knownDevices_.erase(device) > 0) {
        saveKnownDevices();
        return true;
    }
    return false;
}

void
ContactList::setAccountDeviceName(const dht::PkId& device, const std::string& name)
{
    auto dev = knownDevices_.find(device);
    if (dev != knownDevices_.end()) {
        if (dev->second.name != name) {
            dev->second.name = name;
            saveKnownDevices();
            callbacks_.devicesChanged(knownDevices_);
        }
    }
}

std::string
ContactList::getAccountDeviceName(const dht::PkId& device) const
{
    auto dev = knownDevices_.find(device);
    if (dev != knownDevices_.end()) {
        return dev->second.name;
    }
    return {};
}

DeviceSync
ContactList::getSyncData() const
{
    DeviceSync sync_data;
    sync_data.date = clock::now().time_since_epoch().count();
    // sync_data.device_name = deviceName_;
    sync_data.peers = getContacts();

    static constexpr size_t MAX_TRUST_REQUESTS = 20;
    std::lock_guard lk(mutex_);
    if (trustRequests_.size() <= MAX_TRUST_REQUESTS)
        for (const auto& req : trustRequests_)
            sync_data.trust_requests.emplace(req.first,
                                             TrustRequest {req.second.device,
                                                           req.second.conversationId,
                                                           req.second.received,
                                                           {}});
    else {
        size_t inserted = 0;
        auto req = trustRequests_.lower_bound(dht::InfoHash::getRandom());
        while (inserted++ < MAX_TRUST_REQUESTS) {
            if (req == trustRequests_.end())
                req = trustRequests_.begin();
            sync_data.trust_requests.emplace(req->first,
                                             TrustRequest {req->second.device,
                                                           req->second.conversationId,
                                                           req->second.received,
                                                           {}});
            ++req;
        }
    }

    for (const auto& dev : knownDevices_) {
        if (!dev.second.certificate) {
            JAMI_WARNING("[Account {}] [Contacts] No certificate found for {}", accountId_, dev.first);
            continue;
        }
        sync_data.devices.emplace(dev.second.certificate->getLongId(),
                                  KnownDeviceSync {dev.second.name,
                                                   dev.second.certificate->getId()});
    }
    return sync_data;
}

bool
ContactList::syncDevice(const dht::PkId& device, const time_point& syncDate)
{
    auto it = knownDevices_.find(device);
    if (it == knownDevices_.end()) {
        JAMI_WARNING("[Account {}] [Contacts] Dropping sync data from unknown device", accountId_);
        return false;
    }
    if (it->second.last_sync >= syncDate) {
        JAMI_LOG("[Account {}] [Contacts] Dropping outdated sync data", accountId_);
        return false;
    }
    it->second.last_sync = syncDate;
    return true;
}

} // namespace jami
