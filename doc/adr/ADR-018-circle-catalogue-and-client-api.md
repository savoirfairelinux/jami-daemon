# ADR-018: Circle Catalogue and Client API

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-11-09
**Recorded:** 2026-11-24

## Context

Circles need invitations, channel subscriptions and account-device sync.
Clients already know how to send, page and display conversation messages.
Requiring a separate channel message API would duplicate that work.

## Decision

Keep an account-local circle catalogue and invitation records under
`<account>/circles`, separate from peer-verifiable repositories. Restore
only recorded mounts on demand rather than scanning directories. Joining
a channel is explicit and clones it from a reader that can serve it.

Send invitations over the message channel. Accepting a request clones
the verified circle and joins it; declining records the dismissal.
An invitation may show an unverified name hint before the clone. Append
catalogue and request fields to the account-device sync array. Devices
clone missing circles and enroll themselves, apply the most recent channel
join or leave, and propagate circle departures and invitation declines.
Older devices ignore the appended fields.

Store shared circle and channel profiles as vCards in the signed circle
tree. Circle profiles support title, description and avatar; channels
support title and description. Profile writes authorize exactly one file,
with `UpdateProfile` for the circle or `ManageChannels` for a channel.
Members can read channel profiles without read access to channel history.
A later write replaces its ancestor; concurrent writes to the same file
select the smallest original commit ID. Different profile files compose;
fields within a card are not merged. Profile writes do not affect member
revisions or pending moderation.

Expose circle and channel management, profiles and lifecycle signals in
`jami/circle_interface.h` and the D-Bus, JNI and Node.js bindings. Treat the
ID of a joined channel as a conversation ID only for message send/load,
cache clearing and permission queries. Use the existing swarm message
signals and display pipeline, including reaction and edit folding.
Channels do not appear in the conversation list, emit `ConversationReady`,
or expose standalone conversation settings.

## Consequences

Circle and channel membership persist across restarts and follow the
account across devices. A circle-aware client can reuse its message view;
older clients do not see channels as ordinary conversations. The message
surface remains limited to [supported channel content](ADR-016-policy-bound-channel-history.md).
