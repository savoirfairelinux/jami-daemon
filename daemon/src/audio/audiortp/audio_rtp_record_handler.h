/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#ifndef AUDIO_RTP_RECORD_HANDLER_H__
#define AUDIO_RTP_RECORD_HANDLER_H__
#include <cstddef>

using std::ptrdiff_t;

#include <ccrtp/rtp.h>
#include <tr1/array>
#include <list>

class SIPCall;
#include "noncopyable.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "audio/noisesuppress.h"
#include "audio/gaincontrol.h"

namespace sfl {

// G.722 VoIP is typically carried in RTP payload type 9.[2] Note that IANA records the clock rate for type 9 G.722 as 8 kHz
// (instead of 16 kHz), RFC3551[3]  clarifies that this is due to a historical error and is retained in order to maintain backward
// compatibility. Consequently correct implementations represent the value 8,000 where required but encode and decode audio at 16 kHz.

inline uint32
timeval2microtimeout(const timeval& t)
{
    return ((t.tv_sec * 1000000ul) + t.tv_usec);
}

struct DTMFEvent {
    DTMFEvent(char digit);
    ost::RTPPacket::RFC2833Payload payload;
    bool newevent;
    int length;
};

/**
 * Class meant to store internal data in order to encode/decode,
 * resample, process, and packetize audio streams. This class should not be
 * handled directly. Use AudioRtpRecordHandler
 */
class AudioRtpRecord {
    public:
        AudioRtpRecord();
        ~AudioRtpRecord();
        std::string callId_;
        int codecSampleRate_;
        std::list<DTMFEvent> dtmfQueue_;

    private:
        AudioCodec *audioCodec_;
        ost::Mutex audioCodecMutex_;
        int codecPayloadType_;
        bool hasDynamicPayloadType_;
        std::tr1::array<SFLDataFormat, DEC_BUFFER_SIZE> decData_;
// FIXME: resampledData should be resized as needed
        std::tr1::array<SFLDataFormat, DEC_BUFFER_SIZE * 4> resampledData_;
        std::tr1::array<unsigned char, DEC_BUFFER_SIZE> encodedData_;
        SamplerateConverter *converterEncode_;
        SamplerateConverter *converterDecode_;
        int codecFrameSize_;
        int converterSamplingRate_;
        double fadeFactor_;

#if HAVE_SPEEXDSP
        NoiseSuppress *noiseSuppressEncode_;
        NoiseSuppress *noiseSuppressDecode_;
        ost::Mutex audioProcessMutex_;
#endif

        unsigned int dtmfPayloadType_;

        bool isDead();
        friend class AudioRtpRecordHandler;
        /**
        * Ramp In audio data to avoid audio click from peer
        */
        void fadeInDecodedData(size_t size);
        NON_COPYABLE(AudioRtpRecord);
#ifdef CCPP_PREFIX
        ost::AtomicCounter dead_;
#else
        ucommon::atomic::counter dead_;
#endif
};


class AudioRtpRecordHandler {
    public:
        AudioRtpRecordHandler(SIPCall &);
        virtual ~AudioRtpRecordHandler();

        /**
         *  Set rtp media for this session
         */
        void setRtpMedia(AudioCodec* audioCodec);

        AudioCodec *getAudioCodec() const {
            return audioRtpRecord_.audioCodec_;
        }

        int getCodecPayloadType() const {
            return audioRtpRecord_.codecPayloadType_;
        }

        int getCodecSampleRate() const {
            return audioRtpRecord_.codecSampleRate_;
        }

        int getCodecFrameSize() const {
            return audioRtpRecord_.codecFrameSize_;
        }

        bool getHasDynamicPayload() const {
            return audioRtpRecord_.hasDynamicPayloadType_;
        }

        bool hasDTMFPending() const {
            return not audioRtpRecord_.dtmfQueue_.empty();
        }

        const unsigned char *getMicDataEncoded() const {
            return audioRtpRecord_.encodedData_.data();
        }

        void initBuffers();

#if HAVE_SPEEXDSP
        void initNoiseSuppress();
#endif

        /**
         * Encode audio data from mainbuffer
         */
        int processDataEncode();

        /**
         * Decode audio data received from peer
         */
        void processDataDecode(unsigned char * spkrData, size_t size, int payloadType);

        void setDtmfPayloadType(unsigned int payloadType) {
            audioRtpRecord_.dtmfPayloadType_ = payloadType;
        }

        unsigned int getDtmfPayloadType() const {
            return audioRtpRecord_.dtmfPayloadType_;
        }

        void putDtmfEvent(char digit);

    protected:
        AudioRtpRecord audioRtpRecord_;

    private:
        const std::string id_;
        GainControl gainController;
};
}

#endif // AUDIO_RTP_RECORD_HANDLER_H__
