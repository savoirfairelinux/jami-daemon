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
#include "pipestream.h"


#include <spa/pod/pod.h>
#include <spa/pod/builder.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/format.h>
#include <spa/param/latency.h>
#include <spa/param/latency-utils.h>
#include <spa/param/tag-utils.h>

#include <spa/param/props-types.h>

#include <pipewire/stream.h>

namespace jami {


PipeWireStream::PipeWireStream(pw_thread_loop* loop, pw_core* core, const char* name, AudioDeviceType type, const PwDeviceInfo& dev_info, std::function<void(pw_buffer* buf)>&& onData, std::function<void(const AudioFormat&)>&& onFormatChange, std::function<void()>&& onReady)
    : loop_(loop)
    , type_(type)
    , onData_(std::move(onData))
    , onFormatChange_(std::move(onFormatChange))
    , onReady_(std::move(onReady))
    , format_(dev_info.rate, dev_info.channels)
{
    JAMI_WARNING("PipeWireStream init '{}'", name);
    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                             PW_KEY_MEDIA_CATEGORY, type == AudioDeviceType::CAPTURE ? "Capture" : "Playback",
                                             PW_KEY_APP_NAME, name,
                                             nullptr);

    stream_ = pw_stream_new(core, name, props);

    //stream_events_
    stream_events_ = pw_stream_events {
        PW_VERSION_STREAM_EVENTS,
        .destroy = nullptr,
        .state_changed = &PipeWireStream::streamStateChanged,
        .param_changed = &PipeWireStream::streamParamChanged,
        .add_buffer = nullptr,
        .remove_buffer = nullptr,
        .process = &PipeWireStream::streamProcess,
        .drained = nullptr,
    };
    pw_stream_add_listener(stream_, &streamListener_, &stream_events_, this);

    // Configure the stream based on dev_info
    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* params[1];

    struct spa_audio_info_raw info = SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_F32,
        /*.rate = dev_info.rate,
        .channels = dev_info.channels,*/
    );

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    pw_stream_connect(stream_,
                      type == AudioDeviceType::CAPTURE ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
                      dev_info.id,
                      (pw_stream_flags) (PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE | PW_STREAM_FLAG_MAP_BUFFERS),
                      params, 1);
}

PipeWireStream::~PipeWireStream()
{
    if (stream_) {
        pw_thread_loop_lock (loop_);
        pw_stream_disconnect(stream_);
        pw_stream_destroy(stream_);
        pw_thread_loop_unlock (loop_);
    }
}

void PipeWireStream::start()
{
    JAMI_LOG("Stream start()  pw_stream_set_active");
    pw_thread_loop_lock (loop_);
    pw_stream_set_active(stream_, true);
    pw_thread_loop_unlock (loop_);
}

void PipeWireStream::stop()
{
    JAMI_LOG("Stream stop()  pw_stream_set_active");
    pw_thread_loop_lock (loop_);
    pw_stream_set_active(stream_, false);
    pw_thread_loop_unlock (loop_);
}

bool PipeWireStream::isReady() const
{
    return ready_;
}

void PipeWireStream::streamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
    PipeWireStream* self = static_cast<PipeWireStream*>(data);
    JAMI_LOG("Stream state changed from {} to {}", pw_stream_state_as_string(old), pw_stream_state_as_string(state));

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        JAMI_ERROR("Stream error: {}", error ? error : "unknown");
        break;
    case PW_STREAM_STATE_PAUSED:
        self->ready_ = true;
        if (self->onReady_) {
            self->onReady_();
        }
        break;
    case PW_STREAM_STATE_STREAMING:
        JAMI_LOG("Stream is now streaming");
        break;
    default:
        break;
    }
}

inline AVSampleFormat
sampleFormatFromPipe(spa_audio_format format) {
    switch (format)
    {
    case SPA_AUDIO_FORMAT_S16:
        return AV_SAMPLE_FMT_S16;
    case SPA_AUDIO_FORMAT_S32:
        return AV_SAMPLE_FMT_S32;
    case SPA_AUDIO_FORMAT_F32:
        return AV_SAMPLE_FMT_FLT;
    case SPA_AUDIO_FORMAT_S16P:
        return AV_SAMPLE_FMT_S16P;
    case SPA_AUDIO_FORMAT_S32P:
        return AV_SAMPLE_FMT_S32P;
    case SPA_AUDIO_FORMAT_F32P:
        return AV_SAMPLE_FMT_FLTP;
    default:
        return AV_SAMPLE_FMT_S16;
    }
}

void printPodObject(const spa_pod_object* param);
void printPodValue(const spa_pod* value);

void printPodChoice(const spa_pod* value)
{
    uint32_t n_values, choice;
    const spa_pod* body = spa_pod_get_values(value, &n_values, &choice);
    switch (choice) {
    case SPA_CHOICE_None:
        JAMI_LOG("PipeWireStream: prop choice: none");
        break;
    case SPA_CHOICE_Range:
        JAMI_LOG("PipeWireStream: prop choice: range");
        if (n_values < 3)
                break;
        printPodValue(&body[0]);
        printPodValue(&body[1]);
        printPodValue(&body[2]);
        break;
    case SPA_CHOICE_Step:
        JAMI_LOG("PipeWireStream: prop choice: step");
        if (n_values < 4)
                break;
        printPodValue(&body[0]);
        printPodValue(&body[1]);
        printPodValue(&body[2]);
        printPodValue(&body[3]);
        break;
    case SPA_CHOICE_Enum:
        JAMI_LOG("PipeWireStream: prop choice: enum");
        break;
    case SPA_CHOICE_Flags:
        JAMI_LOG("PipeWireStream: prop choice: flags");
        break;
    default:
        JAMI_LOG("PipeWireStream: prop choice: unknown");
        break;
    }
}

void printPodStruct(const spa_pod* value)
{
    struct spa_pod *iter;
    SPA_POD_FOREACH((spa_pod*)SPA_POD_BODY(value), SPA_POD_BODY_SIZE(value), iter) {
        printPodValue(iter);
    }
}


void printPodValue(const spa_pod* value)
{
    uint32_t n_values, choice;
    int val;
    bool b;
    long valuel;
    float f;
    double d;
    const char *str;
    struct spa_pod *iter;

    switch (value->type) {
    case SPA_TYPE_Bool:
        spa_pod_get_bool(value, &b);
        JAMI_LOG("PipeWireStream: value: {}", b);
        break;
    case SPA_TYPE_Int:
        spa_pod_get_int(value, &val);
        JAMI_LOG("PipeWireStream: value: {:d}", val);
        break;
    case SPA_TYPE_Long:
        spa_pod_get_long(value, &valuel);
        JAMI_LOG("PipeWireStream: value: {:d}", valuel);
        break;
    case SPA_TYPE_Float:
        spa_pod_get_float(value, &f);
        JAMI_LOG("PipeWireStream: value float: {:f}", f);
        break;
    case SPA_TYPE_Double:
        spa_pod_get_double(value, &d);
        JAMI_LOG("PipeWireStream: value double: {:f}", d);
        break;
    case SPA_TYPE_String:
        spa_pod_get_string(value, &str);
        JAMI_LOG("PipeWireStream: value str: {:s}", str);
        break;
    case SPA_TYPE_Bytes:
        JAMI_LOG("PipeWireStream: value: bytes");
        break;
    case SPA_TYPE_Rectangle:
        JAMI_LOG("PipeWireStream: value: rectangle");
        break;
    case SPA_TYPE_Fraction:
    JAMI_LOG("PipeWireStream: value: fraction");
    break;
    case SPA_TYPE_Array:
        n_values = SPA_POD_ARRAY_N_VALUES((spa_pod_array*)value);
        JAMI_LOG("PipeWireStream: value: array size {}", n_values);
        SPA_POD_ARRAY_FOREACH((spa_pod_array*)value, iter) {
            printPodValue(iter);
        }
        break;
    case SPA_TYPE_Object:
        JAMI_LOG("PipeWireStream: value: object");
        printPodObject((spa_pod_object*)value);
        break;
    case SPA_TYPE_None:
        JAMI_LOG("PipeWireStream: value: none");
        break;
    case SPA_TYPE_Pod:
        JAMI_LOG("PipeWireStream: value: pod");
        break;
    case SPA_TYPE_Id:
        spa_pod_get_id(value, &choice);
        JAMI_LOG("PipeWireStream: value Id: {:d}", choice);
        break;
    case SPA_TYPE_Choice:
        JAMI_LOG("PipeWireStream: value: choice");
        printPodChoice(value);
        break;
    case SPA_TYPE_Fd:
        JAMI_LOG("PipeWireStream: value: fd");
        break;
    case SPA_TYPE_Pointer:
        JAMI_LOG("PipeWireStream: value: pointer");
        break;
    case SPA_TYPE_Struct:
        JAMI_LOG("PipeWireStream: value: struct");
        printPodStruct(value);
        break;
    default:
        JAMI_LOG("PipeWireStream: value: unknown type {}", value->type);
        break;
    }
}

std::string_view
getPropName(uint32_t key)
{
    for (size_t i = 0; spa_type_props[i].name; i++) {
        if (spa_type_props[i].type == key)
            return spa_type_props[i].name;
    }
    return "unknown";
}

std::string_view
getPropIdName(uint32_t key)
{
    for (size_t i = 0; spa_type_param[i].name; i++) {
        if (spa_type_param[i].type == key)
            return spa_type_param[i].name;
    }
    return "unknown";
}

void printPodObject(const spa_pod_object* param)
{
    spa_pod_prop *prop;
    SPA_POD_OBJECT_FOREACH(param, prop) {
        JAMI_LOG("PipeWireStream: prop key: {:s}", getPropName(prop->key));
        printPodValue(&prop->value);
    }
}

void PipeWireStream::streamParamChanged(void* data, uint32_t id, const spa_pod* param)
{
    PipeWireStream* self = static_cast<PipeWireStream*>(data);
    JAMI_LOG("PipeWireStream: Stream param changed: {}", getPropIdName(id));

    if (id == SPA_PARAM_Props) {
        printPodObject((spa_pod_object*)param);
        return;
    }
    else if (id == SPA_PARAM_Latency) {
        spa_latency_info info;
        spa_latency_parse(param, &info);
        JAMI_LOG("PipeWireStream: Stream latency: {:d}\n  quantum: {:f} - {:f}\n  rate: {:d} - {:d}\n  ns: {:d} - {:d}",
            (int)info.direction, info.min_quantum, info.max_quantum, info.min_rate, info.max_rate, info.min_ns, info.max_ns);
        return;
    } else if (id == SPA_PARAM_Tag) {
        if (param) {
            spa_tag_info info;
            void* state = nullptr;
            spa_tag_parse(param, &info, &state);
            if (info.direction == SPA_DIRECTION_INPUT) {
                JAMI_LOG("PipeWireStream: Stream tag input");
            } else {
                JAMI_LOG("PipeWireStream: Stream tag output");
            }
            if (info.info) {
                printPodValue(info.info);
            }
        } else {
            JAMI_LOG("PipeWireStream: empty Stream tag");
        }
        return;
    }

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format) {
        JAMI_WARNING("Invalid format {}", id);
        return;
    }

    struct spa_audio_info format;
    if (spa_format_parse(param, &format.media_type, &format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (format.media_type != SPA_MEDIA_TYPE_audio ||
        format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &format.info.raw);

    self->format_.sample_rate = format.info.raw.rate;
    self->format_.nb_channels = format.info.raw.channels;
    self->format_.sampleFormat = sampleFormatFromPipe(format.info.raw.format);
    JAMI_WARNING("Stream format: {}\n", self->format_.toString());

    self->onFormatChange_(self->format_);
}

void PipeWireStream::streamProcess(void* data)
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
