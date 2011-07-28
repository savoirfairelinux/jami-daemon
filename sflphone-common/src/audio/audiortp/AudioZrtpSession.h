/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#ifndef __SFL_AUDIO_ZRTP_SESSION_H__
#define __SFL_AUDIO_ZRTP_SESSION_H__

#include <cstddef>
#include <stdexcept>

using std::ptrdiff_t;
#include <ccrtp/rtp.h>
#include <libzrtpcpp/zrtpccrtp.h>
#include <libzrtpcpp/ZrtpQueue.h>
#include <libzrtpcpp/ZrtpUserCallback.h>

#include "AudioRtpRecordHandler.h"
#include <cc++/numbers.h> // OST::Time

class SIPCall;

namespace sfl
{

class ZrtpZidException: public std::runtime_error
{
    public:
        ZrtpZidException (const std::string& str="") :
            std::runtime_error("ZRTP ZID initialization failed." + str) {}
};

// class AudioZrtpSession : public ost::TimerPort, public ost::SymmetricZRTPSession, public AudioRtpRecordHandler
class AudioZrtpSession : protected ost::Thread, public AudioRtpRecordHandler, public ost::TRTPSessionBase<ost::SymmetricRTPChannel, ost::SymmetricRTPChannel, ost::ZrtpQueue>
{
    public:
        AudioZrtpSession (SIPCall * sipcall, const std::string& zidFilename);

        ~AudioZrtpSession();

        virtual void final();

        // Thread associated method
        virtual void run ();

        virtual bool onRTPPacketRecv (ost::IncomingRTPPkt&);

        int startRtpThread (AudioCodec *);

        void stopRtpThread (void);

        /**
         * Used mostly when receiving a reinvite
         */
        void updateDestinationIpAddress (void);

        /**
         * Update audio codec dynamically
         */
        void updateSessionMedia (AudioCodec *);

        /**
         * Send DTMF over RTP (RFC2833). The timestamp and sequence number must be
         * incremented as if it was microphone audio. This function change the payload type of the rtp session,
         * send the appropriate DTMF digit using this payload, discard coresponding data from mainbuffer and get
         * back the codec payload for further audio processing.
         */
        void sendDtmfEvent (sfl::DtmfEvent *dtmf);

    private:

        void initializeZid (void);

        /**
         * Set RTP Sockets send/receive timeouts
         */
        void setSessionTimeouts (void);

        /**
         * Set the audio codec for this RTP session
         */
        void setSessionMedia (AudioCodec*);

        /**
         * Retreive destination address for this session. Stored in CALL
         */
        void setDestinationIpAddress (void);

        /**
         * Send encoded data to peer
         */
        void sendMicData();

        /**
         * Receive data from peer
         */
        void receiveSpeakerData ();

        std::string _zidFilename;

        // This semaphore is not used
        // but is needed in order to avoid
        // ambiguous compiling problem.
        // It is set to 0, and since it is
        // optional in ost::thread, then
        // it amounts to the same as doing
        // start() with no semaphore at all.
        ost::Semaphore * _mainloopSemaphore;

        // Main destination address for this rtp session.
        // Stored in case or reINVITE, which may require to forget
        // this destination and update a new one.
        ost::InetHostAddress _remote_ip;


        // Main destination port for this rtp session.
        // Stored in case reINVITE, which may require to forget
        // this destination and update a new one
        unsigned short _remote_port;

        /**
         * Timestamp for this session
         */
        int _timestamp;

        /**
         * Timestamp incrementation value based on codec period length (framesize)
         * except for G722 which require a 8 kHz incrementation.
         */
        int _timestampIncrement;

        /**
         * Timestamp reset freqeuncy specified in number of packet sent
         */
        short _timestampCount;

        SIPCall * _ca;

        bool _isStarted;
};

}

#endif // __AUDIO_ZRTP_SESSION_H__
