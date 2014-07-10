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

#include "audio_rtp_stream.h"

#include "audio/audiolayer.h"
#include "audio/resampler.h"

#if HAVE_SPEEXDSP
#include "audio/dsp.h"
#endif

#include "manager.h"
#include "logger.h"

#include <fstream>
#include <algorithm>
#include <cassert>

namespace sfl {

AudioRtpStream::AudioRtpStream(const std::string &id) :
    id_(id)
    , encoder_(AudioFormat::MONO)
    , decoder_(AudioFormat::MONO)
    , audioCodecs_()
    , codecEncMutex_(), codecDecMutex_()
    , hasDynamicPayloadType_(false)
    , rawBuffer_(RAW_BUFFER_SIZE, AudioFormat::MONO)
    , micData_(RAW_BUFFER_SIZE, AudioFormat::MONO)
    , encodedData_()
    , dead_(false)
    , currentEncoderIndex_(0)
    , currentDecoderIndex_(0)
    , warningInterval_(0)
    , plcCachePool_()
    , plcPool_(nullptr)
    , plcDec_()
{
    pj_caching_pool_init(&plcCachePool_, &pj_pool_factory_default_policy, 0);
    plcPool_ = pj_pool_create(&plcCachePool_.factory, "plc", 64, 1024, nullptr);
}

AudioRtpStream::~AudioRtpStream()
{
    dead_ = true;
    std::lock(codecEncMutex_, codecDecMutex_);
    deleteCodecs();
    codecEncMutex_.unlock();
    codecDecMutex_.unlock();
    plcDec_.clear();
    pj_pool_release(plcPool_);
    pj_caching_pool_destroy(&plcCachePool_);
}

// Call from processData*
bool AudioRtpStream::isDead()
{
    return dead_;
}

sfl::AudioCodec *
AudioRtpStream::getCurrentEncoder() const
{
    if (audioCodecs_.empty() or currentEncoderIndex_ >= audioCodecs_.size()) {
        ERROR("No codec found");
        return nullptr;
    }

    return audioCodecs_[currentEncoderIndex_];
}

sfl::AudioCodec *
AudioRtpStream::getCurrentDecoder() const
{
    if (audioCodecs_.empty() or currentDecoderIndex_ >= audioCodecs_.size()) {
        ERROR("No codec found");
        return nullptr;
    }

    return audioCodecs_[currentDecoderIndex_];
}

void
AudioRtpStream::deleteCodecs()
{
    for (auto &i : audioCodecs_)
        delete i;

    audioCodecs_.clear();
}

// Tries to change decoder (only) if we receive an unexpected payload type that we previously said
// we could decode.
bool AudioRtpStream::tryToSwitchDecoder(int newPt)
{
    std::lock_guard<std::mutex> lock(codecDecMutex_);
    for (unsigned i=0, n=audioCodecs_.size(); i<n; i++) {
        auto codec = audioCodecs_[i];
        if (codec == nullptr || codec->getPayloadType() != newPt) continue;
        AudioFormat f = Manager::instance().getMainBuffer().getInternalAudioFormat();
        codec->setOptimalFormat(f.sample_rate, f.nb_channels);
        decoder_.payloadType_ = codec->getPayloadType();
        decoder_.frameSize_ = codec->getFrameSize();
        decoder_.format_.sample_rate = codec->getCurrentClockRate();
        decoder_.format_.nb_channels = codec->getCurrentChannels();
        hasDynamicPayloadType_ = codec->hasDynamicPayload();
        resetDecoderPLC(codec);
        currentDecoderIndex_ = i; // FIXME: this is not reliable
        DEBUG("Switched payload type to %d", newPt);
        return true;
    }
    ERROR("Could not switch payload types");
    return false;
}

void AudioRtpStream::resetDecoderPLC(const sfl::AudioCodec * codec)
{
    if (!plcPool_) return;
    pj_pool_reset(plcPool_);
    plcDec_.clear();
    if (not codec->supportsPacketLossConcealment()) {
        plcDec_.insert(plcDec_.begin(), decoder_.format_.nb_channels, nullptr);
        for (unsigned i = 0; i < decoder_.format_.nb_channels; i++)
            pjmedia_plc_create(plcPool_, decoder_.format_.sample_rate, decoder_.frameSize_, 0, &plcDec_[i]);
    }
}

AudioRtpContext::AudioRtpContext(AudioFormat f) :
    fadeFactor_(0.)
    , payloadType_(0)
    , frameSize_(0)
    , format_(f)
    , resampledData_(0, AudioFormat::MONO)
    , resampler_(nullptr)
#if HAVE_SPEEXDSP
    , dspMutex_()
    , dsp_()
#endif
{}

AudioRtpContext::~AudioRtpContext()
{
#if HAVE_SPEEXDSP
    std::lock_guard<std::mutex> lock(dspMutex_);
    dsp_.reset(nullptr);
#endif
}

void AudioRtpContext::resetResampler()
{
    // initialize resampler using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    resampler_.reset(new Resampler(format_.sample_rate));
}

void AudioRtpContext::fadeIn(AudioBuffer& buf)
{
    if (fadeFactor_ >= 1.0)
        return;
    // http://en.wikipedia.org/wiki/Smoothstep
    const double gain = fadeFactor_ * fadeFactor_ * (3. - 2. * fadeFactor_);
    buf.applyGain(gain);
    fadeFactor_ += buf.size() / (double) format_.sample_rate;
}

#if HAVE_SPEEXDSP
void AudioRtpContext::resetDSP()
{
    std::lock_guard<std::mutex> lock(dspMutex_);
    assert(frameSize_);
    dsp_.reset(new DSP(frameSize_, format_.nb_channels, format_.sample_rate));
}

void AudioRtpContext::applyDSP(AudioBuffer &buffer)
{
    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(dspMutex_);
        if (!dsp_)
            return;

        if (denoise)
            dsp_->enableDenoise();
        else
            dsp_->disableDenoise();

        if (agc)
            dsp_->enableAGC();
        else
            dsp_->disableAGC();

        dsp_->process(buffer, frameSize_);
    }
}
#endif


void AudioRtpStream::setRtpMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    std::lock(codecEncMutex_, codecDecMutex_);

    deleteCodecs();
    // Set various codec info to reduce indirection
    audioCodecs_ = audioCodecs;

    if (audioCodecs.empty()) {
        codecEncMutex_.unlock();
        codecDecMutex_.unlock();
        ERROR("Audio codecs empty");
        return;
    }

    // FIXME: assuming right encoder/decoder are first?
    currentEncoderIndex_ = currentDecoderIndex_ = 0;
    AudioCodec& codec = *audioCodecs[currentEncoderIndex_];

    AudioFormat f = Manager::instance().getMainBuffer().getInternalAudioFormat();
    codec.setOptimalFormat(f.sample_rate, f.nb_channels);

    const int pt = codec.getPayloadType();
    encoder_.payloadType_ = decoder_.payloadType_ = pt;
    encoder_.frameSize_ = decoder_.frameSize_ = codec.getFrameSize();

    AudioFormat codecFormat(codec.getCurrentClockRate(), codec.getCurrentChannels());
    if (codecFormat != decoder_.format_ or codecFormat != encoder_.format_) {
        encoder_.format_ = decoder_.format_ = codecFormat;
#if HAVE_SPEEXDSP
        resetDSP();
#endif
    }
    Manager::instance().audioFormatUsed(codecFormat);
    hasDynamicPayloadType_ = codec.hasDynamicPayload();
    codecEncMutex_.unlock();

    resetDecoderPLC(audioCodecs[currentDecoderIndex_]);
    codecDecMutex_.unlock();
}

void AudioRtpStream::initBuffers()
{
    encoder_.resetResampler();
    decoder_.resetResampler();
}

#if HAVE_SPEEXDSP
void AudioRtpStream::resetDSP()
{
    encoder_.resetDSP();
    decoder_.resetDSP();
}
#endif

bool AudioRtpStream::waitForDataEncode(const std::chrono::milliseconds& max_wait) const
{
    const auto &mainBuffer = Manager::instance().getMainBuffer();
    const AudioFormat mainBuffFormat = mainBuffer.getInternalAudioFormat();
    const double resampleFactor = (double) mainBuffFormat.sample_rate / encoder_.format_.sample_rate;
    const size_t samplesToGet = resampleFactor * encoder_.frameSize_;

    return mainBuffer.waitForDataAvailable(id_, samplesToGet, max_wait);
}

size_t AudioRtpStream::processDataEncode()
{
    if (isDead())
        return 0;

    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();

    double resampleFactor = (double) mainBuffFormat.sample_rate / encoder_.format_.sample_rate;

    // compute nb of byte to get corresponding to 1 audio frame
    const size_t samplesToGet = resampleFactor * encoder_.frameSize_;

    if (Manager::instance().getMainBuffer().availableForGet(id_) < samplesToGet)
        return 0;

    micData_.setFormat(mainBuffFormat);
    micData_.resize(samplesToGet);
    const size_t samples = Manager::instance().getMainBuffer().getData(micData_, id_);

    if (samples != samplesToGet) {
        ERROR("Asked for %d samples from mainbuffer, got %d", samplesToGet, samples);
        return 0;
    }

    AudioBuffer *out = &micData_;
    if (encoder_.format_.sample_rate != mainBuffFormat.sample_rate) {
        if (!encoder_.resampler_) {
            ERROR("Resampler already destroyed");
            return 0;
        }
        encoder_.resampledData_.setChannelNum(mainBuffFormat.nb_channels);
        encoder_.resampledData_.setSampleRate(encoder_.format_.sample_rate);
        encoder_.resampler_->resample(micData_, encoder_.resampledData_);
        out = &encoder_.resampledData_;
    }
    if (encoder_.format_.nb_channels != mainBuffFormat.nb_channels)
        out->setChannelNum(encoder_.format_.nb_channels, true);

    encoder_.fadeIn(*out);
#if HAVE_SPEEXDSP
    encoder_.applyDSP(*out);
#endif
    {
        std::lock_guard<std::mutex> lock(codecEncMutex_);
        auto codec = getCurrentEncoder();
        if (!codec) {
            ERROR("Audio codec already destroyed");
            return 0;
        }

        const auto codecFrameSize = codec->getFrameSize();
        if (codecFrameSize > out->frames()) {
            // PCM too small (underflow), add zero padding to avoid reading past
            // end of buffer when encoding, for every channel
            out->resize(codecFrameSize);
        }

        size_t encoded = codec->encode(out->getData(), encodedData_.data(), encodedData_.size());
        return encoded;
    }
}


void AudioRtpStream::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    if (isDead())
        return;

    const int decPt = decoder_.payloadType_;
    if (decPt != payloadType) {
        const bool switched = tryToSwitchDecoder(payloadType);

        if (not switched) {
            if (!warningInterval_) {
                warningInterval_ = 250;
                WARN("Invalid payload type %d, expected %d", payloadType, decPt);
            }

            warningInterval_--;
            return;
        }
    }

    rawBuffer_.setFormat(decoder_.format_);
    rawBuffer_.resize(RAW_BUFFER_SIZE);
    {
        std::lock_guard<std::mutex> lock(codecDecMutex_);
        auto codec = getCurrentDecoder();
        if (!codec) {
            ERROR("Audio codec already destroyed");
            return;
        }
        if (spkrData) { // Packet is available
            int decoded = codec->decode(rawBuffer_.getData(), spkrData, size);
            rawBuffer_.resize(decoded);
            if (not plcDec_.empty()) {
                for (unsigned i = 0; i < decoder_.format_.nb_channels; ++i) {
                    pjmedia_plc_save(plcDec_[i], rawBuffer_.getChannel(i)->data());
                }
            }
        } else if (plcDec_.empty()) { // Packet loss concealment using codec
            int decoded = codec->decode(rawBuffer_.getData());
            rawBuffer_.resize(decoded);
        } else { // Generic PJSIP Packet loss concealment using codec
            rawBuffer_.resize(decoder_.frameSize_);
            for (unsigned i = 0; i < decoder_.format_.nb_channels; ++i) {
                pjmedia_plc_generate(plcDec_[i], rawBuffer_.getChannel(i)->data());
            }
        }
    }

#if HAVE_SPEEXDSP
    decoder_.applyDSP(rawBuffer_);
#endif

    decoder_.fadeIn(rawBuffer_);

    // test if resampling or up/down-mixing is required
    AudioBuffer *out = &rawBuffer_;
    AudioFormat decFormat = out->getFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
    if (decFormat.sample_rate != mainBuffFormat.sample_rate) {
        if (!decoder_.resampler_) {
            ERROR("Resampler already destroyed");
            return;
        }
        decoder_.resampledData_.setChannelNum(decFormat.nb_channels);
        decoder_.resampledData_.setSampleRate(mainBuffFormat.sample_rate);
        decoder_.resampler_->resample(rawBuffer_, decoder_.resampledData_);
        out = &decoder_.resampledData_;
    }
    if (decFormat.nb_channels != mainBuffFormat.nb_channels)
        out->setChannelNum(mainBuffFormat.nb_channels, true);
    Manager::instance().getMainBuffer().putData(*out, id_);
}

bool
AudioRtpStream::codecsDiffer(const std::vector<AudioCodec*> &codecs) const
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

int
AudioRtpStream::getEncoderPayloadType() const
{
    return encoder_.payloadType_;
}

int
AudioRtpStream::getEncoderFrameSize() const
{
    return encoder_.frameSize_;
}


int
AudioRtpStream::getTransportRate() const
{
    const int transportRate = encoder_.frameSize_ / encoder_.format_.sample_rate / 1000;
    return transportRate > 0 ? transportRate : 20;
}

}
