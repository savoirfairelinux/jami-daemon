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

namespace dht {

SecureDht::SecureDht(int s, int s6, crypto::Identity id)
: Dht(s, s6, id.second->getPublicKey().getId(), (unsigned char*)"RNG\0"), key_(id.first), certificate_(id.second)
{
    if (s < 0 && s6 < 0)
        return;
    int rc = gnutls_global_init();
    if (rc != GNUTLS_E_SUCCESS) {
        SFL_ERR("Error initializing GnuTLS : %s", gnutls_strerror(rc));
        throw DhtException("Error initializing GnuTLS");
    }
    if (certificate_->getPublicKey().getId() != key_->getPublicKey().getId()) {
        SFL_ERR("SecureDht: provided certificate doesn't match private key.");
    }
    Dht::registerType(crypto::CERTIFICATE);
    Value cert_val {
        crypto::CERTIFICATE,
        *certificate_
    };
    cert_val.owner = getId();
    Dht::put(getId(), std::move(cert_val), [](bool ok) {
        if (ok)
            SFL_DBG("SecureDht: public key announced successfully");
        else
            SFL_ERR("SecureDht: error while announcing public key!");
    });
}

SecureDht::~SecureDht()
{
    gnutls_global_deinit();
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
        std::cout << "Registering public key for " << h << std::endl;
        nodesCertificates_[h] = crt;
    } else {
        std::cerr << "Certificate " << h << " for node " << node << " does not match node id !" << std::endl;
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
    }, Value::TypeFilter(crypto::CERTIFICATE));
}

void
SecureDht::get(const InfoHash& id, GetCallback cb, DoneCallback donecb, Value::Filter filter)
{
    auto done = std::make_shared<bool>(false);
    auto check_sig = std::make_shared<std::vector<std::shared_ptr<Value>>>();
    Dht::get(id,
    [=](const std::vector<std::shared_ptr<Value>>& values) {
        for (const auto& v : values) {
            if (!v->flags.isEncrypted() && v->type == crypto::CERTIFICATE.id)
                registerCertificate(id, v->data);
        }
        if (*done) return false;
        std::vector<std::shared_ptr<Value>> tmpvals {};
        for (const auto& v : values) {
            if (v->flags.isSigned()) {
                check_sig->push_back(v);
                verifySigned(v, [=](bool ok) {
                    auto cs_ptr = check_sig.get();
                    auto it = std::find(cs_ptr->begin(), cs_ptr->end(), v);
                    if (it != cs_ptr->end()) {
                        cs_ptr->erase(it);
                        if (cb && ok) { // Signed data have been verified
                            std::vector<std::shared_ptr<Value>> tmpvals_ {*it};
                            *done |= !cb(tmpvals_);
                        }
                    }
                    if (donecb && *done && cs_ptr->empty()) {
                        donecb(true);
                    }
                });
            } else if (v->flags.isEncrypted()) {
                Blob decrypted_data;
                Value decrypted_val;
                try {
                    decrypted_data = key_->decrypt(v->data);
                    Value decrypted_val { v->id };
                    decrypted_val.unpackBlob(decrypted_data);
                } catch (const std::exception& e) {
                    SFL_WARN("Could not decrypt value %s at infohash %s", v->toString().c_str(), id.toString().c_str());
                    continue;
                }
                auto dv = std::make_shared<Value>(std::move(decrypted_val));
                check_sig->push_back(dv);
                verifySigned(dv, [=](bool ok) {
                    auto cs_ptr = check_sig.get();
                    auto it = std::find(cs_ptr->begin(), cs_ptr->end(), dv);
                    if (it != cs_ptr->end()) {
                        cs_ptr->erase(it);
                        if (cb && ok) { // Signed data have been verified
                            std::vector<std::shared_ptr<Value>> tmpvals_ {*it};
                            *done |= !cb(tmpvals_);
                        }
                    }
                    if (donecb && *done && cs_ptr->empty()) {
                        donecb(true);
                    }
                });
            } else {
                tmpvals.push_back(v);
            }
        }
        if (!tmpvals.empty())
            return cb ? cb(tmpvals) : true;
        else
            return true;
    },
    [=](bool ok) {
        *done = true;
        if (donecb && check_sig->empty())
            donecb(ok);
    },
    filter);
}

void
SecureDht::putSigned(const InfoHash& hash, Value&& val, DoneCallback callback)
{
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
        try {
            put(hash, encrypt(*val, crt->getPublicKey()), callback);
        } catch (const std::exception& e) {
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
    if (v.flags.isSigned() && v.owner == getId())
        return;
    v.owner = getId();
    v.setSignature(key_->sign(v.getToSign()));
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
        return v;
    auto decrypted = key_->decrypt(v.cypher);
    Value ret {v.id};
    auto pb = decrypted.cbegin(), pe = decrypted.cend();
    ret.unpackBody(pb, pe);
    return ret;
}

void
SecureDht::verifySigned(const std::shared_ptr<Value>& data, SignatureCheckCallback callback)
{
    if (!data || data->flags.isEncrypted() || !data->flags.isSigned() || data->signature.empty()) {
        if (callback)
            callback(false);
    }
    findCertificate(data->owner, [data,callback,this](const std::shared_ptr<crypto::Certificate> crt) {
        if (!crt || !*crt) {
            if (callback)
                callback(false);
            return;
        }
        if (callback)
            callback(crt->getPublicKey().checkSignature(data->getToSign(), data->signature));
    });
}


}
