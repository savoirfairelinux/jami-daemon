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

#include <openssl/bio.h>
#include <openssl/evp.h>


#include <cstdio>
#include <cstring>
#include <cerrno>

static uint8 mk[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static uint8 ms[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d };


// static std::string crypto_suite = "AES_CM_128_HMAC_SHA1_32";
// static std::string application = "srtp";
// static std::string srtp_key = "inline:16/14/NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj/2^20/1:32";



namespace sfl
{

AudioSrtpSession::AudioSrtpSession (ManagerImpl * manager, SIPCall * sipcall) :
        ost::SymmetricRTPSession (ost::InetHostAddress (sipcall->getLocalIp().c_str()), sipcall->getLocalAudioPort()),
        AudioRtpSession<AudioSrtpSession> (manager, sipcall)
{
    _debug ("***************** AudioSrtpSession initialized *********************");
    initializeMasterKey();
    initializeMasterSalt();
    initializeInputCryptoContext();
    initializeOutputCryptoContext();

    outputCryptoCtx->deriveSrtpKeys(0);

    setInQueueCryptoContext(inputCryptoCtx);
    setOutQueueCryptoContext(outputCryptoCtx);
}

  /*
std::string AudioSrtpSession::getCryptoInfo() {


    return ;
}
  */

void AudioSrtpSession::initializeMasterKey(void)
{

    for(int i = 0; i < 16; i++)
        _masterKey[i] = mk[i];

    return;
}


void AudioSrtpSession::initializeMasterSalt(void)
{
    for(int i = 0; i < 16; i++)
        _masterSalt[i] = ms[i];

    return;

}

void AudioSrtpSession::initializeInputCryptoContext(void)
{

  // this one does not works
  // inputCryptoCtx = new ost::CryptoContext(IncomingDataQueue::getLocalSSRCNetwork(),
  inputCryptoCtx = new ost::CryptoContext(0x0,
					  0,                           // roc,
					  0L,                          // keydr,
					  SrtpEncryptionAESCM,         // encryption algo
					  SrtpAuthenticationSha1Hmac,  // authtication algo
					  _masterKey,                  // Master Key
					  128 / 8,                     // Master Key length
					  _masterSalt,                 // Master Salt
					  112 / 8,                     // Master Salt length
					  128 / 8,                     // encryption keyl
					  160 / 8,                     // authentication key len
					  112 / 8,                     // session salt len
					  80 / 8);                     // authentication tag len

    
}

void AudioSrtpSession::initializeOutputCryptoContext(void)
{

  // this one works
  // outputCryptoCtx = new ost::CryptoContext(OutgoingDataQueue::getLocalSSRC(),
    outputCryptoCtx = new ost::CryptoContext(OutgoingDataQueue::getLocalSSRC(),
					     0,                           // roc,
					     0L,                          // keydr,
					     SrtpEncryptionAESCM,         // encryption algo
					     SrtpAuthenticationSha1Hmac,  // authtication algo
					     _masterKey,                  // Master Key
					     128 / 8,                     // Master Key length
					     _masterSalt,                 // Master Salt
					     112 / 8,                     // Master Salt length
					     128 / 8,                     // encryption keyl
					     160 / 8,                     // authentication key len
					     112 / 8,                     // session salt len
					     80 / 8);                     // authentication tag len


}


char* AudioSrtpSession::encodeBase64(unsigned char *input, int length)
{
    BIO *b64, *bmem;

    char *buffer = (char *)malloc(length);
    memset(buffer, 0, length);

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf(input, length);
    bmem = BIO_push(bmem, b64);

    BIO_read(bmem, buffer, length);

    BIO_free_all(bmem);

    return buffer;
}

char* AudioSrtpSession::decodeBase64(unsigned char *input, int length)
{
    BIO *b64, *bmem;

    char *buffer = (char *)malloc(length);
    memset(buffer, 0, length);
  
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf(input, length);
    bmem = BIO_push(b64, bmem);

    BIO_read(bmem, buffer, length);

    BIO_free_all(bmem);

    return buffer;
}

}
