---
name: adr
description: Consult, propose, review, or write Architecture Decision Records (ADRs) in doc/adr/
---

# Architecture Decision Records (ADRs)

ADRs record architecturally significant decisions and their rationale, in
the style of [Michael Nygard](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions).

## Directory structure

```
doc/adr/
├── INDEX.md                                       # Catalogue (read this first)
├── ADR-000-adopt-architecture-decision-records.md
└── ADR-NNN-short-slug.md
```

## Workflow

### Before doing anything

1. Read `doc/adr/INDEX.md`.
2. Open only ADRs whose title or status is relevant to the task. Never
   read or scan all of `doc/adr/`; search INDEX.md instead.
3. Treat Accepted ADRs as constraints. If the work conflicts with one,
   tell the user.

### Deciding whether an ADR is needed

Write one when the decision has long-term architectural impact, e.g. it:

- sets or changes major component boundaries
- affects protocols or external contracts (libjami API, wire formats,
  account or conversation storage formats)
- sets persistence, synchronization, networking, media, concurrency, or
  lifecycle models
- changes security or trust boundaries, or cross-platform architecture
- selects a foundational dependency or subsystem strategy
- is costly to reverse or carries non-obvious tradeoffs

Not for bug fixes, local implementation details, refactors, routine
dependency upgrades, style, temporary workarounds, experiments, or matters
already covered by an ADR or project rule. If a commit message or code
comment preserves the reasoning, skip the ADR. Unsure: ask the user.

### Creating an ADR

1. Check INDEX.md that no existing ADR covers the decision.
2. Take the next free number from INDEX.md.
3. Create the file from the template below.
4. Add its row to INDEX.md.
5. Commit it with, or just before, the change it describes.

Merging an ADR accepts it unless its status says otherwise.

### Updating an ADR

Fix typos, links, or clarifications in place. Do not change the decision of
an Accepted ADR; supersede it. Update the INDEX.md row if the title or
status changes.

### Superseding an ADR

1. Create a new ADR with `**Supersedes:** [ADR-NNN](ADR-NNN-slug.md)` under
   its metadata.
2. In the old ADR, set `**Status:** Superseded` and add
   `**Superseded by:** [ADR-NNN](ADR-NNN-slug.md)`.
3. Update both rows in INDEX.md.

Use `Deprecated` when a decision no longer applies and nothing replaces it.

### Retrospective ADRs

For decisions made before they were recorded (`**Origin:** Retrospective`):

- Reconstruct only from evidence: Git history, Gerrit reviews, commit
  messages, docs, issues, tests, source structure, migration code.
- Add a `## Historical evidence` section citing it, and keep apart:
  facts shown by evidence, rationale documented at the time, and rationale
  reconstructed afterwards (label it as such).
- Never invent rationale or dates. Use the precision available: `2023-06`,
  `2023`, or `Unknown`.
- If the rationale would be mostly speculation, do not write the ADR.

## File naming

```
ADR-NNN-short-kebab-slug.md
```

- `NNN`: zero-padded, sequential in record creation order, not decision
  date order.
- Slug: short summary of the decision. No `retro`/`retrospective`.
- One decision per ADR.

## ADR template

```markdown
# ADR-NNN: Title

**Status:** Proposed | Accepted | Deprecated | Superseded
**Origin:** Contemporary | Retrospective
**Decision date:** YYYY-MM-DD | YYYY-MM | YYYY | Unknown
**Recorded:** YYYY-MM-DD

## Context

What forces or problem motivate this decision?

## Decision

What was decided?

## Consequences

What becomes easier or harder as a result?
```

Optional, only when useful: `## References`, and `## Historical evidence`
for retrospective ADRs. Keep the record short.

## Index format

INDEX.md holds one row per ADR, without summaries or rationale:

```markdown
| ADR | Decision | Status | Origin | Decision date |
| --- | --- | --- | --- | --- |
| [ADR-NNN](ADR-NNN-slug.md) | Title | Accepted | Contemporary | 2026-10-07 |
```
