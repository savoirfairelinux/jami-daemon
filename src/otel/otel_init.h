// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
#pragma once

#ifdef ENABLE_OTEL

#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>

#endif // ENABLE_OTEL

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

namespace jami {
namespace otel {

/// Configuration for OTel initialization.
struct OtelConfig
{
    std::string service_name    = "jami-daemon";
    std::string service_version = "16.0.0";

    bool enable_traces  = true;
    bool enable_metrics = true;
    bool enable_logs    = true;

    /// Which exporter backend to use for each signal.
    enum class ExporterType {
        Stdout,   ///< Human-readable ostream exporter (dev/debug)
        OtlpGrpc, ///< OTLP over gRPC (production)
        OtlpHttp, ///< OTLP over HTTP (alternative production)
        None,     ///< Disable this signal entirely
    };

    ExporterType trace_exporter   = ExporterType::Stdout;
    ExporterType metrics_exporter = ExporterType::Stdout;
    ExporterType logs_exporter    = ExporterType::Stdout;

    /// OTLP collector endpoint (host:port for gRPC, http://… for HTTP).
    std::string otlp_endpoint = "localhost:4317";

    /// How often the PeriodicExportingMetricReader pushes metrics.
    std::chrono::milliseconds metrics_export_interval {30000};

    /// Build an OtelConfig from environment variables.
    ///
    /// Recognised variables:
    ///   JAMI_OTEL_EXPORTER          — "otlp_http", "otlp_grpc", "stdout", or "none"
    ///                                  (applied to all three signals; default: "stdout")
    ///   OTEL_EXPORTER_OTLP_ENDPOINT — base URL / host:port for OTLP (default: per exporter)
    ///   JAMI_OTEL_METRICS_INTERVAL  — metric export interval in milliseconds (default: 30000)
    ///
    /// Example:
    ///   JAMI_OTEL_EXPORTER=otlp_http OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318 jamid -c
    static OtelConfig fromEnvironment()
    {
        OtelConfig cfg;

        if (const char* exp = std::getenv("JAMI_OTEL_EXPORTER")) {
            std::string val(exp);
            ExporterType et = ExporterType::Stdout;
            if (val == "otlp_http" || val == "otlp-http" || val == "http")
                et = ExporterType::OtlpHttp;
            else if (val == "otlp_grpc" || val == "otlp-grpc" || val == "grpc")
                et = ExporterType::OtlpGrpc;
            else if (val == "none" || val == "off")
                et = ExporterType::None;
            // else keep Stdout
            cfg.trace_exporter   = et;
            cfg.metrics_exporter = et;
            cfg.logs_exporter    = et;
        }

        if (const char* ep = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT"))
            cfg.otlp_endpoint = ep;
        else if (cfg.trace_exporter == ExporterType::OtlpHttp)
            cfg.otlp_endpoint = "http://localhost:4318";

        if (const char* iv = std::getenv("JAMI_OTEL_METRICS_INTERVAL"))
            cfg.metrics_export_interval = std::chrono::milliseconds(std::atoi(iv));

        return cfg;
    }
};

/// Initialize all OTel providers (Tracer, Meter, Logger).
/// Must be called once, before any worker threads are started.
/// Returns false if initialization fails; the daemon continues normally.
/// When ENABLE_OTEL is not defined this is a no-op that always returns true.
bool initOtel(const OtelConfig& config = {});

/// Gracefully flush and shut down all OTel providers.
/// Call at daemon shutdown (e.g. inside Manager::finish()).
/// No-op when ENABLE_OTEL is not defined.
void shutdownOtel();

#ifdef ENABLE_OTEL

/// Return a Tracer for the given instrumentation scope.
/// Returns a no-op tracer if OTel has not been initialized.
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
getTracer(std::string_view name);

/// Return a Meter for the given instrumentation scope.
/// Returns a no-op meter if OTel has not been initialized.
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>
getMeter(std::string_view name);

/// Return an OTel Logger for the given instrumentation scope.
/// Returns a no-op logger if OTel has not been initialized.
opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>
getOtelLogger(std::string_view name);

#endif // ENABLE_OTEL

} // namespace otel
} // namespace jami
