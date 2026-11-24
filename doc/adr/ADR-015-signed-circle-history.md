# ADR-015: Signed Circle History

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-10-22
**Recorded:** 2026-11-24

## Context

Peers need to derive the same authorized state from writes made while offline.
Trusting the state supplied by a merge author would bypass
[circle policy](ADR-014-circle-owned-channel-policy.md).

## Decision

Store circle history in a bare Git repository. The circle ID is the hash of
its signed root, which includes a fresh 32-byte nonce. Strict version-1 JSON
records bind operations to their circle and actual parents. Verify the
original signed Git bytes, the signing device's account identity, authority
at the original parent, and the exact operation-specific tree changes before
publishing a head conditionally. Fast-forwards do not require a new signature.

Membership and delegated roles belong to accounts, not devices. Invite by
account ID; self-join proves that identity and enrolls an account-certified
device. Additional devices enroll without changing the member's authority.
Leaving or being banned drops roles and enrolled devices; unban creates a
fresh invitation. A ban or unban consumes one eligible account's signed vote;
the vote binds the voter and target revisions, and resolution rechecks both
voter and resolver. There is no quorum. ADMIN must transfer to an active
successor before leaving, or end the circle. An ended circle remains
readable by authorized members, but accepts no new ordinary operations.

Replay divergent histories parent-first. Original parents authorize the
signed events; a deterministic reducer decides what survives:

- Causal lifecycle changes supersede earlier ones. Concurrent membership
  states prefer `BANNED`, `LEFT`, `ACTIVE`, then `INVITED`. Roles and
  devices are bound to their membership generation. Equivalent resolutions
  use the original vote ID, and stale or consumed evidence cannot revive.
- Merge actual permission changes, not whole snapshots. Concurrent removal
  of an allow, addition of a deny, unassignment, or narrowing of a delegation
  wins over its inverse. Relevant concurrent revocations suppress delegated
  actions that depended on the revoked authority, as well as their dependents.
  Recheck composed effects for peer degradation.
- Ending suppresses concurrent work; transferring suppresses concurrent
  operations of the transferring ADMIN. A transfer requires a surviving
  active target. Where a tie needs ordering, use original commit IDs, not
  parent order or wall-clock time.

Save accepted state atomically alongside the repository, keyed by circle, head
and validation version. Like the account's keys and trust store, this state
is trusted local data, never a peer-provided snapshot. Reuse it for reopen
and replay only the new commits on a linear update when possible. Fall
back to full replay when the required history cannot be checked incrementally.

## Consequences

Concurrent valid work can converge without trusting an arbitrary merge tree.
Explicit history, frontier and replay limits fail closed without publishing a
partial head; they do not promise unlimited divergence or historical replay.
The model provides neither Byzantine consensus nor trusted offline timestamps.
