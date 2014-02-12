/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
    BIO_read(bmem, buffer.data(), length);

    BIO_free_all(bmem);

    return buffer;
}

// Fills the array dest with length random bytes
void bufferFillMasterKey(std::vector<uint8>& dest)
{
    DEBUG("Init local master key");

    // Allocate memory for key
    std::vector<unsigned char> random_key(dest.size());

    // Generate ryptographically strong pseudo-random bytes
    if (RAND_bytes(random_key.data(), dest.size()) != 1)
        DEBUG("Error occured while generating cryptographically strong pseudo-random key");

    std::copy(random_key.begin(), random_key.end(), dest.begin());
}

// Fills the array dest with length random bytes
void bufferFillMasterSalt(std::vector<uint8>& dest)
{
    DEBUG("Init local master key");

    // Allocate memory for key
    std::vector<unsigned char> random_key(dest.size());

    // Generate ryptographically strong pseudo-random bytes
    if (RAND_bytes(random_key.data(), dest.size()) != 1)
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
    localMasterKey_(MAX_MASTER_KEY_LENGTH),
    localMasterSalt_(MAX_MASTER_SALT_LENGTH),
    remoteMasterKey_(MAX_MASTER_KEY_LENGTH),
    remoteMasterSalt_(MAX_MASTER_SALT_LENGTH),
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

void AudioSrtpSession::setRemoteCryptoInfo(const sfl::SdesNegotiator& nego)
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

        if (remoteCryptoCtx_) {
            setInQueueCryptoContext(remoteCryptoCtx_);
        }

        remoteOfferIsSet_ = true;
    }
}

namespace {
static const size_t BITS_PER_BYTE = 8;
}

void AudioSrtpSession::initializeLocalMasterKey()
{
    localMasterKey_.resize(sfl::CryptoSuites[localCryptoSuite_].masterKeyLength / BITS_PER_BYTE);
    bufferFillMasterKey(localMasterKey_);
}

void AudioSrtpSession::initializeLocalMasterSalt()
{
    localMasterSalt_.resize(sfl::CryptoSuites[localCryptoSuite_].masterSaltLength / BITS_PER_BYTE);
    bufferFillMasterSalt(localMasterSalt_);
}

std::string AudioSrtpSession::getBase64ConcatenatedKeys()
{
    DEBUG("Get base64 concatenated keys");

    // compute concatenated master and salt length
    std::vector<uint8> concatKeys;
    concatKeys.reserve(localMasterKey_.size() + localMasterSalt_.size());

    // concatenate keys
    concatKeys.insert(concatKeys.end(), localMasterKey_.begin(), localMasterKey_.end());
    concatKeys.insert(concatKeys.end(), localMasterSalt_.begin(), localMasterSalt_.end());

    // encode concatenated keys in base64
    return encodeBase64(concatKeys.data(), concatKeys.size());
}

void AudioSrtpSession::unBase64ConcatenatedKeys(std::string base64keys)
{
    remoteMasterKey_.resize(sfl::CryptoSuites[remoteCryptoSuite_].masterKeyLength / BITS_PER_BYTE);
    remoteMasterSalt_.resize(sfl::CryptoSuites[remoteCryptoSuite_].masterSaltLength / BITS_PER_BYTE);

    // pointer to binary data
    char *dataptr = (char*) base64keys.data();

    // decode concatenated binary keys
    std::vector<char> output(decodeBase64((unsigned char*) dataptr, strlen(dataptr)));

    // copy master and slt respectively
    const std::vector<char>::iterator key_end = output.begin() + remoteMasterKey_.size();
    std::copy(output.begin(), key_end, remoteMasterKey_.begin());
    std::copy(key_end, output.end(), remoteMasterSalt_.begin());
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
            remoteMasterKey_.size(),
            remoteMasterSalt_.data(),
            remoteMasterSalt_.size(),
            crypto.encryptionKeyLength / BITS_PER_BYTE,
            crypto.srtpAuthKeyLength / BITS_PER_BYTE,
            crypto.masterSaltLength / BITS_PER_BYTE,
            crypto.srtpAuthTagLength / BITS_PER_BYTE);

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
            localMasterKey_.size(),
            localMasterSalt_.data(),
            localMasterSalt_.size(),
            crypto.encryptionKeyLength / BITS_PER_BYTE,
            crypto.srtpAuthKeyLength / BITS_PER_BYTE,
            crypto.masterSaltLength / BITS_PER_BYTE,
            crypto.srtpAuthTagLength / BITS_PER_BYTE);
}

CachedAudioRtpState *
AudioSrtpSession::saveState() const
{
    return new CachedAudioRtpState(localMasterKey_, localMasterSalt_);
}

CachedAudioRtpState::CachedAudioRtpState(const std::vector<uint8> &key, const std::vector<uint8> &salt) : key_(key), salt_(salt)
{}

void
AudioSrtpSession::restoreState(const CachedAudioRtpState &state)
{
    localMasterKey_ = state.key_;
    localMasterSalt_ = state.salt_;
}

}
