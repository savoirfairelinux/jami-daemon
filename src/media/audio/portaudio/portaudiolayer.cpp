/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

namespace ring {

enum Direction { Input = 0, Output = 1, End = 2 };

struct PortAudioLayer::PortAudioLayerImpl
{
    PortAudioLayerImpl(PortAudioLayer&, const AudioPreference&);
    ~PortAudioLayerImpl();

    void init(PortAudioLayer&);
    void terminate() const;
    void initStream(PortAudioLayer&);

    std::vector<std::string> getDeviceByType(bool) const;

    PaDeviceIndex indexIn_;
    PaDeviceIndex indexOut_;
    PaDeviceIndex indexRing_;

    AudioBuffer playbackBuff_;

    std::shared_ptr<RingBuffer> mainRingBuffer_;

    std::array<PaStream*, static_cast<int>(Direction::End)> streams_;
    PaStream* fullDuplexStream_;

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

    int paFullDuplexCallback(PortAudioLayer& parent,
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
{}

std::vector<std::string>
PortAudioLayer::getCaptureDeviceList() const
{
    return pimpl_->getDeviceByType(false);
}

std::vector<std::string>
PortAudioLayer::getPlaybackDeviceList() const
{
    return pimpl_->getDeviceByType(true);
}

int
PortAudioLayer::getAudioDeviceIndex(const std::string& name, DeviceType type) const
{

    int numDevices = 0;
    (void) type;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(numDevices));
    } else {
        const PaDeviceInfo* deviceInfo;
        for (int i = 0; i < numDevices; i++) {
            deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo->name == name)
                return i;
        }
    }
    return -1;
}

std::string
PortAudioLayer::getAudioDeviceName(int index, DeviceType type) const
{
    (void) type;
    const PaDeviceInfo *deviceInfo;
    deviceInfo = Pa_GetDeviceInfo(index);
    return deviceInfo->name;
}

int
PortAudioLayer::getIndexCapture() const
{
    return pimpl_->indexIn_;
}

int
PortAudioLayer::getIndexPlayback() const
{
    return pimpl_->indexOut_;
}

int
PortAudioLayer::getIndexRingtone() const
{
    return pimpl_->indexRing_;
}

void
PortAudioLayer::startStream()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Idle)
            return;
        status_ = Status::Started;
    }
    pimpl_->initStream(*this);

    flushUrgent();
    flushMain();
}

void
PortAudioLayer::stopStream()
{
    if (status_ != Status::Started)
        return;

    RING_DBG("Stop PortAudio Streams");

    for (auto& st_ptr : pimpl_->streams_) {
        if (!st_ptr)
            continue;

        auto err = Pa_StopStream(st_ptr);
        if (err != paNoError)
            RING_ERR("Pa_StopStream error : %s", Pa_GetErrorText(err));

        err = Pa_CloseStream(st_ptr);
        if (err != paNoError)
            RING_ERR("Pa_StopStream error : %s", Pa_GetErrorText(err));
    }

    if (auto& st_ptr = pimpl_->fullDuplexStream_) {
        auto err = Pa_StopStream(st_ptr);
        if (err != paNoError)
            RING_ERR("Pa_StopStream error : %s", Pa_GetErrorText(err));

        err = Pa_CloseStream(st_ptr);
        if (err != paNoError)
            RING_ERR("Pa_StopStream error : %s", Pa_GetErrorText(err));
    }

    {
        std::lock_guard<std::mutex> lock {mutex_};
        status_ = Status::Idle;
    }

    // Flush the ring buffers
    flushUrgent();
    flushMain();
}

void
PortAudioLayer::updatePreference(AudioPreference& preference, int index, DeviceType type)
{
    switch (type) {
        case DeviceType::PLAYBACK:
        {
            auto playbackList = pimpl_->getDeviceByType(true);
            if (playbackList.size() > (size_t) index) {
                auto realIdx = getAudioDeviceIndex(playbackList.at(index), type);
                preference.setAlsaCardout(realIdx);
            }
        }
        break;

        case DeviceType::CAPTURE:
        {
            auto captureList = pimpl_->getDeviceByType(false);
            if (captureList.size() > (size_t) index) {
                auto realIdx = getAudioDeviceIndex(captureList.at(index), type);
                preference.setAlsaCardin(realIdx);
            }
        }
        break;

        case DeviceType::RINGTONE:
        preference.setAlsaCardring(index);
        break;

        default:
        break;
    }
}

//##################################################################################################

PortAudioLayer::PortAudioLayerImpl::PortAudioLayerImpl(PortAudioLayer& parent, const AudioPreference& pref)
    : indexIn_ {pref.getAlsaCardin()}
    , indexOut_ {pref.getAlsaCardout()}
    , indexRing_ {pref.getAlsaCardring()}
    , playbackBuff_ {0, parent.audioFormat_}
    , mainRingBuffer_ {Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID)}
{
    init(parent);
}

PortAudioLayer::PortAudioLayerImpl::~PortAudioLayerImpl()
{
    terminate();
}

std::vector<std::string>
PortAudioLayer::PortAudioLayerImpl::getDeviceByType(bool playback) const
{
    std::vector<std::string> ret;
    int numDevices = 0;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(numDevices));
    else {
        for (int i = 0; i < numDevices; i++) {
            const auto deviceInfo = Pa_GetDeviceInfo(i);
            if (playback) {
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
    RING_DBG("Init PortAudioLayer");
    const auto err = Pa_Initialize();
    if (err != paNoError) {
        RING_ERR("PortAudioLayer error : %s",  Pa_GetErrorText(err));
        terminate();
    }

    indexRing_ = indexOut_ = indexOut_ == paNoDevice ? Pa_GetDefaultOutputDevice() : indexOut_;
    indexIn_ = indexIn_ == paNoDevice ? Pa_GetDefaultInputDevice() : indexIn_;

    if (indexOut_ != paNoDevice) {
        if (const auto outputDeviceInfo = Pa_GetDeviceInfo(indexOut_)) {
            parent.audioFormat_.nb_channels = outputDeviceInfo->maxOutputChannels;
            parent.audioFormat_.sample_rate = outputDeviceInfo->defaultSampleRate;
            parent.hardwareFormatAvailable(parent.audioFormat_);
        } else {
            indexOut_ = paNoDevice;
        }
    }

    if (indexIn_ != paNoDevice) {
        if (const auto inputDeviceInfo = Pa_GetDeviceInfo(indexIn_)) {
            parent.audioInputFormat_.nb_channels = inputDeviceInfo->maxInputChannels;
            parent.audioInputFormat_.sample_rate = inputDeviceInfo->defaultSampleRate;
            parent.hardwareInputFormatAvailable(parent.audioInputFormat_);
        } else {
            indexIn_ = paNoDevice;
        }
    }

    std::fill(std::begin(streams_), std::end(streams_), nullptr);
    fullDuplexStream_ = nullptr;
}

void
PortAudioLayer::PortAudioLayerImpl::terminate() const
{
    RING_DBG("PortAudioLayer terminate.");
    auto err = Pa_Terminate();
    if (err != paNoError)
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

static void
openStreamDevice(PaStream** stream,
                 PaDeviceIndex device, Direction direction,
                 PaStreamCallback* callback, void* user_data)
{
    auto is_out = direction == Direction::Output;
    auto device_info = Pa_GetDeviceInfo(device);

    PaStreamParameters params;
    params.device = device;
    params.channelCount = is_out ? device_info->maxOutputChannels : device_info->maxInputChannels;
    params.sampleFormat = paInt16;
    params.suggestedLatency = is_out ? device_info->defaultLowOutputLatency: device_info->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    auto err = Pa_OpenStream(
        stream,
        is_out ? nullptr : &params,
        is_out ? &params : nullptr,
        device_info->defaultSampleRate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        callback,
        user_data);

    if (err != paNoError)
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

static void
openFullDuplexStream(PaStream** stream,
    PaDeviceIndex inputDeviceIndex, PaDeviceIndex ouputDeviceIndex,
    PaStreamCallback* callback, void* user_data)
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

    auto err = Pa_OpenStream(
        stream,
        &inputParams,
        &outputParams,
        std::min(input_device_info->defaultSampleRate, input_device_info->defaultSampleRate),
        paFramesPerBufferUnspecified,
        paNoFlag,
        callback,
        user_data);

    if (err != paNoError)
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
}

void
PortAudioLayer::PortAudioLayerImpl::initStream(PortAudioLayer& parent)
{
    parent.dcblocker_.reset();

    auto apiIndex = Pa_GetDefaultHostApi();
    auto apiInfo = Pa_GetHostApiInfo(apiIndex);
    RING_DBG() << "Portaudio using: " << apiInfo->name;

    RING_DBG("Open PortAudio Full-duplex input/output stream");
    if (indexOut_ != paNoDevice && indexIn_ != paNoDevice) {
        openFullDuplexStream(&fullDuplexStream_,
            indexIn_,
            indexOut_,
            [](const void* inputBuffer,
                void* outputBuffer,
                unsigned long framesPerBuffer,
                const PaStreamCallbackTimeInfo* timeInfo,
                PaStreamCallbackFlags statusFlags,
                void* userData) -> int {
                    auto layer = static_cast<PortAudioLayer*>(userData);
                    return layer->pimpl_->paFullDuplexCallback(*layer,
                        static_cast<const AudioSample*>(inputBuffer),
                        static_cast<AudioSample*>(outputBuffer),
                        framesPerBuffer,
                        timeInfo,
                        statusFlags);
            },
            &parent);
    } else {
        RING_WARN("Warning: No valid output device. There will be no sound.");
        RING_DBG("Open PortAudio Output Stream");
        if (indexOut_ != paNoDevice) {
            openStreamDevice(&streams_[Direction::Output],
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
            RING_ERR("Error: No valid output device. There will be no sound.");
        }

        RING_DBG("Open PortAudio Input Stream");
        if (indexIn_ != paNoDevice) {
            openStreamDevice(&streams_[Direction::Input],
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
            RING_ERR("Error: No valid input device. There will be no mic.");
        }
    }

    RING_DBG("Start PortAudio Streams");
    for (auto& st_ptr : streams_) {
        if (st_ptr) {
            auto err = Pa_StartStream(st_ptr);
            if (err != paNoError)
                RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        }
    }
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
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    auto toPlay = parent.getPlayback(parent.audioFormat_, framesPerBuffer);
    if (!toPlay) {
        std::fill_n(outputBuffer, framesPerBuffer * parent.audioFormat_.nb_channels, 0);
        return paContinue;
    }

    auto nFrames = toPlay->pointer()->nb_samples * toPlay->pointer()->channels;
    std::copy_n((AudioSample*)toPlay->pointer()->extended_data[0], nFrames, outputBuffer);

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
    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    if (framesPerBuffer == 0) {
        RING_WARN("No frames for input.");
        return paContinue;
    }

    auto inBuff = std::make_unique<AudioFrame>(parent.audioInputFormat_, framesPerBuffer);
    auto nFrames = framesPerBuffer * parent.audioInputFormat_.nb_channels;
    if (parent.isCaptureMuted_)
        libav_utils::fillWithSilence(inBuff->pointer());
    else
        std::copy_n(inputBuffer, nFrames, (AudioSample*)inBuff->pointer()->extended_data[0]);
    mainRingBuffer_->put(std::move(inBuff));
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paFullDuplexCallback(PortAudioLayer& parent,
    const AudioSample* inputBuffer,
    AudioSample* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags)
{
    // unused arguments
    (void)timeInfo;
    (void)statusFlags;

    // input
    auto inBuff = std::make_unique<AudioFrame>(parent.audioInputFormat_, framesPerBuffer);
    auto inFrames = framesPerBuffer * parent.audioInputFormat_.nb_channels;
    if (parent.isCaptureMuted_)
        libav_utils::fillWithSilence(inBuff->pointer());
    else
        std::copy_n(inputBuffer, inFrames, (AudioSample*)inBuff->pointer()->extended_data[0]);
    mainRingBuffer_->put(std::move(inBuff));

    // output
    auto toPlay = parent.getPlayback(parent.audioFormat_, framesPerBuffer);
    if (!toPlay) {
        std::fill_n(outputBuffer, framesPerBuffer * parent.audioFormat_.nb_channels, 0);
        return paContinue;
    }

    auto outFrames = toPlay->pointer()->nb_samples * toPlay->pointer()->channels;
    std::copy_n((AudioSample*)toPlay->pointer()->extended_data[0], outFrames, outputBuffer);

    return paContinue;
}

} // namespace ring
