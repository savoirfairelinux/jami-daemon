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
    id_(call.getCallId()),
    warningInterval_(0)
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

void AudioRtpRecordHandler::setRtpMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    std::lock_guard<std::mutex> lock(audioRtpRecord_.audioCodecMutex_);

    audioRtpRecord_.deleteCodecs();
    // Set various codec info to reduce indirection
    audioRtpRecord_.audioCodecs_ = audioCodecs;

    if (audioCodecs.empty()) {
        ERROR("Audio codecs empty");
        return;
    }

    AudioFormat f = Manager::instance().getMainBuffer().getInternalAudioFormat();
    audioCodecs[0]->setOptimalFormat(f.sample_rate, f.channel_num);
    audioRtpRecord_.currentCodecIndex_ = 0;
    // FIXME: this is probably not the right payload type
    const int pt = audioCodecs[0]->getPayloadType();
    audioRtpRecord_.encoder_.setPayloadType(pt);
    audioRtpRecord_.decoder_.setPayloadType(pt);
    audioRtpRecord_.codecSampleRate_ = audioCodecs[0]->getClockRate();
    audioRtpRecord_.codecFrameSize_ = audioCodecs[0]->getFrameSize();
    audioRtpRecord_.codecChannels_ = audioCodecs[0]->getChannels();
    audioRtpRecord_.hasDynamicPayloadType_ = audioCodecs[0]->hasDynamicPayload();
}

void AudioRtpRecordHandler::initBuffers()
{
    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    delete audioRtpRecord_.converterEncode_;
    audioRtpRecord_.converterEncode_ = new SamplerateConverter(getCodecSampleRate());
    delete audioRtpRecord_.converterDecode_;
    audioRtpRecord_.converterDecode_ = new SamplerateConverter(getCodecSampleRate());
}

#if HAVE_SPEEXDSP
void AudioRtpRecordHandler::initDSP()
{
    std::lock_guard<std::mutex> lock(audioRtpRecord_.audioProcessMutex_);
    delete audioRtpRecord_.dspEncode_;
    audioRtpRecord_.dspEncode_ = new DSP(getCodecFrameSize(), getCodecChannels(), getCodecSampleRate());
    delete audioRtpRecord_.dspDecode_;
    audioRtpRecord_.dspDecode_ = new DSP(getCodecFrameSize(), getCodecChannels(), getCodecSampleRate());
}
#endif

void AudioRtpRecordHandler::putDtmfEvent(char digit)
{
    DTMFEvent dtmf(digit);
    dtmfQueue_.push_back(dtmf);
}

#define RETURN_IF_NULL(A, VAL, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return VAL; }

int AudioRtpRecordHandler::processDataEncode()
{
    if (audioRtpRecord_.isDead())
        return 0;

    AudioFormat codecFormat = getCodecFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();

    double resampleFactor = (double) mainBuffFormat.sample_rate / codecFormat.sample_rate;

    // compute nb of byte to get corresponding to 1 audio frame
    const size_t samplesToGet = resampleFactor * getCodecFrameSize();

    if (Manager::instance().getMainBuffer().availableForGet(id_) < samplesToGet)
        return 0;

    AudioBuffer micData(samplesToGet, mainBuffFormat);
    const size_t samps = Manager::instance().getMainBuffer().getData(micData, id_);

    if (samps != samplesToGet) {
        ERROR("Asked for %d samples from mainbuffer, got %d", samplesToGet, samps);
        return 0;
    }

    audioRtpRecord_.fadeInDecodedData();

    AudioBuffer *out = &micData;
    if (codecFormat.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(audioRtpRecord_.converterEncode_, 0, "Converter already destroyed");
        audioRtpRecord_.resampledDataEncode_.setChannelNum(mainBuffFormat.channel_num);
        audioRtpRecord_.resampledDataEncode_.setSampleRate(codecFormat.sample_rate);
        audioRtpRecord_.converterEncode_->resample(micData, audioRtpRecord_.resampledDataEncode_);
        out = &(audioRtpRecord_.resampledDataEncode_);
    }
    if (codecFormat.channel_num != mainBuffFormat.channel_num)
        out->setChannelNum(codecFormat.channel_num, true);

#if HAVE_SPEEXDSP
    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(audioRtpRecord_.audioProcessMutex_);
        RETURN_IF_NULL(audioRtpRecord_.dspEncode_, 0, "DSP already destroyed");
        if (denoise)
            audioRtpRecord_.dspEncode_->enableDenoise();
        else
            audioRtpRecord_.dspEncode_->disableDenoise();

        if (agc)
            audioRtpRecord_.dspEncode_->enableAGC();
        else
            audioRtpRecord_.dspEncode_->disableAGC();

        audioRtpRecord_.dspEncode_->process(*out, getCodecFrameSize());
    }
#endif

    {
        std::lock_guard<std::mutex> lock(audioRtpRecord_.audioCodecMutex_);
        RETURN_IF_NULL(audioRtpRecord_.getCurrentCodec(), 0, "Audio codec already destroyed");
        unsigned char *micDataEncoded = audioRtpRecord_.encodedData_.data();
        int encoded = audioRtpRecord_.getCurrentCodec()->encode(micDataEncoded, out->getData(), getCodecFrameSize());
        return encoded;
    }
}
#undef RETURN_IF_NULL

#define RETURN_IF_NULL(A, M, ...) if (!(A)) { ERROR(M, ##__VA_ARGS__); return; }

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, size_t size, int payloadType)
{
    if (audioRtpRecord_.isDead())
        return;

    const int decPt = audioRtpRecord_.decoder_.getPayloadType();
    if (decPt != payloadType) {
        const bool switched = audioRtpRecord_.tryToSwitchPayloadTypes(payloadType);

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
        std::lock_guard<std::mutex> lock(audioRtpRecord_.audioCodecMutex_);
        RETURN_IF_NULL(audioRtpRecord_.getCurrentCodec(), "Audio codecs already destroyed");
        audioRtpRecord_.decData_.setFormat(getCodecFormat());
        audioRtpRecord_.decData_.resize(DEC_BUFFER_SIZE);
        int decoded = audioRtpRecord_.getCurrentCodec()->decode(audioRtpRecord_.decData_.getData(), spkrData, size);
        audioRtpRecord_.decData_.resize(decoded);
    }

#if HAVE_SPEEXDSP

    const bool denoise = Manager::instance().audioPreference.getNoiseReduce();
    const bool agc = Manager::instance().audioPreference.isAGCEnabled();

    if (denoise or agc) {
        std::lock_guard<std::mutex> lock(audioRtpRecord_.audioProcessMutex_);
        RETURN_IF_NULL(audioRtpRecord_.dspDecode_, "DSP already destroyed");
        if (denoise)
            audioRtpRecord_.dspDecode_->enableDenoise();
        else
            audioRtpRecord_.dspDecode_->disableDenoise();

        if (agc)
            audioRtpRecord_.dspDecode_->enableAGC();
        else
            audioRtpRecord_.dspDecode_->disableAGC();
        audioRtpRecord_.dspDecode_->process(audioRtpRecord_.decData_, getCodecFrameSize());
    }

#endif

    audioRtpRecord_.fadeInDecodedData();

    AudioBuffer *out = &(audioRtpRecord_.decData_);

    // test if resampling or up/down-mixing is required
    AudioFormat decFormat = out->getFormat();
    AudioFormat mainBuffFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
    if (decFormat.sample_rate != mainBuffFormat.sample_rate) {
        RETURN_IF_NULL(audioRtpRecord_.converterDecode_, "Converter already destroyed");
        audioRtpRecord_.resampledDataDecode_.setChannelNum(decFormat.channel_num);
        audioRtpRecord_.resampledDataDecode_.setSampleRate(mainBuffFormat.sample_rate);
        //WARN("Resample %s->%s", audioRtpRecord_.decData_.toString().c_str(), audioRtpRecord_.resampledDataDecode_.toString().c_str());
        audioRtpRecord_.converterDecode_->resample(audioRtpRecord_.decData_, audioRtpRecord_.resampledDataDecode_);
        out = &(audioRtpRecord_.resampledDataDecode_);
    }
    if (decFormat.channel_num != mainBuffFormat.channel_num)
        out->setChannelNum(mainBuffFormat.channel_num, true);

    Manager::instance().getMainBuffer().putData(*out, id_);
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
