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
#ifndef AUDIO_SRTP_SESSION_H_
#define AUDIO_SRTP_SESSION_H_

#include "audio_symmetric_rtp_session.h"
#include "sip/sdes_negotiator.h"
#include "noncopyable.h"

#include <ccrtp/CryptoContext.h>
#include <vector>

class SdesNegotiator;
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

class AudioSrtpSession : public AudioSymmetricRtpSession {
    public:

        /**
         * Constructor for this rtp session
         */
        AudioSrtpSession(SIPCall &call);

        /**
         * Used to get sdp crypto header to be included in sdp session. This
         * method must be called befor setRemoteCryptoInfo in case of an
         * outgoing call or after in case of an outgoing call.
         */
        std::vector<std::string> getLocalCryptoInfo();

        /**
         * Set remote crypto header from incoming sdp offer
         */
        void setRemoteCryptoInfo(sfl::SdesNegotiator &nego);

        /**
         * Init local crypto context for outgoing data
        * this method must be called before sending first Invite request
        * with SDP offer.
        */
        void initLocalCryptoInfo();

        /**
         * Restore the cryptographic context. most likely useful to restore
         * a call after hold action
         */
        void restoreCryptoContext(ost::CryptoContext *, ost::CryptoContext *);


        /** Remote srtp crypto context to be set into incoming data queue. */
        ost::CryptoContext* remoteCryptoCtx_;

        /** Local srtp crypto context to be set into outgoing data queue. */
        ost::CryptoContext* localCryptoCtx_;

    private:
        NON_COPYABLE(AudioSrtpSession);

        /**
         * Init local master key according to current crypto context
         * as defined in SdesNegotiator.h
         */
        void initializeLocalMasterKey();

        /**
         * Init local master salt according to current crypto context
         * as defined in SdesNegotiator.h
         */
        void initializeLocalMasterSalt();

        /**
         * Init remote crypto context in audio srtp session. This method
         * must be called after unBase64ConcatenatedKeys.
         */
        void initializeRemoteCryptoContext();

        /**
         * Init local crypto context in audio srtp session. Make sure remote
         * crypto context is set before calling this method for incoming calls.
         */
        void initializeLocalCryptoContext();

        /**
         * Used to generate local keys to be included in SDP offer/answer.
         */
        std::string getBase64ConcatenatedKeys();

        /**
         * Used to retreive keys from base64 serialization
         */
        void unBase64ConcatenatedKeys(std::string base64keys);

        /** Default local crypto suite is AES_CM_128_HMAC_SHA1_80*/
        int localCryptoSuite_;

        /** Remote crypto suite is initialized at AES_CM_128_HMAC_SHA1_80*/
        int remoteCryptoSuite_;

        uint8 localMasterKey_[16];

        /** local master key length in byte */
        int localMasterKeyLength_;

        uint8 localMasterSalt_[14];

        /** local master salt length in byte */
        int localMasterSaltLength_;

        uint8 remoteMasterKey_[16];

        /** remote master key length in byte */
        int remoteMasterKeyLength_;

        uint8 remoteMasterSalt_[14];

        /** remote master salt length in byte */
        int remoteMasterSaltLength_;

        /** Used to make sure remote crypto context not initialized wice. */
        bool remoteOfferIsSet_;
};
}

#endif // __AUDIO_SRTP_SESSION_H__
