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

struct AudioData {
    uint32_t node_id;
    std::string app_name;
    const void* data;
    size_t size;
    uint32_t rate;
    uint32_t channels;
};

using AudioCallback = std::function<void(const AudioData&)>;

class LoopbackCapture {
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
        LoopbackCapture* parent;
    };

    struct AppData {
        struct pw_main_loop *loop;
        struct pw_context *context;
        struct pw_core *core;
        struct pw_registry *registry;
        std::map<uint32_t, std::unique_ptr<StreamData>> streams;
        LoopbackCapture* parent;
        std::string excluded_app;  // Application name to exclude
    };

    AppData app_data_;
    AudioCallback callback_;
    std::unique_ptr<std::thread> capture_thread_;
    std::atomic<bool> is_running_;
    std::atomic<bool> should_stop_;

    static constexpr uint32_t DEFAULT_RATE = 44100;
    static constexpr uint32_t DEFAULT_CHANNELS = 2;

    // Helper methods
    void start_recording_stream(uint32_t node_id, const std::string &app_name);
    void roundtrip();
    void run_capture_loop();
    bool initialize_pipewire();
    void cleanup();

public:
    LoopbackCapture();
    ~LoopbackCapture();

    // Delete copy constructor and assignment operator
    LoopbackCapture(const LoopbackCapture&) = delete;
    LoopbackCapture& operator=(const LoopbackCapture&) = delete;

    // Main interface
    bool startCaptureAsync(const std::string& exclude_app, AudioCallback callback);
    void stopCapture();
    bool isRunning() const { return is_running_.load(); }
};