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

/**
 * @file telemetry_test.cpp
 * @brief Unit test for the telemetry bootstrap layer.
 *
 * Verifies that:
 *   1. initTelemetry() creates a functional TracerProvider
 *   2. Spans created via the calls tracer are captured by the in-memory exporter
 *   3. drainSpans() returns the expected span data
 *   4. shutdownTelemetry() cleanly resets to a NoopProvider
 */

#include "telemetry/telemetry.h"
#include "telemetry/calls_trace.h"

#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/provider.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace trace_sdk = opentelemetry::sdk::trace;

static int failures = 0;

#define ASSERT_TRUE(cond, msg)                                      \
    do {                                                            \
        if (!(cond)) {                                              \
            std::cerr << "FAIL: " << msg << " (" #cond ")\n";     \
            ++failures;                                             \
        } else {                                                    \
            std::cout << "OK: " << msg << "\n";                    \
        }                                                           \
    } while (0)

int
main()
{
    std::cout << "=== Telemetry In-Memory Verification ===\n\n";

    // ── 1. Init ────────────────────────────────────────────────────────
    jami::telemetry::initTelemetry("jami-daemon-test", "0.0.0-test");

    // ── 2. Simulate a call lifecycle ───────────────────────────────────
    {
        auto tracer = jami::trace::callsTracer();
        ASSERT_TRUE(tracer != nullptr, "callsTracer() returns non-null");

        // Outgoing call
        auto span = tracer->StartSpan("call.outgoing");
        span->SetAttribute("call.id", std::string("test-call-001"));
        span->SetAttribute("call.type", std::string("OUTGOING"));
        span->SetAttribute("call.account_id", std::string("acc-123"));
        span->AddEvent("call.state_transition", {
            {"call.state.from", std::string("INACTIVE")},
            {"call.state.to", std::string("TRYING")},
        });
        span->AddEvent("sdp.offer.sent");
        span->AddEvent("ice.negotiation.started");
        span->AddEvent("ice.negotiation.completed");
        span->AddEvent("media.started");
        span->AddEvent("call.state_transition", {
            {"call.state.from", std::string("TRYING")},
            {"call.state.to", std::string("CURRENT")},
        });
        span->SetAttribute("call.peer_uri", std::string("sip:peer@example.com"));
        span->SetAttribute("call.duration_ms", static_cast<int64_t>(5000));
        span->SetStatus(opentelemetry::trace::StatusCode::kOk);
        span->End();
    }

    {
        // Incoming call that fails
        auto tracer = jami::trace::callsTracer();
        auto span = tracer->StartSpan("call.incoming");
        span->SetAttribute("call.id", std::string("test-call-002"));
        span->SetAttribute("call.type", std::string("INCOMING"));
        span->AddEvent("call.failure", {
            {"call.failure_code", static_cast<int64_t>(486)},
        });
        span->SetStatus(opentelemetry::trace::StatusCode::kError, "Busy Here");
        span->End();
    }

    // ── 3. Drain and verify ────────────────────────────────────────────
    auto spans = jami::telemetry::drainSpans();

    std::cout << "\nDrained " << spans.size() << " span(s):\n";
    ASSERT_TRUE(spans.size() >= 2, "At least 2 spans emitted");

    bool foundOutgoing = false;
    bool foundIncoming = false;

    for (auto& sp : spans) {
        auto name = std::string(sp->GetName());
        std::cout << "  Span: name=" << name
                  << "  status=" << static_cast<int>(sp->GetStatus())
                  << "  events=" << sp->GetEvents().size()
                  << "  attrs=" << sp->GetAttributes().size()
                  << "\n";

        // Print attributes
        for (auto& [key, val] : sp->GetAttributes()) {
            std::cout << "    attr: " << key << "\n";
        }

        // Print events
        for (auto& ev : sp->GetEvents()) {
            std::cout << "    event: " << ev.GetName() << "\n";
        }

        if (name == "call.outgoing") {
            foundOutgoing = true;
            ASSERT_TRUE(sp->GetStatus() == opentelemetry::trace::StatusCode::kOk,
                        "call.outgoing has OK status");
            ASSERT_TRUE(sp->GetEvents().size() >= 6,
                        "call.outgoing has at least 6 events");

            // Check for call.id attribute
            bool hasCallId = false;
            for (auto& [key, val] : sp->GetAttributes()) {
                if (key == "call.id") hasCallId = true;
            }
            ASSERT_TRUE(hasCallId, "call.outgoing has call.id attribute");
        }

        if (name == "call.incoming") {
            foundIncoming = true;
            ASSERT_TRUE(sp->GetStatus() == opentelemetry::trace::StatusCode::kError,
                        "call.incoming has ERROR status");
        }
    }

    ASSERT_TRUE(foundOutgoing, "Found a span named 'call.outgoing'");
    ASSERT_TRUE(foundIncoming, "Found a span named 'call.incoming'");

    // ── 4. Second drain should be empty ────────────────────────────────
    auto spans2 = jami::telemetry::drainSpans();
    ASSERT_TRUE(spans2.empty(), "Second drain is empty (destructive read)");

    // ── 5. Shutdown ────────────────────────────────────────────────────
    jami::telemetry::shutdownTelemetry();

    // After shutdown, drainSpans should return empty (no crash)
    auto spans3 = jami::telemetry::drainSpans();
    ASSERT_TRUE(spans3.empty(), "drainSpans() after shutdown returns empty");

    // After shutdown, tracer should be noop (no crash)
    {
        auto tracer = jami::trace::callsTracer();
        auto span = tracer->StartSpan("test.noop");
        span->End();
    }

    std::cout << "\n=== Results: " << (failures == 0 ? "ALL PASSED" : "FAILURES")
              << " (" << failures << " failure(s)) ===\n";

    return failures == 0 ? 0 : 1;
}
