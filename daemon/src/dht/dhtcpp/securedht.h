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
#include "crypto.h"

//#include <iostream>
#include <map>
#include <vector>
#include <memory>
//#include <algorithm>

namespace dht {

class SecureDht : private Dht {
public:

    typedef std::function<void(bool)> SignatureCheckCallback;

    SecureDht() {}

    /**
     * s, s6: bound socket descriptors for IPv4 and IPv6, respectively.
     *        For the Dht to be initialised, at least one of them must be >= 0.
     * id:    the identity to use for the crypto layer and to compute
     *        our own hash on the Dht.
     */
    SecureDht(int s, int s6, crypto::Identity id);

    virtual ~SecureDht();

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
    void sign(Value& v) const;

    Value encrypt(Value& v, const crypto::PublicKey& to) const;

    Value decrypt(const Value& v);

    void verifySigned(const std::shared_ptr<Value>& data, SignatureCheckCallback callback);

    void findCertificate(const InfoHash& node, std::function<void(const std::shared_ptr<crypto::Certificate>)> cb);

    const std::shared_ptr<crypto::Certificate> registerCertificate(const InfoHash& node, const Blob& publicKey);
    const std::shared_ptr<crypto::Certificate> getCertificate(const InfoHash& node) const;

private:
    // prevent copy
    SecureDht(const SecureDht&) = delete;
    SecureDht& operator=(const SecureDht&) = delete;

    std::shared_ptr<crypto::PrivateKey> key {};
    std::shared_ptr<crypto::Certificate> certificate {};

    std::map<InfoHash, std::shared_ptr<crypto::Certificate>> nodesCertificates {};

};

}
