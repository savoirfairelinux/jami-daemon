# ADR-016: Policy-Bound Channel History

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-10-23
**Recorded:** 2026-11-24

## Context

Channels need independent replication without duplicating
[circle authority](ADR-014-circle-owned-channel-policy.md). A signed
channel history must not be swappable into another circle or use policy
from an unaccepted branch.

## Decision

Create an empty-tree signed channel root citing an accepted circle policy
commit, the circle ID, creator, nonce and initial access rules. Register
the exact signed root in a subsequent circle operation. The root hash is
the channel ID; the registration must match the root's policy and initial
access. Verify the creator against the cited policy and the registrar
against the registration's parent. Delegated registration may only
restrict inherited access; an orphan root is not an active channel.
Archiving is a permanent registry tombstone that stops new content writes
but does not by itself revoke read access.

Channel text, replies, reactions, edits and deletions use conversation
message fields inside signed version-1 channel records. The record binds
the channel, actual parent and the accepted circle policy authorizing it.
Validate the author, action permission, target and signed history against
that policy. Signed channel merges bind their actual parents and add no
content. A missing or unaccepted policy is a pending dependency, not
authorization to publish or serve the incoming history.

Local writers use the current accepted policy, never a caller-selected
historical policy. History already authorized under an accepted ancestor
stays valid after a permission change, departure, ban or archive. Live
serving instead checks current policy.

## Consequences

Channels have independent content history but cannot fork membership or
authority away from their parent circle. Revocation is weak: signatures
cannot prove when an offline write happened, and current access controls
cannot recall already replicated bytes. Attachments, calls and collaborative
documents are not channel content in this version.
