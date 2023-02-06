/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Vsevolod Ivanov <vsevolod.ivanov@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "certstore.h"

#include "client/ring_signal.h"

#include "fileutils.h"
#include "logger.h"

#include <opendht/thread_pool.h>
#include <gnutls/ocsp.h>

#include <thread>
#include <sstream>

namespace jami {
namespace tls {

CertificateStore&
CertificateStore::instance()
{
    // Meyers singleton
    static std::mutex instanceMtx_;

    std::lock_guard<std::mutex> lock(instanceMtx_);
    static CertificateStore instance_;
    return instance_;
}

CertificateStore::CertificateStore()
    : certPath_(fileutils::get_data_dir() + DIR_SEPARATOR_CH + "certificates")
    , crlPath_(fileutils::get_data_dir() + DIR_SEPARATOR_CH + "crls")
    , ocspPath_(fileutils::get_data_dir() + DIR_SEPARATOR_CH + "ocsp")
{
    fileutils::check_dir(certPath_.c_str());
    fileutils::check_dir(crlPath_.c_str());
    fileutils::check_dir(ocspPath_.c_str());
    loadLocalCertificates();
}

unsigned
CertificateStore::loadLocalCertificates()
{
    std::lock_guard<std::mutex> l(lock_);

    auto dir_content = fileutils::readDirectory(certPath_);
    unsigned n = 0;
    for (const auto& f : dir_content) {
        try {
            auto crt = std::make_shared<crypto::Certificate>(
                fileutils::loadFile(certPath_ + DIR_SEPARATOR_CH + f));
            auto id = crt->getId().toString();
            auto longId = crt->getLongId().toString();
            if (id != f && longId != f)
                throw std::logic_error("Certificate id mismatch");
            while (crt) {
                id = crt->getId().toString();
                longId = crt->getLongId().toString();
                certs_.emplace(std::move(id), crt);
                certs_.emplace(std::move(longId), crt);
                loadRevocations(*crt);
                crt = crt->issuer;
                ++n;
            }
        } catch (const std::exception& e) {
            JAMI_WARN() << "Remove cert. " << e.what();
            remove((certPath_ + DIR_SEPARATOR_CH + f).c_str());
        }
    }
    JAMI_DBG("CertificateStore: loaded %u local certificates.", n);
    return n;
}

void
CertificateStore::loadRevocations(crypto::Certificate& crt) const
{
    auto dir = crlPath_ + DIR_SEPARATOR_CH + crt.getId().toString();
    for (const auto& crl : fileutils::readDirectory(dir)) {
        try {
            crt.addRevocationList(std::make_shared<crypto::RevocationList>(
                fileutils::loadFile(dir + DIR_SEPARATOR_CH + crl)));
        } catch (const std::exception& e) {
            JAMI_WARN("Can't load revocation list: %s", e.what());
        }
    }
    auto ocsp_dir = ocspPath_ + DIR_SEPARATOR_CH + crt.getId().toString();
    for (const auto& ocsp : fileutils::readDirectory(ocsp_dir)) {
        try {
            std::string ocsp_filepath = ocsp_dir + DIR_SEPARATOR_CH + ocsp;
            JAMI_DBG("Found %s", ocsp_filepath.c_str());
            auto serial = crt.getSerialNumber();
            if (dht::toHex(serial.data(), serial.size()) != ocsp)
                continue;
            // Save the response
            dht::Blob ocspBlob = fileutils::loadFile(ocsp_filepath);
            crt.ocspResponse = std::make_shared<dht::crypto::OcspResponse>(ocspBlob.data(),
                                                                           ocspBlob.size());
            unsigned int status = crt.ocspResponse->getCertificateStatus();
            if (status == GNUTLS_OCSP_CERT_GOOD)
                JAMI_DBG("Certificate %s has good OCSP status", crt.getId().to_c_str());
            else if (status == GNUTLS_OCSP_CERT_REVOKED)
                JAMI_ERR("Certificate %s has revoked OCSP status", crt.getId().to_c_str());
            else if (status == GNUTLS_OCSP_CERT_UNKNOWN)
                JAMI_ERR("Certificate %s has unknown OCSP status", crt.getId().to_c_str());
            else
                JAMI_ERR("Certificate %s has invalid OCSP status", crt.getId().to_c_str());

        } catch (const std::exception& e) {
            JAMI_WARN("Can't load OCSP revocation status: %s", e.what());
        }
    }
}

std::vector<std::string>
CertificateStore::getPinnedCertificates() const
{
    std::lock_guard<std::mutex> l(lock_);

    std::vector<std::string> certIds;
    certIds.reserve(certs_.size());
    for (const auto& crt : certs_)
        certIds.emplace_back(crt.first);
    return certIds;
}

std::shared_ptr<crypto::Certificate>
CertificateStore::getCertificate(const std::string& k)
{
    auto getCertificate_ = [this](const std::string& k) -> std::shared_ptr<crypto::Certificate> {
        auto cit = certs_.find(k);
        if (cit == certs_.cend())
            return {};
        return cit->second;
    };
    std::unique_lock<std::mutex> l(lock_);
    auto crt = getCertificate_(k);
    // Check if certificate is complete
    // If the certificate has been splitted, reconstruct it
    auto top_issuer = crt;
    while (top_issuer && top_issuer->getUID() != top_issuer->getIssuerUID()) {
        if (top_issuer->issuer) {
            top_issuer = top_issuer->issuer;
        } else if (auto cert = getCertificate_(top_issuer->getIssuerUID())) {
            top_issuer->issuer = cert;
            top_issuer = cert;
        } else {
            // In this case, a certificate was not found
            JAMI_WARN("Incomplete certificate detected %s", k.c_str());
            break;
        }
    }
    return crt;
}

std::shared_ptr<crypto::Certificate>
CertificateStore::findCertificateByName(const std::string& name, crypto::NameType type) const
{
    std::unique_lock<std::mutex> l(lock_);
    for (auto& i : certs_) {
        if (i.second->getName() == name)
            return i.second;
        if (type != crypto::NameType::UNKNOWN) {
            for (const auto& alt : i.second->getAltNames())
                if (alt.first == type and alt.second == name)
                    return i.second;
        }
    }
    return {};
}

std::shared_ptr<crypto::Certificate>
CertificateStore::findCertificateByUID(const std::string& uid) const
{
    std::unique_lock<std::mutex> l(lock_);
    for (auto& i : certs_) {
        if (i.second->getUID() == uid)
            return i.second;
    }
    return {};
}

std::shared_ptr<crypto::Certificate>
CertificateStore::findIssuer(const std::shared_ptr<crypto::Certificate>& crt) const
{
    std::shared_ptr<crypto::Certificate> ret {};
    auto n = crt->getIssuerUID();
    if (not n.empty()) {
        if (crt->issuer and crt->issuer->getUID() == n)
            ret = crt->issuer;
        else
            ret = findCertificateByUID(n);
    }
    if (not ret) {
        n = crt->getIssuerName();
        if (not n.empty())
            ret = findCertificateByName(n);
    }
    if (not ret)
        return ret;
    unsigned verify_out = 0;
    int err = gnutls_x509_crt_verify(crt->cert, &ret->cert, 1, 0, &verify_out);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_WARN("gnutls_x509_crt_verify failed: %s", gnutls_strerror(err));
        return {};
    }
    if (verify_out & GNUTLS_CERT_INVALID)
        return {};
    return ret;
}

static std::vector<crypto::Certificate>
readCertificates(const std::string& path, const std::string& crl_path)
{
    std::vector<crypto::Certificate> ret;
    if (fileutils::isDirectory(path)) {
        auto files = fileutils::readDirectory(path);
        for (const auto& file : files) {
            auto certs = readCertificates(path + DIR_SEPARATOR_CH + file, crl_path);
            ret.insert(std::end(ret),
                       std::make_move_iterator(std::begin(certs)),
                       std::make_move_iterator(std::end(certs)));
        }
    } else {
        try {
            auto data = fileutils::loadFile(path);
            const gnutls_datum_t dt {data.data(), (unsigned) data.size()};
            gnutls_x509_crt_t* certs {nullptr};
            unsigned cert_num {0};
            gnutls_x509_crt_list_import2(&certs, &cert_num, &dt, GNUTLS_X509_FMT_PEM, 0);
            for (unsigned i = 0; i < cert_num; i++)
                ret.emplace_back(certs[i]);
            gnutls_free(certs);
        } catch (const std::exception& e) {
        };
    }
    return ret;
}

void
CertificateStore::pinCertificatePath(const std::string& path,
                                     std::function<void(const std::vector<std::string>&)> cb)
{
    dht::ThreadPool::computation().run([&, path, cb]() {
        auto certs = readCertificates(path, crlPath_);
        std::vector<std::string> ids;
        std::vector<std::weak_ptr<crypto::Certificate>> scerts;
        ids.reserve(certs.size());
        scerts.reserve(certs.size());
        {
            std::lock_guard<std::mutex> l(lock_);

            for (auto& cert : certs) {
                auto shared = std::make_shared<crypto::Certificate>(std::move(cert));
                scerts.emplace_back(shared);
                auto e = certs_.emplace(shared->getId().toString(), shared);
                ids.emplace_back(e.first->first);
                e = certs_.emplace(shared->getLongId().toString(), shared);
                ids.emplace_back(e.first->first);
            }
            paths_.emplace(path, std::move(scerts));
        }
        JAMI_DBG("CertificateStore: loaded %zu certificates from %s.", certs.size(), path.c_str());
        if (cb)
            cb(ids);
        emitSignal<libjami::ConfigurationSignal::CertificatePathPinned>(path, ids);
    });
}

unsigned
CertificateStore::unpinCertificatePath(const std::string& path)
{
    std::lock_guard<std::mutex> l(lock_);

    auto certs = paths_.find(path);
    if (certs == std::end(paths_))
        return 0;
    unsigned n = 0;
    for (const auto& wcert : certs->second) {
        if (auto cert = wcert.lock()) {
            certs_.erase(cert->getId().toString());
            ++n;
        }
    }
    paths_.erase(certs);
    return n;
}

std::vector<std::string>
CertificateStore::pinCertificate(const std::vector<uint8_t>& cert, bool local) noexcept
{
    try {
        return pinCertificate(crypto::Certificate(cert), local);
    } catch (const std::exception& e) {
    }
    return {};
}

std::vector<std::string>
CertificateStore::pinCertificate(crypto::Certificate&& cert, bool local)
{
    return pinCertificate(std::make_shared<crypto::Certificate>(std::move(cert)), local);
}

std::vector<std::string>
CertificateStore::pinCertificate(const std::shared_ptr<crypto::Certificate>& cert, bool local)
{
    bool sig {false};
    std::vector<std::string> ids {};
    {
        auto c = cert;
        std::lock_guard<std::mutex> l(lock_);
        while (c) {
            bool inserted;
            auto id = c->getId().toString();
            auto longId = c->getLongId().toString();
            decltype(certs_)::iterator it;
            std::tie(it, inserted) = certs_.emplace(id, c);
            if (not inserted)
                it->second = c;
            std::tie(it, inserted) = certs_.emplace(longId, c);
            if (not inserted)
                it->second = c;
            if (local) {
                for (const auto& crl : c->getRevocationLists())
                    pinRevocationList(id, *crl);
            }
            ids.emplace_back(longId);
            ids.emplace_back(id);
            c = c->issuer;
            sig |= inserted;
        }
        if (local) {
            if (sig)
                fileutils::saveFile(certPath_ + DIR_SEPARATOR_CH + ids.front(), cert->getPacked());
        }
    }
    for (const auto& id : ids)
        emitSignal<libjami::ConfigurationSignal::CertificatePinned>(id);
    return ids;
}

bool
CertificateStore::unpinCertificate(const std::string& id)
{
    std::lock_guard<std::mutex> l(lock_);

    certs_.erase(id);
    return remove((certPath_ + DIR_SEPARATOR_CH + id).c_str()) == 0;
}

bool
CertificateStore::setTrustedCertificate(const std::string& id, TrustStatus status)
{
    if (status == TrustStatus::TRUSTED) {
        if (auto crt = getCertificate(id)) {
            trustedCerts_.emplace_back(crt);
            return true;
        }
    } else {
        auto tc = std::find_if(trustedCerts_.begin(),
                               trustedCerts_.end(),
                               [&](const std::shared_ptr<crypto::Certificate>& crt) {
                                   return crt->getId().toString() == id;
                               });
        if (tc != trustedCerts_.end()) {
            trustedCerts_.erase(tc);
            return true;
        }
    }
    return false;
}

std::vector<gnutls_x509_crt_t>
CertificateStore::getTrustedCertificates() const
{
    std::vector<gnutls_x509_crt_t> crts;
    crts.reserve(trustedCerts_.size());
    for (auto& crt : trustedCerts_)
        crts.emplace_back(crt->getCopy());
    return crts;
}

void
CertificateStore::pinRevocationList(const std::string& id,
                                    const std::shared_ptr<dht::crypto::RevocationList>& crl)
{
    try {
        if (auto c = getCertificate(id))
            c->addRevocationList(crl);
        pinRevocationList(id, *crl);
    } catch (...) {
        JAMI_WARN("Can't add revocation list");
    }
}

void
CertificateStore::pinRevocationList(const std::string& id, const dht::crypto::RevocationList& crl)
{
    fileutils::check_dir((crlPath_ + DIR_SEPARATOR_CH + id).c_str());
    fileutils::saveFile(crlPath_ + DIR_SEPARATOR_CH + id + DIR_SEPARATOR_CH
                            + dht::toHex(crl.getNumber()),
                        crl.getPacked());
}

void
CertificateStore::pinOcspResponse(const dht::crypto::Certificate& cert)
{
    if (not cert.ocspResponse)
        return;
    try {
        cert.ocspResponse->getCertificateStatus();
    } catch (dht::crypto::CryptoException& e) {
        JAMI_ERR("Failed to read certificate status of OCSP response: %s", e.what());
        return;
    }
    auto id = cert.getId().toString();
    auto serial = cert.getSerialNumber();
    auto serialhex = dht::toHex(serial);
    auto dir = ocspPath_ + DIR_SEPARATOR_CH + id;

    if (auto localCert = getCertificate(id)) {
        // Update certificate in the local store if relevant
        if (localCert.get() != &cert && serial == localCert->getSerialNumber()) {
            JAMI_DBG("Updating OCSP for certificate %s in the local store", id.c_str());
            localCert->ocspResponse = cert.ocspResponse;
        }
    }

    dht::ThreadPool::io().run([path = dir + DIR_SEPARATOR_CH + serialhex,
                               dir = std::move(dir),
                               id = std::move(id),
                               serialhex = std::move(serialhex),
                               ocspResponse = cert.ocspResponse] {
        JAMI_DBG("Saving OCSP Response of device %s with serial %s", id.c_str(), serialhex.c_str());
        std::lock_guard<std::mutex> lock(fileutils::getFileLock(path));
        fileutils::check_dir(dir.c_str());
        fileutils::saveFile(path, ocspResponse->pack());
    });
}

TrustStore::PermissionStatus
TrustStore::statusFromStr(const char* str)
{
    if (!std::strcmp(str, libjami::Certificate::Status::ALLOWED))
        return PermissionStatus::ALLOWED;
    if (!std::strcmp(str, libjami::Certificate::Status::BANNED))
        return PermissionStatus::BANNED;
    return PermissionStatus::UNDEFINED;
}

const char*
TrustStore::statusToStr(TrustStore::PermissionStatus s)
{
    switch (s) {
    case PermissionStatus::ALLOWED:
        return libjami::Certificate::Status::ALLOWED;
    case PermissionStatus::BANNED:
        return libjami::Certificate::Status::BANNED;
    case PermissionStatus::UNDEFINED:
    default:
        return libjami::Certificate::Status::UNDEFINED;
    }
}

TrustStatus
trustStatusFromStr(const char* str)
{
    if (!std::strcmp(str, libjami::Certificate::TrustStatus::TRUSTED))
        return TrustStatus::TRUSTED;
    return TrustStatus::UNTRUSTED;
}

const char*
statusToStr(TrustStatus s)
{
    switch (s) {
    case TrustStatus::TRUSTED:
        return libjami::Certificate::TrustStatus::TRUSTED;
    case TrustStatus::UNTRUSTED:
    default:
        return libjami::Certificate::TrustStatus::UNTRUSTED;
    }
}

bool
TrustStore::addRevocationList(dht::crypto::RevocationList&& crl)
{
    allowed_.add(crl);
    return true;
}

bool
TrustStore::setCertificateStatus(const std::string& cert_id,
                                 const TrustStore::PermissionStatus status)
{
    return setCertificateStatus(nullptr, cert_id, status, false);
}

bool
TrustStore::setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                                 const TrustStore::PermissionStatus status,
                                 bool local)
{
    return setCertificateStatus(cert, cert->getId().toString(), status, local);
}

bool
TrustStore::setCertificateStatus(std::shared_ptr<crypto::Certificate> cert,
                                 const std::string& cert_id,
                                 const TrustStore::PermissionStatus status,
                                 bool local)
{
    if (cert)
        CertificateStore::instance().pinCertificate(cert, local);
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    updateKnownCerts();
    bool dirty {false};
    if (status == PermissionStatus::UNDEFINED) {
        unknownCertStatus_.erase(cert_id);
        dirty = certStatus_.erase(cert_id);
    } else {
        bool allowed = (status == PermissionStatus::ALLOWED);
        auto s = certStatus_.find(cert_id);
        if (s == std::end(certStatus_)) {
            // Certificate state is currently undefined
            if (not cert)
                cert = CertificateStore::instance().getCertificate(cert_id);
            if (cert) {
                unknownCertStatus_.erase(cert_id);
                auto& crt_status = certStatus_[cert_id];
                if (not crt_status.first)
                    crt_status.first = cert;
                crt_status.second.allowed = allowed;
                setStoreCertStatus(*cert, allowed);
            } else {
                // Can't find certificate
                unknownCertStatus_[cert_id].allowed = allowed;
            }
        } else {
            // Certificate is already allowed or banned
            if (s->second.second.allowed != allowed) {
                s->second.second.allowed = allowed;
                if (allowed) // Certificate is re-added after ban, rebuld needed
                    dirty = true;
                else
                    allowed_.remove(*s->second.first, false);
            }
        }
    }
    if (dirty)
        rebuildTrust();
    return true;
}

TrustStore::PermissionStatus
TrustStore::getCertificateStatus(const std::string& cert_id) const
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    auto s = certStatus_.find(cert_id);
    if (s == std::end(certStatus_)) {
        auto us = unknownCertStatus_.find(cert_id);
        if (us == std::end(unknownCertStatus_))
            return PermissionStatus::UNDEFINED;
        return us->second.allowed ? PermissionStatus::ALLOWED : PermissionStatus::BANNED;
    }
    return s->second.second.allowed ? PermissionStatus::ALLOWED : PermissionStatus::BANNED;
}

std::vector<std::string>
TrustStore::getCertificatesByStatus(TrustStore::PermissionStatus status) const
{
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    std::vector<std::string> ret;
    for (const auto& i : certStatus_)
        if (i.second.second.allowed == (status == TrustStore::PermissionStatus::ALLOWED))
            ret.emplace_back(i.first);
    for (const auto& i : unknownCertStatus_)
        if (i.second.allowed == (status == TrustStore::PermissionStatus::ALLOWED))
            ret.emplace_back(i.first);
    return ret;
}

bool
TrustStore::isAllowed(const crypto::Certificate& crt, bool allowPublic)
{
    // Match by certificate pinning
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    bool allowed {allowPublic};
    for (auto c = &crt; c; c = c->issuer.get()) {
        auto status = getCertificateStatus(c->getId().toString()); // lock mutex_
        if (status == PermissionStatus::ALLOWED)
            allowed = true;
        else if (status == PermissionStatus::BANNED)
            return false;
    }

    // Match by certificate chain
    updateKnownCerts();
    auto ret = allowed_.verify(crt);
    // Unknown issuer (only that) are accepted if allowPublic is true
    if (not ret
        and !(allowPublic and ret.result == (GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND))) {
        JAMI_WARN("%s", ret.toString().c_str());
        return false;
    }

    return allowed;
}

void
TrustStore::updateKnownCerts()
{
    auto i = std::begin(unknownCertStatus_);
    while (i != std::end(unknownCertStatus_)) {
        if (auto crt = CertificateStore::instance().getCertificate(i->first)) {
            certStatus_.emplace(i->first, std::make_pair(crt, i->second));
            setStoreCertStatus(*crt, i->second.allowed);
            i = unknownCertStatus_.erase(i);
        } else
            ++i;
    }
}

void
TrustStore::setStoreCertStatus(const crypto::Certificate& crt, bool status)
{
    if (status)
        allowed_.add(crt);
    else
        allowed_.remove(crt, false);
}

void
TrustStore::rebuildTrust()
{
    allowed_ = {};
    for (const auto& c : certStatus_)
        setStoreCertStatus(*c.second.first, c.second.second.allowed);
}

} // namespace tls
} // namespace jami
