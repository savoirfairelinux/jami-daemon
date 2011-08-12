/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef AUDIORTPRECORDHANDLER_H_
#define AUDIORTPRECORDHANDLER_H_
#include <cstddef>

using std::ptrdiff_t;

#include <ccrtp/rtp.h>
#include <list>

#include "sip/sipcall.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "audio/audioprocessing.h"
#include "audio/noisesuppress.h"
#include "audio/speexechocancel.h"
#include "audio/echosuppress.h"
#include "audio/gaincontrol.h"
#include "managerimpl.h"

namespace sfl
{

// Frequency (in packet number)
#define RTP_TIMESTAMP_RESET_FREQ 100

// Factor use to increase volume in fade in
#define FADEIN_STEP_SIZE 4;

static const int schedulingTimeout = 4000;
static const int expireTimeout = 1000000;

// G.722 VoIP is typically carried in RTP payload type 9.[2] Note that IANA records the clock rate for type 9 G.722 as 8 kHz
// (instead of 16 kHz), RFC3551[3]  clarifies that this is due to a historical error and is retained in order to maintain backward
// compatibility. Consequently correct implementations represent the value 8,000 where required but encode and decode audio at 16 kHz.
static const int g722PayloadType = 9;
static const int g722RtpClockRate = 8000;
static const int g722RtpTimeincrement = 160;

inline uint32
timeval2microtimeout (const timeval& t)
{
    return ( (t.tv_sec * 1000000ul) + t.tv_usec);
}

typedef struct DtmfEvent {
    ost::RTPPacket::RFC2833Payload payload;
    int factor;
    int length;
    bool newevent;
} DtmfEvent;

typedef std::list<DtmfEvent *> EventQueue;

/**
 * Class meant to store internal data in order to encode/decode,
 * resample, process, and packetize audio streams. This class should not be
 * handled directly. Use AudioRtpRecorrdHandeler
 */
class AudioRtpRecord
{
    public:
        AudioRtpRecord ();
        ~AudioRtpRecord();

        AudioCodec *_audioCodec;
        ost::Mutex audioCodecMutex;
        int _codecPayloadType;
        bool _hasDynamicPayloadType;
        SFLDataFormat decData[DEC_BUFFER_SIZE];
        SFLDataFormat resampledData[DEC_BUFFER_SIZE];
        unsigned char encodedData[DEC_BUFFER_SIZE];
        SamplerateConverter *_converter;
        int _codecSampleRate;
        int _codecFrameSize;
        int _converterSamplingRate;
        EventQueue _eventQueue;
        SFLDataFormat _micAmplFactor;
        AudioProcessing *_audioProcess;
        NoiseSuppress *_noiseSuppress;
        ost::Mutex audioProcessMutex;
        std::string _callId;
        unsigned int _dtmfPayloadType;

};


class AudioRtpRecordHandler
{
    public:
        AudioRtpRecordHandler (SIPCall *ca);
        virtual ~AudioRtpRecordHandler();

        /**
         *  Set rtp media for this session
         */

        void setRtpMedia (AudioCodec* audioCodec);

        void updateRtpMedia (AudioCodec *audioCodec);


        AudioCodec *getAudioCodec (void) const {
            return _audioRtpRecord._audioCodec;
        }

        int getCodecPayloadType (void) const {
            return _audioRtpRecord._codecPayloadType;
        }

        int getCodecSampleRate (void) const {
            return _audioRtpRecord._codecSampleRate;
        }

        int getCodecFrameSize (void) const {
            return _audioRtpRecord._codecFrameSize;
        }

        bool getHasDynamicPayload (void) const {
            return _audioRtpRecord._hasDynamicPayloadType;
        }

        EventQueue *getEventQueue (void) {
            return &_audioRtpRecord._eventQueue;
        }

        int getEventQueueSize (void) const {
            return _audioRtpRecord._eventQueue.size();
        }

        const unsigned char *getMicDataEncoded (void) const {
            return _audioRtpRecord.encodedData;
        }

        void initBuffers (void);

        void initNoiseSuppress (void);

        /**
         * Encode audio data from mainbuffer
         */
        int processDataEncode (void);

        /**
         * Decode audio data received from peer
         */
        void processDataDecode (unsigned char * spkrData, unsigned int size, int payloadType);

        /**
        * Ramp In audio data to avoid audio click from peer
        */
        void fadeIn (SFLDataFormat *audio, int size, SFLDataFormat *factor);

        void setDtmfPayloadType(unsigned int payloadType) {
        	_audioRtpRecord._dtmfPayloadType = payloadType;
        }

        unsigned int getDtmfPayloadType(void) const {
        	return _audioRtpRecord._dtmfPayloadType;
        }

        void putDtmfEvent (int digit);

    protected:

        AudioRtpRecord	_audioRtpRecord;

    private:

        const std::string id_;

        EchoSuppress echoCanceller;

        GainControl gainController;
};

}

#endif /* AUDIORTPRECORD_H_ */
