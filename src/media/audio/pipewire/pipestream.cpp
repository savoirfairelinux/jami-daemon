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
#include "pipestream.h"

namespace jami {


PipeWireStream::PipeWireStream(pw_core* core, const char* name, AudioDeviceType type, const PwDeviceInfo& dev_info, std::function<void()>&& onData, std::function<void()>&& onReady)
    : type_(type)
    , onData_(std::move(onData))
    , onReady_(std::move(onReady))
    , ready_(false)
{
    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                             PW_KEY_MEDIA_CATEGORY, type == AudioDeviceType::CAPTURE ? "Capture" : "Playback",
                                             PW_KEY_APP_NAME, name,
                                             nullptr);

    stream_ = pw_stream_new(core, name, props);

    pw_stream_events_init(&stream_events_);
    stream_events_.state_changed = &PipeWireStream::stateChangedCallback;
    stream_events_.process = &PipeWireStream::processCallback;
    pw_stream_add_listener(stream_, &streamListener_, &stream_events_, this);

    // Configure the stream based on dev_info
    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = dev_info.channels,
            .rate = dev_info.rate
        ));

    pw_stream_connect(stream_,
                      type == AudioDeviceType::CAPTURE ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
                      dev_info.id,
                      dev_info.flags | PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);
}

PipeWireStream::~PipeWireStream()
{
    if (stream_) {
        pw_stream_destroy(stream_);
    }
}

void PipeWireStream::start() 
{
    pw_stream_set_active(stream_, true);
}

void PipeWireStream::stop() 
{
    pw_stream_set_active(stream_, false);
}

bool PipeWireStream::isReady() const
{
    return ready_;
}

void PipeWireStream::stateChangedCallback(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
    PipeWireStream* self = static_cast<PipeWireStream*>(data);
    JAMI_DBG("Stream state changed from %s to %s", pw_stream_state_as_string(old), pw_stream_state_as_string(state));

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        JAMI_ERR("Stream error: %s", error ? error : "unknown");
        break;
    case PW_STREAM_STATE_PAUSED:
        self->ready_ = true;
        if (self->onReady_) {
            self->onReady_();
        }
        break;
    case PW_STREAM_STATE_STREAMING:
        JAMI_DBG("Stream is now streaming");
        break;
    default:
        break;
    }
}

void PipeWireStream::processCallback(void* data)
{
    PipeWireStream* self = static_cast<PipeWireStream*>(data);
    
    pw_buffer* buf = pw_stream_dequeue_buffer(self->stream_);
    if (buf == nullptr) {
        JAMI_WARN("Out of buffers");
        return;
    }

    if (self->onData_) {
        self->onData_(buf);
    }

    pw_stream_queue_buffer(self->stream_, buf);
}
} // namespace jami
