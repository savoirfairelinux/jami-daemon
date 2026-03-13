# jami-daemon OpenTelemetry Knowledge Base — Master Index

## Status: reviewed
## Last Updated: 2026-03-13

---

## Overview

This knowledge base documents the complete analysis, design, and implementation of
OpenTelemetry (OTel) observability instrumentation for `jami-daemon` (also known as
`libjami`). It was produced during a multi-phase engineering initiative to bring
production-grade distributed tracing, structured metrics, and log forwarding to the
Jami P2P communication daemon. The KB covers: the existing daemon architecture (12
subsystems, 6 architectural layers), the OTel C++ SDK API reference, per-subsystem
integration plans with exact code injection points, the CMake build integration, and
privacy/cardinality rules that govern every instrumentation decision. Together these
documents form the authoritative reference for any engineer adding, reviewing, or
operating OTel instrumentation in jami-daemon.

---

## Quick Navigation

| Goal | Start here |
|------|-----------|
| **Add new instrumentation** (traces or metrics) | [OPENTELEMETRY.md](../OPENTELEMETRY.md) §5, then the relevant `integration_*.md` for your subsystem |
| **Understand the overall architecture** | [subsystem_overview.md](subsystem_overview.md) → [TELEMETRY_ARCHITECTURE.md](../TELEMETRY_ARCHITECTURE.md) |
| **Understand the OTel C++ API** | [otel_cpp_sdk.md](otel_cpp_sdk.md) → [otel_traces.md](otel_traces.md) → [otel_metrics.md](otel_metrics.md) → [otel_logs.md](otel_logs.md) |
| **Understand naming / semantic conventions** | [otel_semconv.md](otel_semconv.md) |
| **Build jami with OTel enabled** | [otel_cmake_integration.md](otel_cmake_integration.md) → [OPENTELEMETRY.md](../OPENTELEMETRY.md) §2 |
| **Privacy and cardinality rules** | [OPENTELEMETRY.md](../OPENTELEMETRY.md) §7–8; also in each `layer_*.md` under «Cardinality Warnings» |
| **DHT / ICE instrumentation details** | [integration_dht_layer.md](integration_dht_layer.md) + [layer_network_dht.md](layer_network_dht.md) |
| **Media pipeline instrumentation details** | [integration_media_pipeline.md](integration_media_pipeline.md) + [layer_data_plane.md](layer_data_plane.md) |
| **Call manager instrumentation details** | [integration_call_manager.md](integration_call_manager.md) + [layer_signaling_control.md](layer_signaling_control.md) |

---

## Knowledge Base Files

### Build System & Dependencies

| File | Description | Status |
|------|-------------|--------|
| [build_system.md](build_system.md) | CMake and Meson build systems, dependency tiers (`contrib/`, pkg-config, system), key options, MSVC notes | draft |
| [otel_cmake_integration.md](otel_cmake_integration.md) | CMake `FetchContent` and `find_package` approaches for OTel C++ SDK v1.25.0; required targets; build options; gRPC/protobuf dependencies | draft |

---

### Subsystem Analysis

| File | Subsystem | Instrumentation Value | Status |
|------|-----------|-----------------------|--------|
| [subsystem_overview.md](subsystem_overview.md) | All subsystems — cross-cutting tables & architecture summary | High | draft |
| [subsystem_call_manager.md](subsystem_call_manager.md) | Call Manager | Very High | complete |
| [subsystem_dht_layer.md](subsystem_dht_layer.md) | DHT Layer | High | draft |
| [subsystem_media_pipeline.md](subsystem_media_pipeline.md) | Media Pipeline | High (metrics only) | draft |
| [subsystem_account_management.md](subsystem_account_management.md) | Account Management | High | draft |
| [subsystem_conference.md](subsystem_conference.md) | Conference / Swarm | Medium | draft |
| [subsystem_logging.md](subsystem_logging.md) | Logging Infrastructure | Very High (as infrastructure) | draft |
| [subsystem_connectivity.md](subsystem_connectivity.md) | Connectivity / Transport | High | draft |
| [subsystem_im_messaging.md](subsystem_im_messaging.md) | IM / Messaging | Medium | draft |
| [subsystem_data_transfer.md](subsystem_data_transfer.md) | Data Transfer | Low–Medium | draft |
| [subsystem_plugin_system.md](subsystem_plugin_system.md) | Plugin System | Low | draft |
| [subsystem_certificate_pki.md](subsystem_certificate_pki.md) | Certificate / PKI | Medium | draft |
| [subsystem_config_persistence.md](subsystem_config_persistence.md) | Config Persistence | Low | draft |

---

### Integration Plans

| File | Subsystem | Signals | Status |
|------|-----------|---------|--------|
| [integration_call_manager.md](integration_call_manager.md) | Call Manager | Traces + Metrics | draft |
| [integration_dht_layer.md](integration_dht_layer.md) | DHT + ICE + NameDirectory | Traces + Metrics | draft |
| [integration_media_pipeline.md](integration_media_pipeline.md) | Media Pipeline | Metrics only (sparse traces) | draft |
| [integration_account_management.md](integration_account_management.md) | Account Management | Traces + Metrics | draft |

---

### OTel API Reference

| File | Topic | Status |
|------|-------|--------|
| [otel_cpp_sdk.md](otel_cpp_sdk.md) | SDK architecture (API vs SDK), complete initialization examples (stdout + OTLP), provider lifetime | draft |
| [otel_traces.md](otel_traces.md) | Tracer acquisition, `StartSpan`, attributes, events, status, span kinds, context propagation | draft |
| [otel_metrics.md](otel_metrics.md) | Meter acquisition, all 5 instrument types, attribute sets, export interval, histogram Views | draft |
| [otel_logs.md](otel_logs.md) | LoggerProvider, `LogRecord` structure, severity mapping, trace context injection, jami bridge design | draft |
| [otel_semconv.md](otel_semconv.md) | OTel semantic conventions 1.40.0 relevant to jami: span naming, RPC, network, messaging, error, `jami.*` namespace | draft |

---

### Architectural Layers

| File | Layer | Primary Signals | Status |
|------|-------|-----------------|--------|
| [layer_public_api.md](layer_public_api.md) | Layer 1 — Public API / IPC | Traces (root spans) + Metrics (request rate/errors) | draft |
| [layer_account_identity.md](layer_account_identity.md) | Layer 2 — Account & Identity | Traces (async spans) + Metrics (registration health) | draft |
| [layer_signaling_control.md](layer_signaling_control.md) | Layer 3 — Signaling & Control | Traces (multi-step spans) + Metrics (call setup SLO) | draft |
| [layer_data_plane.md](layer_data_plane.md) | Layer 4 — Data Plane / Media | **Metrics ONLY** (Counters + Histograms; no hot-path spans) | draft |
| [layer_network_dht.md](layer_network_dht.md) | Layer 5 — Network & DHT | Traces (async spans) + Metrics (connectivity health) | draft |
| [layer_system_platform.md](layer_system_platform.md) | Layer 6 — System / Platform | Metrics (resource gauges) + Log Bridge | draft |

---

## Implementation Status

### Phase 4 Implementation Checklist

- [x] `src/otel/` bootstrap module created (`otel_init.h`, `otel_init.cpp`)
- [x] `OtelConfig` struct with `ExporterType` enum defined
- [x] `initOtel()` / `shutdownOtel()` entry points implemented
- [x] `getTracer()` / `getMeter()` / `getOtelLogger()` helper functions implemented
- [x] `CMake/Findopentelemetry-cpp.cmake` find module created
- [x] `ENABLE_OTEL` build flag added to `CMakeLists.txt`
- [x] `src/otel/otel_context.h` — `SpanScope` RAII guard + `AsyncSpan` implemented
- [x] `src/otel/otel_attributes.h` — all standard `jami.*` attribute key constants defined
- [x] `src/otel/otel_log_bridge.h/.cpp` — `installOtelLogBridge()` / `removeOtelLogBridge()` implemented; min-severity filter; trace context injection
- [x] `src/call_metrics.h` — `CallMetrics` struct defined: `active_calls`, `total_calls`, `failed_calls`, `setup_duration`, `call_duration`
- [ ] Call manager instrumented with real spans (`call.outgoing`, `call.incoming`, child spans) — **Phase 5 work**
- [ ] DHT layer instrumentation (`dht.bootstrap`, `dht.peer.lookup`, `dht.channel.open`) — **Phase 5 work**
- [ ] Account management instrumentation (`account.register`, `account.dht.join`) — **Phase 5 work**
- [ ] Media pipeline metrics (`jami.media.rtp.*`, `jami.media.*.bitrate`) — **Phase 5 work**
- [ ] ICE setup spans (`call.ice.init`, `call.ice.negotiate`) — **Phase 5 work**
- [ ] W3C TraceContext propagation in SIP INVITE / DHT invite payload — **Phase 6 work**
- [ ] Meson build system support for `ENABLE_OTEL` — **Phase 6 work**

---

## Open Questions (Cross-Cutting)

The following questions remain unresolved and affect multiple subsystems:

1. **`initOtel()` call site** — `OtelConfig` is defined but the integration into `jamid/main.cpp` (or `Manager::init()`) has not been finalized. The daemon needs a config-file-driven mechanism to set `otlp_endpoint`, `trace_exporter`, and `metrics_export_interval` at runtime without recompilation.

2. **Thread safety of span handle storage** — integration plans recommend storing `shared_ptr<Span>` members in `SIPCall`, `Account`, and `JamiAccount`. The exact mutex rules (which existing guards protect these members) must be documented per-class before implementation begins.

3. **`hashForTelemetry()` implementation** — multiple KB files reference this function for hashing account IDs, call IDs, and peer IDs before use in telemetry. The canonical implementation (SHA-256, 16-hex-char truncation) must be placed in `src/otel/` or `src/string_utils` and referenced uniformly.

4. **Sampling strategy** — no sampling policy has been chosen for production. At > 5 concurrent calls, unsampled head-based tracing may overwhelm any reasonable OTLP collector. Options: `ParentBased(TraceIdRatioBased(0.1))`, or tail sampling at the collector. The decision should be documented in `TELEMETRY_ARCHITECTURE.md`.

5. **W3C TraceContext over the wire** — `integration_dht_layer.md` proposes injecting `traceparent` into the DHT invite JSON. This requires both sender and receiver to support OTel and agree on the carrier format. This is Phase 6 scope but the JSON schema change should be planned now to avoid a future breaking change.

6. **dhtnet instrumentation boundary** — `dhtnet` is an external library. Several integration plans call for child spans inside ICE negotiation and TLS handshake phases that live inside `dhtnet`. These either require a callback/hook API in `dhtnet` or accepting coarser granularity. This needs resolution before Phase 5 DHT instrumentation work begins.

7. **Meson support** — `build_system.md` identifies Meson as the primary build system for Linux distributions. The `ENABLE_OTEL` option and find-module strategy must be ported to `meson.build` for distro packages to ship OTel-enabled builds.

8. **Android and iOS OTel SDK supply** — the `contrib/` vendoring system handles most deps for mobile, but the OTel C++ SDK is not currently in `contrib/src/`. Adding it (with `WITH_OTLP_GRPC=OFF` for mobile) is a prerequisite for enabling OTel on Android/iOS targets.
