/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#pragma once

#include "dht.h"
#include "crypto.h"

#include <map>
#include <vector>
#include <memory>

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
    using Dht::exportNodes;
    using Dht::exportValues;
    using Dht::importValues;
    using Dht::getStatus;
    using Dht::dumpTables;
    using Dht::put;
    using Dht::setLoggers;

    InfoHash getId() const {
        return key_->getPublicKey().getId();
    }

    ValueType secureType(ValueType&& type);

    ValueType secureType(const ValueType& type) {
        ValueType tmp_type = type;
        return secureType(std::move(tmp_type));
    }

    virtual void registerType(const ValueType& type) {
        Dht::registerType(secureType(type));
    }
    virtual void registerType(ValueType&& type) {
        Dht::registerType(secureType(std::forward<ValueType>(type)));
    }
    virtual void registerInsecureType(const ValueType& type) {
        Dht::registerType(type);
    }

    /**
     * "Secure" get(), that will check the signature of signed data, and decrypt encrypted data.
     * If the signature can't be checked, or if the data can't be decrypted, it is not returned.
     * Public, non-signed & non-encrypted data is retransmitted as-is.
     */
    void get(const InfoHash& id, GetCallback cb, DoneCallback donecb, Value::Filter = Value::AllFilter());

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

    void findCertificate(const InfoHash& node, std::function<void(const std::shared_ptr<crypto::Certificate>)> cb);

    const std::shared_ptr<crypto::Certificate> registerCertificate(const InfoHash& node, const Blob& publicKey);
    const std::shared_ptr<crypto::Certificate> getCertificate(const InfoHash& node) const;

private:
    // prevent copy
    SecureDht(const SecureDht&) = delete;
    SecureDht& operator=(const SecureDht&) = delete;

    std::shared_ptr<crypto::PrivateKey> key_ {};
    std::shared_ptr<crypto::Certificate> certificate_ {};

    std::map<InfoHash, std::shared_ptr<crypto::Certificate>> nodesCertificates_ {};
};

const ValueType CERTIFICATE_TYPE = {
    8, "Certificate", 60 * 60 * 24 * 7,
    // A certificate can only be stored at it's public key ID.
    [](InfoHash id, std::shared_ptr<Value>& v, InfoHash, const sockaddr*, socklen_t) {
        try {
            crypto::Certificate crt(v->data);
            // TODO check certificate signature
            return crt.getPublicKey().getId() == id;
        } catch (const std::exception& e) {}
        return false;
    },
    [](InfoHash /*id*/, const std::shared_ptr<Value>& o, std::shared_ptr<Value>& n, InfoHash, const sockaddr*, socklen_t) {
        try {
            crypto::Certificate crt_old(o->data);
            crypto::Certificate crt_new(n->data);
            return crt_old.getPublicKey().getId() == crt_new.getPublicKey().getId();
        } catch (const std::exception& e) {}
        return false;
    }
};

}
