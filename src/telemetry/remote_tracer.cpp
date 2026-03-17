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

/**
 * @file remote_tracer.cpp
 * @brief Provides remoteTracerProvider() — a TracerProvider with
 *        service.name "jami-daemon-remote" so that spans emitted by the
 *        receiving side of a distributed call appear as a distinct service
 *        (different colour) in Jaeger.
 *
 * Kept in its own translation unit so the linker only pulls THIS object
 * from libjami-core.a, not the full telemetry.cpp object (which shares
 * symbol names with the client's telemetry.cpp and would cause ODR
 * violations when both are linked into the jami executable).
 */

#include "logger.h"

#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/trace/provider.h>

#include <opentelemetry/exporters/memory/in_memory_span_data.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter_factory.h>

#ifdef JAMI_OTEL_EXPORT_ENABLED
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#endif

#include <cstdlib>
#include <memory>
#include <mutex>

namespace trace_api  = opentelemetry::trace;
namespace trace_sdk  = opentelemetry::sdk::trace;
namespace resource   = opentelemetry::sdk::resource;
namespace memory_exp = opentelemetry::exporter::memory;

namespace {

std::mutex g_remoteMutex;
std::shared_ptr<trace_sdk::TracerProvider> g_remoteProvider;
std::shared_ptr<memory_exp::InMemorySpanData> g_remoteInMemoryData;

} // anonymous namespace

namespace jami {
namespace telemetry {

opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>
remoteTracerProvider()
{
    std::lock_guard lk{g_remoteMutex};
    if (g_remoteProvider) {
        std::shared_ptr<trace_api::TracerProvider> base = g_remoteProvider;
        return opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(base);
    }

    try {
        auto remoteResource = resource::Resource::Create({
            {"service.name",           std::string("jami-daemon-remote")},
            {"telemetry.sdk.language", std::string("cpp")},
        });

        std::vector<std::unique_ptr<trace_sdk::SpanProcessor>> processors;

        // Chain 1: InMemory (always active)
        {
            auto exporter = memory_exp::InMemorySpanExporterFactory::Create(g_remoteInMemoryData);
            auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
            processors.push_back(std::move(processor));
        }

#ifdef JAMI_OTEL_EXPORT_ENABLED
        {
            namespace otlp = opentelemetry::exporter::otlp;

            otlp::OtlpHttpExporterOptions opts;
            opts.url = "http://localhost:4318/v1/traces";
            const char* envEndpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
            if (envEndpoint && envEndpoint[0] != '\0') {
                std::string ep(envEndpoint);
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
        }
#endif

        g_remoteProvider = std::make_shared<trace_sdk::TracerProvider>(
            std::move(processors), remoteResource);

        JAMI_LOG("[otel] Remote TracerProvider initialized (service=jami-daemon-remote)");

    } catch (const std::exception& e) {
        JAMI_ERROR("[otel] Failed to create remote TracerProvider: {}", e.what());
        return trace_api::Provider::GetTracerProvider();
    }

    return opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
        std::shared_ptr<trace_api::TracerProvider>(g_remoteProvider));
}

} // namespace telemetry
} // namespace jami
