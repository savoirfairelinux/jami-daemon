/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
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

#include "diffie-hellman.h"
#include "logger.h"

#include <chrono>
#include <ciso646>

namespace ring { namespace tls {

DhParams::DhParams(const std::vector<uint8_t>& data)
{
    gnutls_dh_params_t new_params_;
    int ret = gnutls_dh_params_init(&new_params_);
    if (ret)
        throw std::runtime_error(std::string("Error initializing DH params: ") + gnutls_strerror(ret));
    params_.reset(new_params_);
    const gnutls_datum_t dat {(uint8_t*)data.data(), (unsigned)data.size()};
    if (int ret_pem = gnutls_dh_params_import_pkcs3(params_.get(), &dat, GNUTLS_X509_FMT_PEM))
        if (int ret_der = gnutls_dh_params_import_pkcs3(params_.get(), &dat, GNUTLS_X509_FMT_DER))
            throw std::runtime_error(std::string("Error importing DH params: ") + gnutls_strerror(ret_pem) + " " + gnutls_strerror(ret_der));
}

DhParams&
DhParams::operator=(const DhParams& other)
{
    if (not params_) {
        // We need a valid DH params pointer for the copy
        gnutls_dh_params_t new_params_;
        auto err = gnutls_dh_params_init(&new_params_);
        if (err != GNUTLS_E_SUCCESS)
            throw std::runtime_error(std::string("Error initializing DH params: ") + gnutls_strerror(err));
        params_.reset(new_params_);
    }

    auto err = gnutls_dh_params_cpy(params_.get(), other.get());
    if (err != GNUTLS_E_SUCCESS)
        throw std::runtime_error(std::string("Error copying DH params: ") + gnutls_strerror(err));

    return *this;
}

std::vector<uint8_t>
DhParams::serialize() const
{
    if (!params_) {
        RING_WARN("serialize() called on an empty DhParams");
        return {};
    }
    gnutls_datum_t out;
    if (gnutls_dh_params_export2_pkcs3(params_.get(), GNUTLS_X509_FMT_PEM, &out))
        return {};
    std::vector<uint8_t> ret {out.data, out.data+out.size};
    gnutls_free(out.data);
    return ret;
}

DhParams
DhParams::generate()
{
    using clock = std::chrono::high_resolution_clock;

    auto bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, /* GNUTLS_SEC_PARAM_HIGH */ GNUTLS_SEC_PARAM_HIGH);
    RING_DBG("Generating DH params with %u bits", bits);
    auto start = clock::now();

    gnutls_dh_params_t new_params_;
    int ret = gnutls_dh_params_init(&new_params_);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error initializing DH params: %s", gnutls_strerror(ret));
        return {};
    }
    DhParams params {new_params_};

    ret = gnutls_dh_params_generate2(params.get(), bits);
    if (ret != GNUTLS_E_SUCCESS) {
        RING_ERR("Error generating DH params: %s", gnutls_strerror(ret));
        return {};
    }

    std::chrono::duration<double> time_span = clock::now() - start;
    RING_DBG("Generated DH params with %u bits in %lfs", bits, time_span.count());
    return params;
}

}} // namespace ring::tls
