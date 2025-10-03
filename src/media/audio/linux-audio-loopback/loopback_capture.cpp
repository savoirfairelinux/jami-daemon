#include "loopback_capture.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define X11_Success 0

LoopbackCapture::LoopbackCapture() 
    : context(nullptr)
    , stream(nullptr)
    , mainloop(nullptr)
    , mainloop_api(nullptr)
    , channel_map_set(false)
    , flags(static_cast<pa_stream_flags_t>(0))
    , capturing(false)
    , should_stop(false)
    , verbose(false)
    , stream_ready(false)
{
    // Set default audio format
    sample_spec.format = PA_SAMPLE_S16LE;
    sample_spec.rate = 44100;
    sample_spec.channels = 2;
}

LoopbackCapture::~LoopbackCapture() {
    stopCaptureAsync();
}

void LoopbackCapture::setSampleSpec(pa_sample_format_t format, uint32_t rate, uint8_t channels) {
    if (capturing) {
        std::cerr << "Cannot change sample spec while capturing" << std::endl;
        return;
    }
    
    sample_spec.format = format;
    sample_spec.rate = rate;
    sample_spec.channels = channels;
}

void LoopbackCapture::setVerbose(bool verbose_mode) {
    verbose = verbose_mode;
}

bool LoopbackCapture::startCaptureAsync(const std::string& deviceId, AudioDataCallback callback) {
    if (capturing) {
        std::cerr << "Already capturing" << std::endl;
        return false;
    }
    
    if (!callback) {
        std::cerr << "Audio callback cannot be null" << std::endl;
        return false;
    }
    
    audio_callback = callback;
    device_name = deviceId.empty() ? "" : deviceId;
    should_stop = false;
    stream_ready = false;
    
    // Start capture thread
    capture_thread = std::thread(&LoopbackCapture::capture_loop, this);

    // Wait for stream to be ready (with timeout)
    std::unique_lock<std::mutex> lock(stream_mutex);
    if (stream_ready_cv.wait_for(lock, std::chrono::seconds(5), [this] { return stream_ready || should_stop; })) {
        return stream_ready && !should_stop;
    } else {
        std::cerr << "Timeout waiting for stream to be ready" << std::endl;
        stopCaptureAsync();
        return false;
    }
    
    return true;
}

bool LoopbackCapture::startCaptureFromWindow(const std::string& windowId, AudioDataCallback callback) {
    if (capturing) {
        std::cerr << "Already capturing" << std::endl;
        return false;
    }
    
    if (!callback) {
        std::cerr << "Audio callback cannot be null" << std::endl;
        return false;
    }
    
    // Force verbose mode for debugging
    setVerbose(true);
    
    std::cout << "[DEBUG] Starting capture for window ID: " << windowId << std::endl;
    
    // Get PID from window ID
    pid_t pid = getWindowPID(windowId);
    if (pid <= 0) {
        std::cerr << "[ERROR] Failed to find process for window ID: " << windowId << std::endl;
        return false;
    }
    
    std::cout << "[DEBUG] Found PID: " << pid << " for window " << windowId << std::endl;
    
    // Find PulseAudio stream for this PID
    std::string stream_name = findStreamByPID(pid);
    if (stream_name.empty()) {
        std::cerr << "[ERROR] No PulseAudio stream found for PID: " << pid << std::endl;
        std::cerr << "[DEBUG] Available streams found: " << available_streams.size() << std::endl;
        for (const auto& stream : available_streams) {
            std::cerr << "[DEBUG] - " << stream << std::endl;
        }
        
        // Try fallback: use default monitor source
        std::cout << "[DEBUG] Trying fallback to default monitor source" << std::endl;
        stream_name = "@DEFAULT_MONITOR@";
    }
    
    std::cout << "[DEBUG] Using stream/device: " << stream_name << " for PID " << pid << std::endl;
    
    // Start capturing from the monitor source of the found stream
    return startCaptureAsync(stream_name, callback);
}

void LoopbackCapture::stopCaptureAsync() {
    if (!capturing && !capture_thread.joinable()) {
        return;
    }
    
    should_stop = true;
    
    // Signal the mainloop to quit
    if (mainloop_api) {
        mainloop_api->quit(mainloop_api, 0);
    }
    
    // Wait for capture thread to finish
    if (capture_thread.joinable()) {
        capture_thread.join();
    }
    
    capturing = false;
}

bool LoopbackCapture::isCapturing() const {
    return capturing;
}

void LoopbackCapture::capture_loop() {
    capturing = true;
    
    // Set up a new main loop
    if (!(mainloop = pa_mainloop_new())) {
        std::cerr << "pa_mainloop_new() failed" << std::endl;
        capturing = false;
        return;
    }
    
    mainloop_api = pa_mainloop_get_api(mainloop);
    
    // Create a new connection context
    if (!(context = pa_context_new(mainloop_api, "LoopbackCapture"))) {
        std::cerr << "pa_context_new() failed" << std::endl;
        cleanup();
        capturing = false;
        return;
    }
    
    pa_context_set_state_callback(context, context_state_callback, this);
    
    // Connect the context
    if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        std::cerr << "pa_context_connect() failed: " << pa_strerror(pa_context_errno(context)) << std::endl;
        cleanup();
        capturing = false;
        return;
    }
    
    // Run the main loop
    int ret;
    if (pa_mainloop_run(mainloop, &ret) < 0) {
        std::cerr << "pa_mainloop_run() failed" << std::endl;
    }
    
    cleanup();
    capturing = false;
}

void LoopbackCapture::cleanup() {
    if (stream) {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
        stream = nullptr;
    }
    
    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = nullptr;
    }
    
    if (mainloop) {
        pa_mainloop_free(mainloop);
        mainloop = nullptr;
        mainloop_api = nullptr;
    }
}

void LoopbackCapture::quit_mainloop(int ret) {
    if (mainloop_api) {
        mainloop_api->quit(mainloop_api, ret);
    }
}

// Static callback functions
void LoopbackCapture::context_state_callback(pa_context* c, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(c);
    
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
            
        case PA_CONTEXT_READY: {
            assert(c);
            assert(!self->stream);
            
            if (self->verbose) {
                std::cerr << "Connection established" << std::endl;
            }
            
            // Validate sample spec
            if (!pa_sample_spec_valid(&self->sample_spec)) {
                std::cerr << "Invalid sample specification" << std::endl;
                self->quit_mainloop(1);
                return;
            }
            
            if (!(self->stream = pa_stream_new(c, "LoopbackCaptureStream", &self->sample_spec, 
                                             self->channel_map_set ? &self->channel_map : nullptr))) {
                std::cerr << "pa_stream_new() failed: " << pa_strerror(pa_context_errno(c)) << std::endl;
                self->quit_mainloop(1);
                return;
            }
            
            pa_stream_set_state_callback(self->stream, stream_state_callback, userdata);
            pa_stream_set_read_callback(self->stream, stream_read_callback, userdata);
            pa_stream_set_suspended_callback(self->stream, stream_suspended_callback, userdata);
            pa_stream_set_moved_callback(self->stream, stream_moved_callback, userdata);
            pa_stream_set_overflow_callback(self->stream, stream_overflow_callback, userdata);
            pa_stream_set_started_callback(self->stream, stream_started_callback, userdata);
            pa_stream_set_event_callback(self->stream, stream_event_callback, userdata);
            
            const char* device = self->device_name.empty() ? nullptr : self->device_name.c_str();
            
            if (pa_stream_connect_record(self->stream, device, nullptr, self->flags) < 0) {
                std::cerr << "pa_stream_connect_record() failed: " << pa_strerror(pa_context_errno(c)) << std::endl;
                self->quit_mainloop(1);
                return;
            }
            
            break;
        }
        
        case PA_CONTEXT_TERMINATED:
            self->quit_mainloop(0);
            break;
            
        case PA_CONTEXT_FAILED:
        default:
            std::cerr << "Connection failure: " << pa_strerror(pa_context_errno(c)) << std::endl;
            self->quit_mainloop(1);
            break;
    }
}

void LoopbackCapture::stream_state_callback(pa_stream* s, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
            
        case PA_STREAM_READY:
            {
                std::lock_guard<std::mutex> lock(self->stream_mutex);
                self->stream_ready = true;
                self->stream_ready_cv.notify_one();
            }
            if (self->verbose) {
                const pa_buffer_attr* a;
                char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];
                
                std::cerr << "Stream successfully created" << std::endl;
                
                if (!(a = pa_stream_get_buffer_attr(s))) {
                    std::cerr << "pa_stream_get_buffer_attr() failed: " 
                             << pa_strerror(pa_context_errno(pa_stream_get_context(s))) << std::endl;
                } else {
                    std::cerr << "Buffer metrics: maxlength=" << a->maxlength 
                             << ", fragsize=" << a->fragsize << std::endl;
                }
                
                std::cerr << "Using sample spec '" 
                         << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(s))
                         << "', channel map '" 
                         << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(s))
                         << "'" << std::endl;
                
                std::cerr << "Connected to device " << pa_stream_get_device_name(s) 
                         << " (" << pa_stream_get_device_index(s) 
                         << ", " << (pa_stream_is_suspended(s) ? "" : "not ") << "suspended)" << std::endl;
            }
            break;
            
        case PA_STREAM_FAILED:
        default:
            {
                std::lock_guard<std::mutex> lock(self->stream_mutex);
                self->stream_ready = false;
                self->stream_ready_cv.notify_one();
            }
            std::cerr << "Stream error: " << pa_strerror(pa_context_errno(pa_stream_get_context(s))) << std::endl;
            self->quit_mainloop(1);
            break;
    }
}

void LoopbackCapture::stream_read_callback(pa_stream* s, size_t length, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    const void* data;
    assert(s);
    assert(length > 0);
    
    if (self->should_stop) {
        self->quit_mainloop(0);
        return;
    }
    
    if (pa_stream_peek(s, &data, &length) < 0) {
        std::cerr << "pa_stream_peek() failed: " << pa_strerror(pa_context_errno(self->context)) << std::endl;
        self->quit_mainloop(1);
        return;
    }
    
    if (data && length > 0 && self->audio_callback) {
        const pa_sample_spec* spec = pa_stream_get_sample_spec(s);
        self->audio_callback(data, length, *spec);
    }
    
    pa_stream_drop(s);
}

void LoopbackCapture::stream_suspended_callback(pa_stream* s, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    
    if (self->verbose) {
        if (pa_stream_is_suspended(s)) {
            std::cerr << "Stream device suspended" << std::endl;
        } else {
            std::cerr << "Stream device resumed" << std::endl;
        }
    }
}

void LoopbackCapture::stream_overflow_callback(pa_stream* s, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    
    if (self->verbose) {
        std::cerr << "Stream overrun" << std::endl;
    }
}

void LoopbackCapture::stream_started_callback(pa_stream* s, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    
    if (self->verbose) {
        std::cerr << "Stream started" << std::endl;
    }
}

void LoopbackCapture::stream_moved_callback(pa_stream* s, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    
    if (self->verbose) {
        std::cerr << "Stream moved to device " << pa_stream_get_device_name(s) 
                 << " (" << pa_stream_get_device_index(s) 
                 << ", " << (pa_stream_is_suspended(s) ? "" : "not ") << "suspended)" << std::endl;
    }
}

void LoopbackCapture::stream_event_callback(pa_stream* s, const char* name, pa_proplist* pl, void* userdata) {
    LoopbackCapture* self = static_cast<LoopbackCapture*>(userdata);
    assert(s);
    assert(name);
    assert(pl);
    
    if (self->verbose) {
        char* t = pa_proplist_to_string_sep(pl, ", ");
        std::cerr << "Got event '" << name << "', properties '" << t << "'" << std::endl;
        pa_xfree(t);
    }
}

// Helper methods for window-based capture
pid_t LoopbackCapture::getWindowPID(const std::string& windowId) {
    std::cout << "[DEBUG] Opening X11 display to get PID for window " << windowId << "\n";
    
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "[ERROR] Failed to open X11 display\n";
        return -1;
    }
    
    // Convert window ID string to Window
    Window window;
    try {
        if (windowId.substr(0, 2) == "0x" || windowId.substr(0, 2) == "0X") {
            // Hexadecimal format
            window = static_cast<Window>(std::stoul(windowId, nullptr, 16));
        } else {
            // Decimal format
            window = static_cast<Window>(std::stoul(windowId, nullptr, 10));
        }
        std::cout << "[DEBUG] Parsed window ID: " << windowId << " -> " << window << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse window ID: " << windowId << " (" << e.what() << ")\n";
        XCloseDisplay(display);
        return -1;
    }
    
    Atom atom = XInternAtom(display, "_NET_WM_PID", True);
    if (atom == None) {
        std::cerr << "[ERROR] _NET_WM_PID atom not found\n";
        XCloseDisplay(display);
        return -1;
    }
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = nullptr;
    
    std::cout << "[DEBUG] Querying window property _NET_WM_PID for window " << window << "\n";
    
    int result = XGetWindowProperty(display, window, atom, 0, 1, False,
                                   XA_CARDINAL, &actual_type, &actual_format,
                                   &nitems, &bytes_after, &prop);
    
    pid_t pid = -1;
    if (result == X11_Success && prop) {
        if (actual_type == XA_CARDINAL && actual_format == 32 && nitems == 1) {
            pid = *reinterpret_cast<pid_t*>(prop);
            std::cout << "[DEBUG] Successfully extracted PID: " << pid << " from window " << window << "\n";
        } else {
            std::cerr << "[ERROR] Window property format mismatch: type=" << actual_type 
                     << ", format=" << actual_format << ", nitems=" << nitems << "\n";
        }
        XFree(prop);
    } else {
        std::cerr << "[ERROR] Failed to get _NET_WM_PID property: result=" << result << "\n";
        if (result != X11_Success) {
            std::cerr << "[ERROR] X11 error code: " << result << "\n";
        }
    }
    
    XCloseDisplay(display);
    return pid;
}

std::string LoopbackCapture::findStreamByPID(pid_t target_pid) {
    target_stream_name.clear();
    available_streams.clear();
    
    // Create a temporary context to query PulseAudio
    pa_mainloop* ml = pa_mainloop_new();
    if (!ml) {
        std::cerr << "Failed to create mainloop for stream query" << std::endl;
        return "";
    }
    
    pa_mainloop_api* api = pa_mainloop_get_api(ml);
    pa_context* ctx = pa_context_new(api, "StreamFinder");
    if (!ctx) {
        std::cerr << "Failed to create context for stream query" << std::endl;
        pa_mainloop_free(ml);
        return "";
    }
    
    // Simple synchronous connection
    pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    
    // Wait for connection
    pa_context_state_t state;
    while ((state = pa_context_get_state(ctx)) != PA_CONTEXT_READY) {
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            std::cerr << "Failed to connect to PulseAudio for stream query" << std::endl;
            pa_context_unref(ctx);
            pa_mainloop_free(ml);
            return "";
        }
        pa_mainloop_iterate(ml, 1, nullptr);
    }
    
    // Query sink inputs (applications playing audio)
    struct QueryData {
        LoopbackCapture* self;
        pid_t target_pid;
        bool done;
    } query_data = { this, target_pid, false };
    
    pa_context_get_sink_input_info_list(ctx, sink_input_info_callback, &query_data);
    
    // Process events until query is done
    while (!query_data.done) {
        pa_mainloop_iterate(ml, 1, nullptr);
    }
    
    // If we didn't find it in sink inputs, try source outputs (applications recording audio)
    if (target_stream_name.empty()) {
        query_data.done = false;
        pa_context_get_source_output_info_list(ctx, source_output_info_callback, &query_data);
        
        while (!query_data.done) {
            pa_mainloop_iterate(ml, 1, nullptr);
        }
    }
    
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(ml);
    
    if (!target_stream_name.empty()) {
        // For sink inputs, we want to capture from the monitor source
        if (target_stream_name.find("sink:") == 0) {
            target_stream_name = target_stream_name.substr(5) + ".monitor";
        }
    }
    
    return target_stream_name;
}

void LoopbackCapture::sink_input_info_callback(pa_context* c, const pa_sink_input_info* info, int eol, void* userdata) {
    (void)c; // Suppress unused parameter warning
    
    struct QueryData {
        LoopbackCapture* self;
        pid_t target_pid;
        bool done;
    };
    
    QueryData* data = static_cast<QueryData*>(userdata);
    
    if (eol) {
        data->done = true;
        return;
    }
    
    if (!info) return;
    
    // Get PID from properties
    const char* pid_str = pa_proplist_gets(info->proplist, "application.process.id");
    const char* app_name = pa_proplist_gets(info->proplist, "application.name");
    
    if (pid_str) {
        pid_t pid = static_cast<pid_t>(std::stoul(pid_str));
        
        // Add to available streams list for debugging
        std::string stream_desc = "sink_input:" + std::string(app_name ? app_name : "unknown") + 
                                 " (PID:" + std::string(pid_str) + ")";
        data->self->available_streams.push_back(stream_desc);
        
        if (data->self->verbose) {
            std::cout << "[DEBUG] Found sink input: " << stream_desc << "\n";
        }
        
        if (pid == data->target_pid) {
            // Found the target process
            const char* sink_name = pa_proplist_gets(info->proplist, "sink.name");
            if (!sink_name) {
                // Get sink name from sink index
                data->self->target_stream_name = "sink:" + std::to_string(info->sink);
            } else {
                data->self->target_stream_name = "sink:" + std::string(sink_name);
            }
            
            std::cout << "[DEBUG] Found target sink input for PID " << pid 
                     << " (app: " << (app_name ? app_name : "unknown") 
                     << ") on sink: " << data->self->target_stream_name << "\n";
        }
    }
}

void LoopbackCapture::source_output_info_callback(pa_context* c, const pa_source_output_info* info, int eol, void* userdata) {
    (void)c; // Suppress unused parameter warning
    
    struct QueryData {
        LoopbackCapture* self;
        pid_t target_pid;
        bool done;
    };
    
    QueryData* data = static_cast<QueryData*>(userdata);
    
    if (eol) {
        data->done = true;
        return;
    }
    
    if (!info) return;
    
    // Get PID from properties
    const char* pid_str = pa_proplist_gets(info->proplist, "application.process.id");
    const char* app_name = pa_proplist_gets(info->proplist, "application.name");
    
    if (pid_str) {
        pid_t pid = static_cast<pid_t>(std::stoul(pid_str));
        
        // Add to available streams list for debugging
        std::string stream_desc = "source_output:" + std::string(app_name ? app_name : "unknown") + 
                                 " (PID:" + std::string(pid_str) + ")";
        data->self->available_streams.push_back(stream_desc);
        
        if (data->self->verbose) {
            std::cout << "[DEBUG] Found source output: " << stream_desc << "\n";
        }
        
        if (pid == data->target_pid) {
            // Found the target process
            const char* source_name = pa_proplist_gets(info->proplist, "source.name");
            if (!source_name) {
                data->self->target_stream_name = "source:" + std::to_string(info->source);
            } else {
                data->self->target_stream_name = std::string(source_name);
            }
            
            std::cout << "[DEBUG] Found target source output for PID " << pid 
                     << " (app: " << (app_name ? app_name : "unknown") 
                     << ") on source: " << data->self->target_stream_name << "\n";
        }
    }
}