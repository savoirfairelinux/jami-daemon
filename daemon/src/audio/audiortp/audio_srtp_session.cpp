/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "audio_srtp_session.h"
#include "logger.h"
#include "array_size.h"

#include <algorithm>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>

#include <cstdio>
#include <cstring>
#include <cerrno>

namespace sfl {

namespace {
    std::string
    encodeBase64(unsigned char *input, int length)
    {
        // init decoder
        BIO *b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        // init internal buffer
        BIO *bmem = BIO_new(BIO_s_mem());

        // create decoder chain
        b64 = BIO_push(b64, bmem);

        BIO_write(b64, input, length);
        // BIO_flush (b64);

        // get pointer to data
        BUF_MEM *bptr = 0;
        BIO_get_mem_ptr(b64, &bptr);

        std::string output(bptr->data, bptr->length);

        BIO_free_all(bmem);

        return output;
    }

    std::vector<char> decodeBase64(unsigned char *input, int length)
    {
        BIO *b64, *bmem;

        // init decoder and read-only BIO buffer
        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        // init internal buffer
        bmem = BIO_new_mem_buf(input, length);

        // create encoder chain
        bmem = BIO_push(b64, bmem);

        std::vector<char> buffer(length, 0);
        BIO_read(bmem, &(*buffer.begin()), length);

        BIO_free_all(bmem);

        return buffer;
    }

    // Fills the array dest with length random bytes
    void bufferFillMasterKey(std::tr1::array<uint8, MAX_MASTER_KEY_LENGTH>& dest)
    {
        DEBUG("Init local master key");

        // Allocate memory for key
        std::vector<unsigned char> random_key(dest.size());

        // Generate ryptographically strong pseudo-random bytes
        if (RAND_bytes(&(*random_key.begin()), dest.size()) != 1)
            DEBUG("Error occured while generating cryptographically strong pseudo-random key");

        std::copy(random_key.begin(), random_key.end(), dest.begin());
    }

    // Fills the array dest with length random bytes
    void bufferFillMasterSalt(std::tr1::array<uint8, MAX_MASTER_SALT_LENGTH>& dest)
    {
        DEBUG("Init local master key");

        // Allocate memory for key
        std::vector<unsigned char> random_key(dest.size());

        // Generate ryptographically strong pseudo-random bytes
        if (RAND_bytes(&(*random_key.begin()), dest.size()) != 1)
            DEBUG("Error occured while generating cryptographically strong pseudo-random key");

        std::copy(random_key.begin(), random_key.end(), dest.begin());
    }
}



AudioSrtpSession::AudioSrtpSession(SIPCall &call) :
    AudioSymmetricRtpSession(call),
    remoteCryptoCtx_(0),
    localCryptoCtx_(0),
    localCryptoSuite_(0),
    remoteCryptoSuite_(0),
    localMasterKey_(),
    localMasterKeyLength_(0),
    localMasterSalt_(),
    localMasterSaltLength_(0),
    remoteMasterKey_(),
    remoteMasterKeyLength_(0),
    remoteMasterSalt_(),
    remoteMasterSaltLength_(0),
    remoteOfferIsSet_(false)
{}


void AudioSrtpSession::initLocalCryptoInfo()
{
    DEBUG("AudioSrtpSession: Set cryptographic info for this rtp session");

    // Initialize local Crypto context
    initializeLocalMasterKey();
    initializeLocalMasterSalt();
    initializeLocalCryptoContext();

    // Set local crypto context in ccrtp
    localCryptoCtx_->deriveSrtpKeys(0);

    setOutQueueCryptoContext(localCryptoCtx_);
}

void AudioSrtpSession::initLocalCryptoInfoOnOffhold()
{
    DEBUG("AudioSrtpSession: Set cryptographic info for this rtp session");

    // Initialize local Crypto context
    initializeLocalCryptoContext();

    // Set local crypto context in ccrtp
    localCryptoCtx_->deriveSrtpKeys(0);

    setOutQueueCryptoContext(localCryptoCtx_);
}

std::vector<std::string> AudioSrtpSession::getLocalCryptoInfo()
{

    DEBUG("Get Cryptographic info from this rtp session");

    std::vector<std::string> crypto_vector;

    // @TODO we should return a vector containing supported
    // cryptographic context tagged 1, 2, 3...
    std::string tag = "1";

    std::string crypto_suite(sfl::CryptoSuites[localCryptoSuite_].name);

    // srtp keys formated as the following  as the following
    // inline:keyParameters|keylifetime|MasterKeyIdentifier
    // inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj|2^20|1:32
    std::string srtp_keys = "inline:";
    srtp_keys += getBase64ConcatenatedKeys();
    // srtp_keys.append("|2^20|1:32");

    // generate crypto attribute
    std::string crypto_attr = tag.append(" ");
    crypto_attr += crypto_suite.append(" ");
    crypto_attr += srtp_keys;

    DEBUG("%s", crypto_attr.c_str());

    crypto_vector.push_back(crypto_attr);

    return crypto_vector;
}

void AudioSrtpSession::setRemoteCryptoInfo(sfl::SdesNegotiator& nego)
{
    if (not remoteOfferIsSet_) {
        // Use second crypto suite if key length is 32 bit, default is 80;
        if (nego.getAuthTagLength() == "32") {
            localCryptoSuite_ = 1;
            remoteCryptoSuite_ = 1;
        }

        // decode keys
        unBase64ConcatenatedKeys(nego.getKeyInfo());

        // init crypto content in Srtp session
        initializeRemoteCryptoContext();
        if(remoteCryptoCtx_) {
            setInQueueCryptoContext(remoteCryptoCtx_);
        }

        remoteOfferIsSet_ = true;
    }
}

void AudioSrtpSession::initializeLocalMasterKey()
{
    DEBUG("Init local master key");
    // @TODO key may have different length depending on cipher suite
    localMasterKeyLength_ = sfl::CryptoSuites[localCryptoSuite_].masterKeyLength / 8;
    bufferFillMasterKey(localMasterKey_);
}

void AudioSrtpSession::initializeLocalMasterSalt()
{
    // @TODO key may have different length depending on cipher suite
    localMasterSaltLength_ = sfl::CryptoSuites[localCryptoSuite_].masterSaltLength / 8;
    bufferFillMasterSalt(localMasterSalt_);
}

std::string AudioSrtpSession::getBase64ConcatenatedKeys()
{
    DEBUG("Get base64 concatenated keys");

    // compute concatenated master and salt length
    int concatLength = localMasterKeyLength_ + localMasterSaltLength_;

    uint8 concatKeys[concatLength];

    // concatenate keys
    memcpy((void*) concatKeys, (void*) localMasterKey_.data(), localMasterKeyLength_);
    memcpy((void*)(concatKeys + localMasterKeyLength_), (void*) localMasterSalt_.data(), localMasterSaltLength_);

    // encode concatenated keys in base64
    return encodeBase64((unsigned char*) concatKeys, concatLength);
}

void AudioSrtpSession::unBase64ConcatenatedKeys(std::string base64keys)
{
    remoteMasterKeyLength_ = sfl::CryptoSuites[remoteCryptoSuite_].masterKeyLength / 8;
    remoteMasterSaltLength_ = sfl::CryptoSuites[remoteCryptoSuite_].masterSaltLength / 8;

    // pointer to binary data
    char *dataptr = (char*) base64keys.data();

    // decode concatenated binary keys
    std::vector<char> output(decodeBase64((unsigned char*) dataptr, strlen(dataptr)));

    // copy master and slt respectively
    // std::copy(output.begin()
    memcpy((void*) remoteMasterKey_.data(), &(*output.begin()), remoteMasterKeyLength_);
    memcpy((void*) remoteMasterSalt_.data(), &(*output.begin()) + remoteMasterKeyLength_, remoteMasterSaltLength_);
}

void AudioSrtpSession::initializeRemoteCryptoContext()
{
    DEBUG("Initialize remote crypto context");

    const CryptoSuiteDefinition &crypto = sfl::CryptoSuites[remoteCryptoSuite_];

    remoteCryptoCtx_ = new ost::CryptoContext(0x0,
                                              0,    // roc,
                                              0L,   // keydr,
                                              SrtpEncryptionAESCM,
                                              SrtpAuthenticationSha1Hmac,
                                              remoteMasterKey_.data(),
                                              remoteMasterKeyLength_,
                                              remoteMasterSalt_.data(),
                                              remoteMasterSaltLength_,
                                              crypto.encryptionKeyLength / 8,
                                              crypto.srtpAuthKeyLength / 8,
                                              crypto.masterSaltLength / 8,
                                              crypto.srtpAuthTagLength / 8);

}

void AudioSrtpSession::initializeLocalCryptoContext()
{
    DEBUG("Initialize local crypto context");

    const CryptoSuiteDefinition &crypto = sfl::CryptoSuites[localCryptoSuite_];

    localCryptoCtx_ = new ost::CryptoContext(OutgoingDataQueue::getLocalSSRC(),
                                             0,     // roc,
                                             0L,    // keydr,
                                             SrtpEncryptionAESCM,
                                             SrtpAuthenticationSha1Hmac,
                                             localMasterKey_.data(),
                                             localMasterKeyLength_,
                                             localMasterSalt_.data(),
                                             localMasterSaltLength_,
                                             crypto.encryptionKeyLength / 8,
                                             crypto.srtpAuthKeyLength / 8,
                                             crypto.masterSaltLength / 8,
                                             crypto.srtpAuthTagLength / 8);
}

void
AudioSrtpSession::setLocalMasterKey(std::tr1::array<uint8, MAX_MASTER_KEY_LENGTH>& key)
{
    std::copy(key.begin(), key.end(), localMasterKey_.begin());
    localMasterKeyLength_ = localMasterKey_.size();
}

void
AudioSrtpSession::getLocalMasterKey(std::tr1::array<uint8, MAX_MASTER_KEY_LENGTH>& key)
{
    std::copy(localMasterKey_.begin(), localMasterKey_.end(), key.begin());
}

void
AudioSrtpSession::setLocalMasterSalt(std::tr1::array<uint8, MAX_MASTER_SALT_LENGTH>& salt)
{
    std::copy(salt.begin(), salt.end(), localMasterSalt_.begin());
    localMasterSaltLength_ = localMasterSalt_.size();
}

void
AudioSrtpSession::getLocalMasterSalt(std::tr1::array<uint8, MAX_MASTER_SALT_LENGTH>& salt)
{
    std::copy(localMasterSalt_.begin(), localMasterSalt_.end(), salt.begin());
}

void
AudioSrtpSession::setRemoteMasterKey(std::tr1::array<uint8, MAX_MASTER_KEY_LENGTH>& key)
{
    std::copy(key.begin(), key.end(), remoteMasterKey_.begin());
    remoteMasterKeyLength_ = remoteMasterKey_.size();
}

void
AudioSrtpSession::getRemoteMasterKey(std::tr1::array<uint8, MAX_MASTER_KEY_LENGTH>& key)
{
    std::copy(remoteMasterKey_.begin(), remoteMasterKey_.end(), key.begin());
}

void
AudioSrtpSession::setRemoteMasterSalt(std::tr1::array<uint8, MAX_MASTER_SALT_LENGTH>& salt)
{
    std::copy(salt.begin(), salt.end(), remoteMasterSalt_.begin());
    remoteMasterSaltLength_ = remoteMasterSalt_.size();
}

void
AudioSrtpSession::getRemoteMasterSalt(std::tr1::array<uint8, MAX_MASTER_SALT_LENGTH>& salt)
{
    std::copy(remoteMasterSalt_.begin(), remoteMasterSalt_.end(), salt.begin());
}

}
