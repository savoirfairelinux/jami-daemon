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

namespace ring {
namespace tls {

namespace crypto = dht::crypto;

class CertificateStore {
public:
    static CertificateStore& instance();

    std::vector<std::string> getPinnedCertificates() { return {}; }

    std::string pinCertificate(const std::vector<uint8_t>&) { return {}; }
    std::string pinCertificate(const std::string&) { return {}; }
    std::string pinCertificate(const crypto::Certificate&) { return {}; }

    bool unpinCertificate(const std::string&) { return false; }

private:
};

}} // namespace ring::tls
