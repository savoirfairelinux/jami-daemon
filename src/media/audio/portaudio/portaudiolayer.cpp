/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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
 */

#include "portaudiolayer.h"
#include "manager.h"
#include "noncopyable.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audio/audioloop.h"

#include <portaudio.h>
#include <algorithm>
#include <cmath>

namespace jami {

enum Direction { Input = 0, Output = 1, IO = 2, End = 3 };

struct PortAudioLayer::PortAudioLayerImpl
{
    PortAudioLayerImpl(PortAudioLayer&, const AudioPreference&);
    ~PortAudioLayerImpl();

    void init(PortAudioLayer&);
    void initInput(PortAudioLayer&);
    void initOutput(PortAudioLayer&);
    void terminate() const;
    bool initInputStream(PortAudioLayer&);
    bool initOutputStream(PortAudioLayer&);
    bool initFullDuplexStream(PortAudioLayer&);
    bool apiInitialised_ {false};

    std::vector<std::string> getDeviceByType(AudioDeviceType type) const;
    int getIndexByType(AudioDeviceType type);
    int getInternalIndexByType(const int index, AudioDeviceType type);

    PaDeviceIndex indexIn_;
    bool inputInitialized_ {false};
    PaDeviceIndex indexOut_;
    PaDeviceIndex indexRing_;
    bool outputInitialized_ {false};

    AudioBuffer playbackBuff_;

    std::array<PaStream*, static_cast<int>(Direction::End)> streams_;

    int paOutputCallback(PortAudioLayer& parent,
                         const AudioSample* inputBuffer,
                         AudioSample* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags);

    int paInputCallback(PortAudioLayer& parent,
                        const AudioSample* inputBuffer,
                        AudioSample* outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags);

    int paIOCallback(PortAudioLayer& parent,
                     const AudioSample* inputBuffer,
                     AudioSample* outputBuffer,
                     unsigned long framesPerBuffer,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags);
};

//##################################################################################################

PortAudioLayer::PortAudioLayer(const AudioPreference& pref)
    : AudioLayer {pref}
    , pimpl_ {new PortAudioLayerImpl(*this, pref)}
{
    setHasNativeAEC(false);

    auto numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        JAMI_ERR("Pa_CountDevices returned 0x%x", numDevices);
        return;
    }
    const PaDeviceInfo* deviceInfo;
    for (auto i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        JAMI_DBG("PortAudio device: %d, %s", i, deviceInfo->name);
    }
}

PortAudioLayer::~PortAudioLayer()
{
    stopStream();
}

std::vector<std::string>
PortAudioLayer::getCaptureDeviceList() const
{
    return pimpl_->getDeviceByType(AudioDeviceType::CAPTURE);
}

std::vector<std::string>
PortAudioLayer::getPlaybackDeviceList() const
{
    return pimpl_->getDeviceByType(AudioDeviceType::PLAYBACK);
}

int
PortAudioLayer::getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const
{
    auto deviceList = pimpl_->getDeviceByType(type);

    int numDevices = 0;
    numDevices = deviceList.size();
    if (numDevices < 0) {
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(numDevices));
    } else {
        int i = 0;
        for (auto d = deviceList.cbegin(); d != deviceList.cend(); ++d, ++i) {
            if (*d == name) {
                return i;
            }
        }
    }
    return paNoDevice;
}

std::string
PortAudioLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    (void) type;
    const PaDeviceInfo* deviceInfo;
    deviceInfo = Pa_GetDeviceInfo(index);
    return deviceInfo->name;
}

int
PortAudioLayer::getIndexCapture() const
{
    return pimpl_->getIndexByType(AudioDeviceType::CAPTURE);
}

int
PortAudioLayer::getIndexPlayback() const
{
    auto index = pimpl_->getIndexByType(AudioDeviceType::PLAYBACK);
    return index;
}

int
PortAudioLayer::getIndexRingtone() const
{
    return pimpl_->getIndexByType(AudioDeviceType::RINGTONE);
}

void
PortAudioLayer::startStream(AudioDeviceType stream)
{
    if (!pimpl_->apiInitialised_) {
        JAMI_WARN("PortAudioLayer API not initialised");
        return;
    }

    auto startPlayback = [this](bool fullDuplexMode = false) -> bool {
        std::unique_lock<std::mutex> lock(mutex_);
        if (status_.load() != Status::Idle)
            return false;
        bool ret {false};
        if (fullDuplexMode)
            ret = pimpl_->initFullDuplexStream(*this);
        else
            ret = pimpl_->initOutputStream(*this);
        if (ret) {
            status_.store(Status::Started);
            lock.unlock();
            flushUrgent();
            flushMain();
        }
        return ret;
    };

    switch (stream) {
    case AudioDeviceType::ALL:
        if (!startPlayback(true)) {
            pimpl_->initInputStream(*this);
            startPlayback();
        }
        break;
    case AudioDeviceType::CAPTURE:
        pimpl_->initInputStream(*this);
        break;
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        startPlayback();
        break;
    }
}

void
PortAudioLayer::stopStream(AudioDeviceType stream)
{
    auto stopPaStream = [](PaStream* stream) -> bool {
        if (!stream)
            return false;
        auto err = Pa_StopStream(stream);
        if (err != paNoError) {
            JAMI_ERR("Pa_StopStream error : %s", Pa_GetErrorText(err));
            return false;
        }
        err = Pa_CloseStream(stream);
        if (err != paNoError) {
            JAMI_ERR("Pa_CloseStream error : %s", Pa_GetErrorText(err));
            return false;
        }
        return true;
    };

    auto stopPlayback = [this, &stopPaStream](bool fullDuplexMode = false) -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() != Status::Started)
            return false;
        bool stopped = false;
        if (fullDuplexMode)
            stopped = stopPaStream(pimpl_->streams_[Direction::IO]);
        else
            stopped = stopPaStream(pimpl_->streams_[Direction::Output]);
        if (stopped)
            status_.store(Status::Idle);
        return stopped;
    };

    bool stopped = false;
    switch (stream) {
    case AudioDeviceType::ALL:
        if (pimpl_->streams_[Direction::IO]) {
            stopped = stopPlayback(true);
        } else {
            stopped = stopPaStream(pimpl_->streams_[Direction::Input]) && stopPlayback();
        }
        if (stopped) {
            recordChanged(false);
            playbackChanged(false);
            JAMI_DBG("PortAudioLayer I/O streams stopped");
        } else
            return;
        break;
    case AudioDeviceType::CAPTURE:
        if (stopPaStream(pimpl_->streams_[Direction::Input])) {
            recordChanged(false);
            JAMI_DBG("PortAudioLayer input stream stopped");
        } else
            return;
        break;
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        if (stopPlayback()) {
            playbackChanged(false);
            JAMI_DBG("PortAudioLayer output stream stopped");
        } else
            return;
        break;
    }

    // Flush the ring buffers
    flushUrgent();
    flushMain();
}

void
PortAudioLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    auto internalIndex = pimpl_->getInternalIndexByType(index, type);
    switch (type) {
    case AudioDeviceType::PLAYBACK:
        preference.setAlsaCardout(internalIndex);
        break;
    case AudioDeviceType::CAPTURE:
        preference.setAlsaCardin(internalIndex);
        break;
    case AudioDeviceType::RINGTONE:
        preference.setAlsaCardring(internalIndex);
        break;
    default:
        break;
    }
}

//##################################################################################################

PortAudioLayer::PortAudioLayerImpl::PortAudioLayerImpl(PortAudioLayer& parent,
                                                       const AudioPreference& pref)
    : indexIn_ {pref.getAlsaCardin()}
    , indexOut_ {pref.getAlsaCardout()}
    , indexRing_ {pref.getAlsaCardring()}
    , playbackBuff_ {0, parent.audioFormat_}
{
    init(parent);
}

PortAudioLayer::PortAudioLayerImpl::~PortAudioLayerImpl()
{
    terminate();
}

void
PortAudioLayer::PortAudioLayerImpl::initInput(PortAudioLayer& parent)
{
    auto numDevices = Pa_GetDeviceCount();
    if (indexIn_ <= paNoDevice || indexIn_ >= numDevices) {
        indexIn_ = Pa_GetDefaultInputDevice();
    }

    // Pa_GetDefaultInputDevice returned paNoDevice or we already initialized the device
    if (indexIn_ == paNoDevice || inputInitialized_)
        return;

    if (const auto inputDeviceInfo = Pa_GetDeviceInfo(indexIn_)) {
        if (inputDeviceInfo->maxInputChannels <= 0) {
            indexIn_ = paNoDevice;
            return initInput(parent);
        }
        parent.audioInputFormat_.sample_rate = inputDeviceInfo->defaultSampleRate;
        parent.audioInputFormat_.nb_channels = inputDeviceInfo->maxInputChannels;
        parent.hardwareInputFormatAvailable(parent.audioInputFormat_);
        JAMI_DBG("PortAudioLayer initialized input: %s {%d Hz, %d channels}",
                 inputDeviceInfo->name,
                 parent.audioInputFormat_.sample_rate,
                 parent.audioInputFormat_.nb_channels);
        inputInitialized_ = true;
    } else {
        JAMI_WARN("PortAudioLayer could not initialize input");
        indexIn_ = paNoDevice;
        inputInitialized_ = true;
    }
}

void
PortAudioLayer::PortAudioLayerImpl::initOutput(PortAudioLayer& parent)
{
    auto numDevices = Pa_GetDeviceCount();
    if (indexOut_ <= paNoDevice || indexOut_ >= numDevices) {
        indexRing_ = indexOut_ = Pa_GetDefaultOutputDevice();
    } else {
        indexRing_ = indexOut_;
    }

    // Pa_GetDefaultOutputDevice returned paNoDevice or we already initialized the device
    if (indexOut_ == paNoDevice || outputInitialized_)
        return;

    if (const auto outputDeviceInfo = Pa_GetDeviceInfo(indexOut_)) {
        if (outputDeviceInfo->maxOutputChannels <= 0) {
            indexOut_ = paNoDevice;
            return initOutput(parent);
        }
        parent.audioFormat_.sample_rate = outputDeviceInfo->defaultSampleRate;
        parent.audioFormat_.nb_channels = outputDeviceInfo->maxOutputChannels;
        parent.hardwareFormatAvailable(parent.audioFormat_);
        JAMI_DBG("PortAudioLayer initialized output: %s {%d Hz, %d channels}",
                 outputDeviceInfo->name,
                 parent.audioFormat_.sample_rate,
                 parent.audioFormat_.nb_channels);
        outputInitialized_ = true;
    } else {
        JAMI_WARN("PortAudioLayer could not initialize output");
        indexOut_ = paNoDevice;
        outputInitialized_ = true;
    }
}

std::vector<std::string>
PortAudioLayer::PortAudioLayerImpl::getDeviceByType(AudioDeviceType type) const
{
    std::vector<std::string> ret;
    int numDevices = 0;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(numDevices));
    else {
        for (int i = 0; i < numDevices; i++) {
            const auto deviceInfo = Pa_GetDeviceInfo(i);
            if (type == AudioDeviceType::PLAYBACK) {
                if (deviceInfo->maxOutputChannels > 0)
                    ret.push_back(deviceInfo->name);
            } else {
                if (deviceInfo->maxInputChannels > 0)
                    ret.push_back(deviceInfo->name);
            }
        }
    }
    return ret;
}

void
PortAudioLayer::PortAudioLayerImpl::init(PortAudioLayer& parent)
{
    JAMI_DBG("PortAudioLayer Init");
    const auto err = Pa_Initialize();
    auto apiIndex = Pa_GetDefaultHostApi();
    auto apiInfo = Pa_GetHostApiInfo(apiIndex);
    if (err != paNoError || apiInfo == nullptr) {
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        terminate();
        return;
    }

    apiInitialised_ = true;
    JAMI_DBG() << "Portaudio initialized using: " << apiInfo->name;

    auto defaultInputIndex = Pa_GetDefaultInputDevice();
    if (const auto defaultInputDeviceInfo = Pa_GetDeviceInfo(defaultInputIndex)) {
        JAMI_DBG("PortAudioLayer default input: %s {%.0f Hz, %d channels}",
                 defaultInputDeviceInfo->name,
                 defaultInputDeviceInfo->defaultSampleRate,
                 defaultInputDeviceInfo->maxInputChannels);    
    }

    auto defaultOutputIndex = Pa_GetDefaultOutputDevice();
    if (const auto defaultOutputDeviceInfo = Pa_GetDeviceInfo(defaultOutputIndex)) {
        JAMI_DBG("PortAudioLayer default output: %s {%.0f Hz, %d channels}",
                 defaultOutputDeviceInfo->name,
                 defaultOutputDeviceInfo->defaultSampleRate,
                 defaultOutputDeviceInfo->maxOutputChannels);    
    }

    auto commInputIndex = Pa_GetDefaultCommInputDevice();
    if (const auto commInputDeviceInfo = Pa_GetDeviceInfo(commInputIndex)) {
        JAMI_DBG("PortAudioLayer default comm input: %s {%.0f Hz, %d channels}",
                 commInputDeviceInfo->name,
                 commInputDeviceInfo->defaultSampleRate,
                 commInputDeviceInfo->maxInputChannels);    
    }

    auto commOutputIndex = Pa_GetDefaultCommOutputDevice();
    if (const auto commOutputDeviceInfo = Pa_GetDeviceInfo(commOutputIndex)) {
        JAMI_DBG("PortAudioLayer default comm output: %s {%.0f Hz, %d channels}",
                 commOutputDeviceInfo->name,
                 commOutputDeviceInfo->defaultSampleRate,
                 commOutputDeviceInfo->maxOutputChannels);    
    }

    initInput(parent);
    initOutput(parent);

    std::fill(std::begin(streams_), std::end(streams_), nullptr);
}

int
PortAudioLayer::PortAudioLayerImpl::getIndexByType(AudioDeviceType type)
{
    int index = indexRing_;
    if (type == AudioDeviceType::PLAYBACK) {
        index = indexOut_;
    } else if (type == AudioDeviceType::CAPTURE) {
        index = indexIn_;
    }

    auto deviceList = getDeviceByType(type);
    if (!deviceList.size()) {
        return paNoDevice;
    }

    const PaDeviceInfo* indexedDeviceInfo;
    indexedDeviceInfo = Pa_GetDeviceInfo(index);
    if (!indexedDeviceInfo) {
        return paNoDevice;
    }

    for (int i = 0; i < deviceList.size(); ++i) {
        if (deviceList.at(i) == indexedDeviceInfo->name) {
            return i;
        }
    }

    return paNoDevice;
}

int
PortAudioLayer::PortAudioLayerImpl::getInternalIndexByType(const int index, AudioDeviceType type)
{
    auto deviceList = getDeviceByType(type);
    if (!deviceList.size() || index >= deviceList.size()) {
        return paNoDevice;
    }

    for (int i = 0; i < Pa_GetDeviceCount(); i++) {
        const auto deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceList.at(index) == deviceInfo->name) {
            return i;
        }
    }

    return paNoDevice;
}

void
PortAudioLayer::PortAudioLayerImpl::terminate() const
{
    JAMI_DBG("PortAudioLayer terminate.");
    auto err = Pa_Terminate();
    if (err != paNoError)
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

static void
openStreamDevice(PaStream** stream,
                 PaDeviceIndex device,
                 Direction direction,
                 PaStreamCallback* callback,
                 void* user_data)
{
    auto is_out = direction == Direction::Output;
    auto device_info = Pa_GetDeviceInfo(device);

    PaStreamParameters params;
    params.device = device;
    params.channelCount = is_out ? device_info->maxOutputChannels : device_info->maxInputChannels;
    params.sampleFormat = paInt16;
    params.suggestedLatency = is_out ? device_info->defaultLowOutputLatency
                                     : device_info->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    auto err = Pa_OpenStream(stream,
                             is_out ? nullptr : &params,
                             is_out ? &params : nullptr,
                             device_info->defaultSampleRate,
                             paFramesPerBufferUnspecified,
                             paNoFlag,
                             callback,
                             user_data);

    if (err != paNoError)
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

static void
openFullDuplexStream(PaStream** stream,
                     PaDeviceIndex inputDeviceIndex,
                     PaDeviceIndex ouputDeviceIndex,
                     PaStreamCallback* callback,
                     void* user_data)
{
    auto input_device_info = Pa_GetDeviceInfo(inputDeviceIndex);
    auto output_device_info = Pa_GetDeviceInfo(ouputDeviceIndex);

    PaStreamParameters inputParams;
    inputParams.device = inputDeviceIndex;
    inputParams.channelCount = input_device_info->maxInputChannels;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = input_device_info->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters outputParams;
    outputParams.device = ouputDeviceIndex;
    outputParams.channelCount = output_device_info->maxOutputChannels;
    outputParams.sampleFormat = paInt16;
    outputParams.suggestedLatency = output_device_info->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    auto err = Pa_OpenStream(stream,
                             &inputParams,
                             &outputParams,
                             std::min(input_device_info->defaultSampleRate,
                                      input_device_info->defaultSampleRate),
                             paFramesPerBufferUnspecified,
                             paNoFlag,
                             callback,
                             user_data);

    if (err != paNoError)
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

bool
PortAudioLayer::PortAudioLayerImpl::initInputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Input Stream");
    auto& stream = streams_[Direction::Input];
    if (indexIn_ != paNoDevice) {
        openStreamDevice(
            &streams_[Direction::Input],
            indexIn_,
            Direction::Input,
            [](const void* inputBuffer,
               void* outputBuffer,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void* userData) -> int {
                auto layer = static_cast<PortAudioLayer*>(userData);
                return layer->pimpl_->paInputCallback(*layer,
                                                      static_cast<const AudioSample*>(inputBuffer),
                                                      static_cast<AudioSample*>(outputBuffer),
                                                      framesPerBuffer,
                                                      timeInfo,
                                                      statusFlags);
            },
            &parent);
    } else {
        JAMI_ERR("Error: No valid input device. There will be no mic.");
        return false;
    }

    JAMI_DBG("Starting PortAudio Input Stream");
    auto err = Pa_StartStream(stream);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        return false;
    }

    parent.recordChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initOutputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Output Stream");
    auto& stream = streams_[Direction::Output];
    if (indexOut_ != paNoDevice) {
        openStreamDevice(
            &stream,
            indexOut_,
            Direction::Output,
            [](const void* inputBuffer,
               void* outputBuffer,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void* userData) -> int {
                auto layer = static_cast<PortAudioLayer*>(userData);
                return layer->pimpl_->paOutputCallback(*layer,
                                                       static_cast<const AudioSample*>(inputBuffer),
                                                       static_cast<AudioSample*>(outputBuffer),
                                                       framesPerBuffer,
                                                       timeInfo,
                                                       statusFlags);
            },
            &parent);
    } else {
        JAMI_ERR("Error: No valid output device. There will be no sound.");
        return false;
    }

    JAMI_DBG("Starting PortAudio Output Stream");
    auto err = Pa_StartStream(stream);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        return false;
    }

    parent.playbackChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initFullDuplexStream(PortAudioLayer& parent)
{
    if (indexOut_ == paNoDevice || indexIn_ == paNoDevice) {
        JAMI_ERR("Error: Invalid input/output devices. There will be no audio.");
        return false;
    }

    parent.dcblocker_.reset();

    JAMI_DBG("Open PortAudio Full-duplex input/output stream");
    auto& stream = streams_[Direction::IO];
    openFullDuplexStream(
        &stream,
        indexIn_,
        indexOut_,
        [](const void* inputBuffer,
           void* outputBuffer,
           unsigned long framesPerBuffer,
           const PaStreamCallbackTimeInfo* timeInfo,
           PaStreamCallbackFlags statusFlags,
           void* userData) -> int {
            auto layer = static_cast<PortAudioLayer*>(userData);
            return layer->pimpl_->paIOCallback(*layer,
                                               static_cast<const AudioSample*>(inputBuffer),
                                               static_cast<AudioSample*>(outputBuffer),
                                               framesPerBuffer,
                                               timeInfo,
                                               statusFlags);
        },
        &parent);

    JAMI_DBG("Start PortAudio I/O Streams");
    auto err = Pa_StartStream(stream);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        return false;
    }

    parent.recordChanged(true);
    parent.playbackChanged(true);
    return true;
}

int
PortAudioLayer::PortAudioLayerImpl::paOutputCallback(PortAudioLayer& parent,
                                                     const AudioSample* inputBuffer,
                                                     AudioSample* outputBuffer,
                                                     unsigned long framesPerBuffer,
                                                     const PaStreamCallbackTimeInfo* timeInfo,
                                                     PaStreamCallbackFlags statusFlags)
{
    // unused arguments
    (void) inputBuffer;
    (void) timeInfo;
    (void) statusFlags;

    auto toPlay = parent.getPlayback(parent.audioFormat_, framesPerBuffer);
    if (!toPlay) {
        std::fill_n(outputBuffer, framesPerBuffer * parent.audioFormat_.nb_channels, 0);
        return paContinue;
    }

    auto nFrames = toPlay->pointer()->nb_samples * toPlay->pointer()->channels;
    std::copy_n((AudioSample*) toPlay->pointer()->extended_data[0], nFrames, outputBuffer);
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paInputCallback(PortAudioLayer& parent,
                                                    const AudioSample* inputBuffer,
                                                    AudioSample* outputBuffer,
                                                    unsigned long framesPerBuffer,
                                                    const PaStreamCallbackTimeInfo* timeInfo,
                                                    PaStreamCallbackFlags statusFlags)
{
    // unused arguments
    (void) outputBuffer;
    (void) timeInfo;
    (void) statusFlags;

    if (framesPerBuffer == 0) {
        JAMI_WARN("No frames for input.");
        return paContinue;
    }

    auto inBuff = std::make_shared<AudioFrame>(parent.audioInputFormat_, framesPerBuffer);
    auto nFrames = framesPerBuffer * parent.audioInputFormat_.nb_channels;
    if (parent.isCaptureMuted_)
        libav_utils::fillWithSilence(inBuff->pointer());
    else
        std::copy_n(inputBuffer, nFrames, (AudioSample*) inBuff->pointer()->extended_data[0]);
    parent.putRecorded(std::move(inBuff));
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paIOCallback(PortAudioLayer& parent,
                                                 const AudioSample* inputBuffer,
                                                 AudioSample* outputBuffer,
                                                 unsigned long framesPerBuffer,
                                                 const PaStreamCallbackTimeInfo* timeInfo,
                                                 PaStreamCallbackFlags statusFlags)
{
    paInputCallback(parent, inputBuffer, nullptr, framesPerBuffer, timeInfo, statusFlags);
    paOutputCallback(parent, nullptr, outputBuffer, framesPerBuffer, timeInfo, statusFlags);
    return paContinue;
}

} // namespace jami
