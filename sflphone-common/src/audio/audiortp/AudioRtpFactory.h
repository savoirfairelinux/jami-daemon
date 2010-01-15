/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
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

#ifndef __SFL_AUDIO_RTP_FACTORY_H__
#define __SFL_AUDIO_RTP_FACTORY_H__

#include <stdexcept>
#include <cc++/thread.h>

#include "sip/SdesNegotiator.h"

class SdesNegotiator;
class SIPCall;

namespace sfl {
    class AudioZrtpSession;
    class AudioSrtpSession;
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

        /**
         * Lazy instantiation method. Create a new RTP session of a given 
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
        void start();
     
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
        inline RtpMethod getAudioRtpType(RtpMethod type) { return _rtpSessionType = type; }
 
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
        
        private:
           void * _rtpSession;
           RtpMethod _rtpSessionType;
           ost::Mutex _audioRtpThreadMutex;
    };
}
#endif // __AUDIO_RTP_FACTORY_H__
