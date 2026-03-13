# OpenTelemetry Traces — C++ API Guide

| Field        | Value       |
|--------------|-------------|
| Status       | draft       |
| Last Updated | 2026-03-13  |

---

## 1. Tracer Acquisition Pattern

Tracers are obtained from the global `TracerProvider`. Acquire once and cache.

```cpp
#include "opentelemetry/trace/provider.h"

namespace trace_api = opentelemetry::trace;

// Acquire once (e.g., in a class constructor or static initializer):
auto tracer = trace_api::Provider::GetTracerProvider()
                  ->GetTracer("jami.sip",        // instrumentation scope name
                              "1.0.0",            // scope version (optional)
                              "https://jami.net/schema/sip");  // schema URL (optional)
```

**Instrumentation scope name** should identify the component doing the instrumentation (e.g., `"jami.sip"`, `"jami.dht"`, `"jami.account"`). This is NOT `service.name`.

---

## 2. Span Creation (`StartSpan`)

### Basic span

```cpp
auto span = tracer->StartSpan("call.setup");
// ... do work ...
span->End();
```

### Span with initial attributes

```cpp
trace_api::StartSpanOptions opts;
// opts.kind is set below

auto span = tracer->StartSpan(
    "dht.lookup",
    {
        {"dht.key",    std::string("abc123")},
        {"dht.family", static_cast<int64_t>(AF_INET)},
    });
span->End();
```

> The second argument to `StartSpan` is an `opentelemetry::common::KeyValueIterable`. A brace-list of `{key, value}` pairs works for small attribute sets.

---

## 3. Span Attributes (`SetAttribute`)

Set attributes at any point before `End()`.

```cpp
span->SetAttribute("jami.account.type",   std::string("RING"));
span->SetAttribute("jami.call.id",        std::string(callId));   // see privacy note
span->SetAttribute("net.transport",       std::string("ip_tcp"));
span->SetAttribute("rpc.system",          std::string("jami"));
span->SetAttribute("error.type",          std::string("timeout"));

// Numeric attributes
span->SetAttribute("jami.packet.size",    static_cast<int64_t>(len));
span->SetAttribute("jami.rtt_ms",         static_cast<double>(rtt));
```

**Attribute value types supported**: `bool`, `int32_t`, `int64_t`, `uint32_t`, `double`, `const char*`, `std::string`, `nostd::string_view`, and `nostd::span<T>` for arrays.

> **Privacy**: Never set real peer usernames, phone numbers, or unmasked identifiers as attribute values. Use a SHA-256 hash truncated to 8 hex chars, or a session-scoped numeric ID.

---

## 4. Span Events (`AddEvent`)

Events are time-stamped log entries attached to a span.

```cpp
// Simple event
span->AddEvent("ice.negotiation.started");

// Event with attributes
span->AddEvent("ice.candidate.added", {
    {"ice.candidate.type", std::string("srflx")},
    {"ice.component",      static_cast<int64_t>(1)},
});

// Event with explicit timestamp
auto ts = opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now());
span->AddEvent("tls.handshake.complete", ts);
```

---

## 5. Span Status (`SetStatus`)

```cpp
#include "opentelemetry/trace/span_startoptions.h"

// Success (also the default if SetStatus is never called):
span->SetStatus(trace_api::StatusCode::kOk);

// Error:
span->SetStatus(trace_api::StatusCode::kError, "ICE negotiation timeout after 30s");

// Unset (default):
span->SetStatus(trace_api::StatusCode::kUnset);
```

**Rule**: Only call `SetStatus(kError, ...)` when the operation definitively failed. Do not set `kOk` on intermediate spans — leave them `kUnset` and let the root span reflect the outcome.

---

## 6. Span Kinds

```cpp
#include "opentelemetry/trace/span_startoptions.h"

trace_api::StartSpanOptions opts;
opts.kind = trace_api::SpanKind::kInternal;   // default — intra-process
opts.kind = trace_api::SpanKind::kClient;     // outbound RPC/network call
opts.kind = trace_api::SpanKind::kServer;     // inbound RPC/network server handler
opts.kind = trace_api::SpanKind::kProducer;   // async message publish
opts.kind = trace_api::SpanKind::kConsumer;   // async message receive

auto span = tracer->StartSpan("dbus.method.call", {}, opts);
```

**Guidelines for jami-daemon**:
| Scenario | SpanKind |
|----------|----------|
| Outbound SIP INVITE | `kClient` |
| Incoming SIP INVITE handler | `kServer` |
| DHT `get`/`put` call | `kClient` |
| Emitting a message to a Jami swarm | `kProducer` |
| Processing a received swarm message | `kConsumer` |
| Internal helper function | `kInternal` |

---

## 7. Parent Span / Context Propagation (Same Process)

When a span is marked **active**, child spans automatically inherit its context.

```cpp
// Mark a span as active → returns an RAII Scope guard
auto outer_span  = tracer->StartSpan("call.setup");
auto outer_scope = tracer->WithActiveSpan(outer_span);   // ← makes it active

{
    // This span automatically has outer_span as parent:
    auto inner_span  = tracer->StartSpan("sip.invite.send");
    auto inner_scope = tracer->WithActiveSpan(inner_span);

    // ... send SIP INVITE ...

    inner_span->End();
}   // inner_scope destructor pops the active span

outer_span->End();
```

To **explicitly** parent a span without the active-span mechanism:

```cpp
trace_api::StartSpanOptions opts;
opts.parent = parent_span->GetContext();   // SpanContext

auto child = tracer->StartSpan("sip.ack", {}, opts);
```

---

## 8. Context Injection/Extraction (Cross-Process Propagation)

Uses W3C TraceContext (traceparent/tracestate headers) by default.

### Setup

```cpp
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"

// Called once during OtelInit():
opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::context::propagation::TextMapPropagator>(
        new opentelemetry::trace::propagation::HttpTraceContext()));
```

### Inject (outbound request — e.g., SIP header)

```cpp
// Custom carrier wrapping a SIP header map:
template<typename HeaderMap>
class SipTextMapCarrier : public opentelemetry::context::propagation::TextMapCarrier {
public:
    explicit SipTextMapCarrier(HeaderMap& headers) : headers_(headers) {}
    nostd::string_view Get(nostd::string_view key) const noexcept override {
        auto it = headers_.find(std::string(key));
        return (it != headers_.end()) ? nostd::string_view(it->second) : "";
    }
    void Set(nostd::string_view key, nostd::string_view value) noexcept override {
        headers_[std::string(key)] = std::string(value);
    }
private:
    HeaderMap& headers_;
};

// Usage:
SipTextMapCarrier<std::map<std::string,std::string>> carrier(sip_headers);
auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
propagator->Inject(carrier, ctx);
// sip_headers now contains "traceparent" and optionally "tracestate"
```

### Extract (inbound request — e.g., incoming SIP)

```cpp
SipTextMapCarrier carrier(incoming_sip_headers);
auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
auto remote_ctx = propagator->Extract(carrier, ctx);

// Create a span that is a child of the remote span:
trace_api::StartSpanOptions opts;
opts.parent = opentelemetry::trace::propagation::GetSpan(remote_ctx)->GetContext();
auto span = tracer->StartSpan("sip.invite.received", {}, opts);
```

---

## 9. Async Span Patterns (Span Across Threads)

When a span is started in one thread and ended in another, do **not** rely on `WithActiveSpan` (which is thread-local). Instead pass the `nostd::shared_ptr<Span>` explicitly.

```cpp
// Thread A: start span and hand it to async work
auto span = tracer->StartSpan("ice.gathering");
span->SetAttribute("jami.component", static_cast<int64_t>(1));

thread_pool.post([span = std::move(span)]() mutable {
    // Thread B: do work, then end the span
    span->AddEvent("ice.candidate.collected");
    span->SetStatus(trace_api::StatusCode::kOk);
    span->End();
});
```

**Key rule**: The `Span` object is reference-counted (`nostd::shared_ptr`). It is safe to move or copy across threads as long as you don't call methods on it from two threads simultaneously.

---

## 10. RAII Scope Guard Pattern

`Scope` from `WithActiveSpan` is RAII: the span is deactivated when the scope goes out of life.

```cpp
// Pattern: use braces to scope span lifetime
{
    auto span  = tracer->StartSpan("account.registration");
    auto scope = tracer->WithActiveSpan(span);

    try {
        DoRegistration();
        span->SetStatus(trace_api::StatusCode::kOk);
    } catch (const std::exception& e) {
        span->SetStatus(trace_api::StatusCode::kError, e.what());
        span->SetAttribute("exception.message", std::string(e.what()));
        span->SetAttribute("exception.type",    std::string(typeid(e).name()));
        throw;
    }
    span->End();
}   // scope destructor runs here
```

> **Important**: `span->End()` and `scope` destructor are independent. Always call `span->End()` explicitly; do not rely on the span being ended by the scope going out of life.

---

## Source References

- [OTel C++ Instrumentation (Traces)](https://opentelemetry.io/docs/languages/cpp/instrumentation/#traces)
- [Traces API namespace docs](https://opentelemetry-cpp.readthedocs.io/en/latest/otel_docs/namespace_opentelemetry__trace.html)
- [Traces SDK namespace docs](https://opentelemetry-cpp.readthedocs.io/en/latest/otel_docs/namespace_opentelemetry__sdk__trace.html)
- [Context propagation example](https://opentelemetry.io/docs/languages/cpp/instrumentation/#context-propagation)
- [examples/http](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/http)
- [examples/multithreaded](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/multithreaded)

---

## Open Questions

1. **SIP header carrier**: Jami's SIP library (pjsip) uses `pjsip_hdr` linked lists, not a map. What is the cleanest `TextMapCarrier` adapter for pjsip header structs?
2. **DHT propagation**: DHT operations don't have HTTP-style headers. Should W3C traceparent be embedded in a DHT value's metadata field, or should we use a span-link instead of parent?
3. **Span-link vs parent**: For message-queue-style architectures (swarm messages), is it better to use `span->AddLink()` (linking to the producer span) rather than a parent relationship?
4. **Sampling strategy**: At what rate should spans be sampled? `AlwaysOnSampler` is the default. For production, a `TraceIdRatioBased` sampler should be considered to limit volume.
5. **Active span in jami threads**: Does jami-daemon use thread pools that recycle threads? If so, is there a risk of stale active-span context leaking across calls?
