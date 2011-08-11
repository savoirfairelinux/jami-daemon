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

#include "AudioRtpRecordHandler.h"
#include <fstream>

#include "audio/audiolayer.h"
#include "manager.h"

// #define DUMP_PROCESS_DATA_ENCODE

namespace sfl
{

static const SFLDataFormat initFadeinFactor = 32000;

AudioRtpRecord::AudioRtpRecord () : _audioCodec (NULL)
    , _hasDynamicPayloadType (false)
    , _micData (NULL)
    , _micDataConverted (NULL)
    , _micDataEncoded (NULL)
    , _spkrDataDecoded (NULL)
    , _spkrDataConverted (NULL)
    , _converter (NULL)
    , _codecSampleRate (0)
    , _codecFrameSize (0)
    , _micFadeInComplete (false)
    , _spkrFadeInComplete (false)
    , _micAmplFactor (initFadeinFactor)
    , _spkrAmplFactor (initFadeinFactor)
    , _audioProcess (NULL)
    , _noiseSuppress (NULL)
    , _callId ("")
    , _dtmfPayloadType(101) // same as Asterisk
{

}


AudioRtpRecord::~AudioRtpRecord()
{
    _debug ("AudioRtpRecord: Delete audio rtp internal data");

    delete [] _micData;

    delete [] _micDataConverted;

    delete [] _micDataEncoded;

    delete [] _micDataEchoCancelled;

    delete [] _spkrDataDecoded;

    delete [] _spkrDataConverted;

    delete _converter;

    audioCodecMutex.enter();

    delete _audioCodec;

    audioCodecMutex.leave();

    audioProcessMutex.enter();

    delete _audioProcess;

    delete _noiseSuppress;

    audioProcessMutex.leave();
}


AudioRtpRecordHandler::AudioRtpRecordHandler (SIPCall *ca) : _audioRtpRecord (), id_ (ca->getCallId()), echoCanceller(ca->getMemoryPool()), gainController(8000, -10.0)
{

}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {}

void AudioRtpRecordHandler::setRtpMedia (AudioCodec* audioCodec)
{
    _audioRtpRecord.audioCodecMutex.enter();

    // Set varios codec info to reduce indirection
    _audioRtpRecord._audioCodec = audioCodec;
    _audioRtpRecord._codecPayloadType = audioCodec->getPayloadType();
    _audioRtpRecord._codecSampleRate = audioCodec->getClockRate();
    _audioRtpRecord._codecFrameSize = audioCodec->getFrameSize();
    _audioRtpRecord._hasDynamicPayloadType = audioCodec->hasDynamicPayload();

    _audioRtpRecord.audioCodecMutex.leave();
}


void AudioRtpRecordHandler::updateRtpMedia (AudioCodec *audioCodec)
{
    int lastSamplingRate = _audioRtpRecord._codecSampleRate;

    _audioRtpRecord.audioCodecMutex.enter();

    if (_audioRtpRecord._audioCodec) {
        delete _audioRtpRecord._audioCodec;
        _audioRtpRecord._audioCodec = NULL;
    }

    _audioRtpRecord._audioCodec = audioCodec;
    _audioRtpRecord._codecPayloadType = audioCodec->getPayloadType();
    _audioRtpRecord._codecSampleRate = audioCodec->getClockRate();
    _audioRtpRecord._codecFrameSize = audioCodec->getFrameSize();
    _audioRtpRecord._hasDynamicPayloadType = audioCodec->hasDynamicPayload();

    _audioRtpRecord.audioCodecMutex.leave();

    Manager::instance().audioSamplingRateChanged(_audioRtpRecord._codecSampleRate);

    if (lastSamplingRate != _audioRtpRecord._codecSampleRate)
        updateNoiseSuppress();
}

void AudioRtpRecordHandler::init()
{
}

void AudioRtpRecordHandler::initBuffers()
{
    int codecSampleRate = _audioRtpRecord._codecSampleRate;

    // Set sampling rate, main buffer choose the highest one
    // Manager::instance().getMainBuffer()->setInternalSamplingRate (codecSampleRate);
    Manager::instance().audioSamplingRateChanged(codecSampleRate);

    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    int rate = getCodecSampleRate();
    _audioRtpRecord._converter = new SamplerateConverter (rate);

    int nbSamplesMax = (int) ( (getCodecSampleRate() * getCodecFrameSize() / 1000));
    _audioRtpRecord._micData = new SFLDataFormat[nbSamplesMax];
    _audioRtpRecord._micDataConverted = new SFLDataFormat[nbSamplesMax];
    _audioRtpRecord._micDataEchoCancelled = new SFLDataFormat[nbSamplesMax];
    _audioRtpRecord._micDataEncoded = new unsigned char[nbSamplesMax * 2];
    _audioRtpRecord._spkrDataConverted = new SFLDataFormat[nbSamplesMax];
    _audioRtpRecord._spkrDataDecoded = new SFLDataFormat[nbSamplesMax];
}

void AudioRtpRecordHandler::initNoiseSuppress()
{
    _audioRtpRecord.audioProcessMutex.enter();

    NoiseSuppress *noiseSuppress = new NoiseSuppress (getCodecFrameSize(), getCodecSampleRate());
    AudioProcessing *processing = new AudioProcessing (noiseSuppress);

    _audioRtpRecord._noiseSuppress = noiseSuppress;
    _audioRtpRecord._audioProcess = processing;

    _audioRtpRecord.audioProcessMutex.leave();
}

void AudioRtpRecordHandler::updateNoiseSuppress()
{

    _audioRtpRecord.audioProcessMutex.enter();

    if (_audioRtpRecord._audioProcess)
        delete _audioRtpRecord._audioProcess;

    _audioRtpRecord._audioProcess = NULL;

    if (_audioRtpRecord._noiseSuppress)
        delete _audioRtpRecord._noiseSuppress;

    _audioRtpRecord._noiseSuppress = NULL;

    _debug ("AudioRtpSession: Update noise suppressor with sampling rate %d and frame size %d", getCodecSampleRate(), getCodecFrameSize());

    NoiseSuppress *noiseSuppress = new NoiseSuppress (getCodecFrameSize(), getCodecSampleRate());
    AudioProcessing *processing = new AudioProcessing (noiseSuppress);

    _audioRtpRecord._noiseSuppress = noiseSuppress;
    _audioRtpRecord._audioProcess = processing;

    _audioRtpRecord.audioProcessMutex.leave();

}

void AudioRtpRecordHandler::putDtmfEvent (int digit)
{
    sfl::DtmfEvent *dtmf = new sfl::DtmfEvent();
    dtmf->payload.event = digit;
    dtmf->payload.ebit = false; // end of event bit
    dtmf->payload.rbit = false; // reserved bit
    dtmf->payload.duration = 1; // duration for this event
    dtmf->newevent = true;
    dtmf->length = 1000;
    getEventQueue()->push_back (dtmf);
    _debug ("AudioRtpSession: Put Dtmf Event %d", digit);
}

#ifdef DUMP_PROCESS_DATA_ENCODE
std::ofstream teststream("test_process_data_encode.raw");
#endif

int AudioRtpRecordHandler::processDataEncode (void)
{
    SFLDataFormat *micData = _audioRtpRecord._micData;
    unsigned char *micDataEncoded = _audioRtpRecord._micDataEncoded;
    SFLDataFormat *micDataConverted = _audioRtpRecord._micDataConverted;

    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    // compute codec framesize in ms
    float fixedCodecFramesize = ((float)getCodecFrameSize() * 1000.0) / (float)codecSampleRate;
    // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
    int bytesToGet = (int) ( ( (float) mainBufferSampleRate * fixedCodecFramesize * sizeof (SFLDataFormat)) / 1000.0);

    if (Manager::instance().getMainBuffer()->availForGet (id_) < bytesToGet)
        return 0;

    int bytes = Manager::instance().getMainBuffer()->getData (micData, bytesToGet, id_);
    if (bytes == 0)
        return 0;

    if (!_audioRtpRecord._micFadeInComplete)
        _audioRtpRecord._micFadeInComplete = fadeIn (micData, bytes / sizeof(SFLDataFormat), &_audioRtpRecord._micAmplFactor);

    // nb bytes to be sent over RTP
    int compSize;

    // test if resampling is required
    if (codecSampleRate != mainBufferSampleRate) {
        _audioRtpRecord._converter->resample (micData, micDataConverted, codecSampleRate, mainBufferSampleRate, bytes / sizeof(SFLDataFormat));

        _audioRtpRecord.audioProcessMutex.enter();

        if (Manager::instance().audioPreference.getNoiseReduce()) {
            _audioRtpRecord._audioProcess->processAudio (micDataConverted, bytes);
        }

        if(Manager::instance().getEchoCancelState() == "enabled") {
            echoCanceller.getData(micData);
        }

        _audioRtpRecord.audioProcessMutex.leave();

        _audioRtpRecord.audioCodecMutex.enter();

        compSize = _audioRtpRecord._audioCodec->encode (micDataEncoded, micData, bytes);

        _audioRtpRecord.audioCodecMutex.leave();

    } else {        // no resampling required
        _audioRtpRecord.audioProcessMutex.enter();

        if (Manager::instance().audioPreference.getNoiseReduce())
            _audioRtpRecord._audioProcess->processAudio (micData, bytes);

        if(Manager::instance().getEchoCancelState() == "enabled")
            echoCanceller.getData(micData);
	
#ifdef DUMP_PROCESS_DATA_ENCODE
        teststream.write(reinterpret_cast<char *>(micData), nbSample * sizeof(SFLDataFormat));
#endif
        
        _audioRtpRecord.audioProcessMutex.leave();

        _audioRtpRecord.audioCodecMutex.enter();
        compSize = _audioRtpRecord._audioCodec->encode (micDataEncoded, micData, bytes);
        _audioRtpRecord.audioCodecMutex.leave();
    }

    return compSize;
}

void AudioRtpRecordHandler::processDataDecode (unsigned char *spkrData, unsigned int size)
{
    int codecSampleRate = getCodecSampleRate();

    SFLDataFormat *spkrDataDecoded = _audioRtpRecord._spkrDataConverted;
    SFLDataFormat *spkrDataConverted = _audioRtpRecord._spkrDataDecoded;

    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    _audioRtpRecord.audioCodecMutex.enter();

    // Return the size of data in bytes
    int expandedSize = _audioRtpRecord._audioCodec->decode (spkrDataDecoded , spkrData , size);

    _audioRtpRecord.audioCodecMutex.leave();

    // buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
    int nbSample = expandedSize / sizeof (SFLDataFormat);

    if (!_audioRtpRecord._spkrFadeInComplete) {
        _audioRtpRecord._spkrFadeInComplete = fadeIn (spkrDataDecoded, nbSample, &_audioRtpRecord._micAmplFactor);
    }

    // Normalize incomming signal
    gainController.process(spkrDataDecoded, nbSample);

    // test if resampling is required
    if (codecSampleRate != mainBufferSampleRate) {
        // Do sample rate conversion
        _audioRtpRecord._converter->resample (spkrDataDecoded, spkrDataConverted, codecSampleRate, mainBufferSampleRate, nbSample);

        if(Manager::instance().getEchoCancelState() == "enabled") {
            echoCanceller.putData(spkrDataConverted, expandedSize);
        }

        // put data in audio layer, size in byte
        Manager::instance().getMainBuffer()->putData (spkrDataConverted, expandedSize, id_);

    } else {
    	if(Manager::instance().getEchoCancelState() == "enabled") {
    	    echoCanceller.putData(spkrDataDecoded, expandedSize);
    	}
        // put data in audio layer, size in byte
        Manager::instance().getMainBuffer()->putData (spkrDataDecoded, expandedSize, id_);
    }
}

bool AudioRtpRecordHandler::fadeIn (SFLDataFormat *audio, int size, SFLDataFormat *factor)
{
    // if factor reach 0, this function should no be called anymore
    if (*factor <= 0)
        return true;

    while (size)
        audio[--size] /= *factor;

    *factor /= FADEIN_STEP_SIZE;

    return *factor <= 0;
}

}

