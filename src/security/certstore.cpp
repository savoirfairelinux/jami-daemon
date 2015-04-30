/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <thread>
#include <sstream>

namespace ring { namespace tls {

CertificateStore&
CertificateStore::instance()
{
    // Meyers singleton
    static CertificateStore instance_;
    return instance_;
}

CertificateStore::CertificateStore()
    : certPath_(fileutils::get_data_dir()+DIR_SEPARATOR_CH+"certificates")
{
    fileutils::check_dir(certPath_.c_str());
    loadLocalCertificates(certPath_);
}

unsigned
CertificateStore::loadLocalCertificates(const std::string& path)
{
    std::lock_guard<std::mutex> l(lock_);

    auto dir_content = fileutils::readDirectory(path);
    unsigned n = 0;
    for (const auto& f : dir_content) {
        try {
            auto crt = crypto::Certificate(fileutils::loadFile(path+DIR_SEPARATOR_CH+f));
            auto id = crt.getId().toString();
            if (id != f)
                throw std::logic_error({});
            certs_.emplace(crt.getId().toString(), std::make_shared<crypto::Certificate>(std::move(crt)));
            ++n;
        } catch (const std::exception& e) {
            remove((path+DIR_SEPARATOR_CH+f).c_str());
        }
    }
    RING_DBG("CertificateStore: loaded %u local certificates.", n);
    return n;
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
CertificateStore::getCertificate(const std::string& k) const
{
    std::unique_lock<std::mutex> l(lock_);

    auto cit = certs_.find(k);
    if (cit == certs_.cend()) {
        l.unlock();
        try {
            return std::make_shared<crypto::Certificate>(fileutils::loadFile(k));
        } catch (const std::exception& e) {
            return {};
        }
    }
    return cit->second;
}

static std::vector<crypto::Certificate>
readCertificates(const std::string& path)
{
    std::vector<crypto::Certificate> ret;
    if (fileutils::isDirectory(path)) {
        auto files = fileutils::readDirectory(path);
        for (const auto& file : files) {
            auto certs = readCertificates(path+"/"+file);
            ret.insert(std::end(ret),
                       std::make_move_iterator(std::begin(certs)),
                       std::make_move_iterator(std::end(certs)));
        }
    } else {
        try {
            auto data = fileutils::loadFile(path);
            const gnutls_datum_t dt {data.data(), (unsigned)data.size()};
            gnutls_x509_crt_t* certs {nullptr};
            unsigned cert_num {0};
            gnutls_x509_crt_list_import2(&certs, &cert_num, &dt, GNUTLS_X509_FMT_PEM, 0);
            for (unsigned i=0; i<cert_num; i++)
                ret.emplace_back(certs[i]);
        } catch (const std::exception& e) {};
    }
    return ret;
}

void
CertificateStore::pinCertificatePath(const std::string& path)
{
    std::thread([&, path]() {
        auto certs = readCertificates(path);
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
            }
            paths_.emplace(path, std::move(scerts));
        }
        RING_DBG("CertificateStore: loaded %lu certificates from %s.",
                 certs.size(), path.c_str());
        emitSignal<DRing::ConfigurationSignal::CertificatePathPinned>(path, ids);
    }).detach();
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

std::string
CertificateStore::pinCertificate(const std::vector<uint8_t>& cert,
                                 bool local) noexcept
{
    try {
        return pinCertificate(cert, local);
    } catch (const std::exception& e) {}
    return {};
}

std::string
CertificateStore::pinCertificate(crypto::Certificate&& cert, bool local)
{
    return pinCertificate(std::make_shared<crypto::Certificate>(std::move(cert)), local);
}

std::string
CertificateStore::pinCertificate(std::shared_ptr<crypto::Certificate> cert,
                                 bool local)
{
    auto id = cert->getId().toString();
    bool sig {false};
    {
        std::lock_guard<std::mutex> l(lock_);

        decltype(certs_)::iterator it;
        std::tie(it, sig) = certs_.emplace(id, std::move(cert));
        if (sig and local)
            fileutils::saveFile(certPath_+DIR_SEPARATOR_CH+id, it->second->getPacked());
    }
    if (sig)
        emitSignal<DRing::ConfigurationSignal::CertificatePinned>(id);
    return id;
}

bool
CertificateStore::unpinCertificate(const std::string& id)
{
    std::lock_guard<std::mutex> l(lock_);

    certs_.erase(id);
    return remove((certPath_+DIR_SEPARATOR_CH+id).c_str()) == 0;
}

TrustStore::Status
TrustStore::statusFromStr(const char* str)
{
    if (!std::strcmp(str, DRing::Certificate::Status::ALLOWED))
        return Status::ALLOWED;
    if (!std::strcmp(str, DRing::Certificate::Status::BANNED))
        return Status::BANNED;
    return Status::UNDEFINED;
}

const char*
TrustStore::statusToStr(TrustStore::Status s)
{
    switch (s) {
        case Status::ALLOWED:
            return DRing::Certificate::Status::ALLOWED;
        case Status::BANNED:
            return DRing::Certificate::Status::BANNED;
        default:
            return DRing::Certificate::Status::UNDEFINED;
    }
}

TrustStore::TrustStore()
{
    gnutls_x509_trust_list_init(&trust_, 0);
}

TrustStore::~TrustStore()
{
    gnutls_x509_trust_list_deinit(trust_, false);
}

bool
TrustStore::setCertificateStatus(const std::string& cert_id,
                                 const TrustStore::Status status)
{
    updateKnownCerts();
    auto s = certStatus_.find(cert_id);
    if (s == std::end(certStatus_)) {
        if (auto cert = CertificateStore::instance().getCertificate(cert_id)) {
            certStatus_[cert->getId().toString()] = {cert, status};
            setStoreCertStatus(*cert, status);
        } else
            unknownCertStatus_[cert_id] = status;
    } else {
        s->second.second = status;
        setStoreCertStatus(*s->second.first, status);
    }
    return true;
}

bool
TrustStore::setCertificateStatus(std::shared_ptr<crypto::Certificate>& cert,
                                 const TrustStore::Status status, bool local)
{
    CertificateStore::instance().pinCertificate(cert, local);
    certStatus_[cert->getId().toString()] = {cert, status};
    setStoreCertStatus(*cert, status);
    return true;
}

TrustStore::Status
TrustStore::getCertificateStatus(const std::string& cert_id) const
{
    auto s = certStatus_.find(cert_id);
    if (s == std::end(certStatus_)) {
        auto us = unknownCertStatus_.find(cert_id);
        if (us == std::end(unknownCertStatus_))
            return Status::UNDEFINED;
        return us->second;
    }
    return s->second.second;
}

std::vector<std::string>
TrustStore::getCertificatesByStatus(TrustStore::Status status)
{
    std::vector<std::string> ret;
    for (const auto& i : certStatus_)
        if (i.second.second == status)
            ret.emplace_back(i.first);
    for (const auto& i : unknownCertStatus_)
        if (i.second == status)
            ret.emplace_back(i.first);
    return ret;
}

bool
TrustStore::isTrusted(const crypto::Certificate& crt)
{
    updateKnownCerts();
    auto crts = getChain(crt);
    unsigned result = 0;

#if GNUTLS_VERSION_NUMBER > 0x030308
    auto ret = gnutls_x509_trust_list_verify_crt2(
        trust_,
        crts.data(), crts.size(),
        nullptr, 0,
        GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_MEDIUM),
        &result, nullptr);
#else
    auto ret = gnutls_x509_trust_list_verify_crt(
        trust_,
        crts.data(), crts.size(),
        0,
        &result, nullptr);
#endif

    if (ret < 0) {
        RING_ERR("Error verifying certificate: %s", gnutls_strerror(ret));
        return false;
    }

    return !(result & GNUTLS_CERT_INVALID);
}

std::vector<gnutls_x509_crt_t>
TrustStore::getChain(const crypto::Certificate& crt)
{
    std::vector<gnutls_x509_crt_t> crts;
    auto c = &crt;
    do {
        crts.emplace_back(c->cert);
        c = c->issuer.get();
    } while (c);
    return crts;
}

void
TrustStore::updateKnownCerts()
{
    auto i = std::begin(unknownCertStatus_);
    while (i != std::end(unknownCertStatus_)) {
        if (auto crt = CertificateStore::instance().getCertificate(i->first)) {
            certStatus_.emplace(i->first, std::make_pair(crt, i->second));
            setStoreCertStatus(*crt, i->second);
            i = unknownCertStatus_.erase(i);
        } else
            ++i;
    }
}

void
TrustStore::setStoreCertStatus(const crypto::Certificate& crt,
                               TrustStore::Status status)
{
    if (status == Status::ALLOWED)
        gnutls_x509_trust_list_add_cas(trust_, &crt.cert, 1, 0);
    else if (status == Status::BANNED)
        gnutls_x509_trust_list_remove_cas(trust_, &crt.cert, 1);

    RING_DBG("TrustStore: setting %s status to %s.",
             crt.getId().toString().c_str(),
             status == TrustStore::Status::ALLOWED ? "ALLOWED" : "BANNED");
}


#if 0
void
TrustStore::generateCAList(const std::string& out_path)
{
    updateKnownCerts();
    std::ostringstream ss;
    for (const auto& cert : certStatus_)
        ss << cert.second.first->toString();
    auto res = ss.str();
    fileutils::saveFile(out_path, {std::begin(res), std::end(res)});
}
#endif

}} // namespace ring::tls
