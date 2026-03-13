// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// call_metrics_test.cpp
// ─────────────────────
// Standalone smoke-test for the jami::metrics::CallMetrics instruments.
//
// Build with ENABLE_OTEL defined and link against the OTel ostream exporters.
// See the sibling CMakeLists.txt for the complete build recipe.
//
// Successful run prints exactly one line:
//   CALL_METRICS_TEST_PASS
// Any failure prints a message to std::cerr and exits with a non-zero code.

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "otel/otel_init.h"

#ifdef ENABLE_OTEL
#  include "call_metrics.h"
#  include "call_metrics.cpp"   // single-TU build: pull in the .cpp directly
#endif

int main()
{
    // ── 1. Initialise the OTel providers with stdout exporters ─────────────
    jami::otel::OtelConfig cfg;
    cfg.service_name             = "jami-call-metrics-test";
    cfg.service_version          = "0.0.1";
    cfg.enable_traces            = true;
    cfg.enable_metrics           = true;
    cfg.enable_logs              = false;
    cfg.trace_exporter           = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.metrics_exporter         = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.metrics_export_interval  = std::chrono::milliseconds(500); // fast for tests

    if (!jami::otel::initOtel(cfg)) {
        std::cerr << "CALL_METRICS_TEST_FAIL: initOtel() returned false\n";
        return EXIT_FAILURE;
    }

#ifdef ENABLE_OTEL
    // ── 2. Obtain CallMetrics singleton ────────────────────────────────────
    jami::metrics::CallMetrics* metricsPtr = nullptr;
    try {
        metricsPtr = &jami::metrics::getCallMetrics();
    } catch (const std::exception& ex) {
        std::cerr << "CALL_METRICS_TEST_FAIL: getCallMetrics() threw: " << ex.what() << '\n';
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    if (!metricsPtr) {
        std::cerr << "CALL_METRICS_TEST_FAIL: getCallMetrics() returned null reference\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    auto& m = *metricsPtr;

    // ── 3. Validate that all instruments were initialised ──────────────────
    if (!m.active_calls) {
        std::cerr << "CALL_METRICS_TEST_FAIL: active_calls instrument is null\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }
    if (!m.total_calls) {
        std::cerr << "CALL_METRICS_TEST_FAIL: total_calls instrument is null\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }
    if (!m.failed_calls) {
        std::cerr << "CALL_METRICS_TEST_FAIL: failed_calls instrument is null\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }
    if (!m.setup_duration) {
        std::cerr << "CALL_METRICS_TEST_FAIL: setup_duration instrument is null\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }
    if (!m.call_duration) {
        std::cerr << "CALL_METRICS_TEST_FAIL: call_duration instrument is null\n";
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    // ── 4. Exercise each instrument (no-throw, no-crash) ──────────────────
    try {
        // Simulate: 2 calls started
        m.total_calls->Add(2);

        // Simulate: both became active
        m.active_calls->Add(2);

        // Simulate: setup latencies of 120 ms and 350 ms
        m.setup_duration->Record(120.0, {});
        m.setup_duration->Record(350.0, {});

        // Simulate: 1 call ended normally after 42 seconds
        m.active_calls->Add(-1);
        m.call_duration->Record(42.0, {});

        // Simulate: 1 call ended with an error
        m.active_calls->Add(-1);
        m.failed_calls->Add(1);

        // Verify singleton stability (second access returns same object)
        auto& m2 = jami::metrics::getCallMetrics();
        if (&m != &m2) {
            std::cerr << "CALL_METRICS_TEST_FAIL: getCallMetrics() returned different address\n";
            jami::otel::shutdownOtel();
            return EXIT_FAILURE;
        }

    } catch (const std::exception& ex) {
        std::cerr << "CALL_METRICS_TEST_FAIL: exception while recording metrics: "
                  << ex.what() << '\n';
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    // ── 5. Flush and shut down ─────────────────────────────────────────────
    jami::otel::shutdownOtel();

#else // !ENABLE_OTEL — the test must still pass as a no-op when OTel is off

    // Nothing to test; just confirm initOtel() / shutdownOtel() compile fine.
    jami::otel::shutdownOtel();

#endif // ENABLE_OTEL

    std::cout << "CALL_METRICS_TEST_PASS\n";
    return EXIT_SUCCESS;
}
