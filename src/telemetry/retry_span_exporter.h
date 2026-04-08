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

// This file is only meaningful when the OTLP HTTP exporter is compiled in.
#ifdef JAMI_OTEL_EXPORT_ENABLED

#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/common/exporter_utils.h>
#include <opentelemetry/nostd/span.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace jami {
namespace telemetry {

/**
 * RetryingSpanExporter wraps any SpanExporter and buffers batches that the
 * inner exporter fails to deliver (e.g. when the network is unavailable).
 *
 * A background thread wakes up every kRetryIntervalMs milliseconds and
 * re-attempts export of all buffered batches.  Once a batch succeeds it is
 * discarded; if it fails again it is returned to the tail of the buffer.
 *
 * buffered batches are capped at kMaxBufferedBatches; older excess batches
 * are silently dropped so memory usage stays bounded.
 *
 * Export() always returns kSuccess to the BatchSpanProcessor, preventing the
 * processor from marking spans as dropped on its side.
 *
 * Thread-safety: Export() and the retry loop each serialise their calls to the
 * inner exporter through inner_mutex_, satisfying the OTel spec requirement
 * that Export() must not be called concurrently for the same exporter instance.
 */
/**
 * RetryingSpanExporter wraps any SpanExporter and pre-buffers ALL spans
 * before attempting delivery so that spans generated while the network is
 * unreachable are not silently dropped.
 *
 * Why pre-buffering is necessary
 * ──────────────────────────────
 * OtlpHttpExporter::Export() always returns kSuccess regardless of the
 * HTTP outcome (both sync and async compile modes).  BatchSpanProcessor
 * therefore considers every batch acknowledged and discards it from its
 * own queue.  Without a secondary buffer the spans are gone.
 *
 * Delivery gate
 * ─────────────
 * Before attempting to forward the buffer to the inner exporter the
 * background thread probes the OTLP endpoint with a short non-blocking
 * TCP connect.  If the probe fails (no route, connection refused, …) the
 * buffer is kept intact and the thread sleeps for kRetryIntervalMs before
 * trying again.  Calling notifyNetworkAvailable() wakes the thread
 * immediately so newly restored connectivity is exploited without waiting
 * for the full interval.
 *
 * Thread-safety
 * ─────────────
 * buffer_mutex_ protects span_buffer_.
 * inner_mutex_ serialises calls to inner_->Export() / ForceFlush().
 */
class RetryingSpanExporter : public opentelemetry::sdk::trace::SpanExporter
{
public:
    /// Interval between delivery retries when endpoint is unreachable.
    static constexpr int kRetryIntervalMs = 15'000;
    /// Maximum number of spans held in the pre-buffer at any one time.
    /// Oldest spans are evicted first when the cap is reached.
    static constexpr std::size_t kMaxBufferedSpans = 4096;

    /**
     * @param inner        The actual exporter to forward spans to (e.g.
     *                     OtlpHttpExporter).
     * @param endpoint_url Full OTLP URL used to derive the host/port for the
     *                     TCP reachability probe, e.g.
     *                     "http://192.168.1.1:4318/v1/traces".
     */
    RetryingSpanExporter(
        std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> inner,
        std::string endpoint_url);

    ~RetryingSpanExporter() override;

    // --- SpanExporter interface ---

    std::unique_ptr<opentelemetry::sdk::trace::Recordable>
    MakeRecordable() noexcept override;

    opentelemetry::sdk::common::ExportResult
    Export(const opentelemetry::nostd::span<
               std::unique_ptr<opentelemetry::sdk::trace::Recordable>>& spans) noexcept override;

    bool ForceFlush(
        std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

    bool Shutdown(
        std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

    /**
     * Wake the background retry thread immediately so that buffered spans
     * are flushed as soon as the next TCP probe succeeds.
     *
     * Call this whenever the application learns that network connectivity
     * has been (re-)established.
     */
    void notifyNetworkAvailable() noexcept;

private:
    void retryLoop();
    bool probeTcpEndpoint() const noexcept;
    void exportBuffer() noexcept; ///< must be called with inner_mutex_ held

    using SpanBuffer = std::vector<std::unique_ptr<opentelemetry::sdk::trace::Recordable>>;

    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> inner_;

    /// Parsed from endpoint_url — used for the TCP reachability probe.
    std::string endpoint_host_;
    uint16_t    endpoint_port_ {4318};

    /// Serialises all calls to inner_->Export() / inner_->ForceFlush().
    std::mutex inner_mutex_;

    /// Pre-buffer holding spans waiting for delivery confirmation.
    std::mutex buffer_mutex_;
    std::condition_variable cv_;
    SpanBuffer span_buffer_; ///< guarded by buffer_mutex_

    std::thread retry_thread_;
    std::atomic<bool> shutdown_         {false};
    std::atomic<bool> flush_requested_  {false};
};

} // namespace telemetry
} // namespace jami

#endif // JAMI_OTEL_EXPORT_ENABLED
