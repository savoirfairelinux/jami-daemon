# ADR-000: Adopt Architecture Decision Records

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-10-07
**Recorded:** 2026-10-07

## Context

jami-daemon has years of architectural history: peer-to-peer connectivity,
swarm conversations, account and identity models, media pipelines, and a
contrib-based dependency strategy. The reasoning behind these choices is
scattered across commits, Gerrit reviews, and maintainers' memory. Humans
and coding agents changing the code often cannot tell which constraints are
deliberate.

## Decision

Record architecturally significant decisions as Architecture Decision
Records (ADRs), in lightweight Nygard style.

- ADRs live in `doc/adr/` as `ADR-NNN-short-slug.md`, numbered sequentially
  in creation order.
- `doc/adr/INDEX.md` is the authoritative catalogue and the primary
  discovery mechanism. Readers consult it and open only relevant ADRs.
- Each ADR carries Status (Proposed, Accepted, Deprecated, Superseded),
  Origin (Contemporary or Retrospective), Decision date, and Recorded date.
- Merging an ADR accepts it unless its status says otherwise.
- Accepted ADRs are not rewritten when the decision changes; a new ADR
  supersedes them, and both are linked.
- Earlier decisions may be recorded retrospectively, only from historical
  evidence, keeping documented and reconstructed rationale distinct and
  never inventing rationale or dates.
- ADRs stay concise, one decision each, and are reserved for decisions with
  meaningful long-term architectural impact.

Operational rules live in `doc/adr/README.md`; the agent workflow lives in
`.agents/skills/adr/SKILL.md`.

## Consequences

- Rationale for major decisions becomes discoverable without archaeology.
- Agents can check architectural constraints by reading one compact index.
- Maintainers must keep INDEX.md current and judge what merits an ADR;
  overuse would dilute the corpus and waste reader attention.
- Retrospective ADRs require care to avoid recording speculation as fact.
