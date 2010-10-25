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

#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "audio/audioprocessing.h"
#include "audio/noisesuppress.h"

namespace sfl
{

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
	AudioRtpRecord();
	virtual ~AudioRtpRecord();

	/**
	 * Return a pointer to the current codec instance
	 */
    inline AudioCodec *getAudioCodec() const;

    /**
     * Get current payload type (static or dynamic)
     */
    int getCodecPayloadType() const;

    /**
     * Return wether or not this codec has dynamic payload to be
     * negotiated during SDP session
     */
    bool getHasDynamicPayload() const;

    /**
     * Get current audio layer frame size
	 */
    int getAudioLayerFrameSize() const;

    /**
     * Get current audio layer sampling rate
     */
    int getAudioLayerSampleRate() const;

    /**
     * Get current codec frame size
     */
    int getCodecFrameSize() const;

    /**
     * Return current sample rate
     */
    int getCodecSampleRate() const;

    /**
     * Get the sampling rate converter for this session
     */
    SamplerateConverter *getConverter() const;

    /**
     * Get sampling rate converter's sampling rate
     */
    inline int getConverterSamplingRate() const;

    /**
     * Return a poiner to the DTMF event queue
     */
    EventQueue *getEventQueue() const;

    /**
     * Return the number of DTMF event waiting in the queue
     */
    int getEventQueueSize() const;

    unsigned char *getEncodedData() const;

    /**
     * Return a pointer to the current codec instance
     */
    void setAudioCodec(AudioCodec *audioCodec);

    /**
     * Set current codec payload (static or dynamic)
     */
    void setCodecPayloadType(int codecPayloadType);

    /**
     * Set audio layer frame size
     */
    void setAudioLayerFrameSize(int _audioLayerFrameSize);

    /**
     * Set current audio layer sampling rate
     * used to process sampling rate conversion
     */
    void setAudioLayerSampleRate(int _audioLayerSampleRate);

    /**
     * Set codec frame size used to compute sampling rate conversion
     */
    void setCodecFrameSize(int _codecFrameSize);

    /**
     * Set codec sampling rate used to compute sampling rate conversion
     */
    void setCodecSampleRate(int _codecSampleRate);

    /**
     * Set sampling rate converter for this session
     */
    void setConverter(SamplerateConverter *_converter);

    /**
     * Set converter sampling rate used to compute conversion buffer size
     */
    void setConverterSamplingRate(int _converterSamplingRate);

private:
    /**
     * Pointer to the session's codec
	 */
    AudioCodec * _audioCodec;

    /**
     * Codec payload type
     */
    int _codecPayloadType;

    /**
     * Dynamic payload are negotiated during sdp session while static payload
     * are predefined numbers identifying the codec
     */
    bool _hasDynamicPayloadType;

    /**
     * Mic-data related buffers
	 */
    SFLDataFormat* _micData;
    SFLDataFormat* _micDataConverted;
    unsigned char* _micDataEncoded;

    /**
     * Speaker-data related buffers
	 */
    SFLDataFormat* _spkrDataDecoded;
    SFLDataFormat* _spkrDataConverted;

    /**
     * Sample rate converter object
	 */
    SamplerateConverter * _converter;

    /**
     * Variables to process audio stream: sample rate for playing sound (typically 44100HZ)
     */
    int _audioLayerSampleRate;

    /**
     * Sample rate of the codec we use to encode and decode (most of time 8000HZ)
	 */
    int _codecSampleRate;

    /**
     * Length of the sound frame we capture in ms (typically 20ms)
	 */
    int _audioLayerFrameSize;

    /**
     * Codecs frame size in samples (20 ms => 882 at 44.1kHz)
     * The exact value is stored in the codec
	 */
    int _codecFrameSize;

    /**
     * Sampling rate of audio converter
     */
    int _converterSamplingRate;

    /**
     * EventQueue used to store list of DTMF-
     */
    EventQueue _eventQueue;

    /**
     * State of mic fade in
     */
    bool _micFadeInComplete;

    /**
       * State of spkr fade in
     */
    bool _spkrFadeInComplete;

    /**
     * Ampliturde factor to fade in mic data
     */
    SFLDataFormat _micAmplFactor;

    /**
     * Amplitude factor to fade in spkr data
     */
    SFLDataFormat _spkrAmplFactor;

    /**
     * Audio process containing noise reduction engine
     */
    AudioProcessing *_audioProcess;

    /**
     * Noise reduction engine
     */
    NoiseSuppress *_noiseSuppress;

};


class AudioRtpRecordHandler {
public:
	AudioRtpRecordHandler();
	virtual ~AudioRtpRecordHandler();

    /**
     *  Set rtp media for this session
     */
    void setRtpMedia(AudioCodec* audioCodec);

	int getCodecPayloadType(void) { return _audioRtpRecord.getCodecPayloadType(); }

	int getCodecSampleRate(void) { return _audioRtpRecord.getCodecSampleRate(); }

	int getCodecFrameSize(void) { return _audioRtpRecord.getCodecFrameSize(); }

	int getHasDynamicPayload(void) { return _audioRtpRecord.getHasDynamicPayload(); }

	int getAudioLayerFrameSize(void) { return _audioRtpRecord.getAudioLayerFrameSize(); }

	int getAudioLayerSampleRate(void) { return _audioRtpRecord.getAudioLayerSampleRate(); }

	EventQueue *getEventQueue(void) { return _audioRtpRecord.getEventQueue(); }

	int getEventQueueSize(void) { return _audioRtpRecord.getEventQueueSize(); }

	unsigned char *getEncodedData(void) {return _audioRtpRecord.getEncodedData(); }

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

private:

    AudioRtpRecord	_audioRtpRecord;

};

}

#endif /* AUDIORTPRECORD_H_ */
