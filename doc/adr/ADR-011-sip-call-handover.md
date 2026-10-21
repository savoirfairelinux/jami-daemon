# ADR-011: Keep Jami Calls Alive across Network Changes

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2026-09
**Recorded:** 2026-10-21

## Context

Jami calls run over a `sip` channel (ADR-002). When that channel was
lost, for instance when a phone switched from Wi-Fi to cellular, the call
ended.

## Decision

Peers advertise support with an `X-Jami-Call-Handover` header in the
INVITE and its answer. When the channel is lost, or the client calls
`libjami::networkInterfaceChanged()`, the call opens a new authenticated
`sip` channel to the same peer device and moves its dialog there. The
caller restarts ICE with a re-INVITE; the callee announces its new
channel with an in-dialog OPTIONS, so re-INVITEs never cross. A dialog
never moves back to a channel it left. The call ends if recovery does not
complete within 20 seconds.

## Consequences

- Clients should report network changes to the daemon.
- Media changes, hold and resume wait for recovery to complete.
- Peers without the header and SIP accounts keep the previous behaviour;
  the header is a compatibility contract.

## Historical evidence

- **Facts:** `039a2c929` (2026-09) "sip: keep calls alive across network
  changes"; `5fb720a59`, `91a048c83` and `fca858ddf` (2026-09) extend it
  to conference calls and harden it. Tests in
  `test/unitTest/call/handover.cpp`.
- **Documented rationale:** `039a2c929` body, summarized above.
- **Reconstructed:** none.
