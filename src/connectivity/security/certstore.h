/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#pragma once

#include "jami/security_const.h"
#include "noncopyable.h"

#include <opendht/crypto.h>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <mutex>

namespace crypto = ::dht::crypto;

namespace jami {
namespace tls {

enum class TrustStatus { UNTRUSTED = 0, TRUSTED };
TrustStatus trustStatusFromStr(const char* str);
const char* statusToStr(TrustStatus s);

/**
 * Global certificate store.
 * Stores system root CAs and any other encountred certificate
 */
class CertificateStore
{
public:
    static CertificateStore& instance();

    CertificateStore();

    std::vector<std::string> getPinnedCertificates() const;
    /**
     * Return certificate (with full chain)
     */
    std::shared_ptr<crypto::Certificate> getCertificate(const std::string& cert_id);

    std::shared_ptr<crypto::Certificate> findCertificateByName(
        const std::string& name, crypto::NameType type = crypto::NameType::UNKNOWN) const;
    std::shared_ptr<crypto::Certificate> findCertificateByUID(const std::string& uid) const;
    std::shared_ptr<crypto::Certificate> findIssuer(
        const std::shared_ptr<crypto::Certificate>& crt) const;

    std::vector<std::string> pinCertificate(const std::vector<uint8_t>& crt,
                                            bool local = true) noexcept;
    std::vector<std::string> pinCertificate(crypto::Certificate&& crt, bool local = true);
    std::vector<std::string> pinCertificate(const std::shared_ptr<crypto::Certificate>& crt,
                                            bool local = true);
    bool unpinCertificate(const std::string&);

    void pinCertificatePath(const std::string& path,
                            std::function<void(const std::vector<std::string>&)> cb = {});
    unsigned unpinCertificatePath(const std::string&);

    bool setTrustedCertificate(const std::string& id, TrustStatus status);
    std::vector<gnutls_x509_crt_t> getTrustedCertificates() const;

    void pinRevocationList(const std::string& id,
                           const std::shared_ptr<dht::crypto::RevocationList>& crl);
    void pinRevocationList(const std::string& id, dht::crypto::RevocationList&& crl)
    {
        pinRevocationList(id,
                          std::make_shared<dht::crypto::RevocationList>(
                              std::forward<dht::crypto::RevocationList>(crl)));
    }
    void pinOcspResponse(const dht::crypto::Certificate& cert);

    void loadRevocations(crypto::Certificate& crt) const;

private:
    NON_COPYABLE(CertificateStore);

    unsigned loadLocalCertificates();
    void pinRevocationList(const std::string& id, const dht::crypto::RevocationList& crl);

    const std::string certPath_;
    const std::string crlPath_;
    const std::string ocspPath_;

    mutable std::mutex lock_;
    std::map<std::string, std::shared_ptr<crypto::Certificate>> certs_;
    std::map<std::string, std::vector<std::weak_ptr<crypto::Certificate>>> paths_;

    // globally trusted certificates (root CAs)
    std::vector<std::shared_ptr<crypto::Certificate>> trustedCerts_;
};

/**
 * Keeps track of the allowed and trust status of certificates
 * Trusted is the status of top certificates we trust to build our
 * certificate chain: root CAs and other configured CAs.
 *
 * Allowed is the status of certificates we accept for incoming
 * connections.
 */
class TrustStore
{
public:
    TrustStore() = default;

    enum class PermissionStatus { UNDEFINED = 0, ALLOWED, BANNED };

    static PermissionStatus statusFromStr(const char* str);
    static const char* statusToStr(PermissionStatus s);

    bool addRevocationList(dht::crypto::RevocationList&& crl);

    bool setCertificateStatus(const std::string& cert_id, const PermissionStatus status);
    bool setCertificateStatus(const std::shared_ptr<crypto::Certificate>& cert,
                              PermissionStatus status,
                              bool local = true);

    PermissionStatus getCertificateStatus(const std::string& cert_id) const;

    std::vector<std::string> getCertificatesByStatus(PermissionStatus status) const;

    /**
     * Check that the certificate is allowed (valid and permited) for contact.
     * Valid means the certificate chain matches with our CA list,
     * has valid signatures, expiration dates etc.
     * Permited means at least one of the certificate in the chain is
     * ALLOWED (if allowPublic is false), and none is BANNED.
     *
     * @param crt the end certificate of the chain to check
     * @param allowPublic if false, requires at least one ALLOWED certificate.
     *                    (not required otherwise). In any case a BANNED
     *                    certificate means permission refusal.
     * @return true if the certificate is valid and permitted.
     */
    bool isAllowed(const crypto::Certificate& crt, bool allowPublic = false);

private:
    NON_COPYABLE(TrustStore);
    TrustStore(TrustStore&& o) = delete;
    TrustStore& operator=(TrustStore&& o) = delete;

    void updateKnownCerts();
    bool setCertificateStatus(std::shared_ptr<crypto::Certificate> cert,
                              const std::string& cert_id,
                              const TrustStore::PermissionStatus status,
                              bool local);
    void setStoreCertStatus(const crypto::Certificate& crt, bool status);
    void rebuildTrust();

    struct Status
    {
        bool allowed;
    };

    // unknown certificates with known status
    mutable std::recursive_mutex mutex_;
    std::map<std::string, Status> unknownCertStatus_;
    std::map<std::string, std::pair<std::shared_ptr<crypto::Certificate>, Status>> certStatus_;
    dht::crypto::TrustList allowed_;
};

} // namespace tls
} // namespace jami
