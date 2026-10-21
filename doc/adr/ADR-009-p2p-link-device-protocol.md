# ADR-009: Link New Devices over an Authenticated Peer-to-Peer Channel

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2024-12
**Recorded:** 2026-10-21

## Context

Adding a device previously used `exportOnRing`: the account archive,
encrypted with the password and a generated PIN, was published on the
DHT for the new device to fetch.

## Decision

Device linking is a peer-to-peer exchange. The new device shows a
`jami-auth://` token; the existing device connects to it over an `auth:`
channel (ADR-002); the user confirms the account and device (and gives
the password if set); the archive is sent over that channel. Each side
runs a daemon state machine reported to clients through
`deviceAuthStateChanged` / `addDeviceStateChanged`.

API: `addDevice`, `confirmAddDevice`, `cancelAddDevice` on the exporting
side; `addAccount` with `Account.archiveURL=jami-auth` and
`provideAccountAuthentication` on the importing side. `exportOnRing` is
removed.

## Consequences

- The account archive is no longer published on the DHT.
- Both devices must be online and able to connect at the same time.
- The states and their details are a client-facing contract.

## Historical evidence

- **Facts:** `86d3bb664` (2024-12) "ArchiveAccountManager: add support for
  p2p account import" introduces the APIs above and removes
  `exportOnRing`; `ae3c757fe` (2025-07) follows up.
  [Link new device](https://docs.jami.net/developer/jami-concepts/link-new-device.html)
  documents the state machine and API.
- **Documented rationale:** none found.
- **Reconstructed:** not publishing the encrypted archive on the DHT and
  requiring interactive confirmation are effects of the change; the
  record does not state them as its motivation.
