# Integration Plan: Call Manager

## Status: draft

## Last Updated: 2026-03-13

---

## Proposed Span Hierarchy

Spans are shown as trees. Indentation encodes parent → child relationship. All spans belong to the instrumentation scope `jami.calls` / `1.0.0`.

### Outgoing call

```
call.outgoing  (ROOT, SpanKind::CLIENT)
 ├── call.sdp.offer_build  (INTERNAL)
 ├── call.ice.init  (INTERNAL)
 │    └── call.ice.candidate_gathering  (INTERNAL)
 ├── [SIP network — not a span; represented by call.outgoing duration]
 ├── call.sdp.negotiate  (INTERNAL)   ← triggered by sdp_media_update_cb
 ├── call.ice.negotiate  (INTERNAL)   ← only when ICE enabled
 └── call.media.start  (INTERNAL)
      ├── call.media.stream.start  (INTERNAL)  [one per RtpStream, label as attr]
      └── call.media.stream.start  (INTERNAL)
```

### Incoming call

```
call.incoming  (ROOT, SpanKind::SERVER)
 ├── call.sdp.offer_parse  (INTERNAL)
 ├── call.ice.init  (INTERNAL)
 │    └── call.ice.candidate_gathering  (INTERNAL)
 ├── call.sdp.negotiate  (INTERNAL)
 ├── call.ice.negotiate  (INTERNAL)
 └── call.media.start  (INTERNAL)
      └── call.media.stream.start  (INTERNAL)  [one per RtpStream]
```

### Hold / Resume

```
call.hold  (INTERNAL, child of the active call span via link or baggage)
 └── call.sdp.negotiate  (INTERNAL)   ← re-INVITE SDP round-trip

call.resume  (INTERNAL)
 └── call.sdp.negotiate  (INTERNAL)
 └── call.media.start  (INTERNAL)
```

### Transfer

```
call.transfer  (INTERNAL)
call.transfer.attended  (INTERNAL)
```

---

### Span Definitions

#### `call.outgoing` / `call.incoming`

| Field | Value |
|---|---|
| Kind | `SpanKind::CLIENT` (outgoing) / `SpanKind::SERVER` (incoming) |
| Start | `SIPCall::SIPCall()` constructor (immediately after `jami_tracepoint(call_start, ...)`) |
| End | `Call::setState(CallState::OVER, ...)` inside `Call::removeCall()` |
| Status ERROR when | `SIPCall::onFailure()` called; `CallState` transitions to `MERROR`; `Call::removeCall()` called with non-zero `code`; ringing timeout fires |

Key attributes (`jami.*` namespace):

| Attribute | Type | Source | Notes |
|---|---|---|---|
| `jami.call.id` | string | `Call::getCallId()` | Internal UUID. Not a SIP Call-ID; safe to record |
| `jami.call.direction` | string | `Call::getCallType()` → `"outgoing"` / `"incoming"` | |
| `jami.account.id` | string | `Call::getAccountId()` | Account UUID |
| `jami.account.type` | string | from account | `"SIP"` / `"RING"` |
| `jami.call.link_type` | string | `Call::getLinkType()` | `"SIP"` |
| `jami.call.is_subcall` | bool | `Call::isSubcall()` | True for forked legs |
| `jami.call.media_count` | int | `rtpStreams_.size()` | Number of RTP streams |
| `jami.call.has_video` | bool | `SIPCall::hasVideo()` | |
| `jami.call.srtp_enabled` | bool | `SIPCall::isSrtpEnabled()` | |
| `jami.call.ice_enabled` | bool | `SIPCall::isIceEnabled()` | |

**Do NOT record**: `peerNumber_`, `peerDisplayName_`, `toUsername_`, SIP URI, `peerUri_`, contact header. See [Privacy Considerations](#privacy-considerations).

---

#### `call.sdp.offer_build` / `call.sdp.offer_parse`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `Sdp::setLocalMediaCapabilities()` call in `SIPCall::SIPCall()` (outgoing) or `sdp_create_offer_cb` / `on_rx_offer2` (incoming) |
| End | After `pjsip_inv_send_msg` sends the INVITE (outgoing) or after `Sdp` parses remote offer (incoming) |
| Status ERROR when | `SdpException` thrown |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.sdp.direction` | string | `"offer"` / `"answer"` |
| `jami.sdp.audio_codec_count` | int | from `getMediaSlots()` |
| `jami.sdp.video_codec_count` | int | from `getMediaSlots()` |
| `jami.sdp.ice_in_sdp` | bool | `remoteHasValidIceAttributes()` |

---

#### `call.sdp.negotiate`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | Entry of `sdp_media_update_cb()` in `sipvoiplink.cpp` |
| End | Exit of `SIPCall::onMediaNegotiationComplete()` (after branching to ICE or no-ICE path) |
| Status ERROR when | `sdp_media_update_cb` receives `status != PJ_SUCCESS` (PJMEDIA returns `PJSIP_SC_UNSUPPORTED_MEDIA_TYPE`) |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.sdp.ice_path` | bool | Whether `startIceMedia()` was invoked |
| `jami.sdp.stream_count` | int | Number of active media slots |

---

#### `call.ice.init`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `SIPCall::createIceMediaTransport()` → `initIceMediaTransport()` |
| End | `IceTransport` initialization done callback |
| Status ERROR when | `IceTransport` init fails; `createIceMediaTransport()` returns false |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.ice.is_reinvite` | bool | Whether this is a re-INVITE ICE session |
| `jami.ice.role` | string | `"controlling"` / `"controlled"` |

---

#### `call.ice.candidate_gathering`

| Field | Value |
|---|---|
| Kind | `INTERNAL`, child of `call.ice.init` |
| Start | Inside `initIceMediaTransport()` when STUN/TURN gathering begins |
| End | Gathering complete callback |
| Status ERROR when | `IceTransport` reports failure or `DEFAULT_ICE_INIT_TIMEOUT` (35 s) exceeded |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.ice.candidate_count` | int | Local candidates gathered |
| `jami.ice.has_srflx` | bool | Whether server-reflexive candidates were gathered |
| `jami.ice.has_relay` | bool | Whether relay (TURN) candidates were gathered |

---

#### `call.ice.negotiate`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `SIPCall::startIceMedia()` — after `IceTransport::startIce()` is called |
| End | `SIPCall::onIceNegoSucceed()` entry |
| Status ERROR when | `IceTransport::isFailed()` true; `onFailure(PJSIP_SC_NOT_ACCEPTABLE_HERE)` called from `startIceMedia()` |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.ice.stream_count` | int | `rtpStreams_.size()` |
| `jami.ice.component_count` | int | `rtpStreams_.size() * ICE_COMP_COUNT_PER_STREAM` |

---

#### `call.media.start`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `SIPCall::startAllMedia()` entry |
| End | All per-stream `rtpSession->start()` tasks posted to `dht::ThreadPool::io()` have completed (use a counter or `AtomicBool` latch) |
| Status ERROR when | Any `rtpSession->start()` throws or returns a failure signal |

Attributes:

| Attribute | Type | Value |
|---|---|---|
| `jami.media.stream_count` | int | `rtpStreams_.size()` |
| `jami.media.ice_transport_active` | bool | Whether ICE sockets are passed to `rtpSession->start()` |

---

#### `call.media.stream.start`

One child span per `RtpStream`:

| Attribute | Type | Value |
|---|---|---|
| `jami.media.stream.type` | string | `"audio"` / `"video"` |
| `jami.media.stream.label` | string | `rtpStream.mediaAttribute_->label_` |
| `jami.media.stream.muted` | bool | `mediaAttribute_->muted_` |
| `jami.media.stream.enabled` | bool | `mediaAttribute_->enabled_` |

---

#### `call.hold` / `call.resume`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `SIPCall::hold()` / `SIPCall::resume()` entrance |
| End | `onMediaNegotiationComplete()` for re-INVITE completes + `startAllMedia()` done |
| Status ERROR when | `SIPSessionReinvite()` returns non-`PJ_SUCCESS` |

Attributes: `jami.call.id`.

---

#### `call.transfer` / `call.transfer.attended`

| Field | Value |
|---|---|
| Kind | `INTERNAL` |
| Start | `SIPCall::transfer()` / `SIPCall::attendedTransfer()` |
| End | BYE sent (local leg) or error |
| Status ERROR when | `transferCommon()` returns false |

---

## Proposed Metric Instruments

All instruments use meter scope `jami.calls` / `1.0.0`.

| Name | Type | Unit | Description | Labels |
|---|---|---|---|---|
| `jami.calls.active` | `Int64UpDownCounter` | `{calls}` | Number of calls currently not in OVER or INACTIVE state | `call.direction` (`outgoing`/`incoming`), `jami.account.type` |
| `jami.calls.total` | `Uint64Counter` | `{calls}` | Total calls ever created | `call.direction`, `jami.account.type` |
| `jami.calls.failed` | `Uint64Counter` | `{calls}` | Calls that terminated with MERROR or an error code | `error.type` (SIP status code string e.g. `"486"`, `"408"`, `"ice_failure"`, `"timeout"`), `call.direction` |
| `jami.call.setup.duration` | `DoubleHistogram` | `ms` | Time from `SIPCall` constructor to `ConnectionState::CONNECTED` | `call.direction`, `jami.call.ice_enabled`, `jami.account.type` |
| `jami.call.duration` | `DoubleHistogram` | `s` | Total call duration from `duration_start_` to `CallState::OVER` (`getCallDuration()`) | `call.direction`, `jami.account.type` |
| `jami.call.ice.init.duration` | `DoubleHistogram` | `ms` | Time for ICE transport initialization | `jami.call.ice_enabled`, `jami.ice.role` |
| `jami.call.ice.negotiate.duration` | `DoubleHistogram` | `ms` | Time for ICE connectivity checks (start → running) | `jami.ice.role`, `jami.ice.has_relay` |
| `jami.call.sdp.negotiate.duration` | `DoubleHistogram` | `ms` | Time inside `sdp_media_update_cb` + `onMediaNegotiationComplete` | `jami.sdp.ice_path` |
| `jami.call.media.start.duration` | `DoubleHistogram` | `ms` | Time for `startAllMedia()` to complete all streams | `jami.media.stream_count`, `jami.call.has_video` |
| `jami.calls.hold.total` | `Uint64Counter` | `{events}` | Number of hold operations | `jami.account.type` |
| `jami.calls.resume.total` | `Uint64Counter` | `{events}` | Number of resume operations | `jami.account.type` |
| `jami.calls.transfer.total` | `Uint64Counter` | `{events}` | Number of transfer operations | `transfer.type` (`blind`/`attended`) |
| `jami.calls.ringing_timeout.total` | `Uint64Counter` | `{events}` | Calls dropped because ringing timeout expired | `call.direction` |

**Recommended histogram bucket boundaries:**

```cpp
// call.setup.duration (ms)  — ICE adds 500 ms–5 s
{50, 100, 250, 500, 1000, 2000, 3000, 5000, 10000, 35000}

// call.duration (s)
{5, 30, 60, 300, 900, 1800, 3600, 7200}

// ice.init.duration + ice.negotiate.duration (ms)
{100, 250, 500, 1000, 2000, 5000, 10000, 35000}
```

---

## Code Injection Points

### `call.outgoing` / `call.incoming` (root span)

**Start:** `SIPCall::SIPCall()` in [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp), immediately after the `jami_tracepoint(call_start, callId.c_str())` line (~line 110).

```cpp
// After: jami_tracepoint(call_start, callId.c_str());
auto span = tracer->StartSpan(
    type == Call::CallType::INCOMING ? "call.incoming" : "call.outgoing",
    {opentelemetry::trace::SpanKind::kServer /* or kClient */});
callSpan_ = std::move(span);   // store in SIPCall private member
```

**End:** `Call::removeCall()` in [src/call.cpp](../../src/call.cpp) before `setState(CallState::OVER, code)` (~line 135). At this point `code` is available as the termination reason.

```cpp
if (callSpan_) {
    if (code != 0 && code != PJSIP_SC_OK && code != PJSIP_SC_REQUEST_TERMINATED)
        callSpan_->SetStatus(opentelemetry::trace::StatusCode::kError,
                             std::to_string(code));
    callSpan_->End();
    callSpan_.reset();
}
```

**Active-call gauge:** increment in `CallFactory::newSipCall()` after inserting into `callMaps_`; decrement in `CallFactory::removeCall()` before erasing.

---

### `call.sdp.negotiate`

**Start:** `sdp_media_update_cb()` in [src/sip/sipvoiplink.cpp](../../src/sip/sipvoiplink.cpp) (~line 1044), at the entry of the function after the null check:

```cpp
auto sdpSpan = tracer->StartSpan("call.sdp.negotiate",
    {{SpanKind::kInternal}, {/* parent = callSpan_ from SIPCall */}});
```

**End:** End of `SIPCall::onMediaNegotiationComplete()` in [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) (~line 2624), after the ICE/no-ICE branch. Set ERROR if `status != PJ_SUCCESS` in `sdp_media_update_cb`.

---

### `call.ice.init` + `call.ice.candidate_gathering`

**Start:** `SIPCall::createIceMediaTransport()` in [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp).

**`call.ice.candidate_gathering` start:** Inside `initIceMediaTransport()`, after `IceTransportFactory::createTransport()` returns.

**End (gathering):** ICE `onInitDone` callback — the lambda registered with `IceTransport` (see the `waitForIceInit_` atomics and related code around line 2672 in sipcall.cpp).

**End (`ice.init`):** Gathering complete callback, or on failure.

---

### `call.ice.negotiate`

**Start:** `SIPCall::startIceMedia()` in [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) (~line 2654), just before `iceMedia->startIce(rem_ice_attrs, ...)`.

**End:** `SIPCall::onIceNegoSucceed()` (~line 2693), at entry. Set ERROR if `startIceMedia()` calls `onFailure()`.

---

### `call.media.start` + `call.media.stream.start`

**Start:** `SIPCall::startAllMedia()` (~line 2008).

**Per-stream child start:** Inside the `dht::ThreadPool::io().run(...)` lambda per `rtpStream`, before `rtpSession->start()`.

**Per-stream child end:** After `rtpSession->start()` returns (inside the lambda).

**`call.media.start` end:** Implement a `std::atomic<int>` countdown; decrement on each stream completion; end parent span when counter reaches zero. Alternatively use a wrapper callback on `RtpSession::setSuccessfulSetupCb` which already exists.

---

### `call.hold` / `call.resume`

**Start:** `SIPCall::hold()` private method (called from `SIPCall::hold(OnReadyCb&&)` after the ICE-pending check).

**End:** Inside the re-`onMediaNegotiationComplete()` path triggered by the re-INVITE's `sdp_media_update_cb`, or on `SIPSessionReinvite()` failure.

---

### Metric recording locations

| Metric | Where |
|---|---|
| `jami.calls.total` | `CallFactory::newSipCall()` — after inserting into `callMaps_` |
| `jami.calls.active` +1 | Same location |
| `jami.calls.active` -1 | `CallFactory::removeCall()` — before erasing |
| `jami.calls.failed` | `Call::setState(CallState::MERROR)` inside `Call::setState()` overloads, or `SIPCall::onFailure()` |
| `jami.call.setup.duration` | `SIPCall::onAnswered()` — compute elapsed since `SIPCall` construction timestamp |
| `jami.call.duration` | `Call::removeCall()` — `getCallDuration().count()` is already computed; record here |
| `jami.call.ice.init.duration` | ICE init done callback |
| `jami.call.ice.negotiate.duration` | `SIPCall::onIceNegoSucceed()` entry — compute elapsed since `startIceMedia()` |
| `jami.call.sdp.negotiate.duration` | `SIPCall::onMediaNegotiationComplete()` exit |
| `jami.call.media.start.duration` | After all `start()` tasks complete (see countdown approach above) |
| `jami.calls.ringing_timeout.total` | Inside the `asio::steady_timer` callback in `Call::Call()` when `callShPtr->hangup()` is called ~line 95 in call.cpp |

---

## Context Propagation Strategy

The call subsystem is entirely intra-process; there is no HTTP/gRPC boundary to inject W3C `traceparent` headers into. Context propagation is therefore via **in-process span parenting**, not wire injection.

```
Client API thread
  └─ call.outgoing span created in SIPCall::SIPCall()
       └─ store as SIPCall::callSpan_ (opentelemetry::nostd::shared_ptr<Span>)

PJSIP thread (callbacks arrive asynchronously)
  └─ sdp_media_update_cb
       └─ start call.sdp.negotiate with explicit parent context:
            opentelemetry::trace::SpanContext ctx = call->callSpan_->GetContext();
            StartSpan("call.sdp.negotiate", parent=ctx)

main thread (runOnMainThread post)
  └─ onMediaNegotiationComplete
       └─ startIceMedia → call.ice.negotiate with parent = callSpan_->GetContext()

dht::ThreadPool::io()
  └─ per-stream rtpSession->start()
       └─ call.media.stream.start — parent passed via captured shared_ptr to context
```

**Implementation pattern:** Store the call's root `opentelemetry::trace::SpanContext` as a value (not a shared pointer to `Span`) in `SIPCall` to avoid keeping the span artificially alive. Child spans retrieve it via `StartSpan(..., {SpanKind, parent_context})`.

For conference scenarios, an additional span link (not parent) from `call.incoming` to the `conference.call` span should be used to express the relationship without forcing a parent-child hierarchy across subsystems.

---

## Privacy Considerations

### MUST NOT record (ever)

| Data | Location in code | Reason |
|---|---|---|
| `peerNumber_` / `Call::getPeerNumber()` | `call.h` | Phone number or JAMI URI of peer — directly identifies a person |
| `peerDisplayName_` / `Call::getPeerDisplayName()` | `call.h` | Human-readable name of peer |
| `toUsername_` / `Call::toUsername()` | `call.h` | SIP "To" username — identifies destination account |
| `peerUri_` / `SIPCall::peerUri()` | `sipcall.h` | Full peer SIP URI |
| `contactHeader_` / `SIPCall::getContactHeader()` | `sipcall.h` | SIP Contact header contains IP address and URI |
| `peerUserAgent_` | `sipcall.h` | Can be used for fingerprinting |
| SDP session content | `Sdp` object | May contain IP addresses, codec preferences usable for fingerprinting |
| `peerRegisteredName_` | `sipcall.h` | Registered display name — PII |
| Audio/video content | `RtpSession` | Media frames must never reach telemetry |

### MAY record (anonymized / non-identifying)

| Data | Why safe |
|---|---|
| `Call::getCallId()` — internal UUID | Random UUID with no user identity; rotated per call |
| `Call::getAccountId()` — account UUID | Identifies which account, not which human |
| `Call::getCallType()` — INCOMING/OUTGOING | Directional flag, no identity |
| Call durations and latencies | Aggregate statistics, no identity |
| Media type flags (has_video, stream_count, ice_enabled, srtp_enabled) | Technical configuration |
| Error codes (SIP status codes, ICE failure reason) | Numeric codes only |
| ICE candidate types (srflx/relay presence as bool) | Network topology hints, not addresses |
| Codec type (audio/video) | Technical, not personal |

**Note on SIP status codes:** `486 Busy Here` paired with a timestamp is borderline — it can imply a person was at a certain state. Record only as a counter label, never with peer identity.

---

## Thread Safety Notes

### Span lifecycle across thread boundaries

The root span (`callSpan_`) is created on the calling thread in `SIPCall::SIPCall()` and ended on whichever thread calls `Call::removeCall()` (can be ASIO io_context, dht::ThreadPool::io(), or the client thread). The OTel C++ SDK's `Span` interface is thread-safe for concurrent `AddEvent`, `SetAttribute`, and `SetStatus` calls, but:

1. **`callSpan_->End()` must be called exactly once.** Use a `std::once_flag` or `std::atomic_bool ended_` guard inside `SIPCall::removeCall()` override to prevent double-ending if the call is cleaned up from multiple paths.

2. **Child spans started on the PJSIP thread** must capture the parent `SpanContext` (a value type) rather than a `Span*` or `shared_ptr<Span>` to avoid locking across thread boundaries. Copying a `SpanContext` is safe and cheap.

3. **`call.media.stream.start` spans** are created and ended entirely on `dht::ThreadPool::io()` worker threads. Use the captured `SpanContext` pattern. Do not share `Scope` objects across threads.

4. **`call.ice.negotiate` span** starts on main thread (inside `runOnMainThread` post) via `startIceMedia()`, ends on a dhtnet ICE thread in `onIceNegoSucceed()`. This is an explicit cross-thread span. Store the context in a `std::atomic` or pass it as a `SpanContext` value to `onIceNegoSucceed`.

5. **`SIPCall::callMutex_` must NOT be held when calling any OTel SDK method.** OTel SDK internal locks + `callMutex_` creates a potential deadlock. End/close telemetry operations before re-acquiring `callMutex_`.

6. **ICE callback reentrancy:** `onIceNegoSucceed()` acquires `callMutex_` from the dhtnet ICE thread. Ending the `call.ice.negotiate` span should happen before acquiring that lock to avoid holding OTel locks under `callMutex_`.

---

## Risks & Complications

### 1. Multi-device forked calls (subcalls)

JAMI supports multi-device call forking via `Call::addSubCall()`. A parent call can have multiple child `SIPCall` instances (one per device). Each child has `isSubcall() == true`.  
**Risk:** Naively instrumenting every `SIPCall` constructor creates sibling root spans that have no relationship. Subcall spans should be children of the parent call's root span.  
**Mitigation:** Check `isSubcall()` in `SIPCall::SIPCall()`. If subcall, retrieve parent call's `SpanContext` via `parent_->callSpan_->GetContext()` and use it as the parent.

### 2. Re-INVITE and re-negotiation

`SIPSessionReinvite()` triggers a second pass through `sdp_media_update_cb` → `onMediaNegotiationComplete()` → `startIceMedia()` → `onIceNegoSucceed()` → `startAllMedia()`. These re-use the same call ID but represent a different operation (hold, resume, media change, device orientation).  
**Mitigation:** Use `AddEvent` on the root call span rather than creating new child spans for re-INVITE paths, annotated with `event.name = "call.reinvite"` and a `jami.reinvite.reason` attribute (`"hold"`, `"resume"`, `"media_change"`). Alternatively, create re-INVITE child spans with a `jami.call.reinvite = true` attribute plus `jami.reinvite.index` counter.

### 3. ICE reuse in re-INVITEs

The `REUSE_ICE_IN_REINVITE_REQUIRED_VERSION` version negotiation means some peers reuse the existing ICE transport across re-INVITEs (no new `createIceMediaTransport()` call). The `call.ice.init` and `call.ice.negotiate` spans must only be emitted when a new ICE session is actually created, not during every re-INVITE.  
**Mitigation:** Gate span creation on `reinvIceMedia_ != nullptr` (new ICE created) inside `SIPSessionReinvite()`.

### 4. `waitForIceInit_` race window

In `startIceMedia()`, if `IceTransport` is not yet initialized (`!iceMedia->isInitialized()`), it sets `waitForIceInit_ = true` and returns. The ICE init completion fires later in a separate callback that calls `startIceMedia()` again. This means `call.ice.negotiate` span semantics are split across two call-site invocations.  
**Mitigation:** Record the span start time when `waitForIceInit_` is set; store the start time as a member (`iceNegotiateStartTime_`) and open the actual span in the callback.

### 5. `startAllMedia()` is asynchronous

Individual `rtpSession->start()` calls are posted to `dht::ThreadPool::io()` as separate lambdas. There is no join point in the current code; `startAllMedia()` returns before all streams are actually running. The `setSuccessfulSetupCb` callback is the only post-facto signal.  
**Mitigation:** Use `setSuccessfulSetupCb` (which already calls `rtpSetupSuccess()`) as the end point for `call.media.stream.start`. Use an `std::atomic<int>` countdown initialized to `rtpStreams_.size()` stored in `SIPCall` for the parent `call.media.start` span.

### 6. `SIPCall::callMutex_` is `std::recursive_mutex`

Because it is recursive, the same thread can acquire it multiple times. However, `setState()` is called from `StateListenerCb` while holding the lock. OTel span operations must not be done inside the locked section to avoid priority inversion with OTel SDK internal locks.

### 7. Call object may be destroyed before spans end

If `SIPCall` is destroyed (ref-count drops to zero) while an ICE thread holds a `weak_ptr` and calls `onIceNegoSucceed()`, the call object could be gone. The existing code handles this via `w.lock()` checks. Spans stored in `SIPCall` will be destroyed with the object — ensure `End()` is called in the `SIPCall` destructor if the span was not already ended.

### 8. `SIPVoIPLink` is a singleton

`SIPVoIPLink` owns `endpt_` as a module-level static. There is only one PJSIP thread. OTel `sdp_media_update_cb` injection will be in a C-style static function, requiring careful extraction of the `SIPCall*` via `getCallFromInvite()` before accessing `callSpan_`.

---

## Source References

| File | Lines | Topic |
|---|---|---|
| [src/call.h](../../src/call.h) | 80–100 | `ConnectionState`, `CallState`, `CallType` enum definitions |
| [src/call.h](../../src/call.h) | 175–197 | `setState()` overloads |
| [src/call.cpp](../../src/call.cpp) | 72–128 | `Call::Call()` constructor, ringing timeout, `StateListenerCb` registration |
| [src/call.cpp](../../src/call.cpp) | 132–148 | `Call::removeCall()` — terminal teardown |
| [src/call.cpp](../../src/call.cpp) | 168–225 | `Call::validStateTransition()` — exhaustive transition table |
| [src/call.cpp](../../src/call.cpp) | 227–260 | `Call::setState()` — mutex, listener dispatch, signal emit |
| [src/call_factory.h](../../src/call_factory.h) | 42–50 | `CallFactory::newSipCall()` |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 100–150 | `SIPCall::SIPCall()` — `jami_tracepoint`, ICE/SDP init |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 2582–2650 | `SIPCall::onMediaNegotiationComplete()` — ICE branch decision |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 2654–2692 | `SIPCall::startIceMedia()` — ICE start + `waitForIceInit_` logic |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 2693–2730 | `SIPCall::onIceNegoSucceed()` — `setupNegotiatedMedia` + `startAllMedia` |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 1916–2007 | `SIPCall::setupNegotiatedMedia()` — `configureRtpSession` loop |
| [src/sip/sipcall.cpp](../../src/sip/sipcall.cpp) | 2008–2120 | `SIPCall::startAllMedia()` — async dispatch via `ThreadPool::io()` |
| [src/sip/sipvoiplink.cpp](../../src/sip/sipvoiplink.cpp) | 796–896 | `invite_session_state_changed_cb` — PJSIP state → `Call::on*()` dispatch |
| [src/sip/sipvoiplink.cpp](../../src/sip/sipvoiplink.cpp) | 1044–1080 | `sdp_media_update_cb` — SDP negotiation result handler |
| [src/sip/sipcall.h](../../src/sip/sipcall.h) | 80–95 | `SIPCall::RtpStream` struct |
| [src/sip/sipcall.h](../../src/sip/sipcall.h) | 340–390 | Private method declarations: `startIceMedia`, `onIceNegoSucceed`, `setupNegotiatedMedia`, `startAllMedia`, `stopAllMedia` |

---

## Open Questions

1. **Span storage in `SIPCall`:** Should `callSpan_` be a `nostd::shared_ptr<opentelemetry::trace::Span>` member of `SIPCall`, or should the `SpanContext` be stored separately? The latter avoids accidentally keeping the span alive via shared ownership with child spans on other threads.

2. **Subcall span parenting:** When `addSubCall()` is called, the subcall's `SIPCall` is already constructed. Is there a reliable window to set the parent context before the subcall starts making PJSIP calls, or does the subcall already have a racing PJSIP INVITE in flight?

3. **Re-INVITE count attribute:** Should `jami.call.reinvite.count` be an attribute on the root call span (updated via `SetAttribute` which is cumulative by latest value), or should each re-INVITE leg be its own child span? Using `AddEvent` may be the cleanest option.

4. **Ringing timeout granularity:** The timeout is `Manager::getRingingTimeout()` — is this value constant or configurable? If configurable, record it as `jami.call.ringing_timeout_ms` on the root span for debugging.

5. **`jami_tracepoint` co-existence:** LTTng tracepoints already instrument `call_start`. OTel spans and LTTng tracepoints can coexist, but the `jami_tracepoint(call_start, ...)` line should remain as it targets kernel-level tooling. There is no duplication concern beyond timestamp skew.

6. **ICE failure detail:** `onFailure(PJSIP_SC_NOT_ACCEPTABLE_HERE)` is called from `startIceMedia()` but the ICE-specific failure reason (e.g., no STUN reachability, no candidates in common) is not exposed at the `SIPCall` level. Should the `call.ice.negotiate` span's error description include the `IceTransport` failure reason string?

7. **`EXPECTED_ICE_INIT_MAX_TIME` enforcement:** The constant is defined in `sipcall.cpp` at 5000 ms but is annotated as an expectation, not a hard cutoff. Should an OTel span event `"ice.init.exceeded_expected_time"` be emitted when init takes longer than this threshold, even if init eventually succeeds?

8. **Conference integration:** When a call enters a conference via `SIPCall::enterConference()`, the call span is still independently active. How should the relationship between `call.incoming` and `conference.*` spans be expressed — via `SpanLink`, or by ending the call span and creating a conference-child span?
