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
    struct PublicKey {
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
    };

    struct Certificate : public Serializable {
        Certificate() {}
        Certificate(gnutls_x509_crt_t crt) : cert(crt) {}
        Certificate(const Blob& crt);
        Certificate(Certificate&& o) noexcept : cert(o.cert) { o.cert = nullptr; };
        ~Certificate();
        operator bool() const { return cert; }
        PublicKey getPublicKey() const;
        //Blob serialize() const;
        void pack(Blob& b) const override;
        void unpack(Blob::const_iterator& begin, Blob::const_iterator& end) override;
        //void pack(Blob::const_iterator& begin, Blob::const_iterator& end) const;

    private:
        Certificate(const Certificate&) = delete;
        Certificate& operator=(const Certificate&) = delete;
        gnutls_x509_crt_t cert {};
    };

    typedef std::pair<std::shared_ptr<PrivateKey>, std::shared_ptr<Certificate>> Identity;
    typedef std::function<void(bool)> SignatureCheckCallback;

    SecureDht() {}

    /**
     * s, s6: bound socket descriptors for IPv4 and IPv6, respectively.
     *        For the Dht to be initialised, at least one of them must be >= 0.
     * id:    the identity to use for the crypto layer and to compute
     *        our own hash on the Dht.
     */
    SecureDht(int s, int s6, Identity id);

    /**
     * Generate an RSA key pair (2048 bits) and a self-signed certificate and returns them.
     */
    static Identity generateIdentity();

    using Dht::periodic;
    using Dht::pingNode;
    using Dht::insertNode;
    using Dht::getNodes;
    using Dht::getStatus;
    using Dht::getId;
    using Dht::dumpTables;
    using Dht::put;

    /**
     * "Secure" get(), that will check the signature of signed data, and decrypt encrypted data.
     * If the signature can't be checked, or if the data can't be decrypted, it is not returned.
     * Public, non-signed & non-encrypted data is retransmitted as-is.
     */
    void get(const InfoHash& id, GetCallback cb, DoneCallback donecb);

    /**
     * Will take ownership of the value, sign it using our private key and put it in the DHT.
     */
    void putSigned(const InfoHash& hash, Value&& data, DoneCallback callback);

    /**
     * Will sign the data using our private key, encrypt it using the recipient' public key,
     * and put it in the DHT.
     * The operation will be immediate if the recipient' public key is known (otherwise it will be retrived first).
     */
    void putEncrypted(const InfoHash& hash, const InfoHash& to, const std::shared_ptr<Value>& val, DoneCallback callback);
    void putEncrypted(const InfoHash& hash, const InfoHash& to, Value&& v, DoneCallback callback) {
        putEncrypted(hash, to, std::make_shared<Value>(std::move(v)), callback);
    }

    /**
     * Take ownership of the value and sign it using our private key.
     */
    void sign(Value& v) const {
        if (v.flags.isEncrypted())
            throw DhtException("Can't sign encrypted data.");
        if (v.flags.isSigned() && v.owner == getId())
            return;
        v.owner = getId();
        v.setSignature(key->sign(v.getToSign()));
    }

    Value encrypt(Value& v, const PublicKey& to) const {
        if (v.flags.isEncrypted()) {
            throw DhtException("Data is already encrypted.");
        }
        v.setRecipient(to.getId());
        sign(v);
        Value nv {v.id};
        nv.setCypher(to.encrypt(v.getToEncrypt()));
        return nv;
    }

    Value decrypt(Value& v) {
        if (not v.flags.isEncrypted())
            return v;
        auto decrypted = key->decrypt(v.cypher);
        Value ret {v.id};
        auto pb = decrypted.cbegin(), pe = decrypted.cend();
        ret.unpackBody(pb, pe);
        return ret;
    }

    void verifySigned(const std::shared_ptr<Value>& data, SignatureCheckCallback callback) {
        if (!data || data->flags.isEncrypted() || !data->flags.isSigned() || data->signature.empty()) {
            if (callback)
                callback(false);
        }
        findCertificate(data->owner, [data,callback,this](const std::shared_ptr<Certificate> crt) {
            if (!crt || !*crt) {
                if (callback)
                    callback(false);
                return;
            }
            if (callback)
                callback(crt->getPublicKey().checkSignature(data->getToSign(), data->signature));
        });
    }

    void findCertificate(const InfoHash& node, std::function<void(const std::shared_ptr<Certificate>)> cb);

    const std::shared_ptr<Certificate> registerCertificate(const InfoHash& node, const Blob& publicKey);
    const std::shared_ptr<Certificate> getCertificate(const InfoHash& node) const;

private:
    // prevent copy
    SecureDht(const SecureDht&) = delete;
    SecureDht& operator=(const SecureDht&) = delete;

    std::shared_ptr<PrivateKey> key {};
    std::shared_ptr<Certificate> certificate {};

    std::map<InfoHash, std::shared_ptr<Certificate>> nodesCertificates {};

};

}
