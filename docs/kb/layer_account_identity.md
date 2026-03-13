# Layer 2 — Account & Identity Layer

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 2 owns the persistent identity and lifecycle of every communication account within the daemon. It spans from account creation and cryptographic key generation, through the state machine that tracks registration health, to multi-device linking, archive backup/restore, and certificate trust management. Logically it sits immediately beneath Layer 1 (Public API) and above Layer 3 (Signalling), feeding registered account objects and authenticated identities downward to those layers.

### Constituting Files and Classes

#### Base Account Infrastructure

| Class | File | Role |
|---|---|---|
| `Account` | `src/account.h` / `.cpp` | Abstract base; holds `accountId_`, `registrationState_`, `callSet_`, `msgEngine_`; fires `StateListenerCb` on state changes |
| `AccountConfig` | `src/account_config.h` / `.cpp` | Serialisable config base; `buildConfig()` / `setConfig()` / `loadConfig()` lifecycle |
| `AccountFactory` | `src/account_factory.h` / `.cpp` | Creates `SIPAccount` or `JamiAccount` by type string; maintains thread-safe account map |
| `RegistrationState` | `src/registration_states.h` | Enum: `UNLOADED`, `UNREGISTERED`, `TRYING`, `REGISTERED`, `ERROR_AUTH`, `ERROR_NETWORK`, `ERROR_HOST`, `ERROR_SERVICE_UNAVAILABLE`, `ERROR_GENERIC` |

#### SIP Account Path

| Class | File | Role |
|---|---|---|
| `SIPAccountBase` | `src/sip/sipaccountbase.h` / `.cpp` | Shared SIP logic: codec negotiation, SRTP, ICE parameters, presence base |
| `SIPAccount` | `src/sip/sipaccount.h` / `.cpp` | PJSIP registrar client (`pjsip_regc`); NAT traversal; STUN/TURN config; TLS transport creation |
| `SipAccountConfig` | `src/sip/sipaccount_config.h` / `.cpp` | SIP-specific config: registrar URI, credential list, transport, STUN/TURN |

#### JAMI/DHT Account Path

| Class | File | Role |
|---|---|---|
| `JamiAccount` | `src/jamidht/jamiaccount.h` / `.cpp` | P2P account; owns `dht::DhtRunner`; drives `dhtnet::ConnectionManager`; manages `ConversationModule`, `SyncModule` |
| `JamiAccountConfig` | `src/jamidht/jamiaccount_config.h` / `.cpp` | DHT-specific config: bootstrap list, archive path, name server URL, TURN usage |
| `AccountManager` | `src/jamidht/account_manager.h` / `.cpp` | Holds `AccountInfo` (Ed25519 identity + contacts); `forEachDevice()`, `findCertificate()`, `startSync()` |
| `ArchiveAccountManager` | `src/jamidht/archive_account_manager.h` / `.cpp` | Encrypts/decrypts local `.gz` account archive |
| `ServerAccountManager` | `src/jamidht/server_account_manager.h` / `.cpp` | JAMS REST API authentication backend |
| `ContactList` | `src/jamidht/contact_list.h` / `.cpp` | Trusted/blocked/pending contact state; persisted via msgpack; fires `OnContactAdded`, `OnDevicesChanged` |
| `NameDirectory` | `src/jamidht/namedirectory.h` / `.cpp` | HTTP client for username↔DHT key resolution (`ns.jami.net`) |
| `AccountArchive` | `src/jamidht/accountarchive.h` / `.cpp` | Encrypted backup container |

#### Certificate & PKI (shared with Layer 5)

| Class | File | Role |
|---|---|---|
| `TlsValidator` | `src/connectivity/security/tlsvalidator.h` | Validates certificate chains; produces per-property status flags |
| `dhtnet::CertificateStore` | `<dhtnet/certstore.h>` (external) | Trust anchor storage; certificate caching |
| `dht::crypto::Identity` | `<opendht/crypto.h>` (external) | Ed25519 keypair + device certificate chain; core identity |

---

## OTel Relevance

Layer 2 is characterised by **infrequent, high-latency, and security-critical operations**. These are ideal candidates for distributed traces rather than metrics because:

- **Registration is an async multi-step flow.** `JamiAccount::doRegister_()` initiates DHT bootstrap, waits for the identity to be announced, and only then transitions to `REGISTERED`. This spans multiple threads (ASIO pool, OpenDHT callbacks) and can take 500 ms–30 s. A trace spanning the whole flow gives exact step-by-step timing.
- **Authentication failures are high-value events.** `ERROR_AUTH` and `ERROR_HOST` transitions should generate both a completed error span and a metric counter increment so operations dashboards can alert on authentication spikes.
- **TLS handshake during SIP registration** is a meaningful sub-operation that can independently fail; it deserves a child span.
- **Device linking and account import/export** are rare but complex flows with multiple async steps.

---

## Recommended Signal Mix

| Signal | Instruments | Rationale |
|---|---|---|
| **Traces** | `account.register` (root, async span pattern); `account.dht.join` (child); `account.sip.register` (child); `sip.transport.tls.handshake` (grandchild); `account.import`, `account.export` (root spans) | Low frequency, long duration, critical errors |
| **Metrics** | `jami.accounts.registered` UpDownCounter; `jami.account.registration.duration` Histogram; `jami.account.registration.failures` Counter; `jami.accounts.total` ObservableGauge | Registration health, SLO tracking |
| **Logs** | Bridge `JAMI_ERR` calls in `Account::setRegistrationState()` to OTel Log Bridge API with `trace_id`/`span_id` injected for correlation | Structured error context |

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use `accountId_` (raw UUID string) as a metric label | It is a high-cardinality identifier; use only as a **hashed** span attribute |
| Use the SIP URI / registrar address as a metric label | Unbounded; could expose user data |
| Use device ID as a metric label | High cardinality; belongs only as a hashed span attribute |
| Record archive file paths or encryption keys anywhere in telemetry | Serious privacy/security risk |
| Use `peerUri_`, display name, or username as attributes without hashing | PII |

**Approved metric label values:**
- `jami.account.type`: `"SIP"` or `"RING"` — bounded
- `jami.account.manager`: `"archive"`, `"server"`, or `"none"` — bounded
- `outcome`: `"success"` or `"failure"` — bounded
- `error.type`: `"auth"`, `"network"`, `"host"`, `"service_unavailable"`, `"generic"` — bounded

**Approved span attribute values (not metrics):**
- `jami.account.id`: SHA-256 of the raw `accountId_`, first 16 hex chars via `hashForTelemetry()`
- `dht.peer.device_id_hash`: SHA-256 of device ID string, first 16 hex chars

---

## Example C++ Instrumentation Snippet

### Async Span Pattern — `Account::setRegistrationState()`

This is the single canonical injection point covering **both** `SIPAccount` and `JamiAccount`. A span is opened when `TRYING` is entered and kept alive (stored as a `shared_ptr` member) until a terminal state is reached.

```cpp
// src/account.h — add to protected section
#include "opentelemetry/trace/span.h"
#include <chrono>
#include <memory>

protected:
    // OTel: live registration span (null when not in a registration cycle)
    std::shared_ptr<opentelemetry::trace::Span> registrationSpan_ {};
    std::chrono::steady_clock::time_point       registrationStartTime_ {};
```

```cpp
// src/account.cpp
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/metrics/provider.h"
#include "src/telemetry/telemetry_utils.h"   // hashForTelemetry()

namespace trace_api  = opentelemetry::trace;
namespace metric_api = opentelemetry::metrics;

// Instrument handles — created once per process, cached as statics.
static auto& RegistrationDurationHistogram() {
    static auto h = metric_api::Provider::GetMeterProvider()
        ->GetMeter("jami.account", "1.0.0")
        ->CreateDoubleHistogram(
            "jami.account.registration.duration",
            "Time from TRYING to first terminal registration state",
            "ms");
    return *h;
}

static auto& RegistrationFailuresCounter() {
    static auto c = metric_api::Provider::GetMeterProvider()
        ->GetMeter("jami.account", "1.0.0")
        ->CreateUInt64Counter(
            "jami.account.registration.failures",
            "Total registration failure events",
            "{failures}");
    return *c;
}

static auto& RegisteredAccountsUpDown() {
    static auto u = metric_api::Provider::GetMeterProvider()
        ->GetMeter("jami.account", "1.0.0")
        ->CreateInt64UpDownCounter(
            "jami.accounts.registered",
            "Currently registered accounts",
            "{accounts}");
    return *u;
}

// ── Async span pattern ────────────────────────────────────────────────────────
void Account::setRegistrationState(RegistrationState state,
                                   int              detail_code,
                                   const std::string& detail_str)
{
    const bool entering_trying =
        (state == RegistrationState::TRYING &&
         registrationState_ != RegistrationState::TRYING);

    const bool is_terminal =
        (state == RegistrationState::REGISTERED   ||
         state == RegistrationState::UNREGISTERED ||
         state >= RegistrationState::ERROR_GENERIC);

    // ── OPEN span when entering TRYING ────────────────────────────────────────
    if (entering_trying) {
        auto tracer = trace_api::Provider::GetTracerProvider()
                          ->GetTracer("jami.account", "1.0.0");
        trace_api::StartSpanOptions opts;
        opts.kind = trace_api::SpanKind::kClient;

        registrationSpan_ = tracer->StartSpan("account.register", opts);
        registrationSpan_->SetAttribute("jami.account.type",
                                        std::string(getAccountType()));
        registrationSpan_->SetAttribute("jami.account.id",
                                        hashForTelemetry(accountID_));
        registrationSpan_->SetAttribute("jami.account.manager",
                                        std::string(getAccountManagerType()));
        registrationStartTime_ = std::chrono::steady_clock::now();
    }

    // ── CLOSE span when terminal state reached ─────────────────────────────
    if (registrationSpan_ && is_terminal) {
        auto duration_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - registrationStartTime_).count();

        std::map<std::string, std::string> attrs = {
            {"jami.account.type",    std::string(getAccountType())},
            {"jami.account.manager", std::string(getAccountManagerType())},
        };

        if (state == RegistrationState::REGISTERED) {
            registrationSpan_->SetStatus(trace_api::StatusCode::kOk);
            attrs["outcome"] = "success";
            RegisteredAccountsUpDown().Add(+1,
                opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
        } else if (state == RegistrationState::UNREGISTERED) {
            registrationSpan_->SetStatus(trace_api::StatusCode::kOk);
            attrs["outcome"] = "success";   // clean unregister is a success
        } else {
            // ERROR_* states
            std::string err_type = registrationStateToErrorType(state);
            registrationSpan_->SetAttribute("error.type", err_type);
            registrationSpan_->SetStatus(trace_api::StatusCode::kError, detail_str);
            attrs["error.type"] = err_type;
            attrs["outcome"]    = "failure";

            RegistrationFailuresCounter().Add(1,
                opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
        }

        attrs["outcome"] = (state == RegistrationState::REGISTERED) ? "success" : "failure";
        auto kv = opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs};
        RegistrationDurationHistogram().Record(duration_ms, kv);

        registrationSpan_->End();
        registrationSpan_.reset();
    }

    // ── Decrement registered count when previously REGISTERED account errors ──
    if (registrationState_ == RegistrationState::REGISTERED && is_terminal &&
        state != RegistrationState::REGISTERED) {
        std::map<std::string, std::string> attrs = {
            {"jami.account.type", std::string(getAccountType())}};
        RegisteredAccountsUpDown().Add(-1,
            opentelemetry::common::KeyValueIterableView<decltype(attrs)>{attrs});
    }

    // ── Existing logic (unchanged) ─────────────────────────────────────────
    registrationState_ = state;
    // ... signal emission, callbacks ...
}
```

**Helper: map `RegistrationState` to a bounded error-type string**

```cpp
// src/account.cpp (file-scope static)
static std::string registrationStateToErrorType(RegistrationState s) {
    switch (s) {
        case RegistrationState::ERROR_AUTH:                return "auth";
        case RegistrationState::ERROR_NETWORK:             return "network";
        case RegistrationState::ERROR_HOST:                return "host";
        case RegistrationState::ERROR_SERVICE_UNAVAILABLE: return "service_unavailable";
        case RegistrationState::ERROR_NEED_MIGRATION:      return "need_migration";
        default:                                           return "generic";
    }
}
```

### Histogram Bucket Configuration

```cpp
// During OTel SDK initialisation (in jamid/main or the provider-setup helper):
// Registration can take from~50 ms (fast SIP) to ~30 s (DHT bootstrap on slow network)
std::vector<double> reg_boundaries = {
    50, 100, 250, 500, 1000, 2500, 5000, 10000, 20000, 30000
};
// Apply via a View binding in the MeterProvider configuration.
```

---

## Subsystems in This Layer

| Subsystem | Relationship |
|---|---|
| **account_management** | This layer is a direct description of the `account_management` subsystem |
| **dht_layer** | `JamiAccount` delegates DHT identity publication to `AccountManager` and `NameDirectory`; child spans (`dht.bootstrap`, `dht.identity.announce`) are part of the `account.register` tree |
| **certificate_pki** | `TlsValidator`, `dhtnet::CertificateStore`, and `dht::crypto::Identity` are invoked during registration and device linking; TLS handshake creates a grandchild span under `account.sip.register` |
| **config_persistence** | `AccountConfig` / `Archiver` are invoked during account create and modify; no tracing needed here (synchronous, fast) |
| **im_messaging** | `MessageEngine` is owned by `Account`; its retry queue starts when an account transitions to `REGISTERED` |
| **connectivity** | `SipTransportBroker` is invoked by `SIPAccount::doRegister()` to create TLS transports; scope of the TLS handshake child span |

---

## Source References

- `src/account.h` / `src/account.cpp`
- `src/account_factory.h`
- `src/registration_states.h`
- `src/sip/sipaccount.h` / `.cpp`
- `src/jamidht/jamiaccount.h` / `.cpp`
- `src/jamidht/account_manager.h` / `.cpp`
- `src/jamidht/archive_account_manager.h` / `.cpp`
- `src/jamidht/server_account_manager.h` / `.cpp`
- `src/jamidht/namedirectory.h` / `.cpp`
- `src/connectivity/security/tlsvalidator.h`
- KB: `subsystem_account_management.md`
- KB: `subsystem_certificate_pki.md`
- KB: `integration_account_management.md` — full injection-point specification
- KB: `otel_traces.md` — async span examples
- KB: `otel_metrics.md` — UpDownCounter and Histogram patterns
- KB: `otel_semconv.md` — attribute naming rules

---

## Open Questions

1. **`getAccountManagerType()` method**: a helper returning `"archive"`, `"server"`, or `"none"` does not exist today. It must be added to `Account` (virtual, overridden in `JamiAccount` and `SIPAccount`).
2. **Thread safety of span storage**: `registrationSpan_` is read/written from both the ASIO io_context thread and the PJSIP event thread (`sipThread_`). Either protect it with the existing `configurationMutex_` or use an `std::atomic<std::shared_ptr<>>` (C++20).
3. **Account import span**: `JamiAccount::loadAccountFromFile()` can be called from multiple paths; the `account.import` root span must not accidentally nest inside an active `account.register` span.
4. **Device-link span**: multi-device linking (add device from QR/PIN) is a distinct flow not covered by `account.register`. It should have its own `account.device.link` root span with sub-spans for DHT value exchange and archive transfer.
5. **JAMS (ServerAccountManager) spans**: JAMS REST API calls (`/api/accounts`, `/api/auth`) should create a `server.account.authenticate` span as a child of `account.register`. This requires instrumentation in `ServerAccountManager::initAuthentication()`.
