/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
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

#include "portaudiolayer.h"
#include "manager.h"
#include "noncopyable.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"

namespace ring {
//FIXME: We should use WASAPi or DirectSound
PortAudioLayer::PortAudioLayer(const AudioPreference &pref)
: AudioLayer(pref)
, indexIn_(pref.getAlsaCardin())
, indexOut_(pref.getAlsaCardout())
, indexRing_(pref.getAlsaCardring())
, playbackBuff_(0, audioFormat_)
, mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{
    isStarted_ = false;
    this->init();
}

PortAudioLayer::~PortAudioLayer()
{
    this->terminate();
}

std::vector<std::string>
PortAudioLayer::getCaptureDeviceList() const {
    return this->getDeviceByType(false);
}

std::vector<std::string>
PortAudioLayer::getPlaybackDeviceList() const {
    return this->getDeviceByType(true);
}

int
PortAudioLayer::getAudioDeviceIndex(const std::string& name,
    DeviceType type) const {

    int numDevices = 0;
    (void) type;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        this->handleError(numDevices);
    else {
        const PaDeviceInfo *deviceInfo;
        for (int i = 0; i < numDevices; i++) {
            deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo->name == name)
                return i;
        }
    }
    return -1;
}

std::string
PortAudioLayer::getAudioDeviceName(int index, DeviceType type) const {
    (void) type;
    const PaDeviceInfo *deviceInfo;
    deviceInfo = Pa_GetDeviceInfo(index);
    return deviceInfo->name;
}

int
PortAudioLayer::getIndexCapture() const {
    return this->indexIn_;
}

int
PortAudioLayer::getIndexPlayback() const {
    return this->indexOut_;
}

int
PortAudioLayer::getIndexRingtone() const {
    return this->indexRing_;
}

void
PortAudioLayer::startStream() {
    if (isRunning_)
        return;
    this->initStream();
    isRunning_ = isStarted_ = true;
}

void
PortAudioLayer::stopStream() {

    if (!isRunning_)
        return;

    RING_DBG("Stop PortAudio Streams");

    //FIX ME : Abort or Stop ??
    for (int i = 0; i < direction::END; i++) {
        PaError err = Pa_AbortStream(streams[i]);
        if(err != paNoError)
            this->handleError(err);

        err = Pa_CloseStream(streams[i]);
        if (err != paNoError)
            this->handleError(err);
    }

    isRunning_ = isStarted_ = false;

    /* Flush the ring buffers */
    flushUrgent();
    flushMain();
}

void
PortAudioLayer::updatePreference(AudioPreference &preference,
    int index, DeviceType type)
{
    switch (type) {
        case DeviceType::PLAYBACK:
        preference.setAlsaCardout(index);
        break;

        case DeviceType::CAPTURE:
        preference.setAlsaCardin(index);
        break;

        case DeviceType::RINGTONE:
        preference.setAlsaCardring(index);
        break;

        default:
        break;
    }
}

std::vector<std::string>
PortAudioLayer::getDeviceByType(const bool& playback) const {
    std::vector<std::string> ret;
    int numDevices = 0;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        this->handleError(numDevices);
    else {
        const PaDeviceInfo *deviceInfo;
        for (int i = 0; i < numDevices; i++) {
            deviceInfo = Pa_GetDeviceInfo(i);
            if (playback) {
                if (deviceInfo->maxOutputChannels > 0)
                    ret.push_back(deviceInfo->name);
            }
            else {
                if (deviceInfo->maxInputChannels > 0)
                    ret.push_back(deviceInfo->name);
            }
        }
    }
    return ret;
}

int
PortAudioLayer::paOutputCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData) {

    (void) inputBuffer;
    (void) timeInfo;
    (void) statusFlags;

    PortAudioLayer *ref = (PortAudioLayer*)userData;
    AudioSample *out = (AudioSample*)outputBuffer;

    AudioFormat mainBufferAudioFormat =
        Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample =
        ref->audioFormat_.sample_rate != mainBufferAudioFormat.sample_rate;
    unsigned urgentFramesToGet =
        ref->urgentRingBuffer_.availableForGet(RingBufferPool::DEFAULT_ID);

    if (urgentFramesToGet > 0) {
        RING_WARN("Getting urgent frames.");
        size_t totSample = std::min(framesPerBuffer,
            (unsigned long)urgentFramesToGet);

        ref->playbackBuff_.setFormat(ref->audioFormat_);
        ref->playbackBuff_.resize(totSample);
        ref->urgentRingBuffer_.get(ref->playbackBuff_, RingBufferPool::DEFAULT_ID);

        ref->playbackBuff_.applyGain(ref->isPlaybackMuted_ ? 0.0 : ref->playbackGain_);

        ref->playbackBuff_.interleave(out);

        Manager::instance().getRingBufferPool().discard(totSample,
            RingBufferPool::DEFAULT_ID);
    }

    unsigned normalFramesToGet =
        Manager::instance().getRingBufferPool().availableForGet(RingBufferPool::DEFAULT_ID);
    if (normalFramesToGet > 0) {
        double resampleFactor = 1.0;
        unsigned readableSamples = framesPerBuffer;

        if (resample) {
            resampleFactor =
                static_cast<double>(ref->audioFormat_.sample_rate)
                    / mainBufferAudioFormat.sample_rate;
            readableSamples = std::ceil(framesPerBuffer / resampleFactor);
        }

        readableSamples = std::min(readableSamples, normalFramesToGet);

        ref->playbackBuff_.setFormat(ref->audioFormat_);
        ref->playbackBuff_.resize(readableSamples);
        Manager::instance().getRingBufferPool().getData(ref->playbackBuff_,
            RingBufferPool::DEFAULT_ID);
        ref->playbackBuff_.applyGain(ref->isPlaybackMuted_ ? 0.0 : ref->playbackGain_);

        if (resample) {
            AudioBuffer resampledOutput(readableSamples, ref->audioFormat_);
            ref->resampler_->resample(ref->playbackBuff_, resampledOutput);

            resampledOutput.interleave(out);
        } else {
            ref->playbackBuff_.interleave(out);
        }
    }
    if (normalFramesToGet <= 0) {
        AudioLoop* tone = Manager::instance().getTelephoneTone();
        AudioLoop* file_tone = Manager::instance().getTelephoneFile();

        ref->playbackBuff_.setFormat(ref->audioFormat_);
        ref->playbackBuff_.resize(framesPerBuffer);

        if (tone) {
                tone->getNext(ref->playbackBuff_, ref->playbackGain_);
        }
        else if (file_tone) {
                file_tone->getNext(ref->playbackBuff_, ref->playbackGain_);
        }
        else {
                //RING_WARN("No tone or file_tone!");
                ref->playbackBuff_.reset();
        }
        ref->playbackBuff_.interleave(out);
    }
    return paContinue;
}

int
PortAudioLayer::paInputCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData) {

    (void) outputBuffer;
    (void) timeInfo;
    (void) statusFlags;

    PortAudioLayer* ref = (PortAudioLayer*)userData;
    AudioSample *in = (AudioSample*)inputBuffer;

    if (framesPerBuffer == 0) {
        RING_WARN("No frames for input.");
        return paContinue;
    }

    const AudioFormat mainBufferFormat =
        Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample =
        ref->audioInputFormat_.sample_rate != mainBufferFormat.sample_rate;

    AudioBuffer inBuff(framesPerBuffer, ref->audioInputFormat_);

    inBuff.deinterleave(in, framesPerBuffer, ref->audioInputFormat_.nb_channels);

    inBuff.applyGain(ref->isCaptureMuted_ ? 0.0 : ref->captureGain_);

    if (resample) {
        int outSamples =
            framesPerBuffer
            / (static_cast<double>(ref->audioInputFormat_.sample_rate)
                / mainBufferFormat.sample_rate);
        AudioBuffer out(outSamples, mainBufferFormat);
        ref->inputResampler_->resample(inBuff, out);
        ref->dcblocker_.process(out);
        ref->mainRingBuffer_->put(out);
    } else {
        ref->dcblocker_.process(inBuff);
        ref->mainRingBuffer_->put(inBuff);
    }
    return paContinue;
}

// PRIVATE METHOD
void
PortAudioLayer::handleError(const PaError& err) const {
    RING_ERR("PortAudioLayer error : %s",  Pa_GetErrorText(err));
}

void
PortAudioLayer::init() {
    RING_DBG("Init PortAudioLayer");
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        this->handleError(err);
        this->terminate();
    }
    //FIXME: Maybe find a better way
    const PaDeviceInfo *outputDeviceInfo = Pa_GetDeviceInfo(indexOut_);
    audioFormat_.nb_channels = outputDeviceInfo->maxOutputChannels;
    audioFormat_.sample_rate = 48000;
    hardwareFormatAvailable(audioFormat_);
}

void
PortAudioLayer::terminate() const {
    RING_DBG("PortAudioLayer terminate.");
    PaError err = Pa_Terminate();
    if (err != paNoError)
        this->handleError(err);
}

void
PortAudioLayer::initStream() {
    dcblocker_.reset();

    RING_DBG("Open PortAudio Output Stream");
    PaStreamParameters outputParameters;
    outputParameters.device = indexOut_;
    RING_DBG("Index Device: %d", indexOut_);
    if (outputParameters.device == paNoDevice) {
        //TODO Should we fallback to default output device ?
        RING_ERR("Error: No valid output device. There will be no sound.");
    }
    const PaDeviceInfo *outputDeviceInfo =
        Pa_GetDeviceInfo(outputParameters.device);
    RING_DBG("Default Sample Rate : %d", outputDeviceInfo->defaultSampleRate);
    outputParameters.channelCount =
        audioFormat_.nb_channels = outputDeviceInfo->maxOutputChannels;
    //FIX ME : Default is 0....
    audioFormat_.sample_rate = 48000;

    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = outputDeviceInfo->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    PaError err = Pa_OpenStream(
            &streams[direction::OUTPUT],
            NULL,
            &outputParameters,
            audioFormat_.sample_rate,
            paFramesPerBufferUnspecified,
            paNoFlag,
            &PortAudioLayer::paOutputCallback,
            this);
    if(err != paNoError)
            this->handleError(err);

    RING_DBG("Open PortAudio Input Stream");
    PaStreamParameters inputParameters;
    inputParameters.device = indexIn_;
    if (inputParameters.device == paNoDevice) {
        //TODO Should we fallback to default output device ?
        RING_ERR("Error: No valid input device. There will be no mic.");
    }

    const PaDeviceInfo *inputDeviceInfo =
        Pa_GetDeviceInfo(inputParameters.device);
    //FIX ME : Default is 0....
    audioInputFormat_.sample_rate = 48000;
    inputParameters.channelCount =
        audioInputFormat_.nb_channels = inputDeviceInfo->maxInputChannels;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = inputDeviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            &streams[direction::INPUT],
            &inputParameters,
            NULL,
            audioInputFormat_.sample_rate,
            paFramesPerBufferUnspecified,
            paNoFlag,
            &PortAudioLayer::paInputCallback,
            this);

    RING_DBG("Start PortAudio Streams");
    for (int i = 0; i < direction::END; i++) {
        err = Pa_StartStream(streams[i]);
        if(err != paNoError)
            this->handleError(err);
    }

    hardwareFormatAvailable(audioFormat_);
    hardwareInputFormatAvailable(audioInputFormat_);

    flushUrgent();
    flushMain();
}
} // ::ring
