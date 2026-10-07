---
name: adr
description: Consult, propose, review, or write Architecture Decision Records (ADRs) for architecturally significant decisions
---

# Reading

1. Read `doc/adr/INDEX.md` first.
2. Open only ADRs whose title or status is relevant to the task.
3. Do not scan or preload `doc/adr/`. If the index grows large, search it
   (or filenames) instead of opening speculative ADRs.
4. Accepted ADRs are constraints; flag work that conflicts with one.

# Writing

Read `doc/adr/README.md` before creating, superseding, or changing the
status of an ADR, or when policy details are needed.

- Search the index for an existing ADR first.
- Only write one for a decision with lasting architectural impact. Not for
  bug fixes, local implementation details, refactors, routine upgrades,
  style, temporary workarounds, or matters already governed by an ADR or
  project rule. Unsure: ask the user.
- Use the next sequential number; keep Context, Decision, Consequences short.
- Update `doc/adr/INDEX.md` whenever an ADR is added or its status changes.
- Supersede with a new ADR; never silently rewrite an accepted decision.
- Retrospective ADRs: follow the evidence rules in the README; never invent
  rationale or dates.
