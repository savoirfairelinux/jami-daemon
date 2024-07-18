#include "pipelayer.h"

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
                             std::bind(&PipeWireLayer::writeToSpeaker, this));
            }
        }
        
        if (type == AudioDeviceType::RINGTONE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sinkList_, getPreferredRingtoneDevice())) {
                createStream(ringtone_,
                             AudioDeviceType::RINGTONE,
                             *dev_info,
                             std::bind(&PipeWireLayer::ringtoneToSpeaker, this));
            }
        }
        
        if (type == AudioDeviceType::CAPTURE || type == AudioDeviceType::ALL) {
            if (auto dev_info = getDeviceInfo(sourceList_, getPreferredCaptureDevice())) {
                createStream(record_,
                             AudioDeviceType::CAPTURE,
                             *dev_info,
                             std::bind(&PipeWireLayer::readFromMic, this));
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

} // namespace jami