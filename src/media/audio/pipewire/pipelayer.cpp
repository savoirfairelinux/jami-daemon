/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include "pipelayer.h"
#include "pipestream.h"
#include "preferences.h"
#include "manager.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <pipewire/extensions/metadata.h>
#include <spa/utils/dict.h>
#include <spa/utils/result.h>

namespace jami {


static int
metadata_property(void *data, uint32_t id, const char *key, const char *type, const char *value)
{
    if (key && value) {
        JAMI_LOG("Metadata: {:d} {:s} {:s} = {:s}", id, key, type, value);
    }
    return 0;
}

static const struct pw_metadata_events metadata_events = {
    PW_VERSION_METADATA_EVENTS,
    .property = metadata_property,
};

static PwDeviceInfo defaultDeviceInfo {
    PW_ID_ANY,           // id - lets PipeWire choose the default
    "",                  // name - empty for default
    "default",           // description
    2,                   // channels - default stereo
    48000,               // rate - default sample rate
    PW_STREAM_FLAG_NONE  // flags
};

const PwDeviceInfo*
getDeviceInfo(const std::vector<PwDeviceInfo>& list, const std::string& name)
{
    // Empty name means use system default device
    if (name.empty()) {
        JAMI_LOG("Using system default device (PW_ID_ANY)");
        return &defaultDeviceInfo;
    }
    
    if (list.empty()) {
        JAMI_WARNING("getDeviceInfo: device list is empty");
        return nullptr;
    }
    auto dev_info = std::find_if(list.begin(), list.end(), 
        [&name](const PwDeviceInfo& info) { return info.name == name; });
    
    if (dev_info == list.end()) {
        JAMI_WARNING("Preferred device {:s} not found in device list, selecting default {:s} instead.",
                  name,
                  list.front().name);
        return &list.front();
    }
    return &(*dev_info);
}


PipeWireLayer::PipeWireLayer(AudioPreference& pref)
    : AudioLayer(pref)
{
    JAMI_LOG("PipeWireLayer init");
    pw_init(nullptr, nullptr);
    loop_ = pw_thread_loop_new("PipeWire-loop", nullptr);
    context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
    core_ = pw_context_connect(context_, nullptr, 0);

    //pw_core_add_listener(core_, &core_listener, this);

    JAMI_LOG("PipeWireLayer pw_core_get_registry");
    registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
    registry_events = pw_registry_events {
        PW_VERSION_REGISTRY_EVENTS,
        .global = &PipeWireLayer::registryEventCallback,
        .global_remove = &PipeWireLayer::registryEventCallbackRemove,
    };

    JAMI_LOG("PipeWireLayer pw_registry_add_listener");
    pw_registry_add_listener(registry_, &registry_listener, &registry_events, this);
    pw_thread_loop_start(loop_);
    
    // Wait for initial device enumeration
    updateDeviceList();
    devicesEnumerated_ = true;
}

PipeWireLayer::~PipeWireLayer()
{
    stopStream(AudioDeviceType::ALL);
    pw_thread_loop_stop(loop_);
    pw_proxy_destroy((pw_proxy*)registry_);
    pw_core_disconnect(core_);
    pw_context_destroy(context_);
    pw_thread_loop_destroy(loop_);
    pw_deinit();
}

void PipeWireLayer::startStream(AudioDeviceType type)
{
    // Ensure devices are enumerated before trying to start streams
    if (!devicesEnumerated_) {
        updateDeviceList();
        devicesEnumerated_ = true;
    }
    
    // Get device info before locking PipeWire loop
    const PwDeviceInfo* playbackInfo = nullptr;
    const PwDeviceInfo* ringtoneInfo = nullptr;
    const PwDeviceInfo* captureInfo = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(deviceListMutex_);
        if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL) {
            playbackInfo = getDeviceInfo(sinkList_, pref_.getPulseDevicePlayback());
            if (!playbackInfo) {
                JAMI_ERROR("Failed to get device info for playback device: {:s}", 
                         pref_.getPulseDevicePlayback());
            }
        }
        
        if (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL) {
            ringtoneInfo = getDeviceInfo(sinkList_, pref_.getPulseDeviceRingtone());
            if (!ringtoneInfo) {
                JAMI_ERROR("Failed to get device info for ringtone device: {:s}", 
                         pref_.getPulseDeviceRingtone());
            }
        }
        
        if (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL) {
            captureInfo = getDeviceInfo(sourceList_, pref_.getPulseDeviceRecord());
            if (!captureInfo) {
                JAMI_ERROR("Failed to get device info for capture device: {:s}", 
                         pref_.getPulseDeviceRecord());
            }
        }
    }
    
    pw_thread_loop_lock(loop_);

    try {
        // Create Streams using the device info we got earlier
        if (playbackInfo) {
            createStream(playback_,
                         AudioDeviceType::PLAYBACK,
                         *playbackInfo,
                         std::bind(&PipeWireLayer::writeToSpeaker, this, std::placeholders::_1));
        }
        
        if (ringtoneInfo) {
            createStream(ringtone_,
                         AudioDeviceType::RINGTONE,
                         *ringtoneInfo,
                         std::bind(&PipeWireLayer::ringtoneToSpeaker, this, std::placeholders::_1));
        }
        
        if (captureInfo) {
            createStream(record_,
                         AudioDeviceType::CAPTURE,
                         *captureInfo,
                         std::bind(&PipeWireLayer::readFromMic, this, std::placeholders::_1));
        }

        status_ = Status::Started;
        startedCv_.notify_all();
    }
    catch (const std::exception& e) {
        JAMI_ERROR("Error starting PipeWire stream: {:s}", e.what());
        status_ = Status::Idle;
    }

    pw_thread_loop_unlock(loop_);
}

void PipeWireLayer::createStream(std::unique_ptr<PipeWireStream>& stream,
                                 AudioDeviceType type,
                                 const PwDeviceInfo& dev_info,
                                 std::function<void(pw_buffer* buf)>&& onData)
{
    if (stream) {
        JAMI_WARNING("Stream already exists");
        return;
    }

    const char* name = type == AudioDeviceType::PLAYBACK
                           ? "Playback"
                           : (type == AudioDeviceType::CAPTURE
                                  ? "Record"
                                  : (type == AudioDeviceType::RINGTONE ? "Ringtone" : "?"));

    pendingStreams++;
    stream = std::make_unique<PipeWireStream>(loop_, core_,
                                              name,
                                              type,
                                              dev_info,
                                              std::move(onData),
                                              [this,type](const AudioFormat& format) {
                                                hardwareFormatAvailable(format);
                                                if (type == AudioDeviceType::PLAYBACK) {
                                                    playbackChanged(true);
                                                } else if (type == AudioDeviceType::CAPTURE) {
                                                    recordChanged(true);
                                                }
                                              },
                                              [this]() { onStreamReady(); });
}

void PipeWireLayer::onStreamReady()
{
    if (--pendingStreams == 0) {
        JAMI_DEBUG("All streams ready, starting audio");
        flushUrgent();
        flushMain();
        if (status_ == Status::Started) {
            if (playback_) {
                playback_->start();
            }
            if (ringtone_) {
                ringtone_->start();
            }
            if (record_) {
                record_->start();
            }
        }
    }
}

void
PipeWireLayer::startCaptureStream(const std::string& id)
{
    // Todo: implement audio sharing of a specific application window
    // Currently, Wayland does not allow this
    // So we only capture 'desktop' audio on both X11 and Wayland for a similar experience 
    // See https://github.com/flatpak/xdg-desktop-portal/issues/957

    if (loopbackCapture_.isRunning()) {
        JAMI_WARNING("Loopback capture is already running");
        return;
    }

    JAMI_LOG("Starting Loopback Capture for ID {:s}", id);

    // Map to store per-stream ring buffers
    // This needs to be a shared_ptr so it can be captured by the lambda
    auto streamRingBuffers = std::make_shared<std::map<uint32_t, std::shared_ptr<jami::RingBuffer>>>();

    loopbackCapture_.startCaptureAsync(
        PACKAGE_NAME,
        [id, streamRingBuffers](const AudioData& audioData) {
            if (audioData.size == 0 || !audioData.data) {
                JAMI_WARNING("No audio data captured.");
                return;
            }

            auto& rbPool = Manager::instance().getRingBufferPool();

            std::shared_ptr<jami::RingBuffer> streamRingBuffer;
            auto it = streamRingBuffers->find(audioData.node_id);
            
            if (it == streamRingBuffers->end()) {
                std::string streamId = id + "_stream_" + std::to_string(audioData.node_id);
                streamRingBuffer = rbPool.createRingBuffer(streamId);
                
                if (!streamRingBuffer) {
                    JAMI_ERROR("Failed to create ring buffer for stream {:d}", audioData.node_id);
                    return;
                }
                
                rbPool.bindHalfDuplexOut(id, streamId);
                
                (*streamRingBuffers)[audioData.node_id] = streamRingBuffer;

                JAMI_LOG("Created and bound ring buffer for stream {:d} ({}) to ring buffer {}", 
                         audioData.node_id, audioData.app_name, id);
            } else {
                streamRingBuffer = it->second;
            }

            // Create audio frame and put it in the stream's ring buffer
            auto capturedFrame = std::make_shared<AudioFrame>(
                AudioFormat{
                    audioData.rate,
                    audioData.channels,
                    audioData.size / audioData.channels / sizeof(int16_t) == 0 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16
                },
                audioData.size / (audioData.channels * sizeof(int16_t))
            );
            std::memcpy(capturedFrame->pointer()->data[0], audioData.data, audioData.size);
            streamRingBuffer->put(std::move(capturedFrame));
        }
    );
}

void
PipeWireLayer::stopCaptureStream(const std::string& id)
{
    if (!loopbackCapture_.isRunning()) {
        JAMI_WARNING("Loopback capture is not running");
        return;
    }

    JAMI_LOG("Stopping Loopback Capture for ID {:s}", id);

    loopbackCapture_.stopCapture();

    auto& rbPool = Manager::instance().getRingBufferPool();
    rbPool.unBindAll(id);
}

void PipeWireLayer::stopStream(AudioDeviceType type)
{
    pw_thread_loop_lock(loop_);

    try {
        if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL) {
            if (playback_) {
                playback_->stop();
                playback_.reset();
                playbackChanged(false);
            }
        }

        if (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL) {
            if (ringtone_) {
                ringtone_->stop();
                ringtone_.reset();
            }
        }

        if (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL) {
            if (record_) {
                record_->stop();
                record_.reset();
                recordChanged(false);
            }
        }

        // If all streams are stopped, update the status
        if (!playback_ && !ringtone_ && !record_) {
            pendingStreams = 0;
            status_ = Status::Idle;
            startedCv_.notify_all();
        }
    }
    catch (const std::exception& e) {
        JAMI_ERROR("Error stopping PipeWire stream: {:s}", e.what());
        status_ = Status::Idle;
    }

    pw_thread_loop_unlock(loop_);
}

void PipeWireLayer::updateDeviceList()
{
    JAMI_DEBUG("Updating PipeWire device list");
    
    // wait for device enumeration to complete
    std::unique_lock<std::mutex> lock(deviceListMutex_);
    if (!deviceListCv_.wait_for(lock, std::chrono::seconds(2), [this] {
        return !sinkList_.empty() || !sourceList_.empty();
    })) {
        JAMI_WARNING("Timeout waiting for PipeWire device enumeration");
    } else {
        JAMI_LOG("Device enumeration complete: {} sinks, {} sources", 
                 sinkList_.size(), sourceList_.size());
    }
}

void PipeWireLayer::registryEventCallback(void* data, 
                                          uint32_t id,
                                          uint32_t /*permissions*/,
                                          const char* type,
                                          uint32_t /*version*/,
                                          const struct spa_dict* props)
{

    PipeWireLayer* self = static_cast<PipeWireLayer*>(data);
    if (!type) {
        JAMI_WARNING("Received null type");
        return;
    }
    std::string_view stype(type);
    JAMI_LOG("registryEventCallback {} id {}", stype, id);
    
    if (stype == PW_TYPE_INTERFACE_Node) {
        if (const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) {
            std::string_view mediaClass(media_class);
            if (mediaClass == "Audio/Sink") {
                // This is an audio sink (playback device)
                self->handleSinkEvent(id, props);
            } else if (mediaClass == "Audio/Source") {
                // This is an audio source (capture device)
                self->handleSourceEvent(id, props);
            } else {
                JAMI_LOG("Unknown media class: {}", media_class);
            }
        }
    } else if (stype == PW_TYPE_INTERFACE_Metadata) {
        JAMI_LOG("Received metadata event");

        // filter metadata.name = default
        const char* name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
        if (name && std::string_view(name) == "default") {
            JAMI_LOG("Received default metadata");
            pw_metadata *metadata = (pw_metadata *)pw_registry_bind(self->registry_, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_CLIENT, 0);
            pw_metadata_add_listener(metadata, &self->metadata_listener, &metadata_events, nullptr);
        }
    }
}

void
PipeWireLayer::registryEventCallbackRemove(void* data, uint32_t id)
{
    PipeWireLayer* self = static_cast<PipeWireLayer*>(data);
    JAMI_LOG("registryEventCallbackRemove id {}", id);

    bool deviceRemoved = false;
    {
        std::lock_guard<std::mutex> lock(self->deviceListMutex_);
        // Remove the device from the list
        auto it = std::find_if(self->sinkList_.begin(), self->sinkList_.end(),
                               [id](const PwDeviceInfo& info) { return info.id == id; });
        if (it != self->sinkList_.end()) {
            self->sinkList_.erase(it);
            deviceRemoved = true;
        }

        it = std::find_if(self->sourceList_.begin(), self->sourceList_.end(),
                          [id](const PwDeviceInfo& info) { return info.id == id; });
        if (it != self->sourceList_.end()) {
            self->sourceList_.erase(it);
            deviceRemoved = true;
        }
    }

    if (deviceRemoved) {
        self->deviceListCv_.notify_all();
        self->devicesChanged();
    }
}

void PipeWireLayer::handleSinkEvent(uint32_t id, const struct spa_dict* props)
{
    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (!name || !description) {
        JAMI_WARNING("Incomplete sink information received");
        return;
    }
    const char* channels = spa_dict_lookup(props, PW_KEY_AUDIO_CHANNELS);
    const char* rates = spa_dict_lookup(props, PW_KEY_AUDIO_RATE);

    JAMI_LOG("Added new sink: {} ({})\n{}\n{}", name, description, channels ? channels : "(null)", rates ? rates : "(null)");

    {
        std::lock_guard<std::mutex> lock(deviceListMutex_);
        auto it = std::find_if(sinkList_.begin(), sinkList_.end(),
                               [id](const PwDeviceInfo& info) { return info.id == id; });

        if (it != sinkList_.end()) {
            // Update existing sink
            it->name = name;
            it->description = description;
            JAMI_LOG("Updated sink: {} ({})", name, description);
        } else {
            // Add new sink
            PwDeviceInfo newSink;
            newSink.id = id;
            newSink.name = name;
            newSink.description = description;
            if (channels) {
                newSink.channels = std::stoi(channels);
            }
            if (rates) {
                newSink.rate = std::stoi(rates);
            }
            
            sinkList_.push_back(newSink);
            JAMI_LOG("Added new sink: {} ({})", name, description);
        }
    }
    
    deviceListCv_.notify_all();
    devicesChanged();
}

void PipeWireLayer::handleSourceEvent(uint32_t id, const struct spa_dict* props)
{
    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    
    if (!name || !description) {
        JAMI_WARNING("Incomplete source information received");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(deviceListMutex_);
        auto it = std::find_if(sourceList_.begin(), sourceList_.end(),
                               [id](const PwDeviceInfo& info) { return info.id == id; });

        if (it != sourceList_.end()) {
            // Update existing source
            it->name = name;
            it->description = description;
            JAMI_DEBUG("Updated source: {:s} ({:s})", name, description);
        } else {
            // Add new source
            PwDeviceInfo newSource;
            newSource.id = id;
            newSource.name = name;
            newSource.description = description;
            // might want to query more properties here, such as channels and sample rate
            
            sourceList_.emplace_back(std::move(newSource));
            JAMI_DEBUG("Added new source: {:s} ({:s})", name, description);
        }
    }
    
    deviceListCv_.notify_all();
    devicesChanged();
}


void
PipeWireLayer::writeToSpeaker(pw_buffer* buf)
{
    spa_buffer* spa_buf = buf->buffer;
    spa_data* first_buf = &spa_buf->datas[0];
    if (first_buf->type != SPA_DATA_MemPtr) {
        JAMI_WARNING("Received non memory pointer");
        return;
    }

    uint8_t* data = (uint8_t*)first_buf->data;
    if (!data) {
        JAMI_WARNING("Received null data pointer");
        return;
    }
    auto format = playback_->format();
    auto frameSize = format.getBytesPerFrame();

    size_t writableBytes = first_buf->maxsize;
    size_t requestedSamples = buf->requested;
    if (requestedSamples == 0) {
        requestedSamples = writableBytes / frameSize;
    }

    if (requestedSamples != 0) {
        const auto& buff = getToPlay(format, requestedSamples);
        if (not buff or isPlaybackMuted_)
            memset(data, 0, writableBytes);
        else
            std::memcpy(data,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * frameSize);
        first_buf->chunk->size = requestedSamples * frameSize;
        first_buf->chunk->stride = frameSize;
        first_buf->chunk->offset = 0;
    }
}

void
PipeWireLayer::readFromMic(pw_buffer* buf)
{
    spa_buffer* spa_buf = buf->buffer;
    spa_data* first_buf = &spa_buf->datas[0];
    if (first_buf->type != SPA_DATA_MemPtr) {
        JAMI_WARNING("Received non memory pointer");
        return;
    }

    if (first_buf->chunk->size == 0) {
        JAMI_WARNING("Received empty buffer");
        return;
    }

    uint8_t* data = (uint8_t*)first_buf->data;
    if (!data) {
        JAMI_WARNING("Received null data pointer");
        return;
    }

    size_t readableBytes = first_buf->chunk->size;
    size_t offset = first_buf->chunk->offset;
    
    auto format = record_->format();
    auto frameSize = format.getBytesPerFrame();
    auto samples = readableBytes / frameSize;

    auto out = std::make_shared<AudioFrame>(format, samples);
    if (isCaptureMuted_)
        libav_utils::fillWithSilence(out->pointer());
    else
        std::memcpy(out->pointer()->data[0], data + offset, readableBytes);
    putRecorded(std::move(out));
}

void
PipeWireLayer::ringtoneToSpeaker(pw_buffer* buf)
{
    spa_buffer* spa_buf = buf->buffer;
    spa_data* first_buf = &spa_buf->datas[0];

    uint8_t* data = (uint8_t*)first_buf->data;
    if (!data) {
        JAMI_WARNING("Received null data pointer");
        return;
    }
    auto format = ringtone_->format();
    auto frameSize = format.getBytesPerFrame();

    size_t writableBytes = first_buf->maxsize;
    size_t requestedSamples = buf->requested;
    JAMI_LOG("writeToSpeaker: writableBytes = {}, requestedSamples = {}", writableBytes, requestedSamples);
    if (requestedSamples == 0) {
        requestedSamples = writableBytes / frameSize;
    }

    if (requestedSamples != 0) {
        const auto& buff = getToRing(format, requestedSamples);
        if (not buff or isPlaybackMuted_)
            memset(data, 0, writableBytes);
        else
            std::memcpy(data,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * frameSize);
        first_buf->chunk->size = requestedSamples * frameSize;
        first_buf->chunk->stride = frameSize;
        first_buf->chunk->offset = 0;
    }
}


void
PipeWireLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
    case AudioDeviceType::PLAYBACK:
        JAMI_DEBUG("setting {:s} for playback", devName);
        preference.setPulseDevicePlayback(devName);
        break;

    case AudioDeviceType::CAPTURE:
        JAMI_DEBUG("setting {:s} for capture", devName);
        preference.setPulseDeviceRecord(devName);
        break;

    case AudioDeviceType::RINGTONE:
        JAMI_DEBUG("setting {:s} for ringer", devName);
        preference.setPulseDeviceRingtone(devName);
        break;

    default:
        break;
    }
}


std::string
PipeWireLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    // Index 0 is always "default" (empty string means use system default)
    if (index == 0) {
        return "";
    }
    
    // Adjust index to account for "default" at position 0
    int adjustedIndex = index - 1;
    
    std::lock_guard<std::mutex> lock(deviceListMutex_);
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        if (adjustedIndex < 0 or static_cast<size_t>(adjustedIndex) >= sinkList_.size()) {
            JAMI_ERROR("Index {:d} out of range", index);
            return "";
        }
        return sinkList_[adjustedIndex].name;

    case AudioDeviceType::CAPTURE:
        if (adjustedIndex < 0 or static_cast<size_t>(adjustedIndex) >= sourceList_.size()) {
            JAMI_ERROR("Index {:d} out of range", index);
            return "";
        }
        return sourceList_[adjustedIndex].name;

    default:
        // Should never happen
        JAMI_ERROR("Unexpected type");
        return "";
    }
}


/**
 * Unary function to search for a device by name in a list using std functions.
 */
class NameComparator
{
public:
    explicit NameComparator(const std::string& ref)
        : baseline(ref)
    {}
    bool operator()(const PwDeviceInfo& arg) { return arg.name == baseline; }

private:
    const std::string& baseline;
};

class DescriptionComparator
{
public:
    explicit DescriptionComparator(const std::string& ref)
        : baseline(ref)
    {}
    bool operator()(const PwDeviceInfo& arg) { return arg.description == baseline; }

private:
    const std::string& baseline;
};

int
PipeWireLayer::getAudioDeviceIndex(const std::string& descr, AudioDeviceType type) const
{
    // "default" is always at index 0
    if (descr == "default") {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(deviceListMutex_);
    int foundIndex = -1;
    
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        foundIndex = std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          DescriptionComparator(descr)));
        // Add 1 to account for "default" at index 0
        return (foundIndex < static_cast<int>(sinkList_.size())) ? foundIndex + 1 : 0;
        
    case AudioDeviceType::CAPTURE:
        foundIndex = std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          DescriptionComparator(descr)));
        // Add 1 to account for "default" at index 0
        return (foundIndex < static_cast<int>(sourceList_.size())) ? foundIndex + 1 : 0;
        
    default:
        JAMI_ERROR("Unexpected device type");
        return 0;
    }
}

int
PipeWireLayer::getAudioDeviceIndexByName(const std::string& name, AudioDeviceType type) const
{
    // Empty name or "default" means index 0
    if (name.empty() || name == "default") {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(deviceListMutex_);
    int foundIndex = -1;
    
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        foundIndex = std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          NameComparator(name)));
        // Add 1 to account for "default" at index 0
        return (foundIndex < static_cast<int>(sinkList_.size())) ? foundIndex + 1 : 0;
        
    case AudioDeviceType::CAPTURE:
        foundIndex = std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          NameComparator(name)));
        // Add 1 to account for "default" at index 0
        return (foundIndex < static_cast<int>(sourceList_.size())) ? foundIndex + 1 : 0;
        
    default:
        JAMI_ERROR("Unexpected device type");
        return 0;
    }
}

int
PipeWireLayer::getIndexCapture() const
{
    return getAudioDeviceIndexByName(pref_.getPulseDeviceRecord(), AudioDeviceType::CAPTURE);
}

int
PipeWireLayer::getIndexPlayback() const
{
    return getAudioDeviceIndexByName(pref_.getPulseDevicePlayback(),
                                     AudioDeviceType::PLAYBACK);
}

int
PipeWireLayer::getIndexRingtone() const
{
    return getAudioDeviceIndexByName(pref_.getPulseDeviceRingtone(),
                                     AudioDeviceType::RINGTONE);
}


std::vector<std::string>
PipeWireLayer::getCaptureDeviceList() const
{
    std::lock_guard<std::mutex> lock(deviceListMutex_);
    std::vector<std::string> names;
    names.reserve(sourceList_.size() + 1);
    
    // Add "default" as the first option (index 0), matching PulseAudio behavior
    names.emplace_back("default");
    
    for (const auto& s : sourceList_)
        names.emplace_back(s.description);
    return names;
}

std::vector<std::string>
PipeWireLayer::getPlaybackDeviceList() const
{
    std::lock_guard<std::mutex> lock(deviceListMutex_);
    std::vector<std::string> names;
    names.reserve(sinkList_.size() + 1);
    
    // Add "default" as the first option (index 0), matching PulseAudio behavior
    names.emplace_back("default");
    
    for (const auto& s : sinkList_)
        names.emplace_back(s.description);
    return names;
}

} // namespace jami