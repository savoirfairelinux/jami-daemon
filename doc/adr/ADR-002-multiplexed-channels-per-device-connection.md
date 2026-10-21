# ADR-002: Multiplex Peer Traffic over One Connection per Device Pair

**Status:** Accepted
**Origin:** Retrospective
**Decision date:** 2019-10
**Recorded:** 2026-10-21

## Context

A device exchanges many concurrent flows with the same peer device: call
signalling, Git fetches, file transfers, device sync, protocol messages.
Negotiating a separate connection for each costs DHT signalling, ICE
negotiation and a TLS handshake.

## Decision

Each account has a ConnectionManager that keeps one authenticated TLS
connection per peer device (negotiated with ICE, signalled via the DHT)
and multiplexes named channels over it. Channel 0 is the control channel.
Each feature uses its own channel scheme (`sip`, `git://`,
`data-transfer://`, `sync://`, `swarm://`, `msg:`, `auth:`, `ydoc://`, ...)
handled by a channel handler (`src/jamidht/*_channel_handler.*`).

Jami calls use the `sip` channel as their SIP transport
(`ChanneledSIPTransport`) instead of negotiating a dedicated control
socket.

## Consequences

- A channel to an already-connected device costs only a channel request,
  so calls and transfers start quickly.
- Channels share the connection's fate: losing it affects all of them
  (see ADR-011 for calls).
- Channel names are a wire contract. New features add schemes and
  handlers, not new connections.
- Peer devices are authenticated once, at the TLS layer, by certificate.

## Historical evidence

- **Facts:** `eb0fb2bdc` (2019-10) "ConnectionManager: first version":
  channels over a MultiplexedSocket over TLS, channel 0 for control, one
  ConnectionManager per account. `c285b5412` (2020-02) moves calls onto
  the channeled transport; `b5c090ec0` (2020-06) adds file transfers
  over channels.
  The implementation now lives in dhtnet (ADR-006).
- **Documented rationale:** `c285b5412`: "no control socket is negotiated
  anymore and call are basically instantanely in a calling state".
  [Connection manager](https://docs.jami.net/developer/jami-concepts/connection-manager.html):
  a further channel reuses the socket and "only needs 2 TLS packet, so
  it's fast". [DRT](https://docs.jami.net/developer/jami-concepts/drt.html):
  one socket per peer "to optimize the number of sockets".
- **Reconstructed:** none.
