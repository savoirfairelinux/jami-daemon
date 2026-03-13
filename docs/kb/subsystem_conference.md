# Conference

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The conference subsystem manages multi-party audio/video calls hosted by a single Jami daemon instance. It assembles a set of participant calls into a `Conference` object, drives audio mixing (via `RingBufferPool`) and video composition (via `VideoMixer`), tracks per-participant layout, mute, and moderator status, and provides a distributed conference protocol that allows participants to synchronize conference state over swarm messages. For JAMI accounts the conference is also tightly integrated with the Conversation (swarm) layer, allowing active-call presence to be tracked and shared across devices.

---

## Key Files

- `src/conference.h` / `src/conference.cpp` — `Conference` class, `ParticipantInfo` struct
- `src/conference_protocol.h` / `src/conference_protocol.cpp` — `ConfProtocolParser` (JSON protocol messages)
- `src/jamidht/conversation.h` / `.cpp` — `Conversation` (swarm-backed chat + active-call tracking)
- `src/jamidht/swarm/swarm_manager.h` / `.cpp` — `SwarmManager` (peer routing for distributed delivery)
- `src/jamidht/swarm/swarm_protocol.h` / `.cpp` — `SwarmProtocol` (message format and routing rules)
- `src/jamidht/swarm/routing_table.h` / `.cpp` — `RoutingTable` (Kademlia-like peer selection)
- `src/media/video/video_mixer.h` / `.cpp` — `VideoMixer` (conference video compositor)
- `src/media/audio/ringbufferpool.h` — `RingBufferPool` (audio mixing by routing ring buffers)
- `src/jami/conversation_interface.h` — public API: `getActiveCalls`, `hostConference`

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `Conference` | Central object for a hosted multi-party call; owns participant call map, moderator list, layout state | `src/conference.h` |
| `ParticipantInfo` | Per-participant record: uri, device, sinkId, active, muted flags, position (x/y/w/h), voiceActivity | `src/conference.h` |
| `ConfProtocolParser` | Parses and emits the JSON protocol messages exchanged between conference participants | `src/conference_protocol.h` |
| `Conversation` | Swarm-backed persistent conversation; tracks `activeCalls` and `hostedCalls` subdirectories | `src/jamidht/conversation.h` |
| `ConversationRepository` | Git-based append-only repository of conversation messages | `src/jamidht/conversationrepository.h` |
| `SwarmManager` | Manages overlay mesh of dhtnet channel connections to conversation members | `src/jamidht/swarm/swarm_manager.h` |
| `SwarmProtocol` | Defines swarm message types and routing/delivery rules | `src/jamidht/swarm/swarm_protocol.h` |
| `RoutingTable` | Kademlia-inspired routing table for peer selection within a swarm | `src/jamidht/swarm/routing_table.h` |
| `VideoMixer` | Composites frames from multiple `VideoInput` sources into a single renderer output | `src/media/video/video_mixer.h` |

---

## External Dependencies

- **jsoncpp** — conference protocol message serialization
- **asio** — channel event dispatch and timer-driven participant timeout
- **dhtnet** (`ChannelSocket`) — transport for swarm message exchange
- **libgit2** (via `ConversationRepository`) — persistent conversation store
- **FFmpeg** (via `VideoMixer`) — frame scaling and composition
- **msgpack** — swarm message serialization

---

## Threading Model

- **Conference state mutations** (join/leave, mute, layout changes): driven by the ASIO io_context via channel socket callbacks from `SwarmManager` and `ConfProtocolParser`.
- **Audio mixing**: the `RingBufferPool` is shared across all calls in the conference; mixing happens on the `AudioLayer` hardware callback thread by routing buffers rather than running a separate mixing thread.
- **Video mixing**: `VideoMixer` runs on its own `ThreadLoop`; inputs are grabbed from per-participant video sinks.
- **Swarm routing** (`SwarmManager`, `RoutingTable`): asio io_context callbacks.

---

## Estimated Instrumentation Value

**Medium.** Conference participant join/leave events and moderator actions are meaningful for support diagnostics. The swarm routing layer (message delivery latency, missing ACKs) is also worth tracing. Layout change streams are high-frequency and lower value for tracing; aggregate counts would suffice.

---

## Open Questions

1. What is the maximum tested participant count for a hosted conference?
2. Does the conference protocol enforce a single host, or can hosting migrate to another peer?
3. How is the `VideoMixer` layout algorithm triggered — is it automatic (grid) or manually set by a moderator?
4. What is the failure behavior when the conference host disconnects — does the conference dissolve or is there a hand-off mechanism?
5. Is conversation-backed conference state (activeCalls directory) eventually consistent or do conflicts require manual resolution?
