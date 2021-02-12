/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "archive_account_manager.h"
#include "accountarchive.h"
#include "fileutils.h"
#include "libdevcrypto/Common.h"
#include "archiver.h"
#include "base64.h"
#include "dring/account_const.h"
#include "account_schema.h"

#include <opendht/dhtrunner.h>
#include <opendht/thread_pool.h>

#include <memory>
#include <fstream>

namespace jami {

const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);

void
ArchiveAccountManager::initAuthentication(PrivateKey key,
                                          std::string deviceName,
                                          std::unique_ptr<AccountCredentials> credentials,
                                          AuthSuccessCallback onSuccess,
                                          AuthFailureCallback onFailure,
                                          OnChangeCallback onChange)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->key = key;
    ctx->request = buildRequest(key);
    ctx->deviceName = std::move(deviceName);
    ctx->credentials = dynamic_unique_cast<ArchiveAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);

    if (not ctx->credentials) {
        onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    onChange_ = std::move(onChange);

    if (ctx->credentials->scheme == "dht") {
        loadFromDHT(ctx);
        return;
    }

    dht::ThreadPool::computation().run([ctx = std::move(ctx), onAsync = onAsync_]() mutable {
        onAsync([ctx = std::move(ctx)](AccountManager& accountManager) mutable {
            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
            try {
                if (ctx->credentials->scheme == "file") {
                    // Import from external archive
                    this_.loadFromFile(*ctx);
                } else {
                    // Create/migrate local account
                    bool hasArchive = not ctx->credentials->uri.empty()
                                      and fileutils::isFile(ctx->credentials->uri);
                    if (hasArchive) {
                        // Create/migrate from local archive
                        if (ctx->credentials->updateIdentity.first
                            and ctx->credentials->updateIdentity.second
                            and needsMigration(ctx->credentials->updateIdentity)) {
                            this_.migrateAccount(*ctx);
                        } else {
                            this_.loadFromFile(*ctx);
                        }
                    } else if (ctx->credentials->updateIdentity.first
                               and ctx->credentials->updateIdentity.second) {
                        auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(
                            &dev::KeyPair::create);
                        AccountArchive a;
                        JAMI_WARN("[Auth] converting certificate from old account %s",
                                  ctx->credentials->updateIdentity.first->getPublicKey()
                                      .getId()
                                      .toString()
                                      .c_str());
                        a.id = std::move(ctx->credentials->updateIdentity);
                        try {
                            a.ca_key = std::make_shared<dht::crypto::PrivateKey>(
                                fileutils::loadFile("ca.key", this_.path_));
                        } catch (...) {
                        }
                        this_.updateCertificates(a, ctx->credentials->updateIdentity);
                        a.eth_key = future_keypair.get().secret().makeInsecure().asBytes();
                        this_.onArchiveLoaded(*ctx, std::move(a));
                    } else {
                        this_.createAccount(*ctx);
                    }
                }
            } catch (const std::exception& e) {
                ctx->onFailure(AuthError::UNKNOWN, e.what());
            }
        });
    });
}

bool
ArchiveAccountManager::updateCertificates(AccountArchive& archive, dht::crypto::Identity& device)
{
    JAMI_WARN("Updating certificates");
    using Certificate = dht::crypto::Certificate;

    // We need the CA key to resign certificates
    if (not archive.id.first or not *archive.id.first or not archive.id.second or not archive.ca_key
        or not *archive.ca_key)
        return false;

    // Currently set the CA flag and update expiration dates
    bool updated = false;

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    // Update CA if possible and relevant
    if (not ca or (not ca->issuer and (not ca->isCA() or ca->getExpiration() < clock::now()))) {
        ca = std::make_shared<Certificate>(
            Certificate::generate(*archive.ca_key, "Jami CA", {}, true));
        updated = true;
        JAMI_DBG("CA CRT re-generated");
    }

    // Update certificate
    if (updated or not cert->isCA() or cert->getExpiration() < clock::now()) {
        cert = std::make_shared<Certificate>(
            Certificate::generate(*archive.id.first,
                                  "Jami",
                                  dht::crypto::Identity {archive.ca_key, ca},
                                  true));
        updated = true;
        JAMI_DBG("Jami CRT re-generated");
    }

    if (updated and device.first and *device.first) {
        // update device certificate
        device.second = std::make_shared<Certificate>(
            Certificate::generate(*device.first, "Jami device", archive.id));
        JAMI_DBG("device CRT re-generated");
    }

    return updated;
}

void
ArchiveAccountManager::createAccount(AuthContext& ctx)
{
    AccountArchive a;
    auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(&dev::KeyPair::create);
    auto ca = dht::crypto::generateIdentity("Jami CA");
    if (!ca.first || !ca.second) {
        throw std::runtime_error("Can't generate CA for this account.");
    }
    a.id = dht::crypto::generateIdentity("Jami", ca, 4096, true);
    if (!a.id.first || !a.id.second) {
        throw std::runtime_error("Can't generate identity for this account.");
    }
    JAMI_WARN("[Auth] new account: CA: %s, RingID: %s",
              ca.second->getId().toString().c_str(),
              a.id.second->getId().toString().c_str());
    a.ca_key = ca.first;
    auto keypair = future_keypair.get();
    a.eth_key = keypair.secret().makeInsecure().asBytes();
    onArchiveLoaded(ctx, std::move(a));
}

void
ArchiveAccountManager::loadFromFile(AuthContext& ctx)
{
    JAMI_WARN("[Auth] loading archive from: %s", ctx.credentials->uri.c_str());
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx.credentials->uri, ctx.credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARN("[Auth] can't read file: %s", ex.what());
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, ex.what());
        return;
    }
    onArchiveLoaded(ctx, std::move(archive));
}

struct ArchiveAccountManager::DhtLoadContext
{
    dht::DhtRunner dht;
    std::pair<bool, bool> stateOld {false, true};
    std::pair<bool, bool> stateNew {false, true};
    bool found {false};
};

void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx)
{
    ctx->dhtContext = std::make_unique<DhtLoadContext>();
    ctx->dhtContext->dht.run(ctx->credentials->dhtPort, {}, true);
    for (const auto& bootstrap : ctx->credentials->dhtBootstrap)
        ctx->dhtContext->dht.bootstrap(bootstrap);
    auto searchEnded = [ctx]() {
        if (not ctx->dhtContext or ctx->dhtContext->found) {
            return;
        }
        auto& s = *ctx->dhtContext;
        if (s.stateOld.first && s.stateNew.first) {
            dht::ThreadPool::computation().run(
                [ctx, network_error = !s.stateOld.second && !s.stateNew.second] {
                    ctx->dhtContext.reset();
                    JAMI_WARN("[Auth] failure looking for archive on DHT: %s",
                              /**/ network_error ? "network error" : "not found");
                    ctx->onFailure(network_error ? AuthError::NETWORK : AuthError::UNKNOWN, "");
                });
        }
    };

    auto search = [ctx, searchEnded, onAsync = onAsync_](bool previous) {
        std::vector<uint8_t> key;
        dht::InfoHash loc;
        auto& s = previous ? ctx->dhtContext->stateOld : ctx->dhtContext->stateNew;

        // compute archive location and decryption keys
        try {
            std::tie(key, loc) = computeKeys(ctx->credentials->password,
                                             ctx->credentials->uri,
                                             previous);
            JAMI_DBG("[Auth] trying to load account from DHT with %s at %s",
                     /**/ ctx->credentials->uri.c_str(),
                     loc.toString().c_str());
            if (not ctx->dhtContext or ctx->dhtContext->found) {
                return;
            }
            ctx->dhtContext->dht.get(
                loc,
                [ctx, key = std::move(key), onAsync](const std::shared_ptr<dht::Value>& val) {
                    std::vector<uint8_t> decrypted;
                    try {
                        decrypted = archiver::decompress(dht::crypto::aesDecrypt(val->data, key));
                    } catch (const std::exception& ex) {
                        return true;
                    }
                    JAMI_DBG("[Auth] found archive on the DHT");
                    ctx->dhtContext->found = true;
                    dht::ThreadPool::computation().run([ctx,
                                                        decrypted = std::move(decrypted),
                                                        onAsync] {
                        try {
                            auto archive = AccountArchive(decrypted);
                            onAsync([&](AccountManager& accountManager) {
                                auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
                                if (ctx->dhtContext) {
                                    ctx->dhtContext->dht.join();
                                    ctx->dhtContext.reset();
                                }
                                this_.onArchiveLoaded(*ctx,
                                                      std::move(archive) /*, std::move(contacts)*/);
                            });
                        } catch (const std::exception& e) {
                            ctx->onFailure(AuthError::UNKNOWN, "");
                        }
                    });
                    return not ctx->dhtContext->found;
                },
                [=, &s](bool ok) {
                    JAMI_DBG("[Auth] DHT archive search ended at %s", /**/ loc.toString().c_str());
                    s.first = true;
                    s.second = ok;
                    searchEnded();
                });
        } catch (const std::exception& e) {
            // JAMI_ERR("Error computing kedht::ThreadPool::computation().run(ys: %s", e.what());
            s.first = true;
            s.second = true;
            searchEnded();
            return;
        }
    };
    dht::ThreadPool::computation().run(std::bind(search, true));
    dht::ThreadPool::computation().run(std::bind(search, false));
}

void
ArchiveAccountManager::migrateAccount(AuthContext& ctx)
{
    JAMI_WARN("[Auth] account migration needed");
    AccountArchive archive;
    try {
        archive = readArchive(ctx.credentials->password);
    } catch (...) {
        JAMI_DBG("[Auth] Can't load archive");
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
        return;
    }

    updateArchive(archive);

    if (updateCertificates(archive, ctx.credentials->updateIdentity)) {
        onArchiveLoaded(ctx, std::move(archive));
    } else
        ctx.onFailure(AuthError::UNKNOWN, "");
}

void
ArchiveAccountManager::onArchiveLoaded(AuthContext& ctx, AccountArchive&& a)
{
    auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    fileutils::check_dir(path_.c_str(), 0700);

    auto path = fileutils::getFullPath(path_, archivePath_);
    a.save(path, ctx.credentials ? ctx.credentials->password : "");

    if (not a.id.second->isCA()) {
        JAMI_ERR("[Auth] trying to sign a certificate with a non-CA.");
    }
    JAMI_WARN("[Auth] creating new device certificate");

    auto request = ctx.request.get();
    if (not request->verify()) {
        JAMI_ERR("[Auth] Invalid certificate request.");
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
        return;
    }

    auto deviceCertificate = std::make_shared<dht::crypto::Certificate>(
        dht::crypto::Certificate::generate(*request, a.id));
    auto receipt = makeReceipt(a.id, *deviceCertificate, ethAccount);
    auto receiptSignature = a.id.first->sign({receipt.first.begin(), receipt.first.end()});

    auto info = std::make_unique<AccountInfo>();
    info->identity.first = ctx.key.get();
    info->identity.second = deviceCertificate;
    info->accountId = a.id.second->getId().toString();
    info->deviceId = deviceCertificate->getPublicKey().getId().toString();
    if (ctx.deviceName.empty())
        ctx.deviceName = info->deviceId.substr(8);

    info->contacts = std::make_unique<ContactList>(a.id.second, path_, onChange_);
    info->contacts->setContacts(a.contacts);
    info->contacts->foundAccountDevice(deviceCertificate, ctx.deviceName, clock::now());
    info->conversations = a.conversations;
    info->conversationsRequests = a.conversationsRequests;
    info->ethAccount = ethAccount;
    info->announce = std::move(receipt.second);
    info_ = std::move(info);
    saveConvInfos();
    saveConvRequests();

    JAMI_WARN("[Auth] created new device: %s", info_->deviceId.c_str());
    ctx.onSuccess(*info_,
                  std::move(a.config),
                  std::move(receipt.first),
                  std::move(receiptSignature));
}

std::pair<std::vector<uint8_t>, dht::InfoHash>
ArchiveAccountManager::computeKeys(const std::string& password,
                                   const std::string& pin,
                                   bool previous)
{
    // Compute time seed
    auto now = std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch());
    auto tseed = now.count() / std::chrono::seconds(EXPORT_KEY_RENEWAL_TIME).count();
    if (previous)
        tseed--;
    std::ostringstream ss;
    ss << std::hex << tseed;
    auto tseed_str = ss.str();

    // Generate key for archive encryption, using PIN as the salt
    std::vector<uint8_t> salt_key;
    salt_key.reserve(pin.size() + tseed_str.size());
    salt_key.insert(salt_key.end(), pin.begin(), pin.end());
    salt_key.insert(salt_key.end(), tseed_str.begin(), tseed_str.end());
    auto key = dht::crypto::stretchKey(password, salt_key, 256 / 8);

    // Generate public storage location as SHA1(key).
    auto loc = dht::InfoHash::get(key);

    return {key, loc};
}

std::pair<std::string, std::shared_ptr<dht::Value>>
ArchiveAccountManager::makeReceipt(const dht::crypto::Identity& id,
                                   const dht::crypto::Certificate& device,
                                   const std::string& ethAccount)
{
    JAMI_DBG("[Auth] signing device receipt");
    auto devId = device.getId();
    DeviceAnnouncement announcement;
    announcement.dev = devId;
    dht::Value ann_val {announcement};
    ann_val.sign(*id.first);

    std::ostringstream is;
    is << "{\"id\":\"" << id.second->getId() << "\",\"dev\":\"" << devId << "\",\"eth\":\""
       << ethAccount << "\",\"announce\":\"" << base64::encode(ann_val.getPacked()) << "\"}";

    // auto announce_ = ;
    return {is.str(), std::make_shared<dht::Value>(std::move(ann_val))};
}

bool
ArchiveAccountManager::needsMigration(const dht::crypto::Identity& id)
{
    if (not id.second)
        return false;
    auto cert = id.second->issuer;
    while (cert) {
        if (not cert->isCA()) {
            JAMI_WARN("certificate %s is not a CA, needs update.", cert->getId().toString().c_str());
            return true;
        }
        if (cert->getExpiration() < clock::now()) {
            JAMI_WARN("certificate %s is expired, needs update.", cert->getId().toString().c_str());
            return true;
        }
        cert = cert->issuer;
    }
    return false;
}

void
ArchiveAccountManager::syncDevices()
{
    if (not dht_ or not dht_->isRunning()) {
        JAMI_WARN("Not syncing devices: DHT is not running");
        return;
    }
    JAMI_DBG("Building device sync from %s", info_->deviceId.c_str());
    auto sync_data = info_->contacts->getSyncData();

    for (const auto& dev : getKnownDevices()) {
        // don't send sync data to ourself
        if (dev.first.toString() == info_->deviceId)
            continue;
        JAMI_DBG("sending device sync to %s %s",
                 dev.second.name.c_str(),
                 dev.first.toString().c_str());
        auto syncDeviceKey = dht::InfoHash::get("inbox:" + dev.first.toString());
        dht_->putEncrypted(syncDeviceKey, dev.first, sync_data);
    }
}

void
ArchiveAccountManager::startSync(const OnNewDeviceCb& cb)
{
    AccountManager::startSync(std::move(cb));

    dht_->listen<DeviceSync>(dht::InfoHash::get("inbox:" + info_->deviceId), [this](DeviceSync&& sync) {
        // Received device sync data.
        // check device certificate
        findCertificate(sync.from,
                        [this, sync](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                            if (!cert or cert->getId() != sync.from) {
                                JAMI_WARN("Can't find certificate for device %s",
                                          sync.from.toString().c_str());
                                return;
                            }
                            if (not foundAccountDevice(cert))
                                return;
                            onSyncData(std::move(sync));
                        });

        return true;
    });
}

void
ArchiveAccountManager::onSyncData(DeviceSync&& sync)
{
    auto sync_date = clock::time_point(clock::duration(sync.date));
    if (not info_->contacts->syncDevice(sync.from, sync_date)) {
        return;
    }

    // Sync known devices
    JAMI_DBG("[Contacts] received device sync data (%lu devices, %lu contacts)",
             sync.devices_known.size(),
             sync.peers.size());
    for (const auto& d : sync.devices_known) {
        findCertificate(d.first, [this, d](const std::shared_ptr<dht::crypto::Certificate>& crt) {
            if (not crt)
                return;
            // std::lock_guard<std::mutex> lock(deviceListMutex_);
            foundAccountDevice(crt, d.second);
        });
    }
    // saveKnownDevices();

    // Sync contacts
    for (const auto& peer : sync.peers)
        info_->contacts->updateContact(peer.first, peer.second);
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

AccountArchive
ArchiveAccountManager::readArchive(const std::string& pwd) const
{
    JAMI_DBG("[Auth] reading account archive");
    return AccountArchive(fileutils::getFullPath(path_, archivePath_), pwd);
}

void
ArchiveAccountManager::updateArchive(AccountArchive& archive) const
{
    using namespace DRing::Account::ConfProperties;

    // Keys not exported to archive
    static const auto filtered_keys = {Ringtone::PATH,
                                       ARCHIVE_PATH,
                                       DEVICE_ID,
                                       DEVICE_NAME,
                                       Conf::CONFIG_DHT_PORT,
                                       DHT_PROXY_LIST_URL,
                                       AUTOANSWER,
                                       PROXY_ENABLED,
                                       PROXY_SERVER,
                                       PROXY_PUSH_TOKEN};

    // Keys with meaning of file path where the contents has to be exported in base64
    static const auto encoded_keys = {TLS::CA_LIST_FILE,
                                      TLS::CERTIFICATE_FILE,
                                      TLS::PRIVATE_KEY_FILE};

    JAMI_DBG("[Auth] building account archive");
    for (const auto& it : onExportConfig_()) {
        // filter-out?
        if (std::any_of(std::begin(filtered_keys), std::end(filtered_keys), [&](const auto& key) {
                return key == it.first;
            }))
            continue;

        // file contents?
        if (std::any_of(std::begin(encoded_keys), std::end(encoded_keys), [&](const auto& key) {
                return key == it.first;
            })) {
            try {
                archive.config.emplace(it.first, base64::encode(fileutils::loadFile(it.second)));
            } catch (...) {
            }
        } else
            archive.config[it.first] = it.second;
    }
    archive.contacts = info_->contacts->getContacts();
    archive.conversations = info_->conversations;
    archive.conversationsRequests = info_->conversationsRequests;
}

void
ArchiveAccountManager::saveArchive(AccountArchive& archive, const std::string& pwd)
{
    try {
        updateArchive(archive);
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        archive.save(fileutils::getFullPath(path_, archivePath_), pwd);
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("[Auth] Can't export archive: %s", ex.what());
        return;
    }
}

bool
ArchiveAccountManager::changePassword(const std::string& password_old,
                                      const std::string& password_new)
{
    try {
        auto path = fileutils::getFullPath(path_, archivePath_);
        AccountArchive(path, password_old).save(path, password_new);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string
generatePIN(size_t length = 8)
{
    static constexpr const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    dht::crypto::random_device rd;
    std::uniform_int_distribution<size_t> dis(0, sizeof(alphabet) - 2);
    std::string ret;
    ret.reserve(length);
    for (size_t i = 0; i < length; i++)
        ret.push_back(alphabet[dis(rd)]);
    return ret;
}

void
ArchiveAccountManager::addDevice(const std::string& password, AddDeviceCallback cb)
{
    dht::ThreadPool::computation().run([onAsync = onAsync_, password, cb = std::move(cb)]() mutable {
        onAsync([password = std::move(password),
                 cb = std::move(cb)](AccountManager& accountManager) mutable {
            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);

            std::vector<uint8_t> key;
            dht::InfoHash loc;
            std::string pin_str;
            AccountArchive a;
            try {
                JAMI_DBG("[Auth] exporting account");

                a = this_.readArchive(password);

                // Generate random PIN
                pin_str = generatePIN();

                std::tie(key, loc) = computeKeys(password, pin_str);
            } catch (const std::exception& e) {
                JAMI_ERR("[Auth] can't export account: %s", e.what());
                cb(AddDeviceResult::ERROR_CREDENTIALS, {});
                return;
            }
            // now that key and loc are computed, display to user in lowercase
            std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::tolower);
            try {
                this_.updateArchive(a);
                auto encrypted = dht::crypto::aesEncrypt(archiver::compress(a.serialize()), key);
                if (not this_.dht_ or not this_.dht_->isRunning())
                    throw std::runtime_error("DHT is not running..");
                JAMI_WARN("[Auth] exporting account with PIN: %s at %s (size %zu)",
                          pin_str.c_str(),
                          loc.toString().c_str(),
                          encrypted.size());
                this_.dht_->put(loc, encrypted, [cb, pin = std::move(pin_str)](bool ok) {
                    JAMI_DBG("[Auth] account archive published: %s", ok ? "success" : "failure");
                    if (ok)
                        cb(AddDeviceResult::SUCCESS_SHOW_PIN, pin);
                    else
                        cb(AddDeviceResult::ERROR_NETWORK, {});
                });
            } catch (const std::exception& e) {
                JAMI_ERR("[Auth] can't export account: %s", e.what());
                cb(AddDeviceResult::ERROR_NETWORK, {});
                return;
            }
        });
    });
}

bool
ArchiveAccountManager::revokeDevice(const std::string& password,
                                    const std::string& device,
                                    RevokeDeviceCallback cb)
{
    auto fa = dht::ThreadPool::computation().getShared<AccountArchive>(
        [this, password] { return readArchive(password); });
    findCertificate(dht::InfoHash(device),
                    [fa = std::move(fa), password, device, cb, onAsync = onAsync_](
                        const std::shared_ptr<dht::crypto::Certificate>& crt) mutable {
                        if (not crt) {
                            cb(RevokeDeviceResult::ERROR_NETWORK);
                            return;
                        }
                        onAsync([cb,
                                 crt = std::move(crt),
                                 fa = std::move(fa),
                                 password = std::move(password)](
                                    AccountManager& accountManager) mutable {
                            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
                            this_.info_->contacts->foundAccountDevice(crt);
                            AccountArchive a;
                            try {
                                a = fa.get();
                            } catch (...) {
                                cb(RevokeDeviceResult::ERROR_CREDENTIALS);
                                return;
                            }
                            // Add revoked device to the revocation list and resign it
                            if (not a.revoked)
                                a.revoked = std::make_shared<decltype(a.revoked)::element_type>();
                            a.revoked->revoke(*crt);
                            a.revoked->sign(a.id);
                            // add to CRL cache
                            tls::CertificateStore::instance()
                                .pinRevocationList(a.id.second->getId().toString(), a.revoked);
                            tls::CertificateStore::instance().loadRevocations(*a.id.second);

                            this_.saveArchive(a, password);
                            this_.info_->contacts->removeAccountDevice(crt->getId());
                            cb(RevokeDeviceResult::SUCCESS);
                            this_.syncDevices();
                        });
                    });
    return false;
}

bool
ArchiveAccountManager::exportArchive(const std::string& destinationPath, const std::string& password)
{
    try {
        // Save contacts if possible before exporting
        AccountArchive archive = readArchive(password);
        updateArchive(archive);
        archive.save(fileutils::getFullPath(path_, archivePath_), password);

        // Export the file
        auto sourcePath = fileutils::getFullPath(path_, archivePath_);
        std::ifstream src(sourcePath, std::ios::in | std::ios::binary);
        if (!src)
            return false;
        std::ofstream dst(destinationPath, std::ios::out | std::ios::binary);
        dst << src.rdbuf();
        return true;
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("[Auth] Can't export archive: %s", ex.what());
        return false;
    } catch (...) {
        JAMI_ERR("[Auth] Can't export archive: can't read archive");
        return false;
    }
}

bool
ArchiveAccountManager::isPasswordValid(const std::string& password)
{
    try {
        readArchive(password);
        return true;
    } catch (...) {
        return false;
    }
}

#if HAVE_RINGNS
void
ArchiveAccountManager::registerName(const std::string& password,
                                    const std::string& name,
                                    RegistrationCallback cb)
{
    std::string signedName;
    auto nameLowercase {name};
    std::transform(nameLowercase.begin(), nameLowercase.end(), nameLowercase.begin(), ::tolower);
    std::string publickey;
    std::string accountId;
    std::string ethAccount;

    try {
        auto archive = readArchive(password);
        auto privateKey = archive.id.first;
        auto pk = privateKey->getPublicKey();
        publickey = pk.toString();
        accountId = pk.getId().toString();
        signedName = base64::encode(
            privateKey->sign(std::vector<uint8_t>(nameLowercase.begin(), nameLowercase.end())));
        ethAccount = dev::KeyPair(dev::Secret(archive.eth_key)).address().hex();
    } catch (const std::exception& e) {
        // JAMI_ERR("[Auth] can't export account: %s", e.what());
        cb(NameDirectory::RegistrationResponse::invalidCredentials);
        return;
    }

    nameDir_.get().registerName(accountId, nameLowercase, ethAccount, cb, signedName, publickey);
}
#endif

} // namespace jami
