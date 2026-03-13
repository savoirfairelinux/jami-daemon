// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// log_bridge_test.cpp
// ───────────────────
// Smoke-test for the OTel logging bridge.
//
// The test:
//   1. Initialises OTel with the stdout exporter (logs enabled).
//   2. Installs the OTel log bridge (min_severity = 0 → forward everything).
//   3. Enables jami's console sink so the LogDispatcher thread starts.
//   4. Creates an active span so trace_id / span_id are injected into records.
//   5. Emits log messages at all severity levels through jami's Logger.
//   6. Removes the bridge and shuts down OTel.
//   7. Prints "LOG_BRIDGE_TEST_PASS" on stdout.
//
// Expected CTest outcome: the process exits with code 0 and the output matches
// the PASS_REGULAR_EXPRESSION "LOG_BRIDGE_TEST_PASS".
//
// Build:
//   cmake -S test/oteltest -B _build_log_bridge \
//         -DENABLE_OTEL=ON \
//         -DOPENTELEMETRY_CPP_ROOT=/path/to/otel/install \
//         -DCMAKE_MODULE_PATH=/path/to/jami-daemon/CMake
//   cmake --build _build_log_bridge
//   ctest --test-dir _build_log_bridge --output-on-failure

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

// jami OTel wrappers
#include "otel/otel_init.h"
#include "otel/otel_log_bridge.h"

// jami logger (provides JAMI_DBG / JAMI_INFO / JAMI_WARN / JAMI_ERR macros
// and Logger::setConsoleLog / Logger::setDebugMode).
#include "logger.h"

#ifdef ENABLE_OTEL
#  include "otel/otel_context.h"   // SpanScope

#  include <opentelemetry/trace/provider.h>
#  include <opentelemetry/context/runtime_context.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void
failWith(const char* reason) noexcept
{
    std::cerr << "LOG_BRIDGE_TEST_FAIL: " << reason << '\n';
    std::exit(EXIT_FAILURE);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    // ── 1. Initialise OTel (stdout exporter, all signals enabled) ────────────
    jami::otel::OtelConfig cfg;
    cfg.service_name            = "jami-log-bridge-test";
    cfg.service_version         = "0.0.1";
    cfg.enable_traces           = true;
    cfg.enable_metrics          = false;  // not needed for this test
    cfg.enable_logs             = true;
    cfg.trace_exporter          = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.logs_exporter           = jami::otel::OtelConfig::ExporterType::Stdout;

    if (!jami::otel::initOtel(cfg))
        failWith("initOtel() returned false");

    // ── 2. Enable jami's console sink so that LogDispatcher starts its thread.
    //       Without at least one enabled sink (or the extra handler), calls to
    //       Logger::write() return immediately before enqueuing anything.
    //       We use setDebugMode(true) to allow DEBUG records through vlog().
    jami::Logger::setConsoleLog(true);
    jami::Logger::setDebugMode(true);

    // ── 3. Install the OTel log bridge (forward all severity levels).
    //       min_severity = 0 → forward DEBUG and above.
    if (!jami::otel::installOtelLogBridge(0 /* DEBUG and above */))
        failWith("installOtelLogBridge() returned false");

#ifdef ENABLE_OTEL
    // ── 4. Create an active span so that trace_id / span_id are injected ─────
    try {
        jami::otel::SpanScope testSpan("jami.log-bridge-test", "log.bridge.test");

        // ── 5. Emit log messages through jami's Logger macros ────────────────
        // These use printf-style vlog() which respects the debugEnabled guard.
        JAMI_DBG("log-bridge-test: DEBUG message (iteration %d)", 1);
        JAMI_INFO("log-bridge-test: INFO  message (iteration %d)", 1);
        JAMI_WARN("log-bridge-test: WARN  message (iteration %d)", 1);
        JAMI_ERR ("log-bridge-test: ERROR message (iteration %d)", 1);

        // fmt-style macros (go through Logger::write() directly).
        JAMI_LOG("log-bridge-test: JAMI_LOG fmt-style message");
        JAMI_WARNING("log-bridge-test: JAMI_WARNING fmt-style message");
        JAMI_ERROR  ("log-bridge-test: JAMI_ERROR   fmt-style message");

        // The SpanScope destructor ends the span here, finalising the context.
    } catch (const std::exception& ex) {
        jami::otel::removeOtelLogBridge();
        jami::otel::shutdownOtel();
        failWith(ex.what());
    }

    // Allow the BatchLogRecordProcessor a moment to process the queue before
    // we call shutdownOtel() (which also flushes, but the small sleep makes
    // the stdout output easier to read in the test log).
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif // ENABLE_OTEL

    // ── 6. Remove bridge before shutting down OTel ────────────────────────────
    jami::otel::removeOtelLogBridge();

    // Disable console log so the jami dispatcher thread stops cleanly.
    jami::Logger::setConsoleLog(false);

    // ── 7. Shutdown OTel (flushes all pending batches) ───────────────────────
    jami::otel::shutdownOtel();

    // ── 8. Confirm success ────────────────────────────────────────────────────
    std::cout << "LOG_BRIDGE_TEST_PASS\n";
    return EXIT_SUCCESS;
}
