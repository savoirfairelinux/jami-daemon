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

#include "dht.h"

extern "C" {
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>
}

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>

namespace Dht {

class SecureDht : private Dht {
public:

    struct SignedData {
        SignedData() {}
        SignedData(const Blob& blob) {
            if (blob.size() < HASH_LEN + 256)
                throw std::invalid_argument("blob too small to hold signed data.");
            data = {blob.begin()+HASH_LEN+256, blob.end()};
            signer = {blob.data()};
            signature = {blob.begin()+HASH_LEN, blob.begin()+HASH_LEN+256};
        }
        SignedData(const Blob& data, const InfoHash& signer, const Blob& signature) : data(data), signer(signer), signature(signature) {}

        operator Blob(){
            Blob b;
            b.reserve(signer.size() + signature.size() + data.size());
            b.insert(b.end(), signer.begin(), signer.end());
            b.insert(b.end(), signature.begin(), signature.end());
            b.insert(b.end(), data.begin(), data.end());
            return b;
        }

        Blob data {};
        InfoHash signer {};
        Blob signature {};
    };

    struct PublicKey {
        PublicKey() {}
        PublicKey(gnutls_pubkey_t k) : pk(k) {}
        PublicKey(PublicKey&& o) noexcept : pk(o.pk) { o.pk = nullptr; };
        ~PublicKey();
        operator bool() const { return pk; }

        InfoHash getId() const;
        bool checkSignature(const SignedData&) const;
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
        ~PrivateKey();
        operator bool() const { return key; }
        PublicKey getPublicKey() const;
        Blob serialize() const;
        SignedData sign(const Blob&, const InfoHash& signer_id) const;
        Blob decrypt(const Blob& cypher) const;

    private:
        PrivateKey(const PrivateKey&) = delete;
        PrivateKey& operator=(const PrivateKey&) = delete;
        gnutls_privkey_t key {};
        gnutls_x509_privkey_t x509_key {};
    };

    struct Certificate {
        Certificate() {}
        Certificate(gnutls_x509_crt_t crt) : cert(crt) {}
        Certificate(const Blob& crt);
        Certificate(Certificate&& o) noexcept : cert(o.cert) { o.cert = nullptr; };
        ~Certificate();
        operator bool() const { return cert; }
        PublicKey getPublicKey() const;
        Blob serialize() const;

    private:
        Certificate(const Certificate&) = delete;
        Certificate& operator=(const Certificate&) = delete;
        gnutls_x509_crt_t cert {};
    };

    typedef std::pair<std::shared_ptr<PrivateKey>, std::shared_ptr<Certificate>> Identity;
    typedef std::function<void(const SignedData&, bool)> SignatureCheckCallback;

    SecureDht() {}

    /**
     * s, s6: bound socket descriptors for IPv45 and IPv6, respectively.
     *        For the Dht to be initialised, at least one of them must be >= 0.
     * id:    the identity to use for the crypto layer and to compute
     *        our own hash on the Dht.
     */
    SecureDht(int s, int s6, Identity id)
    : Dht(s, s6, id.second->getPublicKey().getId(), (unsigned char*)"RNG\0"),
        key(id.first), certificate(id.second)
    {
        if (s < 0 && s6 < 0)
            return;
        if (certificate->getPublicKey().getId() != key->getPublicKey().getId()) {
            std::cerr << "SecureDht: provided certificate doesn't match private key." << std::endl;
        }
        Value pk {Value::Type::Certificate, certificate->serialize()};
        Dht::put(getId(), pk, [](bool ok) {
            if (ok)
                std::cout << "SecureDht: public key announced successfully" << std::endl;
            else
                std::cerr << "SecureDht: error while announcing public key!" << std::endl;
        });
    }

    /**
     * Generate an RSA key pair (2048 bits) and a self-signed certificate and returns them.
     * Don't forget to free them after use (gnutls_x509_privkey_deinit, gnutls_x509_crt_deinit).
     */
    static Identity generateIdentity();

    using Dht::periodic;
    using Dht::pingNode;
    using Dht::getStatus;
    using Dht::getId;

    using Dht::put;

    /**
     * "Secure" get(), that will check the signature of signed data, and decrypt encrypted data.
     * If the signature can't be checked, or if the data can't be decrypted, it is not returned.
     * Public, non-signed & non-encrypted data is retransmitted as-is.
     */
    void get(const InfoHash& id, GetCallback cb, DoneCallback donecb);

    /**
     * Will sign the data using our private key and put it in the DHT.
     * Data must not be already signed.
     */
    void putSigned(const InfoHash& hash, const Value& val, DoneCallback callback);

    /**
     * Will sign the data using our private key, encrypt it using the destinee' public key,
     * and put it in the DHT.
     */
    void putEncrypted(const InfoHash& hash, const InfoHash& to, const Value& data, DoneCallback callback);

    void verifySigned(const SignedData& data, SignatureCheckCallback callback) {
        findCertificate(data.signer, [data,callback,this](const std::shared_ptr<Certificate> crt) {
            if (!crt || !*crt) {
                callback(data, false);
                return;
            }
            callback(data, crt->getPublicKey().checkSignature(data));
        });
    }

    void findCertificate(const InfoHash& node, std::function<void(const std::shared_ptr<Certificate>)> cb);

    const std::shared_ptr<Certificate> registerCertificate(const InfoHash& node, const Blob& publicKey);
    const std::shared_ptr<Certificate> getCertificate(const InfoHash& node) const;

private:
    // prevent copy
    SecureDht(const SecureDht&);
    SecureDht& operator=(const SecureDht&);

    std::shared_ptr<PrivateKey> key {};
    std::shared_ptr<Certificate> certificate {};

    std::map<InfoHash, std::shared_ptr<Certificate>> nodesCertificates {};

};

}
