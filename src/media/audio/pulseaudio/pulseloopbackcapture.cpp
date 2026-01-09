/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "pulseloopbackcapture.h"
#include "logger.h"

namespace {

PulseLoopbackCapture*
get_instance(void* userdata)
{
    return static_cast<PulseLoopbackCapture*>(userdata);
}

} // namespace

PulseLoopbackCapture::PulseLoopbackCapture()
{
    my_pid_ = getpid();
    unique_sink_name_ = "audiocapture_null_sink_" + std::to_string(my_pid_);
}

PulseLoopbackCapture::~PulseLoopbackCapture()
{
    stopCapture();
}

bool
PulseLoopbackCapture::startCaptureAsync(AudioFrameCallback callback)
{
    if (running_) {
        JAMI_WARNING("[pulseloopbackcapture] Already running");
        return false;
    }

    if (mainloop_ || context_ || record_stream_) {
        JAMI_WARNING("[pulseloopbackcapture] Previous state not fully cleaned, forcing cleanup");
        stopCapture();
    }

    data_callback_ = std::move(callback);

    mainloop_ = pa_threaded_mainloop_new();
    if (!mainloop_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create mainloop");
        return false;
    }

    mainloop_api_ = pa_threaded_mainloop_get_api(mainloop_);

    context_ = pa_context_new(mainloop_api_, "AudioCaptureLib");
    if (!context_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create context");
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        return false;
    }
    pa_context_set_state_callback(context_, context_state_callback, this);

    if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to connect context: {}", pa_strerror(pa_context_errno(context_)));
        pa_context_unref(context_);
        context_ = nullptr;
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        return false;
    }

    if (pa_threaded_mainloop_start(mainloop_) < 0) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to start mainloop");
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        context_ = nullptr;
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        return false;
    }

    running_ = true;
    return true;
}

void
PulseLoopbackCapture::stopCapture()
{
    if (!running_ || !mainloop_)
        return;

    pa_threaded_mainloop_lock(mainloop_);

    auto wait_for_op = [this](pa_operation* op) {
        if (!op)
            return;
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mainloop_);
        }
        pa_operation_unref(op);
    };

    auto completion_cb = [](pa_context* c, int success, void* userdata) {
        (void) c;
        (void) success;
        pa_threaded_mainloop* m = (pa_threaded_mainloop*) userdata;
        pa_threaded_mainloop_signal(m, 0);
    };

    if (loopback_module_idx_ != PA_INVALID_INDEX) {
        pa_operation* op = pa_context_unload_module(context_, loopback_module_idx_, completion_cb, mainloop_);
        wait_for_op(op);
        loopback_module_idx_ = PA_INVALID_INDEX;
    }
    if (null_sink_module_idx_ != PA_INVALID_INDEX) {
        pa_operation* op = pa_context_unload_module(context_, null_sink_module_idx_, completion_cb, mainloop_);
        wait_for_op(op);
        null_sink_module_idx_ = PA_INVALID_INDEX;
    }

    if (record_stream_) {
        pa_stream_disconnect(record_stream_);
        pa_stream_unref(record_stream_);
        record_stream_ = nullptr;
    }

    pa_context_disconnect(context_);
    pa_context_unref(context_);
    context_ = nullptr;

    pa_threaded_mainloop_unlock(mainloop_);

    pa_threaded_mainloop_stop(mainloop_);
    pa_threaded_mainloop_free(mainloop_);
    mainloop_ = nullptr;

    running_ = false;
}

// -------------------------------------------------------------------------
// Callbacks (Trampolines)
// -------------------------------------------------------------------------

void
PulseLoopbackCapture::context_state_callback(pa_context* c, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY: {
        pa_operation* o = pa_context_get_server_info(c, server_info_callback, self);
        if (o)
            pa_operation_unref(o);
        break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        JAMI_ERROR("[pulseloopbackcapture] Context failed/terminated: {}", pa_strerror(pa_context_errno(c)));
        break;
    default:
        break;
    }
}

void
PulseLoopbackCapture::server_info_callback(pa_context* c, const pa_server_info* i, void* userdata)
{
    if (!i)
        return;
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    self->default_sink_name_ = i->default_sink_name;

    std::string null_args = fmt::format("sink_name={} sink_properties=device.description='Desktop_Capture_Mix_{}' "
                                        "format=s16le rate=48000 channels=2",
                                        self->unique_sink_name_,
                                        self->my_pid_);

    pa_operation* o = pa_context_load_module(c, "module-null-sink", null_args.c_str(), module_loaded_callback, self);
    if (o)
        pa_operation_unref(o);
}

void
PulseLoopbackCapture::module_loaded_callback(pa_context* c, uint32_t idx, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    if (self->null_sink_module_idx_ == PA_INVALID_INDEX) {
        self->null_sink_module_idx_ = idx;
        JAMI_DEBUG("[pulseloopbackcapture] Loaded null-sink module: {}", idx);

        std::string loop_args = fmt::format("source={}.monitor sink={} latency_msec=50",
                                            self->unique_sink_name_,
                                            self->default_sink_name_);
        pa_operation* o = pa_context_load_module(c, "module-loopback", loop_args.c_str(), module_loaded_callback, self);
        if (o)
            pa_operation_unref(o);
    } else if (self->loopback_module_idx_ == PA_INVALID_INDEX) {
        self->loopback_module_idx_ = idx;
        JAMI_DEBUG("[pulseloopbackcapture] Loaded loopback module: {}", idx);

        pa_context_set_subscribe_callback(c, subscribe_callback, self);
        pa_operation* o = pa_context_subscribe(c,
                                               (pa_subscription_mask_t) PA_SUBSCRIPTION_MASK_SINK_INPUT,
                                               nullptr,
                                               nullptr);
        if (o)
            pa_operation_unref(o);

        o = pa_context_get_sink_input_info_list(c, sink_input_info_callback, self);
        if (o)
            pa_operation_unref(o);

        self->startRecordingStream();
    }
}

void
PulseLoopbackCapture::startRecordingStream()
{
    record_stream_ = pa_stream_new(context_, "DesktopCapture", &SAMPLE_SPEC, nullptr);
    if (!record_stream_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create record stream");
        return;
    }

    pa_stream_set_read_callback(record_stream_, stream_read_callback, this);

    std::string monitor = unique_sink_name_ + ".monitor";

    pa_buffer_attr attr;
    const uint32_t latency_ms = 10;
    const uint32_t sample_rate = SAMPLE_SPEC.rate;
    const uint32_t channels = SAMPLE_SPEC.channels;
    const uint32_t frame_size = sizeof(int16_t) * channels;

    attr.maxlength = sample_rate * frame_size * latency_ms / 1000;
    attr.tlength = (uint32_t) -1;
    attr.prebuf = (uint32_t) -1;
    attr.minreq = (uint32_t) -1;
    attr.fragsize = sample_rate * frame_size * latency_ms / 2000;

    JAMI_DEBUG("[pulseloopbackcapture] Buffer config: maxlength={}B fragsize={}B ({}ms latency)",
               attr.maxlength,
               attr.fragsize,
               latency_ms);

    pa_stream_connect_record(record_stream_, monitor.c_str(), &attr, PA_STREAM_ADJUST_LATENCY);
    JAMI_DEBUG("[pulseloopbackcapture] Recording stream connected to {}", monitor);
}

void
PulseLoopbackCapture::stream_read_callback(pa_stream* s, size_t length, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || !self->record_stream_ || self->record_stream_ != s)
        return;

    const void* data;
    if (pa_stream_peek(s, &data, &length) < 0)
        return;

    if (data && length > 0 && self->data_callback_) {
        self->data_callback_(data, length);
    }

    pa_stream_drop(s);
}

void
PulseLoopbackCapture::subscribe_callback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
            pa_operation* o = pa_context_get_sink_input_info(c, idx, sink_input_info_callback, self);
            if (o)
                pa_operation_unref(o);
        }
    }
}

void
PulseLoopbackCapture::sink_input_info_callback(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata)
{
    if (eol < 0 || !i)
        return;
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    pid_t pid = 0;
    if (i->proplist) {
        const char* pid_str = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_ID);
        if (pid_str)
            pid = (pid_t) atoi(pid_str);
    }

    self->moveStreamIfNeeded(i->index, pid, i->owner_module, i->name);
}

void
PulseLoopbackCapture::moveStreamIfNeeded(uint32_t stream_index,
                                         pid_t stream_pid,
                                         uint32_t owner_module_idx,
                                         const char* stream_name)
{
    (void) stream_name;
    if (!context_)
        return;
    if (stream_pid == my_pid_)
        return;
    if (owner_module_idx != PA_INVALID_INDEX && owner_module_idx == loopback_module_idx_)
        return;

    pa_operation* o = pa_context_move_sink_input_by_name(context_,
                                                         stream_index,
                                                         unique_sink_name_.c_str(),
                                                         nullptr,
                                                         nullptr);
    if (o)
        pa_operation_unref(o);
}

void
PulseLoopbackCapture::runMainLoop()
{}
