# OpenTelemetry Developer Guide — jami-daemon

**Last Updated:** 2026-03-13  
**Applies to:** jami-daemon / libjami v16.0.0+

---

## Table of Contents

1. [Overview](#1-overview)
2. [Building with OpenTelemetry Enabled](#2-building-with-opentelemetry-enabled)
3. [Running the Daemon with OTel Debug Exporter](#3-running-the-daemon-with-otel-debug-exporter)
4. [Connecting to a Local OTLP Collector](#4-connecting-to-a-local-otlp-collector-jaeger-tempo-etc)
5. [How to Add New Instrumentation](#5-how-to-add-new-instrumentation)
   - [5.1 Adding a Trace Span](#51-adding-a-trace-span)
   - [5.2 Adding Metrics](#52-adding-metrics)
   - [5.3 Span Naming Rules](#53-span-naming-rules)
   - [5.4 Attribute Naming Rules](#54-attribute-naming-rules)
6. [Naming Conventions](#6-naming-conventions)
7. [What NOT to Instrument (Cardinality & Performance Traps)](#7-what-not-to-instrument-cardinality--performance-traps)
8. [Privacy Rules](#8-privacy-rules)
9. [Instrumentation File Layout](#9-instrumentation-file-layout)

---

## 1. Overview

[OpenTelemetry](https://opentelemetry.io/) (OTel) is a vendor-neutral observability
framework that defines APIs and SDKs for collecting **distributed traces**,
**metrics**, and **structured logs** from applications. jami-daemon uses the
[OTel C++ SDK](https://github.com/open-telemetry/opentelemetry-cpp) (v1.25.0+).

**Why jami-daemon uses OTel:**

- Call setup spans the entire daemon stack — from the public API through SIP
  signalling, ICE/TURN negotiation, and DHT peer discovery. A trace reconstructs
  the exact timing and failure point of any call setup attempt in seconds.
- DHT bootstrap and ICE negotiation latencies vary widely by network. Histograms
  broken down by `ice.result` and `account.type` are the primary SLO metrics for
  the P2P network.
- Existing `JAMI_ERR` / `JAMI_WARN` log calls gain trace correlation (`trace_id`,
  `span_id`) for free via the log bridge, making support diagnosis dramatically
  faster.

**OTel is entirely optional.** The build flag `ENABLE_OTEL` guards all
instrumentation. When the flag is **off** (`-DENABLE_OTEL=OFF`, the default), every
`#ifdef ENABLE_OTEL` block compiles away to zero bytes. The OTel API headers use
no-op stubs so instrumented code paths have no runtime overhead in unenlightened
builds.

---

## 2. Building with OpenTelemetry Enabled

### Prerequisites

| Requirement | Minimum version | Notes |
|-------------|----------------|-------|
| CMake | ≥ 3.25 | Required by OTel SDK's own `CMakeLists.txt` |
| opentelemetry-cpp | v1.25.0 | System install, `contrib/` build, or `FetchContent` |
| gRPC | ≥ 1.43 | Only needed if `WITH_OTLP_GRPC=ON` |
| protobuf | ≥ 3.21 | Required by OTLP gRPC exporter |
| C++17 | — | Required for `WITH_STL=ON`; jami already uses C++20 |

### CMake Build Commands

The `CMake/Findopentelemetry-cpp.cmake` find module handles discovery.

**With an existing system/vcpkg install of opentelemetry-cpp:**

```bash
cmake -B build \
      -DENABLE_OTEL=ON \
      -DWITH_OTLP_HTTP=ON \
      -DWITH_OTLP_GRPC=ON \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      ..
make -C build -j$(nproc)
```

**With FetchContent (no system install required; adds 2–5 min to first build):**

> **⚠️ Known limitation:** FetchContent currently pulls v1.19.0 and may
> introduce `-isystem /usr/include` before `contrib/` include paths, causing
> header conflicts (e.g., opendht). Prefer a system/Nix install of v1.25.0+
> when available.

```bash
cmake -B build \
      -DENABLE_OTEL=ON \
      -DOTEL_USE_FETCHCONTENT=ON \
      -DWITH_OTLP_HTTP=ON \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      ..
make -C build -j$(nproc)

# Speed up incremental FetchContent builds in CI:
cmake -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON ...
```

**Stdout exporter only (no gRPC/HTTP needed; dev/debug only):**

```bash
cmake -B build -DENABLE_OTEL=ON ..
make -C build -j$(nproc)
```

### CMake Feature Flags Summary

| Flag | Default | Meaning |
|------|---------|---------|
| `ENABLE_OTEL` | `OFF` | Master switch for all OTel compilation |
| `WITH_OTLP_GRPC` | `OFF` | Enable OTLP/gRPC exporter (requires gRPC + protobuf) |
| `WITH_OTLP_HTTP` | `OFF` | Enable OTLP/HTTP exporter |
| `OTEL_USE_FETCHCONTENT` | `OFF` | Fetch and build OTel SDK from GitHub at configure time |

### Runtime Environment Variables

When the daemon is built with `ENABLE_OTEL=ON`, the exporter type and endpoint
are selected at runtime via environment variables (no recompile needed):

| Variable | Values | Default | Meaning |
|----------|--------|---------|---------|
| `JAMI_OTEL_EXPORTER` | `otlp_http`, `otlp_grpc`, `stdout`, `none` | `stdout` | Default exporter for **all** signals (traces, metrics, logs) |
| `JAMI_OTEL_TRACE_EXPORTER` | same as above | (from `JAMI_OTEL_EXPORTER`) | Per-signal override for **traces** |
| `JAMI_OTEL_METRICS_EXPORTER` | same as above | (from `JAMI_OTEL_EXPORTER`) | Per-signal override for **metrics** |
| `JAMI_OTEL_LOGS_EXPORTER` | same as above | (from `JAMI_OTEL_EXPORTER`) | Per-signal override for **logs** |
| `OTEL_EXPORTER_OTLP_ENDPOINT` | URL or `host:port` | `http://localhost:4318` (HTTP) / `localhost:4317` (gRPC) | OTLP collector endpoint |
| `JAMI_OTEL_METRICS_INTERVAL` | milliseconds | `30000` | How often the metric reader exports |

Per-signal variables (`JAMI_OTEL_TRACE_EXPORTER`, etc.) take precedence over the
global `JAMI_OTEL_EXPORTER`. This is useful when the collector only supports a
subset of OTLP signals.

**Quick example — send everything to a full OTLP collector:**

```bash
JAMI_OTEL_EXPORTER=otlp_http \
OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318 \
jamid -c
```

**Jaeger all-in-one — traces only (Jaeger does not accept logs or metrics):**

> **Note:** Jaeger is a tracing backend. It exposes `/v1/traces` but returns
> `404` for `/v1/logs` and `/v1/metrics`. Use per-signal overrides to disable
> the signals Jaeger cannot receive.

```bash
JAMI_OTEL_EXPORTER=otlp_http \
JAMI_OTEL_LOGS_EXPORTER=none \
JAMI_OTEL_METRICS_EXPORTER=none \
OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318 \
jamid -c
```

### Meson (Future Work)

Meson support for `ENABLE_OTEL` is planned but **not yet implemented**. Linux
distro packagers building from Meson should set `ENABLE_OTEL=OFF` (the default)
until the option is added to `meson_options.txt`. Tracking issue: see
[docs/kb/index.md](kb/index.md) — Open Questions §7.

---

## 3. Running the Daemon with OTel Debug Exporter

The stdout exporter prints all spans, metrics, and log records to standard output.
Use it during development to verify instrumentation without a collector.

In your daemon startup code (e.g., before `libjami::init()` is called):

```cpp
#ifdef ENABLE_OTEL
#include "otel/otel_init.h"
#include "otel/otel_log_bridge.h"
#endif

void startDaemon()
{
#ifdef ENABLE_OTEL
    jami::otel::OtelConfig config;
    config.service_name    = "jami-daemon";
    config.service_version = "16.0.0";

    // Stdout exporter: prints all signals to stdout (human-readable)
    config.trace_exporter   = jami::otel::OtelConfig::ExporterType::Stdout;
    config.metrics_exporter = jami::otel::OtelConfig::ExporterType::Stdout;
    config.logs_exporter    = jami::otel::OtelConfig::ExporterType::Stdout;

    // Export metrics every 15 s (lower for dev; default is 30 s)
    config.metrics_export_interval = std::chrono::milliseconds(15000);

    jami::otel::initOtel(config);

    // Forward JAMI_WARN and JAMI_ERR to OTel Logs API (default min_severity=2)
    jami::otel::installOtelLogBridge();
#endif

    // ... start libjami ...
    libjami::init(flags);
}
```

At shutdown:

```cpp
void stopDaemon()
{
    libjami::fini();

#ifdef ENABLE_OTEL
    jami::otel::removeOtelLogBridge();  // remove before shutdownOtel
    jami::otel::shutdownOtel();         // flush and clean up providers
#endif
}
```

---

## 4. Connecting to a Local OTLP Collector (Jaeger, Tempo, etc.)

### Step 1: Start Jaeger all-in-one with Docker Compose

```yaml
# docker-compose.yml
services:
  jaeger:
    image: jaegertracing/all-in-one:1.57
    environment:
      - COLLECTOR_OTLP_ENABLED=true
    ports:
      - "4317:4317"    # OTLP gRPC receiver
      - "4318:4318"    # OTLP HTTP receiver
      - "16686:16686"  # Jaeger UI
```

```bash
docker compose up -d
# Jaeger UI: http://localhost:16686
```

Alternatively, use [Grafana Tempo](https://grafana.com/docs/tempo/latest/) with the
same OTLP ports — it accepts the same protocol.

### Step 2: Configure jami-daemon to Send OTLP/gRPC

```cpp
#ifdef ENABLE_OTEL
jami::otel::OtelConfig config;
config.service_name    = "jami-daemon";
config.service_version = "16.0.0";

config.trace_exporter   = jami::otel::OtelConfig::ExporterType::OtlpGrpc;
config.metrics_exporter = jami::otel::OtelConfig::ExporterType::OtlpGrpc;
config.logs_exporter    = jami::otel::OtelConfig::ExporterType::OtlpGrpc;
config.otlp_endpoint    = "localhost:4317";  // host:port for gRPC

jami::otel::initOtel(config);
#endif
```

For OTLP/HTTP (port 4318):

```cpp
config.trace_exporter   = jami::otel::OtelConfig::ExporterType::OtlpHttp;
config.otlp_endpoint    = "http://localhost:4318";  // base URL only
// otel_init.cpp appends /v1/traces, /v1/metrics, /v1/logs automatically
```

### Step 3: Verify

After placing a test call, open Jaeger UI at `http://localhost:16686`, select
service `jami-daemon`, and search for `call.outgoing`. The full call setup trace
should appear as a tree of child spans.

---

## 5. How to Add New Instrumentation

Before instrumenting a new code path, read:
- [docs/kb/otel_semconv.md](kb/otel_semconv.md) — naming rules
- [docs/TELEMETRY_ARCHITECTURE.md](TELEMETRY_ARCHITECTURE.md) — which signal type
  belongs in which layer
- §7 and §8 of this document — what **not** to instrument and privacy rules

### 5.1 Adding a Trace Span

#### Pattern A: RAII `SpanScope` for Synchronous Operations

Use this for functions that complete before returning. The span ends automatically
when the `SpanScope` goes out of scope.

```cpp
// In the .cpp file only — NOT in the header
#ifdef ENABLE_OTEL
#include "otel/otel_context.h"
#include "otel/otel_attributes.h"
#endif

void SomeClass::doWork(const std::string& accountId, bool isVideo)
{
#ifdef ENABLE_OTEL
    jami::otel::SpanScope span("jami.mymodule", "mymodule.dowork");
    span.setAttribute(jami::otel::attr::CALL_TYPE, isVideo ? "video" : "audio");
    span.setAttribute(jami::otel::attr::ACCOUNT_TYPE, getAccountType());
    // Do NOT set accountId directly — hash it first (see §8)
#endif
    // ... actual work ...
}
```

For operations that can fail, record the error before the scope ends:

```cpp
void SomeClass::doRiskyWork()
{
#ifdef ENABLE_OTEL
    jami::otel::SpanScope span("jami.mymodule", "mymodule.risky_op");
#endif
    try {
        performRiskyOperation();
    } catch (const std::exception& e) {
#ifdef ENABLE_OTEL
        span.recordError(e.what());
#endif
        throw;
    }
}
```

#### Pattern B: `AsyncSpan` for Async Callbacks

Use when the operation starts in one function and completes in a callback (possibly
on a different thread).

```cpp
#ifdef ENABLE_OTEL
#include "otel/otel_context.h"
#endif

void SomeClass::startAsyncLookup(const std::string& key)
{
#ifdef ENABLE_OTEL
    auto asyncSpan = jami::otel::AsyncSpan::start(
        "jami.mymodule",
        "mymodule.lookup",
        opentelemetry::trace::SpanKind::kClient);
    asyncSpan.setAttribute(jami::otel::attr::DHT_OPERATION, "get");
#endif

    performLookup(key,
        // Capture the span by move into the completion lambda.
        // The span ends when the lambda is called (or destroyed on cancel).
        [
#ifdef ENABLE_OTEL
            asyncSpan = std::move(asyncSpan)
#endif
        ](bool success, const std::string& errorMsg) mutable {
#ifdef ENABLE_OTEL
            asyncSpan.end(success, success ? "" : errorMsg);
#endif
        });
}
```

#### Pattern C: Child Span from a Stored Parent Context

Use when a long-lived object (e.g., `SIPCall`) creates child spans from callbacks
on different threads. The parent context is stored at construction time and used
to parent each child.

```cpp
// In the header (.h), guarded:
#ifdef ENABLE_OTEL
    opentelemetry::context::Context callSpanCtx_ {};
#endif

// In the .cpp, at construction:
#ifdef ENABLE_OTEL
    jami::otel::SpanScope rootSpan("jami.calls", "call.outgoing",
                                   opentelemetry::trace::SpanKind::kClient);
    callSpanCtx_ = rootSpan.getContext();
    // Transfer ownership of rootSpan to a member or move it into a wrapper
    // that ends the span on OVER state.
#endif

// In a callback method (different thread):
#ifdef ENABLE_OTEL
    jami::otel::SpanScope child("jami.calls", "call.sdp.negotiate",
                                callSpanCtx_);
#endif
```

---

### 5.2 Adding Metrics

Use the lazy-initialization pattern so instruments are created once per process.
All instrument creation must happen **after** `jami::otel::initOtel()` returns.

#### Counter (monotonically increasing total)

```cpp
#ifdef ENABLE_OTEL
#include "otel/otel_init.h"
#include <opentelemetry/metrics/meter.h>

static auto& getEventsCounter() {
    static auto counter = jami::otel::getMeter("jami.mymodule")
        ->CreateUInt64Counter("jami.mymodule.events_total",
                              "Total events processed",
                              "{events}");
    return *counter;
}
#endif

void SomeClass::processEvent(const std::string& accountType)
{
#ifdef ENABLE_OTEL
    std::map<std::string, std::string> attrs = {
        {"jami.account.type", accountType}  // low-cardinality enum value only
    };
    getEventsCounter().Add(
        1, opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
#endif
    // ... actual work ...
}
```

#### Histogram (latency / size distribution)

```cpp
#ifdef ENABLE_OTEL
static auto& getSetupDurationHistogram() {
    static auto hist = jami::otel::getMeter("jami.mymodule")
        ->CreateDoubleHistogram("jami.mymodule.setup.duration",
                                "Setup time from start to ready",
                                "ms");
    return *hist;
}
#endif

void SomeClass::onSetupComplete(std::chrono::steady_clock::time_point startTime,
                                bool success,
                                const std::string& accountType)
{
#ifdef ENABLE_OTEL
    double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - startTime).count();
    std::map<std::string, std::string> attrs = {
        {"jami.account.type", accountType},
        {"outcome", success ? "success" : "failure"}
    };
    getSetupDurationHistogram().Record(
        ms, opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
#endif
}
```

#### ObservableGauge (current value, polled at export time)

Use for things like "number of active calls", "DHT routing table size", or
"number of registered accounts" — values that are sampled rather than streamed.

```cpp
#ifdef ENABLE_OTEL
static auto& getActiveSessionsGauge() {
    static auto gauge = jami::otel::getMeter("jami.mymodule")
        ->CreateInt64ObservableGauge("jami.mymodule.sessions.active",
                                     "Currently active sessions",
                                     "{sessions}");
    return *gauge;
}

// Call once during init, after initOtel():
void SomeClass::registerOtelCallbacks(SomeClass* self) {
    getActiveSessionsGauge().AddCallback(
        [](opentelemetry::metrics::ObserverResult result, void* state) {
            auto* obj = static_cast<SomeClass*>(state);
            result.Observe(static_cast<int64_t>(obj->getActiveSessionCount()));
        },
        static_cast<void*>(self));
}
#endif
```

---

### 5.3 Span Naming Rules

- **Format:** `subsystem.verb_noun` — all **lowercase**, dot-separated
- **Low cardinality:** span names must be static strings, never including dynamic
  values (IDs, addresses, user data)
- Put dynamic values in **attributes**, not in the span name

| ✅ Correct | ❌ Wrong |
|-----------|---------|
| `call.setup` | `CallSetup`, `call setup`, `CALL_SETUP` |
| `dht.peer.lookup` | `lookup_dht`, `DHTlookup` |
| `sip.invite.send` | `SendSIPInvite` |
| `account.register` | `RegisterAccount` |
| `media.session.start` | `startMediaSession` |
| `ice.candidate.gathering` | `ICE-Negotiation` |

**Reserved prefixes and their scopes:**

| Prefix | Scope | Example |
|--------|-------|---------|
| `call.*` | Call lifecycle | `call.outgoing`, `call.sdp.negotiate` |
| `dht.*` | OpenDHT operations | `dht.bootstrap`, `dht.peer.lookup` |
| `account.*` | Account lifecycle | `account.register`, `account.dht.join` |
| `sip.*` | SIP protocol messages | `sip.invite.send`, `sip.transport.tls.handshake` |
| `ice.*` | ICE negotiation | `ice.candidate.gathering`, `ice.connectivity.check` |
| `media.*` | Media session events | `media.session.start`, `media.codec.negotiate` |
| `message.*` | IM / messaging | `message.send`, `message.receive` |
| `name.*` | Name directory | `name.directory.lookup` |

---

### 5.4 Attribute Naming Rules

- Always use the constants from `src/otel/otel_attributes.h` when one exists
- Custom attributes: `jami.<subsystem>.<name>` format, all lowercase
- Values must be **low cardinality** — see §7
- **Never use raw user data** as attribute values — see §8

```cpp
// Use constants from otel_attributes.h — never inline strings!
span.setAttribute(jami::otel::attr::ACCOUNT_TYPE, "sip");    // ✅
span.setAttribute("jami.account.type", "sip");               // ✅ if no constant yet
span.setAttribute("accountType", "sip");                     // ❌ wrong format
span.setAttribute("jami.account.type", rawAccountId);        // ❌ PII!
```

---

## 6. Naming Conventions

### Span Names

| Rule | Example |
|------|---------|
| Lowercase, dot-separated | `call.sdp.negotiate` |
| Verb-noun or noun phrase | `dht.peer.lookup`, `media.session.start` |
| No PII or dynamic data | `call.setup` ← not `call.setup/user@host` |
| Noun from jami domain | `ice`, `dht`, `srtp`, `sip`, `call`, `account` |

### Metric Names

| Rule | Example |
|------|---------|
| `jami.<subsystem>.<noun>.<verb_or_unit>` | `jami.calls.active`, `jami.dht.lookup.duration` |
| Use OTel unit annotation | `jami.call.setup.duration` → unit `ms` |
| Counters: `.total` or `.count` suffix | `jami.calls.total`, `jami.dht.operations.failed` |
| Gauges: `.active`, `.size`, `.count` | `jami.calls.active`, `jami.dht.routing_table.size` |

### Attribute Keys

| Rule | Example |
|------|---------|
| Standard OTel semconv first | `error.type`, `rpc.system`, `net.transport` |
| Custom jami attributes: `jami.*` prefix | `jami.account.type`, `jami.call.direction` |
| Snake-case for readability | `jami.ice.candidate_type` |
| No user-identifiable values | See §8 |

### Attribute Values (Labels)

| Key | Allowed values | Notes |
|-----|---------------|-------|
| `jami.account.type` | `"sip"`, `"jami"` | Two states; always bounded |
| `jami.call.direction` | `"outgoing"`, `"incoming"` | Always bounded |
| `jami.call.type` | `"audio"`, `"video"` | Always bounded |
| `jami.media.type` | `"audio"`, `"video"` | Always bounded |
| `error.type` | `"timeout"`, `"rejected"`, `"ice_failure"`, `"tls_failure"`, `"auth_failure"`, `"codec_error"`, `"dht_not_found"`, `"_OTHER"` | Fixed vocabulary |
| `outcome` | `"success"`, `"failure"` | Binary |
| `jami.ice.result` | `"success"`, `"failure"`, `"timeout"`, `"turn_fallback"` | Bounded |
| `jami.media.codec` | `"opus"`, `"h264"`, `"h265"`, `"vp8"`, `"g711a"`, `"g711u"` | Bounded set from codec registry |
| `channel_type` | `"audioCall"`, `"videoCall"`, `"sync"`, `"msg"` | Bounded |

---

## 7. What NOT to Instrument (Cardinality & Performance Traps)

### Hot-Path Methods — NEVER add spans here

Adding spans (`StartSpan`, `EndSpan`, `AddEvent`) in the following methods will
cause audio glitches, video frame drops, or allocation-induced latency spikes.
**This prohibition is absolute.**

| Method | Reason |
|--------|--------|
| `ThreadLoop::process()` (all variants) | Real-time audio/video loop; 50–100 Hz |
| `AudioSender::update()` | Called for every encoded audio frame (50/s per call) |
| `VideoSender::encodeAndSendVideo()` | Called for every encoded video frame (30/s per call) |
| `AudioReceiveThread` decode loop body | RTP receive loop; blocks on socket |
| `VideoReceiveThread::decodeFrame()` | RTP decode loop; blocks on demuxer |
| `SocketPair::readCallback()` | Per-packet receive callback |
| `SocketPair::writeData()` | Per-packet send callback |
| `AudioLayer` hardware I/O callback | OS real-time constraint; no heap allocation at all |

**What IS allowed in the hot path:** lock-free counter and histogram operations with
pre-computed attribute sets (< 50 ns per call):

```cpp
// In AudioSender::update() — ONLY this kind of instrumentation:
#ifdef ENABLE_OTEL
    packetsSentCounter_.Add(1);  // lock-free atomic; no allocation
#endif
```

### Per-Packet Operations

Per-packet operations belong in **counters only**, never in histograms with
per-packet measurements and never in spans.

```cpp
// ✅ Correct: count packets (atomic, no alloc)
packets_sent_.Add(1);

// ❌ Wrong: histogram per packet (map alloc per call)
encode_duration_hist_.Record(elapsed_ns, per_packet_attrs_map);

// ❌ Wrong: span per packet (SpanData heap alloc + registration)
SpanScope span("jami.media", "rtp.packet.send");
```

### Dynamic String Values as Metric Labels

Metric labels must **never** be derived from unbounded data sources:

| ❌ Never use as a metric label | Why |
|-------------------------------|-----|
| SIP URIs (`sip:user@host.com`) | Unbounded; PII |
| IP addresses (`203.0.113.1`) | Unbounded; PII-adjacent |
| Call IDs (`abc123-uuid`) | High cardinality; each call is a unique value |
| Account IDs (raw UUID strings) | High cardinality |
| DHT `InfoHash` values | 160-bit values; unbounded |
| Device IDs | High cardinality |
| Usernames or display names | PII; unbounded |

**Cardinality example: why this breaks a metrics backend:**  
Suppose you use `call_id` as a label on a histogram. After 10,000 calls, the
metrics backend has 10,000 distinct time-series for that instrument. Most backends
(Prometheus, Thanos, Grafana Mimir) have a default cardinality limit of 10,000–
100,000 time-series per instance. Exceeding this causes OOM crashes or silent data
loss.

**Rule:** only use values from a **statically known, bounded set** as metric labels.
The allowed sets are documented in §6.

---

## 8. Privacy Rules

jami-daemon handles sensitive user identity data. All OTel instrumentation must
comply with GDPR (EU Regulation 2016/679) and Quebec Law 25 (Bill 64) personal
data protection requirements.

### Never include in spans, metrics, or logs

| Data | Classification | Rule |
|------|---------------|------|
| SIP URIs (`sip:user@host`) | PII — directly identifies a user | **OMIT** from all telemetry |
| Jami usernames / display names | PII | **OMIT** |
| Phone numbers | PII | **OMIT** |
| Peer IP addresses from SDP / ICE | PII-adjacent (reveals peer location) | **OMIT** from metrics and spans; log only at DEBUG (not forwarded to OTel) |
| Account private keys | Security-critical | **NEVER** |
| JWT tokens, session credentials, SRTP keys | Security-critical | **NEVER** |
| Archive file paths or contents | PII + security | **NEVER** |
| Raw account IDs / device IDs | High cardinality; can be linked to identity | **HASH** before use in any telemetry field |
| Raw peer DHT `InfoHash` in full | Linked to public key | Use only **first 16 hex chars** as span attribute |

### The `hashForTelemetry()` Pattern

Any identifier that might be derived from or linked to user data must be hashed
before appearing in any span attribute or metric label. Use a consistent helper:

```cpp
// src/otel/otel_init.h provides (or will provide) this utility:
#ifdef ENABLE_OTEL
// Returns SHA-256 of the input, encoded as the first 16 lowercase hex chars.
// Example: hashForTelemetry("sip:alice@example.com") → "3d7a8f2b1c4e9a0d"
std::string hashForTelemetry(std::string_view raw_id);
#endif

// Usage in instrumentation code:
#ifdef ENABLE_OTEL
span.setAttribute(jami::otel::attr::ACCOUNT_ID_HASH,
                  jami::otel::hashForTelemetry(accountId_));
#endif
```

The `attr::ACCOUNT_ID_HASH` constant (`"jami.account.id_hash"`) already signals to
any reader of the telemetry data that the value is a one-way hash, not the real ID.

### Tip: Use the `attr::*` constants liberally

Every constant in `src/otel/otel_attributes.h` has been reviewed against privacy
rules. If you need a new attribute and no constant exists yet, add it to
`otel_attributes.h` with a comment describing the privacy requirements, rather than
inlining the key string at the call site.

---

## 9. Instrumentation File Layout

### Where to put new instrumentation code

Instrumentation is **co-located with the code being instrumented**. Do not create
separate "telemetry wrapper" classes, "observability modules", or parallel class
hierarchies for existing code. This approach keeps the instrumentation close to the
implementation it describes and ensures it is updated when the implementation changes.

| What | Where |
|------|-------|
| Span creation and attribute setting for a class | In the `.cpp` file of that class |
| `#ifdef ENABLE_OTEL` span context member variables | In the `.h` file, inside `#ifdef ENABLE_OTEL` guards |
| Metric instrument singletons (lazy static helpers) | In the `.cpp` file next to the code that records measurements |
| New `jami.*` attribute key constants | In `src/otel/otel_attributes.h` |
| Per-subsystem metric struct (like `CallMetrics`) | In the subsystem's own directory (e.g., `src/call_metrics.h`) |
| OTel init / provider management | Only in `src/otel/otel_init.h` / `otel_init.cpp` |
| Log bridge | Only in `src/otel/otel_log_bridge.h` / `otel_log_bridge.cpp` |

### Pattern for adding span context to an existing class

```cpp
// ── In the header: myclass.h ──────────────────────────────────────────────────
class MyClass {
public:
    // ... existing public interface ...

private:
    // ... existing private members ...

#ifdef ENABLE_OTEL
    // Span context saved at construction; used to parent child spans from
    // callbacks that run on different threads.
    opentelemetry::context::Context myOperationCtx_ {};
    // Live span for a long-running async operation (e.g., registration).
    std::shared_ptr<opentelemetry::trace::Span> myOperationSpan_ {};
#endif
};

// ── In the implementation: myclass.cpp ───────────────────────────────────────
#ifdef ENABLE_OTEL
#include "otel/otel_context.h"
#include "otel/otel_attributes.h"
#endif

MyClass::MyClass()
{
#ifdef ENABLE_OTEL
    jami::otel::SpanScope span("jami.mymodule", "myclass.init");
    myOperationCtx_ = span.captureContext();
#endif
    // ... existing constructor body ...
}
```

### Do NOT do this

```cpp
// ❌ Separate telemetry wrapper class — anti-pattern
class MyClassTelemetry {
public:
    void onInit() { /* spans here */ }
    void onError() { /* spans here */ }
};

class MyClass {
    MyClassTelemetry telemetry_;  // ❌ creates a separate concern
};
```

```cpp
// ❌ Separate telemetry subdirectory
// src/telemetry/myclass_telemetry.cpp  ← do NOT create this
```

The reason: when `MyClass` is modified, developers will forget to update a separate
telemetry class. Co-location ensures the instrumentation is reviewed as part of
any code change to the class.
