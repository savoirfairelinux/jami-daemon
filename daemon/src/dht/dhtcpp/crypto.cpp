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


#include "securedht.h"
#include "logger.h"

extern "C" {
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>
}

#include <random>

static gnutls_digest_algorithm_t get_dig_for_pub(gnutls_pubkey_t pubkey)
{
    gnutls_digest_algorithm_t dig;
    int result = gnutls_pubkey_get_preferred_hash_algorithm(pubkey, &dig, nullptr);
    if (result < 0) {
        SFL_ERR("crt_get_preferred_hash_algorithm: %s\n", gnutls_strerror(result));
        return GNUTLS_DIG_UNKNOWN;
    }
    return dig;
}

static gnutls_digest_algorithm_t get_dig(gnutls_x509_crt_t crt)
{
    gnutls_pubkey_t pubkey;
    gnutls_pubkey_init(&pubkey);

    int result = gnutls_pubkey_import_x509(pubkey, crt, 0);
    if (result < 0) {
        SFL_ERR("gnutls_pubkey_import_x509: %s\n", gnutls_strerror(result));
        return GNUTLS_DIG_UNKNOWN;
    }

    gnutls_digest_algorithm_t dig = get_dig_for_pub(pubkey);
    gnutls_pubkey_deinit(pubkey);
    return dig;
}

namespace dht {
namespace crypto {

PrivateKey::PrivateKey(gnutls_x509_privkey_t k) : x509_key(k) {
    gnutls_privkey_init(&key);
    if (gnutls_privkey_import_x509(key, k, GNUTLS_PRIVKEY_IMPORT_COPY) != GNUTLS_E_SUCCESS) {
        key = nullptr;
        throw DhtException("Can't load private key !");
    }
}

PrivateKey::~PrivateKey()
{
    if (key) {
        gnutls_privkey_deinit(key);
        key = nullptr;
    }
    if (x509_key) {
        gnutls_x509_privkey_deinit(x509_key);
        x509_key = nullptr;
    }
}

Blob
PrivateKey::sign(const Blob& data) const
{
    if (!key)
        throw DhtException("Can't sign data: no private key set !");
    gnutls_datum_t sig;
    const gnutls_datum_t dat {(unsigned char*)data.data(), (unsigned)data.size()};
    if (gnutls_privkey_sign_data(key, GNUTLS_DIG_SHA512, 0, &dat, &sig) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't sign data !");
    Blob ret(sig.data, sig.data+sig.size);
    gnutls_free(sig.data);
    return ret;
}

Blob
PrivateKey::decrypt(const Blob& cipher) const
{
    if (!key)
        throw DhtException("Can't decrypt data without private key !");
    const gnutls_datum_t dat {(uint8_t*)cipher.data(), (unsigned)cipher.size()};
    gnutls_datum_t out;
    if (gnutls_privkey_decrypt_data(key, 0, &dat, &out) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't decrypt data !");
    Blob ret {out.data, out.data+out.size};
    gnutls_free(out.data);
    return ret;
}

Blob
PrivateKey::serialize() const
{
    if (!x509_key)
        return {};
    size_t buf_sz = 8192;
    Blob buffer;
    buffer.resize(buf_sz);
    int err = gnutls_x509_privkey_export_pkcs8(x509_key, GNUTLS_X509_FMT_PEM, nullptr, GNUTLS_PKCS_PLAIN, buffer.data(), &buf_sz);
    if (err != GNUTLS_E_SUCCESS) {
        std::cerr << "Could not export certificate - " << gnutls_strerror(err) << std::endl;
        return {};
    }
    buffer.resize(buf_sz);
    return buffer;
}

PublicKey
PrivateKey::getPublicKey() const
{
    gnutls_pubkey_t pk;
    gnutls_pubkey_init(&pk);
    if (gnutls_pubkey_import_privkey(pk, key, GNUTLS_KEY_KEY_CERT_SIGN | GNUTLS_KEY_CRL_SIGN, 0) != GNUTLS_E_SUCCESS)
        return {};
    return PublicKey {pk};
}

PublicKey::~PublicKey()
{
    if (pk) {
        gnutls_pubkey_deinit(pk);
        pk = nullptr;
    }
}

bool
PublicKey::checkSignature(const Blob& data, const Blob& signature) const {
    if (!pk)
        return false;
    const gnutls_datum_t sig {(uint8_t*)signature.data(), (unsigned)signature.size()};
    const gnutls_datum_t dat {(uint8_t*)data.data(), (unsigned)data.size()};
    int rc = gnutls_pubkey_verify_data2(pk, GNUTLS_SIGN_RSA_SHA512, 0, &dat, &sig);
    return rc >= 0;
}

Blob
PublicKey::encrypt(const Blob& data) const
{
    if (!pk)
        throw DhtException("Can't read public key !");
    const gnutls_datum_t dat {(uint8_t*)data.data(), (unsigned)data.size()};
    gnutls_datum_t encrypted;
    if (gnutls_pubkey_encrypt_data(pk, 0, &dat, &encrypted) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't encrypt data using public key !");
    Blob ret {encrypted.data, encrypted.data+encrypted.size};
    gnutls_free(encrypted.data);
    return ret;
}

InfoHash
PublicKey::getId() const
{
    InfoHash id;
    size_t sz = id.size();
    gnutls_pubkey_get_key_id(pk, 0, id.data(), &sz);
    return id;
}

Certificate::Certificate(const Blob& certData) : cert(nullptr)
{
    unpackBlob(certData);
}

void
Certificate::unpack(Blob::const_iterator& begin, Blob::const_iterator& end)
{
    if (cert)
        gnutls_x509_crt_deinit(cert);
    gnutls_x509_crt_init(&cert);
    const gnutls_datum_t crt_dt {(uint8_t*)&(*begin), (unsigned)(end-begin)};
    int err = gnutls_x509_crt_import(cert, &crt_dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS) {
        cert = nullptr;
        throw std::invalid_argument(std::string("Could not read certificate - ") + gnutls_strerror(err));
    }
}

void
Certificate::pack(Blob& b) const
{
    auto b_size = b.size();
    size_t buf_sz = 8192;
    b.resize(b_size + buf_sz);
    int err = gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_PEM, b.data()+b_size, &buf_sz);
    if (err != GNUTLS_E_SUCCESS) {
        std::cerr << "Could not export certificate - " << gnutls_strerror(err) << std::endl;
        b.resize(b_size);
    }
    b.resize(b_size + buf_sz);
}

Certificate::~Certificate()
{
    if (cert) {
        gnutls_x509_crt_deinit(cert);
        cert = nullptr;
    }
}

PublicKey
Certificate::getPublicKey() const
{
    gnutls_pubkey_t pk;
    gnutls_pubkey_init(&pk);
    if (gnutls_pubkey_import_x509(pk, cert, 0) != GNUTLS_E_SUCCESS)
        return {};
    return PublicKey {pk};
}


crypto::Identity
generateIdentity()
{
    SFL_WARN("SecureDht: generating a new identity (2048 bits RSA key pair and self-signed certificate).");
    gnutls_x509_privkey_t key;
    gnutls_privkey_t pkey;
    if (gnutls_x509_privkey_init(&key) != GNUTLS_E_SUCCESS)
        return {};
    if (gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0) != GNUTLS_E_SUCCESS)
        return {};
    gnutls_privkey_init(&pkey);
    gnutls_privkey_import_x509(pkey, key, 0);

    gnutls_x509_crt_t cert;
    gnutls_x509_crt_init(&cert);
    gnutls_x509_crt_set_activation_time(cert, time(NULL));
    gnutls_x509_crt_set_expiration_time(cert, time(NULL) + (700 * 24 * 60 * 60));
    if (gnutls_x509_crt_set_key(cert, key) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when setting certificate key" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(key);
        return {};
    }
    if (gnutls_x509_crt_set_version(cert, 1) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when setting certificate version" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(key);
        return {};
    }
    /*uint8_t keyid[128];
    size_t keyidsize = sizeof(keyid);
    gnutls_x509_crt_get_key_id(cert, 0, keyid, &keyidsize);
    gnutls_x509_crt_set_subject_key_id(cert, keyid, keyidsize);*/

    const char* name = "dhtclient";
    gnutls_x509_crt_set_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, name, strlen(name));
    gnutls_x509_crt_set_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, name, strlen(name));
    //gnutls_x509_crt_set_key_usage (cert, GNUTLS_KEY_KEY_CERT_SIGN | GNUTLS_KEY_CRL_SIGN);

    std::mt19937 rd {std::random_device{}()};
    std::uniform_int_distribution<uint8_t> dist {};
    uint8_t cert_version = 1;//dist(rd);
    gnutls_x509_crt_set_serial(cert, &cert_version, sizeof(cert_version));
    //if (gnutls_x509_crt_sign2(cert, cert, key, GNUTLS_DIG_SHA512, 0) != GNUTLS_E_SUCCESS) {
    if (gnutls_x509_crt_privkey_sign(cert, cert, pkey, get_dig(cert), 0) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when signing certificate" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(key);
        return {};
    }
    return {std::make_shared<PrivateKey>(key), std::make_shared<Certificate>(cert)};
}

}

}
