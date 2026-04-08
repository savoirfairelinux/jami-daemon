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

// Only compiled when the OTLP HTTP exporter flag is set.
#ifdef JAMI_OTEL_EXPORT_ENABLED

#include "retry_span_exporter.h"
#include "logger.h"

#include <opentelemetry/sdk/trace/span_data.h>

#include <chrono>
#include <cerrno>
#include <cstring>

// POSIX network headers for the TCP reachability probe.
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

namespace trace_sdk = opentelemetry::sdk::trace;
using ExportResult  = opentelemetry::sdk::common::ExportResult;

namespace jami {
namespace telemetry {

// ── Helpers ────────────────────────────────────────────────────────────────

/// Parse "http[s]://host:port/..." → (host, port).
static void parseOtlpEndpoint(const std::string& url,
                               std::string& host,
                               uint16_t&   port)
{
    host.clear();
    port = 4318;

    // Strip scheme (http:// or https://)
    auto schemEnd = url.find("://");
    if (schemEnd == std::string::npos)
        return;
    std::string rest = url.substr(schemEnd + 3);

    // Strip path component
    auto slash = rest.find('/');
    if (slash != std::string::npos)
        rest = rest.substr(0, slash);

    // Split host:port
    auto colon = rest.rfind(':');
    if (colon != std::string::npos) {
        host = rest.substr(0, colon);
        try   { port = static_cast<uint16_t>(std::stoul(rest.substr(colon + 1))); }
        catch (...) {}
    } else {
        host = rest;
    }
}

// ── Construction / destruction ─────────────────────────────────────────────

RetryingSpanExporter::RetryingSpanExporter(
    std::unique_ptr<trace_sdk::SpanExporter> inner,
    std::string endpoint_url)
    : inner_(std::move(inner))
    , retry_thread_([this] { retryLoop(); })
{
    parseOtlpEndpoint(endpoint_url, endpoint_host_, endpoint_port_);
    JAMI_LOG("[otel] RetryingSpanExporter: probing {}:{} for delivery gate",
             endpoint_host_, endpoint_port_);
}

RetryingSpanExporter::~RetryingSpanExporter()
{
    Shutdown();
}

// ── SpanExporter interface ─────────────────────────────────────────────────

std::unique_ptr<trace_sdk::Recordable>
RetryingSpanExporter::MakeRecordable() noexcept
{
    return inner_->MakeRecordable();
}

ExportResult
RetryingSpanExporter::Export(
    const opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>& spans) noexcept
{
    if (shutdown_.load(std::memory_order_relaxed))
        return ExportResult::kFailure;

    // Pre-buffer ALL spans before attempting any delivery.
    //
    // OtlpHttpExporter::Export() always returns kSuccess regardless of the
    // HTTP outcome (both sync and async compile modes of opentelemetry-cpp
    // 1.19.0 ignore the underlying HTTP result and return kSuccess).
    // BatchSpanProcessor therefore believes every batch was acknowledged and
    // frees its copy — leaving us with no spans to retry.
    //
    // By copying spans here we keep them alive in span_buffer_ until the
    // background thread confirms the OTLP endpoint is reachable (via a
    // two-second non-blocking TCP connect) and actually forwards them.
    {
        std::lock_guard lk(buffer_mutex_);
        for (const auto& r : spans) {
            // Evict the oldest entry if the cap is reached.
            if (span_buffer_.size() >= kMaxBufferedSpans) {
                span_buffer_.erase(span_buffer_.begin());
            }
            const auto* sd = dynamic_cast<const trace_sdk::SpanData*>(r.get());
            if (sd) {
                span_buffer_.push_back(
                    std::unique_ptr<trace_sdk::Recordable>(new trace_sdk::SpanData(*sd)));
            }
        }
    }

    // Wake the retry thread so it re-checks connectivity without waiting for
    // the full kRetryIntervalMs.
    cv_.notify_one();

    return ExportResult::kSuccess;
}

void
RetryingSpanExporter::notifyNetworkAvailable() noexcept
{
    flush_requested_.store(true, std::memory_order_release);
    cv_.notify_one();
}

bool
RetryingSpanExporter::ForceFlush(std::chrono::microseconds timeout) noexcept
{
    // Drain the pre-buffer and forward synchronously (skip TCP probe since
    // the caller explicitly requests a flush).
    SpanBuffer toExport;
    {
        std::lock_guard lk(buffer_mutex_);
        span_buffer_.swap(toExport);
    }

    std::lock_guard lk(inner_mutex_);
    if (!toExport.empty()) {
        auto sp = opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>(
            toExport.data(), toExport.size());
        inner_->Export(sp);
    }
    return inner_->ForceFlush(timeout);
}

bool
RetryingSpanExporter::Shutdown(std::chrono::microseconds /*timeout*/) noexcept
{
    if (shutdown_.exchange(true))
        return true; // Already shut down.

    cv_.notify_all();
    if (retry_thread_.joinable())
        retry_thread_.join();

    // Final best-effort flush regardless of network state.
    SpanBuffer remaining;
    {
        std::lock_guard lk(buffer_mutex_);
        span_buffer_.swap(remaining);
    }

    std::lock_guard lk(inner_mutex_);
    if (!remaining.empty()) {
        JAMI_LOG("[otel] RetryingSpanExporter: shutdown flush of {} buffered span(s)",
                 remaining.size());
        auto sp = opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>(
            remaining.data(), remaining.size());
        inner_->Export(sp);
    }
    return inner_->Shutdown();
}

// ── TCP reachability probe ─────────────────────────────────────────────────

bool
RetryingSpanExporter::probeTcpEndpoint() const noexcept
{
    if (endpoint_host_.empty())
        return false;

    // Resolve the host.
    struct addrinfo hints {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[8];
    std::snprintf(portStr, sizeof(portStr), "%u",
                  static_cast<unsigned>(endpoint_port_));

    struct addrinfo* res = nullptr;
    if (::getaddrinfo(endpoint_host_.c_str(), portStr, &hints, &res) != 0 || !res)
        return false;

    // Create a non-blocking socket.
    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        ::freeaddrinfo(res);
        return false;
    }

    // Set non-blocking.
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int r = ::connect(fd, res->ai_addr, res->ai_addrlen);
    ::freeaddrinfo(res);

    bool reachable = false;
    if (r == 0) {
        // Immediate connect (loopback / already-connected).
        reachable = true;
    } else if (errno == EINPROGRESS) {
        // Wait up to 2 seconds for the connection to complete.
        struct timeval tv {2, 0};
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        int sel = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
        if (sel > 0) {
            int so_error = 0;
            socklen_t len = sizeof(so_error);
            ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
            reachable = (so_error == 0);
        }
    }

    ::close(fd);
    return reachable;
}

// ── Retry background thread ────────────────────────────────────────────────

void
RetryingSpanExporter::exportBuffer() noexcept
{
    // caller must hold inner_mutex_
    SpanBuffer toExport;
    {
        std::lock_guard lk(buffer_mutex_);
        span_buffer_.swap(toExport);
    }
    if (toExport.empty())
        return;

    auto sp = opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>(
        toExport.data(), toExport.size());
    inner_->Export(sp);
    JAMI_LOG("[otel] RetryingSpanExporter: forwarded {} buffered span(s) to OTLP exporter",
             toExport.size());
    // toExport freed here; spans are now owned by the HTTP pipeline.
}

void
RetryingSpanExporter::retryLoop()
{
    bool lastProbeResult = false;

    while (!shutdown_.load(std::memory_order_acquire)) {
        // Wait for kRetryIntervalMs or until notified (new spans / network up).
        {
            std::unique_lock lk(buffer_mutex_);
            cv_.wait_for(lk,
                         std::chrono::milliseconds(kRetryIntervalMs),
                         [this] {
                             return shutdown_.load(std::memory_order_relaxed)
                                    || flush_requested_.load(std::memory_order_relaxed)
                                    || !span_buffer_.empty();
                         });
        }

        if (shutdown_.load(std::memory_order_acquire))
            break;

        flush_requested_.store(false, std::memory_order_relaxed);

        // Nothing to export yet?
        {
            std::lock_guard lk(buffer_mutex_);
            if (span_buffer_.empty())
                continue;
        }

        // ── TCP reachability probe ────────────────────────────────────────
        bool reachable = probeTcpEndpoint();

        if (!reachable) {
            size_t pending;
            {
                std::lock_guard lk(buffer_mutex_);
                pending = span_buffer_.size();
            }
            if (lastProbeResult) {
                // Transition: reachable → unreachable
                JAMI_WARNING("[otel] RetryingSpanExporter: OTLP endpoint "
                             "{}:{} unreachable — {} span(s) buffered, will retry",
                             endpoint_host_, endpoint_port_, pending);
            }
            lastProbeResult = false;
            continue; // keep buffer, retry next tick
        }

        if (!lastProbeResult) {
            JAMI_LOG("[otel] RetryingSpanExporter: OTLP endpoint reachable, "
                     "flushing buffer");
        }
        lastProbeResult = true;

        // ── Delivery ──────────────────────────────────────────────────────
        std::lock_guard lk(inner_mutex_);
        exportBuffer();
    }
}

} // namespace telemetry
} // namespace jami

#endif // JAMI_OTEL_EXPORT_ENABLED
