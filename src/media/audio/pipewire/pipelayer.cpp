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

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <pipewire/extensions/metadata.h>
#include <spa/utils/dict.h>
#include <spa/utils/result.h>

namespace jami {


static int metadata_property(void *data, uint32_t id, const char *key, const char *type, const char *value)
{
    if (key && value) {
        printf("Metadata: %d %s %s = %s\n", id, key, type, value);
    }
    return 0;
}

static const struct pw_metadata_events metadata_events = {
    PW_VERSION_METADATA_EVENTS,
    .property = metadata_property,
};

const PwDeviceInfo*
getDeviceInfo(const std::vector<PwDeviceInfo>& list, const std::string& name)
{
    if (list.empty()) {
        JAMI_WARN("getDeviceInfo: device list is empty");
        return nullptr;
    }
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
        .global_remove = nullptr,
    };

    JAMI_LOG("PipeWireLayer pw_registry_add_listener");
    pw_registry_add_listener(registry_, &registry_listener, &registry_events, this);
    pw_thread_loop_start(loop_);
    //updateDeviceList();
}

PipeWireLayer::~PipeWireLayer()
{
    pw_proxy_destroy((pw_proxy*)registry_);
    stopStream(AudioDeviceType::ALL);
    pw_core_disconnect(core_);
    pw_context_destroy(context_);
    pw_thread_loop_stop(loop_);
    pw_thread_loop_destroy(loop_);
    pw_deinit();
}

void PipeWireLayer::startStream(AudioDeviceType type)
{
    pw_thread_loop_lock(loop_);

    try {
        // Create Streams
        if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sinkList_, pref_.getPulseDevicePlayback())) {
                createStream(playback_,
                             AudioDeviceType::PLAYBACK,
                             *dev_info,
                             std::bind(&PipeWireLayer::writeToSpeaker, this, std::placeholders::_1));
            }
        }
        
        if (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sinkList_, pref_.getPulseDeviceRingtone())) {
                createStream(ringtone_,
                             AudioDeviceType::RINGTONE,
                             *dev_info,
                             std::bind(&PipeWireLayer::ringtoneToSpeaker, this, std::placeholders::_1));
            }
        }
        
        if (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sourceList_, pref_.getPulseDeviceRecord())) {
                createStream(record_,
                             AudioDeviceType::CAPTURE,
                             *dev_info,
                             std::bind(&PipeWireLayer::readFromMic, this, std::placeholders::_1));
            }
        }

        // Start the streams
        /*if (playback_ && (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL)) {
            playback_->start();
            playbackChanged(true);
        }
        
        if (ringtone_ && (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL)) {
            ringtone_->start();
        }
        
        if (record_ && (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL)) {
            record_->start();
        }*/

        status_ = Status::Started;
        startedCv_.notify_all();
    }
    catch (const std::exception& e) {
        JAMI_ERR("Error starting PipeWire stream: %s", e.what());
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
        JAMI_WARN("Stream already exists");
        return;
    }

    const char* name = type == AudioDeviceType::PLAYBACK
                           ? "Playback"
                           : (type == AudioDeviceType::CAPTURE
                                  ? "Record"
                                  : (type == AudioDeviceType::RINGTONE ? "Ringtone" : "?"));

    pendingStreams++;
    stream = std::make_unique<PipeWireStream>(core_,
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
        JAMI_DBG("All streams ready, starting audio");
        flushUrgent();
        flushMain();
        if (status_ == Status::Started) {
            if (playback_) {
                //hardwareFormatAvailable(playback_->format());
                playback_->start();
                //playbackChanged(true);
            }
            if (ringtone_) {
                //hardwareFormatAvailable(ringtone_->format());
                ringtone_->start();
            }
            if (record_) {
                //hardwareFormatAvailable(record_->format());
                record_->start();
                //recordChanged(true);
            }
        }
    }
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
        status_ = Status::Idle;
    }

    pw_thread_loop_unlock(loop_);
}

void PipeWireLayer::updateDeviceList() 
{
}

void PipeWireLayer::registryEventCallback(void* data, 
                                          uint32_t id,
                                          uint32_t permissions,
                                          const char* type,
                                          uint32_t version,
                                          const struct spa_dict* props)
{
    //JAMI_LOG("PipeWireLayer registryEventCallback");
    PipeWireLayer* self = static_cast<PipeWireLayer*>(data);
    if (!type) {
        JAMI_WARN("Received null type");
        return;
    }
    std::string_view stype(type);
    
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
        /*
        // enumerate everything in props
        if (props) {
            for (int i = 0; i < props->n_items; i++) {
                printf("Metadata: %s = %s\n", props->items[i].key, props->items[i].value);
            }
        }*/

        // filter metadata.name = default
        const char* name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
        if (name && std::string_view(name) == "default") {
            JAMI_LOG("Received default metadata");
            pw_metadata *metadata = (pw_metadata *)pw_registry_bind(self->registry_, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_CLIENT, 0);
            pw_metadata_add_listener(metadata, &self->metadata_listener, &metadata_events, nullptr);
        }
    }
}

void PipeWireLayer::handleSinkEvent(uint32_t id, const struct spa_dict* props)
{
    JAMI_LOG("PipeWireLayer handleSinkEvent {}", id);

    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (!name || !description) {
        JAMI_WARN("Incomplete sink information received");
        return;
    }
    const char* channels = spa_dict_lookup(props, PW_KEY_AUDIO_CHANNELS);
    const char* rates = spa_dict_lookup(props, PW_KEY_AUDIO_RATE);

    JAMI_LOG("Added new sink: {} ({})\n{}\n{}", name, description, channels ? channels : "(null)", rates ? rates : "(null)");

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
        // channels and sample rate
        if (channels) {
            newSink.channels = std::stoi(channels);
        }
        if (rates) {
            newSink.rate = std::stoi(rates);
        }
        
        sinkList_.push_back(newSink);
        JAMI_LOG("Added new sink: {} ({})", name, description);
    }

    // Notify that the device list has changed
    devicesChanged();
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

    uint8_t* data = (uint8_t*)spa_buf->datas[0].data;
    if (!data) {
        JAMI_WARN("Received null data pointer");
        return;
    }

    size_t writableBytes = spa_buf->datas[0].chunk->size;
    size_t offset = spa_buf->datas[0].chunk->offset;
    //int ret = pa_stream_begin_write(playback_->stream(), &data, &writableBytes);

    if (writableBytes != 0) {
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
    if (!buf) {
        JAMI_WARNING("Received null buffer");
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    if (!spa_buf) {
        JAMI_WARNING("Received null spa_buffer");
        return;
    }
    
    auto n_datas = spa_buf->n_datas;
    if (n_datas == 0) {
        JAMI_WARNING("Received empty buffer");
        return;
    } else if (n_datas > 1) {
        JAMI_WARNING("Received multiple datas");
    }

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
    size_t stride = first_buf->chunk->stride;
    
    auto format = record_->format();
    auto frameSize = format.getBytesPerFrame();
    auto samples = readableBytes / frameSize;
    //JAMI_WARNING("readFromMic {:p} {} {} {} with {}. Samples: {}", fmt::ptr(data), readableBytes, offset, stride, format.toString(), samples);

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
    /*if (!ringtone_ or !ringtone_->isReady())
        return;
*/
    /*void* data = nullptr;
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
    }*/

    if (!buf) {
        JAMI_WARN("Received null buffer");
        return;
    }

    spa_buffer* spa_buf = buf->buffer;
    if (!spa_buf) {
        JAMI_WARN("Received null spa_buffer");
        return;
    }

    uint8_t* data = (uint8_t*)spa_buf->datas[0].data;
    if (!data) {
        JAMI_WARN("Received null data pointer");
        return;
    }

    size_t writableBytes = spa_buf->datas[0].chunk->size;
    size_t offset = spa_buf->datas[0].chunk->offset;

    const auto& buff = getToRing(ringtone_->format(), writableBytes / ringtone_->frameSize());
    if (not buff or isRingtoneMuted_)
        memset(data + offset, 0, writableBytes);
    else
        std::memcpy(data + offset,
                    buff->pointer()->data[0],
                    buff->pointer()->nb_samples * ringtone_->frameSize());
}


void
PipeWireLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
    case AudioDeviceType::PLAYBACK:
        JAMI_DBG("setting %s for playback", devName.c_str());
        preference.setPulseDevicePlayback(devName);
        break;

    case AudioDeviceType::CAPTURE:
        JAMI_DBG("setting %s for capture", devName.c_str());
        preference.setPulseDeviceRecord(devName);
        break;

    case AudioDeviceType::RINGTONE:
        JAMI_DBG("setting %s for ringer", devName.c_str());
        preference.setPulseDeviceRingtone(devName);
        break;

    default:
        break;
    }
}


std::string
PipeWireLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
            JAMI_ERR("Index %d out of range", index);
            return "";
        }
        return sinkList_[index].name;

    case AudioDeviceType::CAPTURE:
        if (index < 0 or static_cast<size_t>(index) >= sourceList_.size()) {
            JAMI_ERR("Index %d out of range", index);
            return "";
        }
        return sourceList_[index].name;

    default:
        // Should never happen
        JAMI_ERR("Unexpected type");
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
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        return std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          DescriptionComparator(descr)));
    case AudioDeviceType::CAPTURE:
        return std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          DescriptionComparator(descr)));
    default:
        JAMI_ERR("Unexpected device type");
        return 0;
    }
}

int
PipeWireLayer::getAudioDeviceIndexByName(const std::string& name, AudioDeviceType type) const
{
    if (name.empty())
        return 0;
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        return std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          NameComparator(name)));
    case AudioDeviceType::CAPTURE:
        return std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          NameComparator(name)));
    default:
        JAMI_ERR("Unexpected device type");
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
    std::vector<std::string> names;
    names.reserve(sourceList_.size());
    for (const auto& s : sourceList_)
        names.emplace_back(s.description);
    return names;
}

std::vector<std::string>
PipeWireLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sinkList_.size());
    for (const auto& s : sinkList_)
        names.emplace_back(s.description);
    return names;
}

} // namespace jami