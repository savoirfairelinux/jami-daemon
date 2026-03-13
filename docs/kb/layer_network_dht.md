# Layer 5 — Network & DHT Layer

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 5 is the **decentralised connectivity foundation** that enables JAMI accounts to find, authenticate, and reach peers without any central signalling server. It encompasses the OpenDHT distributed hash table node, the `dhtnet` connection manager that negotiates ICE-based TLS channels, STUN/TURN server interaction, UPnP port mapping, and the name directory HTTP service. For SIP accounts it also includes the PJSIP network event thread, SIP transport lifecycle, and ICE candidate exchange embedded in SDP.

Layer 5 spans sit downstream of the call setup spans in Layer 3 — a `dht.channel.open` span is a sibling or child of `call.outgoing`, depending on whether the channel was pre-established.

### Constituting Files and Classes

#### OpenDHT Node (JAMI accounts)

| Class | File | Role |
|---|---|---|
| `dht::DhtRunner` | `<opendht/dhtrunner.h>` (external) | DHT node lifecycle: `run()`, `bootstrap()`, `listen<T>()`, `get<T>()`, `put()`, `join()` |
| `JamiAccount` | `src/jamidht/jamiaccount.h` / `.cpp` | Owns `DhtRunner` as `dht_`; all DHT operations initiated here |
| `AccountManager` | `src/jamidht/account_manager.h` / `.cpp` | `forEachDevice()`, `findCertificate()` — DHT `get<T>` wrappers; `startSync()` triggers identity announce |

#### dhtnet Connection Manager

| Class | File | Role |
|---|---|---|
| `dhtnet::ConnectionManager` | `<dhtnet/connectionmanager.h>` (external) | Creates ICE-over-DHT authenticated TLS channels; `connectDevice()`, `onConnectionReady()` |
| `dhtnet::IceTransport` | (dhtnet external) | ICE agent: gathers host/srflx/relay candidates; connectivity checks; STUN/TURN allocation |
| `dhtnet::IceTransportFactory` | (dhtnet external) | Factory for `IceTransport` instances; owned by `Manager` |
| `ChanneledTransport` | `src/jamidht/channeled_transport.h` / `.cpp` | Bridges `dhtnet::ChannelSocket` as a PJSIP transport for SIP-over-DHT |
| `AuthChannelHandler` | `src/jamidht/auth_channel_handler.h` / `.cpp` | Mutual-authentication handshake on new dhtnet channel open |

#### Name Directory (HTTP)

| Class | File | Role |
|---|---|---|
| `NameDirectory` | `src/jamidht/namedirectory.h` / `.cpp` | HTTP client for `ns.jami.net`; `lookupAddress()`, `lookupName()`, `registerName()` via `dht::http::Request` |

#### SIP Transport (SIP accounts — Layer 5 portion)

| Class | File | Role |
|---|---|---|
| `SipTransport` / `SipTransportBroker` | `src/sip/siptransport.h` / `.cpp` | PJSIP transport lifecycle (UDP/TCP/TLS); TLS factory; state listener |
| `SIPVoIPLink` | `src/sip/sipvoiplink.h` / `.cpp` | PJSIP network event thread (`sipThread_`); ICE embed in SDP |

#### Connectivity Utilities

| Class | File | Role |
|---|---|---|
| IP utilities | `src/connectivity/ip_utils.h` / `.cpp` | Interface enumeration, NAT detection |
| `dhtnet::upnp` | (dhtnet external) | UPnP / NAT-PMP port mapping; asio-based async |

---

## DHT Operation Summary

The following `dht::DhtRunner` operations in `JamiAccount` are the primary trace targets:

| Operation | Method | Latency Range | Trace Target |
|---|---|---|---|
| `dht_->run()` + bootstrap | `doRegister_()` | 500 ms – 30 s | `dht.bootstrap` root span |
| `dht_->put()` (identity announce) | `AccountManager::startSync()` | 100 ms – 5 s | child of `dht.bootstrap` |
| `dht_->get<T>()` (device discovery) | `AccountManager::forEachDevice()` | 100 ms – 10 s | `dht.peer.lookup` span |
| `dht_->get<T>()` (cert lookup) | `AccountManager::findCertificate()` | 100 ms – 5 s | `dht.cert.lookup` span |
| `dht_->listen<DeviceAnnouncement>()` | `JamiAccount::trackPresence()` | one-time | `dht.presence.subscribe` span |
| `connectionManager_->connectDevice()` | `JamiAccount::requestSIPConnection()` | 200 ms – 20 s | `dht.channel.open` span |
| HTTP GET `name.jami.net/addr/<addr>` | `NameDirectory::lookupAddress()` | 50 ms – 3 s | `name.directory.lookup` span |

---

## OTel Relevance

Layer 5 operations are the **dominant source of call setup latency** in JAMI P2P accounts:
- DHT peer lookup can range from 100 ms (cached routing table hit) to 10 s (cold network).
- ICE negotiation dominates in restricted NAT environments: 1–20 s.
- TURN relay allocation adds another 200–500 ms when STUN fails.

**Why traces are essential here**: these operations are fully asynchronous, with callbacks delivered on OpenDHT's internal thread pool. A trace is the only way to reconstruct the sequential relationship between `bootstrapped → identity announced → device discovered → channel opened → call setup`, which may span multiple ASIO callbacks and OpenDHT events over several seconds.

**Why metrics are also essential**: connectivity success rates, DHT lookup durations, and ICE outcome distributions are the primary indicators of network health in a fleet deployment. They answer: "Are DHT lookups getting slower in region X? Are TURN servers being hit more than usual?"

---

## Recommended Signal Mix

| Signal | Instruments | Purpose |
|---|---|---|
| **Traces** | `dht.bootstrap`, `dht.peer.lookup`, `dht.channel.open`, `name.directory.lookup`, `dht.cert.lookup`, `dht.presence.subscribe` (all async span pattern) | Root-cause analysis for call setup failures; step-by-step latency breakdown |
| **Metrics** | `jami.dht.bootstrap.duration` Histogram; `jami.dht.lookups.total` Counter; `jami.dht.lookup.duration` Histogram; `jami.dht.peers.connected` UpDownCounter; `jami.ice.setup.duration` Histogram; `jami.name.directory.lookup.duration` Histogram; `jami.dht.operations.failed` Counter; `jami.dht.storage.items` ObservableGauge | Fleet health, SLO tracking, capacity planning |
| **Logs** | Bridge `JAMI_ERR` in `JamiAccount::doUnregister_()`, DHT error callbacks, ICE failure callbacks | Already covered; bridge to OTel Logs Bridge API |

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use raw DHT `InfoHash` as a metric label | `InfoHash` is a 160-bit value; functionally unbounded as a label |
| Use raw `accountId` as a metric label | Hashed only; use `jami.account.id_hash` (first 16 hex chars of SHA-256) |
| Use raw `deviceId` as any metric label | Same; hashed span attribute only |
| Use peer IP address from ICE candidates as any telemetry field | PII-adjacent; reveals peer location |
| Use DHT key names derived from usernames in raw form | Must be hashed before any OTel attribute |
| Use the full DHT `InfoHash.toString()` as a span attribute | Use only the **first 16 characters** of the hex string |

**Approved metric label values:**
- `jami.account.id_hash`: SHA-256 of `accountId`, first 16 hex chars — acceptable cardinality for per-account metrics
- `channel_type`: `"audioCall"`, `"videoCall"`, `"sync"`, `"msg"` — bounded
- `result`: `"success"`, `"failure"`, `"timeout"`, `"not_found"` — bounded
- `proxy_enabled`: `true` / `false` — boolean
- `server`: the name server hostname (e.g., `"ns.jami.net"`) — bounded in practice

---

## Privacy Concerns

DHT and ICE operations handle data whose telemetry exposure requires explicit protection:

| Data element | Classification | Rule |
|---|---|---|
| DHT `InfoHash` of peer account | Derived from public key; not directly PII | Use only first 16 hex chars in span attributes; never in metrics |
| ICE candidate peer IP addresses | PII-adjacent (reveals peer location) | **Never** include in any span attribute or metric label; log only in DEBUG level which should not be forwarded to OTel |
| DHT key derived from username | `InfoHash(username)` could be reversed by brute-force | **Always hash with SHA-256 before use in telemetry** |
| JAMS server URL | Configuration; not PII | Acceptable in span attributes and as a bounded metric label |
| Device certificate fingerprint | Public; non-reversible | Safe as a hashed span attribute if needed |
| SIP URI in `ChanneledTransport` | PII (reveals account identity) | Hash or omit entirely |

---

## Example C++ Instrumentation Snippet

### Async DHT Lookup Span with Lambda Capture of SpanContext

```cpp
// src/jamidht/jamiaccount.cpp  — JamiAccount::startOutgoingCall()
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/context/context.h"
#include "src/telemetry/telemetry_utils.h"

namespace trace_api = opentelemetry::trace;

void JamiAccount::startOutgoingCall(const std::shared_ptr<SIPCall>& call,
                                    const std::string& toUri)
{
    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.dht", "1.0.0");

    // ── OPEN async span before forEachDevice() ────────────────────────────────
    // Parent context comes from the active call.outgoing span set in
    // SIPCall::SIPCall() and stored in call->callSpanContext_.
    auto token = opentelemetry::context::RuntimeContext::Attach(
                     call->callSpanContext_);

    trace_api::StartSpanOptions opts;
    opts.kind = trace_api::SpanKind::kClient;
    auto lookupSpan = tracer->StartSpan("dht.peer.lookup", opts);

    // Hash the peer URI — never store raw URI in telemetry
    lookupSpan->SetAttribute("dht.peer.id_hash", hashForTelemetry(toUri));
    lookupSpan->SetAttribute("jami.account.id",  hashForTelemetry(accountID_));

    // Capture span as shared_ptr; capture context for callbacks on DHT thread
    auto spanPtr     = std::shared_ptr<trace_api::Span>(std::move(lookupSpan));
    auto savedCtx    = opentelemetry::context::RuntimeContext::GetCurrent();
    // token destructor detaches context from this thread — span remains open

    // ── Metrics: increment lookup counter ─────────────────────────────────────
    std::map<std::string, std::string> mAttrs = {
        {"jami.account.id_hash", hashForTelemetry(accountID_)},
    };
    dhtLookupsCounter_->Add(1,
        opentelemetry::common::KeyValueIterableView<decltype(mAttrs)>{mAttrs});

    auto lookupStart = std::chrono::steady_clock::now();

    // ── Dispatch async DHT get (OpenDHT thread pool) ──────────────────────────
    auto peer_account = dht::InfoHash(toUri);
    accountManager_->forEachDevice(
        peer_account,
        // Per-device callback — called for each device found
        [wCall = std::weak_ptr<SIPCall>(call),
         this](const std::shared_ptr<dht::crypto::PublicKey>& dev) {
             auto call = wCall.lock();
             if (!call) return;
             requestSIPConnection(/* ... */);
        },
        // End callback — called once when DHT traversal is complete
        [spanPtr, savedCtx, lookupStart, mAttrs, this](bool ok) mutable {
            // Restore saved context on DHT callback thread
            auto token = opentelemetry::context::RuntimeContext::Attach(savedCtx);

            auto duration_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - lookupStart).count();

            std::string result = ok ? "found" : "not_found";
            spanPtr->SetAttribute("dht.lookup.result", result);
            if (ok) {
                spanPtr->SetStatus(trace_api::StatusCode::kOk);
            } else {
                spanPtr->SetStatus(trace_api::StatusCode::kError,
                                   "DHT peer lookup returned no devices");
                spanPtr->SetAttribute("error.type", std::string("not_found"));
            }
            spanPtr->End();

            // Record duration histogram
            auto rAttrs = mAttrs;
            rAttrs["result"] = result;
            dhtLookupDurationHist_->Record(duration_ms,
                opentelemetry::common::KeyValueIterableView<decltype(rAttrs)>{rAttrs});
        });
}
```

### Async Channel Open Span (`dht.channel.open`)

```cpp
// JamiAccount::requestSIPConnection()
void JamiAccount::requestSIPConnection(const std::string& toUri,
                                        const DeviceId& deviceId,
                                        const std::string& channelType,
                                        bool isOutgoing,
                                        std::shared_ptr<SIPCall> call)
{
    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.dht", "1.0.0");

    // Parent: the dht.peer.lookup span must pass its context forward.
    // In practice, attach call->callSpanContext_ here to keep the same trace.
    auto token = opentelemetry::context::RuntimeContext::Attach(call->callSpanContext_);

    trace_api::StartSpanOptions opts;
    opts.kind = trace_api::SpanKind::kClient;
    auto chanSpan = tracer->StartSpan("dht.channel.open", opts);

    // First 16 hex chars only — never full hash
    std::string devIdHash = hashForTelemetry(deviceId.toString()).substr(0, 16);
    chanSpan->SetAttribute("dht.peer.device_id_hash", devIdHash);
    chanSpan->SetAttribute("channel.type",            channelType);
    chanSpan->SetAttribute("jami.account.id",         hashForTelemetry(accountID_));

    auto spanPtr  = std::shared_ptr<trace_api::Span>(std::move(chanSpan));
    auto savedCtx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto chanStart = std::chrono::steady_clock::now();

    connectionManager_->connectDevice(
        deviceId, channelType,
        // onConnectionReady: fired on dhtnet executor thread
        [spanPtr, savedCtx, chanStart, channelType, this]
        (std::shared_ptr<dhtnet::ChannelSocket> socket, const DeviceId& dev) mutable {
            auto token = opentelemetry::context::RuntimeContext::Attach(savedCtx);

            auto duration_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - chanStart).count();

            bool ok = (socket != nullptr);
            std::string result = ok ? "success" : "failure";

            spanPtr->SetAttribute("ice.result", result);
            if (ok) {
                spanPtr->SetStatus(trace_api::StatusCode::kOk);
            } else {
                spanPtr->SetStatus(trace_api::StatusCode::kError,
                                   "Channel open failed (ICE/TLS)");
                spanPtr->SetAttribute("error.type", std::string("ice_failure"));
            }
            spanPtr->End();

            // Histogram
            std::map<std::string, std::string> attrs = {
                {"jami.account.id_hash", hashForTelemetry(accountID_)},
                {"channel_type",         channelType},
                {"result",               result},
            };
            iceSetupDurationHist_->Record(duration_ms,
                opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});

            // UpDownCounter
            if (ok) {
                dhtPeersConnected_->Add(+1,
                    opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
            }
        });
}
```

### ObservableGauge for DHT Routing Table Size

```cpp
// In JamiAccount::doRegister_() after dht_->run():
auto meter = opentelemetry::metrics::Provider::GetMeterProvider()
                 ->GetMeter("jami.dht", "1.0.0");

routingTableGauge_ = meter->CreateInt64ObservableGauge(
    "jami.dht.storage.items",
    "Number of nodes in DHT routing table",
    "{nodes}");

routingTableGauge_->AddCallback(
    [](opentelemetry::metrics::ObserverResult result, void* state) {
        auto* account = static_cast<JamiAccount*>(state);
        if (!account->dht_ || !account->dht_->isRunning()) return;
        auto stats = account->dht_->getNodesStats();
        int64_t total = static_cast<int64_t>(
            stats.good_nodes + stats.dubious_nodes + stats.banned_nodes);
        std::map<std::string, std::string> attrs = {
            {"jami.account.id_hash", hashForTelemetry(account->accountID_)},
        };
        result.Observe(total,
            opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
    },
    static_cast<void*>(this));
```

---

## Subsystems in This Layer

| Subsystem | Relationship |
|---|---|
| **dht_layer** | This layer is a direct mapping of the `dht_layer` subsystem; `JamiAccount`, `AccountManager`, `NameDirectory`, `ChanneledTransport` are the primary instrumentation targets |
| **connectivity** | `IceTransport` (dhtnet), `SipTransportBroker`, UPnP/NAT-PMP belong to the connectivity subsystem and are reached from this layer; ICE spans are joint territory |
| **account_identity** (Layer 2) | `dht.bootstrap` span is a child of the `account.register` span at Layer 2; these layers share the `dht::DhtRunner` object |
| **signaling_control** (Layer 3) | `dht.channel.open` is a prerequisite sibling span of `call.outgoing`; the call setup flow reaches Layer 3 only after this span completes |
| **certificate_pki** | TLS handshake within dhtnet channel open (`tls.handshake` grandchild span) is part of `dht.channel.open`; cert validation via `TlsValidator` is called here |
| **im_messaging** | `MessageChannelHandler` uses the same `dhtnet::ConnectionManager` channel infrastructure; channel open timing for message delivery is a secondary trace target |
| **data_transfer** | `TransferChannelHandler` likewise uses dhtnet channels; same span patterns apply |

---

## Source References

- `src/jamidht/jamiaccount.h` / `src/jamidht/jamiaccount.cpp`
- `src/jamidht/account_manager.h` / `.cpp`
- `src/jamidht/namedirectory.h` / `.cpp`
- `src/jamidht/channeled_transport.h` / `.cpp`
- `src/jamidht/auth_channel_handler.h` / `.cpp`
- `src/sip/siptransport.h` / `.cpp`
- `src/sip/sipvoiplink.h` / `.cpp`
- `src/connectivity/ip_utils.h` / `.cpp`
- KB: `subsystem_dht_layer.md` — DHT operation table and critical code paths
- KB: `subsystem_connectivity.md` — ICE and transport details
- KB: `subsystem_certificate_pki.md` — TLS handshake in dhtnet channels
- KB: `integration_dht_layer.md` — full span/metric specification for this layer
- KB: `otel_traces.md` — async span pattern with lambda context capture
- KB: `otel_semconv.md` — network and messaging attribute naming

---

## Open Questions

1. **dhtnet instrumentation access**: `dhtnet::ConnectionManager` and `dhtnet::IceTransport` are in a separate library. ICE sub-phases (candidate gathering, connectivity checks, TURN allocation) cannot be traced without either modifying dhtnet or relying solely on the `dht.channel.open` span duration as a proxy. What is the policy for instrumenting dhtnet — fork + patch, or upstream contribution?
2. **`dht.node.query` child spans**: the `integration_dht_layer.md` proposes per-DHT-node query spans inside `forEachDevice()`. At scale (100-node traversal), this produces 100 child spans per call setup. Is this acceptable, or should only aggregate timing be recorded?
3. **Bootstrap connection spans**: `dht_->bootstrap(host)` issues one connection per entry in the bootstrap list. Should each attempt be a `dht.bootstrap.connect` child span with `server.address` / `server.port` attributes?
4. **NameDirectory error types**: `NameDirectory::lookupName()` response codes (`found`, `notFound`, `invalidResponse`, `error`) map cleanly to `result` metric labels. But the mapping from HTTP response codes is inside `dht::http::Request` callbacks. Is the full HTTP status code safe to record in a span attribute?
5. **TURN relay usage metric**: there is no metric today for "fraction of calls using TURN relay". This is high operational value. Can `dhtnet::IceTransport` expose a `usedTurnRelay()` accessor, or must it be inferred from ICE candidate type in dhtnet callbacks?
