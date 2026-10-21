# ADR-010: Collaborative Documents as Y-CRDT over Swarm Repositories

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2026-07
**Recorded:** 2026-10-21

## Context

Collaborative editing needs concurrent edits from several devices to
converge without a central arbiter, reusing Jami's membership, trust and
synchronization.

## Decision

- Document state is a Y-CRDT, used through the yffi C FFI (libyrs, Rust),
  built as a contrib.
- Each document is a swarm repository (ADR-001) in `DOCUMENT` mode,
  linked to a parent conversation. CRDT updates persist as `CHECKPOINT`
  commits; attachments are content-addressed blobs.
- Live updates between editing devices travel over `ydoc://` channels
  (ADR-002).
- Holding a replica is a per-device choice. Documents are excluded from
  the conversation list, own-device sync and the account archive.

## Consequences

- Rust is a build dependency on every platform.
- Document repositories must never surface as conversations.
- Leaving a parent conversation drops its documents' replicas.

## Historical evidence

- **Facts:** `1083f0927` (2026-07) yffi contrib; `a0323c276` (2026-07)
  Y-CRDT wrapper; `301d5a12a` and `ac68d8222` (2026-08) document mode and
  synchronization; `1c057cc21` (2026-09) replication on announcement.
- **Documented rationale:** `1083f0927`: a CRDT "so that concurrent edits
  from several devices converge without a central arbitrator"; Y-CRDT "is
  a mature implementation" with a C FFI. `ac68d8222`: "Everything durable
  moves through that swarm."
- **Reconstructed:** none.
