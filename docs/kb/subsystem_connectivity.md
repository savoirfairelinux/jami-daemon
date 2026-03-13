# Connectivity

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The connectivity subsystem manages all network-level concerns below the SIP/DHT application layer: IP address discovery and NAT detection, SIP transport lifecycle (UDP, TCP, TLS factory and per-call transport objects), ICE candidate gathering and connectivity checks (delegated to dhtnet), STUN/TURN server interaction, and UPnP/NAT-PMP port mapping. It bridges the gap between the abstract "send bytes to peer" requirement of the call and messaging layers and the concrete IP-level socket operations needed to traverse NAT.

---

## Key Files

- `src/connectivity/ip_utils.h` / `src/connectivity/ip_utils.cpp` — IP address enumeration, interface listing, IPv4/IPv6 helpers
- `src/connectivity/sip_utils.h` / `src/connectivity/sip_utils.cpp` — SIP URI/header parsing utilities
- `src/connectivity/utf8_utils.h` / `.cpp` — UTF-8 string validation (used in SIP header construction)
- `src/connectivity/security/` — TLS certificate validation (see also [certificate_pki](subsystem_certificate_pki.md))
- `src/sip/siptransport.h` / `src/sip/siptransport.cpp` — `SipTransport`, `SipTransportBroker`, `TlsListener`, `TlsInfos`
- `src/sip/sipvoiplink.h` / `src/sip/sipvoiplink.cpp` — `SIPVoIPLink` (PJSIP endpoint, network event thread)
- `src/sip/sdp.h` / `src/sip/sdp.cpp` — ICE candidate embedding in SDP
- `src/jamidht/channeled_transport.h` / `.cpp` — SIP-over-dhtnet channel transport adapter
- `src/jamidht/abstract_sip_transport.h` — `AbstractSipTransport` interface (shared by `SipTransport` and `ChanneledTransport`)

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `SipTransport` | Wraps a `pjsip_transport*`; tracks state via `SipTransportStateCallback`; supports UDP/TCP/TLS | `src/sip/siptransport.h` |
| `SipTransportBroker` | Factory and registry for SIP transport objects; creates TLS listeners, manages transport lifetimes | `src/sip/siptransport.h` |
| `TlsListener` | RAII holder for `pjsip_tpfactory*` (TLS server-side listener); destroys factory on destruct | `src/sip/siptransport.h` |
| `TlsInfos` | Snapshot of TLS session parameters at connection time: cipher, protocol version, peer cert | `src/sip/siptransport.h` |
| `SIPVoIPLink` | PJSIP `pjsip_endpoint` owner; runs PJSIP network event loop on a dedicated thread | `src/sip/sipvoiplink.h` |
| `ChanneledTransport` | `AbstractSipTransport` implementation wrapping a `dhtnet::ChannelSocket`; enables SIP-over-DHT | `src/jamidht/channeled_transport.h` |
| `IceTransport` | (dhtnet) — ICE agent; gathers host/srflx/relay candidates; performs connectivity checks | dhtnet (external) |
| `IceTransportFactory` | (dhtnet) — creates `IceTransport` instances; owned by `Manager` | dhtnet (external) |

---

## External Dependencies

- **PJSIP / PJNATH** (`pjsip.h`, `pjnath/stun_config.h`) — SIP transport, STUN client
- **dhtnet** (`dhtnet/ip_utils.h`, `dhtnet/connectionmanager.h`, ICE implementation) — ICE agent, UPnP mapping, multiplexed channels
- **OpenSSL / GnuTLS** (via dhtnet and PJSIP TLS) — TLS session
- **asio** — timer-based transport keep-alives and reconnection

---

## Threading Model

- **PJSIP network thread** (`SIPVoIPLink::sipThread_`): single dedicated `std::thread`; all PJSIP transport events (connect, disconnect, receive) fire here.
- **dhtnet ICE thread pool**: ICE candidate gathering and STUN/TURN exchanges are async within dhtnet's internal executor; results delivered via callback.
- **UPnP** (`dhtnet::upnp`): asio-based background queries; results posted to the io_context.
- **`SipTransportBroker`**: accessed from both the PJSIP thread (transport callbacks) and ASIO thread (account operations); requires careful locking (uses internal mutex per broker).

---

## Estimated Instrumentation Value

**High.** Network connectivity is the leading cause of call setup failures. Tracing transport creation/destruction events, ICE connectivity check outcomes (success, timeout, TURN fallback), STUN binding responses, UPnP mapping status, and TLS handshake failures would provide actionable data for diagnosing issues in enterprise and home NAT environments.

---

## Open Questions

1. Is there a TURN-only mode that bypasses ICE host/srflx candidates for privacy?
2. How does `SipTransportBroker` handle transport recycling — is a transport per-call or per-account?
3. When TLS verification fails (`pj_ssl_cert_verify_flag_t`), is the call dropped or continued with a warning?
4. Does `ChanneledTransport` support DTLS for media path security, or is SRTP key exchange entirely via SDP/SDES?
5. What is the behavior when both STUN and TURN servers are unreachable — is there a direct fallback?
