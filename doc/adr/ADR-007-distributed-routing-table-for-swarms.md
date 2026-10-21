# ADR-007: Connect Swarm Members through a Distributed Routing Table

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2023-03
**Recorded:** 2026-10-21

## Context

Every swarm member connected to every other member, which does not scale
with conversation size and is costly for mobile devices.

## Decision

Each conversation has a SwarmManager (`src/jamidht/swarm/`) keeping a
routing table of member devices in buckets, modelled on OpenDHT. A device
connects to a subset of members, and messages propagate through them.
Mobile devices announce themselves; when they disconnect they are kept as
mobile nodes, reached through existing sockets or push notifications,
rather than reconnected.

## Consequences

- In large swarms delivery is multi-hop; members are not all directly
  connected.
- Mobile behaviour depends on the mobile-node state and push support.
- Messages on `swarm://` channels are a wire contract.

## Historical evidence

- **Facts:** `3b0def092` and `f2288e0cc` (2023-03) add buckets, routing
  table, swarm protocol and manager; `b3d8577db` (2023-03) integrates them
  into conversations.
- **Documented rationale:** `f2288e0cc`: it "should avoid the fact that
  every peer in a swarm must connects to every other peer", treating
  mobile nodes specially for push and battery.
  [DRT](https://docs.jami.net/developer/jami-concepts/drt.html) lists the
  alternatives considered.
- **Reconstructed:** none.
