/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef __SFL_AUDIO_RTP_FACTORY_H__
#define __SFL_AUDIO_RTP_FACTORY_H__

#include <stdexcept>
#include <cc++/thread.h>

#include "sip/SdesNegotiator.h"
#include "audio/codecs/audiocodec.h"

class SdesNegotiator;
class SIPCall;

namespace sfl {
    class AudioZrtpSession;
    class AudioSrtpSession;
    class AudioSymmetricRtpSession;
}

namespace sfl {

    class AudioZrtpSession;
    class AudioSrtpSession;

    // Possible kind of rtp session
    typedef enum RtpMethod {
        Symmetric,
        Zrtp,
        Sdes
    } RtpMethod;


    class UnsupportedRtpSessionType : public std::logic_error {
        public:
        UnsupportedRtpSessionType(const std::string& msg = "") : std::logic_error(msg) {}
    };
    
    class AudioRtpFactoryException : public std::logic_error {
        public:
        AudioRtpFactoryException(const std::string& msg = "") : std::logic_error(msg) {}
    };

    class AudioRtpFactory {
        public:
        AudioRtpFactory();
        AudioRtpFactory(SIPCall * ca);
        ~AudioRtpFactory();

	void initAudioRtpConfig(SIPCall *ca);

	/**
	 * 	Lazy instantiation method. Create a new RTP session of a given
	 * type according to the content of the configuration file.
	 * @param ca A pointer on a SIP call
	 * @return A new AudioRtpSession object
	 */
	void initAudioRtpSession(SIPCall *ca);

	/**
	 * Start the audio rtp thread of the type specified in the configuration
	 * file. initAudioRtpSession must have been called prior to that.
	 * @param None
	 */
	void start(AudioCodec*);

	/**
	 * Stop the audio rtp thread of the type specified in the configuration
	 * file. initAudioRtpSession must have been called prior to that.
	 * @param None
	 */
	void stop();

	/**
	 * Update current RTP destination address with one stored in call
	 * @param None
	 */
	void updateDestinationIpAddress (void);
          
	/**
	 * @param None
	 * @return The internal audio rtp thread of the type specified in the configuration
	 * file. initAudioRtpSession must have been called prior to that.
	 */
	inline void * getAudioRtpSession(void) { return _rtpSession; }

	AudioSymmetricRtpSession * getAudioSymetricRtpSession();

	/** 
	 * @param None
	 * @return The internal audio rtp session type
	 *         Symmetric = 0
	 *         Zrtp = 1
	 *         Sdes = 2
	 */
	inline RtpMethod getAudioRtpType(void) { return _rtpSessionType; }
	
	/**
	 * @param Set internal audio rtp session type (Symmetric, Zrtp, Sdes)
	 */
	inline void setAudioRtpType(RtpMethod type) { _rtpSessionType = type; }

	/**
	 * Manually set the srtpEnable option (usefull for RTP fallback)
	 */
	void setSrtpEnabled(bool enable){ _srtpEnabled = enable; }

	/**
	 * Manually set the keyExchangeProtocol parameter (usefull for RTP fallback)
	 */
	void setKeyExchangeProtocol(int proto){ _keyExchangeProtocol = proto; }

	/**
	 * Manually set the setHelloHashEnabled parameter (usefull for RTP fallback)
	 */
	void setHelloHashEnabled(bool enable){ _helloHashEnabled = enable; }
 
	/**
	 * Get the current AudioZrtpSession. Throws an AudioRtpFactoryException
	 * if the current rtp thread is null, or if it's not of the correct type.
	 * @return The current AudioZrtpSession thread.
	 */
	sfl::AudioZrtpSession * getAudioZrtpSession();

	/**
	 * Set remote cryptographic info. Should be called after negotiation in SDP
	 * offer/answer session.
	 */
	void setRemoteCryptoInfo(sfl::SdesNegotiator& nego);

	/**
	 * Send DTMF over RTP (RFC2833). The timestamp and sequence number must be
	 * incremented as if it was microphone audio. This function change the payload type of the rtp session,
	 * send the appropriate DTMF digit using this payload, discard coresponding data from mainbuffer and get
	 * back the codec payload for further audio processing.
	 */
	void sendDtmfDigit(int digit);
        
private:
	void * _rtpSession;
	RtpMethod _rtpSessionType;
	ost::Mutex _audioRtpThreadMutex;

	// Field used when initializinga udio rtp session
	// May be set manually or from config using initAudioRtpConfig
	bool _srtpEnabled;

	// Field used when initializinga udio rtp session
	// May be set manually or from config using initAudioRtpConfig
	int _keyExchangeProtocol;

	// Field used when initializinga udio rtp session
	// May be set manually or from config using initAudioRtpConfig
	bool _helloHashEnabled;
};
}
#endif // __AUDIO_RTP_FACTORY_H__
