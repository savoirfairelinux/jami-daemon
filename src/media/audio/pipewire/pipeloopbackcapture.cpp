#include "pipeloopbackcapture.h"

#include "logger.h"

#include <cerrno>
#include <cstring>

// Static PipeWire event structures
static const struct pw_stream_events stream_events = {
    /* .version = */ PW_VERSION_STREAM_EVENTS,
    /* .destroy = */ nullptr,
    /* .state_changed = */ nullptr,
    /* .control_info = */ nullptr,
    /* .io_changed = */ nullptr,
    /* .param_changed = */ nullptr,
    /* .add_buffer = */ nullptr,
    /* .remove_buffer = */ nullptr,
    /* .process = */ LoopbackCapture::on_process,
    /* .drained = */ nullptr,
    /* .command = */ nullptr,
    /* .trigger_done = */ nullptr,
};

static const struct pw_registry_events registry_events = {
    /* .version = */ PW_VERSION_REGISTRY_EVENTS,
    /* .global = */ LoopbackCapture::registry_event_global,
    /* .global_remove = */ LoopbackCapture::registry_event_global_remove,
};

LoopbackCapture::LoopbackCapture() 
    : is_running_(false), should_stop_(false) {
    // Initialize app_data structure
    app_data_.loop = nullptr;
    app_data_.context = nullptr;
    app_data_.core = nullptr;
    app_data_.registry = nullptr;
    app_data_.parent = this;
    app_data_.excluded_app = "";
}

LoopbackCapture::~LoopbackCapture() {
    stopCapture();
}

bool LoopbackCapture::startCaptureAsync(const std::string& exclude_app, AudioCallback callback) {
    if (is_running_.load()) {
        JAMI_WARNING("Capture is already running");
        return false;
    }

    callback_ = std::move(callback);
    app_data_.excluded_app = exclude_app;
    should_stop_.store(false);

    // Initialize PipeWire in the background thread
    capture_thread_ = std::make_unique<std::thread>(&LoopbackCapture::run_capture_loop, this);
    
    return true;
}

void LoopbackCapture::stopCapture() {
    if (!is_running_.load() && !capture_thread_) {
        return;
    }

    should_stop_.store(true);
    
    if (app_data_.loop) {
        pw_main_loop_quit(app_data_.loop);
    }

    if (capture_thread_ && capture_thread_->joinable()) {
        capture_thread_->join();
    }
    
    capture_thread_.reset();
    is_running_.store(false);
}

void LoopbackCapture::run_capture_loop() {
    if (!initialize_pipewire()) {
        JAMI_ERROR("Failed to initialize PipeWire");
        return;
    }

    is_running_.store(true);
    JAMI_LOG("LoopbackCapture started. Monitoring audio streams...");

    // Initial enumeration
    roundtrip();

    // Keep running to capture audio
    pw_main_loop_run(app_data_.loop);

    cleanup();
    is_running_.store(false);
    JAMI_LOG("LoopbackCapture stopped.");
}

bool LoopbackCapture::initialize_pipewire() {
    int argc = 0;
    char** argv = nullptr;
    pw_init(&argc, &argv);

    app_data_.loop = pw_main_loop_new(NULL);
    if (!app_data_.loop) {
        JAMI_ERROR("Error: failed to create new main loop object");
        return false;
    }

    app_data_.context = pw_context_new(pw_main_loop_get_loop(app_data_.loop), NULL, 0);
    if (!app_data_.context) {
        JAMI_ERROR("Error: failed to create new context object");
        pw_main_loop_destroy(app_data_.loop);
        return false;
    }

    app_data_.core = pw_context_connect(app_data_.context, NULL, 0);
    if (!app_data_.core) {
        JAMI_ERROR("Error: failed to connect context: {} (errno={})", std::strerror(errno), errno);
        pw_context_destroy(app_data_.context);
        pw_main_loop_destroy(app_data_.loop);
        return false;
    }

    app_data_.registry = pw_core_get_registry(app_data_.core, PW_VERSION_REGISTRY, 0);
    if (!app_data_.registry) {
        JAMI_ERROR("Error: failed to get core registry");
        pw_core_disconnect(app_data_.core);
        pw_context_destroy(app_data_.context);
        pw_main_loop_destroy(app_data_.loop);
        return false;
    }

    static struct spa_hook registry_listener;
    spa_zero(registry_listener);
    pw_registry_add_listener(app_data_.registry, &registry_listener, &registry_events, &app_data_);

    return true;
}

void LoopbackCapture::cleanup() {
    // Cleanup streams
    for (auto &[id, stream] : app_data_.streams) {
        if (stream->stream) {
            pw_stream_destroy(stream->stream);
        }
    }
    app_data_.streams.clear();

    // Cleanup PipeWire objects
    if (app_data_.registry) {
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(app_data_.registry));
    }
    if (app_data_.core) {
        pw_core_disconnect(app_data_.core);
    }
    if (app_data_.context) {
        pw_context_destroy(app_data_.context);
    }
    if (app_data_.loop) {
        pw_main_loop_destroy(app_data_.loop);
    }
}

void LoopbackCapture::on_process(void *userdata) {
    auto *stream_data = static_cast<LoopbackCapture::StreamData*>(userdata);
    struct pw_buffer *b;
    struct spa_buffer *buf;

    b = pw_stream_dequeue_buffer(stream_data->stream);
    if (!b) {
        JAMI_WARNING("out of buffers");
        return;
    }

    buf = b->buffer;
    if (buf->datas[0].data == NULL) {
        pw_stream_queue_buffer(stream_data->stream, b);
        return;
    }

    // Get audio data
    size_t size = buf->datas[0].chunk->size;
    
    if (size > 0 && stream_data->parent && stream_data->parent->callback_) {
        AudioData audio_data = {
            .node_id = stream_data->node_id,
            .app_name = stream_data->app_name,
            .data = buf->datas[0].data,
            .size = size,
            .rate = DEFAULT_RATE,
            .channels = DEFAULT_CHANNELS
        };
        
        stream_data->parent->callback_(audio_data);
    }

    pw_stream_queue_buffer(stream_data->stream, b);
}

void LoopbackCapture::start_recording_stream(uint32_t node_id, const std::string &app_name) {
    auto stream_data_ptr = std::make_unique<StreamData>();
    stream_data_ptr->node_id = node_id;
    stream_data_ptr->app_name = app_name;
    stream_data_ptr->is_recording = false;
    stream_data_ptr->parent = this;

    JAMI_LOG("Starting capture for {} (node {})", app_name, node_id);

    // Create recording stream
    stream_data_ptr->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(app_data_.loop),
        ("loopback_capture_" + app_name).c_str(),
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL),
        &stream_events,
        stream_data_ptr.get());

    if (!stream_data_ptr->stream) {
        JAMI_ERROR("Failed to create stream for {}", app_name);
        return;
    }

    // Set up audio format
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    struct spa_audio_info_raw audio_info = SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_S16,
        .rate = DEFAULT_RATE,
        .channels = DEFAULT_CHANNELS
    );
    
    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect to the specific node for recording
    int result = pw_stream_connect(stream_data_ptr->stream,
                                   PW_DIRECTION_INPUT,
                                   node_id,
                                   static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | 
                                                               PW_STREAM_FLAG_MAP_BUFFERS | 
                                                               PW_STREAM_FLAG_RT_PROCESS),
                                   params, 1);

    if (result < 0) {
        JAMI_ERROR("Failed to connect stream for {}: {}", app_name, std::strerror(-result));
        pw_stream_destroy(stream_data_ptr->stream);
        return;
    }

    stream_data_ptr->is_recording = true;
    app_data_.streams[node_id] = std::move(stream_data_ptr);
}

void LoopbackCapture::registry_event_global(void *data, uint32_t id,
                                           uint32_t permissions,
                                           const char *type,
                                           uint32_t version,
                                           const struct spa_dict *props) {
    auto *app = static_cast<AppData*>(data);
    
    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    if (!props) {
        return;
    }

    const char *media_class = nullptr;
    const char *app_name = nullptr;
    const char *node_name = nullptr;

    for (uint32_t i = 0; i < props->n_items; ++i) {
        if (strcmp(props->items[i].key, "media.class") == 0) {
            media_class = props->items[i].value;
        } else if (strcmp(props->items[i].key, "application.name") == 0) {
            app_name = props->items[i].value;
        } else if (strcmp(props->items[i].key, "node.name") == 0) {
            node_name = props->items[i].value;
        }
    }

    if (media_class && strcmp(media_class, "Stream/Output/Audio") == 0) {
        std::string name = app_name ? app_name : (node_name ? node_name : "unknown");
        
        // Check if this application should be excluded
        if (!app->excluded_app.empty() && name == app->excluded_app) {
            JAMI_DEBUG("Excluding audio output stream: {} (node {})", name, id);
            return;
        }
        
        JAMI_DEBUG("Found audio output stream: {} (node {})", name, id);
        app->parent->start_recording_stream(id, name);
    }
}

void LoopbackCapture::registry_event_global_remove(void *data, uint32_t id) {
    auto *app = static_cast<AppData*>(data);
    
    auto it = app->streams.find(id);
    if (it != app->streams.end()) {
        JAMI_LOG("Stopping capture for {} (node {})", it->second->app_name, id);
        if (it->second->stream) {
            pw_stream_destroy(it->second->stream);
        }
        app->streams.erase(it);
    }
}

void LoopbackCapture::on_core_done(void *data, uint32_t id, int seq) {
    struct roundtrip_data {
        int pending;
        struct pw_main_loop *loop;
    };
    
    auto *d = static_cast<roundtrip_data*>(data);

    if (id == PW_ID_CORE && seq == d->pending)
        pw_main_loop_quit(d->loop);
}

void LoopbackCapture::roundtrip() {
    struct roundtrip_data {
        int pending;
        struct pw_main_loop *loop;
    };

    static const struct pw_core_events core_events = {
        /* .version = */ PW_VERSION_CORE_EVENTS,
        /* .info = */ nullptr,
        /* .done = */ on_core_done,
        /* .ping = */ nullptr,
        /* .error = */ nullptr,
        /* .remove_id = */ nullptr,
        /* .bound_id = */ nullptr,
        /* .add_mem = */ nullptr,
        /* .remove_mem = */ nullptr,
    };

    struct roundtrip_data d = { .loop = app_data_.loop };
    struct spa_hook core_listener;
    int err;

    pw_core_add_listener(app_data_.core, &core_listener, &core_events, &d);

    d.pending = pw_core_sync(app_data_.core, PW_ID_CORE, 0);

    err = pw_main_loop_run(app_data_.loop);
    if (err < 0)
        JAMI_ERROR("main_loop_run error: {}", err);

    spa_hook_remove(&core_listener);
}