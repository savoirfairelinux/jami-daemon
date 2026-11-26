/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include <string_view>
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
    std::lock_guard lock(devicesMutex_);
    saveKnownDevices();
}

bool
ContactList::setCertificateStatus(const std::string& cert_id, const dhtnet::tls::TrustStore::PermissionStatus status)
{
    std::unique_lock lk(mutex_);
    if (contacts_.find(dht::InfoHash(cert_id)) != contacts_.end()) {
        JAMI_LOG("[Account {}] [Contacts] Unable to set certificate status for existing contacts {}",
                 accountId_,
                 cert_id);
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
    std::lock_guard notificationLock(notificationMutex_);
    std::unique_lock lk(mutex_);
    JAMI_WARNING("[Account {}] [Contacts] addContact: {}, conversation: {}", accountId_, h, conversationId);
    auto c = contacts_.find(h);
    Contact next;
    if (c != contacts_.end() && c->second.isActive()) {
        if (c->second.confirmed == confirmed && c->second.conversationId == conversationId)
            return false;
        // conversationId is not reconciled by Contact::merge, so it is written directly.
        next = c->second;
        next.confirmed |= confirmed;
        next.conversationId = conversationId;
    } else {
        Contact addition;
        addition.added = nextEventTime(c == contacts_.end() ? nullptr : &c->second);
        addition.confirmed = confirmed;
        addition.conversationId = conversationId;
        next = Contact::merge(c == contacts_.end() ? addition : c->second, addition);
    }
    if (!storeContactsLocked({{h, next}}))
        return false;
    auto hStr = h.toString();
    updateTrustStatus(h, next);
    lk.unlock();
    callbacks_.contactAdded(hStr, next.confirmed);
    return true;
}

bool
ContactList::updateConversation(const dht::InfoHash& h, const std::string& conversationId)
{
    std::lock_guard lk(mutex_);
    auto c = contacts_.find(h);
    if (c != contacts_.end() && c->second.conversationId != conversationId) {
        c->second.conversationId = conversationId;
        saveContacts();
        return true;
    }
    return false;
}

bool
ContactList::removeContact(const dht::InfoHash& h, bool ban)
{
    std::lock_guard notificationLock(notificationMutex_);
    std::unique_lock lk(mutex_);
    JAMI_WARNING("[Account {}] [Contacts] removeContact: {} (banned: {})", accountId_, h, ban);
    auto c = contacts_.find(h);
    Contact removal;
    removal.removed = nextEventTime(c == contacts_.end() ? nullptr : &c->second);
    removal.banned = ban;
    // A ban keeps the conversation so that unblocking reuses it; a plain removal unlinks it.
    if (ban && c != contacts_.end())
        removal.conversationId = c->second.conversationId;
    const auto next = Contact::merge(c == contacts_.end() ? removal : c->second, removal);
    if (!storeContactsLocked({{h, next}}))
        return false;
    auto uri = h.toString();
    updateTrustStatus(h, next);
    if (trustRequests_.erase(h) > 0)
        saveTrustRequests();
    lk.unlock();
#ifdef ENABLE_PLUGIN
    auto filename = path_.filename().string();
    jami::Manager::instance().getJamiPluginManager().getChatServicesManager().cleanChatSubjects(filename, uri);
#endif
    callbacks_.contactRemoved(uri, ban);
    return true;
}

bool
ContactList::removeContactConversation(const dht::InfoHash& h)
{
    std::unique_lock lk(mutex_);
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
    std::unique_lock lk(mutex_);
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
    std::lock_guard lk(mutex_);
    const auto c = contacts_.find(h);
    if (c == std::end(contacts_)) {
        JAMI_WARNING("[Account {}] [Contacts] Contact '{}' not found", accountId_, h.to_view());
        return {};
    }
    return c->second;
}

std::optional<Metadata::Change>
ContactList::setContactMetadata(const dht::InfoHash& h, const std::string& writer, const Metadata::Values& updates)
{
    std::lock_guard notificationLock(notificationMutex_);
    std::unique_lock lk(mutex_);
    const auto c = contacts_.find(h);
    if (c == contacts_.end() || !c->second.isActive()) {
        JAMI_WARNING("[Account {}] Cannot update metadata for unknown or inactive contact {}", accountId_, h);
        return std::nullopt;
    }
    auto next = c->second;
    auto change = next.metadata.update(writer, updates);
    if (!change)
        return std::nullopt;
    auto changes = storeContactsLocked({{h, std::move(next)}});
    if (!changes)
        return std::nullopt;
    lk.unlock();
    notifyContactChanges(*changes);
    return change;
}

bool
ContactList::mergeContacts(const std::map<dht::InfoHash, Contact>& copies)
{
    std::lock_guard notificationLock(notificationMutex_);
    std::unique_lock lk(mutex_);
    std::map<dht::InfoHash, Contact> next;
    for (const auto& [id, copy] : copies) {
        if (!id || !copy.isMergeable()) {
            JAMI_WARNING("[Account {}] [Contacts] Ignoring invalid contact {}", accountId_, id);
            continue;
        }
        const auto c = contacts_.find(id);
        next.emplace(id, Contact::merge(c == contacts_.end() ? copy : c->second, copy));
    }
    auto changes = storeContactsLocked(next);
    if (!changes)
        return false;
    bool trustRequestsChanged = false;
    for (const auto& change : *changes)
        if (change.lifecycleChanged())
            trustRequestsChanged |= trustRequests_.erase(change.id) > 0;
    if (trustRequestsChanged)
        saveTrustRequests();
    lk.unlock();
    notifyContactChanges(*changes);
    return true;
}

TimePoint
ContactList::nextEventTime(const Contact* contact)
{
    const auto now = nowMs();
    if (!contact)
        return now;
    // Use a later second than the previous event: daemons that store seconds only (older than commit
    // 5d4ce4c0e) would otherwise see both events at the same time and could keep either.
    const auto previous = std::chrono::floor<std::chrono::seconds>(std::max(contact->added, contact->removed));
    return std::max(now, TimePoint(previous + std::chrono::seconds(1)));
}

std::optional<std::vector<ContactList::ContactChange>>
ContactList::storeContactsLocked(const std::map<dht::InfoHash, Contact>& next)
{
    std::vector<ContactChange> changes;
    for (const auto& [id, contact] : next) {
        const auto c = contacts_.find(id);
        if (c != contacts_.end() && c->second == contact)
            continue;
        changes.push_back({id, c == contacts_.end() ? std::nullopt : std::optional(c->second), contact});
    }
    if (changes.empty())
        return changes;
    for (const auto& change : changes)
        contacts_[change.id] = change.after;
    if (!saveContactsAtomically()) {
        for (const auto& change : changes) {
            if (change.before)
                contacts_[change.id] = *change.before;
            else
                contacts_.erase(change.id);
        }
        return std::nullopt;
    }
    return changes;
}

bool
ContactList::ContactChange::lifecycleChanged() const
{
    // Compare events, not only the derived state: a newer addition of an active contact or a removal
    // of an unknown peer must be reported like the local action that produced it.
    return !before || before->added != after.added || before->removed != after.removed
           || after.hasDifferentState(*before);
}

void
ContactList::notifyContactChanges(const std::vector<ContactChange>& changes)
{
    for (const auto& change : changes) {
        const auto uri = change.id.toString();
        if (change.lifecycleChanged()) {
            updateTrustStatus(change.id, change.after);
            if (change.after.isActive())
                callbacks_.contactAdded(uri, change.after.confirmed);
            else
                callbacks_.contactRemoved(uri, change.after.banned);
        }
        // Removal clears metadata silently: ContactRemoved already implies it.
        if (!change.after.isActive() || (change.before && change.before->metadata == change.after.metadata))
            continue;
        if (callbacks_.metadataChanged)
            callbacks_.metadataChanged(change.id, change.after);
        auto values = change.after.metadata.values();
        const auto previous = change.before && change.before->isActive() ? change.before->metadata.values()
                                                                         : Metadata::Values {};
        if (values != previous)
            emitSignal<libjami::ConfigurationSignal::ContactMetadataChanged>(accountId_, uri, values);
    }
}

std::map<dht::InfoHash, Contact>
ContactList::getContacts() const
{
    std::lock_guard lk(mutex_);
    return contacts_;
}

void
ContactList::setContacts(const std::map<dht::InfoHash, Contact>& contacts)
{
    JAMI_LOG("[Account {}] [Contacts] replacing contact list (old: {} new: {})",
             accountId_,
             contacts_.size(),
             contacts.size());
    contacts_.clear();
    for (const auto& [id, contact] : contacts)
        if (id && contact.isMergeable())
            updateTrustStatus(id, contacts_[id] = Contact::merge(contact, contact));
    saveContacts();
    const auto stored = contacts_;
    // Set contacts is used when creating a new device, so just announce new contacts
    for (auto& peer : stored)
        if (peer.second.isActive())
            callbacks_.contactAdded(peer.first.toString(), peer.second.confirmed);
}

std::map<dht::InfoHash, Contact>
ContactList::contactsFromPath(const std::filesystem::path& path)
{
    std::map<dht::InfoHash, Contact> contacts;
    try {
        std::lock_guard fileLock(dhtnet::fileutils::getFileLock(path / "contacts"));
        auto file = fileutils::loadFile("contacts", path);
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(contacts);
    } catch (const std::exception& e) {
        JAMI_WARNING("[Contacts] Error loading contacts from {}: {}", path.string(), e.what());
    }
    return contacts;
}

void
ContactList::loadContacts()
{
    auto contacts = contactsFromPath(path_);
    JAMI_WARNING("[Account {}] [Contacts] Loaded {} contacts", accountId_, contacts.size());
    std::lock_guard lk(mutex_);
    for (const auto& [id, stored] : contacts) {
        if (!id || !stored.isMergeable()) {
            JAMI_WARNING("[Account {}] [Contacts] Ignoring invalid stored contact {}", accountId_, id);
            continue;
        }
        // Merging a contact with itself restores the invariants of data written by older versions.
        updateTrustStatus(id, contacts_[id] = Contact::merge(stored, stored));
    }
}

void
ContactList::updateTrustStatus(const dht::InfoHash& id, const Contact& contact)
{
    // A list built without an account certificate has no trust store.
    if (!trust_)
        return;
    using Status = dhtnet::tls::TrustStore::PermissionStatus;
    trust_->setCertificateStatus(id.toString(),
                                 contact.isActive()   ? Status::ALLOWED
                                 : contact.isBanned() ? Status::BANNED
                                                      : Status::UNDEFINED);
}

void
ContactList::saveContacts() const
{
    JAMI_LOG("[Account {}] [Contacts] saving {} contacts", accountId_, contacts_.size());
    std::lock_guard fileLock(dhtnet::fileutils::getFileLock(path_ / "contacts"));
    std::ofstream file(path_ / "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

bool
ContactList::saveContactsAtomically() const
{
    JAMI_LOG("[Account {}] [Contacts] saving {} contacts", accountId_, contacts_.size());
    std::lock_guard fileLock(dhtnet::fileutils::getFileLock(path_ / "contacts"));
    const auto staging = path_ / "contacts.new";
    std::ofstream file(staging, std::ios::trunc | std::ios::binary);
    if (!file) {
        JAMI_ERROR("[Account {}] Cannot open contact staging file {}", accountId_, staging);
        return false;
    }
    std::error_code ec;
    std::filesystem::permissions(staging, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
    if (!ec)
        msgpack::pack(file, contacts_);
    file.close();
    if (file && !ec)
        std::filesystem::rename(staging, path_ / "contacts", ec);
    if (!file || ec) {
        JAMI_ERROR("[Account {}] Cannot save contacts: {}", accountId_, ec ? ec.message() : "write failed");
        std::error_code cleanupError;
        std::filesystem::remove(staging, cleanupError);
        if (cleanupError)
            JAMI_WARNING("Cannot remove contact staging file {}: {}", staging, cleanupError.message());
        return false;
    }
    return true;
}

void
ContactList::saveTrustRequests() const
{
    // mutex_ MUST BE locked
    std::ofstream file(path_ / "incomingTrustRequests", std::ios::trunc | std::ios::binary);
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
                       std::move(tr.second.payload),
                       tr.second.invited);
}

bool
ContactList::onTrustRequest(const dht::InfoHash& peer_account,
                            const std::shared_ptr<dht::crypto::PublicKey>& peer_device,
                            TimePoint received,
                            bool confirm,
                            const std::string& conversationId,
                            std::vector<uint8_t>&& payload,
                            TimePoint invited)
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
                saveContacts();
                callbacks_.contactAdded(peer_account.toString(), true);
            }
        }
    }
    if (not active) {
        auto req = trustRequests_.find(peer_account);
        if (req == trustRequests_.end()) {
            // Add trust request
            req = trustRequests_
                      .emplace(peer_account, TrustRequest {peer_device, conversationId, received, payload, invited})
                      .first;
        } else {
            auto incomingTimestamp = invited != TimePoint {} ? invited : received;
            auto existingTimestamp = req->second.invited != TimePoint {} ? req->second.invited : req->second.received;

            if (incomingTimestamp > existingTimestamp) {
                req->second.device = peer_device;
                req->second.conversationId = conversationId;
                req->second.received = received;
                req->second.payload = payload;
                req->second.invited = invited;
            } else {
                JAMI_LOG("[Account {}] [Contacts] Ignoring outdated trust request from {}", accountId_, peer_account);
            }
        }
        saveTrustRequests();
    }
    lk.unlock();
    // Note: call JamiAccount's callback to build ConversationRequest anyway
    if (!confirm)
        callbacks_.trustRequest(peer_account.toString(), conversationId, std::move(payload), received, invited);
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
                 {libjami::Account::TrustRequest::RECEIVED, std::to_string(toSecondsSinceEpoch(r.second.received))},
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
                {libjami::Account::TrustRequest::RECEIVED, std::to_string(toSecondsSinceEpoch(r->second.received))},
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
    auto convId = i->second.conversationId;
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
    return discardTrustRequest(from, {}, TimePoint::max());
}

bool
ContactList::discardTrustRequest(const dht::InfoHash& from, const std::string& conversationId, TimePoint declined)
{
    std::lock_guard lk(mutex_);
    auto request = trustRequests_.find(from);
    if (request == trustRequests_.end() || (!conversationId.empty() && request->second.conversationId != conversationId))
        return false;
    const auto invited = request->second.invited != TimePoint {} ? request->second.invited : request->second.received;
    if (invited > declined)
        return false;
    trustRequests_.erase(request);
    saveTrustRequests();
    return true;
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

        std::map<dht::PkId, KnownDeviceData> knownDevices;
        oh.get().convert(knownDevices);
        for (const auto& d : knownDevices) {
            if (auto crt = certStore.getCertificate(d.first.toString())) {
                auto lastSync = clock::time_point(std::chrono::milliseconds(d.second.lastSyncMs));
                if (not foundAccountDevice(crt, d.second.name, lastSync, false))
                    JAMI_WARNING("[Account {}] [Contacts] Unable to add device {}", accountId_, d.first);
            } else {
                JAMI_WARNING("[Account {}] [Contacts] Unable to find certificate for device {}", accountId_, d.first);
            }
        }
        if (not knownDevices.empty()) {
            callbacks_.devicesChanged(getKnownDevices());
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

    std::map<dht::PkId, KnownDeviceData> devices;
    for (const auto& id : knownDevices_) {
        auto lastSyncMs = std::chrono::duration_cast<std::chrono::milliseconds>(id.second.last_sync.time_since_epoch())
                              .count();
        devices.emplace(id.first, KnownDeviceData {id.second.name, lastSyncMs});
    }

    msgpack::pack(file, devices);
}

void
ContactList::foundAccountDevice(const dht::PkId& device, const std::string& name, const time_point& updated)
{
    std::unique_lock lock(devicesMutex_);
    // insert device
    auto it = knownDevices_.emplace(device, KnownDevice {{}, name, updated});
    if (it.second) {
        JAMI_LOG("[Account {}] [Contacts] Found account device: {} {}", accountId_, name, device);
        saveKnownDevices();
        lock.unlock();
        callbacks_.devicesChanged(getKnownDevices());
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_LOG("[Account {}] [Contacts] Updating device name: {} {}", accountId_, name, device);
            it.first->second.name = name;
            saveKnownDevices();
            lock.unlock();
            callbacks_.devicesChanged(getKnownDevices());
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
                     accountId_,
                     id,
                     verifyResult.toString());
        return false;
    }

    // insert device
    std::unique_lock lock(devicesMutex_);
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
            lock.unlock();
            callbacks_.devicesChanged(getKnownDevices());
        }
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_LOG("[Account {}] [Contacts] updating device name: {} {}", accountId_, name, id);
            it.first->second.name = name;
            if (notify) {
                saveKnownDevices();
                lock.unlock();
                callbacks_.devicesChanged(getKnownDevices());
            }
        }
    }
    return true;
}

bool
ContactList::removeAccountDevice(const dht::PkId& device)
{
    std::lock_guard lock(devicesMutex_);
    if (knownDevices_.erase(device) > 0) {
        saveKnownDevices();
        return true;
    }
    return false;
}

void
ContactList::setAccountDeviceName(const dht::PkId& device, const std::string& name)
{
    std::unique_lock lock(devicesMutex_);
    auto dev = knownDevices_.find(device);
    if (dev != knownDevices_.end()) {
        if (dev->second.name != name) {
            dev->second.name = name;
            saveKnownDevices();
            lock.unlock();
            callbacks_.devicesChanged(getKnownDevices());
        }
    }
}

std::string
ContactList::getAccountDeviceName(const dht::PkId& device) const
{
    std::lock_guard lock(devicesMutex_);
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
                                                           {},
                                                           req.second.invited});
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
                                                           {},
                                                           req->second.invited});
            ++req;
        }
    }

    for (const auto& dev : getKnownDevices()) {
        if (!dev.second.certificate) {
            JAMI_WARNING("[Account {}] [Contacts] No certificate found for {}", accountId_, dev.first);
            continue;
        }
        sync_data.devices.emplace(dev.second.certificate->getLongId(), KnownDeviceSync {dev.second.name});
    }
    return sync_data;
}

std::map<dht::PkId, KnownDevice>
ContactList::getKnownDevices() const
{
    std::lock_guard lock(devicesMutex_);
    return knownDevices_;
}

bool
ContactList::recordDeviceSync(const dht::PkId& device, const time_point& receivedAt)
{
    std::unique_lock lock(devicesMutex_);
    auto it = knownDevices_.find(device);
    if (it == knownDevices_.end() || it->second.last_sync >= receivedAt)
        return false;
    it->second.last_sync = receivedAt;
    saveKnownDevices();
    auto devices = knownDevices_;
    lock.unlock();
    callbacks_.devicesChanged(devices);
    return true;
}

} // namespace jami
