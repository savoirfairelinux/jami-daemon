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

#pragma once

#include "dring/security_const.h"
#include "noncopyable.h"

#include <opendht/crypto.h>

#include <string>
#include <vector>
#include <map>
#include <set>

struct gnutls_x509_trust_list_st;

namespace ring {
namespace tls {

namespace crypto = dht::crypto;

class CertificateStore {
public:
    static CertificateStore& instance();

    CertificateStore();

    std::vector<std::string> getPinnedCertificates() const;
    std::shared_ptr<crypto::Certificate> getCertificate(const std::string& cert_id) const;

    std::string pinCertificate(const std::vector<uint8_t>& crt, bool local = true);
    std::string pinCertificate(crypto::Certificate&& crt, bool local = true);
    std::string pinCertificate(std::shared_ptr<crypto::Certificate> crt, bool local = true);

    std::string pinCertificate(const std::string& path);

    bool unpinCertificate(const std::string&);

private:

    void loadCertificates(const std::string& path);

    const std::string certPath_;

    std::map<std::string, std::shared_ptr<crypto::Certificate>> certs_;
};


class TrustStore {
public:
    TrustStore();
    virtual ~TrustStore();

    bool setCertificateStatus(const std::string& cert_id, const DRing::Certificate::Status status);
    bool setCertificateStatus(std::shared_ptr<crypto::Certificate>& cert, const DRing::Certificate::Status status, bool local = true);
    DRing::Certificate::Status getCertificateStatus(const std::string& cert_id) const;
    std::vector<std::string> getCertificatesByStatus(DRing::Certificate::Status status);

    bool isTrusted(const crypto::Certificate& crt);

private:
    NON_COPYABLE(TrustStore);

    static std::vector<gnutls_x509_crt_t> getChain(const crypto::Certificate& crt);

    void updateKnownCerts();
    void setStoreCertStatus(const crypto::Certificate& crt, DRing::Certificate::Status status);

    // unknown certificates with known status
    std::map<std::string, DRing::Certificate::Status> unknownCertStatus_;
    std::map<std::string, std::pair<std::shared_ptr<crypto::Certificate>, DRing::Certificate::Status>> certStatus_;
    gnutls_x509_trust_list_st* trust_;
};

}} // namespace ring::tls
