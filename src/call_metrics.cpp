// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// call_metrics.cpp
// ─────────────────
// Implements jami::metrics::getCallMetrics() — lazy singleton factory.

#ifdef ENABLE_OTEL

#include "call_metrics.h"
#include "otel/otel_init.h"

#include <mutex>

namespace jami {
namespace metrics {

CallMetrics&
getCallMetrics()
{
    // Construct and initialise once; safe across concurrent first calls.
    static std::once_flag  s_initFlag;
    static CallMetrics     s_metrics;

    std::call_once(s_initFlag, []() {
        auto meter = jami::otel::getMeter("jami.calls");

        s_metrics.active_calls = meter->CreateInt64UpDownCounter(
            "jami.calls.active",
            "Number of calls currently in the CURRENT (answered) state",
            "{calls}");

        s_metrics.total_calls = meter->CreateUInt64Counter(
            "jami.calls.total",
            "Total number of calls initiated since daemon start",
            "{calls}");

        s_metrics.failed_calls = meter->CreateUInt64Counter(
            "jami.calls.failed",
            "Total number of calls that ended with a media error (MERROR)",
            "{calls}");

        s_metrics.setup_duration = meter->CreateDoubleHistogram(
            "jami.call.setup.duration",
            "Time from call creation to CURRENT state (answered), in milliseconds",
            "ms");

        s_metrics.call_duration = meter->CreateDoubleHistogram(
            "jami.call.duration",
            "Duration of a call from CURRENT (answered) to OVER, in seconds",
            "s");
    });

    return s_metrics;
}

} // namespace metrics
} // namespace jami

#endif // ENABLE_OTEL
