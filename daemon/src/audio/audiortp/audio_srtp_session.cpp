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
#include "base64.h"

#include <algorithm>
#include <random>

#include <cstdio>
#include <cstring>
#include <cerrno>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define THROW_ERROR(EXCEPTION_CLASS, M) throw EXCEPTION_CLASS(__FILE__ ":" TOSTRING(__LINE__) ":" M)

namespace sfl {

static std::string
encodeBase64(unsigned char *input, int length)
{
    size_t output_length;
    uint8_t *encoded_str = sfl_base64_encode(input, length, &output_length);
    if (!encoded_str)
        THROW_ERROR(AudioSrtpException, "Out of memory for base64 operation");
    std::string output((const char *)encoded_str, output_length);
    free(encoded_str);
    return output;
}

static std::string
decodeBase64(unsigned char *input, int length)
{
    size_t output_length;
    uint8_t *decoded_str = sfl_base64_decode(input, length, &output_length);
    if (!decoded_str)
        THROW_ERROR(AudioSrtpException, "Out of memory for base64 operation");
    std::string output((const char *)decoded_str, output_length);
    free(decoded_str);
    return output;
}

// Fills the array dest with length random bytes
static void
bufferFillMasterKey(std::vector<uint8>& dest)
{
    DEBUG("Init local master key");

    // Prepare pseudo random generationusing Mersenne Twister
    std::mt19937 eng;
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // Allocate memory for key
    std::vector<unsigned char> random_key(dest.size());

    // Fill the key
    for (size_t i = 0; i < dest.size(); i++)
        random_key[i] = dist(eng);

    std::copy(random_key.begin(), random_key.end(), dest.begin());
}

// Fills the array dest with length random bytes
static void
bufferFillMasterSalt(std::vector<uint8>& dest)
{
    DEBUG("Init local master salt");

    // Prepare pseudo random generation using Mersenne Twister
    std::mt19937 eng;
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // Allocate memory for key
    std::vector<unsigned char> random_key(dest.size());

    // Fill the key
    for (size_t i = 0; i < dest.size(); i++)
        random_key[i] = dist(eng);

    std::copy(random_key.begin(), random_key.end(), dest.begin());
}

static const unsigned MAX_MASTER_KEY_LENGTH = 16;
static const unsigned MAX_MASTER_SALT_LENGTH = 14;

AudioSrtpSession::AudioSrtpSession(SIPCall &call) :
    AudioSymmetricRtpSession(call),
    remoteCryptoCtx_(),
    localCryptoCtx_(),
    localCryptoSuite_(0),
    remoteCryptoSuite_(0),
    localMasterKey_(MAX_MASTER_KEY_LENGTH),
    localMasterSalt_(MAX_MASTER_SALT_LENGTH),
    remoteMasterKey_(MAX_MASTER_KEY_LENGTH),
    remoteMasterSalt_(MAX_MASTER_SALT_LENGTH),
    remoteOfferIsSet_(false)
{}

AudioSrtpSession::~AudioSrtpSession()
{
    if (remoteCryptoCtx_)
        removeInQueueCryptoContext(remoteCryptoCtx_);
    if (localCryptoCtx_)
        removeOutQueueCryptoContext(localCryptoCtx_);
}

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

static const size_t BITS_PER_BYTE = 8;

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

    // decode concatenated binary keys
    std::string output(decodeBase64((uint8_t *)base64keys.data(), base64keys.size()));

    // copy master and slt respectively
    const std::string::iterator key_end = output.begin() + remoteMasterKey_.size();

    if (key_end > output.end() or
        (size_t) std::distance(key_end, output.end()) > remoteMasterSalt_.size())
        THROW_ERROR(AudioSrtpException, "Out of bounds copy");

    std::copy(output.begin(), key_end, remoteMasterKey_.begin());
    std::copy(key_end, output.end(), remoteMasterSalt_.begin());

}

void AudioSrtpSession::initializeRemoteCryptoContext()
{
    DEBUG("Initialize remote crypto context");

    const CryptoSuiteDefinition &crypto = sfl::CryptoSuites[remoteCryptoSuite_];

    if (remoteCryptoCtx_)
        removeInQueueCryptoContext(remoteCryptoCtx_);

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

    if (localCryptoCtx_)
        removeOutQueueCryptoContext(localCryptoCtx_);

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
