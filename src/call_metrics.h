// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// call_metrics.h
// ──────────────
// Singleton call-level OpenTelemetry metric instruments.
//
// All instruments are created lazily on the first call to getCallMetrics().
// That first call must happen after jami::otel::initOtel() has been called.
//
// This header is a no-op when ENABLE_OTEL is not defined.

#pragma once

#ifdef ENABLE_OTEL

#include "otel/otel_init.h"

#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/nostd/shared_ptr.h>

namespace jami {
namespace metrics {

// ─────────────────────────────────────────────────────────────────────────────
/// Call-level metric instruments.
///
/// All shared_ptr fields point to instruments vended by the OTel SDK.
/// The SDK guarantees they remain valid until shutdownOtel() is called.
// ─────────────────────────────────────────────────────────────────────────────
struct CallMetrics
{
    /// jami.calls.active — current number of answered (CURRENT state) calls.
    /// Incremented when a call reaches ACTIVE+CONNECTED; decremented on OVER.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::UpDownCounter<int64_t>> active_calls;

    /// jami.calls.total — monotonically increasing count of all calls initiated.
    /// Incremented once per call at construction time (SIPCall::SIPCall).
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::Counter<uint64_t>> total_calls;

    /// jami.calls.failed — monotonically increasing count of calls that ended
    /// with CallState::MERROR.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::Counter<uint64_t>> failed_calls;

    /// jami.call.setup.duration — histogram of call setup time in milliseconds,
    /// measured from SIPCall construction to CallState ACTIVE + ConnectionState CONNECTED.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::Histogram<double>> setup_duration;

    /// jami.call.duration — histogram of call duration in seconds,
    /// measured from ACTIVE+CONNECTED to OVER.
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::Histogram<double>> call_duration;
};

// ─────────────────────────────────────────────────────────────────────────────
/// Returns the singleton CallMetrics instance, creating instruments on first
/// invocation.  Safe to call from any thread after initOtel() has returned
/// successfully.
// ─────────────────────────────────────────────────────────────────────────────
CallMetrics& getCallMetrics();

} // namespace metrics
} // namespace jami

#endif // ENABLE_OTEL
