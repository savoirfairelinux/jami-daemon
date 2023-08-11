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
#include "account_manager.h"
#include "accountarchive.h"
#include "jamiaccount.h"
#include "base64.h"
#include "jami/account_const.h"
#include "account_schema.h"
#include "archiver.h"
#include "manager.h"

#include "libdevcrypto/Common.h"

#include <opendht/thread_pool.h>
#include <opendht/crypto.h>

#include <exception>
#include <future>
#include <fstream>
#include <gnutls/ocsp.h>

namespace jami {

AccountManager::CertRequest
AccountManager::buildRequest(PrivateKey fDeviceKey)
{
    return dht::ThreadPool::computation().get<std::unique_ptr<dht::crypto::CertificateRequest>>(
        [fDeviceKey = std::move(fDeviceKey)] {
            auto request = std::make_unique<dht::crypto::CertificateRequest>();
            request->setName("Jami device");
            const auto& deviceKey = fDeviceKey.get();
            request->setUID(deviceKey->getPublicKey().getId().toString());
            request->sign(*deviceKey);
            return request;
        });
}

AccountManager::~AccountManager() {
    if (dht_)
        dht_->join();
}

void
AccountManager::onSyncData(DeviceSync&& sync, bool checkDevice)
{
    auto sync_date = clock::time_point(clock::duration(sync.date));
    if (checkDevice) {
        // If the DHT is used, we need to check the device here
        if (not info_->contacts->syncDevice(sync.owner->getLongId(), sync_date)) {
            return;
        }
    }

    // Sync known devices
    JAMI_DEBUG("[Contacts] received device sync data ({:d} devices, {:d} contacts)",
             sync.devices_known.size() + sync.devices.size(),
             sync.peers.size());
    for (const auto& d : sync.devices_known) {
        findCertificate(d.first, [this, d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (not crt)
                return;
            // std::lock_guard<std::mutex> lock(deviceListMutex_);
            foundAccountDevice(crt, d.second);
        });
    }
    for (const auto& d : sync.devices) {
        findCertificate(d.second.sha1,
                        [this, d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                            if (not crt || crt->getLongId() != d.first)
                                return;
                            // std::lock_guard<std::mutex> lock(deviceListMutex_);
                            foundAccountDevice(crt, d.second.name);
                        });
    }
    // saveKnownDevices();

    // Sync contacts
    for (const auto& peer : sync.peers) {
        info_->contacts->updateContact(peer.first, peer.second);
    }
    info_->contacts->saveContacts();

    // Sync trust requests
    for (const auto& tr : sync.trust_requests)
        info_->contacts->onTrustRequest(tr.first,
                                        tr.second.device,
                                        tr.second.received,
                                        false,
                                        tr.second.conversationId,
                                        {});
    info_->contacts->saveTrustRequests();
}

dht::crypto::Identity
AccountManager::loadIdentity(const std::string& accountId,
                             const std::string& crt_path,
                             const std::string& key_path,
                             const std::string& key_pwd) const
{
    // Return to avoid unnecessary log if certificate or key is missing. Example case: when
    // importing an account when the certificate has not been unpacked from the archive.
    if (crt_path.empty() or key_path.empty())
        return {};

    JAMI_DEBUG("Loading certificate from '{}' and key from '{}' at {}",
             crt_path,
             key_path,
             path_);
    try {
        dht::crypto::Certificate dht_cert(fileutils::loadFile(crt_path, path_));
        dht::crypto::PrivateKey dht_key(fileutils::loadFile(key_path, path_), key_pwd);
        auto crt_id = dht_cert.getLongId();
        if (!crt_id or crt_id != dht_key.getPublicKey().getLongId()) {
            JAMI_ERR("Device certificate not matching public key!");
            return {};
        }
        auto& issuer = dht_cert.issuer;
        if (not issuer) {
            JAMI_ERROR("Device certificate {:s} has no issuer", dht_cert.getId().toString());
            return {};
        }
        // load revocation lists for device authority (account certificate).
        Manager::instance().certStore(accountId).loadRevocations(*issuer);

        return {std::make_shared<dht::crypto::PrivateKey>(std::move(dht_key)),
                std::make_shared<dht::crypto::Certificate>(std::move(dht_cert))};
    } catch (const std::exception& e) {
        JAMI_ERR("Error loading identity: %s", e.what());
    }
    return {};
}

std::shared_ptr<dht::Value>
AccountManager::parseAnnounce(const std::string& announceBase64,
                              const std::string& accountId,
                              const std::string& deviceSha1)
{
    auto announce_val = std::make_shared<dht::Value>();
    try {
        auto announce = base64::decode(announceBase64);
        msgpack::object_handle announce_msg = msgpack::unpack((const char*) announce.data(),
                                                              announce.size());
        announce_val->msgpack_unpack(announce_msg.get());
        if (not announce_val->checkSignature()) {
            JAMI_ERR("[Auth] announce signature check failed");
            return {};
        }
        DeviceAnnouncement da;
        da.unpackValue(*announce_val);
        if (da.from.toString() != accountId or da.dev.toString() != deviceSha1) {
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
AccountManager::useIdentity(const std::string& accountId,
                            const dht::crypto::Identity& identity,
                            const std::string& receipt,
                            const std::vector<uint8_t>& receiptSignature,
                            const std::string& username,
                            const OnChangeCallback& onChange)
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
    auto contactList = std::make_unique<ContactList>(accountId, accountCertificate, path_, onChange);
    auto result = contactList->isValidAccountDevice(*identity.second);
    if (not result) {
        JAMI_ERR("[Auth] can't use identity: device certificate chain can't be verified: %s",
                 result.toString().c_str());
        return nullptr;
    }

    auto pk = accountCertificate->getSharedPublicKey();
    JAMI_DBG("[Auth] checking device receipt for %s", pk->getId().toString().c_str());
    if (!pk->checkSignature({receipt.begin(), receipt.end()}, receiptSignature)) {
        JAMI_ERR("[Auth] device receipt signature check failed");
        return nullptr;
    }

    auto root = announceFromReceipt(receipt);
    if (!root.isMember("announce")) {
        JAMI_ERR() << this << " device receipt parsing error";
        return nullptr;
    }

    auto dev_id = root["dev"].asString();
    if (dev_id != identity.second->getId().toString()) {
        JAMI_ERR("[Auth] device ID mismatch between receipt and certificate");
        return nullptr;
    }
    auto id = root["id"].asString();
    if (id != pk->getId().toString()) {
        JAMI_ERR("[Auth] account ID mismatch between receipt and certificate");
        return nullptr;
    }

    auto devicePk = identity.first->getSharedPublicKey();
    if (!devicePk) {
        JAMI_ERR("[Auth] No device pk found");
        return nullptr;
    }

    auto announce = parseAnnounce(root["announce"].asString(), id, devicePk->getId().toString());
    if (not announce) {
        return nullptr;
    }

    onChange_ = std::move(onChange);

    auto info = std::make_unique<AccountInfo>();
    info->identity = identity;
    info->contacts = std::move(contactList);
    info->contacts->load();
    info->accountId = id;
    info->devicePk = std::move(devicePk);
    info->deviceId = info->devicePk->getLongId().toString();
    info->announce = std::move(announce);
    info->ethAccount = root["eth"].asString();
    info->username = username;
    info_ = std::move(info);

    JAMI_DBG("[Auth] Device %s receipt checked successfully for account %s",
             info_->deviceId.c_str(),
             id.c_str());
    return info_.get();
}

Json::Value
AccountManager::announceFromReceipt(const std::string& receipt)
{
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(&receipt[0], &receipt[receipt.size()], &root, nullptr)) {
        JAMI_ERR() << this << " device receipt parsing error";
    }
    return root;
}

void
AccountManager::startSync(const OnNewDeviceCb& cb, const OnDeviceAnnouncedCb& dcb)
{
    // Put device announcement
    if (info_->announce) {
        auto h = dht::InfoHash(info_->accountId);
        dht_->put(
            h,
            info_->announce,
            [dcb = std::move(dcb), h](bool ok) {
                if (ok)
                    JAMI_DEBUG("device announced at {}", h.toString());
                // We do not care about the status, it's a permanent put, if this fail,
                // this means the DHT is disconnected but the put will be retried when connected.
                if (dcb)
                    dcb();
            },
            {},
            true);
        for (const auto& crl : info_->identity.second->issuer->getRevocationLists())
            dht_->put(h, crl, dht::DoneCallback {}, {}, true);
        dht_->listen<DeviceAnnouncement>(h, [this, cb = std::move(cb)](DeviceAnnouncement&& dev) {
            findCertificate(dev.dev,
                            [this, cb](const std::shared_ptr<dht::crypto::Certificate>& crt) {
                                foundAccountDevice(crt);
                                if (cb)
                                    cb(crt);
                            });
            return true;
        });
        dht_->listen<dht::crypto::RevocationList>(h, [this](dht::crypto::RevocationList&& crl) {
            if (crl.isSignedBy(*info_->identity.second->issuer)) {
                JAMI_DEBUG("found CRL for account.");
                certStore()
                    .pinRevocationList(info_->accountId,
                                       std::make_shared<dht::crypto::RevocationList>(
                                           std::move(crl)));
            }
            return true;
        });
        syncDevices();
    } else {
        JAMI_WARNING("can't announce device: no announcement...");
    }

    auto inboxKey = dht::InfoHash::get("inbox:" + info_->devicePk->getId().toString());
    dht_->listen<dht::TrustRequest>(inboxKey, [this](dht::TrustRequest&& v) {
        if (v.service != DHT_TYPE_NS)
            return true;

        // allowPublic always true for trust requests (only forbidden if banned)
        onPeerMessage(
            *v.owner,
            true,
            [this, v](const std::shared_ptr<dht::crypto::Certificate>&,
                      dht::InfoHash peer_account) mutable {
                JAMI_WARNING("Got trust request (confirm: {}) from: {} / {}. ConversationId: {}",
                          v.confirm,
                          peer_account.toString(),
                          v.from.toString(),
                          v.conversationId);
                if (info_)
                    if (info_->contacts->onTrustRequest(peer_account,
                                                        v.owner,
                                                        time(nullptr),
                                                        v.confirm,
                                                        v.conversationId,
                                                        std::move(v.payload))) {
                        auto conversationId = v.conversationId;
                        // Check if there was an old active conversation.
                        auto details = info_->contacts->getContactDetails(peer_account);
                        auto oldConvIt = details.find(libjami::Account::TrustRequest::CONVERSATIONID);
                        if (oldConvIt != details.end() && oldConvIt->second != "") {
                            if (conversationId == oldConvIt->second)
                                return;
                            conversationId = oldConvIt->second;
                            JAMI_WARNING("Accept with old convId: {}", conversationId);
                        }
                        sendTrustRequestConfirm(peer_account, conversationId);
                        info_->contacts->saveTrustRequests();
                    }
            });
        return true;
    });
}

const std::map<dht::PkId, KnownDevice>&
AccountManager::getKnownDevices() const
{
    return info_->contacts->getKnownDevices();
}

bool
AccountManager::foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                   const std::string& name,
                                   const time_point& last_sync)
{
    return info_->contacts->foundAccountDevice(crt, name, last_sync);
}

void
AccountManager::setAccountDeviceName(const std::string& name)
{
    if (info_)
        info_->contacts->setAccountDeviceName(DeviceId(info_->deviceId), name);
}

std::string
AccountManager::getAccountDeviceName() const
{
    if (info_)
        return info_->contacts->getAccountDeviceName(DeviceId(info_->deviceId));
    return {};
}

bool
AccountManager::foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                dht::InfoHash& account_id)
{
    if (not crt)
        return false;

    auto top_issuer = crt;
    while (top_issuer->issuer)
        top_issuer = top_issuer->issuer;

    // Device certificate can't be self-signed
    if (top_issuer == crt) {
        JAMI_WARN("Found invalid peer device: %s", crt->getLongId().toString().c_str());
        return false;
    }

    // Check peer certificate chain
    // Trust store with top issuer as the only CA
    dht::crypto::TrustList peer_trust;
    peer_trust.add(*top_issuer);
    if (not peer_trust.verify(*crt)) {
        JAMI_WARN("Found invalid peer device: %s", crt->getLongId().toString().c_str());
        return false;
    }

    // Check cached OCSP response
    if (crt->ocspResponse and crt->ocspResponse->getCertificateStatus() != GNUTLS_OCSP_CERT_GOOD) {
        JAMI_ERR("Certificate %s is disabled by cached OCSP response", crt->getLongId().to_c_str());
        return false;
    }

    account_id = crt->issuer->getId();
    JAMI_WARN("Found peer device: %s account:%s CA:%s",
              crt->getLongId().toString().c_str(),
              account_id.toString().c_str(),
              top_issuer->getId().toString().c_str());
    return true;
}

void
AccountManager::onPeerMessage(const dht::crypto::PublicKey& peer_device,
                              bool allowPublic,
                              std::function<void(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                                 const dht::InfoHash& peer_account)>&& cb)
{
    // quick check in case we already explicilty banned this device
    auto trustStatus = getCertificateStatus(peer_device.toString());
    if (trustStatus == dhtnet::tls::TrustStore::PermissionStatus::BANNED) {
        JAMI_WARN("[Auth] Discarding message from banned device %s", peer_device.toString().c_str());
        return;
    }

    findCertificate(peer_device.getId(),
                    [this, cb = std::move(cb), allowPublic](
                        const std::shared_ptr<dht::crypto::Certificate>& cert) {
                        dht::InfoHash peer_account_id;
                        if (onPeerCertificate(cert, allowPublic, peer_account_id)) {
                            cb(cert, peer_account_id);
                        }
                    });
}

bool
AccountManager::onPeerCertificate(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                  bool allowPublic,
                                  dht::InfoHash& account_id)
{
    dht::InfoHash peer_account_id;
    if (not foundPeerDevice(cert, peer_account_id)) {
        JAMI_WARN("[Auth] Discarding message from invalid peer certificate");
        return false;
    }

    if (not isAllowed(*cert, allowPublic)) {
        JAMI_WARN("[Auth] Discarding message from unauthorized peer %s.",
                  peer_account_id.toString().c_str());
        return false;
    }

    account_id = peer_account_id;
    return true;
}

bool
AccountManager::addContact(const std::string& uri, bool confirmed, const std::string& conversationId)
{
    JAMI_WARN("AccountManager::addContact %d", confirmed);
    dht::InfoHash h(uri);
    if (not h) {
        JAMI_ERR("addContact: invalid contact URI");
        return false;
    }
    if (not info_) {
        JAMI_ERR("addContact(): account not loaded");
        return false;
    }
    if (info_->contacts->addContact(h, confirmed, conversationId)) {
        syncDevices();
        return true;
    }
    return false;
}

void
AccountManager::removeContact(const std::string& uri, bool banned)
{
    dht::InfoHash h(uri);
    if (not h) {
        JAMI_ERR("removeContact: invalid contact URI");
        return;
    }
    if (not info_) {
        JAMI_ERR("addContact(): account not loaded");
        return;
    }
    if (info_->contacts->removeContact(h, banned))
        syncDevices();
}

void
AccountManager::removeContactConversation(const std::string& uri)
{
    dht::InfoHash h(uri);
    if (not h) {
        JAMI_ERR("removeContact: invalid contact URI");
        return;
    }
    if (not info_) {
        JAMI_ERR("addContact(): account not loaded");
        return;
    }
    if (info_->contacts->removeContactConversation(h))
        syncDevices();
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
    dht::InfoHash h(uri);
    if (not h) {
        JAMI_ERR("getContactDetails: invalid contact URI");
        return {};
    }
    return info_->contacts->getContactDetails(h);
}

bool
AccountManager::findCertificate(
    const dht::InfoHash& h,
    std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (auto cert = certStore().getCertificate(h.toString())) {
        if (cb)
            cb(cert);
    } else {
        dht_->findCertificate(h,
                              [cb = std::move(cb), this](
                                  const std::shared_ptr<dht::crypto::Certificate>& crt) {
                                  if (crt && info_) {
                                      certStore().pinCertificate(crt);
                                  }
                                  if (cb)
                                      cb(crt);
                              });
    }
    return true;
}

bool
AccountManager::findCertificate(
    const dht::PkId& id, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (auto cert = certStore().getCertificate(id.toString())) {
        if (cb)
            cb(cert);
    } else if (auto cert = certStore().getCertificateLegacy(fileutils::get_data_dir(), id.toString())) {
        if (cb)
            cb(cert);
    } else if (cb)
        cb(nullptr);
    return true;
}

bool
AccountManager::setCertificateStatus(const std::string& cert_id,
                                     dhtnet::tls::TrustStore::PermissionStatus status)
{
    return info_ and info_->contacts->setCertificateStatus(cert_id, status);
}

bool
AccountManager::setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                              dhtnet::tls::TrustStore::PermissionStatus status,
                              bool local)
{
    return info_ and info_->contacts->setCertificateStatus(cert, status, local);
}

std::vector<std::string>
AccountManager::getCertificatesByStatus(dhtnet::tls::TrustStore::PermissionStatus status)
{
    return info_ ? info_->contacts->getCertificatesByStatus(status) : std::vector<std::string> {};
}

dhtnet::tls::TrustStore::PermissionStatus
AccountManager::getCertificateStatus(const std::string& cert_id) const
{
    return info_ ? info_->contacts->getCertificateStatus(cert_id)
                 : dhtnet::tls::TrustStore::PermissionStatus::UNDEFINED;
}

bool
AccountManager::isAllowed(const crypto::Certificate& crt, bool allowPublic)
{
    return info_ and info_->contacts->isAllowed(crt, allowPublic);
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
AccountManager::acceptTrustRequest(const std::string& from, bool includeConversation)
{
    dht::InfoHash f(from);
    if (info_) {
        auto req = info_->contacts->getTrustRequest(dht::InfoHash(from));
        if (info_->contacts->acceptTrustRequest(f)) {
            sendTrustRequestConfirm(f,
                                    includeConversation
                                        ? req[libjami::Account::TrustRequest::CONVERSATIONID]
                                        : "");
            syncDevices();
            return true;
        }
        return false;
    }
    return false;
}

bool
AccountManager::discardTrustRequest(const std::string& from)
{
    dht::InfoHash f(from);
    return info_ and info_->contacts->discardTrustRequest(f);
}

void
AccountManager::sendTrustRequest(const std::string& to,
                                 const std::string& convId,
                                 const std::vector<uint8_t>& payload)
{
    JAMI_WARN("AccountManager::sendTrustRequest");
    auto toH = dht::InfoHash(to);
    if (not toH) {
        JAMI_ERR("can't send trust request to invalid hash: %s", to.c_str());
        return;
    }
    if (not info_) {
        JAMI_ERR("sendTrustRequest(): account not loaded");
        return;
    }
    if (info_->contacts->addContact(toH, false, convId)) {
        syncDevices();
    }
    forEachDevice(toH,
                  [this, toH, convId, payload](const std::shared_ptr<dht::crypto::PublicKey>& dev) {
                      auto to = toH.toString();
                      JAMI_WARNING("sending trust request to: {:s} / {:s}",
                                   to,
                                   dev->getLongId().toString());
                      dht_->putEncrypted(dht::InfoHash::get("inbox:" + dev->getId().toString()),
                                         dev,
                                         dht::TrustRequest(DHT_TYPE_NS, convId, payload),
                                         [to, size = payload.size()](bool ok) {
                                             if (!ok)
                                                 JAMI_ERROR("Tried to send request {:s} (size: "
                                                            "{:d}), but put failed",
                                                            to,
                                                            size);
                                         });
                  });
}

void
AccountManager::sendTrustRequestConfirm(const dht::InfoHash& toH, const std::string& convId)
{
    JAMI_WARN("AccountManager::sendTrustRequestConfirm");
    dht::TrustRequest answer {DHT_TYPE_NS, ""};
    answer.confirm = true;
    answer.conversationId = convId;

    if (!convId.empty() && info_)
        info_->contacts->acceptConversation(convId);

    forEachDevice(toH, [this, toH, answer](const std::shared_ptr<dht::crypto::PublicKey>& dev) {
        JAMI_WARN("sending trust request reply: %s / %s",
                  toH.toString().c_str(),
                  dev->getLongId().toString().c_str());
        dht_->putEncrypted(dht::InfoHash::get("inbox:" + dev->getId().toString()), dev, answer);
    });
}

void
AccountManager::forEachDevice(
    const dht::InfoHash& to,
    std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)>&& op,
    std::function<void(bool)>&& end)
{
    if (not dht_) {
        JAMI_ERR("forEachDevice: no dht");
        if (end)
            end(false);
        return;
    }
    dht_->get<dht::crypto::RevocationList>(to, [to, this](dht::crypto::RevocationList&& crl) {
        certStore().pinRevocationList(to.toString(), std::move(crl));
        return true;
    });

    struct State
    {
        // Note: state is initialized to 1, because we need to wait that the get is finished
        unsigned remaining {1};
        std::set<dht::PkId> treatedDevices {};
        std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)> onDevice;
        std::function<void(bool)> onEnd;

        void found(std::shared_ptr<dht::crypto::PublicKey> pk)
        {
            remaining--;
            if (pk && *pk) {
                auto longId = pk->getLongId();
                if (treatedDevices.emplace(longId).second) {
                    onDevice(pk);
                }
            }
            ended();
        }

        void ended()
        {
            if (remaining == 0 && onEnd) {
                JAMI_DEBUG("Found {:d} devices", treatedDevices.size());
                onEnd(not treatedDevices.empty());
                onDevice = {};
                onEnd = {};
            }
        }
    };
    auto state = std::make_shared<State>();
    state->onDevice = std::move(op);
    state->onEnd = std::move(end);

    dht_->get<DeviceAnnouncement>(
        to,
        [this, to, state](DeviceAnnouncement&& dev) {
            if (dev.from != to)
                return true;
            state->remaining++;
            findCertificate(dev.dev, [state](const std::shared_ptr<dht::crypto::Certificate>& cert) {
                state->found(cert ? cert->getSharedPublicKey()
                                  : std::shared_ptr<dht::crypto::PublicKey> {});
            });
            return true;
        },
        [state](bool /*ok*/) { state->found({}); });
}

void
AccountManager::lookupUri(const std::string& name,
                          const std::string& defaultServer,
                          LookupCallback cb)
{
    nameDir_.get().lookupUri(name, defaultServer, std::move(cb));
}

void
AccountManager::lookupAddress(const std::string& addr, LookupCallback cb)
{
    nameDir_.get().lookupAddress(addr, cb);
}

dhtnet::tls::CertificateStore&
AccountManager::certStore() const
{
    return Manager::instance().certStore(info_->contacts->accountId());
}

} // namespace jami
