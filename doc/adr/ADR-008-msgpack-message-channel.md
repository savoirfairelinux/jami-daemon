# ADR-008: Exchange Peer Protocol Messages over a Message Channel

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2024-11
**Recorded:** 2026-10-21

## Context

Protocol messages to contacts' devices (commit notifications, read
receipts, typing) were SIP MESSAGE requests over the `sip` channel: limited by
pjsip's message size, carrying SIP header overhead, and unable to carry
raw binary data.

## Decision

Protocol messages to contact devices use a `msg:` channel
(`MessageChannelHandler`) with msgpack serialization. SIP MESSAGE remains
the fallback when the peer has no message channel.

## Consequences

- Larger and binary payloads work between up-to-date peers.
- Both send paths must stay equivalent while older peers exist.
- Messages to the account's own devices use the sync channel instead.

## Historical evidence

- **Facts:** `a5c6ad6c6` (2024-11) "use MessageChannel for p2p protocol
  messages"; `68fc552fc` (2025-04) uses it for connection status.
- **Documented rationale:** `a5c6ad6c6`: "overcoming the pjsip message
  size limitation and avoiding the overhead of the SIP message header",
  allowing raw binary data, with a backward-compatible SIP fallback.
- **Reconstructed:** none.
