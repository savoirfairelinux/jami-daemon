/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */
#include "AudioSrtpSession.h"
#include "user_cfg.h"

#include "sip/sipcall.h"

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>


#include <cstdio>
#include <cstring>
#include <cerrno>

static uint8 mk[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static uint8 ms[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d };


namespace sfl
{

AudioSrtpSession::AudioSrtpSession (ManagerImpl * manager, SIPCall * sipcall) :
        ost::SymmetricRTPSession (ost::InetHostAddress (sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort()),
        AudioRtpSession<AudioSrtpSession> (manager, sipcall)
{

    // Initialize local Crypto context
    initializeLocalMasterKey();
    initializeLocalMasterSalt();
    initializeLocalCryptoContext();

    // Set local crypto context in ccrtp
    _localCryptoCtx->deriveSrtpKeys(0);

    setOutQueueCryptoContext(_localCryptoCtx);
}

 
std::string AudioSrtpSession::getLocalCryptoInfo() {

    _debug("Get Cryptographic info from this rtp session");

    // @TODO we should return a vector containing supported 
    // cryptographic context tagged 1, 2, 3...
    std::string tag = "1";

    std::string crypto_suite = "AES_CM_128_HMAC_SHA1_32";

    // srtp keys formated as the following  as the following
    // inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj|2^20|1:32
    std::string srtp_keys = "inline:";
    srtp_keys += getBase64ConcatenatedKeys();
    srtp_keys.append("|2^20|1:32");

    // generate crypto attribute
    std::string crypto = tag.append(" ");
    crypto += crypto_suite.append(" ");
    crypto += srtp_keys;

    _debug("%s", crypto.c_str());

    return crypto;
}


void AudioSrtpSession::setRemoteCryptoInfo(sfl::SdesNegotiator& nego) {

    _debug("Set remote Cryptographic info for Srtp session");

    // decode keys
    unBase64ConcatenatedKeys(nego.getKeyInfo());

    // init crypto content int Srtp session
    initializeRemoteCryptoContext();
    setInQueueCryptoContext(_remoteCryptoCtx);
}


void AudioSrtpSession::initializeLocalMasterKey(void)
{

    // @TODO key may have different length depending on cipher suite
    _localMasterKeyLength = 16;

    // Allocate memory for key
    unsigned char *random_key = new unsigned char[_localMasterKeyLength];

    // Generate ryptographically strong pseudo-random bytes
    int err;
    if((err = RAND_bytes(random_key, _localMasterKeyLength)) != 1)
        _debug("Error occured while generating cryptographically strong pseudo-random key");

    memcpy(_localMasterKey, random_key, _localMasterKeyLength);

    printf("Local Master: ");
    for(int i = 0; i < _localMasterKeyLength; i++){
        printf("%d", _localMasterKey[i]);
    }
    printf("\n");

    return;
}


void AudioSrtpSession::initializeLocalMasterSalt(void)
{

    // @TODO key may have different length depending on cipher suite 
    _localMasterSaltLength = 14;

    // Allocate memory for key
    unsigned char *random_key = new unsigned char[_localMasterSaltLength];

    // Generate ryptographically strong pseudo-random bytes
    int err;
    if((err = RAND_bytes(random_key, _localMasterSaltLength)) != 1)
        _debug("Error occured while generating cryptographically strong pseudo-random key");

    memcpy(_localMasterSalt, random_key, _localMasterSaltLength);

    return;

}


std::string AudioSrtpSession::getBase64ConcatenatedKeys()
{

    // compute concatenated master and salt length
    int concatLength = _localMasterKeyLength + _localMasterSaltLength;

    uint8 concatKeys[concatLength];

    // concatenate keys
    memcpy((void*)concatKeys, (void*)_localMasterKey, _localMasterKeyLength);
    memcpy((void*)(concatKeys + _localMasterKeyLength), (void*)_localMasterSalt, _localMasterSaltLength);

    // encode concatenated keys in base64
    char *output = encodeBase64((unsigned char*)concatKeys, concatLength);

    // init string containing encoded data
    std::string keys(output);

    free(output);

    return keys;
}


void AudioSrtpSession::unBase64ConcatenatedKeys(std::string base64keys)
{

    _remoteMasterKeyLength = 16;
    _remoteMasterSaltLength = 14;

    // length of decoded data data
    int length;

    // pointer to binary data
    char *dataptr = (char*)base64keys.data();

    // decode concatenated binary keys
    char *output = decodeBase64((unsigned char*)dataptr, strlen(dataptr), &length);

    // copy master and slt respectively
    memcpy((void*)_remoteMasterKey, (void*)output, _remoteMasterKeyLength);
    memcpy((void*)_remoteMasterSalt, (void*)(output + _remoteMasterKeyLength), _remoteMasterSaltLength);

    free(output);
}


void AudioSrtpSession::initializeRemoteCryptoContext(void)
{

    _remoteCryptoCtx = new ost::CryptoContext(0x0,
					     0,                           // roc,
					     0L,                          // keydr,
					     SrtpEncryptionAESCM,         // encryption algo
					     SrtpAuthenticationSha1Hmac,  // authtication algo
					     _remoteMasterKey,            // Master Key
					     128 / 8,                     // Master Key length
					     _remoteMasterSalt,           // Master Salt
					     112 / 8,                     // Master Salt length
					     128 / 8,                     // encryption keyl
					     160 / 8,                     // authentication key len
					     112 / 8,                     // session salt len
					     80 / 8);                     // authentication tag len

    
}

void AudioSrtpSession::initializeLocalCryptoContext(void)
{

    _localCryptoCtx = new ost::CryptoContext(OutgoingDataQueue::getLocalSSRC(),
					      0,                           // roc,
					      0L,                          // keydr,
					      SrtpEncryptionAESCM,         // encryption algo
					      SrtpAuthenticationSha1Hmac,  // authtication algo
					      _localMasterKey,             // Master Key
					      128 / 8,                     // Master Key length
					      _localMasterSalt,            // Master Salt
					      112 / 8,                     // Master Salt length
					      128 / 8,                     // encryption keyl
					      160 / 8,                     // authentication key len
					      112 / 8,                     // session salt len
					      80 / 8);                     // authentication tag len


}


char* AudioSrtpSession::encodeBase64(unsigned char *input, int length)
{
    BIO *b64, *bmem;
    BUF_MEM *bptr ;

    char *buffer = (char *)malloc(2*length);
    memset(buffer, 0, 2*length);

    // init decoder
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // init internal buffer
    bmem = BIO_new(BIO_s_mem());

    // create decoder chain
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input, length);
    BIO_flush(b64);

    // get pointer to data
    BIO_get_mem_ptr(b64, &bptr);

    // copy result in output buffer (-1 since we do not want the EOF character)
    strncpy(buffer, (char*)(bptr->data), bptr->length);

    BIO_free_all(bmem);

    return buffer;    
}

char* AudioSrtpSession::decodeBase64(unsigned char *input, int length, int *length_out)
{
    BIO *b64, *bmem;

    char *buffer = (char *)malloc(length);
    memset(buffer, 0, length);

    // init decoder and read-only BIO buffer
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // init internal buffer
    bmem = BIO_new_mem_buf(input, length);

    // create encoder chain
    bmem = BIO_push(b64, bmem);

    *length_out = BIO_read(bmem, buffer, length);

    BIO_free_all(bmem);

    return buffer;

}

}
