# ADR-004: Host Swarm Calls as Rendezvous Conferences

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2021-07
**Recorded:** 2026-10-21

## Context

Group conversations need multi-party calls without a server, compatible
with the existing SIP call and conference stack, and tolerant of several
members starting a call at once.

## Decision

A swarm call is a conference hosted by one device (a rendezvous).

- The host commits an announcement when the call starts (host account,
  device, conference ID) and another when it ends (with its duration).
  Active calls are derived from these commits.
- Members join by calling `rdv:accountUri/deviceId/conversationId/confId`;
  the host answers authorized callers and mixes them into the conference.
- Calling `swarm:<conversationId>` uses the swarm's default host
  (`rdvAccount`/`rdvDevice` in its profile) if set, else an active call,
  else hosts a new rendezvous.

## Consequences

- The host mixes all media: quality and capacity depend on that device,
  and the call ends with it.
- Call start/end commits are part of the swarm protocol (ADR-001).
- Concurrent starts can leave several active calls in one swarm.
- Swarm calls are SIP calls, sharing conference and moderation code with
  other conferences.

## Historical evidence

- **Facts:** `896093622` (2021-07) "swarm: add call support". Current
  code: `Uri::Scheme::SWARM`/`RENDEZVOUS` handling in `jamiaccount.cpp`,
  `Conversation::hostConference()` and active-call tracking in
  `conversation.cpp`.
- **Documented rationale:** `896093622` body;
  [Calls in swarm](https://docs.jami.net/developer/jami-concepts/calls-in-swarm.html),
  "Goals": compatibility with the SIP stack, distributed operation,
  concurrent starts, heterogeneous devices.
- **Reconstructed:** none.
