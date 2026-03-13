# Integration Plan: Account Management

## Status: draft
## Last Updated: 2026-03-13

---

## Overview

This document specifies how OpenTelemetry tracing and metrics should be injected into the account management subsystem of jami-daemon. It covers both `SIPAccount` (PJSIP registrar client) and `JamiAccount` (OpenDHT P2P) paths, including their divergent authentication and transport flows.

---

## Proposed Span Hierarchy

```
account.register  (ROOT, SpanKind::CLIENT)
  ├── account.dht.join                (INTERNAL — JamiAccount only)
  │     └── dht.bootstrap.connect    (INTERNAL — one per bootstrap node contacted)
  └── account.sip.register           (CLIENT — SIPAccount only)
        └── sip.transport.tls.handshake  (INTERNAL — only when TLS transport used)

account.import  (ROOT, SpanKind::INTERNAL)
account.export  (ROOT, SpanKind::INTERNAL)
```

### Span Attribute Contracts

| Span name | Required attributes | Optional attributes |
|---|---|---|
| `account.register` | `jami.account.type` (`SIP`\|`RING`), `jami.account.id` (hashed), `jami.account.manager` (`archive`\|`server`\|`none`) | `error.type` (on error), `jami.account.alias` |
| `account.dht.join` | `jami.account.type` = `RING` | `dht.node.id` (hashed), `dht.bootstrap.count` |
| `dht.bootstrap.connect` | `server.address`, `server.port` | `network.transport` |
| `account.sip.register` | `server.address`, `server.port`, `sip.registrar.uri` (hostname only — no credentials) | `network.transport` (`udp`\|`tcp`\|`tls`) |
| `sip.transport.tls.handshake` | `tls.protocol` (e.g. `TLSv1.3`), `tls.cipher` | `tls.peer_cert_subject` (hashed) |
| `account.import` | `jami.account.type`, `jami.account.manager` | `jami.import.source` (`file`\|`device`) |
| `account.export` | `jami.account.type` | `jami.export.destination.path` (MUST be omitted — privacy) |

**Status rules:**
- Set `SpanStatusCode::ERROR` + `span->SetStatus(SpanStatusCode::ERROR, detail_str)` on any `ERROR_*` registration state.
- Set `SpanStatusCode::OK` on `REGISTERED`.

---

## Proposed Metric Instruments

| Instrument name | Type | Unit | Labels | Description |
|---|---|---|---|---|
| `jami.accounts.registered` | `UpDownCounter<int64_t>` | `{accounts}` | `jami.account.type` | Currently registered accounts (increment on `REGISTERED`, decrement on `UNREGISTERED` / `ERROR_*`) |
| `jami.accounts.total` | `ObservableGauge<int64_t>` | `{accounts}` | `jami.account.type` | Total loaded accounts (sampled from `AccountFactory::accountCount<SIPAccount>()` + `accountCount<JamiAccount>()`) |
| `jami.account.registration.duration` | `Histogram<double>` | `ms` | `jami.account.type`, `jami.account.manager`, `outcome` (`success`\|`failure`) | Time from `TRYING` entry to first terminal state (`REGISTERED` or `ERROR_*`) |
| `jami.account.registration.failures` | `Counter<uint64_t>` | `{failures}` | `jami.account.type`, `error.type` (`auth`\|`network`\|`host`\|`service_unavailable`\|`need_migration`\|`generic`) | Registration failure events |
| `jami.account.type.distribution` | `Counter<uint64_t>` | `{accounts}` | `jami.account.type` | Incremented once per `AccountFactory::createAccount()` call; tracks cumulative account creation by type |

### Recommended Histogram Boundaries

```cpp
// For jami.account.registration.duration (ms)
// SIP typical: 10–600 ms; DHT typical: 500 ms–30 s
std::vector<double> boundaries = {
    50, 100, 250, 500, 1000, 2500, 5000, 10000, 20000, 30000
};
```

Configure via an OTel SDK `View` binding the histogram name to these explicit boundaries.

---

## Code Injection Points

### 1. `account.register` — ROOT span

**File:** `src/account.cpp`  
**Method:** `Account::setRegistrationState(RegistrationState state, ...)`

This is the single canonical location where every registration state change passes, for both `SIPAccount` and `JamiAccount`. Inject here to avoid duplicating logic in each subclass.

```cpp
// account.cpp
void Account::setRegistrationState(RegistrationState state, int detail_code,
                                   const std::string& detail_str) {
    // [INSTRUMENTATION START]
    if (state == RegistrationState::TRYING && registrationState_ != RegistrationState::TRYING) {
        // Open root span; store in member: std::shared_ptr<trace::Span> registrationSpan_
        auto tracer = opentelemetry::trace::Provider::GetTracerProvider()
                          ->GetTracer("jami.account", "1.0.0");
        opentelemetry::trace::StartSpanOptions opts;
        opts.kind = opentelemetry::trace::SpanKind::kClient;
        registrationSpan_ = tracer->StartSpan("account.register", opts);
        registrationSpan_->SetAttribute("jami.account.type", getAccountType());
        registrationSpan_->SetAttribute("jami.account.id", hashAccountId(accountID_));
        registrationStartTime_ = std::chrono::steady_clock::now();
    }
    if (registrationSpan_ && (state == RegistrationState::REGISTERED ||
        (state >= RegistrationState::ERROR_GENERIC))) {
        auto duration_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - registrationStartTime_).count();
        if (state == RegistrationState::REGISTERED) {
            registrationSpan_->SetStatus(opentelemetry::trace::StatusCode::kOk);
        } else {
            registrationSpan_->SetAttribute("error.type", detail_str);
            registrationSpan_->SetStatus(opentelemetry::trace::StatusCode::kError, detail_str);
        }
        registrationSpan_->End();
        registrationSpan_.reset();
        // record histogram
        registrationDurationHistogram_->Record(duration_ms, /*attrs...*/);
    }
    // [INSTRUMENTATION END]

    // existing state assignment and signal emission ...
}
```

**New `Account` members required:**
```cpp
// account.h (protected section)
std::shared_ptr<opentelemetry::trace::Span> registrationSpan_ {};
std::chrono::steady_clock::time_point registrationStartTime_ {};
```

---

### 2. `account.dht.join` — child of `account.register`

**File:** `src/jamidht/jamiaccount.cpp`  
**Method:** `JamiAccount::doRegister_()` (private)

Start the child span just before `dht_->run()`, propagate the parent context from `registrationSpan_`. End it inside the `onDhtConnected` / `onDhtRunnerReady` callback (the asio lambda called when the DHT node first connects to peers).

```cpp
// In JamiAccount::doRegister_(), after registrationSpan_ is guaranteed to exist:
auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
if (registrationSpan_) {
    ctx = opentelemetry::trace::SetSpan(ctx,
              opentelemetry::trace::TraceContextExt::GetCurrentSpan());
}
opentelemetry::trace::StartSpanOptions dhtOpts;
dhtOpts.parent = registrationSpan_->GetContext(); // propagate parent
dhtOpts.kind   = opentelemetry::trace::SpanKind::kInternal;
auto dhtJoinSpan = tracer->StartSpan("account.dht.join", dhtOpts);

dht_->run(port, id_, /* ... */);
dht_->bootstrap(bootstrapNodes, [weak = weak(), dhtJoinSpan](bool ok) mutable {
    dhtJoinSpan->SetAttribute("dht.bootstrap.success", ok);
    if (!ok)
        dhtJoinSpan->SetStatus(opentelemetry::trace::StatusCode::kError, "bootstrap failed");
    else
        dhtJoinSpan->SetStatus(opentelemetry::trace::StatusCode::kOk);
    dhtJoinSpan->End();
});
```

---

### 3. `dht.bootstrap.connect` — child of `account.dht.join`

**File:** `src/jamidht/jamiaccount.cpp`  
**Trigger:** For each bootstrap node in `dhtBootstrap` config list.

Create one child span per node contacted (start before the connection attempt, end in the per-node callback). Pass `dhtJoinSpan->GetContext()` as parent.

---

### 4. `account.sip.register` — child of `account.register`

**File:** `src/sip/sipaccount.cpp`  
**Method:** `SIPAccount::sendRegister()`

```cpp
void SIPAccount::sendRegister() {
    opentelemetry::trace::StartSpanOptions opts;
    opts.parent = registrationSpan_->GetContext();
    opts.kind   = opentelemetry::trace::SpanKind::kClient;
    sipRegSpan_ = tracer->StartSpan("account.sip.register", opts);
    sipRegSpan_->SetAttribute("server.address", config().hostname);
    sipRegSpan_->SetAttribute("network.transport",
                              config().tlsEnable ? "tls" : (config().transport == "TCP" ? "tcp" : "udp"));
    // ... existing pjsip_regc_* calls ...
}
```

End in `onRegCallback`:
```cpp
// In SIPAccount::onRegCallback (static PJSIP thread callback):
if (sipRegSpan_) {
    if (param->code == 200) {
        sipRegSpan_->SetStatus(opentelemetry::trace::StatusCode::kOk);
    } else {
        sipRegSpan_->SetAttribute("error.type", std::to_string(param->code));
        sipRegSpan_->SetStatus(opentelemetry::trace::StatusCode::kError,
                               param->reason.ptr ? param->reason.ptr : "");
    }
    sipRegSpan_->End();
    sipRegSpan_.reset();
}
```

**New `SIPAccount` member required:**
```cpp
std::shared_ptr<opentelemetry::trace::Span> sipRegSpan_ {};
```

---

### 5. `sip.transport.tls.handshake` — child of `account.sip.register`

**File:** `src/sip/siptransport.cpp`  
**Method:** `SipTransport::stateCallback(pjsip_transport_state state, ...)`

```cpp
void SipTransport::stateCallback(pjsip_transport_state state,
                                 const pjsip_transport_state_info* info) {
    if (state == PJSIP_TP_STATE_CONNECTING) {
        // Start TLS handshake span; store in SipTransport member
    }
    if (state == PJSIP_TP_STATE_CONNECTED) {
        // End span with OK; populate TlsInfos
    }
    if (state == PJSIP_TP_STATE_DISCONNECTED) {
        // End span with error if not yet ended
    }
    // ... existing state listener callbacks ...
}
```

The parent span context must be passed from `SIPAccount::sendRegister()` when creating the transport; stored in `SipTransport` as `opentelemetry::context::Context tlsParentCtx_`.

---

### 6. `account.import` / `account.export` — ROOT spans

**File:** `src/jamidht/archive_account_manager.cpp`

| Operation | Method | Span start/end |
|---|---|---|
| Import (new device) | `ArchiveAccountManager::initAuthentication()` | Start at entry; end in `onSuccess` or `onFailure` callback |
| Import (file restore) | `ArchiveAccountManager::loadFromFile()` | Start at entry; end on return |
| Export | `ArchiveAccountManager::exportArchive()` | Wrap entire function body |

Attributes: `jami.account.type`, `jami.account.manager` (`archive`), `jami.import.source` (`file`|`device`).

---

### 7. Metric Recording Points

| Metric | Where to record |
|---|---|
| `jami.accounts.registered` +1 | `Account::setRegistrationState(REGISTERED)` |
| `jami.accounts.registered` -1 | `Account::setRegistrationState(UNREGISTERED \| ERROR_*)` |
| `jami.accounts.total` callback | `ObservableGauge` callback reads `AccountFactory::accountCount<SIPAccount>()` + `accountCount<JamiAccount>()` |
| `jami.account.registration.duration` | `Account::setRegistrationState()` on terminal state (see §1 above) |
| `jami.account.registration.failures` | `Account::setRegistrationState(ERROR_*)` — map error enum → `error.type` label |
| `jami.account.type.distribution` | `AccountFactory::createAccount()` after successful insertion |

---

## Context Propagation Strategy

### JamiAccount (ASIO thread pool)

OpenDHT and dhtnet callbacks are `std::function` lambdas dispatched through `asio::post`. The ASIO io_context thread is effectively always the same logical "account thread" per account. Use lambda capture to propagate span context:

```cpp
// Capture the parent span context by value before the async post:
auto parentCtx = opentelemetry::trace::SetSpan(
    opentelemetry::context::RuntimeContext::GetCurrent(), parentSpan);

asio::post(ioContext_, [parentCtx, weak = weak()] {
    // Attach context for duration of this callback:
    auto token = opentelemetry::context::RuntimeContext::Attach(parentCtx);
    auto self = weak.lock();
    if (!self) return;
    // ... operations that may create child spans ...
    // token destructor detaches context
});
```

### SIPAccount (PJSIP thread)

PJSIP callbacks (`onRegCallback`) run on `sipThread_`, a dedicated thread that is NOT the ASIO io_context. Thread-local OpenTelemetry context will be empty on this thread unless explicitly set.

**Strategy:** Store the span handle as a `shared_ptr<Span>` member on `SIPAccount` rather than relying on context propagation. The PJSIP callback retrieves it directly:

```cpp
// SIPAccount stores:
std::shared_ptr<opentelemetry::trace::Span> registrationSpan_; // set on doRegister_()
std::shared_ptr<opentelemetry::trace::Span> sipRegSpan_;       // set on sendRegister()
```

Access from PJSIP callbacks is safe because `SIPAccount` outlives the PJSIP registrar context and the spans are accessed only once (one terminal callback). Protect with `std::mutex sipSpanMutex_` if re-registration is possible while a callback is in-flight.

### archive.import (mixed)

`ArchiveAccountManager::AuthContext` already has a `std::mutex`. Add `std::shared_ptr<opentelemetry::trace::Span> importSpan` to `AuthContext` and protect access under `AuthContext::mutex`.

---

## Privacy Considerations

| Datum | Risk | Mitigation |
|---|---|---|
| `accountID_` (JamiAccount) | DHT public key hash — uniquely identifies a person | SHA-256 hash before use as span attribute; truncate to first 16 hex chars for cardinality control |
| SIP `username` / `password` | Plaintext credentials | **Never** attach to spans or metric labels. Use `server.address` (hostname only) for SIP registrar identification |
| Private key material (`dht::crypto::Identity`) | Catastrophic if leaked | Never log, never include in spans; ensure no `std::string` serialization of key bytes passes through instrumentation code |
| Device certificate subject | Derived from account ID | Hash or omit; safe only if treated as opaque ID |
| `archivePath_` | Reveals local filesystem layout | Omit from span attributes; log only relative or basename if needed |
| `token_` (ServerAccountManager JWT) | Bearer token | Never include in spans or metrics |
| SIP Contact/From URIs | Contains username@domain | Strip username; include only domain/host component |

---

## Thread Safety Notes

1. **`registrationSpan_` in `Account`**: Written from TRYING entry (whatever thread calls `setRegistrationState(TRYING)`) and read/ended from a terminal-state call (potentially a different thread for SIPAccount vs JamiAccount). Protect with a dedicated `std::mutex registrationSpanMutex_` in `Account`.

2. **`sipRegSpan_` in `SIPAccount`**: Written on the ASIO thread (inside `sendRegister()`), read on the PJSIP thread (`onRegCallback`). Must be protected by a `std::mutex` or use `std::atomic<std::shared_ptr<...>>` (C++20) to avoid data race.

3. **`dhtJoinSpan` in `JamiAccount::doRegister_()`**: Local span variable captured by the lambda; ASIO single-threaded per account — safe without additional locking as long as the lambda captures by value (`[dhtJoinSpan]`).

4. **`ObservableGauge` callback for `jami.accounts.total`**: Runs on the OTel background exporter thread. Must not call `AccountFactory` methods that acquire `AccountFactory::mutex_` if that mutex is held on other threads for extended periods. Prefer a snapshot approach: maintain a `std::atomic<int64_t>` per type updated at create/remove time.

5. **`SipTransport::tlsParentCtx_`**: Written by `SIPAccount` on ASIO thread when transport is created; read by `stateCallback` on PJSIP thread. Protect with the existing `SipTransport` state listener mutex.

---

## Risks & Complications

1. **Re-registration loops**: SIP RFC 3261 mandates periodic REGISTER refresh (typically every 3600 s). `SIPAccount` re-registers automatically. Each refresh would restart the `account.register` span logic. Decision required: suppress refresh-registration spans (only trace initial registration) or record all re-registrations with a `jami.register.refresh=true` attribute.

2. **Long-lived DHT spans**: The `account.dht.join` span may remain open for 30+ seconds on a cold boot or hostile network. OTel exporters buffer open spans in memory; very long spans are dropped if the export timeout is exceeded. Mitigation: add a `span->AddEvent("dht.timeout")` at 30 s and force-end the span with an error status.

3. **PJSIP thread isolation**: The PJSIP thread has no ASIO context association and no OTel context by default. Any attempt to use `RuntimeContext::GetCurrent()` on the PJSIP thread returns an empty context. All span propagation to/from PJSIP callbacks MUST use explicit span handle passing (not context TLS).

4. **`doRegister_()` is private**: The `account.dht.join` child span must be created inside `JamiAccount::doRegister_()`. This is an implementation detail already; adding OTel calls there is appropriate but must be done carefully alongside the existing `AuthContext` management.

5. **AuthContext async lifetime**: In `ArchiveAccountManager`, `AuthContext` is stored as `std::shared_ptr<AuthContext> authCtx_`. If the account is destroyed (e.g., user removes the account) before authentication completes, the `onSuccess`/`onFailure` callbacks may fire after `JamiAccount` destruction. Span handles captured in these callbacks must use `weak_ptr` guarded access to avoid use-after-free.

6. **JAMS token refresh**: `ServerAccountManager` caches a JWT token and silently refreshes it. A silent token refresh does NOT constitute a new `account.register` event and should not be traced as such, but could produce a spurious `TRYING` → `REGISTERED` state transition if token expiry triggers re-authentication. Audit `ServerAccountManager::onNeedsMigration_` and token refresh paths before instrumenting.

7. **`ArchiveAccountManager::addDevice` and `DeviceAuthState`**: The device-linking flow has its own `DeviceAuthState` enum (INIT → TOKEN_AVAILABLE → CONNECTING → AUTHENTICATING → IN_PROGRESS → DONE). This is a separate operation from `account.register` and should be traced under a separate `device.link` root span (out of scope for this document but noted for the device management integration plan).

8. **Meter provider initialization**: There is currently no OTel `MeterProvider` or `TracerProvider` initialized in jami-daemon. A global initialization point must be added to `Manager::init()` (or a dedicated `TelemetryManager`) before any instrumented code runs. Until then, all API calls return no-op implementations (safe by OTel spec).

---

## Source References

| Symbol | File | Lines |
|---|---|---|
| `Account::doRegister()` (pure virtual) | [src/account.h](../../src/account.h#L164) | L164 |
| `Account::setRegistrationState()` declaration | [src/account.h](../../src/account.h#L283) | L283 |
| `Account::registrationState_` member | [src/account.h](../../src/account.h#L461) | L461 |
| `RegistrationState` enum | [src/registration_states.h](../../src/registration_states.h#L27) | L27–L42 |
| `AccountFactory::createAccount()` | [src/account_factory.cpp](../../src/account_factory.cpp#L42) | L42–L57 |
| `AccountFactory` generators registration | [src/account_factory.cpp](../../src/account_factory.cpp#L32) | L32–L37 |
| `JamiAccount::doRegister()` | [src/jamidht/jamiaccount.h](../../src/jamidht/jamiaccount.h#L161) | L161 |
| `JamiAccount::doRegister_()` (private) | [src/jamidht/jamiaccount.h](../../src/jamidht/jamiaccount.h#L657) | L657 |
| `JamiAccount::setRegistrationState()` | [src/jamidht/jamiaccount.h](../../src/jamidht/jamiaccount.h#L172) | L172 |
| `ArchiveAccountManager::initAuthentication()` | [src/jamidht/archive_account_manager.h](../../src/jamidht/archive_account_manager.h#L62) | L62–L66 |
| `ArchiveAccountManager::AuthContext` | [src/jamidht/archive_account_manager.h](../../src/jamidht/archive_account_manager.h#L109) | L109–L124 |
| `DeviceAuthState` enum | [src/jamidht/archive_account_manager.h](../../src/jamidht/archive_account_manager.h#L28) | L28–L35 |
| `ServerAccountManager::initAuthentication()` | [src/jamidht/server_account_manager.h](../../src/jamidht/server_account_manager.h#L42) | L42–L48 |
| `ServerAccountManager::token_` / `tokenExpire_` | [src/jamidht/server_account_manager.h](../../src/jamidht/server_account_manager.h#L100) | L100–L104 |
| `SIPAccount::doRegister()` | [src/sip/sipaccount.h](../../src/sip/sipaccount.h#L133) | L133 |
| `SIPAccount::sendRegister()` | [src/sip/sipaccount.h](../../src/sip/sipaccount.h#L139) | L139 |
| `SipTransport::stateCallback()` | [src/sip/siptransport.h](../../src/sip/siptransport.h#L88) | L88 |
| `TlsInfos` struct | [src/sip/siptransport.h](../../src/sip/siptransport.h#L73) | L73–L78 |

---

## Open Questions

1. **DHT "connected" signal**: Is there a single canonical callback on `dht::DhtRunner` that fires when the node first successfully contacts a peer (above just "running")? The bootstrap callback fires per-node, not once for the whole join. Clarify what constitutes "DHT join complete" for span-end timing.

2. **Re-registration suppression**: Should periodic SIP REGISTER refreshes produce traces? If yes, use a `jami.register.is_refresh` boolean attribute to distinguish initial registration from renewal.

3. **OTel exporter selection**: Which exporter will be used — OTLP gRPC, OTLP HTTP, Prometheus pull, or in-process logging? This determines whether `Views` for histogram bucket configuration need to be set up at provider init time.

4. **JAMS auth as child span**: Should the JAMS `POST /api/auth/device` HTTP call be traced as a child span (e.g., `jams.auth.device`) under `account.register`? This would require passing the OTel context into `ServerAccountManager`, which currently has no tracing awareness.

5. **`MeterProvider` / `TracerProvider` initialization location**: Should a `TelemetryManager` singleton be added to `Manager`, or should providers be initialized at process startup in `jami.cpp` before `init()` is called? The latter is simpler but less testable.

6. **Account ID hashing function**: Which hash function and truncation length should be used for the `jami.account.id` span attribute? Proposal: `SHA-256(accountID_)[0:16]` (first 8 bytes, 16 hex chars). Confirm this is sufficient for uniqueness and irreversibility in the operational context.

7. **DHT port attribute**: Should `dht.node.port` (the local UDP port bound by `DhtRunner`) be included as a span attribute on `account.dht.join`? It could be useful for multi-instance debugging but exposes local network topology.
