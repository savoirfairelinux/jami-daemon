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
 * @file calls_trace.h
 * @brief Thin helper for obtaining the "jami.calls" tracer and safely
 *        working with spans.  All OTel operations are wrapped so they
 *        never throw into business logic.
 */

#include "logger.h"

#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <opentelemetry/trace/span_context.h>
#include <opentelemetry/trace/trace_id.h>
#include <opentelemetry/trace/span_id.h>
#include <opentelemetry/trace/trace_flags.h>

#include <memory>
#include <string>
#include <cstdio>

namespace jami {
namespace trace {

/// Canonical instrumentation-scope name for the calls subsystem.
inline constexpr const char* kCallsTracerName = "jami.calls";

/**
 * Return the shared Tracer for the calls subsystem.
 * Uses the global TracerProvider set by initTelemetry().
 * Never returns nullptr — gives a Noop tracer if not initialised.
 */
inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
callsTracer()
{
    return opentelemetry::trace::Provider::GetTracerProvider()
        ->GetTracer(kCallsTracerName, PACKAGE_VERSION);
}

/**
 * Extract the OTel Span* from the opaque shared_ptr<void> stored in Call.
 * Returns nullptr if the handle is empty.
 */
inline opentelemetry::trace::Span*
spanFromHandle(const std::shared_ptr<void>& handle)
{
    return static_cast<opentelemetry::trace::Span*>(handle.get());
}

/**
 * Create an opaque span handle (shared_ptr<void>) from a nostd::shared_ptr<Span>.
 * The OTel span is prevented from being deleted by wrapping the nostd::shared_ptr
 * in a shared_ptr<void> via a custom deleter that holds the nostd ptr alive.
 */
inline std::shared_ptr<void>
makeSpanHandle(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> sp)
{
    // Capture the nostd shared_ptr by value in the deleter so its ref-count
    // keeps the underlying span alive for the lifetime of the shared_ptr<void>.
    auto* raw = sp.get();
     return std::shared_ptr<void>(raw, [sp = std::move(sp)](void*) mutable {
    sp = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>{};
});}

/**
 * Start a new span as a child of the span referenced by parentHandle.
 * The parent relationship is set explicitly via StartSpanOptions so that
 * no thread-local Scope needs to be active.
 * Returns a Noop span (never nullptr) if parentHandle is empty.
 */
inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
startChildSpan(const std::shared_ptr<void>& parentHandle, const char* name)
{
    opentelemetry::trace::StartSpanOptions opts;
    if (auto* parent = spanFromHandle(parentHandle))
        opts.parent = parent->GetContext();
    return callsTracer()->StartSpan(name, opts);
}

/**
 * Convert Call::CallState to a human-readable string.
 */
inline const char* callStateStr(uint8_t s)
{
    static constexpr const char* names[] = {
        "INACTIVE", "ACTIVE", "HOLD", "BUSY", "PEER_BUSY", "MERROR", "OVER"};
    return s < 7 ? names[s] : "UNKNOWN";
}

/**
 * Convert Call::ConnectionState to a human-readable string.
 */
inline const char* cnxStateStr(uint8_t s)
{
    static constexpr const char* names[] = {
        "DISCONNECTED", "TRYING", "PROGRESSING", "RINGING", "CONNECTED"};
    return s < 5 ? names[s] : "UNKNOWN";
}

/**
 * Convert Call::CallType to a human-readable string.
 */
inline const char* callTypeStr(uint8_t t)
{
    static constexpr const char* names[] = {"INCOMING", "OUTGOING", "MISSED"};
    return t < 3 ? names[t] : "UNKNOWN";
}

/**
 * Return true if the calling thread has an active (valid, non-noop) span
 * in its OTel context.  Used to decide whether Call::Call() should create
 * a span immediately or defer to addSubCall().
 */
inline bool hasActiveSpan()
{
    auto ctx  = opentelemetry::context::RuntimeContext::GetCurrent();
    auto span = opentelemetry::trace::GetSpan(ctx);
    return span->GetContext().IsValid();
}

// ── W3C traceparent helpers for SDP propagation ────────────────────────

inline uint8_t hexDigit(char c)
{
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
    return 0;
}

/**
 * Format the span context of @p handle as a W3C traceparent string:
 *   "00-<32-hex-traceId>-<16-hex-spanId>-<2-hex-flags>"
 * Returns an empty string when handle is null / invalid.
 */
inline std::string traceParentFromHandle(const std::shared_ptr<void>& handle)
{
    auto* span = spanFromHandle(handle);
    if (!span) return {};
    auto ctx = span->GetContext();
    if (!ctx.IsValid()) return {};

    char traceId[33], spanId[17];
    ctx.trace_id().ToLowerBase16(
        opentelemetry::nostd::span<char, 32>{traceId, 32});
    traceId[32] = '\0';
    ctx.span_id().ToLowerBase16(
        opentelemetry::nostd::span<char, 16>{spanId, 16});
    spanId[16] = '\0';

    char buf[60];
    std::snprintf(buf, sizeof(buf), "00-%s-%s-%02x",
                  traceId, spanId, ctx.trace_flags().flags());
    return std::string(buf);
}

/**
 * Parse a W3C traceparent string and return a remote SpanContext.
 * Returns an invalid context on any format error.
 */
inline opentelemetry::trace::SpanContext parseTraceParent(const std::string& tp)
{
    // "00-<32hex>-<16hex>-<2hex>" → minimum 55 chars
    if (tp.size() < 55 || tp[2] != '-' || tp[35] != '-' || tp[52] != '-')
        return opentelemetry::trace::SpanContext::GetInvalid();

    uint8_t traceIdBuf[16], spanIdBuf[8];
    for (int i = 0; i < 16; ++i)
        traceIdBuf[i] = static_cast<uint8_t>(
            (hexDigit(tp[3 + 2 * i]) << 4) | hexDigit(tp[4 + 2 * i]));
    for (int i = 0; i < 8; ++i)
        spanIdBuf[i] = static_cast<uint8_t>(
            (hexDigit(tp[36 + 2 * i]) << 4) | hexDigit(tp[37 + 2 * i]));
    uint8_t flags = static_cast<uint8_t>(
        (hexDigit(tp[53]) << 4) | hexDigit(tp[54]));

    return opentelemetry::trace::SpanContext{
        opentelemetry::trace::TraceId{
            opentelemetry::nostd::span<const uint8_t, 16>{traceIdBuf, 16}},
        opentelemetry::trace::SpanId{
            opentelemetry::nostd::span<const uint8_t, 8>{spanIdBuf, 8}},
        opentelemetry::trace::TraceFlags{flags},
        true /* is_remote */};
}

/**
 * Start a span whose parent is the given remote SpanContext.
 * If @p remote is invalid the span becomes a root.
 */
inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
startSpanWithRemoteParent(const opentelemetry::trace::SpanContext& remote,
                          const char* name)
{
    opentelemetry::trace::StartSpanOptions opts;
    if (remote.IsValid())
        opts.parent = remote;
    return callsTracer()->StartSpan(name, opts);
}

} // namespace trace
} // namespace jami
