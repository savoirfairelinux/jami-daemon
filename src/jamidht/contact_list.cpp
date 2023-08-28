/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
                         const std::string& path,
                         OnChangeCallback cb)
    : path_(path)
    , callbacks_(std::move(cb))
    , accountId_(accountId)
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
        JAMI_DBG("Can't set certificate status for existing contacts %s", cert_id.c_str());
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
    JAMI_WARN("[Contacts] addContact: %s, conversation: %s", h.to_c_str(), conversationId.c_str());
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
    if (c != contacts_.end()) {
        c->second.conversationId = conversationId;
        saveContacts();
    }
}

bool
ContactList::removeContact(const dht::InfoHash& h, bool ban)
{
    JAMI_WARN("[Contacts] removeContact: %s", h.to_c_str());
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
#ifdef ENABLE_PLUGIN
    std::size_t found = path_.find_last_of(DIR_SEPARATOR_CH);
    if (found != std::string::npos) {
        auto filename = path_.substr(found + 1);
        jami::Manager::instance()
            .getJamiPluginManager()
            .getChatServicesManager()
            .cleanChatSubjects(filename, uri);
    }
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
    // Set contacts is used when creating a new device, so just announce new contacts
    for (auto& peer : contacts)
        if (peer.second.isActive())
            callbacks_.contactAdded(peer.first.toString(), peer.second.confirmed);
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
        // JAMI_DBG("[Contacts] new contact: %s", id.toString().c_str());
        c = contacts_.emplace(id, contact).first;
        stateChanged = c->second.isActive() or c->second.isBanned();
    } else {
        JAMI_DBG("[Contacts] updated contact: %s", id.toString().c_str());
        stateChanged = c->second.update(contact);
    }
    if (stateChanged) {
        if (trustRequests_.erase(id) > 0)
            saveTrustRequests();
        if (c->second.isActive()) {
            trust_->setCertificateStatus(id.toString(), dhtnet::tls::TrustStore::PermissionStatus::ALLOWED);
            callbacks_.contactAdded(id.toString(), c->second.confirmed);
        } else {
            if (c->second.banned)
                trust_->setCertificateStatus(id.toString(),
                                            dhtnet::tls::TrustStore::PermissionStatus::BANNED);
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
        JAMI_WARN("[Contacts] error loading contacts: %s", e.what());
        return;
    }

    for (auto& peer : contacts)
        updateContact(peer.first, peer.second);
}

void
ContactList::saveContacts() const
{
    std::ofstream file(path_ + DIR_SEPARATOR_STR "contacts", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, contacts_);
}

void
ContactList::saveTrustRequests() const
{
    std::ofstream file(path_ + DIR_SEPARATOR_STR "incomingTrustRequests",
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
        JAMI_WARN("[Contacts] error loading trust requests: %s", e.what());
        return;
    }

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
            if (received < req->second.received) {
                req->second.device = peer_device;
                req->second.received = received;
                req->second.payload = payload;
            } else {
                JAMI_DBG("[Contacts] Ignoring outdated trust request from %s",
                         peer_account.toString().c_str());
            }
        }
        saveTrustRequests();
    }
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
    auto i = trustRequests_.find(from);
    if (i == trustRequests_.end())
        return false;
    addContact(from, true, i->second.conversationId);
    // Clear trust request
    trustRequests_.erase(i);
    saveTrustRequests();
    return true;
}

void
ContactList::acceptConversation(const std::string& convId)
{
    if (callbacks_.acceptConversation)
        callbacks_.acceptConversation(convId);
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
    try {
        // read file
        auto file = fileutils::loadFile("knownDevices", path_);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());

        std::map<dht::PkId, std::pair<std::string, uint64_t>> knownDevices;
        oh.get().convert(knownDevices);
        for (const auto& d : knownDevices) {
            if (auto crt = jami::Manager::instance().certStore(accountId_).getCertificate(d.first.toString())) {
                if (not foundAccountDevice(crt, d.second.first, clock::from_time_t(d.second.second)))
                    JAMI_WARN("[Contacts] can't add device %s", d.first.toString().c_str());
            } else {
                JAMI_WARN("[Contacts] can't find certificate for device %s",
                          d.first.toString().c_str());
            }
        }
    } catch (const std::exception& e) {
        // Legacy fallback
        try {
            auto file = fileutils::loadFile("knownDevicesNames", path_);
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::map<dht::InfoHash, std::pair<std::string, uint64_t>> knownDevices;
            oh.get().convert(knownDevices);
            for (const auto& d : knownDevices) {
                if (auto crt = jami::Manager::instance().certStore(accountId_).getCertificate(d.first.toString())) {
                    if (not foundAccountDevice(crt,
                                               d.second.first,
                                               clock::from_time_t(d.second.second)))
                        JAMI_WARN("[Contacts] can't add device %s", d.first.toString().c_str());
                }
            }
        } catch (const std::exception& e) {
            JAMI_WARN("[Contacts] error loading devices: %s", e.what());
        }
        return;
    }
}

void
ContactList::saveKnownDevices() const
{
    std::ofstream file(path_ + DIR_SEPARATOR_STR "knownDevices", std::ios::trunc | std::ios::binary);

    std::map<dht::PkId, std::pair<std::string, uint64_t>> devices;
    for (const auto& id : knownDevices_)
        devices.emplace(id.first,
                        std::make_pair(id.second.name, clock::to_time_t(id.second.last_sync)));

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
        JAMI_DBG("[Contacts] Found account device: %s %s", name.c_str(), device.toString().c_str());
        saveKnownDevices();
        callbacks_.devicesChanged(knownDevices_);
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_DBG("[Contacts] updating device name: %s %s",
                     name.c_str(),
                     device.toString().c_str());
            it.first->second.name = name;
            saveKnownDevices();
            callbacks_.devicesChanged(knownDevices_);
        }
    }
}

bool
ContactList::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                const std::string& name,
                                const time_point& updated)
{
    if (not crt)
        return false;

    auto id = crt->getLongId();

    // match certificate chain
    auto verifyResult = accountTrust_.verify(*crt);
    if (not verifyResult) {
        JAMI_WARN("[Contacts] Found invalid account device: %s: %s",
                  id.toString().c_str(),
                  verifyResult.toString().c_str());
        return false;
    }

    // insert device
    auto it = knownDevices_.emplace(id, KnownDevice {crt, name, updated});
    if (it.second) {
        JAMI_DBG("[Contacts] Found account device: %s %s", name.c_str(), id.toString().c_str());
        jami::Manager::instance().certStore(accountId_).pinCertificate(crt);
        if (crt->ocspResponse) {
            unsigned int status = crt->ocspResponse->getCertificateStatus();
            if (status == GNUTLS_OCSP_CERT_REVOKED) {
                JAMI_ERR("Certificate %s has revoked OCSP status", id.to_c_str());
                trust_->setCertificateStatus(crt, dhtnet::tls::TrustStore::PermissionStatus::BANNED, false);
            }
        }
        saveKnownDevices();
        callbacks_.devicesChanged(knownDevices_);
    } else {
        // update device name
        if (not name.empty() and it.first->second.name != name) {
            JAMI_DBG("[Contacts] updating device name: %s %s", name.c_str(), id.to_c_str());
            it.first->second.name = name;
            saveKnownDevices();
            callbacks_.devicesChanged(knownDevices_);
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

} // namespace jami
