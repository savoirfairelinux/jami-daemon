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


#include "securedht.h"

#include <random>

void print_hex(std::ostream& s, const uint8_t* data, size_t data_len) {
    s << std::hex;
    for (size_t i=0; i<data_len; i++)
        s << std::setfill('0') << std::setw(2) << (unsigned)data[i];
    s << std::dec;
}

namespace Dht {
/*
SecureDht::SecureDht() {
    gnutls_x509_privkey_init(&x509_key);
    gnutls_x509_privkey_generate(x509_key, GNUTLS_PK_RSA, 2048, 0);
    gnutls_privkey_init(&key);
    gnutls_privkey_import_x509(key, x509_key, 0);
    {
        gnutls_datum_t m, e, d, p, q, u;
        gnutls_x509_privkey_export_rsa_raw(x509_key, &m, &e, &d, &p, &q, &u);
        gnutls_datum_t m2 = m;
        if (m2.size % 2) {
             m2.size--;
             m2.data++;
        }

        publicKey.resize(m2.size + e.size);
        std::cerr << "Modulo size: " << m2.size << " Exp size:" << e.size << std::endl;
        memcpy(publicKey.data(), m2.data, m2.size);
        memcpy(publicKey.data()+m2.size, e.data, e.size);

        gnutls_free(m.data); gnutls_free(e.data);
        gnutls_free(d.data);
        gnutls_free(p.data); gnutls_free(q.data);
        gnutls_free(u.data);
    }
     // print the public key
    {
        gnutls_pubkey_t pk;
        gnutls_pubkey_init(&pk);
        gnutls_pubkey_import_privkey(pk, key, 0, 0);
        gnutls_datum_t str;
        gnutls_pubkey_print(pk, GNUTLS_CRT_PRINT_FULL_NUMBERS, &str);
        std::cerr << std::string((char*)str.data, str.size) << std::endl;
        gnutls_free(str.data);
        gnutls_pubkey_deinit(pk);
    }

 //   gnutls_x509_crt_init(&certificate);
//    gnutls_x509_crt_set_key(certificate, x509_key);
}*/

SecureDht::~SecureDht() {
    if (certificate)
        gnutls_x509_crt_deinit (certificate);
    if (key)
        gnutls_privkey_deinit(key);
    if (x509_key)
        gnutls_x509_privkey_deinit(x509_key);
}

std::pair<gnutls_x509_privkey_t, gnutls_x509_crt_t>
SecureDht::generateIdentity()
{
    std::cout << "SecureDht::generateIdentity()" << std::endl;
    gnutls_x509_privkey_t id;
    if (gnutls_x509_privkey_init(&id) != GNUTLS_E_SUCCESS)
        return {};
    if (gnutls_x509_privkey_generate(id, GNUTLS_PK_RSA, 2048, 0) != GNUTLS_E_SUCCESS)
        return {};

    gnutls_x509_crt_t cert;
    gnutls_x509_crt_init(&cert);
    gnutls_x509_crt_set_activation_time(cert, time(NULL));
    gnutls_x509_crt_set_expiration_time(cert, time(NULL) + (700 * 24 * 60 * 60));
    if (gnutls_x509_crt_set_key(cert, id) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when setting certificate key" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(id);
        return {};
    }
    if (gnutls_x509_crt_set_version(cert, 1) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when setting certificate version" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(id);
        return {};
    }

    const char* name = "dhtclient";
    gnutls_x509_crt_set_issuer_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, name, strlen(name));
    gnutls_x509_crt_set_key_usage (cert, GNUTLS_KEY_KEY_CERT_SIGN | GNUTLS_KEY_CRL_SIGN);

    std::mt19937 rd {};
    {
        std::random_device rdev;
        rd.seed(rdev());
    }
    std::uniform_int_distribution<int> dist {};
    int cert_version = dist(rd);
    gnutls_x509_crt_set_serial(cert, &cert_version, sizeof(int));
    if (gnutls_x509_crt_sign2(cert, cert, id, GNUTLS_DIG_SHA512, 0) != GNUTLS_E_SUCCESS) {
        std::cerr << "Error when signing certificate" << std::endl;
        gnutls_x509_crt_deinit (cert);
        gnutls_x509_privkey_deinit(id);
        return {};
    }
    return {id, cert};
}
/*
Blob
SecureDht::certFromPrivkey(const gnutls_x509_privkey_t& privkey)
{
    if (!privkey)
        return Blob {};

    gnutls_datum_t m, e, d, p, q, u;
    gnutls_x509_privkey_export_rsa_raw(privkey, &m, &e, &d, &p, &q, &u);
    gnutls_datum_t m2 = m;
    if (m2.size % 2) { // fix for GNUTLS alignment issue
         m2.size--;
         m2.data++;
    }

    Blob pk;
    pk.resize(m2.size + e.size);
    //std::cerr << "Modulo size: " << m2.size << " Exp size:" << e.size << std::endl;
    memcpy(pk.data(), m2.data, m2.size);
    memcpy(pk.data()+m2.size, e.data, e.size);

    gnutls_free(m.data); gnutls_free(e.data);
    gnutls_free(d.data);
    gnutls_free(p.data); gnutls_free(q.data);
    gnutls_free(u.data);
    return pk;
}
*/
Blob
SecureDht::readCertificate(const gnutls_x509_crt_t& cert)
{
    if (!cert)
        return {};
    size_t buf_sz = 8192;
    Blob buffer;
    buffer.resize(buf_sz);
    int err = gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_PEM, buffer.data(), &buf_sz);
    if (err != GNUTLS_E_SUCCESS) {
        std::cerr << "Could not export certificate - " << gnutls_strerror(err) << std::endl;
        return {};
    }
    buffer.resize(buf_sz);
    return buffer;
}

gnutls_pubkey_t
SecureDht::pubkeyFromCert(const Blob& cert)
{
    gnutls_pubkey_t pk;
    gnutls_pubkey_init(&pk);
    const gnutls_datum_t m {(uint8_t*)cert.data(), (unsigned)256};
    const gnutls_datum_t e {(uint8_t*)cert.data()+256, (unsigned)3};
    if (gnutls_pubkey_import_rsa_raw(pk, &m, &e) != GNUTLS_E_SUCCESS)
        return nullptr;
    return pk;
}

const Blob*
SecureDht::getPublicKey(const InfoHash& node) const {
    if (node == getId())
        return &publicKey;
    for (auto& pk : nodesPublicKeys)
        if (pk.first == node)
            return &pk.second;
    return nullptr;
}

bool
SecureDht::registerPublicKey(const InfoHash& node, const Blob& publicKey) {
    InfoHash h = InfoHash::get(publicKey);
    if (node != h) {
        // public key does not match hash
        std::cerr << "Public key " << h << " stored on node " << node << std::endl;
        return false;
    }
    if (h == getId()) {
        return true;
    }
    for (auto& pk : nodesPublicKeys) {
        if (pk.first == node) {
            return true;
        }
    }
    std::cout << "Registering public key for " << h << std::endl;
    nodesPublicKeys.emplace_back(h, publicKey);
    return true;
}

void
SecureDht::findPublicKey(const InfoHash& node, std::function<void(const Blob*)> cb) {
    const Blob* b = getPublicKey(node);
    if (b) {
        std::cout << "Using public key from cache for " << node << std::endl;
        cb(b);
        return;
    }
    auto found = std::make_shared<bool>(false);
    Dht::get(node, [cb,node,found,this](const std::vector<std::shared_ptr<Value>>& vals){
        if (*found)
            return false;
        for (const auto& v : vals) {
            if (v->type != Value::Type::PublicKey)
                continue;
            if (registerPublicKey(node, v->data)/*vid == node*/) {
                *found = true;
                std::cout << "Found public key for " << node << std::endl;
                cb(&v->data);
                return false;
            }
        }
        return true;
    }, [cb,found](bool ok) {
        if (!*found)
            cb( nullptr);
    });

}

void
SecureDht::get (const InfoHash& id, GetCallback cb, DoneCallback donecb)
{
    auto done = std::make_shared<bool>(false);
    auto check_sig = std::make_shared<std::vector<std::shared_ptr<Value>>>();
    Dht::get(id,
    [=](const std::vector<std::shared_ptr<Value>>& values) {
        for (const auto& v : values) {
            if (v->type == Value::Type::PublicKey)
                registerPublicKey(id, v->data);
        }
        if (*done) return false;
        std::vector<std::shared_ptr<Value>> tmpvals {};
        for (const auto& v : values) {
            if (v->isSigned()) {
                check_sig->push_back(v);
                verifySigned(v->data, [=](const SignedData& d, bool ok) {
                    auto cs_ptr = check_sig.get();
                    auto it = std::find(cs_ptr->begin(), cs_ptr->end(), v);
                    if (it != cs_ptr->end()){
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
            } else if (v->isEncrypted()) {
                tmpvals.push_back(std::make_shared<Value>(
                    v->type,
                    decryptData(v->data),
                    Value::CryptoFlags {true, false}
                ));
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
    });
}

SecureDht::SignedData
SecureDht::signData(const Blob& data) const
{
    gnutls_datum_t sig;
    const gnutls_datum_t dat {(unsigned char*)data.data(), (unsigned)data.size()};
    if (gnutls_privkey_sign_data(key, GNUTLS_DIG_SHA512, 0, &dat, &sig) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't sign data !");
    auto sd = SignedData {
        data,
        getId(),
        Blob {sig.data, sig.data+sig.size}
    };
    gnutls_free(sig.data);
    return sd;
}

void
SecureDht::putSigned(const InfoHash& hash, const Value& val, DoneCallback callback) {
    Value nv {
        val.type,
        signData(val.data),
        Value::CryptoFlags {true, false}
    };
    put(hash, nv, callback);
}

bool
SecureDht::verifySignedData(const SignedData& data, const Blob& pkb) {
    if (InfoHash::get(pkb) != data.signer)
        return false;
    gnutls_pubkey_t pk = pubkeyFromCert(pkb);
    if (!pk)
        return false;
    const gnutls_datum_t sig {(uint8_t*)data.signature.data(), (unsigned)256};
    const gnutls_datum_t dat {(uint8_t*)data.data.data(), (unsigned)data.data.size()};
    int rc = gnutls_pubkey_verify_data2(pk, GNUTLS_SIGN_RSA_SHA512, 0, &dat, &sig);
    gnutls_pubkey_deinit(pk);
    return rc >= 0;
}

Blob
SecureDht::encryptData(const Blob& data, const Blob& key)
{
    gnutls_pubkey_t pk = pubkeyFromCert(key);
    const gnutls_datum_t dat {(uint8_t*)data.data(), (unsigned)data.size()};
    gnutls_datum_t encrypted;
    if (gnutls_pubkey_encrypt_data(pk, 0, &dat, &encrypted) != GNUTLS_E_SUCCESS) {
        gnutls_pubkey_deinit(pk);
        throw DhtException("Can't encrypt data using public key !");
    }
    Blob ret {encrypted.data, encrypted.data+encrypted.size};
    gnutls_free(encrypted.data);
    gnutls_pubkey_deinit(pk);
    return ret;
}

Blob
SecureDht::decryptData(const Blob& cipher)
{
    const gnutls_datum_t dat {(uint8_t*)cipher.data(), (unsigned)cipher.size()};
    gnutls_datum_t out;
    if (gnutls_privkey_decrypt_data(key, 0, &dat, &out) != GNUTLS_E_SUCCESS)
        throw DhtException("Can't decrypt data using private key !");
    Blob ret {out.data, out.data+out.size};
    gnutls_free(out.data);
    return ret;
}

void
SecureDht::putEncrypted(const InfoHash& hash, const InfoHash& to, const Value& val, DoneCallback callback) {
    findPublicKey(to, [=](const Blob* key) {
        if(!key) {
            if (callback)
                callback(false);
            return;
        }
        Value nv {
            val.type,
            encryptData(val.data, *key),
            Value::CryptoFlags {val.isSigned(), true}
        };
        put(hash, nv, callback);
    });
}

}
