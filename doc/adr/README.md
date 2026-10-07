# Architecture Decision Records

ADRs record architecturally significant decisions in jami-daemon and why
they were made. They are written for maintainers and coding agents.

[INDEX.md](INDEX.md) is the catalogue and the entry point: read it first,
then open only the ADRs relevant to your work.

## When to write an ADR

Write one when a decision has long-term architectural impact, e.g. it:

- establishes or changes major component boundaries
- affects protocols or externally visible contracts (libjami API, wire
  formats, on-disk/account formats)
- establishes persistence or synchronization models
- changes security or trust boundaries
- establishes networking, media, concurrency, or lifecycle models
- changes cross-platform architecture
- selects a foundational dependency or subsystem strategy
- is expensive or risky to reverse, or involves non-obvious tradeoffs

Do not write one for bug fixes, local implementation details,
straightforward refactors, routine dependency upgrades, style, temporary
workarounds, experiments, or anything already governed by an existing ADR
or repository rule.

Rule of thumb: if a good commit message, code comment, or local doc
preserves the reasoning, no ADR is needed.

## Files and numbering

    doc/adr/ADR-NNN-short-kebab-slug.md

- `NNN` is zero-padded and sequential. Use the next free number.
- Numbers reflect record creation order, not historical decision order.
- One ADR, one decision. No `retro`/`retrospective` in filenames.

## Format

```markdown
# ADR-NNN: Title

**Status:** Proposed | Accepted | Deprecated | Superseded
**Origin:** Contemporary | Retrospective
**Decision date:** YYYY-MM-DD, YYYY-MM, YYYY, or Unknown
**Recorded:** YYYY-MM-DD

## Context

## Decision

## Consequences
```

Optional sections, only when they carry useful content: `## References`,
and for retrospective records `## Historical evidence`. Keep ADRs short
enough to read in a minute or two.

## Status and lifecycle

- **Proposed**: under discussion, not yet binding.
- **Accepted**: governs current work. Merging an ADR accepts it unless its
  status says otherwise.
- **Deprecated**: no longer applies, with no direct replacement.
- **Superseded**: replaced by a later ADR.

Accepted ADRs are historical records. Do not rewrite the decision when the
architecture changes; supersede it:

1. Add a new ADR with `**Supersedes:** [ADR-NNN](...)` under its metadata.
2. Set the old ADR to `Superseded` and add `**Superseded by:**
   [ADR-NNN](...)` under its metadata.
3. Update both rows in INDEX.md.

Typos, broken links, and clarifications that do not change the decision may
be edited in place.

## Contemporary and retrospective ADRs

`Origin` records provenance, not status. A contemporary ADR is written when
the decision is made. A retrospective ADR documents an earlier decision.

Retrospective ADRs must be conservative:

- Base them on evidence: Git history, Gerrit reviews, commit messages,
  docs, issues, tests, source structure, migration code.
- In `## Historical evidence`, cite the evidence and keep three things
  apart: facts shown by the evidence, rationale documented at the time, and
  rationale reconstructed afterwards (mark it as such).
- Never invent rationale or dates. Use the precision the evidence supports
  (`2023-06`, `2023`, `Unknown`).
- If the rationale would be mostly speculation, do not write the ADR.

## Creating or updating an ADR

1. Check INDEX.md for an existing ADR covering the decision.
2. Add the file with the next number and the format above.
3. Add or update its row in INDEX.md. Rows hold only the table columns: no
   summaries or rationale.
4. Prefer committing a contemporary ADR with, or just before, the change
   it describes.
