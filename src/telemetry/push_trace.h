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
 * @file push_trace.h
 * @brief Thin helper for the "jami.push" tracer — instruments the
 *        push-notification → incoming-call flow.
 *
 * Provides:
 *   - pushTracer()            — shared Tracer for the push subsystem
 *   - traceParentFromHandle() — (reused from calls_trace.h)
 *   - parseTraceParent()      — (reused from calls_trace.h)
 *   - startChildSpan()        — (reused from calls_trace.h)
 *
 * The push notification flow crosses a process boundary (Android ↔ C++
 * daemon) via JNI.  A W3C traceparent string is injected into the push
 * data map on the Android side and extracted here to parent the daemon
 * spans under the same trace.
 */

#include "calls_trace.h"            // reuse span handle helpers, traceparent, callsTracer
#include "logger.h"

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/span.h>

#include <memory>
#include <string>

namespace jami {
namespace trace {

/// Canonical instrumentation-scope name for the push-notification subsystem.
inline constexpr const char* kPushTracerName = "jami.push";

/**
 * Return the shared Tracer for the push-notification subsystem.
 * Uses the global TracerProvider set by initTelemetry().
 * Never returns nullptr — gives a Noop tracer if not initialised.
 */
inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
pushTracer()
{
    return opentelemetry::trace::Provider::GetTracerProvider()
        ->GetTracer(kPushTracerName, PACKAGE_VERSION);
}

/**
 * Key injected into the push data map to carry the W3C traceparent
 * across the JNI (Android → C++) boundary.
 */
inline constexpr const char* kPushTraceparentKey = "otel.traceparent";

/**
 * Start the root span for push-notification processing on the daemon side.
 * If the data map contains a traceparent sent by the Android client, the
 * span becomes a child of that remote context so that the whole flow
 * (FCM → daemon → IncomingCall signal → UI) forms a single trace.
 *
 * @param data  The key/value map forwarded from the Android push payload.
 * @return An opaque span handle (shared_ptr<void>).
 */
inline std::shared_ptr<void>
startPushRootSpan(const std::map<std::string, std::string>& data)
{
    opentelemetry::trace::StartSpanOptions opts;

    // If the Android side injected a traceparent, honour it.
    auto it = data.find(kPushTraceparentKey);
    if (it != data.end() && !it->second.empty()) {
        auto remote = parseTraceParent(it->second);
        if (remote.IsValid())
            opts.parent = remote;
    }

    auto span = pushTracer()->StartSpan("daemon.push.received", opts);

    // Annotate with available push metadata.
    auto toIt = data.find("to");
    if (toIt != data.end())
        span->SetAttribute("push.account_id", toIt->second);
    auto keyIt = data.find("key");
    if (keyIt != data.end())
        span->SetAttribute("push.key", keyIt->second);

    return makeSpanHandle(std::move(span));
}

} // namespace trace
} // namespace jami
