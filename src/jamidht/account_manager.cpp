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

void
ArchiveAccountManager::initAuthentication(
    std::unique_ptr<dht::crypto::CertificateRequest> request,
    std::unique_ptr<AccountCredentials> credentials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    DeviceSyncCallback onSync)
{
    JAMI_WARN("initAuthentication 0");
    auto creds = std::dynamic_pointer_cast<ArchiveAccountCredentials>(std::shared_ptr<AccountCredentials>(std::move(credentials)));
    if (not creds) {
        onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    auto ctx = std::make_shared<AuthContext>();
    ctx->request = std::move(request);
    ctx->credentials = std::move(credentials);
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);

    dht::ThreadPool::computation().run([
        ctx = std::move(ctx),
        onAsync = onAsync_
    ]() mutable {
        onAsync([ctx = std::move(ctx)](AccountManager& accountManager) mutable {
            auto& this_ = *static_cast<ArchiveAccountManager*>(&accountManager);
            try {
                AccountArchive a;
                std::unique_ptr<ContactList> contactList;

                if (ctx->credentials->scheme == "dht") {
                    ctx->onFailure(AuthError::UNKNOWN, "");
                    return;
                } else if (ctx->credentials->scheme == "file") {
                    ctx->onFailure(AuthError::UNKNOWN, "");
                    return;
                } else {
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
                    //this_.ethAccount_ = keypair.address().hex();
                    a.eth_key = keypair.secret().makeInsecure().asBytes();
                    contactList = std::make_unique<ContactList>(this_.getAccount(), a.id.second);
                }
                this_.onArchiveLoaded(*ctx, std::move(a), std::move(contactList));
            } catch (const std::exception& e) {
                ctx->onFailure(AuthError::UNKNOWN, e.what());
            }
        });
    });
}

void
ArchiveAccountManager::loadFromFile(const std::shared_ptr<AuthContext>& ctx, const std::string& archive_path, const std::string& archive_password)
{
    AccountArchive archive;
    try {
        archive = AccountArchive(archive_path, archive_password);
    } catch (const std::exception& ex) {
        JAMI_WARN("[Account %s] can't read file: %s", getAccount().getAccountID().c_str(), ex.what());
        ctx->onFailure(AuthError::UNKNOWN, ex.what());
        return;
    }
    auto contactList = std::make_unique<ContactList>(getAccount(), archive.id.second);
    onArchiveLoaded(*ctx, std::move(archive), std::move(contactList));
}

// must be called while configurationMutex_ is locked
void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx, const std::string& archive_password, const std::string& archive_pin)
{
    auto& dht_ = getAccount().dht();
    // launch dedicated dht instance
    if (dht_.isRunning()) {
        JAMI_ERR("DHT already running (stopping it first).");
        dht_.join();
    }
    dht_.setOnStatusChanged([](dht::NodeStatus s4, dht::NodeStatus s6) {
        //JAMI_WARN("Dht status : IPv4 %s; IPv6 %s", dhtStatusStr(s4), dhtStatusStr(s6));
    });
    /*dht_.run((in_port_t)dhtPortUsed_, {}, true);
    dht_.bootstrap(loadNodes());
    auto bootstrap = loadBootstrap();
    if (not bootstrap.empty())
        dht_.bootstrap(bootstrap);*/

    auto state_old = std::make_shared<std::pair<bool, bool>>(false, true);
    auto state_new = std::make_shared<std::pair<bool, bool>>(false, true);
    auto found = std::make_shared<bool>(false);

    auto searchEnded = [this,ctx,found,state_old,state_new](){
        if (*found)
            return;
        if (state_old->first && state_new->first) {
            bool network_error = !state_old->second && !state_new->second;
            JAMI_WARN("[Account %s] failure looking for archive on DHT: %s", getAccount().getAccountID().c_str(), network_error ? "network error" : "not found");
            ctx->onFailure(network_error ? AuthError::NETWORK : AuthError::UNKNOWN, "");
        }
    };

    auto search = [this, ctx, found, searchEnded, &dht_](bool previous, std::shared_ptr<std::pair<bool, bool>>& state) {
        std::vector<uint8_t> key;
        dht::InfoHash loc;

        // compute archive location and decryption keys
        try {
            std::tie(key, loc) = computeKeys(ctx->credentials->password, ctx->credentials->uri, previous);
            JAMI_DBG("[Account %s] trying to load account from DHT with %s at %s", getAccount().getAccountID().c_str(), ctx->credentials->uri.c_str(), loc.toString().c_str());
            dht_.get(loc, [this, ctx, key, found](const std::shared_ptr<dht::Value>& val) {
                std::vector<uint8_t> decrypted;
                try {
                    decrypted = archiver::decompress(dht::crypto::aesDecrypt(val->data, key));
                } catch (const std::exception& ex) {
                    return true;
                }
                JAMI_DBG("Found archive on the DHT");
                try {
                    *found =  true;
                    auto archive = AccountArchive(decrypted);
                    auto contacts = std::make_unique<ContactList>(getAccount(), archive.id.second);
                    onArchiveLoaded(*ctx, std::move(archive), std::move(contacts));
                } catch (const std::exception& e) {
                    ctx->onFailure(AuthError::UNKNOWN, "");
                }
                return not *found;
            }, [=](bool ok) {
                JAMI_DBG("[Account %s] DHT archive search ended at %s", getAccount().getAccountID().c_str(), loc.toString().c_str());
                state->first = true;
                state->second = ok;
                searchEnded();
            });
        } catch (const std::exception& e) {
            JAMI_ERR("Error computing keys: %s", e.what());
            state->first = true;
            state->second = true;
            searchEnded();
            return;
        }
    };
    dht::ThreadPool::computation().run(std::bind(search, true, state_old));
    dht::ThreadPool::computation().run(std::bind(search, false, state_new));
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
    a.save(path, ctx.credentials->password);

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
    auto receiptSignature = a.id.first->sign({receipt.begin(), receipt.end()});
    JAMI_WARN("[Account %s] created new device: %s",
            getAccount().getAccountID().c_str(), deviceId.toString().c_str());

    ctx.onSuccess(deviceCertificate, {}, std::move(contactList), receipt, receiptSignature);
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

std::string
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

    //auto announce_ = std::make_shared<dht::Value>(std::move(ann_val));
    return is.str();
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

}
