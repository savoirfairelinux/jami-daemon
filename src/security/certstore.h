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

#include <opendht/crypto.h>

#include <string>
#include <vector>
#include <map>
#include <set>

namespace ring {
namespace tls {

namespace crypto = dht::crypto;

class CertificateStore {
public:
    static CertificateStore& instance();

    CertificateStore();

    std::vector<std::string> getCertificateList() const;
    std::shared_ptr<crypto::Certificate> getCertificate(const std::string&) const;
    //std::map<std::string, std::string> getCertificateDetails(const std::string&) const;

    std::string addCertificate(const std::vector<uint8_t>&);
    std::string addCertificate(crypto::Certificate&&);
    std::string addCertificate(std::shared_ptr<crypto::Certificate> crt);

    bool removeCertificate(const std::string& id);

    bool banCertificate(const std::string&);
    bool isBanned(const std::string& id) const { return bannedCerts_.find(id) != bannedCerts_.end(); }
private:

    void loadCertificates();
    void loadBannedCertificates();

    const std::string certPath_;
    const std::string bannedCertPath_;

    std::map<std::string, std::shared_ptr<crypto::Certificate>> certs_;
    std::set<std::string> bannedCerts_;

};

}} // namespace ring::tls
