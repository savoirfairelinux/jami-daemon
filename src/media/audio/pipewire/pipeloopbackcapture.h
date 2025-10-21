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
#include <string>
#pragma once

#include "pipewire/context.h"
#include "pipewire/core.h"
#include "pipewire/main-loop.h"
#include "pipewire/stream.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <functional>
#include <memory>
#include <map>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <vector>


struct AudioData {
    const void* data;
    size_t size;
    uint32_t rate;
    uint32_t channels;
};

using AudioCallback = std::function<void(const AudioData&)>;

class PipeWireLoopbackCapture {
public:
    // Static callback functions for PipeWire (must be public for C callbacks)
    static void on_process(void *userdata);
    static void registry_event_global(void *data, uint32_t id, uint32_t permissions, 
                                    const char *type, uint32_t version, 
                                    const struct spa_dict *props);
    static void registry_event_global_remove(void *data, uint32_t id);
    static void on_core_done(void *data, uint32_t id, int seq);


private:
    struct StreamData {
        uint32_t node_id;
        std::string app_name;
        struct pw_stream *stream;
        bool is_recording;
        PipeWireLoopbackCapture* parent;
    };

    struct AppData {
        struct pw_main_loop *loop;
        struct pw_context *context;
        struct pw_core *core;
        struct pw_registry *registry;
        std::map<uint32_t, std::unique_ptr<StreamData>> streams;
        PipeWireLoopbackCapture* parent;
    };

    AppData app_data_;
    AudioCallback callback_;
    std::unique_ptr<std::thread> capture_thread_;
    std::atomic<bool> is_running_;
    std::atomic<bool> should_stop_;


    // Mixing state
    std::mutex mix_mutex_;
    std::vector<int32_t> mix_accumulator_;
    std::vector<int16_t> mix_output_;
    std::set<uint32_t> cycle_streams_;
    size_t mix_samples_ = 0;

    // Exclude app name
    std::string exclude_app_;

    static constexpr uint32_t DEFAULT_RATE = 44100;
    static constexpr uint32_t DEFAULT_CHANNELS = 2;

    // Helper methods
    void start_recording_stream(uint32_t node_id, const std::string &app_name);
    void roundtrip();
    void run_capture_loop();
    bool initialize_pipewire();
    void cleanup();
    void mix_and_maybe_flush(uint32_t node_id, const void* data, size_t size);
    void flush_mix_locked();

public:
    PipeWireLoopbackCapture();
    ~PipeWireLoopbackCapture();

    // Delete copy constructor and assignment operator
    PipeWireLoopbackCapture(const PipeWireLoopbackCapture&) = delete;
    PipeWireLoopbackCapture& operator=(const PipeWireLoopbackCapture&) = delete;

    // Main interface
    bool startCaptureAsync(const std::string& exclude_app, AudioCallback callback);
    void stopCapture();
    bool isRunning() const { return is_running_.load(); }
};