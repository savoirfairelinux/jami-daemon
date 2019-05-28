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

namespace jami {

const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);

template <typename To, typename From>
std::unique_ptr<To> dynamic_unique_cast(std::unique_ptr<From>&& p) {
    if (auto cast = dynamic_cast<To*>(p.get())) {
        std::unique_ptr<To> result(cast);
        p.release();
        return result;
    }
    return {};
}

void
ArchiveAccountManager::initAuthentication(
    std::unique_ptr<dht::crypto::CertificateRequest> request,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    DeviceSyncCallback onSync)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->request = std::move(request);
    ctx->credentials = dynamic_unique_cast<ArchiveAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);

    if (not ctx->credentials) {
        onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    if (ctx->credentials and ctx->credentials->scheme == "dht") {
        loadFromDHT(ctx);
        return;
    }

    dht::ThreadPool::computation().run([
        ctx = std::move(ctx),
        onAsync = onAsync_
    ]() mutable {
        onAsync([ctx = std::move(ctx)](AccountManager& accountManager) mutable {
            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
            try {
                if (ctx->credentials and ctx->credentials->scheme == "file") {
                    this_.loadFromFile(ctx);
                    return;
                } else {
                    if (ctx->credentials->updateIdentity.first and ctx->credentials->updateIdentity.second) {
                        auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(&dev::KeyPair::create);
                        AccountArchive a;
                        JAMI_WARN("[Auth] converting certificate from old account %s", ctx->credentials->updateIdentity.first->getPublicKey().getId().toString().c_str());
                        a.id = std::move(ctx->credentials->updateIdentity);
                        try {
                            a.ca_key = std::make_shared<dht::crypto::PrivateKey>(fileutils::loadFile("ca.key", this_.getAccount().getPath()));
                        } catch (...) {}
                        this_.updateCertificates(a, ctx->credentials->updateIdentity);
                        auto keypair = future_keypair.get();
                        a.eth_key = keypair.secret().makeInsecure().asBytes();
                        auto contactList = std::make_unique<ContactList>(this_.getAccount(), a.id.second);
                        this_.onArchiveLoaded(*ctx, std::move(a), std::move(contactList));
                    } else {
                        this_.createAccount(ctx);
                    }
                    /*AccountArchive a;
                    auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(&dev::KeyPair::create);
                    auto ca = dht::crypto::generateIdentity("Jami CA");
                    if (!ca.first || !ca.second) {
                        throw std::runtime_error("Can't generate CA for this account.");
                    }
                    a.id = dht::crypto::generateIdentity("Jami", ca, 4096, true);
                    if (!a.id.first || !a.id.second) {
                        throw std::runtime_error("Can't generate identity for this account.");
                    }
                    JAMI_WARN("[Account %s] new account: CA: %s, RingID: %s",
                                this_.getAccount().getAccountID().c_str(), ca.second->getId().toString().c_str(),
                                a.id.second->getId().toString().c_str());
                    a.ca_key = ca.first;
                    auto keypair = future_keypair.get();
                    a.eth_key = keypair.secret().makeInsecure().asBytes();
                    auto contactList = std::make_unique<ContactList>(this_.getAccount(), a.id.second);
                    this_.onArchiveLoaded(*ctx, std::move(a), std::move(contactList));*/
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
    if (not archive.id.first or
        not *archive.id.first or
        not archive.id.second or
        not archive.ca_key or
        not *archive.ca_key)
        return false;

    // Currently set the CA flag and update expiration dates
    bool updated = false;

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    // Update CA if possible and relevant
    if (not ca or (not ca->issuer and (not ca->isCA() or ca->getExpiration() < clock::now()))) {
        ca = std::make_shared<Certificate>(Certificate::generate(*archive.ca_key, "Jami CA", {}, true));
        updated = true;
        JAMI_DBG("CA CRT re-generated");
    }

    // Update certificate
    if (updated or not cert->isCA() or cert->getExpiration() < clock::now()) {
        cert = std::make_shared<Certificate>(Certificate::generate(*archive.id.first, "Jami", dht::crypto::Identity{archive.ca_key, ca}, true));
        updated = true;
        JAMI_DBG("Jami CRT re-generated");
    }

    if (updated and device.first and *device.first) {
        // update device certificate
        device.second = std::make_shared<Certificate>(Certificate::generate(*device.first, "Jami device", archive.id));
        JAMI_DBG("device CRT re-generated");
    }

    return updated;
}

void
ArchiveAccountManager::createAccount(const std::shared_ptr<AuthContext>& ctx)
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
    JAMI_WARN("[Account %s] new account: CA: %s, RingID: %s",
                getAccount().getAccountID().c_str(), ca.second->getId().toString().c_str(),
                a.id.second->getId().toString().c_str());
    a.ca_key = ca.first;
    auto keypair = future_keypair.get();
    a.eth_key = keypair.secret().makeInsecure().asBytes();
    auto contactList = std::make_unique<ContactList>(getAccount(), a.id.second);
    onArchiveLoaded(*ctx, std::move(a), std::move(contactList));
}

void
ArchiveAccountManager::loadFromFile(const std::shared_ptr<AuthContext>& ctx)
{
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx->credentials->uri, ctx->credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARN("[Account %s] can't read file: %s", getAccount().getAccountID().c_str(), ex.what());
        ctx->onFailure(AuthError::UNKNOWN, ex.what());
        return;
    }
    auto contactList = std::make_unique<ContactList>(getAccount(), archive.id.second);
    onArchiveLoaded(*ctx, std::move(archive), std::move(contactList));
}

struct ArchiveAccountManager::DhtLoadContext {
    dht::DhtRunner dht;
    std::pair<bool, bool> stateOld {false, true};
    std::pair<bool, bool> stateNew {false, true};
    bool found {false};
};

// must be called while configurationMutex_ is locked
void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx)
{
    ctx->dhtContext = std::make_unique<DhtLoadContext>();
    ctx->dhtContext->dht.run(ctx->credentials->dhtPort, {}, true);
    ctx->dhtContext->dht.bootstrap(ctx->credentials->dhtBootstrap);
    auto searchEnded = [ctx]() {
        if (not ctx->dhtContext or ctx->dhtContext->found) {
            return;
        }
        auto& s = *ctx->dhtContext;
        if (s.stateOld.first && s.stateNew.first) {
            dht::ThreadPool::computation().run([ctx, network_error = !s.stateOld.second && !s.stateNew.second]{
                ctx->dhtContext.reset();
                JAMI_WARN("[Auth] failure looking for archive on DHT: %s", /*getAccount().getAccountID().c_str(), */network_error ? "network error" : "not found");
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
            std::tie(key, loc) = computeKeys(ctx->credentials->password, ctx->credentials->uri, previous);
            JAMI_DBG("[Auth] trying to load account from DHT with %s at %s", /*getAccount().getAccountID().c_str(), */ctx->credentials->uri.c_str(), loc.toString().c_str());
            ctx->dhtContext->dht.get(loc, [ctx, key=std::move(key), onAsync](const std::shared_ptr<dht::Value>& val) {
                std::vector<uint8_t> decrypted;
                try {
                    decrypted = archiver::decompress(dht::crypto::aesDecrypt(val->data, key));
                } catch (const std::exception& ex) {
                    return true;
                }
                JAMI_DBG("[Auth] found archive on the DHT");
                ctx->dhtContext->found =  true;
                dht::ThreadPool::computation().run([
                    ctx,
                    decrypted = std::move(decrypted),
                    onAsync
                ]{
                    try {
                        auto archive = AccountArchive(decrypted);
                        onAsync([&](AccountManager& accountManager) {
                            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
                            auto contacts = std::make_unique<ContactList>(this_.getAccount(), archive.id.second);
                            if (ctx->dhtContext) {
                                ctx->dhtContext->dht.join();
                                ctx->dhtContext.reset();
                            }
                            this_.onArchiveLoaded(*ctx, std::move(archive), std::move(contacts));
                        });
                    } catch (const std::exception& e) {
                        ctx->onFailure(AuthError::UNKNOWN, "");
                    }
                });
                return not ctx->dhtContext->found;
            }, [=, &s](bool ok) {
                JAMI_DBG("[Auth] DHT archive search ended at %s", /*getAccount().getAccountID().c_str(), */loc.toString().c_str());
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
ArchiveAccountManager::onArchiveLoaded(
    AuthContext& ctx,
    AccountArchive&& a,
    std::unique_ptr<ContactList>&& contactList)
{
    auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    auto path = fileutils::getFullPath(getAccount().getPath(), archivePath_);
    JAMI_WARN("saving archive to %s", path.c_str());
    a.save(path, ctx.credentials ? ctx.credentials->password : "");

    if (not a.id.second->isCA()) {
        JAMI_ERR("[Account %s] trying to sign a certificate with a non-CA.", getAccount().getAccountID().c_str());
    }
    JAMI_WARN("generating device certificate");
    auto deviceCertificate = std::make_shared<dht::crypto::Certificate>(dht::crypto::Certificate::generate(*ctx.request, a.id));

    if (not ctx.request->verify()) {
        JAMI_ERR("[Account %s] Invalid certificate request.", getAccount().getAccountID().c_str());
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
        return;
    }

    fileutils::check_dir(getAccount().getPath().c_str(), 0700);

    auto deviceId = deviceCertificate->getPublicKey().getId();
    auto receipt = makeReceipt(a.id, *deviceCertificate, ethAccount);
    auto receiptSignature = a.id.first->sign({receipt.first.begin(), receipt.first.end()});
    JAMI_WARN("[Account %s] created new device: %s",
            getAccount().getAccountID().c_str(), deviceId.toString().c_str());

    auto info = std::make_unique<AcccountInfo>();
    info->identity = a.id;
    info->contacts = std::move(contactList);
    info->accountId = a.id.second->getId().toString();
    info->deviceId = deviceId.toString();
    info->ethAccount = ethAccount;
    info->announce = std::move(receipt.second);

    ctx.onSuccess(deviceCertificate, {}, std::move(info), std::move(receipt.first), std::move(receiptSignature));
}

std::pair<std::vector<uint8_t>, dht::InfoHash>
ArchiveAccountManager::computeKeys(const std::string& password, const std::string& pin, bool previous)
{
    // Compute time seed
    auto now = std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch());
    auto tseed = now.count() / std::chrono::seconds(EXPORT_KEY_RENEWAL_TIME).count();
    if (previous)
        tseed--;
    std::stringstream ss;
    ss << std::hex << tseed;
    auto tseed_str = ss.str();

    // Generate key for archive encryption, using PIN as the salt
    std::vector<uint8_t> salt_key;
    salt_key.reserve(pin.size() + tseed_str.size());
    salt_key.insert(salt_key.end(), pin.begin(), pin.end());
    salt_key.insert(salt_key.end(), tseed_str.begin(), tseed_str.end());
    auto key = dht::crypto::stretchKey(password, salt_key, 256/8);

    // Generate public storage location as SHA1(key).
    auto loc = dht::InfoHash::get(key);

    return {key, loc};
}

std::pair<std::string, std::shared_ptr<dht::Value>>
ArchiveAccountManager::makeReceipt(const dht::crypto::Identity& id, const dht::crypto::Certificate& device, const std::string& ethAccount)
{
    JAMI_DBG("[Account %s] signing device receipt", getAccount().getAccountID().c_str());
    auto devId = device.getId();
    DeviceAnnouncement announcement;
    announcement.dev = devId;
    dht::Value ann_val {announcement};
    ann_val.sign(*id.first);

    std::ostringstream is;
    is << "{\"id\":\"" << id.second->getId()
    << "\",\"dev\":\"" << devId
    << "\",\"eth\":\"" << ethAccount
    << "\",\"announce\":\"" << base64::encode(ann_val.getPacked()) << "\"}";

    //auto announce_ = ;
    return {is.str(), std::make_shared<dht::Value>(std::move(ann_val))};
}

bool
ArchiveAccountManager::needsMigration(const dht::crypto::Identity& id)
{
    if (not id.second)
        return false;
    auto cert = id.second->issuer;
    while (cert) {
        if (not cert->isCA()){
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

dht::crypto::Identity
AccountManager::loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const
{
    JAMI_DBG("Loading certificate from '%s' and key from '%s'", crt_path.c_str(), key_path.c_str());
    try {
        dht::crypto::Certificate dht_cert(fileutils::loadFile(crt_path, getAccount().getPath()));
        dht::crypto::PrivateKey  dht_key(fileutils::loadFile(key_path, getAccount().getPath()), key_pwd);
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

std::unique_ptr<AcccountInfo>
AccountManager::useIdentity(const dht::crypto::Identity& identity, const std::string& receipt, const std::vector<uint8_t> receiptSignature)
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
    auto contactList = std::make_unique<ContactList>(getAccount(), accountCertificate);
    if (not contactList->isValidAccountDevice(*identity.second)) {
        JAMI_ERR("[Auth] can't use identity: device certificate chain can't be verified");
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

    dht::Value announce_val;
    try {
        auto announce = base64::decode(root["announce"].asString());
        msgpack::object_handle announce_msg = msgpack::unpack((const char*)announce.data(), announce.size());
        announce_val.msgpack_unpack(announce_msg.get());
        if (not announce_val.checkSignature()) {
            JAMI_ERR("[Auth] announce signature check failed");
            return nullptr;
        }
        DeviceAnnouncement da;
        da.unpackValue(announce_val);
        if (da.from.toString() != id or da.dev.toString() != dev_id) {
            JAMI_ERR("[Auth] device ID mismatch in announce");
            return nullptr;
        }
    } catch (const std::exception& e) {
        JAMI_ERR("[Auth] can't read announce: %s", e.what());
        return nullptr;
    }

    auto info = std::make_unique<AcccountInfo>();
    info->identity = identity;
    info->contacts = std::move(contactList);
    info->accountId = id;
    info->deviceId = identity.first->getPublicKey().getId().toString();
    info->announce = std::make_shared<dht::Value>(std::move(announce_val));
    info->ethAccount = root["eth"].asString();

    // success, make use of this identity (certificate chain and private key)
    /*identity_ = identity;
    contactList_ = std::move(contactList);
    ringAccountId_ = id;
    ringDeviceId_ = identity.first->getPublicKey().getId().toString();
    username_ = RING_URI_PREFIX + id;
    announce_ = std::make_shared<dht::Value>(std::move(announce_val));
    ethAccount_ = root["eth"].asString();*/

    JAMI_DBG("[Auth] Account %s device %s receipt checked successfully", id.c_str(), info->deviceId.c_str());
    return info;
}

AccountArchive
ArchiveAccountManager::readArchive(const std::string& pwd) const
{
    JAMI_DBG("[Account %s] reading account archive", getAccount().getAccountID().c_str());
    return AccountArchive(fileutils::getFullPath(getAccount().getPath(), archivePath_), pwd);
}

void
ArchiveAccountManager::updateArchive(AccountArchive& archive/*, const ContactList& syncData*/) const
{
    using namespace DRing::Account::ConfProperties;

    // Keys not exported to archive
    static const auto filtered_keys = { Ringtone::PATH,
                                        ARCHIVE_PATH,
                                        RING_DEVICE_ID,
                                        RING_DEVICE_NAME,
                                        Conf::CONFIG_DHT_PORT };

    // Keys with meaning of file path where the contents has to be exported in base64
    static const auto encoded_keys = { TLS::CA_LIST_FILE,
                                       TLS::CERTIFICATE_FILE,
                                       TLS::PRIVATE_KEY_FILE };

    JAMI_DBG("[Account %s] building account archive", getAccount().getAccountID().c_str());
    for (const auto& it : getAccount().getAccountDetails()) {
        // filter-out?
        if (std::any_of(std::begin(filtered_keys), std::end(filtered_keys),
                        [&](const auto& key){ return key == it.first; }))
            continue;

        // file contents?
        if (std::any_of(std::begin(encoded_keys), std::end(encoded_keys),
                        [&](const auto& key){ return key == it.first; })) {
            try {
                archive.config.emplace(it.first, base64::encode(fileutils::loadFile(it.second)));
            } catch (...) {}
        } else
            archive.config.insert(it);
    }
    //archive.contacts = syncData.getContacts();
}

void
ArchiveAccountManager::saveArchive(AccountArchive& archive, const std::string& pwd)
{
    try {
        updateArchive(archive);
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        archive.save(fileutils::getFullPath(getAccount().getPath(), archivePath_), pwd);
        //archiveHasPassword_ = not pwd.empty();
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("[Account %s] Can't export archive: %s", getAccount().getAccountID().c_str(), ex.what());
        return;
    }
}

bool
ArchiveAccountManager::changePassword(const std::string& password_old, const std::string& password_new)
{
    try {
        auto path = fileutils::getFullPath(getAccount().getPath(), archivePath_);
        AccountArchive(path, password_old).save(path, password_new);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/*
#if HAVE_RINGNS
void
AccountManager::lookupName(const std::string& name, LookupCallback cb)
{
    auto acc = getAccountID();
    NameDirectory::lookupUri(name, nameServer_, cb);
}

void
AccountManager::lookupAddress(const std::string& addr, LookupCallback cb)
{
    auto acc = getAccountID();
    nameDir_.get().lookupAddress(addr, cb);
}

void
ArchiveAccountManager::registerName(const std::string& password, const std::string& name)
{
    std::string signedName;
    auto nameLowercase {name};
    std::transform(nameLowercase.begin(), nameLowercase.end(), nameLowercase.begin(), ::tolower);
    std::string publickey;
    try {
        auto privateKey = readArchive(password).id.first;
        signedName = base64::encode(privateKey->sign(std::vector<uint8_t>(nameLowercase.begin(), nameLowercase.end())));
        publickey = privateKey->getPublicKey().toString();
    } catch (const std::exception& e) {
        JAMI_ERR("[Account %s] can't export account: %s", getAccountID().c_str(), e.what());
        emitSignal<DRing::ConfigurationSignal::NameRegistrationEnded>(getAccountID(), 1, name);
        return;
    }

    nameDir_.get().registerName(accountInfo_->accountId, nameLowercase, accountInfo_->ethAccount, [acc=getAccountID(), name, w=weak()](NameDirectory::RegistrationResponse response){
        int res = (response == NameDirectory::RegistrationResponse::success)      ? 0 : (
                  (response == NameDirectory::RegistrationResponse::invalidName)  ? 2 : (
                  (response == NameDirectory::RegistrationResponse::alreadyTaken) ? 3 : 4));
        if (response == NameDirectory::RegistrationResponse::success) {
            if (auto this_ = w.lock()) {
                std::unique_lock<std::mutex> lock(this_->configurationMutex_);
                this_->registeredName_ = name;
            }
        }
        emitSignal<DRing::ConfigurationSignal::NameRegistrationEnded>(acc, res, name);
    }, signedName, publickey);
}
#endif
*/

}
