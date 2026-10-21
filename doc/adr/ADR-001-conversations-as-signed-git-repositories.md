# ADR-001: Store Conversations as Signed Git Repositories

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2019
**Recorded:** 2026-10-21

## Context

Conversations (swarms) need ordered, synchronized history across devices
and members, with no server, offline divergence and later merging, and
verifiable authorship.

## Decision

Each conversation is a Git repository managed with libgit2.

- The conversation ID is the hash of the initial commit.
- Membership and trust live in the tree: `admins/`, `members/`,
  `devices/`, `invited/`, `banned/`, `votes/`, `CRLs/`.
- Messages are commits whose message is JSON with a `type`; content goes
  in commit messages, not files, to avoid merge conflicts.
- Every commit is signed by the authoring device. Fetched commits are
  validated (signature, device and member certificates, allowed file
  changes per type) before merging; invalid fetches are discarded.
- Divergent histories are reconciled with merge commits.

## Consequences

- Changing the repository layout, commit types or validation rules is a
  protocol change: older peers reject what they cannot validate.
- History cannot be rewritten or deleted; deleting means leaving.
- Repositories grow without bound; pack and fetch costs are inherent.
- Later features (calls, profiles, documents) are built as commit types
  or files within this model.

## Historical evidence

- **Facts:** `c4e912dd8` (2019-10) adds libgit2 as a dependency.
  `6100a792f` (2019-12) creates a conversation as a Git repository holding
  the admin certificate, CRLs and device key, with the first commit hash
  as the conversation ID. `ConversationRepository::createConversation()`
  and the signed-commit code in `conversationrepository.cpp` still do this.
- **Documented rationale:** [Swarm](https://docs.jami.net/developer/jami-concepts/swarm.html),
  "Git / Why this choice": a Merkle tree to order and sync messages,
  distributed by nature, verifiable commits, conflicts avoided by using
  commit messages.
- **Reconstructed:** none. The decision predates its earliest commit; only
  the year is established.
