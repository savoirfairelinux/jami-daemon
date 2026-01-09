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
    // PulseAudio callbacks (static trampolines)
    static void context_state_callback(pa_context* c, void* userdata);
    static void server_info_callback(pa_context* c, const pa_server_info* i, void* userdata);
    static void module_loaded_callback(pa_context* c, uint32_t idx, void* userdata);
    static void subscribe_callback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata);
    static void sink_input_info_callback(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata);
    static void stream_read_callback(pa_stream* s, size_t length, void* userdata);

    // Internal logic
    void runMainLoop();
    void moveStreamIfNeeded(uint32_t stream_index, pid_t stream_pid, uint32_t owner_module_idx, const char* stream_name);
    void setupModules(const std::string& default_sink);
    void startRecordingStream();
    void unloadModulesAndQuit();

    // Configuration
    std::string unique_sink_name_;
    const pa_sample_spec SAMPLE_SPEC = {PA_SAMPLE_S16LE, 48000, 2};

    // State
    pa_threaded_mainloop* mainloop_ = nullptr;
    pa_mainloop_api* mainloop_api_ = nullptr;
    pa_context* context_ = nullptr;
    pa_stream* record_stream_ = nullptr;

    std::atomic<bool> running_ {false};

    AudioFrameCallback data_callback_;

    pid_t my_pid_;
    std::string default_sink_name_;
    uint32_t null_sink_module_idx_ = PA_INVALID_INDEX;
    uint32_t loopback_module_idx_ = PA_INVALID_INDEX;

    // Helper used during initialization to chain async calls
    struct InitData
    {
        PulseLoopbackCapture* self;
        std::string sink_name;
    };
};
