/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include "value.h"

extern "C" {
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>
}

#include <vector>
#include <memory>

namespace dht {
namespace crypto {

struct PrivateKey;
struct Certificate;

typedef std::pair<std::shared_ptr<PrivateKey>, std::shared_ptr<Certificate>> Identity;

/**
 * Generate an RSA key pair (2048 bits) and a certificate.
 * If a certificate authority (ca) is provided, it will be used to
 * sign the certificate, otherwise the certificate will be self-signed.
 */
Identity generateIdentity(const std::string& name = "dhtnode", Identity ca = {});

struct PublicKey
{
    PublicKey() {}
    PublicKey(gnutls_pubkey_t k) : pk(k) {}
    PublicKey(PublicKey&& o) noexcept : pk(o.pk) { o.pk = nullptr; };

    ~PublicKey();
    operator bool() const { return pk; }

    InfoHash getId() const;
    bool checkSignature(const Blob& data, const Blob& signature) const;
    Blob encrypt(const Blob&) const;

    gnutls_pubkey_t pk {};
private:
    PublicKey(const PublicKey&) = delete;
    PublicKey& operator=(const PublicKey&) = delete;
};

struct PrivateKey
{
    PrivateKey() {}
    //PrivateKey(gnutls_privkey_t k) : key(k) {}
    PrivateKey(gnutls_x509_privkey_t k);
    PrivateKey(PrivateKey&& o) noexcept : key(o.key), x509_key(o.x509_key)
        { o.key = nullptr; o.x509_key = nullptr; };
    PrivateKey& operator=(PrivateKey&& o) noexcept;

    PrivateKey(const Blob& import);
    ~PrivateKey();
    operator bool() const { return key; }
    PublicKey getPublicKey() const;
    Blob serialize() const;
    Blob sign(const Blob&) const;
    Blob decrypt(const Blob& cypher) const;

private:
    PrivateKey(const PrivateKey&) = delete;
    PrivateKey& operator=(const PrivateKey&) = delete;
    gnutls_privkey_t key {};
    gnutls_x509_privkey_t x509_key {};

    friend dht::crypto::Identity dht::crypto::generateIdentity(const std::string&, dht::crypto::Identity);
};

struct Certificate : public Serializable {
    Certificate() {}
    Certificate(gnutls_x509_crt_t crt) : cert(crt) {}
    Certificate(const Blob& crt);
    Certificate(Certificate&& o) noexcept : cert(o.cert) { o.cert = nullptr; };
    Certificate& operator=(Certificate&& o) noexcept;

    ~Certificate();
    operator bool() const { return cert; }
    PublicKey getPublicKey() const;
    void pack(Blob& b) const override;
    void unpack(Blob::const_iterator& begin, Blob::const_iterator& end) override;

private:
    Certificate(const Certificate&) = delete;
    Certificate& operator=(const Certificate&) = delete;
    gnutls_x509_crt_t cert {};

    friend dht::crypto::Identity dht::crypto::generateIdentity(const std::string&, dht::crypto::Identity);
};

static const ValueType CERTIFICATE = {8, "Certificate", 60 * 60 * 24 * 7};

}
}
