# IM Messaging

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The IM messaging subsystem provides reliable, persistent, out-of-band text message delivery between Jami peers. For JAMI/DHT accounts it integrates with the Conversation (swarm) layer for group messaging; for SIP accounts it uses the SIP MESSAGE method via PJSIP. `MessageEngine` implements a retry queue with exponential back-off (up to 20 retries), msgpack-based persistence of pending messages, and peer-online notifications to trigger delivery of queued messages. In-call instant messaging uses a separate path (SIP INFO or in-conversation messages). Typing indicators (`Typers`) are handled as ephemeral DHT values.

---

## Key Files

- `src/im/message_engine.h` / `src/im/message_engine.cpp` — `MessageEngine`: retry queue, persistence, delivery state
- `src/im/instant_messaging.h` / `src/im/instant_messaging.cpp` — `InstantMessaging`: SIP MESSAGE content type helpers
- `src/jamidht/message_channel_handler.h` / `.cpp` — `MessageChannelHandler`: routes messages over dhtnet channels for JAMI accounts
- `src/jamidht/conversation.h` / `.cpp` — `Conversation`: git-backed group conversation; calls `ConversationRepository::commitMessage()`
- `src/jamidht/conversationrepository.h` / `.cpp` — `ConversationRepository`: libgit2-backed message log
- `src/jamidht/conversation_module.h` / `.cpp` — `ConversationModule`: per-account conversation registry
- `src/jamidht/typers.h` / `.cpp` — `Typers`: ephemeral typing indicator state (using DHT transient values)
- `src/jami/configurationmanager_interface.h` — `sendTextMessage` (point-to-point)
- `src/jami/conversation_interface.h` — `sendMessage` (conversation-scoped)

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `MessageEngine` | Per-account retry queue; maps `MessageToken` → `MessageStatus`; persists queue via msgpack; uses `asio::steady_timer` for retries | `src/im/message_engine.h` |
| `InstantMessaging` | Helpers to build and parse SIP MESSAGE MIME payloads (Content-Type selection) | `src/im/instant_messaging.h` |
| `MessageChannelHandler` | Opens/accepts dhtnet channels named for messaging; serializes/deserializes message payloads | `src/jamidht/message_channel_handler.h` |
| `Conversation` | Git-backed group conversation; owns `ConversationRepository`; manages members, sending queue, and fetch state | `src/jamidht/conversation.h` |
| `ConversationRepository` | Wraps libgit2; appends signed commits as messages; resolves merge conflicts via DAG linearization | `src/jamidht/conversationrepository.h` |
| `ConversationModule` | Per-`JamiAccount` registry of `Conversation` instances; handles conversation creation, invitation, and sync | `src/jamidht/conversation_module.h` |
| `Typers` | Tracks which peers are currently typing; delivers transient DHT values | `src/jamidht/typers.h` |

---

## External Dependencies

- **msgpack** — `MessageEngine` queue persistence; message payload serialization in `MessageChannelHandler`
- **libgit2** — `ConversationRepository` commit storage and synchronization
- **asio** (`asio/steady_timer.hpp`) — `MessageEngine` retry timer
- **PJSIP** (`pjsip_tx_data`) — SIP MESSAGE send for `SIPAccount` in-band messaging
- **dhtnet** (`ChannelSocket`) — transport for `MessageChannelHandler`
- **OpenDHT crypto** — message signing in `ConversationRepository` commits

---

## Threading Model

- `MessageEngine` state (queue map, timer) is protected by its own `std::mutex`; callbacks from `asio::steady_timer` fire on the io_context thread.
- `onPeerOnline()` can be called from the DHT callback thread → acquires mutex → may post timer cancellation to asio.
- `ConversationRepository` commits are performed on a background worker thread inside `ConversationModule` (dispatch queue pattern) to avoid blocking the io_context.
- `MessageChannelHandler` channel read callbacks arrive on dhtnet's asio executor thread.

---

## Estimated Instrumentation Value

**Medium.** Message delivery latency (time between send and `SENT` status), retry count, and delivery failures are meaningful for SLA and debugging. `MessageEngine`'s `MAX_RETRIES` exhaustion events are high-value but low-frequency trace points. Conversation sync events (fetch, merge) are also worth tracing for diagnosing group message delivery issues.

---

## Open Questions

1. Is message order guaranteed within `MessageEngine`, or can retries cause out-of-order delivery?
2. For JAMI accounts, is `MessageEngine` still used for 1:1 messages, or is the `Conversation` layer always used?
3. What is the msgpack persistence file path and rotation/cleanup policy?
4. How are read receipts tracked — are they separate commit types in `ConversationRepository`?
5. Are typing indicators (`Typers`) sent for group conversations or only for 1:1 DHT messages?
