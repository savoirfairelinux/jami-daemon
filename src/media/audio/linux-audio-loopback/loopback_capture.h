#pragma once

#include <pulse/pulseaudio.h>
#include <pulse/introspect.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class LoopbackCapture {
public:
    // Callback function type for audio data
    // Parameters: data pointer, data length in bytes, sample spec
    using AudioDataCallback = std::function<void(const void* data, size_t length, const pa_sample_spec& spec)>;

    LoopbackCapture();
    ~LoopbackCapture();

    /**
     * Start capturing audio asynchronously from a specific window
     * @param windowId: X11 window ID (e.g., "0x3800016")
     * @param callback: Function to call when audio data is available
     * @return true if capture started successfully, false otherwise
     */
    bool startCaptureFromWindow(const std::string& windowId, AudioDataCallback callback);
    
    /**
     * Start capturing audio asynchronously from a device
     * @param deviceId: PulseAudio source device name (empty string for default)
     * @param callback: Function to call when audio data is available
     * @return true if capture started successfully, false otherwise
     */
    bool startCaptureAsync(const std::string& deviceId, AudioDataCallback callback);
    
    void stopCaptureAsync();
    
    bool isCapturing() const;
    
    // Set audio format parameters
    void setSampleSpec(pa_sample_format_t format, uint32_t rate, uint8_t channels);
    
    void setVerbose(bool verbose);

private:
    // PulseAudio objects
    pa_context* context;
    pa_stream* stream;
    pa_mainloop* mainloop;
    pa_mainloop_api* mainloop_api;
    
    // Audio configuration
    pa_sample_spec sample_spec;
    pa_channel_map channel_map;
    bool channel_map_set;
    pa_stream_flags_t flags;
    
    // Capture state
    std::atomic<bool> capturing;
    std::atomic<bool> should_stop;
    std::thread capture_thread;
    AudioDataCallback audio_callback;
    std::string device_name;
    bool verbose;

    std::mutex stream_mutex;
    std::condition_variable stream_ready_cv;
    bool stream_ready;
    
    // Window to stream mapping
    std::string target_stream_name;
    std::vector<std::string> available_streams;
    
    // Helper methods for window-based capture
    pid_t getWindowPID(const std::string& windowId);
    std::string findStreamByPID(pid_t pid);
    static void sink_input_info_callback(pa_context* c, const pa_sink_input_info* info, int eol, void* userdata);
    static void source_output_info_callback(pa_context* c, const pa_source_output_info* info, int eol, void* userdata);
    
    // PulseAudio callbacks
    static void context_state_callback(pa_context* c, void* userdata);
    static void stream_state_callback(pa_stream* s, void* userdata);
    static void stream_read_callback(pa_stream* s, size_t length, void* userdata);
    static void stream_suspended_callback(pa_stream* s, void* userdata);
    static void stream_overflow_callback(pa_stream* s, void* userdata);
    static void stream_started_callback(pa_stream* s, void* userdata);
    static void stream_moved_callback(pa_stream* s, void* userdata);
    static void stream_event_callback(pa_stream* s, const char* name, pa_proplist* pl, void* userdata);
    
    // Internal methods
    void capture_loop();
    void cleanup();
    void quit_mainloop(int ret = 0);
    
    // Delete copy constructor and assignment operator
    LoopbackCapture(const LoopbackCapture&) = delete;
    LoopbackCapture& operator=(const LoopbackCapture&) = delete;
};