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
#include "libdevcrypto/Common.h"

#include <opendht/thread_pool.h>

#include <exception>

namespace jami {

void
ArchiveAccountManager::initAuthentication(
    std::unique_ptr<dht::crypto::CertificateRequest> request,
    std::unique_ptr<AccountCredentials> credientials,
    AuthSuccessCallback onSuccess,
    AuthFailureCallback onFailure,
    DeviceSyncCallback onSync)
{
    JAMI_WARN("initAuthentication 0");
    auto creds = std::dynamic_pointer_cast<ArchiveAccountCredentials>(std::shared_ptr<AccountCredentials>(std::move(credientials)));
    if (not creds) {
        onFailure(503, "invalid creds");
        return;
    }

    dht::ThreadPool::computation().run([
        request = std::shared_ptr<dht::crypto::CertificateRequest>(std::move(request)),
        credientials = std::move(creds),
        onSuccess, onFailure, onSync,
        onAsync = onAsync_
    ]() mutable {
        JAMI_WARN("initAuthentication 1");
        onAsync([
            request = std::move(request),
            credientials = std::move(credientials),
            onSuccess, onFailure, onSync
        ](AccountManager* accountManager) mutable {
            JAMI_WARN("initAuthentication 2");
            if (not accountManager) return;
            auto& this_ = *static_cast<ArchiveAccountManager*>(accountManager);
            try {
                AccountArchive a;
                std::unique_ptr<ContactList> contactList;

                if (credientials->scheme == "dht") {
                    onFailure(0, "");
                    return;
                } else if (credientials->scheme == "file") {
                    onFailure(0, "");
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
                auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();

                auto path = fileutils::getFullPath(this_.getAccount().getPath(), this_.archivePath_);
                JAMI_WARN("saving archive to %s", path.c_str());
                a.save(path, credientials->password);

                if (not a.id.second->isCA()) {
                    JAMI_ERR("[Account %s] trying to sign a certificate with a non-CA.", this_.getAccount().getAccountID().c_str());
                }
                JAMI_WARN("generating device certificate");
                auto deviceCertificate = std::make_shared<dht::crypto::Certificate>(dht::crypto::Certificate::generate(*request, a.id));

                if (not request->verify()) {
                    JAMI_ERR("[Account %s] Invalid certificate request.", this_.getAccount().getAccountID().c_str());
                    onFailure(0, "");
                    return;
                }

                fileutils::check_dir(this_.getAccount().getPath().c_str(), 0700);

                auto deviceId = deviceCertificate->getPublicKey().getId();
                auto receipt = this_.makeReceipt(a.id, *deviceCertificate, ethAccount);
                auto receiptSignature = a.id.first->sign({receipt.begin(), receipt.end()});
                JAMI_WARN("[Account %s] created new device: %s",
                        this_.getAccount().getAccountID().c_str(), deviceId.toString().c_str());

                onSuccess(deviceCertificate, {}, std::move(contactList), receipt, receiptSignature);
            } catch (const std::exception& e) {
                //JAMI_WARN("Exception during auth: %s", e.what());
                onFailure(0, e.what());
            }
        });
    });
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
