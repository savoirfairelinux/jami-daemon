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
#include "sip/sipcall.h"
#include "audio/audiolayer.h"
#include "manager.h"
#include "audio/samplerateconverter.h"
#include "audio/dsp.h"

namespace sfl {

AudioRtpRecord::AudioRtpRecord() :
    callId_("")
    , codecSampleRate_(0)
    , encoder_()
    , decoder_()
    , audioCodecs_()
    , audioCodecMutex_()
    , hasDynamicPayloadType_(false)
    , decData_(DEC_BUFFER_SIZE, AudioFormat::MONO)
    , resampledDataEncode_(0, AudioFormat::MONO)
    , resampledDataDecode_(0, AudioFormat::MONO)
    , encodedData_()
    , converterEncode_(nullptr)
    , converterDecode_(nullptr)
    , codecFrameSize_(0)
    , codecChannels_(0)
    , converterSamplingRate_(0)
    , fadeFactor_(1.0 / 32000.0)
#if HAVE_SPEEXDSP
    , dspEncode_(nullptr)
    , dspDecode_(nullptr)
    , audioProcessMutex_()
#endif
    , dead_(false)
    , currentCodecIndex_(0)
    , warningInterval_(0)
{}

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
            (*i)->setOptimalFormat(f.sample_rate, f.channel_num);
            decoder_.setPayloadType((*i)->getPayloadType());
            codecSampleRate_ = (*i)->getClockRate();
            codecFrameSize_ = (*i)->getFrameSize();
            codecChannels_ = (*i)->getChannels();
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

    delete converterEncode_;
    converterEncode_ = 0;
    delete converterDecode_;
    converterDecode_ = 0;
    {
        std::lock_guard<std::mutex> lock(audioCodecMutex_);
        deleteCodecs();
    }
#if HAVE_SPEEXDSP
    {
        std::lock_guard<std::mutex> lock(audioProcessMutex_);
        delete dspDecode_;
        dspDecode_ = 0;
        delete dspEncode_;
        dspEncode_ = 0;
    }
#endif
}

AudioRtpRecordHandler::AudioRtpRecordHandler(SIPCall &call) :
    audioRtpRecord_(),
    dtmfQueue_(),
    dtmfPayloadType_(101), // same as Asterisk
    id_(call.getCallId())
{}


AudioRtpRecordHandler::~AudioRtpRecordHandler()
{
}

std::string
AudioRtpRecordHandler::getCurrentAudioCodecNames()
{
    return audioRtpRecord_.getCurrentCodecNames();
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

void AudioRtpRecordHandler::setRtpMedia(const std::vector<AudioCodec*> &codecs)
{
    audioRtpRecord_.setRtpMedia(codecs);
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
    audioCodecs[0]->setOptimalFormat(f.sample_rate, f.channel_num);
    // FIXME: this should be distinct for encoder and decoder
    currentCodecIndex_ = 0;
    // FIXME: this is probably not the right payload type
    const int pt = audioCodecs[0]->getPayloadType();
    encoder_.setPayloadType(pt);
    decoder_.setPayloadType(pt);
    codecSampleRate_ = audioCodecs[0]->getClockRate();
    codecFrameSize_ = audioCodecs[0]->getFrameSize();
    codecChannels_ = audioCodecs[0]->getChannels();
    hasDynamicPayloadType_ = audioCodecs[0]->hasDynamicPayload();
}

void AudioRtpRecordHandler::initBuffers()
{
    audioRtpRecord_.initBuffers();
}

void AudioRtpRecord::initBuffers()
{
    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    delete converterEncode_;
    converterEncode_ = new SamplerateConverter(codecSampleRate_);
    delete converterDecode_;
    converterDecode_ = new SamplerateConverter(codecSampleRate_);
}

#if HAVE_SPEEXDSP
void AudioRtpRecordHandler::initDSP()
{
    audioRtpRecord_.initDSP();
}

void AudioRtpRecord::initDSP()
{
    std::lock_guard<std::mutex> lock(audioProcessMutex_);
    delete dspEncode_;
    dspEncode_ = new DSP(codecFrameSize_, codecChannels_, codecSampleRate_);
    delete dspDecode_;
    dspDecode_ = new DSP(codecFrameSize_, codecChannels_, codecSampleRate_);
}
#endif

void AudioRtpRecordHandler::putDtmfEvent(char digit)
{
    DTMFEvent dtmf(digit);
    dtmfQueue_.push_back(dtmf);
}

int AudioRtpRecordHandler::processDataEncode()
{
    return audioRtpRecord_.processDataEncode(id_);
}

#define RETURN_IF_NULL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return VAL; }

int AudioRtpRecord::processDataEncode(const std::string &id)
{
    if (isDead())
        return 0;

    AudioFormat codecFormat = getCodecFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();

    double resampleFactor = (double) mainBuffFormat.sample_rate / codecFormat.sample_rate;

    // compute nb of byte to get corresponding to 1 audio frame
    const size_t samplesToGet = resampleFactor * codecFrameSize_;

    if (Manager::instance().getMainBuffer().availableForGet(id) < samplesToGet)
        return 0;

    AudioBuffer micData(samplesToGet, mainBuffFormat);
    const size_t samps = Manager::instance().getMainBuffer().getData(micData, id);

    if (samps != samplesToGet) {
        ERROR("Asked for %d samples from mainbuffer, got %d", samplesToGet, samps);
        return 0;
    }

    fadeInDecodedData();

    AudioBuffer *out = &micData;
    if (codecFormat.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(converterEncode_, 0, "Converter already destroyed");
        resampledDataEncode_.setChannelNum(mainBuffFormat.channel_num);
        resampledDataEncode_.setSampleRate(codecFormat.sample_rate);
        converterEncode_->resample(micData, resampledDataEncode_);
        out = &resampledDataEncode_;
    }
    if (codecFormat.channel_num != mainBuffFormat.channel_num)
        out->setChannelNum(codecFormat.channel_num, true);

#if HAVE_SPEEXDSP
    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(audioProcessMutex_);
        RETURN_IF_NULL(dspEncode_, 0, "DSP already destroyed");
        if (denoise)
            dspEncode_->enableDenoise();
        else
            dspEncode_->disableDenoise();

        if (agc)
            dspEncode_->enableAGC();
        else
            dspEncode_->disableAGC();

        dspEncode_->process(*out, codecFrameSize_);
    }
#endif

    {
        std::lock_guard<std::mutex> lock(audioCodecMutex_);
        RETURN_IF_NULL(getCurrentCodec(), 0, "Audio codec already destroyed");
        unsigned char *micDataEncoded = encodedData_.data();
        int encoded = getCurrentCodec()->encode(micDataEncoded, out->getData(), codecFrameSize_);
        return encoded;
    }
}
#undef RETURN_IF_NULL

#define RETURN_IF_NULL(A, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return; }

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    audioRtpRecord_.processDataDecode(spkrData, size, payloadType, id_);
}

void AudioRtpRecord::processDataDecode(unsigned char *spkrData, size_t size, int payloadType, const std::string &id)
{
    if (isDead())
        return;

    const int decPt = decoder_.getPayloadType();
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
        decData_.setFormat(getCodecFormat());
        decData_.resize(DEC_BUFFER_SIZE);
        int decoded = getCurrentCodec()->decode(decData_.getData(), spkrData, size);
        decData_.resize(decoded);
    }

#if HAVE_SPEEXDSP

    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(audioProcessMutex_);
        RETURN_IF_NULL(dspDecode_, "DSP already destroyed");
        if (denoise)
            dspDecode_->enableDenoise();
        else
            dspDecode_->disableDenoise();

        if (agc)
            dspDecode_->enableAGC();
        else
            dspDecode_->disableAGC();
        dspDecode_->process(decData_, codecFrameSize_);
    }

#endif

    fadeInDecodedData();

    AudioBuffer *out = &decData_;

    // test if resampling or up/down-mixing is required
    AudioFormat decFormat = out->getFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
    if (decFormat.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(converterDecode_, "Converter already destroyed");
        resampledDataDecode_.setChannelNum(decFormat.channel_num);
        resampledDataDecode_.setSampleRate(mainBuffFormat.sample_rate);
        //WARN("Resample %s->%s", audioRtpRecord_.decData_.toString().c_str(), audioRtpRecord_.resampledDataDecode_.toString().c_str());
        converterDecode_->resample(decData_, resampledDataDecode_);
        out = &resampledDataDecode_;
    }
    if (decFormat.channel_num != mainBuffFormat.channel_num)
        out->setChannelNum(mainBuffFormat.channel_num, true);
    Manager::instance().getMainBuffer().putData(*out, id);
}
#undef RETURN_IF_NULL

void AudioRtpRecord::fadeInDecodedData()
{
    // if factor reaches 1, this function should have no effect
    if (fadeFactor_ >= 1.0)
        return;

    decData_.applyGain(fadeFactor_);

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
