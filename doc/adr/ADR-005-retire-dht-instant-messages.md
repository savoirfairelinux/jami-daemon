# ADR-005: Retire DHT Instant Messages in Favour of Swarm

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2022-04
**Recorded:** 2026-10-21

## Context

Before swarm (ADR-001), text messages were encrypted values put on the
DHT under each device's `inbox:` key. Once swarm stored conversation
history, there were two message paths.

## Decision

Conversation messages exist only as swarm commits. The DHT no longer
carries instant messages; it is used for discovery and signalling only:
device announcements, CRLs, trust requests on the `inbox:` key, and
connection requests. Ephemeral peer messages (new-commit notifications,
typing, read receipts) travel over peer-to-peer channels (ADR-002,
ADR-008).

## Consequences

- Clients without swarm support cannot exchange messages.
- Delivery needs a peer-to-peer connection, directly or via a later sync.
- Message payloads must not be reintroduced on DHT keys.

## Historical evidence

- **Facts:** `d352fb8f1` (2022-04) removes the downgrade to non-swarm
  conversations on receipt of a DHT message. `7463a8a26` (2024-04)
  removes DHT message exchange, "replaced by dhtnet and SIP messages".
  Remaining DHT `put`/`listen` calls are in `account_manager.cpp`.
- **Documented rationale:** `d352fb8f1`: "it's been months since all
  clients supports swarm", so the fallback was "just unwanted code".
- **Reconstructed:** none.
