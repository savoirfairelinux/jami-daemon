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
 * @file ring_buffer_span_exporter.h
 * @brief A SpanExporter that keeps a bounded ring buffer of completed spans
 *        in memory (~5 MB).  Oldest spans are evicted when the capacity is
 *        exceeded.  The buffer supports both destructive drain (for tests)
 *        and non-destructive snapshot + JSON serialization for local export.
 */

#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/sdk/common/exporter_utils.h>
#include <opentelemetry/nostd/span.h>

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace jami {
namespace telemetry {

class RingBufferSpanExporter : public opentelemetry::sdk::trace::SpanExporter
{
public:
    /**
     * @param maxSpans  Maximum number of spans to hold.  An average
     *                  SpanData is ~600-800 bytes; 8192 spans ≈ 5 MB.
     */
    static constexpr std::size_t kDefaultMaxSpans = 8192;

    explicit RingBufferSpanExporter(std::size_t maxSpans = kDefaultMaxSpans);
    ~RingBufferSpanExporter() override = default;

    // ── SpanExporter interface ─────────────────────────────────────────

    std::unique_ptr<opentelemetry::sdk::trace::Recordable>
    MakeRecordable() noexcept override;

    opentelemetry::sdk::common::ExportResult
    Export(const opentelemetry::nostd::span<
               std::unique_ptr<opentelemetry::sdk::trace::Recordable>>& spans) noexcept override;

    bool ForceFlush(
        std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

    bool Shutdown(
        std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

    // ── Ring-buffer access ─────────────────────────────────────────────

    /**
     * Destructive drain — returns all buffered spans and clears the buffer.
     * Intended for unit tests.
     */
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> drain();

    /**
     * Non-destructive snapshot — returns copies of all buffered spans.
     * The buffer is not modified.
     */
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> snapshot() const;

    /**
     * Serialize the current buffer contents to a JSON string.
     * Each span becomes a JSON object with name, traceId, spanId,
     * parentSpanId, start/end timestamps, status, attributes, and events.
     */
    std::string toJson() const;

    /**
     * Export the current buffer contents to a JSON file at @p path.
     * Creates or overwrites the file.  Returns true on success.
     */
    bool exportToFile(const std::string& path) const;

    /**
     * Return the number of spans currently in the buffer.
     */
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::size_t maxSpans_;
    std::deque<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> buffer_;
    bool shutdown_ {false};
};

} // namespace telemetry
} // namespace jami
