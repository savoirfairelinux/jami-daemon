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

enum Direction {Input=0, Output=1, End=2};

struct PortAudioLayer::PortAudioLayerImpl
{
    PortAudioLayerImpl(PortAudioLayer&, const AudioPreference&);
    ~PortAudioLayerImpl();

    void init(PortAudioLayer&);
    void terminate() const;
    void initStream(PortAudioLayer&);

    std::vector<std::string> getDeviceByType(DeviceType type) const;
    int getIndexByType(DeviceType type);
    int getInternalIndexByType(const int index, DeviceType type);

    PaDeviceIndex indexIn_;
    PaDeviceIndex indexOut_;
    PaDeviceIndex indexRing_;

    AudioBuffer playbackBuff_;

    std::shared_ptr<RingBuffer> mainRingBuffer_;

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
};

//##################################################################################################

PortAudioLayer::PortAudioLayer(const AudioPreference& pref)
    : AudioLayer {pref}
    , pimpl_ {new PortAudioLayerImpl(*this, pref)}
{}

std::vector<std::string>
PortAudioLayer::getCaptureDeviceList() const
{
    return pimpl_->getDeviceByType(DeviceType::CAPTURE);
}

std::vector<std::string>
PortAudioLayer::getPlaybackDeviceList() const
{
    return pimpl_->getDeviceByType(DeviceType::PLAYBACK);
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
    return pimpl_->getIndexByType(DeviceType::CAPTURE);
}

int
PortAudioLayer::getIndexPlayback() const
{
    auto index = pimpl_->getIndexByType(DeviceType::PLAYBACK);
    return index;
}

int
PortAudioLayer::getIndexRingtone() const
{
    return pimpl_->getIndexByType(DeviceType::RINGTONE);
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
    auto internalIndex = pimpl_->getInternalIndexByType(index, type);
    switch (type) {
        case DeviceType::PLAYBACK:
            preference.setAlsaCardout(internalIndex);
            break;
        case DeviceType::CAPTURE:
            preference.setAlsaCardin(internalIndex);
            break;
        case DeviceType::RINGTONE:
            preference.setAlsaCardring(internalIndex);
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
PortAudioLayer::PortAudioLayerImpl::getDeviceByType(DeviceType type) const
{
    std::vector<std::string> ret;
    int numDevices = 0;

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(numDevices));
    else {
        for (int i = 0; i < numDevices; i++) {
            const auto deviceInfo = Pa_GetDeviceInfo(i);
            if (type == DeviceType::PLAYBACK) {
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
    (void) outputBuffer;
    (void) timeInfo;
    (void) statusFlags;

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

void
PortAudioLayer::PortAudioLayerImpl::init(PortAudioLayer& parent)
{
    RING_DBG("Init PortAudioLayer");
    const auto err = Pa_Initialize();
    if (err != paNoError) {
        RING_ERR("PortAudioLayer error : %s",  Pa_GetErrorText(err));
        terminate();
    }

    auto numDevices = Pa_GetDeviceCount();
    if (indexOut_ <= paNoDevice || indexOut_ >= numDevices) {
        indexRing_ = indexOut_ = Pa_GetDefaultOutputDevice();
    } else {
        indexRing_ = indexOut_;
    }

    if (indexIn_ <= paNoDevice || indexIn_ >= numDevices) {
        indexIn_ = Pa_GetDefaultInputDevice();
    }

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
}

int
PortAudioLayer::PortAudioLayerImpl::getIndexByType(DeviceType type)
{
    int index = indexRing_;
    if (type == DeviceType::PLAYBACK) {
        index = indexOut_;
    } else if (type == DeviceType::CAPTURE) {
        index = indexIn_;
    }

    auto deviceList = getDeviceByType(type);
    if (!deviceList.size()) {
        return paNoDevice;
    }

    const PaDeviceInfo *indexedDeviceInfo;
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
PortAudioLayer::PortAudioLayerImpl::getInternalIndexByType(const int index, DeviceType type)
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

void
PortAudioLayer::PortAudioLayerImpl::initStream(PortAudioLayer& parent)
{
    parent.dcblocker_.reset();

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

    RING_DBG("Start PortAudio Streams");
    for (auto& st_ptr : streams_) {
        if (st_ptr) {
            auto err = Pa_StartStream(st_ptr);
            if (err != paNoError)
                RING_ERR("PortAudioLayer error : %s", Pa_GetErrorText(err));
        }
    }
}

} // namespace ring
