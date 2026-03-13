// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// otel_basic_test.cpp
// ───────────────────
// Standalone integration smoke-test for the OTel bootstrap infrastructure.
//
// Build with ENABLE_OTEL defined and link against the OTel ostream exporters.
// See the sibling CMakeLists.txt for the full build recipe.
//
// Successful run prints exactly one line:
//   OTEL_TEST_PASS
// Any failure prints a message to stderr and exits with a non-zero code.

#include <cstdlib>
#include <iostream>
#include <stdexcept>

// Pull in the jami OTel wrappers.
// The ENABLE_OTEL macro must be set by the build system for the real SDK path.
#include "otel/otel_init.h"
#include "otel/otel_attributes.h"

#ifdef ENABLE_OTEL
#  include "otel/otel_context.h"
#endif

int main()
{
    // ── 1. Initialise with stdout exporter (no external collector needed) ──
    jami::otel::OtelConfig cfg;
    cfg.service_name             = "jami-otel-test";
    cfg.service_version          = "0.0.1";
    cfg.enable_traces            = true;
    cfg.enable_metrics           = true;
    cfg.enable_logs              = true;
    cfg.trace_exporter           = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.metrics_exporter         = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.logs_exporter            = jami::otel::OtelConfig::ExporterType::Stdout;
    cfg.metrics_export_interval  = std::chrono::milliseconds(1000);

    if (!jami::otel::initOtel(cfg)) {
        std::cerr << "OTEL_TEST_FAIL: initOtel() returned false\n";
        return EXIT_FAILURE;
    }

#ifdef ENABLE_OTEL
    // ── 2. Create a span via SpanScope, set some attributes ───────────────
    try {
        jami::otel::SpanScope span("jami.test", "call.setup");
        span.setAttribute(jami::otel::attr::CALL_TYPE,      "audio");
        span.setAttribute(jami::otel::attr::CALL_DIRECTION, "outbound");
        span.setAttribute(jami::otel::attr::CALL_ID_HASH,   "abcdef1234567890");
        span.setAttribute(jami::otel::attr::ACCOUNT_TYPE,   "jami");
        span.setAttribute("test.iteration", static_cast<int64_t>(1));
        // Span destructor runs here — records end timestamp.
    } catch (const std::exception& ex) {
        std::cerr << "OTEL_TEST_FAIL: exception during span creation: " << ex.what() << '\n';
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    // ── 3. Create an async span, end it explicitly ─────────────────────────
    try {
        auto async = jami::otel::AsyncSpan::start("jami.test", "dht.lookup");
        if (!async.valid()) {
            std::cerr << "OTEL_TEST_FAIL: AsyncSpan::valid() returned false\n";
            jami::otel::shutdownOtel();
            return EXIT_FAILURE;
        }
        async.end(/*success=*/true);
    } catch (const std::exception& ex) {
        std::cerr << "OTEL_TEST_FAIL: exception during async span: " << ex.what() << '\n';
        jami::otel::shutdownOtel();
        return EXIT_FAILURE;
    }

    // ── 4. Verify tracer/meter/logger accessors return non-null objects ────
    {
        auto tracer = jami::otel::getTracer("jami.test");
        if (!tracer) {
            std::cerr << "OTEL_TEST_FAIL: getTracer() returned null\n";
            jami::otel::shutdownOtel();
            return EXIT_FAILURE;
        }

        auto meter = jami::otel::getMeter("jami.test");
        if (!meter) {
            std::cerr << "OTEL_TEST_FAIL: getMeter() returned null\n";
            jami::otel::shutdownOtel();
            return EXIT_FAILURE;
        }

        auto logger = jami::otel::getOtelLogger("jami.test");
        if (!logger) {
            std::cerr << "OTEL_TEST_FAIL: getOtelLogger() returned null\n";
            jami::otel::shutdownOtel();
            return EXIT_FAILURE;
        }
    }

#else
    // Without ENABLE_OTEL the stubs must at least be callable without crashing.
    jami::otel::OtelConfig cfg2;
    if (!jami::otel::initOtel(cfg2)) {
        std::cerr << "OTEL_TEST_FAIL: stub initOtel() returned false\n";
        return EXIT_FAILURE;
    }
#endif // ENABLE_OTEL

    // ── 5. Graceful shutdown ───────────────────────────────────────────────
    jami::otel::shutdownOtel();

    std::cout << "OTEL_TEST_PASS\n";
    return EXIT_SUCCESS;
}
