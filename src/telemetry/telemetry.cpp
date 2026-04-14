/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include "telemetry.h"
#include "logger.h"

// ── OTel SDK headers (only compiled into this TU) ──────────────────────────
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/provider.h>

// Chain 1 — always active: ring buffer span exporter
#include "ring_buffer_span_exporter.h"

// OTel Logs SDK — LoggerProvider + simple processor
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/recordable.h>
#include <opentelemetry/sdk/logs/read_write_log_record.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/noop.h>

namespace {
/**
 * Minimal no-op LogRecordExporter for the Logs bridge POC.
 *
 * The actual log messages are already routed to console / syslog / file
 * by the existing Logger infrastructure.  This exporter simply accepts
 * records so the OTel Logs SDK pipeline is functional and LogRecords
 * can carry trace context for future correlation features.
 *
 * Replace this with OtlpHttpLogRecordExporter or a ring-buffer exporter
 * when production log aggregation is needed.
 */
class NoopLogRecordExporter final : public opentelemetry::sdk::logs::LogRecordExporter
{
public:
    std::unique_ptr<opentelemetry::sdk::logs::Recordable>
    MakeRecordable() noexcept override
    {
        return std::make_unique<opentelemetry::sdk::logs::ReadWriteLogRecord>();
    }

    opentelemetry::sdk::common::ExportResult
    Export(const opentelemetry::nostd::span<
               std::unique_ptr<opentelemetry::sdk::logs::Recordable>>&) noexcept override
    {
        return opentelemetry::sdk::common::ExportResult::kSuccess;
    }

    bool ForceFlush(std::chrono::microseconds) noexcept override { return true; }
    bool Shutdown(std::chrono::microseconds) noexcept override { return true; }
};
} // anonymous namespace

// Chain 2 — OTLP HTTP, compiled only when the export flag is set
#ifdef JAMI_OTEL_EXPORT_ENABLED
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include "retry_span_exporter.h"
#endif

#include <cstdlib>
#include <mutex>

namespace trace_api   = opentelemetry::trace;
namespace trace_sdk   = opentelemetry::sdk::trace;
namespace resource    = opentelemetry::sdk::resource;
namespace logs_api    = opentelemetry::logs;
namespace logs_sdk    = opentelemetry::sdk::logs;

// ── Module-private state ───────────────────────────────────────────────────

namespace {

/// Guard for one-time init / shutdown.
std::mutex g_telemetryMutex;

/// Handle to the ring buffer exporter (for drainSpans / snapshotSpans / export).
jami::telemetry::RingBufferSpanExporter* g_ringBuffer {nullptr};

/// Keep a raw pointer to the SDK provider so we can call ForceFlush/Shutdown.
trace_sdk::TracerProvider* g_sdkProvider {nullptr};

/// Keep a raw pointer to the Logs SDK provider for shutdown.
logs_sdk::LoggerProvider* g_logProvider {nullptr};

bool g_initialized {false};

#ifdef JAMI_OTEL_EXPORT_ENABLED
/// Raw pointer to the retry exporter so scheduleFlush() can wake it.
jami::telemetry::RetryingSpanExporter* g_retryExporter {nullptr};
#endif

} // anonymous namespace

namespace jami {
namespace telemetry {

void
initTelemetry(const std::string& serviceName, const std::string& version)
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_initialized) {
        JAMI_WARNING("[otel] initTelemetry called more than once — ignoring");
        return;
    }

    try {
        // ── Resource ───────────────────────────────────────────────────
        auto resourceAttrs = resource::Resource::Create({
            {"service.name",           serviceName},
            {"service.version",        version},
            {"telemetry.sdk.language", std::string("cpp")},
        });

        // ── Processor vector ───────────────────────────────────────────
        std::vector<std::unique_ptr<trace_sdk::SpanProcessor>> processors;

        // Chain 1: RingBuffer + Simple  (always active, ~5 MB capacity)
        {
            auto exporter = std::make_unique<RingBufferSpanExporter>();
            g_ringBuffer  = exporter.get();
            auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
            processors.push_back(std::move(processor));
        }

        // Chain 2: OTLP HTTP + Batch  (only when export is enabled)
#ifdef JAMI_OTEL_EXPORT_ENABLED
        {
            namespace otlp = opentelemetry::exporter::otlp;

            otlp::OtlpHttpExporterOptions opts;
            // Default endpoint — same IP as JamiApplicationFirebase so the
            // daemon does not rely on `adb reverse tcp:4318 tcp:4318`.
            // Override at runtime by setting OTEL_EXPORTER_OTLP_ENDPOINT.
            opts.url = "http://192.168.49.117:4318/v1/traces";
            const char* envEndpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
            if (envEndpoint && envEndpoint[0] != '\0') {
                std::string ep(envEndpoint);
                // Ensure the traces path is appended if not already present.
                if (ep.find("/v1/traces") == std::string::npos)
                    ep += "/v1/traces";
                opts.url = ep;
            }
            // set console_debug = true to print raw HTTP status to logcat

            auto otlpExporter = otlp::OtlpHttpExporterFactory::Create(opts);
            // Wrap with retry logic so spans buffered during network outages
            // are re-attempted every ~15 s instead of being silently dropped.
            // Pass the endpoint URL so the exporter can TCP-probe reachability.
            auto retryPtr = std::make_unique<RetryingSpanExporter>(
                std::move(otlpExporter), opts.url);
            g_retryExporter = retryPtr.get();
            auto exporter   = std::move(retryPtr);

            trace_sdk::BatchSpanProcessorOptions bspOpts;
            bspOpts.max_queue_size        = 2048;
            bspOpts.max_export_batch_size = 512;
            bspOpts.schedule_delay_millis = std::chrono::milliseconds(5000);

            auto processor = trace_sdk::BatchSpanProcessorFactory::Create(
                std::move(exporter), bspOpts);
            processors.push_back(std::move(processor));

            JAMI_LOG("[otel] OTLP HTTP exporter active → {}", opts.url);
        }
#endif

        // ── TracerProvider with multiple processors ────────────────────
        auto provider = std::make_unique<trace_sdk::TracerProvider>(
            std::move(processors), resourceAttrs);

        g_sdkProvider = provider.get();

        // Set as the global provider (API layer).
        trace_api::Provider::SetTracerProvider(
            opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(provider.release()));

        g_initialized = true;
        JAMI_LOG("[otel] Telemetry initialized  service={} version={}", serviceName, version);

        // Emit a short startup span so the service appears in Jaeger
        // immediately, even before any calls are made.
        auto tracer = trace_api::Provider::GetTracerProvider()->GetTracer(
            "jami.daemon", version);
        auto startupSpan = tracer->StartSpan("daemon.startup");
        startupSpan->SetAttribute("service.version", version);
        startupSpan->End();

        // ── LoggerProvider (Logs signal) ───────────────────────────────
        // Set up the OTel Logs pipeline.  For this POC, a no-op exporter
        // is used — log messages are already written by the existing sinks.
        // The value is that every OTel LogRecord carries the active span's
        // trace/span IDs, enabling log-trace correlation.  Replace the
        // exporter with OtlpHttpLogRecordExporter for production use.
        {
            auto logExporter = std::make_unique<NoopLogRecordExporter>();
            auto logProcessor = logs_sdk::SimpleLogRecordProcessorFactory::Create(
                std::move(logExporter));

            auto logProvider = logs_sdk::LoggerProviderFactory::Create(
                std::move(logProcessor), resourceAttrs);

            g_logProvider = static_cast<logs_sdk::LoggerProvider*>(logProvider.get());

            logs_api::Provider::SetLoggerProvider(
                opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(logProvider.release()));

            // Enable the OTel log handler in the jami Logger so that all
            // JAMI_LOG / JAMI_WARNING / etc. messages are also emitted as
            // OTel LogRecords.
            Logger::setOTelLog(true);
        }

    } catch (const std::exception& e) {
        JAMI_ERROR("[otel] Failed to initialize telemetry: {}", e.what());
    } catch (...) {
        JAMI_ERROR("[otel] Failed to initialize telemetry (unknown error)");
    }
}

void
shutdownTelemetry()
{
    std::lock_guard lk {g_telemetryMutex};
    if (!g_initialized)
        return;

    try {
        // Disable the OTel log handler first to stop new log records.
        Logger::setOTelLog(false);

        if (g_sdkProvider) {
            // Best-effort flush before shutdown.
            g_sdkProvider->ForceFlush();
            g_sdkProvider->Shutdown();
        }
        if (g_logProvider) {
            g_logProvider->Shutdown();
        }
    } catch (const std::exception& e) {
        JAMI_WARNING("[otel] Exception during telemetry shutdown: {}", e.what());
    } catch (...) {
        JAMI_WARNING("[otel] Unknown exception during telemetry shutdown");
    }

    // Replace the global providers with no-ops so subsequent GetTracer()/
    // GetLogger() calls succeed silently.
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
            new trace_api::NoopTracerProvider));

    logs_api::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(
            new logs_api::NoopLoggerProvider));

    g_sdkProvider  = nullptr;
    g_logProvider  = nullptr;
    g_ringBuffer   = nullptr;
    g_initialized  = false;
#ifdef JAMI_OTEL_EXPORT_ENABLED
    g_retryExporter = nullptr;
#endif
    JAMI_LOG("[otel] Telemetry shut down");
}

std::vector<std::unique_ptr<trace_sdk::SpanData>>
drainSpans()
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_ringBuffer)
        return g_ringBuffer->drain();
    return {};
}

std::vector<std::unique_ptr<trace_sdk::SpanData>>
snapshotSpans()
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_ringBuffer)
        return g_ringBuffer->snapshot();
    return {};
}

bool
exportSpansToFile(const std::string& path)
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_ringBuffer)
        return g_ringBuffer->exportToFile(path);
    return false;
}

std::size_t
spanCount()
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_ringBuffer)
        return g_ringBuffer->size();
    return 0;
}

void
scheduleFlush() noexcept
{
#ifdef JAMI_OTEL_EXPORT_ENABLED
    std::lock_guard lk {g_telemetryMutex};
    if (g_retryExporter)
        g_retryExporter->notifyNetworkAvailable();
#endif
}

} // namespace telemetry
} // namespace jami
