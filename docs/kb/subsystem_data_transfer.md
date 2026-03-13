# Data Transfer

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The data transfer subsystem manages file transfers between Jami peers. It uses `dhtnet::ChannelSocket` as the byte-stream transport, supports both incoming and outgoing transfers, enforces SHA-3 integrity verification at completion, handles user cancellation and channel-close events, and integrates with the conversation layer (files are associated with conversation interaction IDs). Transfers are tracked via `DataTransferId` (uint64) tokens and emit lifecycle events (`DataTransferEventCode`) to the client API.

---

## Key Files

- `src/data_transfer.h` / `src/data_transfer.cpp` — `FileInfo`, `IncomingFile`, `OutgoingFile`, `WaitingRequest`, `TransferManager`, UID generator
- `src/jami/datatransfer_interface.h` — public API: `sendFile`, `downloadFile`, `dataTransferInfo`, event codes
- `src/jamidht/transfer_channel_handler.h` / `.cpp` — `TransferChannelHandler`: registers dhtnet channel for file transfers and dispatches to `TransferManager`
- `src/jamidht/conversation.h` — `Conversation::TransferManager` accessor; associates transfers with conversation interactions

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `FileInfo` | Abstract base for a single transfer; holds `ChannelSocket`, `DataTransferInfo`, `fileId`, `interactionId`; emits `DataTransferEventCode` | `src/data_transfer.h` |
| `IncomingFile` | Receives bytes from channel, writes to filesystem, verifies SHA-3 sum on completion | `src/data_transfer.h` |
| `OutgoingFile` | Reads file from filesystem, sends via channel, handles backpressure | `src/data_transfer.h` |
| `WaitingRequest` | Persisted pending-download record (msgpack): fileId, interactionId, sha3sum, path, totalSize | `src/data_transfer.h` |
| `TransferChannelHandler` | dhtnet channel handler; multiplexes file-transfer channels from JAMI account connections | `src/jamidht/transfer_channel_handler.h` |

---

## External Dependencies

- **dhtnet** (`dhtnet/multiplexed_socket.h`, `ChannelSocket`) — byte-stream transport over authenticated DHT channels
- **msgpack** — `WaitingRequest` persistence for interrupted transfer resume
- **OpenSSL / libgcrypt** (via OpenDHT) — SHA-3 digest computation for integrity check
- `src/jami/datatransfer_interface.h` — `DataTransferEventCode`, `DataTransferInfo`, `DataTransferId`

---

## Threading Model

- **I/O loop**: each `IncomingFile` / `OutgoingFile` performs blocking file I/O; this runs on a dedicated thread (or the channel's read callback, depending on implementation) separate from the ASIO io_context.
- **Channel callbacks**: `ChannelSocket` read/write completion callbacks are dispatched on dhtnet's internal asio executor; `FileInfo::process()` is invoked from there.
- **Cancellation**: `isUserCancelled_` is an `std::atomic_bool`, polled by the I/O loop.
- **Event emission** (`emit()`): posts `DataTransferEventCode` notifications back through the Manager to the client API callback.

---

## Estimated Instrumentation Value

**Low–Medium.** File transfers are user-initiated, infrequent, and already have rich event codes. The most useful trace points would be transfer start/finish/cancel with byte counts and SHA-3 verification outcome. Progress events are too frequent for tracing; they should be counters instead.

---

## Open Questions

1. Is there a transfer resume mechanism after a connection drop, or does the transfer restart from zero?
2. Is there a maximum concurrent transfer count per account?
3. Does `OutgoingFile` implement flow control / backpressure against the channel socket, or does it send as fast as possible?
4. How are `WaitingRequest` records cleaned up if the conversation is deleted before the transfer is accepted?
