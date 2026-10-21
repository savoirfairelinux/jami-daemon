# ADR-006: Move Peer-to-Peer Connectivity into dhtnet

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2022-10
**Recorded:** 2026-10-21

## Context

The daemon contained a general peer-to-peer networking stack
(ConnectionManager, multiplexed sockets, ICE, TLS sessions, certificate
store, TURN, UPnP) mixed with Jami-specific code.

## Decision

Generic connectivity lives in the separate dhtnet library, built as a
contrib and included as `<dhtnet/...>`. The daemon keeps only
Jami-specific logic: accounts, channel handlers and their protocols
(`src/jamidht/`), SIP integration and `src/connectivity/` helpers.

## Consequences

- Connectivity changes land in dhtnet first and reach the daemon through
  a contrib bump.
- dhtnet must not depend on Jami-specific concepts.
- Investigating a call or sync failure can span two repositories.

## Historical evidence

- **Facts:** `ac7ce59c4` (2022-10) separates connectivity code from
  Jami-specific code. `385721bb9` (2023-07) adds the dhtnet contrib.
  `25f641526` (2023-07) "switch to dhtnet" removes about 17,700 lines,
  mostly from `src/connectivity/`.
- **Documented rationale:** `ac7ce59c4`: "to prepare to the development of
  dhtnet ... seperating jami-specific code to connectivity-specific code".
- **Reconstructed:** reuse outside Jami is inferred from dhtnet's README
  ("Lightweight Peer-to-Peer Communication Library"), not stated in daemon
  history.
