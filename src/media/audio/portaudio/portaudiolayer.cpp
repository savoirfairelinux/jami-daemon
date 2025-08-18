/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "portaudiolayer.h"

#include "audio/resampler.h"
#include "audio/portaudio/audio_device_monitor.h"
#include "manager.h"
#include "preferences.h"

#include <portaudio.h>

#include <windows.h>

#include <algorithm>

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

    std::vector<std::string> getDevicesByType(AudioDeviceType type) const;
    int getIndexByType(AudioDeviceType type);
    std::string getDeviceNameByType(const int index, AudioDeviceType type);
    PaDeviceIndex getApiIndexByType(AudioDeviceType type);
    std::string getApiDefaultDeviceName(AudioDeviceType type, bool commDevice) const;

    std::string deviceRecord_ {};
    std::string devicePlayback_ {};
    std::string deviceRingtone_ {};

    static constexpr const int defaultIndex_ {0};

    bool inputInitialized_ {false};
    bool outputInitialized_ {false};

    std::array<PaStream*, static_cast<int>(Direction::End)> streams_;
    mutable std::mutex streamsMutex_;
    bool paStopStream(Direction streamDirection);
    bool hasFullDuplexStream() const;

    AudioDeviceNotificationClientPtr audioDeviceNotificationClient_;
    // The following flag used to debounce the device state changes,
    // as default-device change events often follow plug/unplug events.
    std::atomic<bool> restartRequestPending_ = false;

    int paOutputCallback(PortAudioLayer& parent,
                         const int16_t* inputBuffer,
                         int16_t* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags);

    int paInputCallback(PortAudioLayer& parent,
                        const int16_t* inputBuffer,
                        int16_t* outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags);

    int paIOCallback(PortAudioLayer& parent,
                     const int16_t* inputBuffer,
                     int16_t* outputBuffer,
                     unsigned long framesPerBuffer,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags);
};

// ##################################################################################################

PortAudioLayer::PortAudioLayer(const AudioPreference& pref)
    : AudioLayer {pref}
    , pimpl_ {new PortAudioLayerImpl(*this, pref)}
{
    setHasNativeAEC(false);
    setHasNativeNS(false);

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

    // Notify of device changes in case this layer was reset based on a hotplug event.
    devicesChanged();
}

PortAudioLayer::~PortAudioLayer()
{
    stopStream();
}

std::vector<std::string>
PortAudioLayer::getCaptureDeviceList() const
{
    return pimpl_->getDevicesByType(AudioDeviceType::CAPTURE);
}

std::vector<std::string>
PortAudioLayer::getPlaybackDeviceList() const
{
    return pimpl_->getDevicesByType(AudioDeviceType::PLAYBACK);
}

int
PortAudioLayer::getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const
{
    auto devices = pimpl_->getDevicesByType(type);
    auto it = std::find_if(devices.cbegin(), devices.cend(), [&name](const auto& deviceName) {
        return deviceName == name;
    });
    return it != devices.end() ? std::distance(devices.cbegin(), it) : -1;
}

std::string
PortAudioLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    return pimpl_->getDeviceNameByType(index, type);
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
        std::unique_lock lock(mutex_);
        if (status_.load() != Status::Idle)
            return false;
        bool ret {false};
        if (fullDuplexMode)
            ret = pimpl_->initFullDuplexStream(*this);
        else
            ret = pimpl_->initOutputStream(*this);
        if (ret) {
            status_.store(Status::Started);
            startedCv_.notify_all();
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
    std::lock_guard lock(mutex_);
    if (status_.load() != Status::Started)
        return;

    bool stopped = false;
    bool outputStopped = false;
    switch (stream) {
    case AudioDeviceType::ALL:
        if (pimpl_->hasFullDuplexStream()) {
            stopped = pimpl_->paStopStream(Direction::IO);
            JAMI_DBG("PortAudioLayer full-duplex stream stopped");
            if (stopped) {
                playbackChanged(false);
                recordChanged(false);
                outputStopped = true;
            }
        } else {
            if (pimpl_->paStopStream(Direction::Output)) {
                playbackChanged(false);
                stopped = true;
                outputStopped = true;
                JAMI_DBG("PortAudioLayer output stream stopped");
            }
            if (pimpl_->paStopStream(Direction::Input)) {
                recordChanged(false);
                stopped = true;
                JAMI_DBG("PortAudioLayer input stream stopped");
            }
        }
        break;
    case AudioDeviceType::CAPTURE:
        if (pimpl_->paStopStream(Direction::Input)) {
            recordChanged(false);
            JAMI_DBG("PortAudioLayer input stream stopped");
            stopped = true;
        }
        break;
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        if (pimpl_->paStopStream(Direction::Output)) {
            playbackChanged(false);
            stopped = true;
            outputStopped = true;
            JAMI_DBG("PortAudioLayer output stream stopped");
        }
        break;
    }

    // Flush the ring buffers if any streams were stopped
    if (stopped) {
        JAMI_DBG("PortAudioLayer streams stopped, flushing buffers");
        flushUrgent();
        flushMain();
        if (outputStopped) {
            status_.store(Status::Idle);
            startedCv_.notify_all();
        }
    }
}

void
PortAudioLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    auto deviceName = pimpl_->getDeviceNameByType(index, type);
    switch (type) {
    case AudioDeviceType::PLAYBACK:
        preference.setPortAudioDevicePlayback(deviceName);
        break;
    case AudioDeviceType::CAPTURE:
        preference.setPortAudioDeviceRecord(deviceName);
        break;
    case AudioDeviceType::RINGTONE:
        preference.setPortAudioDeviceRingtone(deviceName);
        break;
    default:
        break;
    }
}

// ##################################################################################################

PortAudioLayer::PortAudioLayerImpl::PortAudioLayerImpl(PortAudioLayer& parent,
                                                       const AudioPreference& pref)
    : deviceRecord_ {pref.getPortAudioDeviceRecord()}
    , devicePlayback_ {pref.getPortAudioDevicePlayback()}
    , deviceRingtone_ {pref.getPortAudioDeviceRingtone()}
    , audioDeviceNotificationClient_ {new AudioDeviceNotificationClient}
{
    // Set up our callback to restart the layer on any device event
    audioDeviceNotificationClient_->setDeviceEventCallback(
        [this, &parent](const std::string& deviceName, const DeviceEventType event) {
            JAMI_LOG("PortAudioLayer device event: {}, {}",
                     deviceName.c_str(),
                     to_string(event).c_str());
            // Here we want to debounce the device events as a DefaultChanged could
            // follow a DeviceAdded event and we don't want to restart the layer twice
            if (!restartRequestPending_.exchange(true)) {
                std::thread([] {
                    // First wait for the debounce period to pass, allowing for multiple events
                    // to be grouped together (e.g. DeviceAdded -> DefaultChanged).
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    auto currentAudioManager = Manager::instance().getAudioManager();
                    Manager::instance().setAudioPlugin(currentAudioManager);
                }).detach();
            }
        });

    init(parent);
}

PortAudioLayer::PortAudioLayerImpl::~PortAudioLayerImpl()
{
    terminate();
}

void
PortAudioLayer::PortAudioLayerImpl::initInput(PortAudioLayer& parent)
{
    // convert out preference to an api index
    auto apiIndex = getApiIndexByType(AudioDeviceType::CAPTURE);

    // Pa_GetDefault[Comm]InputDevice returned paNoDevice or we already initialized the device
    if (apiIndex == paNoDevice || inputInitialized_)
        return;

    const auto inputDeviceInfo = Pa_GetDeviceInfo(apiIndex);
    if (!inputDeviceInfo) {
        // this represents complete failure after attempting a fallback to default
        JAMI_WARN("PortAudioLayer was unable to initialize input");
        deviceRecord_.clear();
        inputInitialized_ = true;
        return;
    }

    // if the device index is somehow no longer a device of the correct type, reset the
    // internal index to paNoDevice and reenter in an attempt to set the default
    // communications device
    if (inputDeviceInfo->maxInputChannels <= 0) {
        JAMI_WARN("PortAudioLayer was unable to initialize input, falling back to default device");
        deviceRecord_.clear();
        return initInput(parent);
    }

    // at this point, the device is of the correct type and can be opened
    parent.audioInputFormat_.sample_rate = inputDeviceInfo->defaultSampleRate;
    parent.audioInputFormat_.nb_channels = inputDeviceInfo->maxInputChannels;
    parent.hardwareInputFormatAvailable(parent.audioInputFormat_);
    JAMI_DBG("PortAudioLayer initialized input: %s {%d Hz, %d channels}",
             inputDeviceInfo->name,
             parent.audioInputFormat_.sample_rate,
             parent.audioInputFormat_.nb_channels);
    inputInitialized_ = true;
}

void
PortAudioLayer::PortAudioLayerImpl::initOutput(PortAudioLayer& parent)
{
    // convert out preference to an api index
    auto apiIndex = getApiIndexByType(AudioDeviceType::PLAYBACK);

    // Pa_GetDefault[Comm]OutputDevice returned paNoDevice or we already initialized the device
    if (apiIndex == paNoDevice || outputInitialized_)
        return;

    const auto outputDeviceInfo = Pa_GetDeviceInfo(apiIndex);
    if (!outputDeviceInfo) {
        // this represents complete failure after attempting a fallback to default
        JAMI_WARN("PortAudioLayer was unable to initialize output");
        devicePlayback_.clear();
        outputInitialized_ = true;
        return;
    }

    // if the device index is somehow no longer a device of the correct type, reset the
    // internal index to paNoDevice and reenter in an attempt to set the default
    // communications device
    if (outputDeviceInfo->maxOutputChannels <= 0) {
        JAMI_WARN("PortAudioLayer was unable to initialize output, falling back to default device");
        devicePlayback_.clear();
        return initOutput(parent);
    }

    // at this point, the device is of the correct type and can be opened
    parent.audioFormat_.sample_rate = outputDeviceInfo->defaultSampleRate;
    parent.audioFormat_.nb_channels = outputDeviceInfo->maxOutputChannels;
    parent.hardwareFormatAvailable(parent.audioFormat_);
    JAMI_DBG("PortAudioLayer initialized output: %s {%d Hz, %d channels}",
             outputDeviceInfo->name,
             parent.audioFormat_.sample_rate,
             parent.audioFormat_.nb_channels);
    outputInitialized_ = true;
}

void
PortAudioLayer::PortAudioLayerImpl::init(PortAudioLayer& parent)
{
    JAMI_DBG("PortAudioLayer Init");
    const auto err = Pa_Initialize();
    auto apiIndex = Pa_GetDefaultHostApi();
    auto apiInfo = Pa_GetHostApiInfo(apiIndex);
    if (err != paNoError || apiInfo == nullptr) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        terminate();
        return;
    }

    apiInitialised_ = true;
    JAMI_DBG() << "Portaudio initialized using: " << apiInfo->name;

    initInput(parent);
    initOutput(parent);

    std::lock_guard lock(streamsMutex_);
    std::fill(std::begin(streams_), std::end(streams_), nullptr);
}

std::vector<std::string>
PortAudioLayer::PortAudioLayerImpl::getDevicesByType(AudioDeviceType type) const
{
    std::vector<std::string> devices;
    auto numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(numDevices));
    else {
        for (int i = 0; i < numDevices; i++) {
            const auto deviceInfo = Pa_GetDeviceInfo(i);
            if (type == AudioDeviceType::CAPTURE) {
                if (deviceInfo->maxInputChannels > 0)
                    devices.push_back(deviceInfo->name);
            } else if (deviceInfo->maxOutputChannels > 0)
                devices.push_back(deviceInfo->name);
        }
        // add the default device aliases if requested and if there are any devices of this type
        if (!devices.empty()) {
            // default comm (index:0)
            auto defaultDeviceName = getApiDefaultDeviceName(type, true);
            devices.insert(devices.begin(), "{{Default}} - " + defaultDeviceName);
        }
    }
    return devices;
}

int
PortAudioLayer::PortAudioLayerImpl::getIndexByType(AudioDeviceType type)
{
    auto devices = getDevicesByType(type);
    if (!devices.size()) {
        return 0;
    }
    std::string_view toMatch = (type == AudioDeviceType::CAPTURE
                                    ? deviceRecord_
                                    : (type == AudioDeviceType::PLAYBACK ? devicePlayback_
                                                                         : deviceRingtone_));
    auto it = std::find_if(devices.cbegin(), devices.cend(), [&toMatch](const auto& deviceName) {
        return deviceName == toMatch;
    });
    return it != devices.end() ? std::distance(devices.cbegin(), it) : 0;
}

std::string
PortAudioLayer::PortAudioLayerImpl::getDeviceNameByType(const int index, AudioDeviceType type)
{
    if (index == defaultIndex_)
        return {};

    auto devices = getDevicesByType(type);
    if (!devices.size() || index >= devices.size())
        return {};

    return devices.at(index);
}

PaDeviceIndex
PortAudioLayer::PortAudioLayerImpl::getApiIndexByType(AudioDeviceType type)
{
    auto numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(numDevices));
        return paNoDevice;
    } else {
        std::string_view toMatch = (type == AudioDeviceType::CAPTURE
                                        ? deviceRecord_
                                        : (type == AudioDeviceType::PLAYBACK ? devicePlayback_
                                                                             : deviceRingtone_));
        if (!toMatch.empty()) {
            for (int i = 0; i < numDevices; ++i) {
                if (const auto deviceInfo = Pa_GetDeviceInfo(i)) {
                    if (deviceInfo->name == toMatch)
                        return i;
                }
            }
        }
    }
    // If nothing was found, return the default device
    return type == AudioDeviceType::CAPTURE ? Pa_GetDefaultCommInputDevice()
                                            : Pa_GetDefaultCommOutputDevice();
}

std::string
PortAudioLayer::PortAudioLayerImpl::getApiDefaultDeviceName(AudioDeviceType type,
                                                            bool commDevice) const
{
    std::string deviceName {};
    PaDeviceIndex deviceIndex {paNoDevice};
    if (type == AudioDeviceType::CAPTURE) {
        deviceIndex = commDevice ? Pa_GetDefaultCommInputDevice() : Pa_GetDefaultInputDevice();
    } else {
        deviceIndex = commDevice ? Pa_GetDefaultCommOutputDevice() : Pa_GetDefaultOutputDevice();
    }
    if (const auto deviceInfo = Pa_GetDeviceInfo(deviceIndex)) {
        deviceName = deviceInfo->name;
    }
    return deviceName;
}

void
PortAudioLayer::PortAudioLayerImpl::terminate() const
{
    JAMI_DBG("PortAudioLayer terminate.");
    auto err = Pa_Terminate();
    if (err != paNoError)
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
}

// Unified PortAudio stream opener for input, output, or full-duplex
static PaError
openPaStream(PaStream** stream,
             PaDeviceIndex inputDeviceIndex,
             PaDeviceIndex outputDeviceIndex,
             PaStreamCallback* callback,
             void* user_data)
{
    const bool hasInput = inputDeviceIndex != paNoDevice;
    const bool hasOutput = outputDeviceIndex != paNoDevice;

    const PaDeviceInfo* inputInfo = hasInput ? Pa_GetDeviceInfo(inputDeviceIndex) : nullptr;
    const PaDeviceInfo* outputInfo = hasOutput ? Pa_GetDeviceInfo(outputDeviceIndex) : nullptr;

    PaStreamParameters inputParams;
    PaStreamParameters outputParams;
    PaStreamParameters* inputParamsPtr = nullptr;
    PaStreamParameters* outputParamsPtr = nullptr;

    if (hasInput && inputInfo) {
        inputParams.device = inputDeviceIndex;
        inputParams.channelCount = inputInfo->maxInputChannels;
        inputParams.sampleFormat = paInt16;
        inputParams.suggestedLatency = inputInfo->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;
        inputParamsPtr = &inputParams;
    }
    if (hasOutput && outputInfo) {
        outputParams.device = outputDeviceIndex;
        outputParams.channelCount = outputInfo->maxOutputChannels;
        outputParams.sampleFormat = paInt16;
        outputParams.suggestedLatency = outputInfo->defaultLowOutputLatency;
        outputParams.hostApiSpecificStreamInfo = nullptr;
        outputParamsPtr = &outputParams;
    }

    // Choose a working sample rate
    double sampleRate = 0.0;
    if (inputParamsPtr && outputParamsPtr) {
        sampleRate = std::min(inputInfo->defaultSampleRate, outputInfo->defaultSampleRate);
    } else if (inputParamsPtr) {
        sampleRate = inputInfo->defaultSampleRate;
    } else if (outputParamsPtr) {
        sampleRate = outputInfo->defaultSampleRate;
    }

    auto err = Pa_OpenStream(stream,
                             inputParamsPtr,
                             outputParamsPtr,
                             sampleRate,
                             paFramesPerBufferUnspecified,
                             paNoFlag,
                             callback,
                             user_data);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
    }
    return err;
}

static bool startPaStream(PaStream* stream)
{
    auto err = Pa_StartStream(stream);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initInputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Input Stream");
    std::lock_guard lock(streamsMutex_);
    auto& stream = streams_[Direction::Input];
    auto apiIndex = getApiIndexByType(AudioDeviceType::CAPTURE);
    if (apiIndex != paNoDevice) {
        auto err = openPaStream(
            &stream,
            apiIndex,
            paNoDevice,
            [](const void* inputBuffer,
               void* outputBuffer,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void* userData) -> int {
                auto layer = static_cast<PortAudioLayer*>(userData);
                return layer->pimpl_->paInputCallback(*layer,
                                                      static_cast<const int16_t*>(inputBuffer),
                                                      static_cast<int16_t*>(outputBuffer),
                                                      framesPerBuffer,
                                                      timeInfo,
                                                      statusFlags);
            },
            &parent);
        if (err != paNoError) {
            return false;
        }
    } else {
        JAMI_ERR("Error: No valid input device. There will be no mic.");
        return false;
    }

    JAMI_DBG("Starting PortAudio Input Stream");
    if (!startPaStream(stream))
        return false;

    parent.recordChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initOutputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Output Stream");
    std::lock_guard lock(streamsMutex_);
    auto& stream = streams_[Direction::Output];
    auto apiIndex = getApiIndexByType(AudioDeviceType::PLAYBACK);
    if (apiIndex != paNoDevice) {
        auto err = openPaStream(
            &stream,
            paNoDevice,
            apiIndex,
            [](const void* inputBuffer,
               void* outputBuffer,
               unsigned long framesPerBuffer,
               const PaStreamCallbackTimeInfo* timeInfo,
               PaStreamCallbackFlags statusFlags,
               void* userData) -> int {
                auto layer = static_cast<PortAudioLayer*>(userData);
                return layer->pimpl_->paOutputCallback(*layer,
                                                       static_cast<const int16_t*>(inputBuffer),
                                                       static_cast<int16_t*>(outputBuffer),
                                                       framesPerBuffer,
                                                       timeInfo,
                                                       statusFlags);
            },
            &parent);
        if (err != paNoError) {
            return false;
        }
    } else {
        JAMI_ERR("Error: No valid output device. There will be no sound.");
        return false;
    }

    JAMI_DBG("Starting PortAudio Output Stream");
    if (!startPaStream(stream))
        return false;

    parent.playbackChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initFullDuplexStream(PortAudioLayer& parent)
{
    auto apiIndexRecord = getApiIndexByType(AudioDeviceType::CAPTURE);
    auto apiIndexPlayback = getApiIndexByType(AudioDeviceType::PLAYBACK);
    if (apiIndexRecord == paNoDevice || apiIndexPlayback == paNoDevice) {
        JAMI_ERR("Error: Invalid input/output devices. There will be no audio.");
        return false;
    }

    JAMI_DBG("Open PortAudio Full-duplex input/output stream");
    std::lock_guard lock(streamsMutex_);
    auto& stream = streams_[Direction::IO];
    auto err = openPaStream(
        &stream,
        apiIndexRecord,
        apiIndexPlayback,
        [](const void* inputBuffer,
           void* outputBuffer,
           unsigned long framesPerBuffer,
           const PaStreamCallbackTimeInfo* timeInfo,
           PaStreamCallbackFlags statusFlags,
           void* userData) -> int {
            auto layer = static_cast<PortAudioLayer*>(userData);
            return layer->pimpl_->paIOCallback(*layer,
                                               static_cast<const int16_t*>(inputBuffer),
                                               static_cast<int16_t*>(outputBuffer),
                                               framesPerBuffer,
                                               timeInfo,
                                               statusFlags);
        },
        &parent);
    if (err != paNoError) {
        return false;
    }

    JAMI_DBG("Start PortAudio I/O Streams");
    if (!startPaStream(stream))
        return false;

    parent.recordChanged(true);
    parent.playbackChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::paStopStream(Direction streamDirection)
{
    std::lock_guard lock(streamsMutex_);
    PaStream* paStream = streams_[streamDirection];
    if (!paStream)
        return false;
    auto ret = Pa_IsStreamStopped(paStream);
    if (ret == 1) {
        JAMI_DBG("PortAudioLayer stream %d already stopped", streamDirection);
        return true;
    } else if (ret < 0) {
        JAMI_ERR("Pa_IsStreamStopped error: %s", Pa_GetErrorText(ret));
        return false;
    }
    auto err = Pa_StopStream(paStream);
    if (err != paNoError) {
        JAMI_ERR("Pa_StopStream error: %s", Pa_GetErrorText(err));
        return false;
    }
    err = Pa_CloseStream(paStream);
    if (err != paNoError) {
        JAMI_ERR("Pa_CloseStream error: %s", Pa_GetErrorText(err));
        return false;
    }
    JAMI_DBG("PortAudioLayer stream %d stopped", streamDirection);
    streams_[streamDirection] = nullptr;
    return true;
};

bool PortAudioLayer::PortAudioLayerImpl::hasFullDuplexStream() const
{
    std::lock_guard lock(streamsMutex_);
    return streams_[Direction::IO] != nullptr;
}

int
PortAudioLayer::PortAudioLayerImpl::paOutputCallback(PortAudioLayer& parent,
                                                     const int16_t* inputBuffer,
                                                     int16_t* outputBuffer,
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

    auto nFrames = toPlay->pointer()->nb_samples * toPlay->pointer()->ch_layout.nb_channels;
    std::copy_n((int16_t*) toPlay->pointer()->extended_data[0], nFrames, outputBuffer);
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paInputCallback(PortAudioLayer& parent,
                                                    const int16_t* inputBuffer,
                                                    int16_t* outputBuffer,
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
    if (parent.isCaptureMuted_ || inputBuffer == nullptr)
        libav_utils::fillWithSilence(inBuff->pointer());
    else
        std::copy_n(inputBuffer, nFrames, (int16_t*) inBuff->pointer()->extended_data[0]);
    parent.putRecorded(std::move(inBuff));
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paIOCallback(PortAudioLayer& parent,
                                                 const int16_t* inputBuffer,
                                                 int16_t* outputBuffer,
                                                 unsigned long framesPerBuffer,
                                                 const PaStreamCallbackTimeInfo* timeInfo,
                                                 PaStreamCallbackFlags statusFlags)
{
    paInputCallback(parent, inputBuffer, nullptr, framesPerBuffer, timeInfo, statusFlags);
    paOutputCallback(parent, nullptr, outputBuffer, framesPerBuffer, timeInfo, statusFlags);
    return paContinue;
}

} // namespace jami
