# Call Manager

## Status: complete

## Last Updated: 2026-03-13

---

## Purpose

The call manager subsystem handles the complete lifecycle of individual calls — both SIP and JAMI P2P. It manages the state machine from `INACTIVE` through `RINGING`, `ACTIVE`, `HOLD`, and `OVER`; drives SDP offer/answer negotiation; orchestrates ICE candidate gathering and connectivity establishment; maps calls to RTP media streams; and provides transfer, conferencing attachment, and DTMF injection. It bridges the public API (`callmanager_interface.h`) to the PJSIP UA layer and to the media pipeline.

---

## Key Files

| File | Role |
|---|---|
| `src/call.h` / `src/call.cpp` | `Call` abstract base class; state machine (`CallState`, `ConnectionState`); `validStateTransition()`; `setState()` overloads; `StateListenerCb` dispatch; ringing timeout timer; subcall management |
| `src/call_factory.h` / `src/call_factory.cpp` | `CallFactory` — thread-safe registry of all active calls keyed by `Call::LinkType`; `newSipCall()`, `removeCall()`, `callCount()` |
| `src/call_set.h` | `CallSet` — per-account active-call set |
| `src/sip/sipcall.h` / `src/sip/sipcall.cpp` | `SIPCall` — owns `pjsip_inv_session*`, `Sdp`, vector of `RtpStream` structs, and two `dhtnet::IceTransport` instances (`iceMedia_`, `reinvIceMedia_`); full media lifecycle |
| `src/sip/sdp.h` / `src/sip/sdp.cpp` | `Sdp` — wraps PJMEDIA SDP session; builds local offer; parses remote answer; extracts media slots and ICE attributes |
| `src/sip/sdes_negotiator.h` / `src/sip/sdes_negotiator.cpp` | `SdesNegotiator` — SDES `a=crypto` parsing, cipher suite selection, SRTP key derivation |
| `src/sip/sipvoiplink.h` / `src/sip/sipvoiplink.cpp` | `SIPVoIPLink` — PJSIP endpoint owner; runs PJSIP event loop on a dedicated `std::thread`; all PJSIP invite/SDP C callbacks registered here |
| `src/sip/siptransport.h` / `src/sip/siptransport.cpp` | `SipTransport` / `SipTransportBroker` — TLS/UDP/TCP transport lifecycle; state listener for transport death detection |
| `src/jami/callmanager_interface.h` | Public C API surface: `placeCall`, `accept`, `hangUp`, `hold`, `transfer`, `attendedTransfer`, `muteMedia`, `requestMediaChange`, etc. |
| `src/jami/call_const.h` | String constants for call detail map keys and state event strings |

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `Call` | Abstract base; holds `CallState` + `ConnectionState` pair; `StateListenerCb` list; subcall `std::set`; `asio::steady_timer` for ring timeout; duration tracking | `src/call.h` |
| `CallFactory` | Thread-safe call registry; `std::map<LinkType, CallMap>` guarded by `std::recursive_mutex`; enforces `allowNewCall_` | `src/call_factory.h` |
| `SIPCall` | Concrete SIP call; owns `std::unique_ptr<Sdp>`, `std::vector<RtpStream>`, `inviteSession_`, two ICE transports, UPnP controller, SRTP flag | `src/sip/sipcall.h` |
| `SIPCall::RtpStream` | Per-media-stream aggregate: `rtpSession_`, send/recv `MediaAttribute`, `rtpSocket_` + `rtcpSocket_` (`IceSocket`s) | `src/sip/sipcall.h` |
| `Sdp` | PJMEDIA SDP wrapper; local/active session pointers; `getMediaSlots()` returns `(local, remote)` description pairs; `getIceAttributes()` | `src/sip/sdp.h` |
| `SdesNegotiator` | Parses SDES `a=crypto` lines, selects cipher suite, derives SRTP master key+salt | `src/sip/sdes_negotiator.h` |
| `SIPVoIPLink` | PJSIP endpoint (`pjsip_endpoint*`); single `sipThread_` running `handleEvents()` loop; owns `SipTransportBroker`; registers all PJSIP callbacks | `src/sip/sipvoiplink.h` |

---

## Call State Machine

### Enumerations

```
ConnectionState (uint8_t):
  DISCONNECTED  (0) — initial / post-hangup
  TRYING        (1) — INVITE sent / received, waiting for provisional
  PROGRESSING   (2) — 100 Trying received
  RINGING       (3) — 180 Ringing received or local ringing (incoming)
  CONNECTED     (4) — 200 OK + ACK exchanged

CallState (uint8_t):
  INACTIVE      (0) — pre-answer state
  ACTIVE        (1) — call established, media flowing
  HOLD          (2) — call on hold (re-INVITE with sendonly/inactive)
  BUSY          (3) — local busy condition
  PEER_BUSY     (4) — remote returned 486 Busy Here
  MERROR        (5) — media error
  OVER          (6) — terminal state; call removed from registry
```

### Valid Transitions (`Call::validStateTransition()`)

Transition from → to (OVER is always permitted from any state):

```
INACTIVE  ──► ACTIVE
          ──► BUSY
          ──► PEER_BUSY
          ──► MERROR

ACTIVE    ──► HOLD
          ──► BUSY
          ──► PEER_BUSY
          ──► MERROR

HOLD      ──► ACTIVE
          ──► MERROR

BUSY      ──► MERROR

Any       ──► OVER   (unconditional)
```

### Client-Visible State Strings (`Call::getStateStr()`)

The `StateChange` signal emits a composite string derived from the `(CallState, ConnectionState)` pair:

| CallState | ConnectionState | String emitted |
|---|---|---|
| INACTIVE | PROGRESSING | `"CONNECTING"` |
| INACTIVE | RINGING | `"INCOMING"` (incoming) / `"RINGING"` (outgoing) |
| INACTIVE | CONNECTED | `"CURRENT"` |
| ACTIVE | PROGRESSING | `"CONNECTING"` |
| ACTIVE | RINGING | `"INCOMING"` / `"RINGING"` |
| ACTIVE | CONNECTED | `"CURRENT"` |
| ACTIVE | DISCONNECTED | `"HUNGUP"` |
| HOLD | * | `"HOLD"` (unless DISCONNECTED → `"HUNGUP"`) |
| BUSY | * | `"BUSY"` |
| PEER_BUSY | * | `"PEER_BUSY"` |
| OVER | * | `"OVER"` |
| MERROR | * | `"FAILURE"` |

### PJSIP Invite State → `Call` state mapping

All mapping happens in `invite_session_state_changed_cb()` in `sipvoiplink.cpp`:

| PJSIP inv_state | cause / status | Call method | Resulting state transition |
|---|---|---|---|
| `PJSIP_INV_STATE_EARLY` | 180 Ringing | `onPeerRinging()` | `→ ConnectionState::RINGING` |
| `PJSIP_INV_STATE_CONFIRMED` | 200 OK + ACK | `onAnswered()` | `→ ACTIVE / CONNECTED` |
| `PJSIP_INV_STATE_DISCONNECTED` | `PJSIP_SC_BUSY_HERE` | `onBusyHere()` | `→ PEER_BUSY` |
| `PJSIP_INV_STATE_DISCONNECTED` | `PJSIP_SC_DECLINE` / `BUSY_EVERYWHERE` (UAC) | `onClosed()` | `→ OVER` |
| `PJSIP_INV_STATE_DISCONNECTED` | `PJSIP_SC_OK` / `REQUEST_TERMINATED` | `onClosed()` | `→ OVER` |
| `PJSIP_INV_STATE_DISCONNECTED` | other | `onFailure(cause)` | `→ MERROR → OVER` |

---

## Critical Code Paths

### Outgoing Call

```
Client API (any thread)
  └─ Manager::newOutgoingCall(peer_uri, mediaList)
       └─ CallFactory::newSipCall(account, OUTGOING, mediaList)
            └─ SIPCall::SIPCall()                        [constructor]
                 ├─ jami_tracepoint(call_start, ...)     [LTTng tracepoint]
                 ├─ sdp_->setLocalMediaCapabilities()
                 └─ initMediaStreams(mediaAttrList)
  └─ SIPAccountBase::newOutgoingCall()  → SIPVoIPLink creates pjsip_inv_session
       └─ pjsip_inv_send_msg(invite)   [PJSIP thread]

PJSIP thread — 180 Ringing received:
  └─ invite_session_state_changed_cb(PJSIP_INV_STATE_EARLY, 180)
       └─ SIPCall::onPeerRinging()
            └─ setState(RINGING)                         [emits StateChange signal]

PJSIP thread — 200 OK received:
  └─ transaction_response_cb() → sends ACK
  └─ sdp_media_update_cb(PJ_SUCCESS)
       └─ sdp_.setActiveLocalSdpSession() / setActiveRemoteSdpSession()
       └─ SIPCall::onMediaNegotiationComplete()          [posted to main thread via runOnMainThread]
            ├─ [ICE path] SIPCall::startIceMedia()
            │    └─ IceTransport::startIce(rem_attrs, candidates)
            │         └─ [async, dhtnet thread] ICE connectivity checks (~1–5 s)
            │              └─ SIPCall::onIceNegoSucceed()
            │                   ├─ setupNegotiatedMedia()   [configure RtpSessions from SDP slots]
            │                   ├─ stopAllMedia()
            │                   └─ startAllMedia()          [dht::ThreadPool::io() per stream]
            └─ [no-ICE path] setupNegotiatedMedia() + stopAllMedia() + startAllMedia()

PJSIP thread — CONFIRMED (ACK sent):
  └─ invite_session_state_changed_cb(PJSIP_INV_STATE_CONFIRMED)
       └─ SIPCall::onAnswered()
            └─ setState(ACTIVE, CONNECTED)               [duration_start_ recorded in StateListenerCb]
```

### Incoming Call

```
PJSIP thread — INVITE received:
  └─ on_rx_offer2() or sdp_create_offer_cb()
  └─ SIPAccountBase creates SIPCall(INCOMING)
       └─ SIPCall::SIPCall()
            └─ jami_tracepoint(call_start, ...)
            └─ StateListenerCb fires on RINGING:
                 └─ asio::steady_timer set for ringing timeout (Manager::getRingingTimeout())
  └─ Manager::incomingCall() → emits IncomingCallWithMedia signal

Client thread — answer():
  └─ SIPCall::answer(mediaList)
       └─ pjsip_inv_answer(200) + SDP answer built
       └─ [ACK received] invite_session_state_changed_cb(CONFIRMED)
            └─ SIPCall::onAnswered() → setState(ACTIVE, CONNECTED)
  ══ media setup identical to outgoing call path above ══

  [timeout path — no answer within getRingingTimeout()]:
  └─ asio::steady_timer fires on io_context thread
       └─ SIPCall::hangup(PJSIP_SC_BUSY_HERE)
       └─ Manager::callFailure()
```

### Hold / Resume

```
Client API → SIPCall::hold(cb)
  └─ [ICE pending] sets remainingRequest_ = Hold; cb queued
  └─ [ICE ready]   SIPCall::hold()
       └─ SIPSessionReinvite(sendonly attrs, needNewIce=false)
            └─ pjsip_inv_reinvite()                      [PJSIP thread]
  └─ sdp_media_update_cb → onMediaNegotiationComplete → setupNegotiatedMedia → startAllMedia
  └─ setState(HOLD)

Client API → SIPCall::resume(cb)
  └─ SIPCall::internalResume()
       └─ SIPSessionReinvite(sendrecv attrs)
  └─ [same media restart path]
  └─ setState(ACTIVE)
```

### Call Teardown

```
Local hangup → SIPCall::hangup(code)
  └─ terminateSipSession(code)           [pjsip_inv_end_session / pjsip_inv_terminate]
  └─ stopAllMedia()                      [RTP sessions torn down]
  └─ Call::removeCall(code)
       ├─ CallFactory::removeCall(*this) [removed from registry]
       ├─ setState(OVER, code)
       ├─ Recordable::stopRecording()
       └─ account->detach(this_)

Peer hangup → PJSIP BYE received:
  └─ invite_session_state_changed_cb(DISCONNECTED, PJSIP_SC_OK)
       └─ SIPCall::onClosed()
            └─ setState(DISCONNECTED) + peerHungup()
                 └─ removeCall(0) [same teardown path]

StateListenerCb (OVER transition):
  └─ [isSubcall=false, JAMI account, outgoing call without '/' in peer URI]
       └─ convModule()->addCallHistoryMessage(peerNumber, duration, reason_)
  └─ monitor()        [debug dump]
  └─ hangupCalls(safePopSubcalls())   [kill any remaining forks]
```

---

## Threading Model

| Thread | Owner | Responsibilities |
|---|---|---|
| **PJSIP event loop** | `SIPVoIPLink::sipThread_` (`std::thread`) | All PJSIP callbacks: `invite_session_state_changed_cb`, `sdp_media_update_cb`, `on_rx_offer2`, `reinvite_received_cb`, `sdp_create_offer_cb`, `transaction_response_cb` |
| **ASIO io_context / main thread** | `Manager::ioContext()` | `runOnMainThread()` posts: `onMediaNegotiationComplete()`, `checkAudio()`, `checkPendingIM()`; ring timeout timer callbacks; voice activity callbacks |
| **dht::ThreadPool::io()** | opendht global thread pool | `startAllMedia()` per-stream `rtpSession->start()` calls; `hangupCallsIf()` for subcall teardown; RTP setup success callbacks |
| **dhtnet ICE thread** | dhtnet internal | ICE connectivity checks; fires `onIceNegoSucceed()` callback (then re-posted to main thread via `runOnMainThread` or direct call) |
| **Client / user thread** | External callers via `callmanager_interface.h` | `placeCall`, `accept`, `hangUp`, `hold`, `transfer` — these acquire the `callMutex_` inside `setState()` |

**Mutex hierarchy:**
- `Call::callMutex_` (`std::recursive_mutex`): protects `callState_`, `connectionState_`, `stateChangedListeners_`, `subcalls_`, `parent_`
- `SIPCall::callMutex_` (inherited): additionally guards `inviteSession_`, `sdp_`, `rtpStreams_`
- `SIPCall::transportMtx_`: protects `iceMedia_` / `reinvIceMedia_` swaps
- `SIPCall::mediaStateMutex_`: protects `isAudioOnly_`, `readyToRecord_`, `pendingRecord_`
- `SIPCall::avStreamsMtx_`: protects `callAVStreams` plugin map
- `CallFactory::callMapsMutex_` (`std::recursive_mutex`): protects `callMaps_`

**Cross-thread span hazard:** `onMediaNegotiationComplete()` is posted via `runOnMainThread()` from the PJSIP callback; `onIceNegoSucceed()` acquires `callMutex_` from the ICE thread and then calls `startAllMedia()` which offloads to `dht::ThreadPool::io()`. Any OTel span that starts on the PJSIP thread and ends after ICE completion crosses at least two thread boundaries.

---

## Key Operations with Expected Latency

| Operation | Typical latency | Worst case | Where |
|---|---|---|---|
| `SIPCall` constructor + `initMediaStreams()` | < 1 ms | 5 ms | Constructor |
| PJSIP INVITE send to 180 Ringing | network RTT (10–500 ms) | 5 s (retransmit) | Network |
| PJSIP 200 OK to ACK | ~1 network RTT | 32 s (SIP timer B) | Network |
| `Sdp::setLocalMediaCapabilities()` | < 1 ms | 2 ms | Constructor |
| SDP offer/answer local processing | < 5 ms | 20 ms | `sdp_media_update_cb` |
| `IceTransport` initialization | 500 ms – 2 s | `DEFAULT_ICE_INIT_TIMEOUT` = 35 s | dhtnet |
| ICE connectivity checks (negotiation) | 500 ms – 3 s | `EXPECTED_ICE_INIT_MAX_TIME` = 5 s | dhtnet |
| `setupNegotiatedMedia()` | < 2 ms | 10 ms | main thread |
| `startAllMedia()` per stream | 10 – 100 ms (codec init) | 500 ms | ThreadPool::io() |
| Hold re-INVITE round-trip | network 2×RTT | 5 s | PJSIP |
| Resume re-INVITE round-trip | network 2×RTT | 5 s | PJSIP |
| `CallFactory::removeCall()` | < 1 ms | 5 ms (lock contention) | Any |
| `convModule()->addCallHistoryMessage()` | 1 – 10 ms (DB write) | 50 ms | StateListenerCb |

---

## External Dependencies

| Dependency | Version constraint | Usage |
|---|---|---|
| **PJSIP** (`pjsip_ua/sip_inv.h`, `pjsip/sip_endpoint.h`) | ≥ 2.10 (enforced at compile time in `sipvoiplink.cpp`) | Invite session lifecycle, SIP transaction layer, SDP negotiation dispatch |
| **PJMEDIA** (`pjmedia/sdp.h`, `pjmedia/sdp_neg.h`) | bundled with PJSIP | SDP data model (`pjmedia_sdp_session`); active session extraction |
| **PJNATH** (`pjnath/stun_config.h`) | bundled | STUN/TURN client used by pjsip_inv for ICE; superseded by dhtnet for media |
| **dhtnet** (`dhtnet/ice_transport.h`, `dhtnet/ice_transport_factory.h`) | workspace dep | `IceTransport` for media: candidate gathering, connectivity checks, `IceSocket` wrapping |
| **opendht** (`opendht/thread_pool.h`) | workspace dep | `dht::ThreadPool::io()` for async media start / hangup dispatch |
| **asio** (`asio/steady_timer.hpp`) | bundled | Ring timeout timer, call timeout |
| **libsrtp** (via `media/srtp.h`) | system | SRTP packet encryption/decryption after SDES key exchange |
| **dhtnet::upnp::Controller** | workspace dep | UPnP port mapping for RTP/RTCP ports (optional; `account->getUPnPActive()`) |
| **LTTng UST** (`tracepoint.h`) | optional | `jami_tracepoint(call_start, ...)` fires from `SIPCall` constructor |

---

## Coupling Map

| Subsystem | Coupling direction | Interface |
|---|---|---|
| **Manager** (central coordinator) | bidirectional | `newOutgoingCall()`, `callFailure()`, `incomingCall()`, `getRingingTimeout()`, `ioContext()`, `upnpContext()` |
| **AccountManagement** (`SIPAccountBase`) | call → account | `isIceForMediaEnabled()`, `isSrtpEnabled()`, `getActiveAccountCodecInfoList()`, `generateAudioPort()`, `getUPnPActive()`, `detach()` |
| **MediaPipeline** (`AudioRtpSession`, `VideoRtpSession`) | call owns | `createRtpSession()`, `configureRtpSession()`, `rtpSession->start()`, `stop()`, `setMuted()`, `setMediaSource()` |
| **ConversationModule** (`JamiAccount::convModule()`) | post-teardown | `addCallHistoryMessage(peerNumber, duration, reason_)` — only for outgoing JAMI calls without device-path in URI |
| **Conference** (`Conference` class) | weakly coupled | `enterConference()`, `exitConference()`, `setVoiceActivity()`, `conf_.lock()` weak_ptr |
| **SipTransport / SipTransportBroker** | call holds shared_ptr | `getSipTransport()`, state listener for transport death → `onFailure()` |
| **CallFactory** | call → factory | `removeCall()` called from `Call::removeCall()`; factory creates calls via `newSipCall()` |
| **CallSet** | account → set | Per-account bookkeeping; independent of `CallFactory` global map |
| **PluginSystem** (`JamiPluginManager`) | optional AV hooks | `createCallAVStreams()` wires `AudioRtpSession` / `VideoRtpSession` outputs into `MediaStreamSubject` observers |
| **SIPVoIPLink** (callback router) | owns endpoint | Registers PJSIP C callbacks; calls `SIPCall::on*()` methods from PJSIP thread |
| **Sdp** | call owns | `SIPCall::sdp_` — unique_ptr; accessed from PJSIP thread (via callbacks) and main thread |

---

## Open Questions

1. What is the authoritative thread-safety contract for `SIPCall::RtpStream` mutation (adding/removing streams during an active call)? The vector is accessed from both the PJSIP callback thread (in `onMediaNegotiationComplete`) and `dht::ThreadPool::io()`.
2. Is SDES (`SdesNegotiator`) used for JAMI P2P calls, or only for interoperability with legacy SIP endpoints?
3. How are subcalls (call forking via `addSubCall`) reconciled when multiple forks answer simultaneously? The `merge()` method is private — who triggers it?
4. Is there a maximum simultaneous call count enforced somewhere, or is `CallFactory::callMaps_` unbounded?
5. The `EXPECTED_ICE_INIT_MAX_TIME` constant (5 s) is defined but how/where is it enforced as a hard timeout? The `DEFAULT_ICE_INIT_TIMEOUT` (35 s) is the actual timer.
