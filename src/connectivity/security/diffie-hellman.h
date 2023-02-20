/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include <gnutls/gnutls.h>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace jami {
namespace tls {

class DhParams
{
public:
    DhParams() = default;
    DhParams(DhParams&&) = default;
    DhParams(const DhParams& other) { *this = other; }

    DhParams& operator=(DhParams&& other) = default;
    DhParams& operator=(const DhParams& other);

    /// \brief Construct by taking ownership of given gnutls DH params
    ///
    /// User should not call gnutls_dh_params_deinit on given \a raw_params.
    /// The object is stolen and its live is manager by our object.
    explicit DhParams(gnutls_dh_params_t p)
        : params_ {p, gnutls_dh_params_deinit}
    {}

    /** Deserialize DER or PEM encoded DH-params */
    DhParams(const std::vector<uint8_t>& data);

    gnutls_dh_params_t get() { return params_.get(); }
    gnutls_dh_params_t get() const { return params_.get(); }

    explicit inline operator bool() const { return bool(params_); }

    /** Serialize data in PEM format */
    std::vector<uint8_t> serialize() const;

    static DhParams generate();

    static DhParams loadDhParams(const std::string& path);

private:
    std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)*>
        params_ {nullptr, gnutls_dh_params_deinit};
};

} // namespace tls
} // namespace jami
