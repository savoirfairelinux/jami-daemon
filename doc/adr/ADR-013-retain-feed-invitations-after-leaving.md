# ADR-013: Retain Feed Invitations after Leaving

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-11-11
**Recorded:** 2026-11-11

## Context

Leaving a feed removes the follower's membership and eventually its local
repository. Recreating only a local invitation cannot enable resubscription:
peers require an invited entry to authorize cloning and validate a join.
Clients already display feed invitations and support accepting or declining
them, so no additional client state is needed.

## Decision

- Grant feed followers, but not admins, the explicit `Leave` permission.
- A follower's voluntary leave commit removes its membership and devices but
  retains `invited/<uri>`. The transition is `MEMBER -> INVITED -> MEMBER`,
  using the existing signed leave and join commits and validation rules.
- After leaving, persist and emit an ordinary local conversation request with
  the feed's metadata and a remaining peer as its source. Emit removal before
  the request so existing clients replace the subscribed row with an invitation.
- Keep that request alongside the removed conversation information during
  device synchronization. Ignore stale state; acceptance or decline consumes
  the invitation without duplicate synchronization recreating it.
- If resubscribing before cleanup, reuse the repository and append the join
  after the leave. This preserves the leave for peers that have not fetched it
  yet and prevents delayed removal acknowledgements from deleting the rejoin.
  Once cleanup has completed, use ordinary invitation acceptance and cloning.

## Consequences

- Followers can resubscribe to the same feed and recover its available history
  without an administrator issuing another invitation.
- Invitations remain dismissible through the existing decline action. Leaving
  does not automatically resubscribe or continue fetching feed activity.
- Admin behavior, moderation, non-feed conversations, and departures made before
  this change are unchanged. Bans still deny channel access and joining.
- No new public API, role, repository directory, or serialization field is
  introduced. The local request does not represent a new invitation sent by
  another participant.
- Peers must support the updated feed leave permission. Older peers may reject
  the leave commit; validation is not weakened to accommodate them.
