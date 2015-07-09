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
#include <future>
#include <mutex>

struct gnutls_x509_trust_list_st;

namespace ring { namespace tls {

namespace crypto = dht::crypto;

class CertificateStore {
public:
    static CertificateStore& instance();

    CertificateStore();

    std::vector<std::string> getPinnedCertificates() const;
    std::shared_ptr<crypto::Certificate> getCertificate(const std::string& cert_id) const;

    std::vector<std::string> pinCertificate(const std::vector<uint8_t>& crt, bool local = true) noexcept;
    std::vector<std::string> pinCertificate(crypto::Certificate&& crt, bool local = true);
    std::vector<std::string> pinCertificate(std::shared_ptr<crypto::Certificate> crt, bool local = true);
    bool unpinCertificate(const std::string&);

    void pinCertificatePath(const std::string& path);
    unsigned unpinCertificatePath(const std::string&);

private:
    NON_COPYABLE(CertificateStore);

    unsigned loadLocalCertificates(const std::string& path);

    const std::string certPath_;

    mutable std::mutex lock_;
    std::map<std::string, std::shared_ptr<crypto::Certificate>> certs_;
    std::map<std::string, std::vector<std::weak_ptr<crypto::Certificate>>> paths_;
};


class TrustStore {
public:
    TrustStore();
    virtual ~TrustStore();

    enum class Status {
        UNDEFINED = 0,
        ALLOWED,
        BANNED
    };

    static Status statusFromStr(const char* str);
    static const char* statusToStr(Status s);

    bool setCertificateStatus(const std::string& cert_id, const Status status);
    bool setCertificateStatus(std::shared_ptr<crypto::Certificate>& cert, const Status status, bool local = true);
    Status getCertificateStatus(const std::string& cert_id) const;
    std::vector<std::string> getCertificatesByStatus(Status status);

    bool isTrusted(const crypto::Certificate& crt);

private:
    NON_COPYABLE(TrustStore);

    static std::vector<gnutls_x509_crt_t> getChain(const crypto::Certificate& crt);

    void updateKnownCerts();
    void setStoreCertStatus(const crypto::Certificate& crt, Status status);

    // unknown certificates with known status
    std::map<std::string, Status> unknownCertStatus_;
    std::map<std::string, std::pair<std::shared_ptr<crypto::Certificate>, Status>> certStatus_;
    gnutls_x509_trust_list_st* trust_;
};

}} // namespace ring::tls
