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

extern "C" {
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>
}

#include <random>

namespace dht {

SecureDht::SecureDht(int s, int s6, crypto::Identity id)
: Dht(s, s6, InfoHash::get("node:"+id.second->getPublicKey().getId().toString())), key_(id.first), certificate_(id.second)
{
    if (s < 0 && s6 < 0)
        return;

    int rc = gnutls_global_init();
    if (rc != GNUTLS_E_SUCCESS)
        throw DhtException(std::string("Error initializing GnuTLS: ")+gnutls_strerror(rc));

    auto certId = certificate_->getPublicKey().getId();
    if (certId != key_->getPublicKey().getId())
        throw DhtException("SecureDht: provided certificate doesn't match private key.");

    registerType(ValueType::USER_DATA);
    registerInsecureType(ServiceAnnouncement::TYPE);
    registerInsecureType(CERTIFICATE_TYPE);

    Dht::put(certId, Value {
        CERTIFICATE_TYPE,
        *certificate_
    }, [this](bool ok) {
        if (ok)
            DHT_DEBUG("SecureDht: public key announced successfully");
        else
            DHT_ERROR("SecureDht: error while announcing public key!");
    });
}

SecureDht::~SecureDht()
{
    gnutls_global_deinit();
}

ValueType
SecureDht::secureType(ValueType&& type)
{
    type.storePolicy = [this,type](InfoHash id, std::shared_ptr<Value>& v, InfoHash nid, const sockaddr* a, socklen_t al) {
        if (v->isSigned()) {
            if (!v->owner.checkSignature(v->getToSign(), v->signature)) {
                DHT_WARN("Signature verification failed");
                return false;
            }
            else
                DHT_WARN("Signature verification succeded");
        }
        return type.storePolicy(id, v, nid, a, al);
    };
    type.editPolicy = [this,type](InfoHash id, const std::shared_ptr<Value>& o, std::shared_ptr<Value>& n, InfoHash nid, const sockaddr* a, socklen_t al) {
        if (!o->isSigned())
            return type.editPolicy(id, o, n, nid, a, al);
        if (o->owner != n->owner) {
            DHT_WARN("Edition forbidden: owner changed.");
            return false;
        }
        if (!o->owner.checkSignature(n->getToSign(), n->signature)) {
            DHT_WARN("Edition forbidden: signature verification failed.");
            return false;
        }
        DHT_WARN("Edition old seq: %d, new seq: %d.", o->seq, n->seq);
        if (o->seq == n->seq) {
            // If the data is exactly the same,
            // it can be reannounced, possibly by someone else.
            if (o->getToSign() != n->getToSign())
                return false;
        }
        else if (n->seq < o->seq)
            return false;
        return true;
    };
    return type;
}

const std::shared_ptr<crypto::Certificate>
SecureDht::getCertificate(const InfoHash& node) const
{
    if (node == getId())
        return certificate_;
    auto it = nodesCertificates_.find(node);
    if (it == nodesCertificates_.end())
        return nullptr;
    else
        return it->second;
}

const std::shared_ptr<crypto::Certificate>
SecureDht::registerCertificate(const InfoHash& node, const Blob& data)
{
    std::shared_ptr<crypto::Certificate> crt;
    try {
        crt = std::make_shared<crypto::Certificate>(data);
    } catch (const std::exception& e) {
        return nullptr;
    }
    InfoHash h = crt->getPublicKey().getId();
    if (node == h) {
        DHT_DEBUG("Registering public key for %s", h.toString().c_str());
        nodesCertificates_[h] = crt;
    } else {
        DHT_DEBUG("Certificate %s for node %s does not match node id !", h.toString().c_str(), node.toString().c_str());
        return nullptr;
    }
    auto it = nodesCertificates_.find(h);
    if (it == nodesCertificates_.end()) {
        return nullptr;
    }
    return it->second;
}

void
SecureDht::findCertificate(const InfoHash& node, std::function<void(const std::shared_ptr<crypto::Certificate>)> cb)
{
    std::shared_ptr<crypto::Certificate> b = getCertificate(node);
    if (b && *b) {
        std::cout << "Using public key from cache for " << node << std::endl;
        cb(b);
        return;
    }
    auto found = std::make_shared<bool>(false);
    Dht::get(node, [cb,node,found,this](const std::vector<std::shared_ptr<Value>>& vals) {
        if (*found)
            return false;
        for (const auto& v : vals) {
            if (auto cert = registerCertificate(node, v->data)) {
                *found = true;
                std::cout << "Found public key for " << node << std::endl;
                cb(cert);
                return false;
            }
        }
        return true;
    }, [cb,found](bool) {
        if (!*found)
            cb(nullptr);
    }, Value::TypeFilter(CERTIFICATE_TYPE));
}


void
SecureDht::get(const InfoHash& id, GetCallback cb, DoneCallback donecb, Value::Filter filter)
{
    Dht::get(id,
        [=](const std::vector<std::shared_ptr<Value>>& values) {
            std::vector<std::shared_ptr<Value>> tmpvals {};
            for (const auto& v : values) {
                if (v->isEncrypted()) {
                    try {
                        Value decrypted_val = std::move(decrypt(*v));
                        if (decrypted_val.recipient == getId()) {
                            auto dv = std::make_shared<Value>(std::move(decrypted_val));
                            if (dv->owner.checkSignature(dv->getToSign(), dv->signature))
                                tmpvals.push_back(v);
                            else
                                DHT_WARN("Signature verification failed for %s", id.toString().c_str());
                        }
                    } catch (const std::exception& e) {
                        DHT_WARN("Could not decrypt value %s at infohash %s", v->toString().c_str(), id.toString().c_str());
                        continue;
                    }
                } else if (v->isSigned()) {
                    if (v->owner.checkSignature(v->getToSign(), v->signature))
                        tmpvals.push_back(v);
                    else
                        DHT_WARN("Signature verification failed for %s", id.toString().c_str());
                } else {
                    tmpvals.push_back(v);
                }
            }
            if (not tmpvals.empty())
                cb(tmpvals);
            return true;
        },
        donecb,
        filter);
}

void
SecureDht::putSigned(const InfoHash& hash, Value&& val, DoneCallback callback)
{
    if (val.id == Value::INVALID_ID) {
        auto id = getId();
        static_assert(sizeof(Value::Id) <= sizeof(InfoHash), "Value::Id can't be larger than InfoHash");
        val.id = *reinterpret_cast<Value::Id*>(id.data());
    }
    // TODO search the DHT instead of using the local value
    auto p = getPut(hash, val.id);
    if (p) {
        DHT_WARN("Found previous value being announced.");
        val.seq = p->seq + 1;
    }
    sign(val);
    put(hash, std::move(val), callback);
}

void
SecureDht::putEncrypted(const InfoHash& hash, const InfoHash& to, const std::shared_ptr<Value>& val, DoneCallback callback)
{
    findCertificate(to, [=](const std::shared_ptr<crypto::Certificate> crt) {
        if(!crt || !*crt) {
            if (callback)
                callback(false);
            return;
        }
        DHT_WARN("Encrypting data for PK: %s", crt->getPublicKey().getId().toString().c_str());
        try {
            put(hash, encrypt(*val, crt->getPublicKey()), callback);
        } catch (const std::exception& e) {
            DHT_WARN("Error putting encrypted data: %s", e.what());
            if (callback)
                callback(false);
        }
    });
}

void
SecureDht::sign(Value& v) const
{
    if (v.flags.isEncrypted())
        throw DhtException("Can't sign encrypted data.");
    v.owner = key_->getPublicKey();
    v.flags = Value::ValueFlags(true, false);
    v.signature = key_->sign(v.getToSign());
}

Value
SecureDht::encrypt(Value& v, const crypto::PublicKey& to) const
{
    if (v.flags.isEncrypted()) {
        throw DhtException("Data is already encrypted.");
    }
    v.setRecipient(to.getId());
    sign(v);
    Value nv {v.id};
    nv.setCypher(to.encrypt(v.getToEncrypt()));
    return nv;
}

Value
SecureDht::decrypt(const Value& v)
{
    if (not v.flags.isEncrypted())
        throw DhtException("Data is not encrypted.");
    auto decrypted = key_->decrypt(v.cypher);
    Value ret {v.id};
    auto pb = decrypted.cbegin(), pe = decrypted.cend();
    ret.unpackBody(pb, pe);
    return ret;
}

}
