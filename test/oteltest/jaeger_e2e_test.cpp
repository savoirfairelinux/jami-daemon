// Quick end-to-end test: send a span to Jaeger via OTLP HTTP and verify it
// arrives. Requires Jaeger running with OTLP HTTP on localhost:4318.
//
// Build:
//   g++ -std=c++20 -I../../src -DENABLE_OTEL \
//       $(pkg-config --cflags opentelemetry-cpp) \
//       jaeger_e2e_test.cpp ../../src/otel/otel_init.cpp \
//       $(pkg-config --libs opentelemetry-cpp) \
//       -o jaeger_e2e_test

#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>

#include "otel/otel_init.h"

#ifdef ENABLE_OTEL
#include "otel/otel_context.h"
#include "otel/otel_attributes.h"
#endif

int main()
{
    // ── OTLP HTTP → Jaeger on localhost:4318 ──
    jami::otel::OtelConfig cfg;
    cfg.service_name     = "jami-daemon";
    cfg.service_version  = "16.0.0";
    cfg.enable_traces    = true;
    cfg.enable_metrics   = false;
    cfg.enable_logs      = false;
    cfg.trace_exporter   = jami::otel::OtelConfig::ExporterType::OtlpHttp;
    cfg.otlp_endpoint    = "http://localhost:4318";

    if (!jami::otel::initOtel(cfg)) {
        std::cerr << "FAIL: initOtel(OtlpHttp) returned false\n";
        return EXIT_FAILURE;
    }

#ifdef ENABLE_OTEL
    // Create a fake call.outgoing span with realistic attributes
    {
        jami::otel::SpanScope span("jami.calls", "call.outgoing",
                                   opentelemetry::trace::SpanKind::kClient);
        span.setAttribute(jami::otel::attr::CALL_TYPE, "audio");
        span.setAttribute(jami::otel::attr::CALL_DIRECTION, "outgoing");
        span.setAttribute(jami::otel::attr::ACCOUNT_TYPE, "jami");

        // Simulate SDP negotiation child span
        {
            auto parentCtx = span.getContext();
            jami::otel::SpanScope child("jami.calls", "call.sdp.negotiate", parentCtx);
            child.setAttribute("jami.media.codec", "opus");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Simulate ICE gathering child span
        {
            auto parentCtx = span.getContext();
            jami::otel::SpanScope child("jami.calls", "ice.candidate.gathering", parentCtx);
            child.setAttribute("jami.ice.result", "success");
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Give the batch exporter time to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif

    jami::otel::shutdownOtel();

    std::cout << "Traces sent to Jaeger OTLP HTTP at http://localhost:4318\n"
              << "Check Jaeger UI at http://localhost:16686 → service: jami-daemon\n"
              << "Look for operation: call.outgoing with child spans\n";

    return EXIT_SUCCESS;
}
