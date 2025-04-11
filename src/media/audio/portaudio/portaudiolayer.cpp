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
#include "preferences.h"

#include <portaudio.h>
#include <algorithm>
#include <windows.h>

namespace jami {

enum Direction { Input = 0, Output = 1, Ringtone = 2, IO = 3, End = 4 };

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
    bool initRingtoneStream(PortAudioLayer&);
    bool initFullDuplexStream(PortAudioLayer&);
    bool apiInitialised_ {false};
    void refreshDeviceList(PortAudioLayer& parent);

    std::vector<std::string> getDevicesByType(AudioDeviceType type) const;
    int getIndexByType(AudioDeviceType type);
    std::string getDeviceNameByType(const int index, AudioDeviceType type);
    PaDeviceIndex getApiIndexByType(AudioDeviceType type);
    std::string getApiDefaultDeviceName(AudioDeviceType type, bool commDevice) const;
    void removeStreamingDevice(AudioDeviceType type);
    AudioDeviceType getDeviceTypeByName(const std::string& deviceName);

    std::string deviceRecord_ {};
    std::string devicePlayback_ {};
    std::string deviceRingtone_ {};

    static constexpr const int defaultIndex_ {0};

    bool inputInitialized_ {false};
    bool outputInitialized_ {false};
    bool ringtoneInitialized_ {false};

    std::array<PaStream*, static_cast<int>(Direction::End)> streams_;

    std::vector<std::string> currentlyStreamingDevices_;

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

    int paRingtoneCallback(PortAudioLayer& parent,
                           const int16_t* inputBuffer,
                           int16_t* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags);

    IMMDeviceEnumerator* deviceEnumerator_ {nullptr};
    AudioDeviceNotificationClient* audioDeviceNotificationClient_ {nullptr};
};

// ##################################################################################################

PortAudioLayer::PortAudioLayer(const AudioPreference& pref)
    : AudioLayer {pref}
    , pimpl_ {new PortAudioLayerImpl(*this, pref)}
{
    setHasNativeAEC(false);
    setHasNativeNS(false);

    pimpl_->audioDeviceNotificationClient_->setDeviceEventCallback(
        [this](const std::string& deviceName, const DeviceEventType event) {
            JAMI_DBG("PortAudio device event for device: %s, event: %s",
                     deviceName.c_str(),
                     (event == DeviceEventType::BecameActive) ? "BecameActive"
                                                              : "BecameInactive");

            // Some specific scenarios we want to handle are:
            // - The device is in use and was just plugged in. In this case, we want to
            //   reinitialize the stream with the new device.
            // - The device is in use and was just unplugged. In this case, we want to stop the
            //   stream and fallback to the current default device.

            // We have the name, so let's check if it's in the list of currently streaming devices.
            // TODO: We need to compare including the default device prefix also.
            auto it = std::find_if(pimpl_->currentlyStreamingDevices_.cbegin(),
                                pimpl_->currentlyStreamingDevices_.cend(),
                                [&deviceName](const auto& device) {
                                    JAMI_DBG("Checking device: %s against %s", deviceName.c_str(),
                                             device.c_str());
                                    return deviceName.find(device) != std::string::npos;
                                });
            bool deviceInUse = it != pimpl_->currentlyStreamingDevices_.cend();

            // Refresh the device list to ensure we have the latest device information
            pimpl_->refreshDeviceList(*this);

            auto deviceType = pimpl_->getDeviceTypeByName(deviceName);
            if (deviceInUse) {
                JAMI_DBG("PortAudio device in use: %s", deviceName.c_str());
                if (event == DeviceEventType::BecameActive) {
                    JAMI_DBG("PortAudio device added, restarting stream for type: %d",
                             static_cast<int>(deviceType));
                    stopStream(deviceType);
                    startStream(deviceType);
                } else if (event == DeviceEventType::BecameInactive) {
                    JAMI_DBG("PortAudio device removed, stopping stream for type: %d",
                             static_cast<int>(deviceType));
                    stopStream(deviceType);
                    // For ringtone, if removed, try to fall back to the playback device
                    if (deviceType == AudioDeviceType::RINGTONE) {
                        JAMI_DBG("Falling back to playback device for ringtone");
                        startStream(AudioDeviceType::PLAYBACK);
                    }
                }
            } else {
                JAMI_DBG("PortAudio device not in use: %s", deviceName.c_str());
                // In this case, if a new device is added, and we weren't able to stream for it's type
                // previously, then we want to reinitialize the stream with the newly available device.
                // It is understood that the device selection in this case will not modify the user's
                // default device preference.
                // Check if there are any devices of the same type available, and if they are currently
                // streaming or not.
                auto hasStreamOfType = false;
                for (auto& device: pimpl_->currentlyStreamingDevices_) {
                    auto streamingDeviceType = pimpl_->getDeviceTypeByName(device);
                    if (streamingDeviceType == deviceType) {
                        hasStreamOfType = true;
                        break;
                    }
                }
                if (hasStreamOfType) {
                    JAMI_DBG("PortAudio device of type %d is already streaming, no action needed",
                             static_cast<int>(deviceType));
                } else {
                    JAMI_DBG("PortAudio device of type %d is not streaming, restarting stream",
                             static_cast<int>(deviceType));
                    startStream(deviceType);
                }
            }
        });

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
    auto devices = pimpl_->getDevicesByType(AudioDeviceType::CAPTURE);
    // Print them all
    for (const auto& device : devices) {
        JAMI_DBG("PortAudio capture device: %s", device.c_str());
    }
    return devices;
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
    (void) index;
    (void) type;
    return {};
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

    bool success = false;

    switch (stream) {
    case AudioDeviceType::ALL:
        // Try full duplex first, fall back to separate streams if needed
        success = pimpl_->initFullDuplexStream(*this);
        if (!success) {
            pimpl_->initInputStream(*this);
            success = pimpl_->initOutputStream(*this);
        }
        break;
    case AudioDeviceType::CAPTURE:
        success = pimpl_->initInputStream(*this);
        break;
    case AudioDeviceType::PLAYBACK:
        success = pimpl_->initOutputStream(*this);
        break;
    case AudioDeviceType::RINGTONE:
        success = pimpl_->initRingtoneStream(*this);
        // Only fall back to playback if explicitly requested and ringtone truly fails
        if (!success) {
            JAMI_WARN("Could not initialize ringtone stream, falling back to default "
                      "communications device");
            // Clear the ringtone device to force using the default
            pimpl_->deviceRingtone_.clear();
            success = pimpl_->initRingtoneStream(*this);
        }
        break;
    }

    if (success) {
        // Flush audio buffers
        flushUrgent();
        flushMain();
    }
}

void
PortAudioLayer::stopStream(AudioDeviceType stream)
{
    auto stopStream = [](PaStream* stream) -> bool {
        if (!stream || Pa_IsStreamStopped(stream) != paNoError)
            return false;

        auto err = Pa_StopStream(stream);
        if (err != paNoError) {
            JAMI_ERR("Pa_StopStream error: %s", Pa_GetErrorText(err));
            return false;
        }

        err = Pa_CloseStream(stream);
        if (err != paNoError) {
            JAMI_ERR("Pa_CloseStream error: %s", Pa_GetErrorText(err));
            return false;
        }

        return true;
    };

    bool stopped = false;

    switch (stream) {
    case AudioDeviceType::ALL:
        if (pimpl_->streams_[Direction::IO]) {
            stopped = stopStream(pimpl_->streams_[Direction::IO]);
            if (stopped) {
                pimpl_->removeStreamingDevice(AudioDeviceType::PLAYBACK);
                pimpl_->removeStreamingDevice(AudioDeviceType::CAPTURE);
                recordChanged(false);
                playbackChanged(false);
                JAMI_DBG("PortAudioLayer I/O streams stopped");
            }
        } else {
            bool inputStopped = stopStream(pimpl_->streams_[Direction::Input]);
            bool outputStopped = stopStream(pimpl_->streams_[Direction::Output]);
            bool ringtoneStopped = stopStream(pimpl_->streams_[Direction::Ringtone]);

            stopped = inputStopped || outputStopped || ringtoneStopped;

            if (inputStopped) {
                pimpl_->removeStreamingDevice(AudioDeviceType::CAPTURE);
                recordChanged(false);
            }

            if (outputStopped) {
                pimpl_->removeStreamingDevice(AudioDeviceType::PLAYBACK);
                playbackChanged(false);
            }

            if (ringtoneStopped) {
                pimpl_->removeStreamingDevice(AudioDeviceType::RINGTONE);
                JAMI_DBG("PortAudioLayer ringtone stream stopped");
            }
        }
        break;
    case AudioDeviceType::CAPTURE:
        stopped = stopStream(pimpl_->streams_[Direction::Input]);
        if (stopped) {
            pimpl_->removeStreamingDevice(AudioDeviceType::CAPTURE);
            recordChanged(false);
            JAMI_DBG("PortAudioLayer input stream stopped");
        }
        break;
    case AudioDeviceType::PLAYBACK:
        stopped = stopStream(pimpl_->streams_[Direction::Output]);
        if (stopped) {
            pimpl_->removeStreamingDevice(AudioDeviceType::PLAYBACK);
            playbackChanged(false);
            JAMI_DBG("PortAudioLayer output stream stopped");
        }
        break;
    case AudioDeviceType::RINGTONE:
        stopped = stopStream(pimpl_->streams_[Direction::Ringtone]);
        if (stopped) {
            pimpl_->removeStreamingDevice(AudioDeviceType::RINGTONE);
            JAMI_DBG("PortAudioLayer ringtone stream stopped");
        }
        break;
    }

    // Flush the ring buffers if any stream was stopped
    if (stopped) {
        flushUrgent();
        flushMain();
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
    , audioDeviceNotificationClient_ {new AudioDeviceNotificationClient()}
{
    // Set up the audio device notification client
    HRESULT hr = CoInitialize(nullptr);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                              nullptr,
                              CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator),
                              (void**) &deviceEnumerator_);

        if (SUCCEEDED(hr)) {
            hr = deviceEnumerator_->RegisterEndpointNotificationCallback(
                audioDeviceNotificationClient_);

            if (FAILED(hr)) {
                JAMI_ERR() << "Failed to register notification callback: " << std::hex << hr;
                deviceEnumerator_->Release();
                deviceEnumerator_ = nullptr;
            }
        } else {
            JAMI_ERR() << "Failed to create MMDeviceEnumerator: " << std::hex << hr;
        }
    } else {
        JAMI_ERR() << "CoInitialize failed: " << std::hex << hr;
    }

    init(parent);
}

PortAudioLayer::PortAudioLayerImpl::~PortAudioLayerImpl()
{
    // Clean up audio device notification client
    if (deviceEnumerator_) {
        try {
            deviceEnumerator_->UnregisterEndpointNotificationCallback(
                audioDeviceNotificationClient_);
            deviceEnumerator_->Release();
        } catch (...) {
            JAMI_ERR("Exception during cleanup of device enumerator");
        }
    }

    if (audioDeviceNotificationClient_) {
        try {
            audioDeviceNotificationClient_->Release();
        } catch (...) {
            JAMI_ERR("Exception during cleanup of notification client");
        }
    }

    CoUninitialize();

    terminate();
}

void
PortAudioLayer::PortAudioLayerImpl::initInput(PortAudioLayer& parent)
{
    // Skip if already initialized or if we have no valid device
    auto apiIndex = getApiIndexByType(AudioDeviceType::CAPTURE);
    if (apiIndex == paNoDevice || inputInitialized_)
        return;

    const auto inputDeviceInfo = Pa_GetDeviceInfo(apiIndex);
    if (!inputDeviceInfo) {
        JAMI_WARN("PortAudioLayer was unable to initialize input");
        deviceRecord_.clear();
        inputInitialized_ = true;
        return;
    }

    // Verify the device is actually an input device
    if (inputDeviceInfo->maxInputChannels <= 0) {
        JAMI_WARN("PortAudioLayer was unable to initialize input, falling back to default device");
        deviceRecord_.clear();
        return initInput(parent); // Try again with default device
    }

    // Configure the audio format based on the device capabilities
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
    // Skip if already initialized or if we have no valid device
    auto apiIndex = getApiIndexByType(AudioDeviceType::PLAYBACK);
    if (apiIndex == paNoDevice || outputInitialized_)
        return;

    const auto outputDeviceInfo = Pa_GetDeviceInfo(apiIndex);
    if (!outputDeviceInfo) {
        JAMI_WARN("PortAudioLayer was unable to initialize output");
        devicePlayback_.clear();
        outputInitialized_ = true;
        return;
    }

    // Verify the device is actually an output device
    if (outputDeviceInfo->maxOutputChannels <= 0) {
        JAMI_WARN("PortAudioLayer was unable to initialize output, falling back to default device");
        devicePlayback_.clear();
        return initOutput(parent); // Try again with default device
    }

    // Configure the audio format based on the device capabilities
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

    // Initialize PortAudio
    const auto err = Pa_Initialize();
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return;
    }

    // Get host API information
    auto apiIndex = Pa_GetDefaultHostApi();
    auto apiInfo = Pa_GetHostApiInfo(apiIndex);
    if (!apiInfo) {
        JAMI_ERR("PortAudioLayer error: Could not get host API info");
        terminate();
        return;
    }

    apiInitialised_ = true;
    JAMI_DBG() << "Portaudio initialized using: " << apiInfo->name;

    // Initialize input and output devices
    initInput(parent);
    initOutput(parent);

    // Clear stream pointers
    std::fill(std::begin(streams_), std::end(streams_), nullptr);
}

std::vector<std::string>
PortAudioLayer::PortAudioLayerImpl::getDevicesByType(AudioDeviceType type) const
{
    std::vector<std::string> devices;
    auto numDevices = Pa_GetDeviceCount();

    if (numDevices < 0) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(numDevices));
        return devices;
    }

    for (int i = 0; i < numDevices; i++) {
        const auto deviceInfo = Pa_GetDeviceInfo(i);
        if (!deviceInfo)
            continue;

        bool isMatchingType = (type == AudioDeviceType::CAPTURE)
                                  ? (deviceInfo->maxInputChannels > 0)
                                  : (deviceInfo->maxOutputChannels > 0);

        if (isMatchingType)
            devices.push_back(deviceInfo->name);
    }

    // Add default device alias if we have devices of this type
    if (!devices.empty()) {
        auto defaultDeviceName = getApiDefaultDeviceName(type, true);
        devices.insert(devices.begin(), "{{Default}} - " + defaultDeviceName);
    }

    return devices;
}

int
PortAudioLayer::PortAudioLayerImpl::getIndexByType(AudioDeviceType type)
{
    auto devices = getDevicesByType(type);
    if (devices.empty())
        return 0;

    std::string_view devicePref = (type == AudioDeviceType::CAPTURE)
                                      ? deviceRecord_
                                      : (type == AudioDeviceType::PLAYBACK ? devicePlayback_
                                                                           : deviceRingtone_);

    if (devicePref.empty())
        return 0;

    auto it = std::find(devices.begin(), devices.end(), devicePref);
    return (it != devices.end()) ? std::distance(devices.begin(), it) : 0;
}

std::string
PortAudioLayer::PortAudioLayerImpl::getDeviceNameByType(const int index, AudioDeviceType type)
{
    // if (index == defaultIndex_) {
    //     return {};
    // }

    auto devices = getDevicesByType(type);
    return (devices.empty() || index >= devices.size()) ? std::string {} : devices[index];
}

PaDeviceIndex
PortAudioLayer::PortAudioLayerImpl::getApiIndexByType(AudioDeviceType type)
{
    // If no device preference is set, return the default device
    std::string_view devicePref = (type == AudioDeviceType::CAPTURE)
                                      ? deviceRecord_
                                      : (type == AudioDeviceType::PLAYBACK ? devicePlayback_
                                                                           : deviceRingtone_);

    if (devicePref.empty()) {
        JAMI_DBG("PortAudioLayer no device preference set, using default device");
        return type == AudioDeviceType::CAPTURE ? Pa_GetDefaultCommInputDevice()
                                                : Pa_GetDefaultCommOutputDevice();
    }

    // Search for the device by name
    auto numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(numDevices));
        return paNoDevice;
    }

    for (int i = 0; i < numDevices; ++i) {
        if (const auto deviceInfo = Pa_GetDeviceInfo(i)) {
            JAMI_DBG("Checking PortAudioLayer device: %d, %s", i, deviceInfo->name);
            // It's possible that the device name contains a prefix like "{{Default}} - "
            // We need to check if the device name contains the preference string
            // instead of checking for exact match. Easy to just remove the prefix.
            if (devicePref.find(deviceInfo->name) != std::string::npos) {
                JAMI_DBG("PortAudioLayer found device: %s", devicePref.data());
                return i;
            }
        }
    }

    JAMI_ERR("PortAudioLayer error: Device %s not found", devicePref.data());
    return paNoDevice;
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
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
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
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
}

bool
PortAudioLayer::PortAudioLayerImpl::initInputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Input Stream");
    auto apiIndex = getApiIndexByType(AudioDeviceType::CAPTURE);
    if (apiIndex == paNoDevice) {
        JAMI_ERR("Error: No valid input device. There will be no mic.");
        return false;
    }

    openStreamDevice(
        &streams_[Direction::Input],
        apiIndex,
        Direction::Input,
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

    JAMI_DBG("Starting PortAudio Input Stream");
    auto err = Pa_StartStream(streams_[Direction::Input]);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return false;
    }

    // Add to currently streaming devices list
    auto deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::CAPTURE),
                                          AudioDeviceType::CAPTURE);
    currentlyStreamingDevices_.push_back(deviceName);
    JAMI_DBG("PortAudioLayer input stream started on device: %s", deviceName.c_str());

    parent.recordChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initOutputStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Output Stream");
    auto apiIndex = getApiIndexByType(AudioDeviceType::PLAYBACK);
    if (apiIndex == paNoDevice) {
        JAMI_ERR("Error: No valid output device. There will be no sound.");
        return false;
    }

    openStreamDevice(
        &streams_[Direction::Output],
        apiIndex,
        Direction::Output,
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

    JAMI_DBG("Starting PortAudio Output Stream");
    auto err = Pa_StartStream(streams_[Direction::Output]);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return false;
    }

    // Add to currently streaming devices list
    auto deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::PLAYBACK),
                                          AudioDeviceType::PLAYBACK);
    currentlyStreamingDevices_.push_back(deviceName);
    JAMI_DBG("PortAudioLayer output stream started on device: %s", deviceName.c_str());

    parent.playbackChanged(true);
    return true;
}

bool
PortAudioLayer::PortAudioLayerImpl::initRingtoneStream(PortAudioLayer& parent)
{
    JAMI_DBG("Open PortAudio Ringtone Stream");
    auto apiIndex = getApiIndexByType(AudioDeviceType::RINGTONE);
    if (apiIndex == paNoDevice) {
        JAMI_ERR("Error: No valid ringtone device. Fallback to default communications device.");
        // Instead of immediately falling back to playback device, try the default communications device
        apiIndex = Pa_GetDefaultCommOutputDevice();
        if (apiIndex == paNoDevice) {
            JAMI_ERR("Error: No valid default communications device for ringtone.");
            return false;
        }
        // Update the ringtone device preference with the default communications device
        if (const auto deviceInfo = Pa_GetDeviceInfo(apiIndex)) {
            deviceRingtone_ = deviceInfo->name;
            JAMI_DBG("Using default communications device for ringtone: %s",
                     deviceRingtone_.c_str());
        }
    }

    // We no longer check if the ringtone device is the same as the playback device
    // This ensures both devices can be set independently, even to the same physical device

    openStreamDevice(
        &streams_[Direction::Ringtone],
        apiIndex,
        Direction::Output, // Ringtone is an output stream
        [](const void* inputBuffer,
           void* outputBuffer,
           unsigned long framesPerBuffer,
           const PaStreamCallbackTimeInfo* timeInfo,
           PaStreamCallbackFlags statusFlags,
           void* userData) -> int {
            auto layer = static_cast<PortAudioLayer*>(userData);
            return layer->pimpl_->paRingtoneCallback(*layer,
                                                     static_cast<const int16_t*>(inputBuffer),
                                                     static_cast<int16_t*>(outputBuffer),
                                                     framesPerBuffer,
                                                     timeInfo,
                                                     statusFlags);
        },
        &parent);

    JAMI_DBG("Starting PortAudio Ringtone Stream");
    auto err = Pa_StartStream(streams_[Direction::Ringtone]);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return false;
    }

    // Add to currently streaming devices list
    auto deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::RINGTONE),
                                          AudioDeviceType::RINGTONE);
    currentlyStreamingDevices_.push_back(deviceName);
    JAMI_DBG("PortAudioLayer ringtone stream started on device: %s", deviceName.c_str());
    ringtoneInitialized_ = true;

    JAMI_DBG("Ringtone stream started on device: %s", deviceName.c_str());
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
    openFullDuplexStream(
        &streams_[Direction::IO],
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

    JAMI_DBG("Start PortAudio I/O Streams");
    auto err = Pa_StartStream(streams_[Direction::IO]);
    if (err != paNoError) {
        JAMI_ERR("PortAudioLayer error: %s", Pa_GetErrorText(err));
        return false;
    }

    // Add to currently streaming devices list
    auto deviceNamePlayback = getDeviceNameByType(getIndexByType(AudioDeviceType::PLAYBACK),
                                                 AudioDeviceType::PLAYBACK);
    auto deviceNameRecord = getDeviceNameByType(getIndexByType(AudioDeviceType::CAPTURE),
                                                 AudioDeviceType::CAPTURE);
    currentlyStreamingDevices_.push_back(deviceNamePlayback);
    currentlyStreamingDevices_.push_back(deviceNameRecord);
    JAMI_DBG("PortAudioLayer I/O streams started on devices: %s, %s",
             deviceNamePlayback.c_str(),
             deviceNameRecord.c_str());

    parent.recordChanged(true);
    parent.playbackChanged(true);
    return true;
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

    // Get data from the ring buffer
    auto toPlay = parent.getPlayback(parent.audioFormat_, framesPerBuffer);

    if (!toPlay) {
        // No data available, output silence
        std::fill_n(outputBuffer, framesPerBuffer * parent.audioFormat_.nb_channels, 0);
        return paContinue;
    }

    // Copy audio data to output buffer
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

    // Create audio frame for the incoming data
    auto inBuff = std::make_shared<AudioFrame>(parent.audioInputFormat_, framesPerBuffer);
    auto nFrames = framesPerBuffer * parent.audioInputFormat_.nb_channels;

    if (parent.isCaptureMuted_) {
        // If capture is muted, fill with silence
        libav_utils::fillWithSilence(inBuff->pointer());
    } else {
        // Otherwise copy input data
        std::copy_n(inputBuffer, nFrames, (int16_t*) inBuff->pointer()->extended_data[0]);
    }

    // Put recorded data in the ring buffer
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
    // Handle input and output in a single callback
    paInputCallback(parent, inputBuffer, nullptr, framesPerBuffer, timeInfo, statusFlags);
    paOutputCallback(parent, nullptr, outputBuffer, framesPerBuffer, timeInfo, statusFlags);
    return paContinue;
}

int
PortAudioLayer::PortAudioLayerImpl::paRingtoneCallback(PortAudioLayer& parent,
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

    // Process urgent buffer (ringtones) - implemented via the ring buffer in AudioLayer
    // Unlike the output callback, we directly use urgent buffer for ringtones
    auto urgent = parent.getToRing(parent.audioFormat_, framesPerBuffer);

    if (!urgent || parent.isRingtoneMuted()) {
        // No data available or ringtone is muted, output silence
        std::fill_n(outputBuffer, framesPerBuffer * parent.audioFormat_.nb_channels, 0);
        return paContinue;
    }

    // Copy audio data to output buffer
    auto nFrames = urgent->pointer()->nb_samples * urgent->pointer()->ch_layout.nb_channels;
    std::copy_n((int16_t*) urgent->pointer()->extended_data[0], nFrames, outputBuffer);

    return paContinue;
}

void
PortAudioLayer::PortAudioLayerImpl::removeStreamingDevice(AudioDeviceType type)
{
    auto deviceName = getDeviceNameByType(getIndexByType(type), type);
    currentlyStreamingDevices_.erase(std::remove(currentlyStreamingDevices_.begin(),
                                                 currentlyStreamingDevices_.end(),
                                                 deviceName),
                                     currentlyStreamingDevices_.end());
    JAMI_DBG("PortAudioLayer removed streaming device: %s", deviceName.c_str());
}

AudioDeviceType
PortAudioLayer::PortAudioLayerImpl::getDeviceTypeByName(const std::string& deviceName)
{
    AudioDeviceType deviceType = AudioDeviceType::ALL;
    if (getDeviceNameByType(getIndexByType(AudioDeviceType::PLAYBACK),
                                    AudioDeviceType::PLAYBACK)
        == deviceName) {
        deviceType = AudioDeviceType::PLAYBACK;
    } else if (getDeviceNameByType(getIndexByType(
                                                AudioDeviceType::CAPTURE),
                                            AudioDeviceType::CAPTURE)
                == deviceName) {
        deviceType = AudioDeviceType::CAPTURE;
    } else if (getDeviceNameByType(getIndexByType(
                                                AudioDeviceType::RINGTONE),
                                            AudioDeviceType::RINGTONE)
                == deviceName) {
        deviceType = AudioDeviceType::RINGTONE;
    }
    return deviceType;
}

std::shared_ptr<AudioFrame>
PortAudioLayer::getUrgent(AudioFormat format, size_t samples)
{
    return getToRing(format, samples);
}

void
PortAudioLayer::PortAudioLayerImpl::refreshDeviceList(PortAudioLayer& parent)
{
    JAMI_DBG("Refreshing PortAudio device list");

    // Save currently active streams and their devices
    struct StreamState
    {
        AudioDeviceType type;
        Direction direction;
        std::string deviceName; // Added to save the actual device name
    };
    std::vector<StreamState> activeStreams;

    for (size_t i = 0; i < static_cast<int>(Direction::End); i++) {
        if (streams_[i] && Pa_IsStreamActive(streams_[i]) == 1) {
            StreamState state;

            if (i == static_cast<int>(Direction::Input)) {
                state.type = AudioDeviceType::CAPTURE;
                state.direction = Direction::Input;
                state.deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::CAPTURE),
                                                       AudioDeviceType::CAPTURE);
                activeStreams.push_back(state);
            } else if (i == static_cast<int>(Direction::Output)) {
                state.type = AudioDeviceType::PLAYBACK;
                state.direction = Direction::Output;
                state.deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::PLAYBACK),
                                                       AudioDeviceType::PLAYBACK);
                activeStreams.push_back(state);
            } else if (i == static_cast<int>(Direction::Ringtone)) {
                state.type = AudioDeviceType::RINGTONE;
                state.direction = Direction::Ringtone;
                state.deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::RINGTONE),
                                                       AudioDeviceType::RINGTONE);
                activeStreams.push_back(state);
            } else if (i == static_cast<int>(Direction::IO)) {
                // For IO, we need both capture and playback
                StreamState capState;
                capState.type = AudioDeviceType::CAPTURE;
                capState.direction = Direction::IO;
                capState.deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::CAPTURE),
                                                          AudioDeviceType::CAPTURE);

                StreamState playState;
                playState.type = AudioDeviceType::PLAYBACK;
                playState.direction = Direction::IO;
                playState.deviceName = getDeviceNameByType(getIndexByType(AudioDeviceType::PLAYBACK),
                                                           AudioDeviceType::PLAYBACK);

                activeStreams.push_back(capState);
                activeStreams.push_back(playState);
            }
        }
    }

    // Copy current device preferences
    std::string savedDeviceRecord = deviceRecord_;
    std::string savedDevicePlayback = devicePlayback_;
    std::string savedDeviceRingtone = deviceRingtone_;

    // Stop all streams
    for (size_t i = 0; i < static_cast<int>(Direction::End); i++) {
        if (streams_[i]) {
            if (Pa_IsStreamActive(streams_[i]) == 1) {
                JAMI_DBG("Stopping stream %zu before refresh", i);
                Pa_StopStream(streams_[i]);
            }
            Pa_CloseStream(streams_[i]);
            streams_[i] = nullptr;
        }
    }

    // Clear the streaming devices list
    currentlyStreamingDevices_.clear();

    // Terminate and re-initialize PortAudio API
    if (apiInitialised_) {
        JAMI_DBG("Terminating PortAudio to refresh device list");
        apiInitialised_ = false;
        terminate();
    }

    init(parent);

    // Restore device preferences
    deviceRecord_ = savedDeviceRecord;
    devicePlayback_ = savedDevicePlayback;
    deviceRingtone_ = savedDeviceRingtone;

    // Log the refreshed device list
    auto numDevices = Pa_GetDeviceCount();
    if (numDevices > 0) {
        JAMI_DBG("Refreshed PortAudio device list (%d devices)", numDevices);
        for (auto i = 0; i < numDevices; i++) {
            if (const auto deviceInfo = Pa_GetDeviceInfo(i)) {
                JAMI_DBG("PortAudio device: %d, %s", i, deviceInfo->name);
            }
        }
    } else {
        JAMI_WARN("No PortAudio devices available after refresh");
    }

    // Check if previously active devices are still available
    // Use the saved device names to check availability
    for (auto& stream : activeStreams) {
        std::vector<std::string> availableDevices = getDevicesByType(stream.type);
        bool deviceStillExists = false;

        // If we have a device name, check if it's still in the list
        if (!stream.deviceName.empty()) {
            deviceStillExists = std::find(availableDevices.begin(),
                                          availableDevices.end(),
                                          stream.deviceName)
                                != availableDevices.end();
        }

        // If the device was removed, clear the preference to use default
        if (!deviceStillExists) {
            JAMI_WARN("Device '%s' for %s is no longer available, will use default",
                      stream.deviceName.c_str(),
                      (stream.type == AudioDeviceType::CAPTURE)    ? "capture"
                      : (stream.type == AudioDeviceType::PLAYBACK) ? "playback"
                                                                   : "ringtone");

            // Clear the corresponding device preference
            switch (stream.type) {
            case AudioDeviceType::CAPTURE:
                deviceRecord_.clear();
                break;
            case AudioDeviceType::PLAYBACK:
                devicePlayback_.clear();
                break;
            case AudioDeviceType::RINGTONE:
                deviceRingtone_.clear();
                break;
            default:
                break;
            }
        }
    }

    // Restart previously active streams
    // We use a set to avoid restarting the same stream type multiple times
    std::set<AudioDeviceType> restartedTypes;
    bool hasIOStream = false;

    for (const auto& stream : activeStreams) {
        // Skip if we've already restarted this type
        if (restartedTypes.find(stream.type) != restartedTypes.end())
            continue;

        if (stream.direction == Direction::IO) {
            // Only restart IO stream once
            if (!hasIOStream) {
                JAMI_DBG("Restarting IO stream after refresh");
                if (initFullDuplexStream(parent)) {
                    restartedTypes.insert(AudioDeviceType::CAPTURE);
                    restartedTypes.insert(AudioDeviceType::PLAYBACK);
                    hasIOStream = true;
                }
            }
        } else {
            bool success = false;

            switch (stream.type) {
            case AudioDeviceType::CAPTURE:
                JAMI_DBG("Restarting capture stream after refresh");
                success = initInputStream(parent);
                break;
            case AudioDeviceType::PLAYBACK:
                JAMI_DBG("Restarting playback stream after refresh");
                success = initOutputStream(parent);
                break;
            case AudioDeviceType::RINGTONE:
                JAMI_DBG("Restarting ringtone stream after refresh");
                success = initRingtoneStream(parent);
                break;
            default:
                break;
            }

            if (success) {
                restartedTypes.insert(stream.type);
            }
        }
    }

    // Notify about device changes
    parent.devicesChanged();
}

} // namespace jami
