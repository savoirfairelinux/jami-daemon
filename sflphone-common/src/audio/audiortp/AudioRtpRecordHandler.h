/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "sip/sipcall.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "audio/audioprocessing.h"
#include "audio/noisesuppress.h"
#include "managerimpl.h"
#include <ccrtp/rtp.h>

namespace sfl
{

// Frequency (in packet number)
#define RTP_TIMESTAMP_RESET_FREQ 100

// Factor use to increase volume in fade in
#define FADEIN_STEP_SIZE 4;

static const int schedulingTimeout = 10000;
static const int expireTimeout = 100000;

// G.722 VoIP is typically carried in RTP payload type 9.[2] Note that IANA records the clock rate for type 9 G.722 as 8 kHz
// (instead of 16 kHz), RFC3551[3]  clarifies that this is due to a historical error and is retained in order to maintain backward
// compatibility. Consequently correct implementations represent the value 8,000 where required but encode and decode audio at 16 kHz.
static const int g722PayloadType = 9;
static const int g722RtpClockRate = 8000;
static const int g722RtpTimeincrement = 160;

inline uint32
timeval2microtimeout(const timeval& t)
{ return ((t.tv_sec * 1000000ul) + t.tv_usec); }

class AudioRtpSessionException: public std::exception
{
        virtual const char* what() const throw() {
            return "AudioRtpSessionException occured";
        }
};

typedef struct DtmfEvent {
    ost::RTPPacket::RFC2833Payload payload;
    int length;
    bool newevent;
} DtmfEvent;

typedef list<DtmfEvent *> EventQueue;

/**
 * Class meant to store internal data in order to encode/decode,
 * resample, process, and packetize audio streams. This class should not be
 * handled directly. Use AudioRtpRecorrdHandeler
 */
class AudioRtpRecord {
public:
	AudioRtpRecord(ManagerImpl *manager);
	virtual ~AudioRtpRecord();
    inline AudioCodec *getAudioCodec() const;
    int getCodecPayloadType() const;
    bool getHasDynamicPayload() const;
    int getAudioLayerFrameSize() const;
    int getAudioLayerSampleRate() const;
    int getCodecFrameSize() const;
    int getCodecSampleRate() const;
    SamplerateConverter *getConverter() const;
    inline int getConverterSamplingRate() const;
    EventQueue *getEventQueue();
    int getEventQueueSize() const;
    SFLDataFormat *getMicData();
    SFLDataFormat *getMicDataConverted();
    unsigned char *getMicDataEncoded();
    SFLDataFormat *getMicAmplFactor();
    bool getMicFadeInComplete() const;
    SFLDataFormat *getSpkrAmplFactor();
    SFLDataFormat *getSpkrDataConverted() const;
    SFLDataFormat *getSpkrDataDecoded() const;
    bool getSpkrFadeInComplete() const;
    AudioProcessing *getNoiseReductionProcess() const;

    void setAudioCodec(AudioCodec *audioCodec);
    void setCodecPayloadType(int codecPayloadType);
    void setHasDynamicPayload(bool hasDynamicPayload);
    void setAudioLayerFrameSize(int _audioLayerFrameSize);
    void setAudioLayerSampleRate(int _audioLayerSampleRate);
    void setCodecFrameSize(int _codecFrameSize);
    void setCodecSampleRate(int _codecSampleRate);
    void setConverter(SamplerateConverter *_converter);
    void setConverterSamplingRate(int _converterSamplingRate);
    void setEventQueue(EventQueue _eventQueue);
    void setMicData(SFLDataFormat *micData);
    void setMicDataConverted(SFLDataFormat *micDataConverted);
    void setMicDataEncoded(unsigned char *micDataEncoded);
    void setMicAmplFactor(SFLDataFormat _micAmplFactor);
    void setMicFadeInComplete(bool _micFadeInComplete);
    void setSpkrAmplFactor(SFLDataFormat _spkrAmplFactor);
    void setSpkrDataConverted(SFLDataFormat *_spkrDataConverted);
    void setSpkrDataDecoded(SFLDataFormat *_spkrDataDecoded);
    void setSpkrFadeInComplete(bool _spkrFadeInComplete);
    void setAudioProcessing(AudioProcessing *audioProcess);
    void setNoiseSuppress(NoiseSuppress *noiseSuppress);

private:
    AudioCodec *_audioCodec;
    int _codecPayloadType;
    bool _hasDynamicPayloadType;
    SFLDataFormat *_micData;
    SFLDataFormat *_micDataConverted;
    unsigned char *_micDataEncoded;
    SFLDataFormat *_spkrDataDecoded;
    SFLDataFormat *_spkrDataConverted;
    SamplerateConverter *_converter;
    int _audioLayerSampleRate;
    int _codecSampleRate;
    int _audioLayerFrameSize;
    int _codecFrameSize;
    int _converterSamplingRate;
    EventQueue _eventQueue;
    bool _micFadeInComplete;
    bool _spkrFadeInComplete;
    SFLDataFormat _micAmplFactor;
    SFLDataFormat _spkrAmplFactor;
    AudioProcessing *_audioProcess;
    NoiseSuppress *_noiseSuppress;

};


class AudioRtpRecordHandler {
public:
	AudioRtpRecordHandler(ManagerImpl *manager, SIPCall *ca);
	virtual ~AudioRtpRecordHandler();

    /**
     *  Set rtp media for this session
     */
    void setRtpMedia(AudioCodec* audioCodec);

    AudioCodec *getAudioCodec(void) { return _audioRtpRecord.getAudioCodec(); }

	int getCodecPayloadType(void) { return _audioRtpRecord.getCodecPayloadType(); }

	int getCodecSampleRate(void) { return _audioRtpRecord.getCodecSampleRate(); }

	int getCodecFrameSize(void) { return _audioRtpRecord.getCodecFrameSize(); }

	int getHasDynamicPayload(void) { return _audioRtpRecord.getHasDynamicPayload(); }

	int getAudioLayerFrameSize(void) { return _audioRtpRecord.getAudioLayerFrameSize(); }

	int getAudioLayerSampleRate(void) { return _audioRtpRecord.getAudioLayerSampleRate(); }

	EventQueue *getEventQueue(void) { return _audioRtpRecord.getEventQueue(); }

	int getEventQueueSize(void) { return _audioRtpRecord.getEventQueueSize(); }

	SFLDataFormat *getMicData(void) { return _audioRtpRecord.getMicData(); }

	SFLDataFormat *getMicDataConverted(void) { return _audioRtpRecord.getMicDataConverted(); }

	unsigned char *getMicDataEncoded(void) { return _audioRtpRecord.getMicDataEncoded(); }

	inline float computeCodecFrameSize (int codecSamplePerFrame, int codecClockRate) {
		return ( (float) codecSamplePerFrame * 1000.0) / (float) codecClockRate;
	}

	int computeNbByteAudioLayer (float codecFrameSize) {
	    return (int) ( ( (float) _audioRtpRecord.getCodecSampleRate() * codecFrameSize * sizeof (SFLDataFormat)) / 1000.0);
	}

	void init(void);

     /**
      * Allocate memory for RTP buffers and fill them with zeros
 	  * @prereq Session codec needs to be initialized prior calling this method
      */
     void initBuffers (void);

     void initNoiseSuppress (void);

    /**
     * Encode audio data from mainbuffer
     */
    int processDataEncode (void);

    /**
     * Decode audio data received from peer
     */
    void processDataDecode (unsigned char * spkrData, unsigned int size);

    /**
    * Ramp In audio data to avoid audio click from peer
    */
    bool fadeIn (SFLDataFormat *audio, int size, SFLDataFormat *factor);

    void putDtmfEvent (int digit);

private:

    AudioRtpRecord	_audioRtpRecord;

    SIPCall *_ca;

};

}

#endif /* AUDIORTPRECORD_H_ */
