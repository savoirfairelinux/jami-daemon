# OpenTelemetry C++ SDK — Overview & Initialization

| Field        | Value                                |
|--------------|--------------------------------------|
| Status       | draft                                |
| Last Updated | 2026-03-13                           |
| SDK Version  | v1.25.0 (latest stable, Feb 7 2026)  |

---

## 1. SDK Architecture: API vs SDK Separation

OpenTelemetry C++ is split into two distinct layers:

| Layer | Role | Dependency |
|-------|------|-----------|
| **API** (`opentelemetry-cpp::api`) | Defines interfaces only (Tracer, Meter, Logger). Zero-cost no-ops when no SDK is present. | Header-only; link in libraries |
| **SDK** (`opentelemetry-cpp::sdk`) | Implements the API interfaces with real processors, exporters, and samplers. | Link only in the application binary |

**Rule of thumb:**
- Library/daemon code that *emits* telemetry **only** depends on the API package.
- The application (e.g., `jamid`) is responsible for pulling in the SDK and wiring up exporters.

This is intentional: it means `libjami` can instrument code without forcing any telemetry framework on callers.

---

## 2. Minimum C++ Standard

The SDK supports **C++14, C++17, and C++20**. C++17 is the recommended minimum for new code (avoids some `nostd::` polyfill pain). CMake target requires at least `CMAKE_CXX_STANDARD 14`.

---

## 3. Provider Initialization Pattern

There are three signal providers. Each follows the same pattern:
1. Create an **exporter** (where data goes).
2. Wrap it in a **processor** (batch or simple).
3. Create a **provider** (optionally with a `Resource`).
4. **Register** the provider globally via `Provider::SetXxxProvider()`.

### 3.1 Resource Configuration (service.name, service.version)

```cpp
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace resource = opentelemetry::sdk::resource;

auto resource_attrs = resource::ResourceAttributes{
    {"service.name",    "jami-daemon"},
    {"service.version", "13.0.0"},
    {"service.instance.id", "node-001"},
    {"process.pid",     static_cast<int64_t>(::getpid())},
};
auto sdk_resource = resource::Resource::Create(resource_attrs);
```

---

## 4. Complete Initialization Example — stdout/debug Exporter

Use this during development. No external dependencies required.

```cpp
// ── Headers ──────────────────────────────────────────────────────────────────
// Traces
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

// Metrics
#include "opentelemetry/exporters/ostream/metrics_exporter_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/metrics/provider.h"

// Logs
#include "opentelemetry/exporters/ostream/log_record_exporter_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/logs/provider.h"

// Resource
#include "opentelemetry/sdk/resource/resource.h"

// ── Namespaces ────────────────────────────────────────────────────────────────
namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exp      = opentelemetry::exporter::trace;

namespace metric_api     = opentelemetry::metrics;
namespace metric_sdk     = opentelemetry::sdk::metrics;
namespace metric_exp     = opentelemetry::exporter::metrics;

namespace logs_api       = opentelemetry::logs;
namespace logs_sdk       = opentelemetry::sdk::logs;
namespace logs_exp       = opentelemetry::exporter::logs;

namespace resource       = opentelemetry::sdk::resource;

// ── Resource (shared by all providers) ───────────────────────────────────────
static auto MakeResource() {
    return resource::Resource::Create({
        {"service.name",    "jami-daemon"},
        {"service.version", "13.0.0"},
    });
}

// ── TracerProvider ────────────────────────────────────────────────────────────
static void InitTracer()
{
    auto exporter  = trace_exp::OStreamSpanExporterFactory::Create();
    auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
    std::shared_ptr<trace_api::TracerProvider> provider =
        trace_sdk::TracerProviderFactory::Create(std::move(processor), MakeResource());
    trace_api::Provider::SetTracerProvider(std::move(provider));
}

// ── MeterProvider ─────────────────────────────────────────────────────────────
static void InitMetrics()
{
    metric_sdk::PeriodicExportingMetricReaderOptions opts;
    opts.export_interval_millis = std::chrono::milliseconds(5000);
    opts.export_timeout_millis  = std::chrono::milliseconds(1000);

    auto exporter = metric_exp::OStreamMetricExporterFactory::Create();
    auto reader   = metric_sdk::PeriodicExportingMetricReaderFactory::Create(
                        std::move(exporter), opts);

    auto u_provider = metric_sdk::MeterProviderFactory::Create();
    auto *mp = static_cast<metric_sdk::MeterProvider *>(u_provider.get());
    mp->AddMetricReader(std::move(reader));
    std::shared_ptr<metric_api::MeterProvider> provider(std::move(u_provider));
    metric_api::Provider::SetMeterProvider(std::move(provider));
}

// ── LoggerProvider ────────────────────────────────────────────────────────────
static void InitLogger()
{
    auto exporter  = logs_exp::OStreamLogRecordExporterFactory::Create();
    auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
    auto provider  = logs_sdk::LoggerProviderFactory::Create(std::move(processor), MakeResource());
    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(std::move(provider)));
}

// ── Entry point ───────────────────────────────────────────────────────────────
void OtelInit()
{
    InitTracer();
    InitMetrics();
    InitLogger();
}
```

---

## 5. Complete Initialization Example — OTLP/gRPC Exporter

Requires building with `-DWITH_OTLP_GRPC=ON`.

```cpp
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"

#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/metrics/provider.h"

#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/logs/provider.h"

#include "opentelemetry/sdk/resource/resource.h"

namespace otlp        = opentelemetry::exporter::otlp;
namespace trace_api   = opentelemetry::trace;
namespace trace_sdk   = opentelemetry::sdk::trace;
namespace metric_api  = opentelemetry::metrics;
namespace metric_sdk  = opentelemetry::sdk::metrics;
namespace logs_api    = opentelemetry::logs;
namespace logs_sdk    = opentelemetry::sdk::logs;
namespace resource    = opentelemetry::sdk::resource;

static auto MakeResource() {
    return resource::Resource::Create({
        {"service.name",    "jami-daemon"},
        {"service.version", "13.0.0"},
    });
}

static void InitTracer(const std::string& endpoint = "localhost:4317")
{
    otlp::OtlpGrpcExporterOptions opts;
    opts.endpoint = endpoint;
    opts.use_ssl_credentials = false;

    auto exporter = otlp::OtlpGrpcExporterFactory::Create(opts);

    trace_sdk::BatchSpanProcessorOptions bsp_opts;
    bsp_opts.max_queue_size    = 4096;
    bsp_opts.max_export_batch_size = 512;
    auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), bsp_opts);

    std::shared_ptr<trace_api::TracerProvider> provider =
        trace_sdk::TracerProviderFactory::Create(std::move(processor), MakeResource());
    trace_api::Provider::SetTracerProvider(std::move(provider));
}

static void InitMetrics(const std::string& endpoint = "localhost:4317")
{
    otlp::OtlpGrpcMetricExporterOptions opts;
    opts.endpoint = endpoint;
    auto exporter = otlp::OtlpGrpcMetricExporterFactory::Create(opts);

    metric_sdk::PeriodicExportingMetricReaderOptions reader_opts;
    reader_opts.export_interval_millis = std::chrono::milliseconds(10000);
    reader_opts.export_timeout_millis  = std::chrono::milliseconds(5000);
    auto reader = metric_sdk::PeriodicExportingMetricReaderFactory::Create(
                      std::move(exporter), reader_opts);

    auto u_provider = metric_sdk::MeterProviderFactory::Create();
    static_cast<metric_sdk::MeterProvider*>(u_provider.get())->AddMetricReader(std::move(reader));
    metric_api::Provider::SetMeterProvider(
        std::shared_ptr<metric_api::MeterProvider>(std::move(u_provider)));
}

static void InitLogger(const std::string& endpoint = "localhost:4317")
{
    otlp::OtlpGrpcLogRecordExporterOptions opts;
    opts.endpoint = endpoint;
    auto exporter  = otlp::OtlpGrpcLogRecordExporterFactory::Create(opts);
    auto processor = logs_sdk::BatchLogRecordProcessorFactory::Create(
                         std::move(exporter), {});
    auto provider  = logs_sdk::LoggerProviderFactory::Create(
                         std::move(processor), MakeResource());
    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(std::move(provider)));
}

void OtelInit(const std::string& collector_endpoint)
{
    InitTracer(collector_endpoint);
    InitMetrics(collector_endpoint);
    InitLogger(collector_endpoint);
}
```

---

## 6. Shutdown / Cleanup Pattern

Proper shutdown flushes in-flight data before the process exits. Always call in reverse order of initialization.

```cpp
void OtelShutdown()
{
    // Traces
    auto tracer_provider = trace_api::Provider::GetTracerProvider();
    if (auto *sdk_tp = dynamic_cast<trace_sdk::TracerProvider*>(tracer_provider.get())) {
        sdk_tp->ForceFlush();
        sdk_tp->Shutdown();
    }
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>{});

    // Metrics
    auto meter_provider = metric_api::Provider::GetMeterProvider();
    if (auto *sdk_mp = dynamic_cast<metric_sdk::MeterProvider*>(meter_provider.get())) {
        sdk_mp->ForceFlush();
        sdk_mp->Shutdown();
    }
    metric_api::Provider::SetMeterProvider(
        opentelemetry::nostd::shared_ptr<metric_api::MeterProvider>{});

    // Logs
    auto logger_provider = logs_api::Provider::GetLoggerProvider();
    if (auto *sdk_lp = dynamic_cast<logs_sdk::LoggerProvider*>(logger_provider.get())) {
        sdk_lp->ForceFlush();
        sdk_lp->Shutdown();
    }
    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>{});
}
```

> **Note**: `ForceFlush()` blocks until all buffered data is exported or a timeout expires. For `jami-daemon`, call `OtelShutdown()` during `Manager::finish()` or in the daemon's signal handler for `SIGTERM`.

---

## 7. Thread Safety Guarantees

- The **API layer** (Tracer, Meter, Logger acquisition) is **thread-safe**. Multiple threads can call `GetTracer()`/`GetMeter()` concurrently.
- **Span/Metric instrument operations** (`SetAttribute`, `AddEvent`, `Add`, `Record`) are thread-safe when operating on distinct span/counter objects.
- **Provider initialization** (`SetTracerProvider`) is **not** guaranteed thread-safe. Perform it once, before any worker threads start.
- The **BatchSpanProcessor** uses an internal queue with its own thread; it is safe to call `OnEnd()` from any thread.

---

## 8. SDK Version

| Property | Value |
|----------|-------|
| Latest stable | **v1.25.0** (released 2026-02-07) |
| Semantic conventions bundled | 1.40.0 |
| Minimum CMake | 3.25 |
| Minimum C++ | C++14 (C++17 recommended) |
| Repository | https://github.com/open-telemetry/opentelemetry-cpp |

---

## Source References

- [OpenTelemetry C++ README](https://github.com/open-telemetry/opentelemetry-cpp)
- [OTel C++ Getting Started](https://opentelemetry.io/docs/languages/cpp/getting-started/)
- [OTel C++ Exporters](https://opentelemetry.io/docs/languages/cpp/exporters/)
- [OTel C++ Instrumentation](https://opentelemetry.io/docs/languages/cpp/instrumentation/)
- [INSTALL.md](https://github.com/open-telemetry/opentelemetry-cpp/blob/main/INSTALL.md)

---

## Open Questions

1. **jami-daemon startup sequence**: Where in `Manager::init()` should `OtelInit()` be called? Before or after loading accounts?
2. **Conditional compile**: Should OTel initialization be guarded by a runtime config flag (`[telemetry] enabled=true`) in `jamid.conf`, or purely a compile-time `WITH_OPENTELEMETRY`?
3. **OTLP endpoint discovery**: Should the collector address be hardcoded to `localhost:4317`, read from environment variable `OTEL_EXPORTER_OTLP_ENDPOINT`, or configurable via daemon preferences?
4. **ForceFlush timeout**: What is an acceptable timeout for `ForceFlush()` during shutdown if the collector is unreachable? (Consider `SIGTERM` time budgets in systemd units.)
5. **SDK version pinning**: Should the project pin to v1.25.0 or track latest? A FetchContent pin with a specific git tag is recommended.
