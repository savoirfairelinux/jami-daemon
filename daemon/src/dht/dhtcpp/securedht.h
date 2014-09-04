/*
Copyright (c) 2014 Savoir-Faire Linux Inc.

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
#include <memory>
#include <algorithm>

void print_hex(std::ostream& s, const uint8_t* data, size_t data_len);

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

    typedef std::pair<gnutls_x509_privkey_t, gnutls_x509_crt_t> Identity;
    typedef std::function<void(const SignedData&, bool)> SignatureCheckCallback;

    SecureDht() : Dht() {}

    /**
     * s, s6: bound socket descriptors for IPv45 and IPv6, respectively.
     *        For the Dht to be initialised, at least one of them must be >= 0.
     * privkey: the identity to use for the crypto layer and to compute
     *          our hash. The new instance take ownership of the object
     *          (caller should not call gnutls_x509_privkey_deinit)
     */
    SecureDht(int s, int s6, Identity id)
    : Dht(s, s6, InfoHash::get(readCertificate(id.second)), (unsigned char*)"RNG\0"),
        x509_key(id.first), certificate(id.second),
        publicKey(readCertificate(id.second))
    {
        if (s < 0 && s6 < 0)
            return;
        gnutls_privkey_init(&key);
        gnutls_privkey_import_x509(key, x509_key, 0);

        Value pk {Value::Type::PublicKey, publicKey};
        Dht::put(getId(), pk, [](bool ok) {
            if (ok)
                std::cout << "SecureDht: public key announced successfully" << std::endl;
            else
                std::cerr << "SecureDht: error while announcing public key!" << std::endl;
        });
    }

    virtual ~SecureDht();

    /**
     * Generate an RSA key pair (2048 bits) and returns a pointer to it.
     * Don't forget to free it after use (gnutls_x509_privkey_deinit).
     */
    static Identity generateIdentity();
    //static Blob certFromPrivkey(const gnutls_x509_privkey_t& privkey);
    static Blob readCertificate(const gnutls_x509_crt_t& cert);

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
        findPublicKey(data.signer, [data,callback,this](const Blob* key){
            if(!key) {
                callback(data, false);
                return;
            }
            callback(data, verifySignedData(data, *key));
        });
    }

    void findPublicKey(const InfoHash& node, std::function<void(const Blob*)> cb);

    bool registerPublicKey(const InfoHash& node, const Blob& publicKey);
    const Blob* getPublicKey(const InfoHash& node) const;

private:
    // prevent copy
    SecureDht(const SecureDht&);
    SecureDht& operator=(const SecureDht&);

    gnutls_x509_privkey_t x509_key {nullptr};
    gnutls_x509_crt_t certificate {nullptr};

    gnutls_privkey_t key {nullptr};
    Blob publicKey {};

    std::vector<std::pair<InfoHash, Blob>> nodesPublicKeys {};

    SignedData signData(const Blob& data) const;

    bool verifySignedData(const SignedData& data, const Blob& publicKey);

    /**
     * Encrypt the data using the provided public key.
     * Throws a DhtException in case of error
     */
    Blob encryptData(const Blob& data, const Blob& key);

    /**
     * Decrypt the ciphertext using our current private key.
     * Throws a DhtException in case of error
     * @param ciphertext encrypted data
     * @return decrypted data
     */
    Blob decryptData(const Blob& ciphertext);

    /**
     * Don't forget to free it after use (gnutls_x509_pubkey_deinit).
     */
    static gnutls_pubkey_t pubkeyFromCert(const Blob& cert);

};

}
