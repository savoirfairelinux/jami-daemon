/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "dring/security_const.h"
#include "noncopyable.h"

#include <opendht/crypto.h>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <mutex>

struct gnutls_x509_trust_list_st;

namespace ring { namespace tls {

namespace crypto = dht::crypto;

enum class TrustStatus {
    UNTRUSTED = 0,
    TRUSTED
};
TrustStatus trustStatusFromStr(const char* str);
const char* statusToStr(TrustStatus s);


/**
 * Global certificate store.
 * Stores system root CAs and any other encountred certificate
 */
class CertificateStore {
public:
    static CertificateStore& instance();

    CertificateStore();

    std::vector<std::string> getPinnedCertificates() const;
    std::shared_ptr<crypto::Certificate> getCertificate(const std::string& cert_id) const;

    std::shared_ptr<crypto::Certificate> findCertificateByName(const std::string& name, crypto::Certificate::NameType type = crypto::Certificate::NameType::UNKNOWN) const;
    std::shared_ptr<crypto::Certificate> findCertificateByUID(const std::string& uid) const;
    std::shared_ptr<crypto::Certificate> findIssuer(std::shared_ptr<crypto::Certificate> crt) const;

    std::vector<std::string> pinCertificate(const std::vector<uint8_t>& crt, bool local = true) noexcept;
    std::vector<std::string> pinCertificate(crypto::Certificate&& crt, bool local = true);
    std::vector<std::string> pinCertificate(std::shared_ptr<crypto::Certificate> crt, bool local = true);
    bool unpinCertificate(const std::string&);

    void pinCertificatePath(const std::string& path, std::function<void(const std::vector<std::string>&)> cb = {});
    unsigned unpinCertificatePath(const std::string&);

    bool setTrustedCertificate(const std::string& id, TrustStatus status);
    std::vector<gnutls_x509_crt_t> getTrustedCertificates() const;

private:
    NON_COPYABLE(CertificateStore);

    unsigned loadLocalCertificates(const std::string& path);

    const std::string certPath_;

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
class TrustStore {
public:
    TrustStore();
    virtual ~TrustStore();

    enum class PermissionStatus {
        UNDEFINED = 0,
        ALLOWED,
        BANNED
    };

    static PermissionStatus statusFromStr(const char* str);
    static const char* statusToStr(PermissionStatus s);

    bool setCertificateStatus(const std::string& cert_id, const PermissionStatus status);
    bool setCertificateStatus(std::shared_ptr<crypto::Certificate>& cert, PermissionStatus status, bool local = true);

    bool setCertificateStatus(const std::string& cert_id, const TrustStatus status);
    bool setCertificateStatus(std::shared_ptr<crypto::Certificate>& cert, TrustStatus status, bool local = true);

    PermissionStatus getCertificateStatus(const std::string& cert_id) const;
    TrustStatus getCertificateTrustStatus(const std::string& cert_id) const;

    std::vector<std::string> getCertificatesByStatus(PermissionStatus status);

    bool isAllowed(const crypto::Certificate& crt);

    std::vector<gnutls_x509_crt_t> getTrustedCertificates() const;

private:
    NON_COPYABLE(TrustStore);

    void updateKnownCerts();
    void setStoreCertStatus(const crypto::Certificate& crt, PermissionStatus status);

    static bool matchTrustStore(std::vector<gnutls_x509_crt_t>&& crts, gnutls_x509_trust_list_st* store);

    struct Status {
        bool allowed : 1;
        bool trusted : 1;
    };

    // unknown certificates with known status
    std::map<std::string, Status> unknownCertStatus_;
    std::map<std::string, std::pair<std::shared_ptr<crypto::Certificate>, Status>> certStatus_;
    gnutls_x509_trust_list_st* allowed_;
};

std::vector<gnutls_x509_crt_t> getChain(const crypto::Certificate& crt);

}} // namespace ring::tls
