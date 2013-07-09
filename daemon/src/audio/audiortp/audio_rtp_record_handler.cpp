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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio_rtp_record_handler.h"
#include <fstream>
#include <algorithm>
#include "logger.h"
#include "sip/sipcall.h"
#include "audio/audiolayer.h"
#include "scoped_lock.h"
#include "manager.h"

namespace sfl {

#ifdef RECTODISK
std::ofstream rtpResampled("testRtpOutputResampled.raw", std::ifstream::binary);
std::ofstream rtpNotResampled("testRtpOutput.raw", std::ifstream::binary);
#endif

DTMFEvent::DTMFEvent(char digit) : payload(), newevent(true), length(1000)
{
    /*
       From RFC2833:

       Event  encoding (decimal)
       _________________________
       0--9                0--9
       *                     10
       #                     11
       A--D              12--15
       Flash                 16
    */

    switch (digit) {
        case '*':
            digit = 10;
            break;

        case '#':
            digit = 11;
            break;

        case 'A' ... 'D':
            digit = digit - 'A' + 12;
            break;

        case '0' ... '9':
            digit = digit - '0';
            break;

        default:
            ERROR("Unexpected DTMF %c", digit);
    }

    payload.event = digit;
    payload.ebit = false; // end of event bit
    payload.rbit = false; // reserved bit
    payload.duration = 1; // duration for this event
}

AudioRtpRecord::AudioRtpRecord() :
    callId_("")
    , codecSampleRate_(0)
    , dtmfQueue_()
    , audioCodecs_()
    , audioCodecMutex_()
    , encoderPayloadType_(0)
    , decoderPayloadType_(0)
    , hasDynamicPayloadType_(false)
    , decData_()     // std::tr1::arrays will be 0-initialized
    , resampledData_()
    , encodedData_()
    , converterEncode_(0)
    , converterDecode_(0)
    , codecFrameSize_(0)
    , converterSamplingRate_(0)
    , fadeFactor_(1.0 / 32000.0)
#if HAVE_SPEEXDSP
    , noiseSuppressEncode_(0)
    , noiseSuppressDecode_(0)
    , audioProcessMutex_()
#endif
    , dtmfPayloadType_(101) // same as Asterisk
    , dead_(false)
    , currentCodecIndex_(0)
{
    pthread_mutex_init(&audioCodecMutex_, NULL);
#if HAVE_SPEEXDSP
    pthread_mutex_init(&audioProcessMutex_, NULL);
#endif
}

// Call from processData*
bool AudioRtpRecord::isDead()
{
#ifdef CCPP_PREFIX
    return (int) dead_;
#else
    return *dead_;
#endif
}

sfl::AudioCodec *
AudioRtpRecord::getCurrentCodec() const
{
    if (audioCodecs_.empty() or currentCodecIndex_ >= audioCodecs_.size()) {
        ERROR("No codec found");
        return 0;
    }

    return audioCodecs_[currentCodecIndex_];
}

void
AudioRtpRecord::deleteCodecs()
{
    for (std::vector<AudioCodec *>::iterator i = audioCodecs_.begin(); i != audioCodecs_.end(); ++i)
        delete *i;

    audioCodecs_.clear();
}

bool AudioRtpRecord::tryToSwitchPayloadTypes(int newPt)
{
    for (std::vector<AudioCodec *>::iterator i = audioCodecs_.begin(); i != audioCodecs_.end(); ++i)
        if (*i and (*i)->getPayloadType() == newPt) {
            decoderPayloadType_ = (*i)->getPayloadType();
            codecSampleRate_ = (*i)->getClockRate();
            codecFrameSize_ = (*i)->getFrameSize();
            hasDynamicPayloadType_ = (*i)->hasDynamicPayload();
            currentCodecIndex_ = std::distance(audioCodecs_.begin(), i);
            DEBUG("Switched payload type to %d", newPt);
            return true;
        }

    ERROR("Could not switch payload types");
    return false;
}

AudioRtpRecord::~AudioRtpRecord()
{
    dead_ = true;
#ifdef RECTODISK
    rtpResampled.close();
    rtpNotResampled.close();
#endif

    delete converterEncode_;
    converterEncode_ = 0;
    delete converterDecode_;
    converterDecode_ = 0;
    {
        ScopedLock lock(audioCodecMutex_);
        deleteCodecs();
    }
#if HAVE_SPEEXDSP
    {
        ScopedLock lock(audioProcessMutex_);
        delete noiseSuppressDecode_;
        noiseSuppressDecode_ = 0;
        delete noiseSuppressEncode_;
        noiseSuppressEncode_ = 0;
    }
#endif
    pthread_mutex_destroy(&audioCodecMutex_);

#if HAVE_SPEEXDSP
    pthread_mutex_destroy(&audioProcessMutex_);
#endif
}


AudioRtpRecordHandler::AudioRtpRecordHandler(SIPCall &call) :
    audioRtpRecord_(),
    id_(call.getCallId()),
    gainController_(8000, -10.0),
    warningInterval_(0)
{}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {}

std::string
AudioRtpRecordHandler::getCurrentAudioCodecNames()
{
    std::string result;
    ScopedLock lock(audioRtpRecord_.audioCodecMutex_);
    {
        std::string sep = "";

        for (std::vector<AudioCodec*>::const_iterator i = audioRtpRecord_.audioCodecs_.begin();
                i != audioRtpRecord_.audioCodecs_.end(); ++i) {
            if (*i)
                result += sep + (*i)->getMimeSubtype();

            sep = " ";
        }
    }

    return result;
}

void AudioRtpRecordHandler::setRtpMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    ScopedLock lock(audioRtpRecord_.audioCodecMutex_);

    audioRtpRecord_.deleteCodecs();
    // Set various codec info to reduce indirection
    audioRtpRecord_.audioCodecs_ = audioCodecs;

    if (audioCodecs.empty()) {
        ERROR("Audio codecs empty");
        return;
    }

    audioRtpRecord_.currentCodecIndex_ = 0;
    audioRtpRecord_.encoderPayloadType_ = audioRtpRecord_.decoderPayloadType_ = audioCodecs[0]->getPayloadType();
    audioRtpRecord_.codecSampleRate_ = audioCodecs[0]->getClockRate();
    audioRtpRecord_.codecFrameSize_ = audioCodecs[0]->getFrameSize();
    audioRtpRecord_.hasDynamicPayloadType_ = audioCodecs[0]->hasDynamicPayload();
}

void AudioRtpRecordHandler::initBuffers()
{
    // Set sampling rate, main buffer choose the highest one
    Manager::instance().audioSamplingRateChanged(audioRtpRecord_.codecSampleRate_);

    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    delete audioRtpRecord_.converterEncode_;
    audioRtpRecord_.converterEncode_ = new SamplerateConverter(getCodecSampleRate());
    delete audioRtpRecord_.converterDecode_;
    audioRtpRecord_.converterDecode_ = new SamplerateConverter(getCodecSampleRate());
}

#if HAVE_SPEEXDSP
void AudioRtpRecordHandler::initNoiseSuppress()
{
    ScopedLock lock(audioRtpRecord_.audioProcessMutex_);
    delete audioRtpRecord_.noiseSuppressEncode_;
    audioRtpRecord_.noiseSuppressEncode_ = new NoiseSuppress(getCodecFrameSize(), getCodecSampleRate());
    delete audioRtpRecord_.noiseSuppressDecode_;
    audioRtpRecord_.noiseSuppressDecode_ = new NoiseSuppress(getCodecFrameSize(), getCodecSampleRate());
}
#endif

void AudioRtpRecordHandler::putDtmfEvent(char digit)
{
    DTMFEvent dtmf(digit);
    audioRtpRecord_.dtmfQueue_.push_back(dtmf);
}

#define RETURN_IF_NULL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return VAL; }

int AudioRtpRecordHandler::processDataEncode()
{
    if (audioRtpRecord_.isDead())
        return 0;

    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();

    double resampleFactor = (double) mainBufferSampleRate / codecSampleRate;

    // compute nb of byte to get coresponding to 1 audio frame
    int samplesToGet = resampleFactor * getCodecFrameSize();
    const size_t bytesToGet = samplesToGet * sizeof(SFLDataFormat);

    if (Manager::instance().getMainBuffer().availableForGet(id_) < bytesToGet)
        return 0;

    SFLDataFormat *micData = audioRtpRecord_.decData_.data();
    const size_t bytes = Manager::instance().getMainBuffer().getData(micData, bytesToGet, id_);

#ifdef RECTODISK
    rtpNotResampled.write((const char *)micData, bytes);
#endif

    if (bytes != bytesToGet) {
        ERROR("Asked for %d bytes from mainbuffer, got %d", bytesToGet, bytes);
        return 0;
    }

    int samples = bytesToGet / sizeof(SFLDataFormat);

    audioRtpRecord_.fadeInDecodedData(samples);

    SFLDataFormat *out = micData;

    if (codecSampleRate != mainBufferSampleRate) {
        RETURN_IF_NULL(audioRtpRecord_.converterEncode_, 0, "Converter already destroyed");

        audioRtpRecord_.converterEncode_->resample(micData,
                audioRtpRecord_.resampledData_.data(),
                audioRtpRecord_.resampledData_.size(),
                mainBufferSampleRate, codecSampleRate,
                samplesToGet);

#ifdef RECTODISK
        rtpResampled.write((const char *)audioRtpRecord_.resampledData_.data(), samplesToGet*sizeof(SFLDataFormat)/2);
#endif

        out = audioRtpRecord_.resampledData_.data();
    }

#if HAVE_SPEEXDSP

    if (Manager::instance().audioPreference.getNoiseReduce()) {
        ScopedLock lock(audioRtpRecord_.audioProcessMutex_);
        RETURN_IF_NULL(audioRtpRecord_.noiseSuppressEncode_, 0, "Noise suppressor already destroyed");
        audioRtpRecord_.noiseSuppressEncode_->process(micData, getCodecFrameSize());
    }

#endif

    {
        ScopedLock lock(audioRtpRecord_.audioCodecMutex_);
        RETURN_IF_NULL(audioRtpRecord_.getCurrentCodec(), 0, "Audio codec already destroyed");
        unsigned char *micDataEncoded = audioRtpRecord_.encodedData_.data();
        return audioRtpRecord_.getCurrentCodec()->encode(micDataEncoded, out, getCodecFrameSize());
    }
}
#undef RETURN_IF_NULL

#define RETURN_IF_NULL(A, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return; }

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    if (audioRtpRecord_.isDead())
        return;

    if (audioRtpRecord_.decoderPayloadType_ != payloadType) {
        const bool switched = audioRtpRecord_.tryToSwitchPayloadTypes(payloadType);

        if (not switched) {
            if (!warningInterval_) {
                warningInterval_ = 250;
                WARN("Invalid payload type %d, expected %d", payloadType, audioRtpRecord_.decoderPayloadType_);
            }

            warningInterval_--;
            return;
        }
    }

    int inSamples = 0;
    size = std::min(size, audioRtpRecord_.decData_.size());
    SFLDataFormat *spkrDataDecoded = audioRtpRecord_.decData_.data();
    {
        ScopedLock lock(audioRtpRecord_.audioCodecMutex_);
        RETURN_IF_NULL(audioRtpRecord_.getCurrentCodec(), "Audio codecs already destroyed");
        // Return the size of data in samples
        inSamples = audioRtpRecord_.getCurrentCodec()->decode(spkrDataDecoded, spkrData, size);
    }

#if HAVE_SPEEXDSP

    if (Manager::instance().audioPreference.getNoiseReduce()) {
        ScopedLock lock(audioRtpRecord_.audioProcessMutex_);
        RETURN_IF_NULL(audioRtpRecord_.noiseSuppressDecode_, "Noise suppressor already destroyed");
        audioRtpRecord_.noiseSuppressDecode_->process(spkrDataDecoded, getCodecFrameSize());
    }

#endif

    audioRtpRecord_.fadeInDecodedData(inSamples);

    // Normalize incomming signal
    gainController_.process(spkrDataDecoded, inSamples);

    SFLDataFormat *out = spkrDataDecoded;
    int outSamples = inSamples;

    int codecSampleRate = getCodecSampleRate();
    int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();

    // test if resampling is required
    if (codecSampleRate != mainBufferSampleRate) {
        RETURN_IF_NULL(audioRtpRecord_.converterDecode_, "Converter already destroyed");
        out = audioRtpRecord_.resampledData_.data();
        // Do sample rate conversion
        outSamples = ((float) inSamples * ((float) mainBufferSampleRate / (float) codecSampleRate));
        audioRtpRecord_.converterDecode_->resample(spkrDataDecoded, out,
                audioRtpRecord_.resampledData_.size(), codecSampleRate,
                mainBufferSampleRate, inSamples);
    }

    Manager::instance().getMainBuffer().putData(out, outSamples * sizeof(SFLDataFormat), id_);
}
#undef RETURN_IF_NULL

void AudioRtpRecord::fadeInDecodedData(size_t size)
{
    // if factor reaches 1, this function should have no effect
    if (fadeFactor_ >= 1.0 or size > decData_.size())
        return;

    for (size_t i = 0; i < size; ++i)
        decData_[i] *= fadeFactor_;

    // Factor used to increase volume in fade in
    const double FADEIN_STEP_SIZE = 4.0;
    fadeFactor_ *= FADEIN_STEP_SIZE;
}

bool
AudioRtpRecordHandler::codecsDiffer(const std::vector<AudioCodec*> &codecs) const
{
    const std::vector<AudioCodec*> &current = audioRtpRecord_.audioCodecs_;

    if (codecs.size() != current.size())
        return true;

    for (std::vector<AudioCodec*>::const_iterator i = codecs.begin(); i != codecs.end(); ++i) {
        if (*i) {
            bool matched = false;

            for (std::vector<AudioCodec*>::const_iterator j = current.begin(); !matched and j != current.end(); ++j)
                matched = (*i)->getPayloadType() == (*j)->getPayloadType();

            if (not matched)
                return true;
        }
    }

    return false;
}

}
