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
#include <algorithm>

#include "logger.h"
#include "sip/sipcall.h"
#include "audio/audiolayer.h"
#include "manager.h"

namespace sfl {

static const SFLDataFormat INIT_FADE_IN_FACTOR = 32000;

AudioRtpRecord::AudioRtpRecord() :
    audioCodec_(0)
    , audioCodecMutex_()
    , codecPayloadType_(0)
    , hasDynamicPayloadType_(false)
    , decData_()     // std::tr1::arrays will be 0-initialized
    , resampledData_()
    , encodedData_()
    , converter_(0)
    , codecSampleRate_(0)
    , codecFrameSize_(0)
    , converterSamplingRate_(0)
    , dtmfQueue_()
    , fadeFactor_(INIT_FADE_IN_FACTOR)
    , noiseSuppress_(0)
    , audioProcessMutex_()
    , callId_("")
    , dtmfPayloadType_(101) // same as Asterisk
{}

AudioRtpRecord::~AudioRtpRecord()
{
    delete converter_;
    delete audioCodec_;
    delete noiseSuppress_;
}


AudioRtpRecordHandler::AudioRtpRecordHandler(SIPCall &call) :
    audioRtpRecord_(),
    id_(call.getCallId()),
    echoCanceller(call.getMemoryPool()),
    gainController(8000, -10.0)
{}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {}

void AudioRtpRecordHandler::setRtpMedia(AudioCodec *audioCodec)
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

int AudioRtpRecordHandler::processDataEncode()
{
    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    double resampleFactor = (double) mainBufferSampleRate / codecSampleRate;

    // compute nb of byte to get coresponding to 1 audio frame
    int samplesToGet = resampleFactor * getCodecFrameSize();
    int bytesToGet = samplesToGet * sizeof(SFLDataFormat);

    if (Manager::instance().getMainBuffer()->availForGet(id_) < bytesToGet)
        return 0;

    SFLDataFormat *micData = audioRtpRecord_.decData_.data();
    int bytes = Manager::instance().getMainBuffer()->getData(micData, bytesToGet, id_);

    if (bytes != bytesToGet) {
        ERROR("%s : asked %d bytes from mainbuffer, got %d",
                __PRETTY_FUNCTION__, bytesToGet, bytes);
        return 0;
    }

    int samples = bytesToGet / sizeof(SFLDataFormat);

    audioRtpRecord_.fadeInDecodedData(samples);

    if (Manager::instance().getEchoCancelState())
        echoCanceller.getData(micData);

    SFLDataFormat *out = micData;
    SFLDataFormat *micDataConverted = audioRtpRecord_.resampledData_.data();

    if (codecSampleRate != mainBufferSampleRate) {
        out = micDataConverted;
        audioRtpRecord_.converter_->resample(micData, micDataConverted,
                codecSampleRate, mainBufferSampleRate, samplesToGet);
    }

    if (Manager::instance().audioPreference.getNoiseReduce()) {
        ost::MutexLock lock(audioRtpRecord_.audioProcessMutex_);
        audioRtpRecord_.noiseSuppress_->process(micData, getCodecFrameSize());
    }

    {
        ost::MutexLock lock(audioRtpRecord_.audioCodecMutex_);
        unsigned char *micDataEncoded = audioRtpRecord_.encodedData_.data();
        return audioRtpRecord_.audioCodec_->encode(micDataEncoded, out, getCodecFrameSize());
    }
}

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    if (getCodecPayloadType() != payloadType)
        return;

    int inSamples = 0;
    size = std::min(size, audioRtpRecord_.decData_.size());
    SFLDataFormat *spkrDataDecoded = audioRtpRecord_.decData_.data();
    {
        ost::MutexLock lock(audioRtpRecord_.audioCodecMutex_);
        // Return the size of data in samples
        inSamples = audioRtpRecord_.audioCodec_->decode(spkrDataDecoded, spkrData, size);
    }

    audioRtpRecord_.fadeInDecodedData(inSamples);

    // Normalize incomming signal
    gainController.process(spkrDataDecoded, inSamples);

    SFLDataFormat *out = spkrDataDecoded;
    int outSamples = inSamples;

    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();

    // test if resampling is required
    if (codecSampleRate != mainBufferSampleRate) {
        out = audioRtpRecord_.resampledData_.data();
        // Do sample rate conversion
        outSamples = ((float) inSamples * ((float) mainBufferSampleRate / (float) codecSampleRate));
        audioRtpRecord_.converter_->resample(spkrDataDecoded, out, codecSampleRate, mainBufferSampleRate, inSamples);
    }

    if (Manager::instance().getEchoCancelState())
        echoCanceller.putData(out, outSamples);

    Manager::instance().getMainBuffer()->putData(out, outSamples * sizeof(SFLDataFormat), id_);
}

void AudioRtpRecord::fadeInDecodedData(size_t size)
{
    // if factor reaches 0, this function should have no effect
    if (fadeFactor_ <= 0 or size > decData_.size())
        return;

    std::transform(decData_.begin(), decData_.begin() + size, decData_.begin(),
            std::bind1st(std::divides<double>(), fadeFactor_));

    // Factor used to increase volume in fade in
    const SFLDataFormat FADEIN_STEP_SIZE = 4;
    fadeFactor_ /= FADEIN_STEP_SIZE;
}
}
