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

/**
 * @file telemetry.h
 * @brief OpenTelemetry bootstrap layer for jami-daemon.
 *
 * Public interface uses only the OTel API types (no SDK headers leak here).
 * Include this header from any translation unit that needs to check whether
 * telemetry is initialized or to obtain the global TracerProvider.
 */

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/trace/span_data.h>

#include <memory>
#include <string>
#include <vector>

namespace jami {
namespace telemetry {

/**
 * Initialize the global OpenTelemetry TracerProvider.
 *
 * Sets up two processor chains:
 *   1. InMemorySpanExporter + SimpleSpanProcessor  (always active)
 *   2. OtlpHttpExporter + BatchSpanProcessor       (only when JAMI_OTEL_EXPORT_ENABLED)
 *
 * @param serviceName   Value for the "service.name" resource attribute.
 * @param version       Value for the "service.version" resource attribute.
 */
void initTelemetry(const std::string& serviceName, const std::string& version);

/**
 * Flush and shut down the global TracerProvider, then replace it with a
 * NoopTracerProvider.  Safe to call even if initTelemetry() was never called.
 */
void shutdownTelemetry();

/**
 * Drain all spans captured by the in-memory exporter since the last drain.
 *
 * This is a destructive read — the internal buffer is cleared.  Useful for
 * unit tests that want to assert span names/attributes.
 *
 * @return Vector of completed SpanData snapshots, or empty if telemetry
 *         has not been initialized.
 */
std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> drainSpans();

} // namespace telemetry
} // namespace jami
