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

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span_startoptions.h>

#include <memory>
#include <string>

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
    return std::shared_ptr<void>(raw, [sp](void*) mutable { sp.reset(); });
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

} // namespace trace
} // namespace jami
