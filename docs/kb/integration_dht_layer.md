# Integration Plan: DHT Layer

## Status: draft

## Last Updated: 2026-03-13

---

## Overview

This document specifies how OpenTelemetry traces and metrics should be injected into the DHT layer of `jami-daemon`. The DHT layer encompasses `JamiAccount`, `AccountManager`, `NameDirectory`, and the `dhtnet::ConnectionManager` pathway that produces ICE-negotiated TLS channels. These are the code paths that dominate end-to-end call setup latency and determine connectivity success/failure in restricted network environments.

---

## Proposed Span Hierarchy

```
call.outgoing  (ROOT — from SIPAccountBase / call_manager layer)
└── dht.peer.lookup  (SpanKind::CLIENT — injected in JamiAccount::startOutgoingCall)
    └── dht.node.query  (SpanKind::INTERNAL — per DHT node queried, emitted by OpenDHT callback)
    └── name.directory.lookup  (SpanKind::CLIENT — only when NameDirectory fallback is used)

dht.bootstrap  (ROOT, SpanKind::INTERNAL — injected in JamiAccount::doRegister_)
    └── dht.identity.announce (INTERNAL — closed in identityAnnouncedCb)

dht.presence.subscribe  (INTERNAL — injected in JamiAccount::trackPresence, per buddy)

dht.channel.open  (SpanKind::CLIENT — injected in JamiAccount::requestSIPConnection)
    └── ice.candidate.gathering  (INTERNAL — from connectionManager_->getIceOptions callback start)
    └── ice.connectivity.check   (INTERNAL — ICE negotiation phase inside dhtnet)
    └── tls.handshake            (INTERNAL — dhtnet TLS session setup)

dht.publish  (SpanKind::INTERNAL — device announce / value put operations)
```

**Span naming convention:** `<subsystem>.<operation>` in lower-snake-case, no PII in span names.

---

## Proposed Metric Instruments

| Instrument Name | Type | Unit | Description | Labels |
|---|---|---|---|---|
| `jami.dht.bootstrap.duration` | Histogram | ms | Time from `dht_->run()` call to `identityAnnouncedCb(ok=true)` | `account_id_hash`, `proxy_enabled` |
| `jami.dht.lookups.total` | Counter | {lookups} | Number of `forEachDevice` / `findCertificate` DHT get operations initiated | `account_id_hash`, `result` (`found`/`not_found`/`timeout`) |
| `jami.dht.lookup.duration` | Histogram | ms | Duration of `AccountManager::forEachDevice()` or `findCertificate()` from call to final callback | `account_id_hash`, `result` |
| `jami.dht.peers.connected` | UpDownCounter | {peers} | Net count of currently active dhtnet `ChannelSocket` connections | `account_id_hash`, `channel_type` (`audioCall`/`videoCall`/`sync`/`msg`) |
| `jami.dht.operations.failed` | Counter | {ops} | Failed DHT operations (bootstrap fail, lookup timeout, announce fail) | `account_id_hash`, `operation` (`bootstrap`/`lookup`/`announce`/`listen`) |
| `jami.ice.setup.duration` | Histogram | ms | Duration of full ICE negotiation inside `dhtnet::ConnectionManager::connectDevice()` | `account_id_hash`, `result` (`success`/`failure`) |
| `jami.ice.setup.success_rate` | Derived (ratio) | — | `jami.ice.setup.duration{result=success}` count / `jami.ice.setup.duration` total count; computed by analysis layer, not a native instrument | — |
| `jami.dht.storage.items` | ObservableGauge | {items} | Number of values stored in routing table (observable; polled from `dht_->getNodesStats()` or similar) | `account_id_hash` |
| `jami.dht.presence.subscriptions` | UpDownCounter | {subs} | Number of active `dht_->listen<DeviceAnnouncement>` subscriptions | `account_id_hash` |
| `jami.name.directory.lookup.duration` | Histogram | ms | Duration of `NameDirectory::lookupName()` / `lookupAddress()` HTTP request | `server`, `result` (`found`/`not_found`/`error`) |
| `jami.dht.channel.open.duration` | Histogram | ms | Time from `requestSIPConnection()` to `onConnectionReady()` for a given device | `account_id_hash`, `channel_type`, `result` |

---

## Code Injection Points

### Span: `dht.bootstrap`

| Property | Value |
|---|---|
| **Start** | `JamiAccount::doRegister_()` — immediately after `dht_->run(port, config, context)` (~line 1970 of `jamiaccount.cpp`) |
| **End (success)** | `JamiAccount::initDhtContext()` — inside `context.identityAnnouncedCb` lambda when `ok == true` (~line 2132) |
| **End (failure)** | Same lambda when `ok == false`; or in the `catch` block of `doRegister_()` (~line 2019) |
| **Span attributes** | `dht.port` = `dhtBoundPort_`, `dht.proxy_enabled` = `conf.proxyEnabled`, `dht.bootstrap_nodes` = count of `loadBootstrap()` list |
| **Class** | `JamiAccount` |

The `identityAnnouncedCb` captures the span context by `std::move` into the lambda. Since the callback fires on an OpenDHT internal thread, no lock is held when the span is ended.

---

### Span: `dht.peer.lookup`

| Property | Value |
|---|---|
| **Start** | `JamiAccount::startOutgoingCall()` — after `dht::InfoHash peer_account(toUri)` construction, before `accountManager_->forEachDevice()` call (~line 805 of `jamiaccount.cpp`) |
| **End (all devices found)** | Inside the `end` callback of `accountManager_->forEachDevice()` — the `[wCall](bool ok)` lambda (~line 812) |
| **Span attributes** | `dht.peer.id_hash` = SHA-256 of `toUri` (never raw URI), `dht.devices_attempted` = count from `sendRequest` invocations |
| **SpanKind** | `SpanKind::CLIENT` (outbound DHT query) |
| **Parent** | `call.outgoing` span if one exists in the active context; otherwise ROOT |
| **Class** | `JamiAccount` |

---

### Span: `name.directory.lookup`

| Property | Value |
|---|---|
| **Start** | `JamiAccount::newOutgoingCallHelper()` — in the `catch` branch, before `NameDirectory::lookupUri()` call (~line 462 of `jamiaccount.cpp`) |
| **End** | Inside the `LookupCallback` lambda passed to `lookupUri`, before `runOnMainThread` dispatch (~line 465) |
| **Span attributes** | `name.server` = `config().nameServer`, `name.response` = `found`/`notFound`/`invalidResponse`/`error` |
| **SpanKind** | `SpanKind::CLIENT` |
| **Class** | `JamiAccount` / `NameDirectory` |

For the `NameDirectory` class itself, a simpler alternative is to inject at `NameDirectory::lookupName()` and `lookupAddress()` entry, closing in the `dht::http::Request` completion callback. This keeps the instrumentation inside `NameDirectory` rather than in every call site.

---

### Span: `dht.channel.open`

| Property | Value |
|---|---|
| **Start** | `JamiAccount::requestSIPConnection()` (~line 3813 of `jamiaccount.cpp`) — when no cached channel exists and `connectionManager_->connectDevice()` is about to be called |
| **End (success)** | `JamiAccount::onConnectionReady()` (~line 2240) — after `ChannelSocket` is delivered and SIP transport is set up |
| **End (failure)** | ICE or TLS error callback from `dhtnet::ConnectionManager`; or call `onFailure()` branch in `sendRequest` lambda |
| **Span attributes** | `dht.peer.device_id_hash` = SHA-256 of `deviceId.toString()`, `channel.type` = `audioCall`/`videoCall`/`sync`/`msg`, `ice.result` = `success`/`failure` |
| **SpanKind** | `SpanKind::CLIENT` |
| **Class** | `JamiAccount` |

Child spans `ice.candidate.gathering` and `ice.connectivity.check` should ideally be injected inside `dhtnet::ConnectionManager` (a separate library). If modifying dhtnet is possible, inject them in `ICETransport::initIce()` and the ICE state-change callbacks. If not, approximate with single `ice.negotiation` span from `getIceOptions()` callback entry to channel-ready.

---

### Span: `dht.presence.subscribe`

| Property | Value |
|---|---|
| **Start** | `JamiAccount::trackPresence()` — before `dht_->listen<DeviceAnnouncement>(h, cb)` (~line 1789) |
| **End** | Immediately after `listen<>` returns (subscription is registered); the span represents the act of subscribing, not the subscription lifetime |
| **Span attributes** | `dht.key_hash` = `h.toString()` (the InfoHash IS the public key hash — safe to emit as-is since it is already a hash of a public key, not a username) |
| **Class** | `JamiAccount` |

---

### Span: `dht.publish`

| Property | Value |
|---|---|
| **Start / End** | Wrap `dht_->put()` calls inside `AccountManager::startSync()` / `ArchiveAccountManager`; these are short-lived confirm-on-put operations |
| **Span attributes** | `dht.value_type` = type name (e.g., `DeviceAnnouncement`), omit key |
| **Class** | `AccountManager` subclasses |

---

## Context Propagation Strategy

The DHT layer is deeply asynchronous with three distinct execution contexts:

1. **OpenDHT callback threads** — deliver `listen<>` results, `get<>` results, and `identityAnnouncedCb`.
2. **ASIO io_context thread** — delivers `NameDirectory` HTTP completions.
3. **Main thread** — `JamiAccount` state mutations, call lifecycle, `Manager` dispatch.

### Recommended approach

- **Span context carried by `shared_ptr`**: Since lambdas already capture `std::weak_ptr<JamiAccount>` or `std::shared_ptr<SIPCall>`, add the active `opentelemetry::trace::Span` (or a `opentelemetry::context::Context` snapshot) as an additional capture in the same lambda. Example:

  ```cpp
  // In startOutgoingCall():
  auto lookupSpan = tracer->StartSpan("dht.peer.lookup", ...);
  auto ctx = opentelemetry::trace::SetSpan(
      opentelemetry::context::RuntimeContext::GetCurrent(), lookupSpan);

  accountManager_->forEachDevice(
      peer_account,
      [ctx, lookupSpan, ...](const auto& dev) {
          // child work — use ctx as parent context
      },
      [lookupSpan](bool ok) {
          lookupSpan->SetStatus(ok ? StatusCode::kOk : StatusCode::kError);
          lookupSpan->End();
      });
  ```

- **No `RuntimeContext` propagation across threads**: Do not rely on `RuntimeContext::GetCurrent()` inside OpenDHT callbacks — it will return an empty context since the thread was not set up by JAMI code. Always capture the context explicitly in the lambda closure.

- **Correlation with `call.outgoing`**: The `call.outgoing` span (if instrumented at the call layer) should pass its `SpanContext` down to `startOutgoingCall()` so the `dht.peer.lookup` span can be constructed as a child. The `SIPCall` object is already passed by `shared_ptr`; the simplest approach is to add a `TraceContext` field to `SIPCall` (or stash it in a `std::map<callId, SpanContext>` guarded by the call mutex).

- **W3C TraceContext over-the-wire**: When the call invitation payload (`application/invite+json`) is sent over the dhtnet channel, inject the `traceparent` / `tracestate` W3C headers into the JSON body. The receiving side can then parent its `dht.channel.open` span on the caller's `dht.peer.lookup`. This enables cross-peer distributed traces.

---

## Privacy Considerations

The DHT layer handles cryptographic identifiers that are directly linked to user identity:

| Data | Risk | Required handling |
|---|---|---|
| `toUri` (raw Jami URI / DHT InfoHash string) | Directly identifies a JAMI user | **HASH before use in any attribute** — use `SHA-256(uri)` truncated to 16 hex chars as `dht.peer.id_hash` |
| `deviceId.toString()` (`dht::PkId`) | Identifies a specific user device | **HASH** — `SHA-256(deviceId)` as `dht.peer.device_id_hash` |
| `dht::InfoHash h` in `trackPresence` | Hash of account public key — already one-way | **Safe to emit as-is** (it is already `SHA-1(pubkey)`; not directly reversible to username without directory) |
| `regName` from `NameDirectory` | Human-readable username | **OMIT** from all span attributes and metric labels |
| `config().nameServer` URL | Server hostname only | **Safe to emit** |
| SDP / ICE candidates | May contain local/public IP | **OMIT** — never attach SDP content to spans |
| Call ID (`call->getCallId()`) | Pseudo-random UUID | **Safe to use** for span correlation within a session |

All metric label cardinality must be bounded: use `account_id_hash = SHA-256(accountId)[0:16]` as a low-cardinality label, never the raw account ID.

---

## Thread Safety Notes

### ASIO strand requirements

- There are **no ASIO strands** wrapping DHT callbacks in the current code. Span operations (`Start`, `End`, `SetAttribute`) from the OpenTelemetry C++ SDK are thread-safe under the `BatchSpanProcessor` / `SimpleSpanProcessor` models, so direct use from any thread is safe for the span itself.
- However, any JAMI-side bookkeeping associated with span handles (e.g., storing a `shared_ptr<Span>` in a `std::map`) must be protected by the same mutex that already protects the associated data structure (`buddyInfoMtx` for presence spans, `sipConnsMtx_` for channel spans, `pendingCallsMutex_` for call spans).

### Span handle lifetimes

- Spans for async operations (e.g., `dht.peer.lookup`) must be heap-allocated and referenced by `shared_ptr` because the operation spans multiple lambda invocations on different threads. `std::shared_ptr<opentelemetry::trace::Span>` is the correct handle type.
- Do not store spans in stack-local `auto` variables when the span will be ended in a later async callback — this will cause use-after-free.

### `connManagerMtx_` interaction

- `initConnectionManager()` acquires a unique lock on `connManagerMtx_`. Instrumenting `dht.channel.open` spans must not call any span methods while holding this lock, since span processors may themselves acquire internal locks and inversion is possible. Capture the span `shared_ptr` before acquiring the lock, and end it after releasing.

---

## Risks & Complications

1. **OpenDHT is an opaque library**: The inner per-node `dht.node.query` spans require either modifying OpenDHT to emit callbacks or using the `DhtRunner::get()` completion count as a proxy metric. OpenDHT does not currently expose per-node query timing.

2. **dhtnet is a separate library**: ICE sub-spans (`ice.candidate.gathering`, `ice.connectivity.check`) require changes to `dhtnet::ConnectionManager` and `dhtnet::IceTransport` — files not in this repo. These can be approximated with a single `dht.channel.open` span that covers the full ICE + TLS duration.

3. **Fan-out to multiple devices**: `startOutgoingCall()` fans out to N device sub-calls via `forEachDevice()`. Each device gets a separate `dht.channel.open` span; they should all be children of the same `dht.peer.lookup` span to represent the parallel nature. Use `SpanContext` from the parent and pass it as `start_options.parent` for each child.

4. **`deviceAnnounced_` gating**: `onAccountDeviceAnnounced()` gates `ConversationModule::bootstrap()`. A `dht.bootstrap` span that ends at `identityAnnouncedCb` will miss conversation bootstrap latency, which can dominate total registration time. Consider extending the span or emitting a `dht.conv.bootstrap` child.

5. **Proxy mode**: When `config.proxyEnabled` is true, the DHT node operates differently (push-notification-based, not direct DHT). Span attributes should include `dht.mode = direct|proxy` to distinguish these code paths in analysis.

6. **High-frequency presence callbacks**: `dht_->listen<DeviceAnnouncement>` callbacks fire on every device announce (periodic, default ~10 min TTL). Emitting a span per callback would generate excessive telemetry. Use metrics only (counters, UpDownCounter) for presence; reserve spans for explicit subscription start/cancel.

7. **`runOnMainThread()` delay**: The gap between a DHT callback arrival on the OpenDHT thread and the `runOnMainThread()` execution can add tens of milliseconds under load. If the `dht.peer.lookup` span is closed inside `runOnMainThread()`, it will include scheduler latency that is not DHT-related. Consider closing spans directly on the OpenDHT thread for accuracy.

---

## Source References

| Symbol | File | Lines |
|---|---|---|
| `JamiAccount::doRegister_()` | `src/jamidht/jamiaccount.cpp` | ~1896–2023 |
| `JamiAccount::initDhtContext()` | `src/jamidht/jamiaccount.cpp` | ~2075–2140, `identityAnnouncedCb` at ~2127 |
| `JamiAccount::trackPresence()` | `src/jamidht/jamiaccount.cpp` | ~1783–1823 |
| `JamiAccount::startOutgoingCall()` (fan-out to devices) | `src/jamidht/jamiaccount.cpp` | ~664–829 |
| `JamiAccount::newOutgoingCall()` + `newOutgoingCallHelper()` | `src/jamidht/jamiaccount.cpp` | ~400–487 |
| `JamiAccount::requestSIPConnection()` | `src/jamidht/jamiaccount.cpp` | ~3813+ |
| `JamiAccount::onConnectionReady()` | `src/jamidht/jamiaccount.cpp` | ~2240+ |
| `JamiAccount::onICERequest()` | `src/jamidht/jamiaccount.cpp` | ~2193+ |
| `JamiAccount::onAccountDeviceAnnounced()` | `src/jamidht/jamiaccount.cpp` | ~2185–2192 |
| `AccountManager::forEachDevice()` | `src/jamidht/account_manager.h` | ~204 (declaration) |
| `AccountManager::findCertificate()` | `src/jamidht/account_manager.h` | ~240 (declaration) |
| `AccountManager::startSync()` | `src/jamidht/account_manager.h` | ~141 (declaration) |
| `AccountInfo` struct | `src/jamidht/account_manager.h` | ~44–57 |
| `JamiAccount::BuddyInfo` struct | `src/jamidht/jamiaccount.cpp` | ~156–170 |
| `JamiAccount::PendingCall` struct | `src/jamidht/jamiaccount.cpp` | ~172–183 |
| `NameDirectory::lookupName()` / `lookupAddress()` | `src/jamidht/namedirectory.h` | ~80–84 |
| `NameDirectory::lookupUri()` | `src/jamidht/namedirectory.cpp` | ~84–100 |
| `JamiAccount::DHT_PORT_RANGE` | `src/jamidht/jamiaccount.h` | ~89 |
| `JamiAccount::initConnectionManager()` | `src/jamidht/jamiaccount.cpp` | ~4384+ |

---

## Open Questions

1. **dhtnet instrumentation boundary**: Should ICE sub-spans live in `dhtnet` (a separate library) or should `JamiAccount::onICERequest()` / `onConnectionReady()` timestamps be used as a coarser proxy? Decision needed before implementation starts.

2. **Tracer provider lifecycle**: OTel SDK initialization (resource, exporter, `TracerProvider`) is not yet present in `jami-daemon`. Should it be initialized in `Manager::init()` or in a dedicated `OtelManager` singleton? See `integration_plan_otel_metrics.md` for the global strategy.

3. **`dht.node.query` feasibility without OpenDHT changes**: Is it acceptable to never instrument per-node queries, relying solely on end-to-end `dht.peer.lookup` timing? Or should we request an upstream hook in OpenDHT?

4. **Metric export interval vs. DHT operation frequency**: `forEachDevice` is called per outgoing call (low frequency). `trackPresence` callbacks are periodic (every ~10 min per contact). Are per-operation histograms meaningful at typical fleet scale, or are cumulative counters sufficient?

5. **W3C `traceparent` in `invite+json` payload**: Does adding a `traceparent` field to the MIME type `application/invite+json` body risk breakage in legacy Jami clients that do not tolerate unknown JSON keys? Should it be opt-in via a capability negotiation flag?

6. **Sampling strategy**: Full trace sampling for every DHT lookup could generate high volume in multi-account daemon deployments. Should `dht.bootstrap` always be sampled (low frequency, high value) while `dht.presence.subscribe` callbacks use tail-based sampling or be metrics-only?
