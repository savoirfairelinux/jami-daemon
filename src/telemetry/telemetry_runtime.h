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
#pragma once

#include "otlp_uploader.h"
#include "telemetry.h"
#include "trace_store.h"

#include <opentelemetry/sdk/trace/tracer_provider.h>

#include <mutex>
#include <string>
#include <string_view>

namespace jami::telemetry::detail {

class TelemetryRuntime
{
public:
    /**
     * @brief TelemetryRuntime Builds the runtime around the shared trace store.
     * @return void
     */
    TelemetryRuntime();

    /**
     * @brief init Installs the tracer provider and starts optional upload helpers.
     * @param serviceName Service name reported in telemetry resources.
     * @param version Service version reported in telemetry resources.
     * @param deviceId Local device identifier attached to spans.
     * @return void
     */
    void init(const std::string& serviceName,
              const std::string& version,
              const std::string& deviceId);
    /**
     * @brief shutdown Flushes the provider, stops uploads, and clears buffered state.
     * @return void
     */
    void shutdown();

    /**
     * @brief isInitialized Reports whether the runtime currently accepts spans.
     * @return true when initialization completed successfully.
     */
    bool isInitialized() const noexcept;

    /**
     * @brief startSpan Creates a new root span using the configured tracer provider.
     * @param name Span name.
     * @param options Span scope and initial attributes.
     * @return Move-only handle that controls the live span.
     */
    SpanHandle startSpan(std::string_view name, const SpanStartOptions& options);
    /**
     * @brief startChildSpan Creates a child span from an existing parent handle.
     * @param parent Parent span handle.
     * @param name Child span name.
     * @param options Span scope and initial attributes.
     * @return Move-only handle that controls the live child span.
     */
    SpanHandle startChildSpan(const SpanHandle& parent,
                              std::string_view name,
                              const SpanStartOptions& options);

    /**
     * @brief recordTrace Records a span that starts and ends immediately.
     * @param name Span name.
     * @param attributes Attributes to attach before closing the span.
     * @return void
     */
    void recordTrace(std::string_view name, const Attributes& attributes);
    /**
     * @brief exportTraces Serializes the current buffer to a local JSON file.
     * @param destinationPath Target file path or empty for the default cache path.
     * @return Exported file path, or an empty string on failure.
     */
    std::string exportTraces(const std::string& destinationPath);

private:
    /**
     * @brief notifySpanBuffered Wakes the uploader after new spans enter the store.
     * @return void
     */
    void notifySpanBuffered();

    mutable std::mutex lifecycleMutex_;
    TraceStore store_;
    OtlpUploader uploader_;
    trace_sdk::TracerProvider* sdkProvider_ {nullptr};
    bool initialized_ {false};
    std::string serviceName_;
    std::string serviceVersion_;
    std::string deviceId_;
};

/**
 * @brief runtime Returns the process-wide telemetry runtime singleton.
 * @return Shared runtime instance used by the public facade.
 */
TelemetryRuntime& runtime();

} // namespace jami::telemetry::detail