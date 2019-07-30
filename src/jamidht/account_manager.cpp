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
#include "account_manager.h"
#include "accountarchive.h"
#include "jamiaccount.h"
#include "base64.h"
#include "dring/account_const.h"
#include "account_schema.h"
#include "archiver.h"

#include "libdevcrypto/Common.h"

#include <opendht/thread_pool.h>

#include <exception>
#include <future>
#include <fstream>

namespace jami {

dht::crypto::Identity
AccountManager::loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const
{
    JAMI_DBG("Loading certificate from '%s' and key from '%s' at %s", crt_path.c_str(), key_path.c_str(), path_.c_str());
    try {
        dht::crypto::Certificate dht_cert(fileutils::loadFile(crt_path, path_));
        dht::crypto::PrivateKey  dht_key(fileutils::loadFile(key_path, path_), key_pwd);
        auto crt_id = dht_cert.getLongId();
        if (!crt_id or crt_id != dht_key.getPublicKey().getLongId()) {
            JAMI_ERR("Device certificate not matching public key!");
            return {};
        }
        if (not dht_cert.issuer) {
            JAMI_ERR("Device certificate %s has no issuer", dht_cert.getId().to_c_str());
            return {};
        }
        // load revocation lists for device authority (account certificate).
        tls::CertificateStore::instance().loadRevocations(*dht_cert.issuer);

        return {
            std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
            std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))
        };
    }
    catch (const std::exception& e) {
        JAMI_ERR("Error loading identity: %s", e.what());
    }
    return {};
}

std::shared_ptr<dht::Value>
AccountManager::parseAnnounce(const std::string& announceBase64, const std::string& accountId, const std::string& deviceId)
{
    auto announce_val = std::make_shared<dht::Value>();
    try {
        auto announce = base64::decode(announceBase64);
        msgpack::object_handle announce_msg = msgpack::unpack((const char*)announce.data(), announce.size());
        announce_val->msgpack_unpack(announce_msg.get());
        if (not announce_val->checkSignature()) {
            JAMI_ERR("[Auth] announce signature check failed");
            return {};
        }
        DeviceAnnouncement da;
        da.unpackValue(*announce_val);
        if (da.from.toString() != accountId or da.dev.toString() != deviceId) {
            JAMI_ERR("[Auth] device ID mismatch in announce");
            return {};
        }
    } catch (const std::exception& e) {
        JAMI_ERR("[Auth] can't read announce: %s", e.what());
        return {};
    }
    return announce_val;
}

const AccountInfo*
AccountManager::useIdentity(
    const dht::crypto::Identity& identity,
    const std::string& receipt,
    const std::vector<uint8_t>& receiptSignature,
    OnChangeCallback&& onChange)
{
    if (receipt.empty() or receiptSignature.empty())
        return nullptr;

    if (not identity.first or not identity.second) {
        JAMI_ERR("[Auth] no identity provided");
        return nullptr;
    }

    auto accountCertificate = identity.second->issuer;
    if (not accountCertificate) {
        JAMI_ERR("[Auth] device certificate must be issued by the account certificate");
        return nullptr;
    }

    // match certificate chain
    auto contactList = std::make_unique<ContactList>(accountCertificate, path_, onChange);
    auto result = contactList->isValidAccountDevice(*identity.second);
    if (not result) {
        JAMI_ERR("[Auth] can't use identity: device certificate chain can't be verified: %s", result.toString().c_str());
        return nullptr;
    }

    auto pk = accountCertificate->getPublicKey();
    JAMI_DBG("[Auth] checking device receipt for %s", pk.getId().toString().c_str());
    if (!pk.checkSignature({receipt.begin(), receipt.end()}, receiptSignature)) {
        JAMI_ERR("[Auth] device receipt signature check failed");
        return nullptr;
    }

    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(&receipt[0], &receipt[receipt.size()], &root, nullptr)) {
        JAMI_ERR() << this << " device receipt parsing error";
        return nullptr;
    }

    auto dev_id = root["dev"].asString();
    if (dev_id != identity.second->getId().toString()) {
        JAMI_ERR("[Auth] device ID mismatch between receipt and certificate");
        return nullptr;
    }
    auto id = root["id"].asString();
    if (id != pk.getId().toString()) {
        JAMI_ERR("[Auth] account ID mismatch between receipt and certificate");
        return nullptr;
    }

    auto announce = parseAnnounce(root["announce"].asString(), id, dev_id);
    if (not announce) {
        return nullptr;
    }

    onChange_ = std::move(onChange);

    auto info = std::make_unique<AccountInfo>();
    info->identity = identity;
    info->contacts = std::move(contactList);
    info->contacts->load();
    info->accountId = id;
    info->deviceId = identity.first->getPublicKey().getId().toString();
    info->announce = std::move(announce);
    info->ethAccount = root["eth"].asString();
    info_ = std::move(info);

    JAMI_DBG("[Auth] Device %s receipt checked successfully for account %s", info_->deviceId.c_str(), id.c_str());
    return info_.get();
}


void
AccountManager::startSync()
{
    // Put device annoucement
    if (info_->announce) {
        auto h = dht::InfoHash(info_->accountId);
        JAMI_DBG("announcing device at %s", h.toString().c_str());
        dht_->put(h, info_->announce, dht::DoneCallback{}, {}, true);
        for (const auto& crl : info_->identity.second->issuer->getRevocationLists())
            dht_->put(h, crl, dht::DoneCallback{}, {}, true);
        dht_->listen<DeviceAnnouncement>(h, [this](DeviceAnnouncement&& dev) {
            findCertificate(dev.dev, [this](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                foundAccountDevice(crt);
            });
            return true;
        });
        dht_->listen<dht::crypto::RevocationList>(h, [this](dht::crypto::RevocationList&& crl) {
            if (crl.isSignedBy(*info_->identity.second->issuer)) {
                JAMI_DBG("found CRL for account.");
                tls::CertificateStore::instance().pinRevocationList(
                    info_->accountId,
                    std::make_shared<dht::crypto::RevocationList>(std::move(crl)));
            }
            return true;
        });
        syncDevices();
    } else {
        JAMI_WARN("can't announce device: no annoucement...");
    }

    auto inboxKey = dht::InfoHash::get("inbox:"+info_->deviceId);
    dht_->listen<dht::TrustRequest>(
        inboxKey,
        [this](dht::TrustRequest&& v) {
            if (v.service != DHT_TYPE_NS)
                return true;

            // allowPublic always true for trust requests (only forbidden if banned)
            onPeerMessage(v.from, true, [this, v](const std::shared_ptr<dht::crypto::Certificate>& cert, dht::InfoHash peer_account) mutable {
                JAMI_WARN("Got trust request from: %s / %s", peer_account.toString().c_str(), v.from.toString().c_str());
                if (info_)
                    if (info_->contacts->onTrustRequest(peer_account, v.from, time(nullptr), v.confirm, std::move(v.payload))) {
                        sendTrustRequestConfirm(peer_account);
                    }
            });
            return true;
        }
    );
}

const std::map<dht::InfoHash, KnownDevice>&
AccountManager::getKnownDevices() const {
    return info_->contacts->getKnownDevices();
}

bool
AccountManager::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name, const time_point& last_sync)
{
    return info_->contacts->foundAccountDevice(crt, name, last_sync);
}

void
AccountManager::setAccountDeviceName(const std::string& name)
{
    if (info_)
        info_->contacts->setAccountDeviceName(dht::InfoHash(info_->deviceId), name);
}


bool
AccountManager::foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, dht::InfoHash& account_id)
{
    if (not crt)
        return false;

    auto top_issuer = crt;
    while (top_issuer->issuer)
        top_issuer = top_issuer->issuer;

    // Device certificate can't be self-signed
    if (top_issuer == crt) {
        JAMI_WARN("Found invalid peer device: %s", crt->getId().toString().c_str());
        return false;
    }

    // Check peer certificate chain
    // Trust store with top issuer as the only CA
    dht::crypto::TrustList peer_trust;
    peer_trust.add(*top_issuer);
    if (not peer_trust.verify(*crt)) {
        JAMI_WARN("Found invalid peer device: %s", crt->getId().toString().c_str());
        return false;
    }

    account_id = crt->issuer->getId();
    JAMI_WARN("Found peer device: %s account:%s CA:%s", crt->getId().toString().c_str(), account_id.toString().c_str(), top_issuer->getId().toString().c_str());
    return true;
}

void
AccountManager::checkImplicitTrustRequest(const dht::InfoHash& account, const dht::InfoHash& peer_device, const std::shared_ptr<dht::crypto::Certificate>& cert){
    bool active = false;
    const auto& contacts = info_->contacts->getContacts();
    auto contact = contacts.find(account);
    if (contact != contacts.end()){
        active = contact->second.isActive();
    }
    if (not active){
        auto func = [this, peer_device](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
            // check peer certificate
            dht::InfoHash peer_account;
            if (not foundPeerDevice(cert, peer_account)) {
                return;
            }
            JAMI_WARN("Got implicit trust request from: %s / %s", peer_account.toString().c_str(), peer_device.toString().c_str());
            info_->contacts->onTrustRequest(peer_account, peer_device, time(nullptr), false, {});
        };
        if (not cert){
            findCertificate(peer_device, func);
        } else {
            func(cert);
        }
    }
}


void
AccountManager::onPeerMessage(const dht::InfoHash& peer_device, bool allowPublic, std::function<void(const std::shared_ptr<dht::crypto::Certificate>& crt, const dht::InfoHash& peer_account)>&& cb)
{
    // quick check in case we already explicilty banned this device
    auto trustStatus = getCertificateStatus(peer_device.toString());
    if (trustStatus == tls::TrustStore::PermissionStatus::BANNED) {
        JAMI_WARN("[Auth] Discarding message from banned device %s", peer_device.toString().c_str());
        return;
    }

    findCertificate(peer_device,
        [this, peer_device, cb=std::move(cb), allowPublic](const std::shared_ptr<dht::crypto::Certificate>& cert) {
        dht::InfoHash peer_account_id;
        if (onPeerCertificate(cert, allowPublic, peer_account_id)) {
            cb(cert, peer_account_id);
        }
    });
}

bool
AccountManager::onPeerCertificate(const std::shared_ptr<dht::crypto::Certificate>& cert, bool allowPublic, dht::InfoHash& account_id)
{
    dht::InfoHash peer_account_id;
    if (not foundPeerDevice(cert, peer_account_id)) {
        JAMI_WARN("[Auth] Discarding message from invalid peer certificate");
        return false;
    }

    if (not isAllowed(*cert, allowPublic)) {
        JAMI_WARN("[Auth] Discarding message from unauthorized peer %s.", peer_account_id.toString().c_str());
        return false;
    }

    account_id = peer_account_id;
    return true;
}

void
AccountManager::addContact(const std::string& uri, bool confirmed)
{
    JAMI_WARN("AccountManager::addContact %d", confirmed);
    dht::InfoHash h (uri);
    if (not h) {
        JAMI_ERR("addContact: invalid contact URI");
        return;
    }
    if (info_->contacts->addContact(h, confirmed)) {
        syncDevices();
    }
}

void
AccountManager::removeContact(const std::string& uri, bool banned)
{
    dht::InfoHash h (uri);
    if (not h) {
        JAMI_ERR("removeContact: invalid contact URI");
        return;
    }
    if (info_->contacts->removeContact(h, banned)) {
        syncDevices();
    }
}

std::vector<std::map<std::string, std::string>>
AccountManager::getContacts() const
{
    if (not info_) {
        JAMI_ERR("getContacts(): account not loaded");
        return {};
    }
    const auto& contacts = info_->contacts->getContacts();
    std::vector<std::map<std::string, std::string>> ret;
    ret.reserve(contacts.size());

    for (const auto& c : contacts) {
        auto details = c.second.toMap();
        if (not details.empty()) {
            details["id"] = c.first.toString();
            ret.emplace_back(std::move(details));
        }
    }
    return ret;
}

/** Obtain details about one account contact in serializable form. */
std::map<std::string, std::string>
AccountManager::getContactDetails(const std::string& uri) const
{
    dht::InfoHash h (uri);
    if (not h) {
        JAMI_ERR("getContactDetails: invalid contact URI");
        return {};
    }
    return info_->contacts->getContactDetails(h);
}

bool
AccountManager::findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (auto cert = tls::CertificateStore::instance().getCertificate(h.toString())) {
        if (cb)
            cb(cert);
    } else {
        dht_->findCertificate(h, [cb](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (crt)
                tls::CertificateStore::instance().pinCertificate(crt);
            if (cb)
                cb(crt);
        });
    }
    return true;
}

bool
AccountManager::setCertificateStatus(const std::string& cert_id, tls::TrustStore::PermissionStatus status)
{
    return info_->contacts->setCertificateStatus(cert_id, status);
}

std::vector<std::string>
AccountManager::getCertificatesByStatus(tls::TrustStore::PermissionStatus status)
{
    return info_->contacts->getCertificatesByStatus(status);
}

tls::TrustStore::PermissionStatus
AccountManager::getCertificateStatus(const std::string& cert_id) const
{
    return info_->contacts->getCertificateStatus(cert_id);
}

bool
AccountManager::isAllowed(const crypto::Certificate& crt, bool allowPublic)
{
    return info_->contacts->isAllowed(crt, allowPublic);
}

std::vector<std::map<std::string, std::string>>
AccountManager::getTrustRequests() const
{
    if (not info_) {
        JAMI_ERR("getTrustRequests(): account not loaded");
        return {};
    }
    return info_->contacts->getTrustRequests();
}

bool
AccountManager::acceptTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    if (info_->contacts->acceptTrustRequest(f)) {
        sendTrustRequestConfirm(f);
        syncDevices();
        return true;
    }
    return false;
}

bool
AccountManager::discardTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    return info_->contacts->discardTrustRequest(f);
}

void
AccountManager::sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload)
{
    JAMI_WARN("AccountManager::sendTrustRequest");
    auto toH = dht::InfoHash(to);
    if (not toH) {
        JAMI_ERR("can't send trust request to invalid hash: %s", to.c_str());
        return;
    }
    if (info_->contacts->addContact(toH)) {
        syncDevices();
    }
    forEachDevice(toH, [this,toH,payload](const dht::InfoHash& dev)
    {
        JAMI_WARN("sending trust request to: %s / %s", toH.toString().c_str(), dev.toString().c_str());
        dht_->putEncrypted(dht::InfoHash::get("inbox:"+dev.toString()),
                          dev,
                          dht::TrustRequest(DHT_TYPE_NS, payload));
    });
}

void
AccountManager::sendTrustRequestConfirm(const dht::InfoHash& toH)
{
    JAMI_WARN("AccountManager::sendTrustRequestConfirm");
    dht::TrustRequest answer {DHT_TYPE_NS};
    answer.confirm = true;
    forEachDevice(toH, [this,toH,answer](const dht::InfoHash& dev) {
        JAMI_WARN("sending trust request reply: %s / %s", toH.toString().c_str(), dev.toString().c_str());
        dht_->putEncrypted(dht::InfoHash::get("inbox:"+dev.toString()), dev, answer);
    });
}


void
AccountManager::forEachDevice(const dht::InfoHash& to,
                           std::function<void(const dht::InfoHash&)>&& op,
                           std::function<void(bool)>&& end)
{
    auto treatedDevices = std::make_shared<std::set<dht::InfoHash>>();
    dht_->get<dht::crypto::RevocationList>(to, [to](dht::crypto::RevocationList&& crl){
        tls::CertificateStore::instance().pinRevocationList(to.toString(), std::move(crl));
        return true;
    });
    dht_->get<DeviceAnnouncement>(to, [this,to,treatedDevices,op=std::move(op)](DeviceAnnouncement&& dev) {
        if (dev.from != to)
            return true;
        if (treatedDevices->emplace(dev.dev).second) {
            op(dev.dev);
        }
        return true;
    }, [=, end=std::move(end)](bool /*ok*/){
        JAMI_DBG("Found %lu devices for %s", treatedDevices->size(), to.to_c_str());
        if (end)
            end(not treatedDevices->empty());
    });
}

void
AccountManager::lookupUri(const std::string& name, const std::string& defaultServer, LookupCallback cb)
{
    nameDir_.get().lookupUri(name, defaultServer, std::move(cb));
}

void
AccountManager::lookupAddress(const std::string& addr, LookupCallback cb)
{
    nameDir_.get().lookupAddress(addr, cb);
}

}
