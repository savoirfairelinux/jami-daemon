/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#ifndef __SFL_AUDIO_SRTP_SESSION_H__
#define __SFL_AUDIO_SRTP_SESSION_H__

#include "AudioRtpSession.h"
#include "sip/SdesNegotiator.h"

#include <ccrtp/CryptoContext.h>
#include <vector>

class SdesNegotiator;
class ManagerImpl;
class SIPCall;

/* 
   Table from RFC 4568 6.2. Crypto-Suites, which define key parameters for supported 
   cipher suite

   +---------------------+-------------+--------------+---------------+
   |                     |AES_CM_128_  | AES_CM_128_  | F8_128_       |
   |                     |HMAC_SHA1_80 | HMAC_SHA1_32 |  HMAC_SHA1_80 |
   +---------------------+-------------+--------------+---------------+
   | Master key length   |   128 bits  |   128 bits   |   128 bits    |
   | Master salt length  |   112 bits  |   112 bits   |   112 bits    |
   | SRTP lifetime       | 2^48 packets| 2^48 packets | 2^48 packets  |
   | SRTCP lifetime      | 2^31 packets| 2^31 packets | 2^31 packets  |
   | Cipher              | AES Counter | AES Counter  | AES F8 Mode   |
   |                     | Mode        | Mode         |               |
   | Encryption key      |   128 bits  |   128 bits   |   128 bits    |
   | MAC                 |  HMAC-SHA1  |  HMAC-SHA1   |  HMAC-SHA1    |
   | SRTP auth. tag      |    80 bits  |    32 bits   |    80 bits    |
   | SRTCP auth. tag     |    80 bits  |    80 bits   |    80 bits    |
   | SRTP auth. key len. |   160 bits  |   160 bits   |   160 bits    |
   | SRTCP auth. key len.|   160 bits  |   160 bits   |   160 bits    |
   +---------------------+-------------+--------------+---------------+
*/


namespace sfl {

    class SrtpException: public std::exception
    {
        virtual const char* what() const throw()
        {
        return "ZRTP ZID initialization failed.";
        }
    };

    class AudioSrtpSession : public ost::SymmetricRTPSession, public AudioRtpSession<AudioSrtpSession> 
    {
        public:

            AudioSrtpSession(ManagerImpl * manager, SIPCall * sipcall);

            std::vector<std::string> getLocalCryptoInfo(void);

            void setRemoteCryptoInfo(sfl::SdesNegotiator& nego);

        private:

            void initializeLocalMasterKey(void);

            void initializeLocalMasterSalt(void);

            void initializeRemoteCryptoContext(void);

            void initializeLocalCryptoContext(void);

            std::string getBase64ConcatenatedKeys();

            void unBase64ConcatenatedKeys(std::string base64keys);

            char* encodeBase64(unsigned char *input, int length);

            char* decodeBase64(unsigned char *input, int length, int *length_out);

            /** Default local crypto suite is AES_CM_128_HMAC_SHA1_80*/
            int _localCryptoSuite;

            /** Remote crypto suite is initialized at AES_CM_128_HMAC_SHA1_80*/
            int _remoteCryptoSuite;

            uint8 _localMasterKey[16];

            /** local master key length in byte */
            int _localMasterKeyLength;

            uint8 _localMasterSalt[14];

            /** local master salt length in byte */
            int _localMasterSaltLength;

            uint8 _remoteMasterKey[16];

            /** remote master key length in byte */
            int _remoteMasterKeyLength;

            uint8 _remoteMasterSalt[14];

            /** remote master salt length in byte */
            int _remoteMasterSaltLength;

            ost::CryptoContext* _remoteCryptoCtx;

            ost::CryptoContext* _localCryptoCtx;
    };
   
}

#endif // __AUDIO_SRTP_SESSION_H__
