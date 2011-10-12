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

#include "audio_rtp_record_handler.h"
#include <fstream>

#include "sip/sipcall.h"
#include "audio/audiolayer.h"
#include "manager.h"

namespace sfl {

static const SFLDataFormat initFadeinFactor = 32000;

AudioRtpRecord::AudioRtpRecord() : audioCodec_(NULL)
    , hasDynamicPayloadType_(false)
    , converter_(NULL)
    , codecSampleRate_(0)
    , codecFrameSize_(0)
    , micAmplFactor_(initFadeinFactor)
    , noiseSuppress_(NULL)
    , callId_("")
    , dtmfPayloadType_(101) // same as Asterisk
{}


AudioRtpRecord::~AudioRtpRecord()
{
    delete converter_;
    delete audioCodec_;
    delete noiseSuppress_;
}


AudioRtpRecordHandler::AudioRtpRecordHandler(SIPCall *ca) : audioRtpRecord_(), id_(ca->getCallId()), echoCanceller(ca->getMemoryPool()), gainController(8000, -10.0)
{}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {}

void AudioRtpRecordHandler::setRtpMedia(AudioCodec* audioCodec)
{
    ost::MutexLock lock(audioRtpRecord_.audioCodecMutex_);

    delete audioRtpRecord_.audioCodec_;
    // Set varios codec info to reduce indirection
    audioRtpRecord_.audioCodec_ = audioCodec;
    audioRtpRecord_.codecPayloadType_ = audioCodec->getPayloadType();
    audioRtpRecord_.codecSampleRate_ = audioCodec->getClockRate();
    audioRtpRecord_.codecFrameSize_ = audioCodec->getFrameSize();
    audioRtpRecord_.hasDynamicPayloadType_ = audioCodec->hasDynamicPayload();
}

void AudioRtpRecordHandler::initBuffers()
{
    // Set sampling rate, main buffer choose the highest one
    Manager::instance().audioSamplingRateChanged(audioRtpRecord_.codecSampleRate_);

    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    delete audioRtpRecord_.converter_;
    audioRtpRecord_.converter_ = new SamplerateConverter(getCodecSampleRate());
}

void AudioRtpRecordHandler::initNoiseSuppress()
{
    ost::MutexLock lock(audioRtpRecord_.audioProcessMutex_);
    delete audioRtpRecord_.noiseSuppress_;
    audioRtpRecord_.noiseSuppress_ = new NoiseSuppress(getCodecFrameSize(), getCodecSampleRate());
}

void AudioRtpRecordHandler::putDtmfEvent(int digit)
{
    audioRtpRecord_.dtmfQueue_.push_back(digit);
}

#ifdef DUMP_PROCESS_DATA_ENCODE
std::ofstream teststream("test_process_data_encode.raw");
#endif

int AudioRtpRecordHandler::processDataEncode()
{
    SFLDataFormat *micData 			= audioRtpRecord_.decData_;
    unsigned char *micDataEncoded 	= audioRtpRecord_.encodedData_;
    SFLDataFormat *micDataConverted = audioRtpRecord_.resampledData_;

    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    double resampleFactor = (double)mainBufferSampleRate / codecSampleRate;

    // compute nb of byte to get coresponding to 1 audio frame
    int samplesToGet = resampleFactor * getCodecFrameSize();
    int bytesToGet = samplesToGet * sizeof(SFLDataFormat);

    if (Manager::instance().getMainBuffer()->availForGet(id_) < bytesToGet)
        return 0;

    int bytes = Manager::instance().getMainBuffer()->getData(micData, bytesToGet, id_);

    if (bytes != bytesToGet) {
        _error("%s : asked %d bytes from mainbuffer, got %d", __PRETTY_FUNCTION__, bytesToGet, bytes);
        return 0;
    }

    int samples = bytesToGet / sizeof(SFLDataFormat);

    fadeIn(micData, samples, &audioRtpRecord_.micAmplFactor_);

    if (Manager::instance().getEchoCancelState())
        echoCanceller.getData(micData);

#ifdef DUMP_PROCESS_DATA_ENCODE
    teststream.write(reinterpret_cast<char *>(micData), bytesToGet);
#endif

    SFLDataFormat *out = micData;

    if (codecSampleRate != mainBufferSampleRate) {
        out = micDataConverted;
        audioRtpRecord_.converter_->resample(micData, micDataConverted, codecSampleRate, mainBufferSampleRate, samplesToGet);
    }

    if (Manager::instance().audioPreference.getNoiseReduce()) {
        ost::MutexLock lock(audioRtpRecord_.audioProcessMutex_);
        audioRtpRecord_.noiseSuppress_->process(micData, getCodecFrameSize());
    }

    int compSize;
    {
        ost::MutexLock lock(audioRtpRecord_.audioCodecMutex_);
        compSize = audioRtpRecord_.audioCodec_->encode(micDataEncoded, out, getCodecFrameSize());
    }

    return compSize;
}

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, unsigned int size, int payloadType)
{
    if (getCodecPayloadType() != payloadType)
        return;

    int codecSampleRate = getCodecSampleRate();

    SFLDataFormat *spkrDataDecoded = audioRtpRecord_.decData_;
    SFLDataFormat *spkrDataConverted = audioRtpRecord_.resampledData_;

    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    int inSamples;
    {
        ost::MutexLock lock(audioRtpRecord_.audioCodecMutex_);
        // Return the size of data in samples
        inSamples = audioRtpRecord_.audioCodec_->decode(spkrDataDecoded , spkrData , size);
    }

    fadeIn(spkrDataDecoded, inSamples, &audioRtpRecord_.micAmplFactor_);

    // Normalize incomming signal
    gainController.process(spkrDataDecoded, inSamples);

    SFLDataFormat *out = spkrDataDecoded;
    int outSamples = inSamples;

    // test if resampling is required
    if (codecSampleRate != mainBufferSampleRate) {
        // Do sample rate conversion
        outSamples = ((float) inSamples * ((float) mainBufferSampleRate / (float) codecSampleRate));
        audioRtpRecord_.converter_->resample(spkrDataDecoded, spkrDataConverted, codecSampleRate, mainBufferSampleRate, inSamples);
        out = spkrDataConverted;
    }

    if (Manager::instance().getEchoCancelState())
        echoCanceller.putData(out, outSamples);

    Manager::instance().getMainBuffer()->putData(out, outSamples * sizeof(SFLDataFormat), id_);
}

void AudioRtpRecordHandler::fadeIn(SFLDataFormat *audio, int size, SFLDataFormat *factor)
{
    // if factor reach 0, this function should have no effect
    if (*factor <= 0)
        return;

    while (size)
        audio[--size] /= *factor;

    *factor /= FADEIN_STEP_SIZE;
}
}
