/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@wisdomvibes.com>
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
#include "audio/audiolayer.h"
#include "manager.h"
#include "audio/samplerateconverter.h"
#include "audio/dsp.h"

namespace sfl {

AudioRtpRecord::AudioRtpRecord(const std::string &id) :
    id_(id)
    , encoder_(AudioFormat::MONO)
    , decoder_(AudioFormat::MONO)
    , audioCodecs_()
    , audioCodecMutex_()
    , hasDynamicPayloadType_(false)
    , rawBuffer_(RAW_BUFFER_SIZE, AudioFormat::MONO)
    , encodedData_()
    , fadeFactor_(1.0 / 32000.0)
    , dead_(false)
    , currentCodecIndex_(0)
    , warningInterval_(0)
    , dtmfPayloadType_(101) // same as Asterisk
    , dtmfQueue_()
{}

AudioRtpRecord::~AudioRtpRecord()
{
    dead_ = true;

    delete encoder_.resampler;
    encoder_.resampler = nullptr;
    delete decoder_.resampler;
    decoder_.resampler = nullptr;

    {
        std::lock_guard<std::mutex> lock(audioCodecMutex_);
        deleteCodecs();
    }
}


// Call from processData*
bool AudioRtpRecord::isDead()
{
    return dead_;
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
    for (auto &i : audioCodecs_)
        delete i;

    audioCodecs_.clear();
}

bool AudioRtpRecord::tryToSwitchPayloadTypes(int newPt)
{
    for (std::vector<AudioCodec *>::iterator i = audioCodecs_.begin(); i != audioCodecs_.end(); ++i)
        if (*i and (*i)->getPayloadType() == newPt) {
            AudioFormat f = Manager::instance().getMainBuffer().getInternalAudioFormat();
            (*i)->setOptimalFormat(f.sample_rate, f.nb_channels);
            decoder_.payloadType = (*i)->getPayloadType();
            decoder_.format.sample_rate = (*i)->getClockRate();
            decoder_.frameSize = (*i)->getFrameSize();
            decoder_.format.nb_channels = (*i)->getChannels();
            hasDynamicPayloadType_ = (*i)->hasDynamicPayload();
            // FIXME: this is not reliable
            currentCodecIndex_ = std::distance(audioCodecs_.begin(), i);
            DEBUG("Switched payload type to %d", newPt);
            return true;
        }

    ERROR("Could not switch payload types");
    return false;
}

AudioRtpContext::~AudioRtpContext()
{
#if HAVE_SPEEXDSP
    std::lock_guard<std::mutex> lock(dspMutex);
    delete dsp;
    dsp = 0;
#endif
}

std::string
AudioRtpRecord::getCurrentCodecNames()
{
    std::string result;
    std::lock_guard<std::mutex> lock(audioCodecMutex_);
    {
        std::string sep = "";

        for (auto &i : audioCodecs_) {
            if (i)
                result += sep + i->getMimeSubtype();

            sep = " ";
        }
    }

    return result;
}

void AudioRtpRecord::setRtpMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    std::lock_guard<std::mutex> lock(audioCodecMutex_);

    deleteCodecs();
    // Set various codec info to reduce indirection
    audioCodecs_ = audioCodecs;

    if (audioCodecs.empty()) {
        ERROR("Audio codecs empty");
        return;
    }

    AudioFormat f = Manager::instance().getMainBuffer().getInternalAudioFormat();
    audioCodecs[0]->setOptimalFormat(f.sample_rate, f.nb_channels);
    // FIXME: this should be distinct for encoder and decoder
    currentCodecIndex_ = 0;
    // FIXME: this is probably not the right payload type
    const int pt = audioCodecs[0]->getPayloadType();
    encoder_.payloadType = pt;
    decoder_.payloadType = pt;
    encoder_.format.sample_rate = decoder_.format.sample_rate = audioCodecs[0]->getClockRate();
    encoder_.frameSize = decoder_.frameSize = audioCodecs[0]->getFrameSize();
    encoder_.format.nb_channels = decoder_.format.nb_channels = audioCodecs[0]->getChannels();
    hasDynamicPayloadType_ = audioCodecs[0]->hasDynamicPayload();
}

void AudioRtpContext::resetResampler()
{
    // initialize resampler using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    delete resampler;
    resampler = new SamplerateConverter(format.sample_rate);
}

void AudioRtpRecord::initBuffers()
{
    encoder_.resetResampler();
    decoder_.resetResampler();
}

#if HAVE_SPEEXDSP
void AudioRtpRecord::resetDSP()
{
    encoder_.resetDSP();
    decoder_.resetDSP();
}

void AudioRtpContext::resetDSP()
{
    std::lock_guard<std::mutex> lock(dspMutex);
    delete dsp;
    dsp = new DSP(frameSize, format.nb_channels, format.sample_rate);
}
#endif

void AudioRtpRecord::putDtmfEvent(char digit)
{
    DTMFEvent dtmf(digit);
    dtmfQueue_.push_back(dtmf);
}

#if HAVE_SPEEXDSP
void AudioRtpContext::applyDSP(AudioBuffer &buffer)
{
    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(dspMutex);
        if (!dsp)
            return;

        if (denoise)
            dsp->enableDenoise();
        else
            dsp->disableDenoise();

        if (agc)
            dsp->enableAGC();
        else
            dsp->disableAGC();

        dsp->process(buffer, frameSize);
    }
}
#endif

#define RETURN_IF_NULL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return VAL; }

int AudioRtpRecord::processDataEncode()
{
    if (isDead())
        return 0;

    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();

    double resampleFactor = (double) mainBuffFormat.sample_rate / encoder_.format.sample_rate;

    // compute nb of byte to get corresponding to 1 audio frame
    const size_t samplesToGet = resampleFactor * encoder_.frameSize;

    if (Manager::instance().getMainBuffer().availableForGet(id_) < samplesToGet)
        return 0;

    AudioBuffer micData(samplesToGet, mainBuffFormat);
    const size_t samps = Manager::instance().getMainBuffer().getData(micData, id_);

    if (samps != samplesToGet) {
        ERROR("Asked for %d samples from mainbuffer, got %d", samplesToGet, samps);
        return 0;
    }

    fadeInRawBuffer();

    AudioBuffer *out = &micData;
    if (encoder_.format.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(encoder_.resampler, 0, "Resampler already destroyed");
        encoder_.resampledData.setChannelNum(mainBuffFormat.nb_channels);
        encoder_.resampledData.setSampleRate(encoder_.format.sample_rate);
        encoder_.resampler->resample(micData, encoder_.resampledData);
        out = &encoder_.resampledData;
    }
    if (encoder_.format.nb_channels != mainBuffFormat.nb_channels)
        out->setChannelNum(encoder_.format.nb_channels, true);

#if HAVE_SPEEXDSP
    encoder_.applyDSP(*out);
#endif

    {
        std::lock_guard<std::mutex> lock(audioCodecMutex_);
        RETURN_IF_NULL(getCurrentCodec(), 0, "Audio codec already destroyed");
        unsigned char *micDataEncoded = encodedData_.data();
        int encoded = getCurrentCodec()->encode(micDataEncoded, out->getData(), encoder_.frameSize);
        return encoded;
    }
}
#undef RETURN_IF_NULL

#define RETURN_IF_NULL(A, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return; }

void AudioRtpRecord::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    if (isDead())
        return;

    const int decPt = decoder_.payloadType;
    if (decPt != payloadType) {
        const bool switched = tryToSwitchPayloadTypes(payloadType);

        if (not switched) {
            if (!warningInterval_) {
                warningInterval_ = 250;
                WARN("Invalid payload type %d, expected %d", payloadType, decPt);
            }

            warningInterval_--;
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(audioCodecMutex_);
        RETURN_IF_NULL(getCurrentCodec(), "Audio codecs already destroyed");
        rawBuffer_.setFormat(decoder_.format);
        rawBuffer_.resize(RAW_BUFFER_SIZE);
        int decoded = getCurrentCodec()->decode(rawBuffer_.getData(), spkrData, size);
        rawBuffer_.resize(decoded);
    }

#if HAVE_SPEEXDSP
    decoder_.applyDSP(rawBuffer_);
#endif

    fadeInRawBuffer();

    AudioBuffer *out = &rawBuffer_;

    // test if resampling or up/down-mixing is required
    AudioFormat decFormat = out->getFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
    if (decFormat.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(decoder_.resampler, "Resampler already destroyed");
        decoder_.resampledData.setChannelNum(decFormat.nb_channels);
        decoder_.resampledData.setSampleRate(mainBuffFormat.sample_rate);
        decoder_.resampler->resample(rawBuffer_, decoder_.resampledData);
        out = &decoder_.resampledData;
    }
    if (decFormat.nb_channels != mainBuffFormat.nb_channels)
        out->setChannelNum(mainBuffFormat.nb_channels, true);
    Manager::instance().getMainBuffer().putData(*out, id_);
}
#undef RETURN_IF_NULL

void AudioRtpRecord::fadeInRawBuffer()
{
    // if factor reaches 1, this function should have no effect
    if (fadeFactor_ >= 1.0)
        return;

    rawBuffer_.applyGain(fadeFactor_);

    // Factor used to increase volume in fade in
    const double FADEIN_STEP_SIZE = 4.0;
    fadeFactor_ *= FADEIN_STEP_SIZE;
}

bool
AudioRtpRecord::codecsDiffer(const std::vector<AudioCodec*> &codecs) const
{
    const std::vector<AudioCodec*> &current = audioCodecs_;

    if (codecs.size() != current.size())
        return true;

    for (const auto &i : codecs) {
        if (!i)
            continue;

        bool matched = false;

        for (const auto &j : current)
            if ((matched = i->getPayloadType() == j->getPayloadType()))
                break;

        if (not matched)
            return true;
    }

    return false;
}

}
