/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#ifndef AUDIO_RTP_SESSION_H_
#define AUDIO_RTP_SESSION_H_

#include "audio_rtp_stream.h"
#include "dtmf_event.h"
#include "ip_utils.h"
#include "noncopyable.h"
#include "logger.h"
#include "threadloop.h"

#include <ccrtp/rtp.h>
#include <ccrtp/formats.h>

#include <chrono>

class SIPCall;

namespace sfl {

class AudioCodec;
class AudioRtpSession;

class CachedAudioRtpState;

class AudioRtpSession {
    public:
        /**
        * Constructor
        * @param sipcall The pointer on the SIP call
        */
        AudioRtpSession(SIPCall &sipcall, ost::RTPDataQueue &queue);
        virtual ~AudioRtpSession();

        void updateSessionMedia(const std::vector<AudioCodec*> &audioCodecs);

        void startRtpThreads(const std::vector<AudioCodec*> &audioCodecs);

        void putDtmfEvent(char digit);
        bool hasDTMFPending() const {
            return not dtmfQueue_.empty();
        }

        int getDtmfPayloadType() const {
            return dtmfPayloadType_;
        }

        void setDtmfPayloadType(int pt) {
            dtmfPayloadType_ = pt;
        }

        int getTransportRate() const;

        /**
         * Used mostly when receiving a reinvite
         */
        void updateDestinationIpAddress();

        virtual int getIncrementForDTMF() const;

        virtual std::vector<long>
        getSocketDescriptors() const = 0;

        virtual CachedAudioRtpState * saveState() const;
        virtual void restoreState(const CachedAudioRtpState &state);

    private:
        bool isStarted_;

        void prepareRtpReceiveThread(const std::vector<AudioCodec*> &audioCodecs);
        /**
         * Set the audio codec for this RTP session
         */
        void setSessionMedia(const std::vector<AudioCodec*> &codec);

    protected:
        bool onRTPPacketRecv(ost::IncomingRTPPkt&);

        ost::RTPDataQueue &queue_;
        SIPCall &call_;

        /**
         * Timestamp for this session
         */
        int timestamp_;

        /**
         * Timestamp incrementation value based on codec period length (framesize)
         * except for G722 which require a 8 kHz incrementation.
         */
        int timestampIncrement_;

        AudioRtpStream rtpStream_;

    private:

        /**
         * Rate at which the transport layer handle packets, should be
         * synchronized with codec requirements.
         */
        unsigned int transportRate_;

        NON_COPYABLE(AudioRtpSession);

         /**
         * Start ccRTP thread loop
         * This thread will send AND receive rtp packets
         */
        virtual void startRTPLoop() = 0;

        /**
         * Send DTMF over RTP (RFC2833). The timestamp and sequence number must be
         * incremented as if it was microphone audio. This function change the payload type of the rtp session,
         * send the appropriate DTMF digit using this payload, discard coresponding data from mainbuffer and get
         * back the codec payload for further audio processing.
         */
        void sendDtmfEvent();

        /**
         * Send encoded data to peer
         */
        virtual size_t sendMicData();

        /**
         * Set RTP Sockets send/receive timeouts
         */
        void setSessionTimeouts();

        /**
         * Receive data from peer
         */
        void receiveSpeakerData();

        /**
         * Used by loop_ */
        void process();

        // Main destination address for this rtp session.
        // Stored in case of reINVITE, which may require to forget
        // this destination and update a new one.
        IpAddr remoteIp_;

        unsigned rxLastSeqNum_;
#ifdef RTP_DEBUG
        std::chrono::high_resolution_clock::time_point rxLast_;
        std::vector<double> rxJitters_;
        unsigned jitterReportInterval_;
#endif

        std::list<DTMFEvent> dtmfQueue_;
        int dtmfPayloadType_;

        // this must be last to ensure that it's destroyed first
        ThreadLoop loop_;
};
}
#endif // AUDIO_RTP_SESSION_H__

