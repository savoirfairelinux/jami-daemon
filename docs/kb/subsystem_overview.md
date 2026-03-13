# Subsystem Overview

## Status: draft

## Last Updated: 2026-03-13

---

## Source References

Files read to produce this document:

- `README.md`
- `CMakeLists.txt` (top-level)
- `src/manager.h` / `src/manager.cpp`
- `src/account.h`
- `src/call.h`
- `src/logger.h`
- `src/threadloop.h`
- `src/preferences.h`
- `src/conference.h`
- `src/data_transfer.h`
- `src/jami/jami.h`
- `src/jami/callmanager_interface.h`
- `src/jami/configurationmanager_interface.h`
- `src/jami/datatransfer_interface.h`
- `src/jami/conversation_interface.h`
- `src/sip/sipaccount.h`
- `src/sip/sipcall.h`
- `src/sip/sdp.h`
- `src/sip/siptransport.h`
- `src/jamidht/jamiaccount.h`
- `src/jamidht/account_manager.h`
- `src/jamidht/namedirectory.h`
- `src/jamidht/conversation.h`
- `src/jamidht/swarm/` (directory listing)
- `src/media/audio/audiolayer.h`
- `src/media/rtp_session.h`
- `src/media/` (directory listing)
- `src/media/audio/` (directory listing)
- `src/media/video/` (directory listing)
- `src/plugin/jamipluginmanager.h`
- `src/plugin/` (directory listing)
- `src/im/message_engine.h`
- `src/im/` (directory listing)
- `src/connectivity/` (directory listing)
- `src/connectivity/security/tlsvalidator.h`
- `src/config/yamlparser.h`
- `src/config/` (directory listing)

---

## Subsystem Table

| Subsystem | Key Files | Purpose | Key Classes | External Deps | Threading Model | Est. Call Volume |
|---|---|---|---|---|---|---|
| **account_management** | `src/account.h`, `src/account_factory.h`, `src/sip/sipaccount.h`, `src/jamidht/jamiaccount.h`, `src/jamidht/account_manager.h` | Lifecycle management for SIP and JAMI/DHT accounts; registration, credential management, contact lists, device linking | `Account`, `SIPAccountBase`, `SIPAccount`, `JamiAccount`, `AccountManager`, `AccountFactory` | PJSIP, OpenDHT, dhtnet | Main thread (ASIO io_context) + per-account async tasks | High — every API call routes through an account |
| **call_manager** | `src/call.h`, `src/call_factory.h`, `src/sip/sipcall.h`, `src/sip/sdp.h`, `src/sip/sipvoiplink.h` | Call lifecycle (outgoing/incoming/hold/transfer/hang-up), SDP offer/answer negotiation, ICE candidate exchange | `Call`, `SIPCall`, `CallFactory`, `Sdp`, `SIPVoIPLink` | PJSIP (`pjsip_inv_session`), dhtnet ICE, asio timers | Dedicated SIP worker thread (PJSIP event loop) + asio timers | High — every call event |
| **dht_layer** | `src/jamidht/jamiaccount.h`, `src/jamidht/account_manager.h`, `src/jamidht/namedirectory.h`, `src/jamidht/contact_list.h` | OpenDHT node management, peer discovery, device announcement, name directory lookup | `JamiAccount`, `AccountManager`, `ArchiveAccountManager`, `ServerAccountManager`, `NameDirectory`, `ContactList` | OpenDHT, dhtnet, libgit2 (gittransport) | OpenDHT thread pool + asio io_context; async callbacks | Medium — DHT lookups at registration and call setup |
| **media_pipeline** | `src/media/media_encoder.h`, `src/media/media_decoder.h`, `src/media/audio/audiolayer.h`, `src/media/audio/audio_rtp_session.h`, `src/media/video/video_rtp_session.h`, `src/media/socket_pair.h`, `src/media/srtp.h` | Audio/video encoding & decoding (FFmpeg/libav), RTP/SRTP packetization, audio ring-buffer mixing, video mixing | `MediaEncoder`, `MediaDecoder`, `AudioLayer`, `AudioRtpSession`, `VideoRtpSession`, `RtpSession`, `SocketPair`, `RingBufferPool`, `VideoMixer` | FFmpeg/libavcodec, libavformat, libswresample, libswscale, libsrtp, Speex (AEC), OpenSLES/PulseAudio/ALSA/CoreAudio/AAudio | Per-stream `ThreadLoop` (setup/process/cleanup pattern); audio capture on hardware callback thread | Very High — continuous media frames during calls |
| **conference** | `src/conference.h`, `src/conference_protocol.h`, `src/jamidht/conversation.h`, `src/jamidht/swarm/swarm_manager.h` | Multi-party call hosting, participant layout management, audio/video mixing, moderator controls, swarm-based distributed session | `Conference`, `ParticipantInfo`, `Conversation`, `SwarmManager`, `SwarmProtocol` | JSON (jsoncpp), asio, dhtnet channels | Main ASIO io_context for signaling; media mixing on `ThreadLoop` threads | Medium — activated when conferences/group calls are ongoing |
| **data_transfer** | `src/data_transfer.h`, `src/data_transfer.cpp`, `src/jami/datatransfer_interface.h`, `src/jamidht/transfer_channel_handler.h` | File transfer over dhtnet channel sockets; incoming/outgoing with resume, SHA3 integrity verification | `FileInfo`, `IncomingFile`, `OutgoingFile`, `TransferManager`, `WaitingRequest` | dhtnet (`ChannelSocket`), msgpack | Separate file I/O thread per transfer (reads/writes stream); channel callbacks on asio thread | Low-Medium — bursty during file transfers |
| **plugin_system** | `src/plugin/jamipluginmanager.h`, `src/plugin/pluginmanager.h`, `src/plugin/callservicesmanager.h`, `src/plugin/chatservicesmanager.h`, `src/plugin/pluginloader.h` | Dynamic plugin loading (.jpl archives), call/chat/preference/webview service injection, certificate-validated installation | `JamiPluginManager`, `PluginManager`, `PluginLoader`, `CallServicesManager`, `ChatServicesManager`, `PreferenceServicesManager`, `WebViewServicesManager` | OpenDHT crypto (cert validation), dlopen/FreeLibrary | Same thread as caller (hook-based injection); plugin execution synchronous within the calling thread | Low — plugin hooks invoked per call event or message |
| **logging** | `src/logger.h`, `src/logger.cpp` | Level-driven (DEBUG/INFO/WARNING/ERROR) thread-safe logging with fmt, syslog/Android logcat/Windows EventLog backends; OpenDHT logger integration | `Logger`, `Logger::Handler`, `Logger::Msg` | {fmt}, syslog, Android NDK logcat, winsyslog | Thread-safe: concurrent callers serialize via internal mutex in handler; low-overhead `fmt`-based formatting | Very High — every subsystem emits logs |
| **im_messaging** | `src/im/message_engine.h`, `src/im/instant_messaging.h`, `src/jamidht/message_channel_handler.h` | Reliable out-of-band text messaging with retry logic, persistence via msgpack, SIP MESSAGE method for SIP accounts | `MessageEngine`, `InstantMessaging`, `MessageChannelHandler` | msgpack, asio timers, PJSIP (SIP MESSAGE) | asio io_context for retry timers; message queue protected by mutex | Medium — per-message, typically low rate |
| **connectivity** | `src/connectivity/ip_utils.h`, `src/connectivity/sip_utils.h`, `src/sip/siptransport.h`, (ICE via dhtnet) | IP address discovery (local/NAT), STUN/TURN resolution, SIP transport lifecycle (UDP/TCP/TLS), UPnP port mapping | `SipTransport`, `SipTransportBroker`, `IceTransport` (dhtnet), `TlsListener`, `TlsInfos` | dhtnet (ICE, UPnP), PJSIP (pjnath), PJLIB | Main ASIO thread + PJSIP network event thread; ICE negotiation is async | High — every call setup involves ICE and transport negotiation |
| **certificate_pki** | `src/connectivity/security/tlsvalidator.h`, `src/connectivity/security/memory.h` (dhtnet certstore via `<dhtnet/certstore.h>`) | TLS certificate validation, trust anchor management, certificate pinning for JAMI accounts, SDES key negotiation | `TlsValidator`, `CertificateStore` (dhtnet), `SdesNegotiator` | dhtnet certstore, GnuTLS/OpenSSL (via dhtnet), OpenDHT crypto | Synchronous validation at connection time; certstore queries on calling thread | Medium — invoked at every TLS handshake and call setup |
| **config_persistence** | `src/config/yamlparser.h`, `src/config/serializable.h`, `src/preferences.h`, `src/account_config.h` | YAML-based serialization/deserialization of global preferences, per-account configuration, and codec lists | `Serializable`, `Preferences`, `VoipPreference`, `AudioPreference`, `AccountConfig`, `SipAccountConfig`, `JamiAccountConfig` | yaml-cpp | Loaded at startup on main thread; saved synchronously on config change | Low — read at startup, written on config change |

---

## Architecture Summary

Jami daemon (`libjami`) is structured as a **C++20 shared/static library** (project name `jami-core`, version 16.0.0) that exposes a pure-C API surface to UIs and IPC bindings. The central coordinating singleton is `Manager`, which owns an ASIO `io_context`, the `AccountFactory`, `CallFactory`, `RingBufferPool`, plugin manager, and global preference objects.

The two account families — **SIP** (via PJSIP) and **JAMI/DHT** (via OpenDHT + dhtnet) — both inherit from `Account` → `SIPAccountBase`. `SIPAccount` handles vanilla SIP registrar interaction; `JamiAccount` layers OpenDHT peer discovery, encrypted channel multiplexing (dhtnet), conversation/swarm management, and device-to-device linking on top.

Call state is managed in the **call_manager** subsystem: `SIPCall` inherits from `Call` and holds a set of `RtpStream` records (each pairing a `RtpSession` with ICE sockets). SDP negotiation is handled by the `Sdp` class using PJMEDIA APIs. Once ICE completes, the **media_pipeline** takes ownership of the ICE sockets and runs audio/video encode–decode loops in dedicated `ThreadLoop` threads.

The **connectivity** subsystem provides the transport bridging: PJSIP transports (UDP/TCP/TLS via `SipTransport`) and ICE tunnels (via `dhtnet::IceTransport`). UPnP is managed by `dhtnet::upnp`.

Group communication is handled by **conference** for real-time mixing and by the **dht_layer** / **im_messaging** subsystems for persistent conversations. `Conversation` objects (in `src/jamidht/conversation.h`) are backed by a git repository (`ConversationRepository`, `GitServer`) providing a content-addressed, tamper-evident history. Message propagation leverages a **swarm** overlay (`SwarmManager`, `SwarmProtocol`) for decentralized delivery.

The **plugin_system** installs `.jpl` zip archives, validates their certificate, dlopen-loads the `.so`, and injects hooks into call, chat, webview, and preference service pipelines.

All major subsystems use `JAMI_DBG`/`JAMI_ERR` macros from **logging** (backed by `Logger`), which dispatches through platform-specific sinks (syslog, Android logcat, Windows EventLog).

Configuration is persisted as YAML via the **config_persistence** subsystem, using a `Serializable` interface implemented by `Preferences` and all `*Config` types.

---

## IPC/API Boundary

The `src/jami/` directory constitutes the **entire public API surface** of libjami. It contains header-only interface declarations under the `libjami` namespace:

| File | Interface |
|---|---|
| `jami.h` | `init()`, `start()`, `fini()`, `version()`, `InitFlag` enum |
| `callmanager_interface.h` | `placeCall`, `accept`, `hangUp`, `hold`, `resume`, `transfer`, `muteLocalMedia`, signal registration |
| `configurationmanager_interface.h` | `addAccount`, `setAccountDetails`, `getAccountDetails`, `addDevice`, `exportToFile`, `revokeDevice`, `getConnectionList` |
| `conversation_interface.h` | `startConversation`, `sendMessage`, `getSwarmMessages`, `acceptConversationRequest`, `addConversationMember` |
| `datatransfer_interface.h` | `sendFile`, `dataTransferInfo`, `downloadFile`, event codes |
| `videomanager_interface.h` | Video sink/source APIs, device listing, frame injection |
| `presencemanager_interface.h` | Presence subscription/publication |
| `plugin_manager_interface.h` | `loadPlugin`, `unloadPlugin`, `installPlugin`, `getInstalledPlugins` |
| `def.h` | `LIBJAMI_PUBLIC` visibility macros |
| `account_const.h`, `call_const.h`, `media_const.h`, `security_const.h`, `presence_const.h` | Shared string constants |
| `trace-tools.h`, `tracepoint.h` | LTTng tracepoint definitions |

UIs (Qt, GTK, Android JNI, Node.js, D-Bus) consume only this surface. D-Bus XML is generated from annotations in `bin/dbus/`; JNI binding is generated via SWIG (`bin/jni/`).

---

## Open Questions

1. **Threading ownership of Manager** — `Manager::instance()` is a Meyers singleton; thread-safety of initialization under concurrent `init()` calls is unclear.
2. **SwarmManager capacity limits** — what is the maximum swarm size before routing degrades? No constant is visible in headers.
3. **Plugin sandbox** — plugins are dlopen-loaded in-process with no memory isolation; security boundary is certificate validity only.
4. **ICE fallback path** — when both STUN and TURN fail, does the code fall back to direct IP or drop the call? The fallback logic is in dhtnet, not directly visible.
5. **MessageEngine persistence format** — messages are serialized with msgpack, but the storage path and rotation policy are not documented.
6. **SDES vs DTLS-SRTP selection** — `sdes_negotiator.h` exists alongside dhtnet TLS channels; it is unclear which path is taken for JAMI calls vs SIP calls.
7. **Video acceleration** — `accel.h/cpp` exists under `src/media/video/`; which codecs/platforms are covered needs verification.
