# Jami Daemon — Telemetry Instrumentation Plan

> Generated: 2026-03-17  
> Scope: OpenTelemetry C++ distributed tracing for all major subsystems  
> Status: Phase 1 — Calls subsystem instrumented; remaining subsystems planned

---

## Calls (`src/call.cpp`, `src/call_factory.cpp`, `src/sip/sipcall.cpp`)

**Responsibility:** Full lifecycle management of voice/video calls — creation,
state machine (ConnectionState × CallState), incoming/outgoing routing,
hold/resume, transfer, sub-call/multi-device merging, and teardown.

**Key instrumentation points:**

- `Call::Call()` constructor — call creation (sets type INCOMING/OUTGOING)
- `Call::setState(CallState, ConnectionState, code)` — central state transition;
  a span event on every transition captures the full state machine
- `SIPCall::answer(mediaList)` — answering an incoming call
  (SDP processing, ICE setup, 200 OK)
- `SIPCall::hangup(code)` — call teardown
  (terminateSipSession, stopAllMedia, removeCall)
- `SIPCall::onAnswered()` — peer answered (sets ACTIVE+CONNECTED)
- `SIPCall::onFailure(code)` — call failure (sets MERROR+DISCONNECTED)
- `SIPCall::onBusyHere()` — busy signal
- `SIPCall::onClosed()` — peer hungup
- `SIPCall::onPeerRinging()` — ringing state
- `SIPCall::onMediaNegotiationComplete()` — SDP negotiation done, ICE or
  direct media start
- `SIPCall::startIceMedia()` — ICE connectivity checks begin
- `SIPCall::onIceNegoSucceed()` — ICE completed, media starts
- `Manager::outgoingCall()` — high-level outgoing call factory
- `Manager::incomingCall()` — high-level incoming call routing

**Suggested span model:**

| Span name | Lifecycle | Parent |
|-----------|-----------|--------|
| `call.outgoing` | `newSipCall()` → `CONNECTED+ACTIVE` or failure | root |
| `call.incoming` | `incomingCall()` → `answer()` → `CONNECTED+ACTIVE` or failure | root |
| `call.hangup` | `hangup()` → `removeCall()` | `call.*` |

Events emitted mid-span:
- `call.state_transition` — on every `setState()` invocation
- `sdp.offer.sent` / `sdp.answer.sent`
- `ice.negotiation.started` / `ice.negotiation.completed`
- `media.started`

**Useful span attributes:**

| Attribute | Source |
|-----------|--------|
| `call.id` | `Call::getCallId()` |
| `call.peer_uri` | `Call::getPeerNumber()` |
| `call.type` | `Call::getCallType()` (INCOMING / OUTGOING) |
| `call.account_id` | `Call::getAccountId()` |
| `call.duration_ms` | `Call::getCallDuration()` (at hangup) |
| `call.hangup_reason` | `reason_` field or SIP status code |
| `call.state.from` / `call.state.to` | previous/new state strings |

**Metrics candidates:**
- `jami.calls.active` (gauge) — current active call count
- `jami.calls.duration_ms` (histogram) — call duration distribution
- `jami.calls.total` (counter) — total calls by type and outcome
- `jami.calls.failure_rate` (counter) — calls ending in MERROR

**Complexity:** Medium — State machine is centralized in `Call::setState()`;
SIP-specific lifecycle methods are well-delineated in `SIPCall`. The
multi-device sub-call merging adds some edge cases.

---

## Accounts (`src/account.h`, `src/account_factory.h`)

**Responsibility:** Abstract base class for all account types (SIP, Jami).
Manages registration lifecycle, codec configuration, call tracking, and
configuration persistence. `AccountFactory` creates and stores accounts.

**Key instrumentation points:**

- `Account::doRegister()` / `doUnregister()` — registration lifecycle
- `Account::setRegistrationState()` — state transitions with detail code
- `Account::newOutgoingCall()` — call origination from this account

**Suggested span model:**

| Span | Lifecycle |
|------|-----------|
| `account.register` | `doRegister()` → registration confirmed/failed |
| `account.unregister` | `doUnregister()` → unregistered |

**Useful span attributes:** `account.id`, `account.type` (SIP/RING),
`account.registration_state`, `account.error_code`

**Metrics candidates:**
- `jami.accounts.registration_duration_ms` (histogram)
- `jami.accounts.registration_failures` (counter)

**Complexity:** Medium — Abstract class requires instrumentation in concrete
subclasses (`JamiAccount`, `SIPAccount`).

---

## Jami DHT Account (`src/jamidht/`)

**Responsibility:** DHT-based peer-to-peer account. Manages DHT bootstrap,
ConnectionManager for peer connectivity, trust requests, contact management,
device linking, conversation module, file transfers, and presence via DHT.

**Key instrumentation points:**

- `JamiAccount::doRegister()` — DHT bootstrap + connection manager init
- `JamiAccount::newOutgoingCall()` / `newIncomingCall()` — DHT-routed SIP calls
- `JamiAccount::requestSIPConnection()` — P2P SIP channel setup
- `JamiAccount::addDevice()` / `revokeDevice()` — device management

**Suggested span model:**

| Span | Lifecycle |
|------|-----------|
| `dht.bootstrap` | DHT node bootstrap → connected |
| `dht.connection.request` | `requestSIPConnection()` → channel ready |

**Useful span attributes:** `account.id`, `peer.uri`, `peer.device_id`,
`dht.bootstrap_time_ms`, `connection.channel_type`

**Metrics candidates:**
- `jami.dht.bootstrap_duration_ms` (histogram)
- `jami.dht.active_connections` (gauge)

**Complexity:** High — Largest class, many async callbacks, DHT + connection
manager + sync module interactions.

---

## SIP Account (`src/sip/sipaccount.h`)

**Responsibility:** Traditional SIP account managing registration with a SIP
registrar, NAT traversal (STUN/UPnP), TLS transport, and credentials.

**Key instrumentation points:**

- `SIPAccount::sendRegister()` / `sendUnregister()` — SIP REGISTER flow
- `SIPAccount::onRegister()` — registration result callback
- `SIPAccount::newOutgoingCall()` / `newIncomingCall()` — SIP call creation

**Suggested span model:**

| Span | Lifecycle |
|------|-----------|
| `sip.register` | `sendRegister()` → registration response |

**Useful span attributes:** `account.id`, `sip.registrar`, `sip.transport_type`,
`sip.status_code`

**Metrics candidates:**
- `jami.sip.registration_time_ms` (histogram)

**Complexity:** Medium — Well-structured around PJSIP registration workflow.

---

## SIP Signaling (`src/sip/sipvoiplink.h`)

**Responsibility:** Core SIP VoIP link singleton owning the PJSIP endpoint.
Handles SIP event processing, transport brokering, and routing incoming
SIP requests to correct accounts.

**Key instrumentation points:**

- `SIPVoIPLink::handleEvents()` — SIP event loop processing
- `SIPVoIPLink::guessAccount()` — inbound call/message routing
- `SIPVoIPLink::resolveSrvName()` — DNS SRV resolution

**Suggested span model:**
Events on a background span for `sip.event_loop` or child spans
`sip.resolve` for DNS SRV lookups.

**Useful span attributes:** `sip.method`, `sip.status_code`, `sip.call_id`

**Metrics candidates:**
- `jami.sip.messages_processed` (counter)
- `jami.sip.dns_resolve_time_ms` (histogram)

**Complexity:** Medium — Deep PJSIP integration but clear entry points.

---

## SDP Negotiation (`src/sip/sdp.h`)

**Responsibility:** Manages SDP offer/answer negotiation. Builds local SDP
from codec capabilities, processes incoming offers, and extracts ICE
candidates.

**Key instrumentation points:**

- `Sdp::createOffer()` — SDP offer generation
- `Sdp::processIncomingOffer()` — answer construction
- `Sdp::startNegotiation()` — negotiation result

**Suggested span model:**
Child spans under a call span: `sdp.offer`, `sdp.answer`, `sdp.negotiate`.

**Useful span attributes:** `sdp.media_count`, `sdp.has_ice`, `sdp.codecs`

**Metrics candidates:**
- `jami.sdp.negotiation_failures` (counter)

**Complexity:** Medium — Tightly coupled to pjmedia SDP structures.

---

## SIP Transport (`src/sip/siptransport.h`)

**Responsibility:** Wraps `pjsip_transport` with lifecycle management, TLS
info tracking, and state listeners. `SipTransportBroker` manages transport
creation and caching.

**Key instrumentation points:**

- `SipTransportBroker::getUdpTransport()` / `getTlsTransport()` — creation
- transport state callbacks — CONNECTED, DISCONNECTED events
- `SipTransportBroker::shutdown()` — teardown

**Suggested span model:**
`sip.transport.create` span with type attribute (UDP/TLS/channeled).

**Useful span attributes:** `transport.type`, `transport.remote_addr`,
`transport.local_addr`

**Metrics candidates:**
- `jami.sip.transports.active` (gauge)

**Complexity:** Low — Small, well-scoped classes with clear state callbacks.

---

## ICE / Media Transport (`src/connectivity/`)

**Responsibility:** Utility wrappers for IP address resolution, SIP utilities,
and UTF-8 handling. The actual ICE transport lives in the external `dhtnet`
library.

**Key instrumentation points:**

- ICE transport is instrumented at call sites in `SIPCall` (startIceMedia,
  onIceNegoSucceed) rather than in this directory.

**Suggested span model:**
Covered by call spans' ICE events.

**Complexity:** Low — Mostly utilities; real ICE logic is in dhtnet.

---

## Certificate / PKI / TLS (`src/connectivity/security/`)

**Responsibility:** `TlsValidator` performs X.509 certificate validation —
expiry, signing strength, revocation, chain validity. `memory.h` provides
secure memory.

**Key instrumentation points:**

- `TlsValidator::getSerializedChecks()` — full validation result
- `TlsValidator::isValid()` — overall verdict

**Suggested span model:**
`tls.validate` span with attribute for check results.

**Useful span attributes:** `cert.serial`, `cert.issuer`, `cert.valid`,
`cert.check_count`

**Metrics candidates:**
- `jami.tls.validation_failures` (counter)

**Complexity:** Low — Self-contained validation class.

---

## Audio Pipeline (`src/media/audio/`)

**Responsibility:** Full audio pipeline: AudioLayer (HW abstraction for
ALSA/PulseAudio/JACK/CoreAudio/AAudio), AudioInput (capture/file decode),
AudioRtpSession (RTP with RTCP quality monitoring), RingBufferPool (mixing),
Resampler, AudioFrameResizer.

**Key instrumentation points:**

- `AudioLayer::startStream()` / `stopStream()` — audio device lifecycle
- `AudioRtpSession::start()` / `stop()` — RTP session lifecycle
- `AudioRtpSession::adaptQualityAndBitrate()` — RTCP quality adaptation
- `AudioInput::switchInput()` — input source switching

**Suggested span model:**
`audio.rtp.session` span covering the RTP session lifetime.
Events for quality adaptations.

**Useful span attributes:** `audio.codec`, `audio.sample_rate`,
`audio.channels`, `audio.bitrate`

**Metrics candidates:**
- `jami.audio.jitter_ms` (histogram)
- `jami.audio.packet_loss_pct` (histogram)
- `jami.audio.rtp_sessions.active` (gauge)

**Complexity:** High — Multi-platform backends, real-time threading,
ring buffer pool for mixing, RTCP feedback loops.

---

## Video Pipeline (`src/media/video/`)

**Responsibility:** Video capture (VideoInput), rendering (SinkClient), RTP
session (VideoRtpSession with bitrate adaptation), conference mixing
(VideoMixer with layout management), HW acceleration, scaling.

**Key instrumentation points:**

- `VideoRtpSession::start()` / `stop()` — video RTP lifecycle
- `VideoMixer::switchInputs()` / `updateLayout()` — conference mixing
- `VideoRtpSession::forceKeyFrame()` — keyframe requests
- `VideoRtpSession::adaptQualityAndBitrate()` — quality control

**Suggested span model:**
`video.rtp.session` span; events for keyframe requests and bitrate changes.

**Useful span attributes:** `video.codec`, `video.resolution`, `video.fps`,
`video.bitrate`, `video.hw_accel`

**Metrics candidates:**
- `jami.video.fps` (gauge)
- `jami.video.bitrate_kbps` (gauge)
- `jami.video.keyframe_requests` (counter)

**Complexity:** High — Real-time processing, HW acceleration, conference
layout management, multi-platform backends.

---

## Media Codec / Encoder / Decoder (`src/media/media_encoder.h`, `media_decoder.h`)

**Responsibility:** FFmpeg wrappers for encoding/decoding audio and video
frames, bitrate management, packet loss handling, FEC, and HW acceleration.

**Key instrumentation points:**

- `MediaEncoder::encode()` / `encodeAudio()` — frame encoding
- `MediaDecoder::decode()` — frame decoding (Success/Error/EOF/FallBack)
- `MediaEncoder::setBitrate()` / `setPacketLoss()` — quality param changes
- `MediaDemuxer::openInput()` — media source initialization

**Suggested span model:**
Metrics rather than spans — encoding/decoding is per-frame and too
high-frequency for trace spans.

**Metrics candidates:**
- `jami.media.encode_time_us` (histogram)
- `jami.media.decode_failures` (counter)
- `jami.media.bitrate_kbps` (gauge)

**Complexity:** Medium — Well-encapsulated FFmpeg wrappers with clear status
enums.

---

## Presence (`src/sip/sippresence.h`, `pres_sub_client.h`, `pres_sub_server.h`)

**Responsibility:** SIP-based presence management. PUBLISH (own status),
SUBSCRIBE (buddy tracking), NOTIFY (subscriber updates).

**Key instrumentation points:**

- `SIPPresence::sendPresence()` — publish presence
- `SIPPresence::subscribeClient()` — subscribe to buddy
- `SIPPresence::reportPresSubClientNotification()` — received update

**Suggested span model:**
`presence.publish` and `presence.subscribe` spans.

**Useful span attributes:** `presence.status`, `presence.buddy_uri`

**Metrics candidates:**
- `jami.presence.subscriptions.active` (gauge)

**Complexity:** Low — Small, well-scoped SIP PUBLISH/SUBSCRIBE.

---

## File Transfer (`src/data_transfer.h`, `src/jamidht/transfer_channel_handler.h`)

**Responsibility:** `TransferManager` orchestrates file transfers — sending
over channeled sockets, receiving with SHA3 verification, tracking progress.
`TransferChannelHandler` manages channel lifecycle.

**Key instrumentation points:**

- `TransferManager::transferFile()` — outgoing transfer
- `TransferManager::onIncomingFileTransfer()` — incoming reception
- `TransferChannelHandler::connect()` / `onReady()` — channel setup

**Suggested span model:**

| Span | Lifecycle |
|------|-----------|
| `file.transfer.send` | `transferFile()` → completion |
| `file.transfer.receive` | `onIncomingFileTransfer()` → SHA3 verified |

**Useful span attributes:** `file.size_bytes`, `file.name`, `file.sha3`,
`peer.uri`, `transfer.duration_ms`

**Metrics candidates:**
- `jami.file_transfer.bytes_sent` (counter)
- `jami.file_transfer.duration_ms` (histogram)
- `jami.file_transfer.failures` (counter)

**Complexity:** Medium — Async channel setup and SHA3 verification.

---

## Conversation / Messaging (`src/jamidht/conversation.h`, `conversation_module.h`)

**Responsibility:** Git-backed conversation system. `Conversation` manages a
single swarm conversation — members, message commit/fetch/sync via git.
`ConversationModule` orchestrates all conversations for an account.

**Key instrumentation points:**

- `ConversationModule::startConversation()` — conversation creation
- `ConversationModule::sendMessage()` — message send (git commit)
- `Conversation::pull()` / `sync()` — git fetch/merge from peers
- `ConversationModule::fetchNewCommits()` — new commit handling

**Suggested span model:**

| Span | Lifecycle |
|------|-----------|
| `conversation.send_message` | `sendMessage()` → commit created |
| `conversation.sync` | `pull()` → merge complete |

**Useful span attributes:** `conversation.id`, `message.type`,
`peer.uri`, `git.commit_hash`

**Metrics candidates:**
- `jami.conversations.messages_sent` (counter)
- `jami.conversations.sync_duration_ms` (histogram)
- `jami.conversations.sync_failures` (counter)

**Complexity:** High — Git-based distributed state, DRT swarm networking,
complex multi-device sync.

---

## Instant Messaging (`src/im/`)

**Responsibility:** `MessageEngine` provides reliable SIP MESSAGE delivery
with retry logic, status tracking (IDLE→SENDING→SENT/FAILURE), and
persistence. `instant_messaging` namespace provides SIP MESSAGE construction.

**Key instrumentation points:**

- `MessageEngine::sendMessage()` — message queuing
- `MessageEngine::onMessageSent()` — delivery confirmation/failure
- `im::sendSipMessage()` — SIP MESSAGE send

**Suggested span model:**
`im.send` span from queue to delivery confirmation.

**Useful span attributes:** `message.id`, `message.status`, `peer.uri`,
`message.retry_count`

**Metrics candidates:**
- `jami.im.messages_sent` (counter)
- `jami.im.delivery_failures` (counter)

**Complexity:** Low — Small state machine with retry; max 20 retries.

---

## Configuration (`src/config/`)

**Responsibility:** YAML-based configuration serialization/deserialization
utilities. Template parse helpers and `SERIALIZE_CONFIG` macros.

**Key instrumentation points:**

- `yaml_utils::parseValue()` — config loading (for tracking errors)
- This subsystem is primarily passive utility.

**Suggested span model:**
Not recommended for direct instrumentation. Config load/save events
can be emitted from the Account registration spans.

**Complexity:** Low — Pure utility, no meaningful runtime operations.

---

## Plugin System (`src/plugin/`)

**Responsibility:** Dynamic plugin loading/unloading, service registration,
certificate/signature verification, preferences management.

**Key instrumentation points:**

- `JamiPluginManager::installPlugin()` — installation with cert validation
- `PluginManager::load()` / `unload()` — dynamic library lifecycle
- `JamiPluginManager::checkPluginSignature()` — security verification

**Suggested span model:**
`plugin.install` and `plugin.load` spans.

**Useful span attributes:** `plugin.name`, `plugin.version`,
`plugin.signature_valid`

**Metrics candidates:**
- `jami.plugins.loaded` (gauge)
- `jami.plugins.install_failures` (counter)

**Complexity:** Medium — Plugin lifecycle is clear but cert verification
adds branching.

---

## Conference (`src/conference.h`, `src/conference.cpp`)

**Responsibility:** Multi-party call management. Tracks participants with rich
metadata (audio/video mute, moderator, hand raise, voice activity). Manages
subcall attachment/detachment, host media, video mixer, and moderation.

**Key instrumentation points:**

- `Conference::addSubCall()` / `removeSubCall()` — participant join/leave
- `Conference::attachHost()` / `detachHost()` — local host changes
- `Conference::requestMediaChange()` — media renegotiation

**Suggested span model:**
`conference.session` root span; child events for participant changes.

**Useful span attributes:** `conference.id`, `conference.participant_count`,
`conference.duration_ms`, `conference.is_recording`

**Metrics candidates:**
- `jami.conference.participants` (gauge)
- `jami.conference.duration_ms` (histogram)

**Complexity:** Medium — Interacts with audio/video mixers and call objects.

---

## Implementation Order (Recommended)

Calls is already committed as the first instrumented subsystem.
Remaining subsystems ranked by value-to-complexity ratio:

| Priority | Subsystem | Value | Complexity | Rationale |
|----------|-----------|-------|------------|-----------|
| **1** | **Calls** | ★★★★★ | Medium | Already implemented. Core user-facing operation. |
| 2 | SIP Transport | ★★★★ | Low | Transport issues cause hard-to-diagnose call failures |
| 3 | Instant Messaging | ★★★ | Low | Small surface, clear delivery state machine |
| 4 | Presence | ★★★ | Low | Small, well-scoped; helps debug presence issues |
| 5 | SIP Account (Registration) | ★★★★ | Medium | Registration failures are common support issues |
| 6 | Certificate / TLS | ★★★ | Low | Security validation visibility |
| 7 | File Transfer | ★★★★ | Medium | User-visible feature; SHA3 verification path |
| 8 | SDP Negotiation | ★★★★ | Medium | Key to diagnosing media negotiation failures |
| 9 | Conference | ★★★★ | Medium | Multi-party complexity benefits from tracing |
| 10 | SIP Signaling | ★★★ | Medium | Deep PJSIP integration |
| 11 | Media Codec (metrics) | ★★★★ | Medium | High value for quality analytics (metrics, not spans) |
| 12 | Plugin System | ★★ | Medium | Lower priority; internal tooling |
| 13 | Jami DHT Account | ★★★★★ | High | Highest value but most complex; do after simpler ones |
| 14 | Audio Pipeline | ★★★★ | High | Real-time constraints require careful instrumentation |
| 15 | Video Pipeline | ★★★★ | High | Same as audio; plus HW acceleration paths |
| 16 | Conversation/Messaging | ★★★★ | High | Git-based sync is complex; high diagnostic value |
| 17 | Configuration | ★ | Low | Passive utility; minimal runtime value |
