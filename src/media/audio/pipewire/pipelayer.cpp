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

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>

namespace jami {

struct PwDeviceInfo 
{
    uint32_t id;
    std::string name;
    std::string description;
    uint32_t channels;
    uint32_t rate;
    pw_stream_flags flags;
};

PipeWireLayer::PipeWireLayer(AudioPreference& pref)
    : AudioLayer(pref), preference_(pref)
{
    pw_init(nullptr, nullptr);
    loop_ = pw_thread_loop_new("PipeWire-loop", nullptr);
    context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
    core_ = pw_context_connect(context_, nullptr, 0);

    pw_registry* registry = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(registry, &registryListener_, &registryEvents_, this);

    updateDeviceList();
}
void PipeWireLayer::startStream(AudioDeviceType type)
{
    pw_thread_loop_lock(loop_);

    try {
        // Create Streams
        if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sinkList_, getPreferredPlaybackDevice())) {
                createStream(playback_,
                             AudioDeviceType::PLAYBACK,
                             *dev_info,
                             std::bind(&PipeWireLayer::writeToSpeaker, this, std::placeholders::_1));
            }
        }
        
        if (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sinkList_, getPreferredRingtoneDevice())) {
                createStream(ringtone_,
                             AudioDeviceType::RINGTONE,
                             *dev_info,
                             std::bind(&PipeWireLayer::ringtoneToSpeaker, this, std::placeholders::_1));
            }
        }
        
        if (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sourceList_, getPreferredCaptureDevice())) {
                createStream(record_,
                             AudioDeviceType::CAPTURE,
                             *dev_info,
                             std::bind(&PipeWireLayer::readFromMic, this, std::placeholders::_1));
            }
        }

        // Start the streams
        if (playback_ && (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL)) {
            playback_->start();
            playbackChanged(true);
        }
        
        if (ringtone_ && (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL)) {
            ringtone_->start();
        }
        
        if (record_ && (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL)) {
            record_->start();
            recordChanged(true);
        }

        status_ = Status::Started;
        startedCv_.notify_all();
    }
    catch (const std::exception& e) {
        JAMI_ERR("Error starting PipeWire stream: %s", e.what());
        status_ = Status::ErrorStart;
    }

    pw_thread_loop_unlock(loop_);
}

void PipeWireLayer::createStream(std::unique_ptr<PipeWireStream>& stream,
                                 AudioDeviceType type,
                                 const PwDeviceInfo& dev_info,
                                 std::function<void()>&& onData)
{
    if (stream) {
        JAMI_WARN("Stream already exists");
        return;
    }

    const char* name = type == AudioDeviceType::PLAYBACK
                           ? "Playback"
                           : (type == AudioDeviceType::CAPTURE
                                  ? "Record"
                                  : (type == AudioDeviceType::RINGTONE ? "Ringtone" : "?"));

    stream = std::make_unique<PipeWireStream>(core_,
                                              name,
                                              type,
                                              dev_info,
                                              std::move(onData),
                                              [this]() { onStreamReady(); });
}

void PipeWireLayer::onStreamReady()
{
    if (--pendingStreams == 0) {
        JAMI_DBG("All streams ready, starting audio");
        flushUrgent();
        flushMain();
        if (playback_) {
            playback_->start();
            playbackChanged(true);
        }
        if (ringtone_) {
            ringtone_->start();
        }
        if (record_) {
            record_->start();
            recordChanged(true);
        }
    }
}

const PwDeviceInfo*
PipeWireLayer::getDeviceInfo(const std::vector<PwDeviceInfo>& list, const std::string& name) const
{
    auto dev_info = std::find_if(list.begin(), list.end(), 
        [&name](const PwDeviceInfo& info) { return info.name == name; });
    
    if (dev_info == list.end()) {
        JAMI_WARN("Preferred device %s not found in device list, selecting default %s instead.",
                  name.c_str(),
                  list.front().name.c_str());
        return &list.front();
    }
    return &(*dev_info);
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
        JAMI_ERR("Error stopping PipeWire stream: %s", e.what());
        status_ = Status::ErrorStop;
    }

    pw_thread_loop_unlock(loop_);
}

void PipeWireLayer::updateDeviceList() 
{
    // Use PipeWire API to enumerate devices and update sinkList_ and sourceList_
}

void PipeWireLayer::registryEventCallback(void* data, 
                                          uint32_t id,
                                          uint32_t permissions,
                                          const char* type,
                                          uint32_t version,
                                          const struct spa_dict* props)
{
    PipeWireLayer* self = static_cast<PipeWireLayer*>(data);
    
    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        
        if (media_class) {
            if (strcmp(media_class, "Audio/Sink") == 0) {
                // This is an audio sink (playback device)
                self->handleSinkEvent(id, props);
            } else if (strcmp(media_class, "Audio/Source") == 0) {
                // This is an audio source (capture device)
                self->handleSourceEvent(id, props);
            }
        }
    }
}

void PipeWireLayer::handleSinkEvent(uint32_t id, const struct spa_dict* props)
{
    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    
    if (!name || !description) {
        JAMI_WARN("Incomplete sink information received");
        return;
    }

    auto it = std::find_if(sinkList_.begin(), sinkList_.end(),
                           [id](const PwDeviceInfo& info) { return info.id == id; });

    if (it != sinkList_.end()) {
        // Update existing sink
        it->name = name;
        it->description = description;
        JAMI_DBG("Updated sink: %s (%s)", name, description);
    } else {
        // Add new sink
        PwDeviceInfo newSink;
        newSink.id = id;
        newSink.name = name;
        newSink.description = description;
        // You might want to query more properties here, such as channels and sample rate
        
        sinkList_.push_back(newSink);
        JAMI_DBG("Added new sink: %s (%s)", name, description);
    }

    // Notify that the device list has changed
    devicesChanged();
}

void PipeWireLayer::queryDeviceProperties(PwDeviceInfo& deviceInfo, const struct spa_dict* props)
{
    // Query channels
    const char* channels = spa_dict_lookup(props, PW_KEY_AUDIO_CHANNELS);
    if (channels) {
        deviceInfo.channels.clear();
        std::istringstream iss(channels);
        uint32_t channel;
        while (iss >> channel) {
            deviceInfo.channels.push_back(channel);
        }
    }

    // Query sample rates
    const char* rates = spa_dict_lookup(props, PW_KEY_AUDIO_RATE);
    if (rates) {
        deviceInfo.sampleRates.clear();
        std::istringstream iss(rates);
        uint32_t rate;
        while (iss >> rate) {
            deviceInfo.sampleRates.push_back(rate);
        }
    }

    // Query flags
    const char* allowDriftingStr = spa_dict_lookup(props, PW_KEY_STREAM_ALLOW_DRIFTING);
    const char* adaptThresholdStr = spa_dict_lookup(props, PW_KEY_STREAM_ADAPT_THRESHOLD);

    deviceInfo.flags = PW_STREAM_FLAG_NONE;
    if (allowDriftingStr && strcmp(allowDriftingStr, "true") == 0) {
        deviceInfo.flags |= PW_STREAM_FLAG_DRIVER;
    }
    if (adaptThresholdStr) {
        deviceInfo.flags |= PW_STREAM_FLAG_ADAPTIVE;
    }
}

void PipeWireLayer::handleSourceEvent(uint32_t id, const struct spa_dict* props)
{
    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    
    if (!name || !description) {
        JAMI_WARN("Incomplete source information received");
        return;
    }

    auto it = std::find_if(sourceList_.begin(), sourceList_.end(),
                           [id](const PwDeviceInfo& info) { return info.id == id; });

    if (it != sourceList_.end()) {
        // Update existing source
        it->name = name;
        it->description = description;
        JAMI_DBG("Updated source: %s (%s)", name, description);
    } else {
        // Add new source
        PwDeviceInfo newSource;
        newSource.id = id;
        newSource.name = name;
        newSource.description = description;
        // You might want to query more properties here, such as channels and sample rate
        
        sourceList_.push_back(newSource);
        JAMI_DBG("Added new source: %s (%s)", name, description);
    }

    // Notify that the device list has changed
    devicesChanged();
}


void
PipeWireLayer::writeToSpeaker(pw_buffer* buf)
{
    /*if (!playback_ or !playback_->isReady())
        return;
*/
    // available bytes to be written in pulseaudio internal buffer
    if (!buf) {
        JAMI_WARN("Received null buffer");
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    if (!spa_buf) {
        JAMI_WARN("Received null spa_buffer");
        return;
    }

    void* data = spa_buf->datas[0].data;
    if (!data) {
        JAMI_WARN("Received null data pointer");
        return;
    }

    size_t writableBytes = spa_buf->datas[0].chunk->size;
    size_t offset = spa_buf->datas[0].chunk->offset;
    //int ret = pa_stream_begin_write(playback_->stream(), &data, &writableBytes);

    if (ret == 0 and data and size != 0) {
        //writableBytes = std::min(pa_stream_writable_size(playback_->stream()), writableBytes);
        const auto& buff = getToPlay(playback_->format(), writableBytes / playback_->frameSize());
        if (not buff or isPlaybackMuted_)
            memset(data + offset, 0, writableBytes);
        else
            std::memcpy(data + offset,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * playback_->frameSize());
        //pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

void
PipeWireLayer::readFromMic(pw_buffer* buf)
{
    /*if (!record_ or !record_->isReady())
        return;

    const char* data = nullptr;
    size_t bytes;
    if (pa_stream_peek(record_->stream(), (const void**) &data, &bytes) < 0 or !data)
        return;

    if (bytes == 0)
        return;

    size_t sample_size = record_->frameSize();
    const size_t samples = bytes / sample_size;

    auto out = std::make_shared<AudioFrame>(record_->format(), samples);
    if (isCaptureMuted_)
        libav_utils::fillWithSilence(out->pointer());
    else
        std::memcpy(out->pointer()->data[0], data, bytes);

    if (pa_stream_drop(record_->stream()) < 0)
        JAMI_ERR("Capture stream drop failed: %s", pa_strerror(pa_context_errno(context_)));

    putRecorded(std::move(out));*/

    if (!buf) {
        JAMI_WARN("Received null buffer");
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    if (!spa_buf) {
        JAMI_WARN("Received null spa_buffer");
        return;
    }

    void* data = spa_buf->datas[0].data;
    if (!data) {
        JAMI_WARN("Received null data pointer");
        return;
    }

    size_t readableBytes = spa_buf->datas[0].chunk->size;
    size_t offset = spa_buf->datas[0].chunk->offset;

    auto out = std::make_shared<AudioFrame>(record_->format(), samples);
    if (isCaptureMuted_)
        libav_utils::fillWithSilence(out->pointer());
    else
        std::memcpy(out->pointer(), data + offset, readableBytes);
    putRecorded(std::move(out));
}

void
PipeWireLayer::ringtoneToSpeaker(pw_buffer* buf)
{
    /*if (!ringtone_ or !ringtone_->isReady())
        return;
*/
    void* data = nullptr;
    size_t writableBytes = (size_t) -1;
    int ret = pa_stream_begin_write(ringtone_->stream(), &data, &writableBytes);
    if (ret == 0 and data and writableBytes != 0) {
        writableBytes = std::min(pa_stream_writable_size(ringtone_->stream()), writableBytes);
        const auto& buff = getToRing(ringtone_->format(), writableBytes / ringtone_->frameSize());
        if (not buff or isRingtoneMuted_)
            memset(data, 0, writableBytes);
        else
            std::memcpy(data,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * ringtone_->frameSize());
        pa_stream_write(ringtone_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

} // namespace jami