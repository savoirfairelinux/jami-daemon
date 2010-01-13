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
#ifndef __SFL_AUDIO_SRTP_SESSION_H__
#define __SFL_AUDIO_SRTP_SESSION_H__

#include "AudioRtpSession.h"
#include "sip/SdesNegotiator.h"

#include <ccrtp/CryptoContext.h>

class SdesNegotiator;
class ManagerImpl;
class SIPCall;

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

	    std::string getLocalCryptoInfo(void);

	    void setRemoteCryptoInfo(sfl::SdesNegotiator& nego);

        private:

            void initializeLocalMasterKey(void);

	    void initializeLocalMasterSalt(void);

	    void initializeRemoteCryptoContext(void);

	    void initializeLocalCryptoContext(void);

	    std::string getBase64ConcatenatedKeys();

	    void unBase64ConcatenatedKeys(std::string base64keys);

	    char* encodeBase64(unsigned char *input, int length);

	    char* decodeBase64(unsigned char *input, int length);

            uint8 _localMasterKey[16];

	    int _localMasterKeyLength;

	    uint8 _localMasterSalt[14];

	    int _localMasterSaltLength;

	    uint8 _remoteMasterKey[16];

	    int _remoteMasterKeyLength;

	    uint8 _remoteMasterSalt[14];

	    int _remoteMasterSaltLength;

	    ost::CryptoContext* _remoteCryptoCtx;

	    ost::CryptoContext* _localCryptoCtx;
    };
   
}

#endif // __AUDIO_SRTP_SESSION_H__
