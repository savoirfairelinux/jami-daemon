/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#ifndef __SFL_AUDIO_RTP_SESSION_H__
#define __SFL_AUDIO_RTP_SESSION_H__

#include <iostream>
#include <exception>
#include <list>

#include "global.h"

#include "AudioRtpRecordHandler.h"
#include "sip/sipcall.h"
// #include "sip/sdp.h"
// #include "audio/audiolayer.h"
#include "audio/codecs/audiocodec.h"

#include "managerimpl.h"

#include <ccrtp/rtp.h>
#include <ccrtp/iqueue.h>
#include <cc++/numbers.h> // ost::Time

#include <fstream>
namespace sfl
{

// class AudioRtpSession : protected ost::Thread, public ost::TimerPort, public AudioRtpRecordHandler, public ost::TRTPSessionBase<ost::DualRTPUDPIPv4Channel,ost::DualRTPUDPIPv4Channel,ost::AVPQueue>
class AudioRtpSession : public ost::TimerPort, public AudioRtpRecordHandler, public ost::SymmetricRTPSession
{
    public:
        /**
        * Constructor
        * @param sipcall The pointer on the SIP call
        */
        AudioRtpSession (ManagerImpl * manager, SIPCall* sipcall);

        ~AudioRtpSession();

        virtual void final ();

        void terminateRtpSession();

        // Thread associated method
        // virtual void run ();

        virtual bool onRTPPacketRecv (ost::IncomingRTPPkt&);

        int startRtpThread (AudioCodec*);

        void stopRtpThread (void);

        /**
         * Used mostly when receiving a reinvite
         */
        void updateDestinationIpAddress (void);

        /**
        * Send DTMF over RTP (RFC2833). The timestamp and sequence number must be
        * incremented as if it was microphone audio. This function change the payload type of the rtp session,
        * send the appropriate DTMF digit using this payload, discard coresponding data from mainbuffer and get
        * back the codec payload for further audio processing.
        */
        void sendDtmfEvent (sfl::DtmfEvent *dtmf);

        void callInitialized (bool init) {
            _callInitialized = init;
        }

        /**
         * Update session audio codec dynamically
         */
        void updateSessionMedia (AudioCodec *);

        /**
         * Send encoded data to peer
         */
        void sendMicData();

    private:

        class AudioRtpThread : public ost::Thread, public ost::TimerPort
        {
            public:
                AudioRtpThread (AudioRtpSession *session);
                ~AudioRtpThread();

                void stopRtpThread (void) {
                    running = false;
                }

                virtual void run();

            private:
                AudioRtpSession *rtpSession;

                bool running;
        };

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
         * Receive data from peer
         */
        void receiveSpeakerData ();

        // This semaphore is not used
        // but is needed in order to avoid
        // ambiguous compiling problem.
        // It is set to 0, and since it is
        // optional in ost::thread, then
        // it amounts to the same as doing
        // start() with no semaphore at all.
        ost::Semaphore *_mainloopSemaphore;

        // Main destination address for this rtp session.
        // Stored in case or reINVITE, which may require to forget
        // this destination and update a new one.
        ost::InetHostAddress _remote_ip;


        // Main destination port for this rtp session.
        // Stored in case reINVITE, which may require to forget
        // this destination and update a new one
        unsigned short _remote_port;

        /**
         * Manager instance.
         */
        ManagerImpl * _manager;

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

        /**
         * Call initialized
         */
        bool _callInitialized;

    protected:

        SIPCall * _ca;

        bool _isStarted;

        AudioRtpThread *_rtpThread;

    public:

        friend class AudioRtpThread;
};

}
#endif // __AUDIO_RTP_SESSION_H__

