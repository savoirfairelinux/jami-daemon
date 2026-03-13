# Layer 6 — System / Platform Layer

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 6 is the **operating environment** on which all other layers run. It provides the threading primitives, file I/O facilities, platform-specific OS integrations, timer infrastructure, and the logging backend. Unlike Layers 1–5, which describe protocol and application logic, Layer 6 describes the runtime substrate: what threads are alive, how much memory the process consumes, how fast the event loop drains, and whether the OS is applying backpressure on I/O. From an observability perspective, Layer 6 is the home of **process-level resource metrics** and **forwarded log records**.

### Constituting Files and Classes

#### Threading Primitives

| Class | File | Role |
|---|---|---|
| `ThreadLoop` | `src/threadloop.h` / `src/threadloop.cpp` | Reusable `setup → process → cleanup` loop abstraction; every media encode/decode thread uses this; joins on destruction |
| `Manager` (io_context owner) | `src/manager.h` / `src/manager.cpp` | Owns the global `asio::io_context` and its thread pool; all async work in Layers 2–5 dispatches through here |

#### File Utilities

| File | Role |
|---|---|
| `src/fileutils.h` / `src/fileutils.cpp` | XDG / platform path resolution; atomic file write; directory creation; config file lock |
| `src/archiver.h` / `src/archiver.cpp` | Top-level YAML config file reader/writer; orchestrates startup config load |

#### Logging Infrastructure

| Class | File | Role |
|---|---|---|
| `Logger` | `src/logger.h` / `src/logger.cpp` | Level-driven, thread-safe logger; `{fmt}`-based formatting; dispatches to platform sinks |
| `Logger::Handler` | `src/logger.h` | Singleton output handler; mutex-protected concurrent dispatch |
| `Logger::Msg` | `src/logger.h` | Immutable log record: level, file, line, message |
| `JAMI_DBG` / `JAMI_INFO` / `JAMI_WARN` / `JAMI_ERR` | `src/logger.h` | Macro API used by all subsystems |

#### Platform-Specific Sinks and Shims

| File | Platform | Role |
|---|---|---|
| `src/winsyslog.h` / `src/winsyslog.c` | Windows | Windows EventLog syslog shim |
| Android NDK `android/log.h` | Android | logcat backend in `Logger::Handler` |
| POSIX `syslog.h` | Linux / macOS | syslog backend |
| CoreAudio / AVFoundation | iOS / macOS | Audio backend (in `src/media/audio/`) |
| AAudio | Android | Audio backend |

#### Configuration Persistence (platform path resolution)

| Class | File | Role |
|---|---|---|
| `Preferences` | `src/preferences.h` / `.cpp` | Global daemon preferences; serialised via yaml-cpp |
| `AccountConfig` | `src/account_config.h` / `.cpp` | Per-account config base |
| `Archiver` | `src/archiver.h` / `.cpp` | YAML config file load/save; `jamidrc` file |

#### Timer Infrastructure

| Mechanism | Where used |
|---|---|
| `asio::steady_timer` | Ring timeout in `Call`; retry timers in `MessageEngine`; UPnP refresh |
| `std::thread` | `ThreadLoop`, `SIPVoIPLink::sipThread_`, RTCP checker threads |

---

## OTel Relevance

Layer 6 telemetry answers **infrastructure-level questions** that no application-level metric can answer:

- *Is the process running out of memory?* — `ObservableGauge` on `VIRT` / `RES` / `shared` from `/proc/self/status`.
- *Is the event loop saturated?* — queue depth of the ASIO `io_context` (if exposed) or indirectly via call setup latency regression.
- *How many threads are alive?* — from `/proc/self/status` `Threads` field.
- *Are open file descriptors approaching the OS limit?* — `ObservableGauge` on FD count.
- *Are `ThreadLoop` threads crashing or restarting?* — counter for `ThreadLoop::setup()` invocations.
- *What build version is running?* — embedded in `Resource` attributes at process start.

This layer is also where the **OTel Logs Bridge** should be inserted: an implementation of `Logger::Handler` that forwards all `JAMI_WARN` and `JAMI_ERR` calls to the OTel LoggerProvider. This turns every existing logging call site in the daemon into a structured, correlatable OTel log record with no code changes in the emitting subsystems.

---

## Recommended Signal Mix

| Signal | Instruments | Rationale |
|---|---|---|
| **Metrics** (primary) | `ObservableGauge` for process memory, FD count, thread count; `UpDownCounter` for active `ThreadLoop` count | Continuous resource health; no application logic required |
| **Logs** | OTel Logs Bridge in `Logger::Handler`; forwards `WARN` and `ERR` with trace context | Structured log forwarding connecting existing log calls to trace correlation |
| **Traces** | None at this layer | System-level events do not have meaningful parent spans; process startup is an exception |

> **No spans** are created at Layer 6 for routine operations. `ThreadLoop` iteration cycles are not traced. The only acceptable span at this layer would be a `process.startup` root span covering the time from `libjami::init()` to `REGISTERED` for the first account — and even that is optional.

---

## Process Resource Attributes — OTel Resource Configuration

These attributes are set **once** during `TracerProvider` / `MeterProvider` initialisation and are attached to **every** signal emitted by the daemon. This is the mechanism by which operators can filter metrics/traces/logs by daemon version, instance, or node.

```cpp
// To be called in jamid/main.cpp (or equivalent initialisation entry point)
// BEFORE libjami::init() is called.
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "src/buildinfo.h"   // provides JAMI_VERSION string

namespace resource = opentelemetry::sdk::resource;

static resource::Resource BuildJamiResource() {
    return resource::Resource::Create({
        // Standard semconv resource attributes
        {"service.name",        "jami-daemon"},
        {"service.version",     JAMI_VERSION},          // e.g. "16.0.0"
        {"service.instance.id", generateInstanceId()},  // persistent UUID per install
        {"process.pid",         static_cast<int64_t>(::getpid())},
        {"process.executable.name", std::string("jamid")},

        // Custom jami resource attributes
        {"jami.build.flavor", getBuildFlavor()},         // e.g. "release", "debug", "asan"
        {"jami.platform",     getPlatformString()},      // e.g. "linux", "android", "macos"
        {"jami.features.video",   isVideoEnabled()   ? "1" : "0"},
        {"jami.features.plugins", isPluginsEnabled() ? "1" : "0"},
    });
}
```

**`service.instance.id`** should be a UUID generated once at daemon installation and persisted to the config directory (e.g., `~/.config/jami/instance-id`). It allows correlating metrics from the same daemon instance across restarts.

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use thread IDs or thread names as metric labels | One label value per `ThreadLoop` × all calls = unbounded cardinality |
| Use file paths as metric labels | Unbounded; paths contain user-specific directories |
| Use log message text as a metric label | Every unique message is a new time series |
| Create per-call-ID resource attributes | Resource attributes are process-scoped and immutable; per-call data belongs in span attributes |
| Emit one `ObservableGauge` per active thread | Sum to a single `jami.process.threads.count` gauge instead |

**Approved metric label values at this layer:**
- `jami.platform`: `"linux"`, `"android"`, `"macos"`, `"ios"`, `"windows"` — bounded
- `threadloop.name`: named thread type: `"audio_sender"`, `"video_sender"`, `"audio_receiver"`, `"video_receiver"`, `"video_mixer"` — bounded; NOT a dynamic thread ID

---

## Example C++ Instrumentation Snippet

### Observable Callback for Process Memory Usage

```cpp
// To be called once during OTel initialisation in jamid/main.cpp

#include "opentelemetry/metrics/provider.h"
#include <fstream>
#include <string>

namespace metric_api = opentelemetry::metrics;

/// Reads VmRSS (resident set size) from /proc/self/status on Linux.
/// Returns bytes, or -1 on error.
static int64_t readVmRssBytes() {
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            // Format: "VmRSS:   123456 kB"
            int64_t kB = 0;
            if (std::sscanf(line.c_str(), "VmRSS: %" SCNd64 " kB", &kB) == 1)
                return kB * 1024LL;
        }
    }
#endif
    return -1LL;
}

static int64_t readOpenFds() {
#if defined(__linux__)
    // Count entries in /proc/self/fd
    int count = 0;
    for (auto& p : std::filesystem::directory_iterator("/proc/self/fd"))
        ++count;
    return static_cast<int64_t>(count);
#else
    return -1LL;
#endif
}

static int64_t readThreadCount() {
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            int64_t n = 0;
            if (std::sscanf(line.c_str(), "Threads: %" SCNd64, &n) == 1)
                return n;
        }
    }
#endif
    return -1LL;
}

void RegisterProcessMetrics()
{
    auto meter = metric_api::Provider::GetMeterProvider()
                     ->GetMeter("jami.system", "1.0.0");

    // ── Memory ────────────────────────────────────────────────────────────────
    auto memGauge = meter->CreateInt64ObservableGauge(
        "process.memory.usage",
        "Resident set size of the jami-daemon process",
        "By");

    memGauge->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void* /*state*/) {
            int64_t rss = readVmRssBytes();
            if (rss >= 0) result.Observe(rss);
        }, nullptr);

    // ── File descriptors ──────────────────────────────────────────────────────
    auto fdGauge = meter->CreateInt64ObservableGauge(
        "process.open_file_descriptors",
        "Number of open file descriptors",
        "{fd}");

    fdGauge->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void* /*state*/) {
            int64_t n = readOpenFds();
            if (n >= 0) result.Observe(n);
        }, nullptr);

    // ── Thread count ──────────────────────────────────────────────────────────
    auto threadGauge = meter->CreateInt64ObservableGauge(
        "process.thread.count",
        "Total OS threads in the jami-daemon process",
        "{threads}");

    threadGauge->AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void* /*state*/) {
            int64_t n = readThreadCount();
            if (n >= 0) result.Observe(n);
        }, nullptr);

    // Keep gauges alive — store as statics or in a global holder object
    // so their callbacks continue to fire for the process lifetime.
    // (Example: store in a ProcessMetrics singleton.)
}
```

### OTel Logs Bridge in `Logger::Handler`

The most impactful single change at Layer 6 is connecting the existing `Logger` infrastructure to the OTel Logs Bridge API. This forwards `JAMI_WARN` and `JAMI_ERR` calls — already emitted throughout the codebase — to the OTel log pipeline without changing any call site.

```cpp
// src/logger.cpp — in Logger::Handler::dispatch() or equivalent sink method
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/common/timestamp.h"

namespace logs_api  = opentelemetry::logs;
namespace trace_api = opentelemetry::trace;
namespace common    = opentelemetry::common;

void Logger::Handler::dispatchToOtel(const Logger::Msg& msg)
{
    // Map jami severity to OTel severity
    logs_api::Severity sev;
    switch (msg.level) {
        case LOG_DEBUG:   sev = logs_api::Severity::kDebug; break;
        case LOG_INFO:    sev = logs_api::Severity::kInfo;  break;
        case LOG_WARNING: sev = logs_api::Severity::kWarn;  break;
        case LOG_ERR:     sev = logs_api::Severity::kError; break;
        default:          sev = logs_api::Severity::kTrace; break;
    }

    // Forward only WARN and above to avoid flooding during debug sessions.
    // In production, INFO can be included; DEBUG should never be forwarded.
    if (sev < logs_api::Severity::kWarn) return;

    // Acquire logger (cached as static — same Logger returned each call)
    static auto otelLogger = logs_api::Provider::GetLoggerProvider()
                                 ->GetLogger("jami.core", "", "https://jami.net/schema");

    auto record = otelLogger->CreateLogRecord();
    if (!record) return;   // no-op logger — SDK not initialised

    record->SetSeverity(sev);
    record->SetSeverityText(logs_api::SeverityNumToText(sev));
    record->SetBody(std::string(msg.message));
    record->SetTimestamp(common::SystemTimestamp(std::chrono::system_clock::now()));

    // Structured attributes from the log call site
    record->SetAttribute("code.filepath",  std::string(msg.file));
    record->SetAttribute("code.lineno",    static_cast<int64_t>(msg.line));

    // Inject active span context for trace correlation
    auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto span = trace_api::GetSpan(ctx);
    if (span && span->GetContext().IsValid()) {
        auto sc = span->GetContext();
        record->SetTraceId(sc.trace_id());
        record->SetSpanId(sc.span_id());
        record->SetTraceFlags(sc.trace_flags());
    }

    otelLogger->EmitLogRecord(std::move(record));
}
```

**Integration point**: call `dispatchToOtel(msg)` at the end of `Logger::Handler`'s existing dispatch method, after the platform sink (logcat / syslog) has been written, so OTel emission never blocks the platform sink.

---

## ThreadLoop Health Counter

`ThreadLoop` is used by every media session. Counting restarts (i.e., the number of times `setup()` is called after the first) provides a leading indicator of media subsystem instability.

```cpp
// src/threadloop.cpp — in ThreadLoop::start() / setup phase
#include "opentelemetry/metrics/provider.h"

void ThreadLoop::start()
{
    static auto restartCounter = opentelemetry::metrics::Provider
        ::GetMeterProvider()
        ->GetMeter("jami.system", "1.0.0")
        ->CreateUInt64Counter(
            "jami.threadloop.restarts",
            "Number of ThreadLoop setup() invocations (first start + restarts)",
            "{restarts}");

    // threadName_ is set at construction (e.g., "audio_sender", "video_sender")
    // It is a bounded set — safe as a label.
    std::map<std::string, std::string> attrs = {
        {"threadloop.name", threadName_},
    };
    restartCounter->Add(1,
        opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});

    // ... existing start logic ...
}
```

---

## Subsystems in This Layer

| Subsystem | Relationship |
|---|---|
| **logging** | `Logger` and `Logger::Handler` are the primary OTel integration points at this layer; the Logs Bridge is inserted here |
| **config_persistence** | `Archiver`, `Preferences`, YAML I/O belong here; no tracing needed; slow startup load would appear in process startup span if one is created |
| **media_pipeline** | `ThreadLoop` is the threading substrate for all audio/video encode/decode threads; `ThreadLoop` health metrics are emitted here |
| **account_management** | `Account::setRegistrationState()` runs on the ASIO `io_context` owned by `Manager`; that io_context is part of this layer |
| **plugin_system** | Plugin load/unload executes dlopen/FreeLibrary on the io_context thread; a `jami.system.dlopen` event or log is the appropriate signal |
| **call_manager** | Ring timeout timer (`asio::steady_timer` in `Call`) is a timer infrastructure element at this layer |

---

## Source References

- `src/threadloop.h` / `src/threadloop.cpp`
- `src/logger.h` / `src/logger.cpp`
- `src/manager.h` / `src/manager.cpp`
- `src/fileutils.h` / `src/fileutils.cpp`
- `src/archiver.h` / `src/archiver.cpp`
- `src/preferences.h` / `src/preferences.cpp`
- `src/winsyslog.h` / `src/winsyslog.c`
- `src/jami/jami.h` — `libjami::init()` / `libjami::fini()` are the process lifecycle hooks
- KB: `subsystem_logging.md` — Logger internals, macro API, platform sinks
- KB: `subsystem_config_persistence.md` — YAML config; startup file I/O
- KB: `build_system.md` — `service.version` value comes from `project(jami-core VERSION 16.0.0)`
- KB: `otel_cpp_sdk.md` — Resource configuration pattern; provider initialisation
- KB: `otel_logs.md` — LogRecord structure; severity mapping; `SetTraceId`/`SetSpanId` injection
- KB: `otel_metrics.md` — ObservableGauge registration; callback lifetime

---

## Open Questions

1. **`service.instance.id` persistence**: the instance ID UUID should survive daemon restarts so metrics from the same installation are continuous time series. Where should it be stored — `~/.config/jami/instance-id` (XDG)? Confirm this path is created by `fileutils::get_data_dir()` and accessible before `Logger::init()`.
2. **ASIO io_context thread count**: `Manager` creates an io_context with a thread pool. The pool size is configurable. Should `jami.system.asio.threads` be an `ObservableGauge` reporting the pool size, or is it better as a Resource attribute (static after init)?
3. **Android-specific metrics**: on Android, the process memory API (`/proc/self/status`) is available but logcat is the primary sink. Should the OTel Logs Bridge be disabled on Android (where logcat already forwards to Google's infrastructure), or configured to route to a local OTLP endpoint?
4. **OTel SDK initialisation before `libjami::init()`**: the process resource attributes must be set before any subsystem starts emitting. This requires the calling application (e.g., `jamid`, Qt app) to initialise the `TracerProvider`, `MeterProvider`, and `LoggerProvider` before calling `libjami::init()`. Should `libjami` provide a `libjami::setOtelProviders(tracer, meter, logger)` hook to make this ordering explicit, or rely on the global OTel provider install pattern?
5. **Log rate limiting**: the `Logger::Handler` already processes every DEBUG log from media threads. Even with the `sev < kWarn` guard in the Logs Bridge, a flood of `JAMI_WARN` calls from `AudioLayer` (e.g., buffer underrun) could saturate the OTel batch log processor. A token-bucket rate limiter or per-subsystem sampling should be considered for the bridge.
