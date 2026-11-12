# ADR-014: Swarm Admin Roles and a Configurable Approval Threshold

**Status:** Proposed
**Origin:** Contemporary
**Decision date:** 2026-11
**Recorded:** 2026-11-12

## Context

A swarm has exactly one admin, its creator, and nothing can change that.
A group whose creator leaves or loses their account keeps no admin, and a
group that wants to share moderation cannot. Issue #798 asks for promoting
and demoting admins, and its discussion settled that admin decisions are
taken by vote, with more than half of the admins required.

Votes already exist for bans and unbans. An admin's `vote` commit adds an
empty file `votes/<ban|unban>/<type>/<uri>/<adminUri>`, and once enough
admins voted, a `member` commit resolves the vote. With a single admin
every vote resolves at once, so the counting was never exercised with
several admins. The preceding change makes validation count the admins of
the parent commit instead of the working directory and require more than
half, as local resolution already did.

[ADR-012](ADR-012-explicit-swarm-permissions.md) made swarm actions
explicit permissions and names custom roles and admin overrides of default
permissions as future work. One fixed majority does not suit every group
either. A small association may prefer that any admin can act alone, and a
larger one may want two thirds.

## Decision

### Approval threshold

Each swarm stores an approval threshold in the file `policy/approval`, as
one canonical token `percent/P` without a trailing newline, with P from 1
to 100. A vote is approved when more than P percent of the admins voted,
capped at all admins. With n admins the required votes are
`min(n, n * P / 100 + 1)` in integer arithmetic.

A missing file means `percent/50`, the rule of #798, so every existing
swarm keeps its behaviour. Canonical means the token matches
`^percent/([1-9][0-9]?|100)$`, so a vote path and the file content
compare byte for byte.

| Admins | `percent/50` | `percent/66` | `percent/100` | `percent/1` |
| --- | --- | --- | --- | --- |
| 1 | 1 | 1 | 1 | 1 |
| 2 | 2 | 2 | 2 | 1 |
| 3 | 2 | 2 | 3 | 1 |
| 4 | 3 | 3 | 4 | 1 |
| 5 | 3 | 4 | 5 | 1 |

`percent/66` requires the same votes as "at least two thirds" up to 52
admins. `percent/1` lets any one admin act alone below 100 admins.

### One rule for every admin decision

Bans, unbans, promotions, demotions and threshold changes resolve through
one rule. The voters that are admins in the parent commit of the resolution
are counted against the threshold stored in that parent commit. The
electorate is every admin of that commit, the targeted admin included.
Self votes stay rejected for every vote type.

### Promoting and demoting

An admin votes with a `vote` commit that carries an action:

```json
{"type":"vote","uri":"<uri>","action":"promote"}
```

It adds `votes/promote/members/<uri>/<adminUri>`, or
`votes/demote/admins/<uri>/<adminUri>` for a demotion. A `vote` commit
without `action` stays a ban or unban vote. Once the vote is approved, a
`member` commit with `action` set to `promote` or `demote` moves
`members/<uri>.crt` to `admins/<uri>.crt` or back, with an identical blob,
and deletes the vote files. Demoting requires at least two admins in the
parent commit, so a role change never removes the last admin.

Stepping down is unilateral. A `demote` commit whose target is its author
needs no votes, like leaving, which already removes the author's
`admins/<uri>.crt` without a vote.

### Changing the threshold

A threshold change is a vote, approved under the old threshold. Each admin
adds `votes/approval/<kind>/<value>/<adminUri>` with
`{"type":"vote","action":"approval","threshold":"<token>"}`. The resolution
is a new commit type:

```json
{"type":"policy","action":"approval","threshold":"<token>"}
```

It writes `policy/approval` and deletes the votes for that token.

### Permissions

Two permissions extend the table of ADR-012:

| Permission | Description | Associated git commits |
| --- | --- | --- |
| ChangeMemberRole | Promote a member to admin or demote an admin. | `vote` commit with `action` set to `promote` or `demote`; once approved, a `member` commit with the same `action`. |
| ChangeApprovalThreshold | Change the swarm's approval threshold. | `vote` commit with `action` set to `approval` and `threshold`; once approved, a `policy` commit with `action` set to `approval`. |

| | `ONE_TO_ONE` | `INVITES_ONLY` | `DOCUMENT` | `FEED` |
| --- | --- | --- | --- | --- |
| ChangeMemberRole | --- | `ADMIN` | `ADMIN` | `ADMIN` |
| ChangeApprovalThreshold | --- | `ADMIN` | `ADMIN` | `ADMIN` |

### Merges, conflicts and events

A merge conflict on `policy/approval` is resolved like one on
`profile.vcf`, where the side with the higher commit id wins, also when
both sides created the file. Both values were approved on their branch.

If a concurrent ban and promotion meet in a merge, a URI can be in
`admins/` and `banned/` at once. A banned entry wins in permission checks
and in the member list.

`libjami::MemberEvent` gains `PROMOTE = 5` and `DEMOTE = 6`. A threshold
change emits `ConversationProfileUpdated`, and the conversation infos carry
`approvalThreshold`.

## Alternatives

- **A threshold per action.** A lower ban threshold would let admins bypass
  the demotion rule by banning an admin, so one rule covers every decision.
- **`percent/0`.** It would require one vote at every admin count, which
  `percent/1` already gives below 100 admins, and two tokens for one rule
  would split the votes of admins who want the same thing.
- **`count/k`.** A fixed number of admins, capped at all of them.
  `percent/1` and `percent/100` already cover one admin and all admins,
  and a second token kind adds parsing, validation and documentation. It
  can still follow with a staged release like this one, because current
  daemons reject unknown tokens.
- **An uncapped threshold.** A threshold above the admin count would freeze
  bans, unbans and the threshold itself, and because a change is approved
  under the old threshold, nothing could lift the freeze. A sole admin who
  wants no co-admins promotes nobody.
- **Stepping down by vote.** Leaving is already unilateral and removes the
  admin certificate, so a vote would protect nothing.

## Consequences

- Daemons without this change reject promotion, demotion and threshold
  commits, so they stop syncing a swarm after the first one. The validation
  should ship one release before the API that creates these commits.
- With two admins and the default threshold, neither admin can ban the
  other, and a lost admin account blocks promotions. A group that prefers
  recovery sets `percent/1` while both admins are reachable.
- Contradicting resolutions on diverged branches can leave a swarm without
  an admin after a merge. This needs approvals on both branches, which is
  rare with the default threshold and realistic only with `percent/1`.
- A vote stays until a resolution consumes it, as ban votes already do. If
  its target changes role another way, for example an admin steps down and
  is promoted again later, the old demotion votes still count. The same
  holds for votes on a threshold after another one was applied.
- Clients need UI for promoting, demoting, the threshold and pending votes.
  The daemon exposes pending votes through `getConversationVotes`.

## References

- Issue #798: https://git.jami.net/savoirfairelinux/jami-daemon/-/issues/798
- Earlier attempt, change 30126: https://review.jami.net/c/jami-daemon/+/30126
- [ADR-012](ADR-012-explicit-swarm-permissions.md)
