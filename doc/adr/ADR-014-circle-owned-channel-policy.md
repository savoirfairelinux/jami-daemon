# ADR-014: Circle-Owned Channel Policy

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-10-22
**Recorded:** 2026-10-22

## Context

Communities need multiple channels without copying membership and governance
into every channel. Standalone swarms retain their existing modes and
[permission matrix](ADR-012-explicit-swarm-permissions.md).

## Decision

A circle owns membership, roles, delegation and a registry of channels.
Each channel belongs to exactly one circle and has no separate membership
policy. The same pure policy evaluator is used for validation and live
authorization; it evaluates a specified member and optional channel against a
verified circle snapshot.

- Only active members exercise ordinary permissions. A member receives the
  default rule and the rules of their assigned roles. Denies win and absent
  grants deny. For each rule a channel may inherit it, replace it, or use
  an explicitly empty replacement. All channel actions require `ReadChannel`.
  This lets a private channel remove default read access and grant it to
  selected roles without introducing a deny that would override those grants.
- There is one designated active `ADMIN`, independent of custom roles.
  ADMIN bypasses ordinary rule denies, not membership and resource lifecycle
  restrictions or validation of channel and permission identifiers.
- ADMIN defines circle defaults, creates roles and sets delegation scopes.
  Delegated assignment and editing require the relevant command permission
  and a single ADMIN-issued grant covering the whole operation, including
  any new power unlocked by a rule edit. Grants cannot be pooled to broaden
  scope. Moderation is independent of role assignment.
- Only ADMIN may degrade another governance holder: delegated moderators
  cannot ban protected peers, and delegated policy changes must preserve
  peers' effective governance permissions and usable delegation scopes,
  including the channel read access they depend on. A member may reduce
  their own authority.

## Consequences

Channels can replicate independently while sharing one source of authority.
Channel names and rules are visible in circle metadata; private channels
restrict content access, not discovery within the circle. Permission lookups
are indexed; policy mutation can inspect affected members and channels to
check indirect grants and losses. No quorum, device roles, or new encryption
model is introduced.
