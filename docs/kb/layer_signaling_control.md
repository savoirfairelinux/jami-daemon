# Layer 3 — Signaling & Control Plane

## Status: draft

## Last Updated: 2026-03-13

---

## Layer Description

Layer 3 encompasses everything involved in establishing, modifying, and tearing down a call at the **protocol level** — before a single media byte flows. It converts high-level operations from Layer 2 (account identity) and Layer 1 (public API) into concrete SIP/JAMI protocol exchanges: SDP offer/answer, ICE candidate gathering and connectivity checks, SRTP key negotiation, and the call state machine transitions that reflect the progress of those exchanges.

### Constituting Files and Classes

#### SIP Signalling Core

| Class | File | Role |
|---|---|---|
| `SIPVoIPLink` | `src/sip/sipvoiplink.h` / `.cpp` | PJSIP `pjsip_endpoint` owner; runs PJSIP event loop on a dedicated `sipThread_`; registers all invite/SDP C callbacks |
| `SIPCall` | `src/sip/sipcall.h` / `.cpp` | Owns `pjsip_inv_session*`, `Sdp`, vector of `RtpStream`, two `dhtnet::IceTransport` instances; drives full media lifecycle |
| `Sdp` | `src/sip/sdp.h` / `.cpp` | PJMEDIA SDP wrapper; builds local offer; parses remote answer; `getMediaSlots()`, `getIceAttributes()`, `remoteHasValidIceAttributes()` |
| `SdesNegotiator` | `src/sip/sdes_negotiator.h` / `.cpp` | Parses SDP `a=crypto` lines; selects SRTP cipher suite; derives session keys |
| `SIPCall::RtpStream` | `src/sip/sipcall.h` | Per-stream aggregate: `rtpSession_`, send/recv `MediaAttribute`, `rtpSocket_` + `rtcpSocket_` ICE sockets |
| `Call` | `src/call.h` / `.cpp` | Abstract base; `CallState` + `ConnectionState` pair; `validStateTransition()`; `StateListenerCb` dispatch; ring timeout timer |
| `CallFactory` | `src/call_factory.h` / `.cpp` | Thread-safe call registry keyed by `LinkType`; `newSipCall()`, `removeCall()`, `callCount()` |

#### ICE / Transport (consumed by this layer)

| Class | File | Role |
|---|---|---|
| `SipTransport` / `SipTransportBroker` | `src/sip/siptransport.h` / `.cpp` | PJSIP transport lifecycle (UDP/TCP/TLS); state listeners |
| `ChanneledTransport` | `src/jamidht/channeled_transport.h` / `.cpp` | SIP-over-dhtnet channel transport adapter; used for JAMI account calls |
| `IceTransport` | `dhtnet` (external) | ICE agent; gathers host/srflx/relay candidates; runs connectivity checks |

#### Call State Machine (enum values)

```
ConnectionState:  DISCONNECTED(0)  TRYING(1)  PROGRESSING(2)  RINGING(3)  CONNECTED(4)
CallState:        INACTIVE(0)  ACTIVE(1)  HOLD(2)  BUSY(3)  PEER_BUSY(4)  MERROR(5)  OVER(6)
```

Key PJSIP callback → Call state mappings (in `invite_session_state_changed_cb()`):

| PJSIP event | Call method | Resulting transition |
|---|---|---|
| `PJSIP_INV_STATE_EARLY` + 180 | `onPeerRinging()` | `→ RINGING` |
| `PJSIP_INV_STATE_CONFIRMED` + 200 OK + ACK | `onAnswered()` | `→ ACTIVE / CONNECTED` |
| `PJSIP_INV_STATE_DISCONNECTED` + 486 | `onBusyHere()` | `→ PEER_BUSY` |
| `PJSIP_INV_STATE_DISCONNECTED` + 487 | `onClosed()` | `→ OVER` |

---

## OTel Relevance

Layer 3 is the **single highest-value tracing target** in the daemon:

1. **Multi-step latency chain**: a call requires SDP construction → INVITE send → 100 Trying → 180 Ringing → 200 OK → ACK → ICE gather → ICE check → media start. Each step adds measurable latency, and failures at any step are a distinct UX problem. A trace spanning this chain directly answers "why is call setup slow?"

2. **ICE failure diagnosis**: ICE negotiation can fail for many reasons (STUN unreachable, TURN quota exhausted, NAT hairpin failure, candidate mismatch). Child spans on ICE phases (gathering, connectivity checks, TURN allocation) let operators distinguish between these root causes.

3. **Direct UX impact**: call setup latency is noticed by users at > 3 seconds. Histograms of `call.setup.duration` broken down by `ice.result` and `account.type` are the most actionable SLO metric in the daemon.

4. **Low cardinality**: there are O(active calls) traces — a modest volume even at scale.

---

## Recommended Signal Mix

| Signal | Instruments | Purpose |
|---|---|---|
| **Traces** | `call.outgoing` / `call.incoming` (root); `call.sdp.offer_build`, `call.sdp.negotiate` (children); `call.ice.init`, `call.ice.candidate_gathering`, `call.ice.negotiate` (children); `call.media.start` (child) | Step-by-step call setup timing; failure root cause |
| **Metrics** | `jami.call.setup.duration` Histogram; `jami.calls.active` UpDownCounter; `jami.calls.started` Counter; `jami.calls.failed` Counter (by `error.type`) | SLO dashboard, capacity planning |
| **Logs** | Bridge `JAMI_ERR` in `SIPCall::onFailure()`, `Call::setState(MERROR,...)` to OTel Log Bridge at `kError` severity with active span context injected | Structured error messages correlated to trace |

---

## Cardinality Warnings

| ⚠️ DO NOT | Reason |
|---|---|
| Use peer SIP URI (`peerNumber_`, `peerUri_`) as a metric label | PII; unbounded cardinality |
| Use peer IP address as a metric label | PII-adjacent; unbounded |
| Use `callId` (internal UUID) as a metric label | High cardinality; belongs only as a **span attribute**, never a metric label |
| Use SIP Call-ID header value as any telemetry field | Potentially correlatable to user identity across systems |
| Record codec negotiation details (codec names) at per-call granularity as metric labels | Use a `codec` label only if the set is bounded (e.g., `opus`, `h264`, `h265`, `vp8`, `g711`) |

**Approved metric label values:**
- `jami.account.type`: `"SIP"` or `"RING"`
- `call.direction`: `"outgoing"` / `"incoming"`
- `error.type`: `"sdp_failure"`, `"ice_timeout"`, `"ice_failure"`, `"tls_failure"`, `"busy"`, `"declined"`, `"timeout"`, `"media_error"` — bounded
- `ice.result`: `"success"`, `"failure"`, `"turn_fallback"` — bounded

---

## Example C++ Instrumentation Snippet

### Nested Child Span — ICE Negotiation Carried Through Call State Machine

The pattern below shows how the root call span is stored on `SIPCall` and child spans are created from it as the state machine progresses. The parent context is propagated explicitly because PJSIP callbacks fire on `sipThread_`, a different thread from where the root span was created.

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// src/sip/sipcall.h  (additions to class SIPCall)
// ─────────────────────────────────────────────────────────────────────────────
#include "opentelemetry/trace/span.h"
#include "opentelemetry/context/context.h"

class SIPCall : public Call {
    // ... existing members ...
public:
    // Span for the full call lifecycle (opened at construction, ended at OVER)
    std::shared_ptr<opentelemetry::trace::Span> callSpan_ {};
    // Saved context so PJSIP callbacks (on sipThread_) can create child spans
    opentelemetry::context::Context             callSpanContext_ {};
};
```

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// src/sip/sipcall.cpp  — outgoing call construction
// ─────────────────────────────────────────────────────────────────────────────
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/metrics/provider.h"
#include "src/telemetry/telemetry_utils.h"

namespace trace_api  = opentelemetry::trace;
namespace metric_api = opentelemetry::metrics;

// Metric instrument forward declarations (defined with static helpers as in Layer 1)
static auto& CallSetupDurationHist() { /* CreateDoubleHistogram jami.call.setup.duration ms */ }
static auto& ActiveCallsCounter()   { /* CreateInt64UpDownCounter jami.calls.active */ }
static auto& CallsStartedCounter()  { /* CreateUInt64Counter jami.calls.started */ }

SIPCall::SIPCall(const std::shared_ptr<SIPAccountBase>& account,
                 const std::string& callId,
                 Call::CallType type,
                 const std::vector<libjami::MediaMap>& mediaList)
    : Call(account, callId, type, mediaList)
{
    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.calls", "1.0.0");

    trace_api::StartSpanOptions opts;
    opts.kind = (type == CallType::OUTGOING)
                    ? trace_api::SpanKind::kClient
                    : trace_api::SpanKind::kServer;

    // If the active context already carries a parent span (e.g., from Layer 1
    // placeCall), StartSpan will automatically attach as a child.
    callSpan_ = tracer->StartSpan(
        (type == CallType::OUTGOING) ? "call.outgoing" : "call.incoming", opts);

    callSpan_->SetAttribute("jami.call.id",        callId);   // internal UUID — safe
    callSpan_->SetAttribute("jami.call.direction",
                             std::string((type == CallType::OUTGOING) ? "outgoing" : "incoming"));
    callSpan_->SetAttribute("jami.account.type",   std::string(account->getAccountType()));
    callSpan_->SetAttribute("jami.account.id",     hashForTelemetry(account->getAccountID()));

    // Save the context so PJSIP sipThread_ callbacks can create child spans.
    callSpanContext_ = opentelemetry::context::RuntimeContext::GetCurrent()
                           .SetValue(trace_api::kSpanKey, callSpan_);

    callSetupStartTime_ = std::chrono::steady_clock::now();

    std::map<std::string, std::string> ma = {
        {"jami.account.type", std::string(account->getAccountType())},
        {"call.direction",    (type == CallType::OUTGOING) ? "outgoing" : "incoming"},
    };
    CallsStartedCounter().Add(1,
        opentelemetry::common::KeyValueIterableView<decltype(ma)>{ma});
    ActiveCallsCounter().Add(+1,
        opentelemetry::common::KeyValueIterableView<decltype(ma)>{ma});
}
```

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// sipvoiplink.cpp — sdp_media_update_cb  (PJSIP callback, runs on sipThread_)
// ─────────────────────────────────────────────────────────────────────────────
static void sdp_media_update_cb(pjsip_inv_session* inv, pj_status_t status)
{
    auto call = static_cast<SIPCall*>(inv->mod_data[sipModule.id]);
    if (!call) return;

    // Restore the call's saved context so this child span is properly parented.
    auto token = opentelemetry::context::RuntimeContext::Attach(call->callSpanContext_);

    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.calls", "1.0.0");
    auto sdp_span = tracer->StartSpan("call.sdp.negotiate");
    sdp_span->SetAttribute("jami.call.id", call->getCallId());

    if (status != PJ_SUCCESS) {
        sdp_span->SetStatus(trace_api::StatusCode::kError, "SDP negotiation failed");
        sdp_span->SetAttribute("error.type", std::string("sdp_failure"));
        sdp_span->End();
        return;
    }
    // ... existing SDP processing logic ...
    sdp_span->SetAttribute("jami.sdp.audio_codec_count",
                           static_cast<int64_t>(call->getSdp().getMediaSlotCount(AUDIO)));
    sdp_span->SetAttribute("jami.sdp.ice_in_sdp",
                           call->getSdp().remoteHasValidIceAttributes());
    sdp_span->SetStatus(trace_api::StatusCode::kOk);
    sdp_span->End();
    // token destructor restores previous context
}
```

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// sipcall.cpp — onMediaNegotiationComplete() — start ICE child span
// ─────────────────────────────────────────────────────────────────────────────
void SIPCall::startIceNegotiation()
{
    auto token = opentelemetry::context::RuntimeContext::Attach(callSpanContext_);
    auto tracer = trace_api::Provider::GetTracerProvider()
                      ->GetTracer("jami.calls", "1.0.0");

    iceNegSpan_ = tracer->StartSpan("call.ice.negotiate");
    iceNegSpan_->SetAttribute("jami.call.id", getCallId());
    iceNegSpan_->AddEvent("ice.candidate_gathering.started");
    // iceNegSpan_ is stored as a member and ended in onIceStateChange(COMPLETED/FAILED)
}

void SIPCall::onIceStateChange(IceTransportState state)
{
    if (!iceNegSpan_) return;
    if (state == IceTransportState::COMPLETED) {
        iceNegSpan_->SetStatus(trace_api::StatusCode::kOk);
        iceNegSpan_->AddEvent("ice.connectivity.complete");
    } else if (state == IceTransportState::FAILED) {
        iceNegSpan_->SetAttribute("error.type", std::string("ice_failure"));
        iceNegSpan_->SetStatus(trace_api::StatusCode::kError, "ICE negotiation failed");
    }
    iceNegSpan_->End();
    iceNegSpan_.reset();
}
```

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// call.cpp — setState(OVER, ...) — end the root call span and record metrics
// ─────────────────────────────────────────────────────────────────────────────
void SIPCall::onCallOver(int cause_code)
{
    if (callSpan_) {
        auto duration_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - callSetupStartTime_).count();

        CallSetupDurationHist().Record(duration_ms, /* account.type, direction attrs */);
        ActiveCallsCounter().Add(-1, /* account.type, direction attrs */);

        if (cause_code != 0 && cause_code != 200) {
            callSpan_->SetStatus(trace_api::StatusCode::kError,
                                 "Call ended with code " + std::to_string(cause_code));
            callSpan_->SetAttribute("jami.call.end_cause", static_cast<int64_t>(cause_code));
        } else {
            callSpan_->SetStatus(trace_api::StatusCode::kOk);
        }
        callSpan_->End();
        callSpan_.reset();
    }
}
```

---

## Cross-Process Trace Propagation

### W3C TraceContext in SIP INVITE (Future / Optional)

When Jami calls a remote party that also runs a Jami daemon with OTel instrumentation, it is technically possible to propagate the W3C `traceparent` header across the SIP INVITE, enabling a **distributed trace** that spans both endpoints.

**Mechanism:**
1. At INVITE send time, extract the current `TraceContext` from the active span:
   ```cpp
   // In SIPCall::initInviteSession() before pjsip_inv_send_msg()
   std::map<std::string, std::string> carrier;
   opentelemetry::context::propagation::GlobalTextMapPropagator
       ::GetGlobalPropagator()
       ->Inject(TextMapCarrier{carrier},
                opentelemetry::context::RuntimeContext::GetCurrent());
   // Add as a custom SIP header: X-Trace-Context: traceparent=<value>
   ```
2. On INVITE receive, extract from the custom header:
   ```cpp
   // In SIPVoIPLink::onInvite() / inv_rx_offer_cb
   auto ctx = GlobalTextMapPropagator::GetGlobalPropagator()
                  ->Extract(TextMapCarrier{sip_headers}, Context::GetCurrent());
   // Use ctx as parent context when creating call.incoming span
   ```

**Custom SIP header**: `X-Trace-Context` carrying the W3C `traceparent` value.

**Status**: **Optional / future work.** This is only meaningful when both endpoints support it and when a centralised trace backend collects from both. Mark `X-Trace-Context` as a non-standard header that intermediate proxies MUST forward or silently discard. Implementation must be guarded by a runtime flag (e.g., `jami.otel.sip_propagation_enabled`) so it does not affect deployments without OTel.

---

## Subsystems in This Layer

| Subsystem | Relationship |
|---|---|
| **call_manager** | This layer directly describes the `call_manager` subsystem; `Call`, `SIPCall`, `CallFactory`, `SIPVoIPLink`, `Sdp` are the core instrumentation targets |
| **connectivity** | `SipTransportBroker`, `IceTransport` are invoked during call setup; ICE state changes drive child span events in this layer |
| **certificate_pki** | `SdesNegotiator` (SRTP key derivation from SDP `a=crypto`) and TLS transport creation are sub-operations of call setup |
| **media_pipeline** | Layer 3 ends and Layer 4 begins at `SIPCall::startAllMedia()` / `RtpSession::start()`; the `call.media.start` child span bridges these layers |
| **conference** | Conference assembly in `Conference` adds/removes participant calls; each participant's Layer 3 call span is a sibling under the same trace |
| **dht_layer** | For JAMI accounts, `dht.channel.open` (Layer 5) is a prerequisite before `call.outgoing` can reach `CONNECTED`; these span trees are linked via `SpanLink` |

---

## Source References

- `src/call.h` / `src/call.cpp`
- `src/call_factory.h` / `src/call_factory.cpp`
- `src/sip/sipcall.h` / `src/sip/sipcall.cpp`
- `src/sip/sdp.h` / `src/sip/sdp.cpp`
- `src/sip/sdes_negotiator.h`
- `src/sip/sipvoiplink.h` / `src/sip/sipvoiplink.cpp`
- `src/sip/siptransport.h`
- `src/jamidht/channeled_transport.h`
- KB: `subsystem_call_manager.md` — complete state machine documentation
- KB: `subsystem_connectivity.md` — ICE and SIP transport details
- KB: `integration_call_manager.md` — full span hierarchy and attribute contracts
- KB: `otel_traces.md` — span context propagation API
- KB: `otel_semconv.md` — RPC and network attribute naming

---

## Open Questions

1. **Span storage thread safety**: `SIPCall::callSpan_` and `callSpanContext_` are written on the io_context thread (constructor) and read on `sipThread_` (PJSIP callbacks). Because `sipThread_` and io_context are both single-threaded in their respective cycles, and `SIPCall` objects are not shared between them during span operations, this should be safe — but needs a formal review against the `SIPCall::mtx_` locking discipline.
2. **Multiple ICE transports**: `SIPCall` holds two ICE transports (`iceMedia_`, `reinvIceMedia_`). Should re-INVITE ICE create a new child span, or add an event to the existing `call.ice.negotiate` span?
3. **Forked calls (subcalls)**: `Call::isSubcall()` returns true for SIP forked legs. Should each fork get its own root span, or be children of the parent call span?
4. **Ring timeout**: `Call` has an `asio::steady_timer` for ringing timeout. This timeout-triggered span end should set `error.type = "timeout"` and `StatusCode::ERROR`. Where exactly is the timer callback — confirm it is `Call::removeCall()`.
5. **Hold/resume span parenting**: hold/resume re-INVITEs happen after the call is `ACTIVE`. Should `call.hold` and `call.resume` be children of the original root call span (via `SpanLink` since the span may have been ended), or independent root spans with a `jami.call.id` attribute for correlation?
