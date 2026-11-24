# ADR-017: Verified Circle Sync

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-11-09
**Recorded:** 2026-11-24

## Context

Peers need to fetch [signed histories](ADR-015-signed-circle-history.md)
without leaking unverified objects or announcing private channel heads to
members who cannot read them.

## Decision

Mount circle and channel repositories from trusted local paths. Peer
requests select a mounted resource, never a path. Use reserved, versioned
`git://<device>/circles/v1/<id>` and `git://<device>/channels/v1/<id>`
routes, with exact device and repository IDs. Malformed or unknown routes
in these namespaces do not fall back to conversation lookup.

Authorize the presented device and its account using current verified
circle policy and existing account trust checks. An invited account can
bootstrap circle metadata, not channel content. Active accounts can read
using an account-certified device before enrolling it as a commit signer.
Channel reads additionally require `ReadChannel`. Device revocation stays
account-level.

Only the currently granted verified head is advertised, and served
objects must be reachable from the advertised and currently granted heads.
Guard requests and outgoing chunks against live revocation. Cache grants
against the mount and publication generations and certificate expiry, with
a 30-second ceiling for trust changes without an event. Limit sessions per
peer account and globally. Download into a temporary ref and advance the
accepted head only after validation; clone into a staging directory before
mounting. If channel validation is pending on circle policy, fetch the
circle from the same device before retrying the channel.

Reuse [swarm routing tables](ADR-007-distributed-routing-table-for-swarms.md)
and mobile leases. Each circle has one table at
`swarm://circles/v1/<circle-id>`, shared by its channels. Exchange heads over
the message channel: circle heads go to invited and active members, and
channel heads only to eligible readers. Coalesce pulls by route and device
and do not echo a head to its source. After a local leave, unmount the
channels and serve the circle until a peer fetches the departure head or
the seven-day grace period expires.

## Consequences

Circles reuse conversation connectivity with bounded announcement fan-out;
only subscriptions need to replicate channel content. Serving checks current
authority independently of historical write validity. Revocation can stop
future output but cannot recall bytes already delivered.
