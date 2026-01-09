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

#pragma once

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <pulse/error.h>
#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <unistd.h>

class PulseLoopbackCapture
{
public:
    using AudioFrameCallback = std::function<void(const void* data, size_t length)>;

    PulseLoopbackCapture();
    ~PulseLoopbackCapture();

    bool startCaptureAsync(AudioFrameCallback callback);
    void stopCapture();

    bool isRunning() const { return running_.load(); }
    uint32_t sampleRate() const { return SAMPLE_SPEC.rate; }
    uint8_t channels() const { return SAMPLE_SPEC.channels; }

private:
    // PulseAudio callbacks
    static void contextStateCallback(pa_context* c, void* userdata);
    static void serverInfoCallback(pa_context* c, const pa_server_info* i, void* userdata);
    static void moduleLoadedCallback(pa_context* c, uint32_t idx, void* userdata);
    static void subscribeCallback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata);
    static void sinkInputInfoCallback(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata);
    static void streamReadCallback(pa_stream* s, size_t length, void* userdata);

    // Internal logic
    void runMainLoop();
    void moveStreamIfNeeded(uint32_t streamIdx, pid_t streamPid, uint32_t ownerModuleIdx, const char* streamName);
    void setupModules(const std::string& default_sink);
    void startRecordingStream();
    void unloadModulesAndQuit();

    // Configuration
    std::string uniqueSinkName_;
    const pa_sample_spec SAMPLE_SPEC = {PA_SAMPLE_S16LE, 48000, 2};

    // State
    pa_threaded_mainloop* mainloop_ = nullptr;
    pa_mainloop_api* mainloopApi_ = nullptr;
    pa_context* context_ = nullptr;
    pa_stream* recordStream_ = nullptr;

    std::atomic<bool> running_ {false};

    AudioFrameCallback dataCallback_;

    pid_t myPid_;
    std::string defaultSinkName_;
    uint32_t nullSinkModuleIdx_ = PA_INVALID_INDEX;
    uint32_t loopbackModuleIdx_ = PA_INVALID_INDEX;
};
