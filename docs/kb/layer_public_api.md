# Layer 1 — Public API / IPC Boundary

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 1 is the **entire public surface of `libjami`**: the set of header files under `src/jami/` that UIs and bindings consume. No UI should ever `#include` anything from `src/sip/`, `src/jamidht/`, or `src/media/` directly. All client interactions enter the daemon exclusively through these headers.

### Constituting Files and Classes

| File | Namespace / Functions | Role |
|---|---|---|
| `src/jami/jami.h` | `libjami::init()`, `start()`, `fini()`, `version()` | Daemon lifecycle: init flags, provider registration |
| `src/jami/callmanager_interface.h` | `libjami::placeCall()`, `accept()`, `hangUp()`, `hold()`, `resume()`, `transfer()`, `attendedTransfer()`, `muteLocalMedia()`, `requestMediaChange()`, `joinParticipant()`, `createConfFromParticipantList()` | Call operations and conferencing |
| `src/jami/configurationmanager_interface.h` | `libjami::addAccount()`, `setAccountDetails()`, `getAccountDetails()`, `addDevice()`, `exportToFile()`, `revokeDevice()`, `getConnectionList()`, `sendTextMessage()` | Account management, device linking, configuration |
| `src/jami/conversation_interface.h` | `libjami::startConversation()`, `sendMessage()`, `getSwarmMessages()`, `acceptConversationRequest()`, `addConversationMember()` | Swarm-backed group messaging |
| `src/jami/datatransfer_interface.h` | `libjami::sendFile()`, `downloadFile()`, `dataTransferInfo()`, `acceptFileTransfer()`, `cancelDataTransfer()` | File transfer operations |
| `src/jami/videomanager_interface.h` | Video sink/source APIs, `getDeviceList()`, `setDefaultDevice()` | Video device management and UI frame injection |
| `src/jami/presencemanager_interface.h` | `libjami::subscribeBuddy()`, `sendPresence()` | Presence publication/subscription |
| `src/jami/plugin_manager_interface.h` | `libjami::loadPlugin()`, `unloadPlugin()`, `installPlugin()`, `getInstalledPlugins()` | Runtime plugin management |
| `src/jami/account_const.h`, `call_const.h`, `media_const.h`, `security_const.h` | Shared string constant tables | Canonical key/value names for detail maps |
| `src/jami/def.h` | `LIBJAMI_PUBLIC` macro | Symbol visibility |
| `src/jami/trace-tools.h`, `tracepoint.h` | LTTng tracepoint macros | Existing static instrumentation hooks |

### IPC / Binding Layers That Consume This Surface

| Binding | Location | Mechanism |
|---|---|---|
| D-Bus (Linux desktop) | `bin/dbus/` | XML introspection generated from annotations; `sdbus-c++` serialisation |
| JNI / Android | `bin/jni/` | SWIG-generated Java wrappers |
| Node.js | `bin/nodejs/` | SWIG-generated JavaScript bindings |
| Qt / GNOME UIs | Directly link against `libjami` | C function calls; signal callbacks |
| iOS | Directly link `libjami.a` | Objective-C++ wrapper layer |

The `Manager` singleton (`src/manager.h`) receives every API call from these bindings and dispatches to the correct subsystem via the ASIO `io_context`.

---

## OTel Relevance

Layer 1 is the **natural root-span boundary** for all user-initiated and network-initiated operations:

- **User-initiated operations** (e.g., `placeCall`, `sendMessage`, `addAccount`) arrive at this layer from external clients. A root span created here captures the complete end-to-end latency of the operation, from client request to final state change.
- **Network-initiated events** (incoming call, incoming message, device announcement) surface at this layer as signal emissions back to the client. These should also be root spans: `call.incoming`, `message.received`, etc.
- **Client SDK context injection**: if a Jami client application has its own OTel instrumentation, it can inject a `traceparent` value (W3C TraceContext) as a parameter when calling `libjami` API functions. The daemon would then call `opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator()->Extract(carrier, ctx)` to continue the trace across the process boundary.

### Why This Layer Is Critical for Observability

Every subsystem operation ultimately originates here. Without root spans at this layer, traces from deeper layers (signalling, media) become orphans with no business-level context. Placing the root span at Layer 1 allows operators to answer: *"How long did it take from when the user pressed Call until the call was established?"*

---

## Recommended Signal Mix

| Signal | Instruments | Rationale |
|---|---|---|
| **Traces** | Root spans on each `placeCall`, `accept`, `hangUp`, `addAccount`, `sendMessage`, `sendFile` call | Low-frequency, high-value; direct mapping to user actions |
| **Metrics** | `jami.api.calls.total` Counter (by method name); `jami.api.errors.total` Counter (by method, error type) | Request rate and error rate; essential for SLO dashboards |
| **Logs** | Not primary — existing `JAMI_DBG`/`JAMI_ERR` macros cover this layer | Bridge Logger to OTel Logs Bridge API for structured correlation |

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use call peer URI, SIP address, or display name as a metric label | Unbounded cardinality; PII exposure |
| Use the raw `callId` string as a metric label | High cardinality; `callId` belongs only in span attributes, never metric labels |
| Create per-connection histograms | One histogram instrument per connection = unbounded label set growth |
| Record method argument values (toUri, accountDetails map values) as span attributes | May contain user credentials or personal data |
| Use `accountId` raw string as a metric label | Must be hashed (SHA-256 truncated to 16 hex chars); or use `account.type` (`SIP`/`RING`) as coarse label |

**Acceptable metric label values at this layer:**
- `jami.account.type`: `"SIP"` or `"RING"` — bounded enumeration
- `call.direction`: `"outgoing"` or `"incoming"` — bounded enumeration
- `api.method`: the low-cardinality method name string (e.g., `"placeCall"`, `"hangUp"`) — bounded

---

## Example C++ Instrumentation Snippet

The following shows how to create a root span at the `CallManager::placeCall` entry point, attach key attributes, propagate the span context into the daemon, and record the result.

```cpp
// src/jami/callmanager.cpp  (implementation file for callmanager_interface.h)
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/metrics/provider.h"
#include "src/logger.h"

namespace trace_api  = opentelemetry::trace;
namespace metric_api = opentelemetry::metrics;

// ── Instrument singletons (created once, cached) ──────────────────────────────
static auto& GetCallsStartedCounter() {
    static auto counter = metric_api::Provider::GetMeterProvider()
        ->GetMeter("jami.public_api", "1.0.0")
        ->CreateUInt64Counter("jami.api.calls.started",
                              "Total outgoing call attempts via public API",
                              "{calls}");
    return *counter;
}

static auto& GetApiErrorsCounter() {
    static auto counter = metric_api::Provider::GetMeterProvider()
        ->GetMeter("jami.public_api", "1.0.0")
        ->CreateUInt64Counter("jami.api.errors.total",
                              "Total API call errors",
                              "{errors}");
    return *counter;
}

// ── placeCall instrumented entry point ────────────────────────────────────────
std::string
CallManager::placeCall(const std::string& accountId,
                       const std::string& to,
                       const std::vector<libjami::MediaMap>& mediaList)
{
    // 1. Acquire tracer (cached; same instance returned each call)
    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.public_api", "1.0.0");

    // 2. Create root span — direction and account type are low-cardinality, safe as attrs
    trace_api::StartSpanOptions opts;
    opts.kind = trace_api::SpanKind::kClient;
    auto span = tracer->StartSpan("call.outgoing", opts);

    // 3. Attach safe, low-cardinality attributes
    //    NEVER set `to` (peer URI) or accountId raw value!
    span->SetAttribute("jami.call.direction", std::string("outgoing"));
    span->SetAttribute("jami.call.has_video", !mediaList.empty() &&
                        std::any_of(mediaList.begin(), mediaList.end(),
                            [](const auto& m) { return m.count("VIDEO") > 0; }));
    // accountId is hashed for the span attribute
    span->SetAttribute("jami.account.id", hashForTelemetry(accountId));
    span->SetAttribute("rpc.system", std::string("jami"));
    span->SetAttribute("rpc.method", std::string("placeCall"));

    // 4. Make span active in the current thread's context so child spans
    //    (created in SIPCall, ICE negotiation, etc.) inherit it automatically
    auto scope = tracer->WithActiveSpan(span);

    // 5. Record metric — account type is safe as label, raw ID is not
    auto account = Manager::instance().getAccount(accountId);
    std::string account_type = account ? account->getAccountType() : "unknown";
    std::map<std::string, std::string> metric_attrs = {
        {"jami.account.type", account_type},
        {"call.direction",    "outgoing"},
    };
    auto kv = opentelemetry::common::KeyValueIterableView<decltype(metric_attrs)>{metric_attrs};
    GetCallsStartedCounter().Add(1, kv);

    // 6. Dispatch to Manager (which will create SIPCall and emit child spans)
    std::string callId;
    try {
        callId = Manager::instance().outgoingCall(accountId, to, mediaList);
    } catch (const std::exception& e) {
        span->SetStatus(trace_api::StatusCode::kError, e.what());
        span->SetAttribute("error.type", std::string("exception"));
        GetApiErrorsCounter().Add(1, {{"api.method", "placeCall"},
                                      {"error.type", "exception"}});
        span->End();
        return {};
    }

    // 7. Attach the internal call ID (safe — it is an internal UUID, not a SIP URI)
    if (!callId.empty()) {
        span->SetAttribute("jami.call.id", callId);
        span->SetStatus(trace_api::StatusCode::kOk);
    } else {
        span->SetStatus(trace_api::StatusCode::kError, "call creation returned empty ID");
        GetApiErrorsCounter().Add(1, {{"api.method", "placeCall"},
                                      {"error.type", "no_call_id"}});
    }

    // NOTE: span->End() is NOT called here — the span lives until the call
    // reaches state OVER. It is stored in SIPCall and ended in
    // Call::setState(CallState::OVER, ...).  scope goes out of scope here,
    // detaching the span from this thread's context, but the span itself
    // remains open in the SIPCall object.
    span->End();  // simplification: in practice, defer End() to call teardown
    return callId;
}
```

**Helper for safe identifier hashing (place in a shared telemetry utilities header):**

```cpp
// src/telemetry/telemetry_utils.h
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

/// Returns the first 16 hex chars of SHA-256(input).
/// Use this whenever an account ID, device ID, or call peer must
/// appear as a span attribute.
inline std::string hashForTelemetry(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),
           input.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i)          // first 8 bytes = 16 hex chars
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    return oss.str();
}
```

---

## Subsystems in This Layer

The following subsystems are directly invoked by or directly emit signals to Layer 1:

| Subsystem | Relationship |
|---|---|
| **account_management** | `configurationmanager_interface.h` is the client-facing face of the account subsystem; `addAccount`, `setAccountDetails`, device operations all enter here |
| **call_manager** | `callmanager_interface.h` surfaces `placeCall`, `accept`, `hangUp` and all call state change signals |
| **conference** | `callmanager_interface.h` surfaces `createConfFromParticipantList`, `joinParticipant`; `conversation_interface.h` surfaces `hostConference`, `getActiveCalls` |
| **data_transfer** | `datatransfer_interface.h` is the complete client API for file transfers |
| **im_messaging** | `configurationmanager_interface.h` (`sendTextMessage`) and `conversation_interface.h` (`sendMessage`) |
| **plugin_system** | `plugin_manager_interface.h` is the exclusive client entry point |
| **logging** | `Logger::Handler` is configured during `libjami::init()` at this layer; log handler callback registered here by the client |

---

## Source References

- `src/jami/jami.h`
- `src/jami/callmanager_interface.h`
- `src/jami/configurationmanager_interface.h`
- `src/jami/conversation_interface.h`
- `src/jami/datatransfer_interface.h`
- `src/jami/videomanager_interface.h`
- `src/jami/presencemanager_interface.h`
- `src/jami/plugin_manager_interface.h`
- `src/jami/def.h`
- `src/jami/trace-tools.h`
- `src/manager.h`
- KB: `subsystem_overview.md` — IPC/API Boundary section
- KB: `integration_call_manager.md` — root span injection context
- KB: `integration_account_management.md` — metric label design

---

## Open Questions

1. **Deferred span lifetime**: the root `call.outgoing` span opened at `placeCall` should ideally end when the call reaches `OVER`. This requires the span to be stored in `SIPCall` and retrieved at `Call::setState(OVER, ...)`. What is the cleanest mechanism — store in `SIPCall` as `std::shared_ptr<trace::Span>` or in a side-table keyed by `callId`?
2. **Signal-side spans**: should incoming call notifications emitted back to the client via signal (e.g., `callStateChanged`) open a separate root span, or should they be events on the existing `call.incoming` span?
3. **W3C TraceContext injection**: no existing API parameter accepts a `traceparent` string today. Should a future `placeCall` overload accept an optional `std::map<std::string, std::string> otel_carrier`, or should the daemon read it from an environment variable / daemon config?
4. **D-Bus / JNI boundary**: if the Qt client has its own OTel SDK, the trace context cannot flow across D-Bus XML calls automatically. Is there appetite to add a `traceparent` D-Bus method argument to `callmanager.xml`?
5. **`libjami::init()` timing**: the OTel `TracerProvider` and `MeterProvider` must be configured before `libjami::init()` returns. This implies the caller (the UI or `jamid` main) owns SDK initialisation. Document the expected initialisation order and whether `libjami` should offer an `injectOtelProvider()` hook.
