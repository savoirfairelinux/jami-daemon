# Architecture Decision Records

Architecturally significant decisions in jami-daemon, with their context
and consequences, following
[Michael Nygard's format](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions).
Open only the records relevant to your task. Authoring rules:
[`.agents/skills/adr/SKILL.md`](../../.agents/skills/adr/SKILL.md).

| ADR | Decision | Status | Origin | Decision date |
| --- | --- | --- | --- | --- |
| [ADR-000](ADR-000-adopt-architecture-decision-records.md) | Adopt Architecture Decision Records | Accepted | Contemporary | 2026-10-07 |
| [ADR-001](ADR-001-conversations-as-signed-git-repositories.md) | Store Conversations as Signed Git Repositories | Accepted | Retrospective | 2019 |
| [ADR-002](ADR-002-multiplexed-channels-per-device-connection.md) | Multiplex Peer Traffic over One Connection per Device Pair | Accepted | Retrospective | 2019-10 |
| [ADR-003](ADR-003-pluggable-audio-processor.md) | Swappable In-Daemon Audio Processing, WebRTC by Default | Accepted | Retrospective | 2021-02 |
| [ADR-004](ADR-004-swarm-calls-as-hosted-rendezvous.md) | Host Swarm Calls as Rendezvous Conferences | Accepted | Retrospective | 2021-07 |
| [ADR-005](ADR-005-retire-dht-instant-messages.md) | Retire DHT Instant Messages in Favour of Swarm | Accepted | Retrospective | 2022-04 |
| [ADR-006](ADR-006-peer-connectivity-in-dhtnet.md) | Move Peer-to-Peer Connectivity into dhtnet | Accepted | Retrospective | 2022-10 |
| [ADR-007](ADR-007-distributed-routing-table-for-swarms.md) | Connect Swarm Members through a Distributed Routing Table | Accepted | Retrospective | 2023-03 |
| [ADR-008](ADR-008-msgpack-message-channel.md) | Exchange Peer Protocol Messages over a Message Channel | Accepted | Retrospective | 2024-11 |
| [ADR-009](ADR-009-p2p-link-device-protocol.md) | Link New Devices over an Authenticated Peer-to-Peer Channel | Accepted | Retrospective | 2024-12 |
| [ADR-010](ADR-010-y-crdt-collaborative-documents.md) | Collaborative Documents as Y-CRDT over Swarm Repositories | Accepted | Retrospective | 2026-07 |
| [ADR-011](ADR-011-sip-call-handover.md) | Keep Jami Calls Alive across Network Changes | Accepted | Retrospective | 2026-09 |
