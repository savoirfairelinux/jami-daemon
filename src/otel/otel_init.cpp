// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.

#include "otel_init.h"

// ─────────────────────────────────────────────────────────────────────────────
// Full implementation only when ENABLE_OTEL is set.
// When the macro is absent every public symbol becomes a trivial stub so that
// the rest of the daemon compiles and links without any OTel dependency.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef ENABLE_OTEL

// ── Standard library ─────────────────────────────────────────────────────────
#include <cstdio>
#include <mutex>
#include <stdexcept>

// ── OTel API ─────────────────────────────────────────────────────────────────
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/trace/provider.h>

// ── OTel SDK — resource ───────────────────────────────────────────────────────
#include <opentelemetry/sdk/resource/resource.h>

// ── OTel SDK — traces ─────────────────────────────────────────────────────────
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>

// ── OTel SDK — metrics ────────────────────────────────────────────────────────
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>

// ── OTel SDK — logs ───────────────────────────────────────────────────────────
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>

// ── ostream (stdout) exporters ────────────────────────────────────────────────
#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>

// ── OTLP gRPC exporters (compiled only when WITH_OTLP_GRPC=ON) ───────────────
#ifdef WITH_OTLP_GRPC
#  include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#endif

// ── OTLP HTTP exporters (compiled only when WITH_OTLP_HTTP=ON) ────────────────
#ifdef WITH_OTLP_HTTP
#  include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#  include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#  include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#  include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#endif

// ── SDK cast helpers ─────────────────────────────────────────────────────────
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>

// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Namespace aliases — local to this translation unit only.
namespace trace_api  = opentelemetry::trace;
namespace trace_sdk  = opentelemetry::sdk::trace;
namespace metric_api = opentelemetry::metrics;
namespace metric_sdk = opentelemetry::sdk::metrics;
namespace logs_api   = opentelemetry::logs;
namespace logs_sdk   = opentelemetry::sdk::logs;
namespace resource   = opentelemetry::sdk::resource;

namespace trace_exp  = opentelemetry::exporter::trace;
namespace metric_exp = opentelemetry::exporter::metrics;
namespace logs_exp   = opentelemetry::exporter::logs;

// Guard: only the first call to initOtel() actually does work.
std::once_flag g_otelInitFlag;
bool           g_otelInitialized = false;

// ── OTLP HTTP URL helper ──────────────────────────────────────────────────────
// The OtlpHttp*ExporterOptions default constructors set the full URL including
// the signal-specific path (e.g. http://localhost:4318/v1/traces).  When the
// user supplies an endpoint we must make sure the signal path is present;
// otherwise the OTLP receiver will return 404.
#ifdef WITH_OTLP_HTTP
std::string
otlpHttpUrl(const std::string& base, const char* signalPath)
{
    // If the user already included the path, respect it as‑is.
    if (base.find(signalPath) != std::string::npos)
        return base;

    // Strip trailing slash(es) then append the signal path.
    std::string url = base;
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    return url + signalPath;
}
#endif

// ── Resource helper ───────────────────────────────────────────────────────────

resource::Resource
makeResource(const jami::otel::OtelConfig& cfg)
{
    return resource::Resource::Create({
        {"service.name",    cfg.service_name},
        {"service.version", cfg.service_version},
        {"process.pid",     static_cast<int64_t>(
#ifdef _WIN32
            static_cast<int64_t>(GetCurrentProcessId())
#else
            static_cast<int64_t>(::getpid())
#endif
        )},
    });
}

// ── TracerProvider setup ──────────────────────────────────────────────────────

void
initTracerProvider(const jami::otel::OtelConfig& cfg,
                   const resource::Resource&     res)
{
    using ET = jami::otel::OtelConfig::ExporterType;

    if (!cfg.enable_traces || cfg.trace_exporter == ET::None)
        return;

    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter;

    switch (cfg.trace_exporter) {
    case ET::Stdout:
        exporter = trace_exp::OStreamSpanExporterFactory::Create();
        break;

#ifdef WITH_OTLP_GRPC
    case ET::OtlpGrpc: {
        opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
        opts.endpoint            = cfg.otlp_endpoint;
        opts.use_ssl_credentials = false;
        exporter = opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);
        break;
    }
#endif

#ifdef WITH_OTLP_HTTP
    case ET::OtlpHttp: {
        opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
        opts.url = otlpHttpUrl(cfg.otlp_endpoint, "/v1/traces");
        exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(opts);
        break;
    }
#endif

    default:
        // Requested exporter not compiled in — fall back to stdout.
        exporter = trace_exp::OStreamSpanExporterFactory::Create();
        break;
    }

    trace_sdk::BatchSpanProcessorOptions bsp_opts;
    bsp_opts.max_queue_size        = 4096;
    bsp_opts.max_export_batch_size = 512;
    auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), bsp_opts);

    std::shared_ptr<trace_api::TracerProvider> provider =
        trace_sdk::TracerProviderFactory::Create(std::move(processor), res);
    trace_api::Provider::SetTracerProvider(std::move(provider));
}

// ── MeterProvider setup ───────────────────────────────────────────────────────

void
initMeterProvider(const jami::otel::OtelConfig& cfg,
                  const resource::Resource&     res)
{
    using ET = jami::otel::OtelConfig::ExporterType;

    if (!cfg.enable_metrics || cfg.metrics_exporter == ET::None)
        return;

    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter;

    switch (cfg.metrics_exporter) {
    case ET::Stdout:
        exporter = metric_exp::OStreamMetricExporterFactory::Create();
        break;

#ifdef WITH_OTLP_GRPC
    case ET::OtlpGrpc: {
        opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions opts;
        opts.endpoint = cfg.otlp_endpoint;
        exporter = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(opts);
        break;
    }
#endif

#ifdef WITH_OTLP_HTTP
    case ET::OtlpHttp: {
        opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions opts;
        opts.url = otlpHttpUrl(cfg.otlp_endpoint, "/v1/metrics");
        exporter = opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(opts);
        break;
    }
#endif

    default:
        exporter = metric_exp::OStreamMetricExporterFactory::Create();
        break;
    }

    metric_sdk::PeriodicExportingMetricReaderOptions reader_opts;
    reader_opts.export_interval_millis = cfg.metrics_export_interval;
    reader_opts.export_timeout_millis  = std::chrono::milliseconds(
        std::min<long long>(cfg.metrics_export_interval.count() / 2, 10000LL));

    auto reader = metric_sdk::PeriodicExportingMetricReaderFactory::Create(
        std::move(exporter), reader_opts);

    auto u_provider = metric_sdk::MeterProviderFactory::Create(
        std::make_unique<metric_sdk::ViewRegistry>(), res);
    static_cast<metric_sdk::MeterProvider*>(u_provider.get())
        ->AddMetricReader(std::move(reader));

    metric_api::Provider::SetMeterProvider(
        std::shared_ptr<metric_api::MeterProvider>(std::move(u_provider)));
}

// ── LoggerProvider setup ──────────────────────────────────────────────────────

void
initLoggerProvider(const jami::otel::OtelConfig& cfg,
                   const resource::Resource&     res)
{
    using ET = jami::otel::OtelConfig::ExporterType;

    if (!cfg.enable_logs || cfg.logs_exporter == ET::None)
        return;

    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter;

    switch (cfg.logs_exporter) {
    case ET::Stdout:
        exporter = logs_exp::OStreamLogRecordExporterFactory::Create();
        break;

#ifdef WITH_OTLP_GRPC
    case ET::OtlpGrpc: {
        opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions opts;
        opts.endpoint = cfg.otlp_endpoint;
        exporter = opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::Create(opts);
        break;
    }
#endif

#ifdef WITH_OTLP_HTTP
    case ET::OtlpHttp: {
        opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions opts;
        opts.url = otlpHttpUrl(cfg.otlp_endpoint, "/v1/logs");
        exporter = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(opts);
        break;
    }
#endif

    default:
        exporter = logs_exp::OStreamLogRecordExporterFactory::Create();
        break;
    }

    opentelemetry::sdk::logs::BatchLogRecordProcessorOptions batch_opts;
    auto processor = logs_sdk::BatchLogRecordProcessorFactory::Create(
        std::move(exporter), batch_opts);

    auto provider = logs_sdk::LoggerProviderFactory::Create(std::move(processor), res);
    auto shared_provider = std::shared_ptr<logs_api::LoggerProvider>(std::move(provider));
    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(shared_provider));
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

namespace jami {
namespace otel {

bool
initOtel(const OtelConfig& config)
{
    bool success = true;

    std::call_once(g_otelInitFlag, [&]() {
        try {
            auto res = makeResource(config);
            initTracerProvider(config, res);
            initMeterProvider(config, res);
            initLoggerProvider(config, res);
            g_otelInitialized = true;
        } catch (const std::exception& ex) {
            std::fprintf(stderr,
                         "[jami-otel] OTel initialization failed: %s — telemetry disabled.\n",
                         ex.what());
            success = false;
        } catch (...) {
            std::fprintf(stderr,
                         "[jami-otel] OTel initialization failed with unknown error"
                         " — telemetry disabled.\n");
            success = false;
        }
    });

    return success;
}

void
shutdownOtel()
{
    if (!g_otelInitialized)
        return;

    // ── Traces ────────────────────────────────────────────────────────────────
    {
        auto tp = opentelemetry::trace::Provider::GetTracerProvider();
        if (auto* sdk_tp = dynamic_cast<opentelemetry::sdk::trace::TracerProvider*>(tp.get())) {
            sdk_tp->ForceFlush();
            sdk_tp->Shutdown();
        }
        opentelemetry::trace::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> {});
    }

    // ── Metrics ───────────────────────────────────────────────────────────────
    {
        auto mp = opentelemetry::metrics::Provider::GetMeterProvider();
        if (auto* sdk_mp =
                dynamic_cast<opentelemetry::sdk::metrics::MeterProvider*>(mp.get())) {
            sdk_mp->ForceFlush();
            sdk_mp->Shutdown();
        }
        opentelemetry::metrics::Provider::SetMeterProvider(
            opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> {});
    }

    // ── Logs ──────────────────────────────────────────────────────────────────
    {
        auto lp = opentelemetry::logs::Provider::GetLoggerProvider();
        if (auto* sdk_lp =
                dynamic_cast<opentelemetry::sdk::logs::LoggerProvider*>(lp.get())) {
            sdk_lp->ForceFlush();
            sdk_lp->Shutdown();
        }
        opentelemetry::logs::Provider::SetLoggerProvider(
            opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider> {});
    }

    g_otelInitialized = false;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
getTracer(std::string_view name)
{
    return opentelemetry::trace::Provider::GetTracerProvider()
        ->GetTracer(std::string(name));
}

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>
getMeter(std::string_view name)
{
    return opentelemetry::metrics::Provider::GetMeterProvider()
        ->GetMeter(std::string(name));
}

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>
getOtelLogger(std::string_view name)
{
    return opentelemetry::logs::Provider::GetLoggerProvider()
        ->GetLogger(std::string(name));
}

} // namespace otel
} // namespace jami

// ─────────────────────────────────────────────────────────────────────────────
// Stub implementations — compiled when ENABLE_OTEL is NOT defined
// ─────────────────────────────────────────────────────────────────────────────

#else // !ENABLE_OTEL

namespace jami {
namespace otel {

bool  initOtel(const OtelConfig&) { return true; }
void  shutdownOtel()              {}

} // namespace otel
} // namespace jami

#endif // ENABLE_OTEL
