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
    myPid_ = getpid();
    uniqueSinkName_ = "audiocapture_null_sink_" + std::to_string(myPid_);
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

    if (mainloop_ || context_ || recordStream_) {
        JAMI_WARNING("[pulseloopbackcapture] Previous state not fully cleaned, forcing cleanup");
        stopCapture();
    }

    dataCallback_ = std::move(callback);

    mainloop_ = pa_threaded_mainloop_new();
    if (!mainloop_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create mainloop");
        return false;
    }

    mainloopApi_ = pa_threaded_mainloop_get_api(mainloop_);

    context_ = pa_context_new(mainloopApi_, "AudioCaptureLib");
    if (!context_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create context");
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        return false;
    }
    pa_context_set_state_callback(context_, contextStateCallback, this);

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

    if (loopbackModuleIdx_ != PA_INVALID_INDEX) {
        pa_operation* op = pa_context_unload_module(context_, loopbackModuleIdx_, completion_cb, mainloop_);
        wait_for_op(op);
        loopbackModuleIdx_ = PA_INVALID_INDEX;
    }
    if (nullSinkModuleIdx_ != PA_INVALID_INDEX) {
        pa_operation* op = pa_context_unload_module(context_, nullSinkModuleIdx_, completion_cb, mainloop_);
        wait_for_op(op);
        nullSinkModuleIdx_ = PA_INVALID_INDEX;
    }

    if (recordStream_) {
        pa_stream_disconnect(recordStream_);
        pa_stream_unref(recordStream_);
        recordStream_ = nullptr;
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
PulseLoopbackCapture::contextStateCallback(pa_context* c, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY: {
        pa_operation* o = pa_context_get_server_info(c, serverInfoCallback, self);
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
PulseLoopbackCapture::serverInfoCallback(pa_context* c, const pa_server_info* i, void* userdata)
{
    if (!i)
        return;
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    self->defaultSinkName_ = i->default_sink_name;

    std::string null_args = fmt::format("sink_name={} sink_properties=device.description='Desktop_Capture_Mix_{}' "
                                        "format=s16le rate=48000 channels=2",
                                        self->uniqueSinkName_,
                                        self->myPid_);

    pa_operation* o = pa_context_load_module(c, "module-null-sink", null_args.c_str(), moduleLoadedCallback, self);
    if (o)
        pa_operation_unref(o);
}

void
PulseLoopbackCapture::moduleLoadedCallback(pa_context* c, uint32_t idx, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    if (self->nullSinkModuleIdx_ == PA_INVALID_INDEX) {
        self->nullSinkModuleIdx_ = idx;
        JAMI_DEBUG("[pulseloopbackcapture] Loaded null-sink module: {}", idx);

        std::string loop_args = fmt::format("source={}.monitor sink={} latency_msec=50",
                                            self->uniqueSinkName_,
                                            self->defaultSinkName_);
        pa_operation* o = pa_context_load_module(c, "module-loopback", loop_args.c_str(), moduleLoadedCallback, self);
        if (o)
            pa_operation_unref(o);
    } else if (self->loopbackModuleIdx_ == PA_INVALID_INDEX) {
        self->loopbackModuleIdx_ = idx;
        JAMI_DEBUG("[pulseloopbackcapture] Loaded loopback module: {}", idx);

        pa_context_set_subscribe_callback(c, subscribeCallback, self);
        pa_operation* o = pa_context_subscribe(c,
                                               (pa_subscription_mask_t) PA_SUBSCRIPTION_MASK_SINK_INPUT,
                                               nullptr,
                                               nullptr);
        if (o)
            pa_operation_unref(o);

        o = pa_context_get_sink_input_info_list(c, sinkInputInfoCallback, self);
        if (o)
            pa_operation_unref(o);

        self->startRecordingStream();
    }
}

void
PulseLoopbackCapture::startRecordingStream()
{
    recordStream_ = pa_stream_new(context_, "DesktopCapture", &SAMPLE_SPEC, nullptr);
    if (!recordStream_) {
        JAMI_ERROR("[pulseloopbackcapture] Failed to create record stream");
        return;
    }

    pa_stream_set_read_callback(recordStream_, streamReadCallback, this);

    std::string monitor = uniqueSinkName_ + ".monitor";

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

    pa_stream_connect_record(recordStream_, monitor.c_str(), &attr, PA_STREAM_ADJUST_LATENCY);
    JAMI_DEBUG("[pulseloopbackcapture] Recording stream connected to {}", monitor);
}

void
PulseLoopbackCapture::streamReadCallback(pa_stream* s, size_t length, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || !self->recordStream_ || self->recordStream_ != s)
        return;

    const void* data;
    if (pa_stream_peek(s, &data, &length) < 0)
        return;

    if (data && length > 0 && self->dataCallback_) {
        self->dataCallback_(data, length);
    }

    pa_stream_drop(s);
}

void
PulseLoopbackCapture::subscribeCallback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata)
{
    auto* self = get_instance(userdata);
    if (!self || !self->context_ || self->context_ != c)
        return;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
            pa_operation* o = pa_context_get_sink_input_info(c, idx, sinkInputInfoCallback, self);
            if (o)
                pa_operation_unref(o);
        }
    }
}

void
PulseLoopbackCapture::sinkInputInfoCallback(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata)
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
PulseLoopbackCapture::moveStreamIfNeeded(uint32_t streamIdx,
                                         pid_t streamPid,
                                         uint32_t ownerModuleIdx,
                                         const char* streamName)
{
    (void) streamName;
    if (!context_)
        return;
    if (streamPid == myPid_)
        return;
    if (ownerModuleIdx != PA_INVALID_INDEX && ownerModuleIdx == loopbackModuleIdx_)
        return;

    pa_operation* o = pa_context_move_sink_input_by_name(context_, streamIdx, uniqueSinkName_.c_str(), nullptr, nullptr);
    if (o)
        pa_operation_unref(o);
}

void
PulseLoopbackCapture::runMainLoop()
{}
