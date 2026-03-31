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

// Chain 1 — always active
#include <opentelemetry/exporters/memory/in_memory_span_data.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter_factory.h>

// Chain 2 — OTLP HTTP, compiled only when the export flag is set
#ifdef JAMI_OTEL_EXPORT_ENABLED
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#endif

#include <cstdlib>
#include <mutex>

namespace trace_api   = opentelemetry::trace;
namespace trace_sdk   = opentelemetry::sdk::trace;
namespace resource    = opentelemetry::sdk::resource;
namespace memory_exp  = opentelemetry::exporter::memory;

// ── Module-private state ───────────────────────────────────────────────────

namespace {

/// Guard for one-time init / shutdown.
std::mutex g_telemetryMutex;

/// Handle to the in-memory span data (for drainSpans).
std::shared_ptr<memory_exp::InMemorySpanData> g_inMemoryData;

/// Keep a raw pointer to the SDK provider so we can call ForceFlush/Shutdown.
trace_sdk::TracerProvider* g_sdkProvider {nullptr};

bool g_initialized {false};

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

        // Chain 1: InMemory + Simple  (always active)
        {
            auto exporter = memory_exp::InMemorySpanExporterFactory::Create(g_inMemoryData);
            auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
            processors.push_back(std::move(processor));
        }

        // Chain 2: OTLP HTTP + Batch  (only when export is enabled)
#ifdef JAMI_OTEL_EXPORT_ENABLED
        {
            namespace otlp = opentelemetry::exporter::otlp;

            otlp::OtlpHttpExporterOptions opts;
            // Default endpoint; overridden at runtime if env var is set.
            opts.url = "http://localhost:4318/v1/traces";
            const char* envEndpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
            if (envEndpoint && envEndpoint[0] != '\0') {
                std::string ep(envEndpoint);
                // Ensure the traces path is appended if not already present.
                if (ep.find("/v1/traces") == std::string::npos)
                    ep += "/v1/traces";
                opts.url = ep;
            }

            auto exporter = otlp::OtlpHttpExporterFactory::Create(opts);

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
        if (g_sdkProvider) {
            // Best-effort flush before shutdown.
            g_sdkProvider->ForceFlush();
            g_sdkProvider->Shutdown();
        }
    } catch (const std::exception& e) {
        JAMI_WARNING("[otel] Exception during telemetry shutdown: {}", e.what());
    } catch (...) {
        JAMI_WARNING("[otel] Unknown exception during telemetry shutdown");
    }

    // Replace the global provider with a no-op so subsequent GetTracer()
    // calls succeed silently.
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
            new trace_api::NoopTracerProvider));

    g_sdkProvider  = nullptr;
    g_inMemoryData.reset();
    g_initialized  = false;
    JAMI_LOG("[otel] Telemetry shut down");
}

std::vector<std::unique_ptr<trace_sdk::SpanData>>
drainSpans()
{
    std::lock_guard lk {g_telemetryMutex};
    if (g_inMemoryData)
        return g_inMemoryData->GetSpans();
    return {};
}

} // namespace telemetry
} // namespace jami
