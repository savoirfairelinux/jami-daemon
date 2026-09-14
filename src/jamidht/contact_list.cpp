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
#include <algorithm>
#include <tuple>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
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
    , accountCertificate_(cert)
    , accountUri_(cert ? cert->getId().toString() : "")
    , accountCertificateId_(cert ? cert->getLongId().toString() : "")
    , callbacks_(std::move(cb))
{
    if (cert) {
        trust_ = std::make_unique<dhtnet::tls::TrustStore>(jami::Manager::instance().certStore(accountId_));
        accountTrust_.add(*cert);
        // Device identity already authenticates this authority. Use the normal
        // TrustStore verifier (including its CRLs), independently of contacts.
        trust_->setCertificateStatus(cert, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED, false);
    }
}

ContactList::~ContactList() {}

bool
ContactList::load()
{
    auto selfChanged = loadContacts();
    loadTrustRequests();
    loadKnownDevices();
    return selfChanged;
}

void
ContactList::save()
{
    saveContacts();
    saveTrustRequests();
    saveKnownDevices();
}

bool
ContactList::setCertificateStatus(const std::string& cert_id, const dhtnet::tls::TrustStore::PermissionStatus status)
{
    if (status == dhtnet::tls::TrustStore::PermissionStatus::BANNED
        && !accountUri_.empty() && (cert_id == accountUri_ || cert_id == accountCertificateId_)) {
        JAMI_WARNING("[Account {}] Refusing to ban the account's own identity", accountId_);
        return false;
    }
    std::unique_lock lk(mutex_);
    if (contacts_.find(dht::InfoHash(cert_id)) != contacts_.end()) {
        JAMI_LOG("[Account {}] [Contacts] Unable to set certificate status for existing contacts {}",
                 accountId_,
                 cert_id);
        return false;
    }
    if (auto cert = jami::Manager::instance().certStore(accountId_).getCertificate(cert_id))
        return setCertificateStatus(cert, status, false);
    return trust_->setCertificateStatus(cert_id, status);
}

bool
ContactList::setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                                  dhtnet::tls::TrustStore::PermissionStatus status,
                                  bool local)
{
    if (!cert)
        return false;
    if (status == dhtnet::tls::TrustStore::PermissionStatus::BANNED
        && cert->getLongId().toString() == accountCertificateId_) {
        JAMI_WARNING("[Account {}] Refusing to ban the account's own identity", accountId_);
        return false;
    }
    return trust_->setCertificateStatus(cert, status, local);
}

bool
ContactList::addContact(const dht::InfoHash& h, bool confirmed, const std::string& conversationId)
{
    std::unique_lock lk(mutex_);
    JAMI_WARNING("[Account {}] [Contacts] addContact: {}, conversation: {}", accountId_, h, conversationId);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact {}).first;
    else if (c->second.isActive() and c->second.confirmed == confirmed && c->second.conversationId == conversationId)
        return false;
    c->second.added = nowMs();
    // NOTE: because we can re-add a contact after removing it
    // we should reset removed (as not removed anymore). This fix isActive()
    // if addContact is called just after removeContact within the same instant
    c->second.removed = TimePoint {};
    c->second.conversationId = conversationId;
    c->second.confirmed |= confirmed;
    auto hStr = h.toString();
    trust_->setCertificateStatus(hStr, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
    saveContacts();
    lk.unlock();
    callbacks_.contactAdded(hStr, c->second.confirmed);
    return true;
}

bool
ContactList::updateConversation(const dht::InfoHash& h, const std::string& conversationId, bool added)
{
    std::lock_guard lk(mutex_);
    auto c = contacts_.find(h);
    if (c != contacts_.end() && c->second.conversationId != conversationId) {
        c->second.conversationId = conversationId;
        if (added) {
            c->second.added = nowMs();
        }
        saveContacts();
        return true;
    }
    return false;
}

bool
ContactList::removeContact(const dht::InfoHash& h, bool ban)
{
    if (ban && h.toString() == accountUri_) {
        JAMI_WARNING("[Account {}] [Contacts] Refusing to block the account's own identity", accountId_);
        return false;
    }
    std::unique_lock lk(mutex_);
    JAMI_WARNING("[Account {}] [Contacts] removeContact: {} (banned: {})", accountId_, h, ban);
    auto c = contacts_.find(h);
    if (c == contacts_.end())
        c = contacts_.emplace(h, Contact {}).first;
    c->second.removed = nowMs();
    c->second.confirmed = false;
    c->second.banned = ban;
    c->second.conversationId = "";
    auto uri = h.toString();
    if (uri != accountUri_)
        trust_->setCertificateStatus(uri,
                                     ban ? dhtnet::tls::TrustStore::PermissionStatus::BANNED
                                         : dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
    if (trustRequests_.erase(h) > 0)
        saveTrustRequests();
    saveContacts();
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

const std::map<dht::InfoHash, Contact>&
ContactList::getContacts() const
{
    return contacts_;
}

void
ContactList::setContacts(const std::map<dht::InfoHash, Contact>& contacts)
{
    JAMI_LOG("[Account {}] [Contacts] replacing contact list (old: {} new: {})",
             accountId_,
             contacts_.size(),
             contacts.size());
    ingestContacts(contacts, true, false);
    // Set contacts is used when creating a new device, so just announce new contacts
    for (auto& peer : contacts)
        if (peer.second.isActive())
            callbacks_.contactAdded(peer.first.toString(), peer.second.confirmed);
}

void
ContactList::updateContact(const dht::InfoHash& id, const Contact& contact, bool emit)
{
    updateContacts({{id, contact}}, emit);
}

bool
ContactList::normalizeSelfContact(const dht::InfoHash& account, std::map<dht::InfoHash, Contact>& contacts)
{
    auto it = contacts.find(account);
    if (!account || it == contacts.end() || !it->second.isBanned())
        return false;
    auto& contact = it->second;
    // Older daemons compare seconds. A millisecond-only bump could tie their
    // original ban, so advance past its whole second as well as its ms stamp.
    if (contact.removed > TimePoint::max() - std::chrono::seconds(1))
        throw std::overflow_error("Self-contact removal timestamp cannot be advanced");
    auto nextSecond = std::chrono::time_point_cast<std::chrono::seconds>(contact.removed)
                      + std::chrono::seconds(1);
    contact.removed = std::max(nowMs(), TimePoint(nextSecond));
    contact.banned = false;
    return true;
}

bool
ContactList::updateContacts(const std::map<dht::InfoHash, Contact>& contacts, bool emit)
{
    return ingestContacts(contacts, false, emit);
}

bool
ContactList::ingestContacts(const std::map<dht::InfoHash, Contact>& contacts, bool replace, bool emit)
{
    auto equal = [](const Contact& a, const Contact& b) {
        return std::tie(a.added, a.removed, a.confirmed, a.banned, a.conversationId)
               == std::tie(b.added, b.removed, b.confirmed, b.banned, b.conversationId);
    };
    std::unique_lock lk(mutex_);
    auto next = replace ? std::map<dht::InfoHash, Contact> {} : contacts_;
    for (const auto& [id, contact] : contacts) {
        if (!id) {
            JAMI_WARNING("[Account {}] [Contacts] Ignoring an invalid contact ID", accountId_);
            continue;
        }
        auto [it, inserted] = next.emplace(id, contact);
        if (!inserted)
            it->second.update(contact);
    }
    const dht::InfoHash self(accountUri_);
    normalizeSelfContact(self, next);
    auto oldSelf = contacts_.find(self);
    auto newSelf = next.find(self);
    bool selfChanged = (oldSelf == contacts_.end()) != (newSelf == next.end())
                       || (oldSelf != contacts_.end() && newSelf != next.end()
                           && !equal(oldSelf->second, newSelf->second));
    bool changed = next.size() != contacts_.size();
    if (!changed)
        for (const auto& [id, contact] : next) {
            auto old = contacts_.find(id);
            if (old == contacts_.end() || !equal(old->second, contact)) {
                changed = true;
                break;
            }
        }
    // Commit the complete batch before changing live permissions or notifying
    // anyone. This also avoids a partial contacts file during startup repair.
    if (changed || replace)
        saveContacts(next);
    auto previous = contacts_;
    if (replace)
        contacts_ = std::move(next);
    else if (changed)
        for (const auto& [id, contact] : next) {
            auto [it, inserted] = contacts_.emplace(id, contact);
            if (!inserted && !equal(it->second, contact))
                it->second = contact;
        }
    std::vector<std::pair<dht::InfoHash, Contact>> notifications;
    for (const auto& [id, contact] : contacts_) {
        auto old = previous.find(id);
        bool stateChanged = old == previous.end() ? contact.isActive() || contact.isBanned()
                                                  : contact.hasDifferentState(old->second);
        bool selfTombstone = id == self && !contact.isActive() && !contact.banned;
        if (selfTombstone) {
            // Clear only the invalid owner permission, never its device/CA
            // permissions or revocations. No ContactRemoved: that deletes notes.
            auto banned = trust_->getCertificatesByStatus(dhtnet::tls::TrustStore::PermissionStatus::BANNED);
            bool restoreOwner = false;
            for (const auto& ownerId : {accountUri_, accountCertificateId_})
                if (std::find(banned.begin(), banned.end(), ownerId) != banned.end())
                    restoreOwner = true;
            if (restoreOwner) {
                trust_->setCertificateStatus(accountCertificateId_, dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED);
                trust_->setCertificateStatus(accountCertificate_, dhtnet::tls::TrustStore::PermissionStatus::ALLOWED, false);
            }
        }
        if (!stateChanged)
            continue;
        if (!selfTombstone && trustRequests_.erase(id) > 0)
            saveTrustRequests();
        if (contact.isActive())
            trust_->setCertificateStatus(id.toString(), dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
        else if (contact.isBanned())
            trust_->setCertificateStatus(id.toString(), dhtnet::tls::TrustStore::PermissionStatus::BANNED);
        if (emit && !selfTombstone)
            notifications.emplace_back(id, contact);
    }
    lk.unlock();
    for (const auto& [id, contact] : notifications) {
        if (contact.isActive())
            callbacks_.contactAdded(id.toString(), contact.confirmed);
        else
            callbacks_.contactRemoved(id.toString(), contact.banned);
    }
    return selfChanged;
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

bool
ContactList::loadContacts()
{
    auto contacts = contactsFromPath(path_);
    JAMI_WARNING("[Account {}] [Contacts] Loaded {} contacts", accountId_, contacts.size());
    return ingestContacts(contacts, false, false);
}

void
ContactList::saveContacts() const
{
    saveContacts(contacts_);
}

void
ContactList::saveContacts(const std::map<dht::InfoHash, Contact>& contacts) const
{
    JAMI_LOG("[Account {}] [Contacts] saving {} contacts", accountId_, contacts.size());
    std::lock_guard fileLock(dhtnet::fileutils::getFileLock(path_ / "contacts"));
    auto staging = path_ / "contacts.new";
    try {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(staging, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, contacts);
        file.flush();
        file.close();
#ifdef _WIN32
        auto handle = CreateFileW(staging.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(GetLastError(), std::system_category(), "Opening contacts for flush");
        auto ok = FlushFileBuffers(handle);
        auto error = GetLastError();
        CloseHandle(handle);
        if (!ok)
            throw std::system_error(error, std::system_category(), "Flushing contacts");
        if (!MoveFileExW(staging.c_str(), (path_ / "contacts").c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(GetLastError(), std::system_category(), "Replacing contacts");
#else
        int fd = open(staging.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "Opening contacts for flush");
        auto error = fsync(fd) == 0 ? 0 : errno;
        if (close(fd) != 0 && !error)
            error = errno;
        if (error)
            throw std::system_error(error, std::generic_category(), "Flushing contacts");
        std::filesystem::rename(staging, path_ / "contacts");
        fd = open(path_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "Opening contacts directory");
        error = fsync(fd) == 0 ? 0 : errno;
        close(fd);
        if (error)
            throw std::system_error(error, std::generic_category(), "Flushing contacts directory");
#endif
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(staging, ec);
        throw;
    }
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
            auto existingTimestamp = req->second.invited != TimePoint {} ? req->second.invited
                                                             : req->second.received;

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
        callbacks_.trustRequest(peer_account.toString(),
                                conversationId,
                                std::move(payload),
                                received,
                                invited);
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
    // insert device
    auto it = knownDevices_.emplace(device, KnownDevice {{}, name, updated});
    if (it.second) {
        JAMI_LOG("[Account {}] [Contacts] Found account device: {} {}", accountId_, name, device);
        saveKnownDevices();
        callbacks_.devicesChanged(knownDevices_);
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_LOG("[Account {}] [Contacts] Updating device name: {} {}", accountId_, name, device);
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
                     accountId_,
                     id,
                     verifyResult.toString());
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
    std::lock_guard lk(mutex_);
    sync_data.peers = getContacts();

    static constexpr size_t MAX_TRUST_REQUESTS = 20;
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

    for (const auto& dev : knownDevices_) {
        if (!dev.second.certificate) {
            JAMI_WARNING("[Account {}] [Contacts] No certificate found for {}", accountId_, dev.first);
            continue;
        }
        sync_data.devices.emplace(dev.second.certificate->getLongId(), KnownDeviceSync {dev.second.name});
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
