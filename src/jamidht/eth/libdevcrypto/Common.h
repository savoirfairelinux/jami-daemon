/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Common.h
 * @author Alex Leverington <nessence@gmail.com>
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Ethereum-specific data structures & algorithms.
 */

#pragma once

#include <mutex>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>

namespace dev {

using Secret = SecureFixedHash<32>;

/// A public key: 64 bytes.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using Public = h512;

/// A signature: 65 bytes: r: [0, 32), s: [32, 64), v: 64.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using Signature = h520;

struct SignatureStruct
{
    SignatureStruct() = default;
    SignatureStruct(Signature const& _s) { *(h520*) this = _s; }
    SignatureStruct(h256 const& _r, h256 const& _s, uint8_t _v)
        : r(_r)
        , s(_s)
        , v(_v)
    {}
    operator Signature() const { return *(h520 const*) this; }

    /// @returns true if r,s,v values are valid, otherwise false
    bool isValid() const noexcept;

    h256 r;
    h256 s;
    uint8_t v = 0;
};

/// A vector of secrets.
using Secrets = std::vector<Secret>;

/// Convert a secret key into the public key equivalent.
Public toPublic(Secret const& _secret);

/// Convert a public key to address.
Address toAddress(Public const& _public);

/// Convert a secret key into address of public key equivalent.
/// @returns 0 if it's not a valid secret key.
Address toAddress(Secret const& _secret);

/// Simple class that represents a "key pair".
/// All of the data of the class can be regenerated from the secret key (m_secret) alone.
/// Actually stores a tuplet of secret, public and address (the right 160-bits of the public).
class KeyPair
{
public:
    KeyPair() = default;
    /// Normal constructor - populates object from the given secret key.
    /// If the secret key is invalid the constructor succeeds, but public key
    /// and address stay "null".
    KeyPair(Secret const& _sec);

    /// Create a new, randomly generated object.
    static KeyPair create();

    /// Create from an encrypted seed.
    // static KeyPair fromEncryptedSeed(bytesConstRef _seed, std::string const& _password);

    Secret const& secret() const { return m_secret; }

    /// Retrieve the public key.
    Public const& pub() const { return m_public; }

    /// Retrieve the associated address of the public key.
    Address const& address() const { return m_address; }

    bool operator==(KeyPair const& _c) const { return m_public == _c.m_public; }
    bool operator!=(KeyPair const& _c) const { return m_public != _c.m_public; }

private:
    Secret m_secret;
    Public m_public;
    Address m_address;
};

namespace crypto {

class InvalidState : public std::runtime_error
{
public:
    InvalidState()
        : runtime_error("invalid state") {};
};
class CryptoException : public std::runtime_error
{
public:
    CryptoException(const char* err)
        : runtime_error(err) {};
};

} // namespace crypto

} // namespace dev
