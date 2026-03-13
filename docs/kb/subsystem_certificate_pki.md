# Certificate PKI

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The certificate PKI subsystem manages the X.509 and OpenDHT cryptographic certificate infrastructure that underpins identity verification, TLS transport security, plugin validation, and SRTP key exchange within libjami. It encompasses certificate validation and trust store management (via `dhtnet::CertificateStore`), local TLS validator utilities, per-account Ed25519/curve25519 keypair management (via OpenDHT crypto), SDES key negotiation for SRTP on SIP calls, and certificate-based plugin signature verification.

---

## Key Files

- `src/connectivity/security/tlsvalidator.h` / `.cpp` — `TlsValidator`: validates a certificate chain, provides detailed status flags
- `src/connectivity/security/memory.h` / `.cpp` — `memory` helpers for secure key material handling (e.g. `SecureArray`)
- `src/sip/sdes_negotiator.h` / `src/sip/sdes_negotiator.cpp` — `SdesNegotiator`: parses `a=crypto` SDP lines, selects SRTP cipher suite
- `src/jami/security_const.h` — string constants for security-related config keys and status values
- External: `<dhtnet/certstore.h>` — `dhtnet::CertificateStore`: trust anchor management, certificate caching, path validation
- External: `<opendht/crypto.h>` — `dht::crypto::Identity`, `dht::crypto::Certificate`, `dht::crypto::PublicKey`: keypair generation, signing, verification
- `src/plugin/store_ca_crt.cpp` — bundled CA certificate for plugin signature verification
- `src/sip/siptransport.h` — `TlsInfos`: per-connection TLS status snapshot

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `TlsValidator` | Inspects a `dht::crypto::Certificate` against configured trust rules; returns per-property enum status | `src/connectivity/security/tlsvalidator.h` |
| `SdesNegotiator` | Parses SDP `a=crypto` attributes, selects a compatible SRTP cipher, derives session keys | `src/sip/sdes_negotiator.h` |
| `TlsInfos` | Captures TLS connection details (cipher, protocol, peer cert, verify flags) at handshake time | `src/sip/siptransport.h` |
| `dhtnet::CertificateStore` | (external) — Stores trusted/pinned certificates; validates chains against trust anchors; owned by `Manager` | `<dhtnet/certstore.h>` |
| `dht::crypto::Identity` | (external) — Keypair (private key + self-signed cert chain); core identity for `JamiAccount` | `<opendht/crypto.h>` |
| `dht::crypto::Certificate` | (external) — X.509 certificate wrapper; used throughout for JAMI identity and TLS | `<opendht/crypto.h>` |

---

## External Dependencies

- **dhtnet** (`dhtnet/certstore.h`, `dhtnet/tls_session.h`) — certificate store, TLS session implementation
- **OpenDHT** (`opendht/crypto.h`) — Ed25519/curve25519 keypair, certificate generation and validation
- **GnuTLS** (underlying dhtnet TLS) — actual TLS protocol and certificate path validation
- **PJSIP TLS** (`pjsip_tpfactory`, `pj_ssl_cipher`) — SIP TLS transport
- **libsrtp** — SRTP cipher suite implementation (SDES-derived keys)

---

## Threading Model

- **Certificate validation** (`TlsValidator::validate()`): called synchronously during TLS handshake negotiation on the PJSIP event thread or dhtnet's connection thread; must complete quickly to avoid stalling handshake.
- **`dhtnet::CertificateStore`**: thread-safe internally; can be queried from any thread.
- **`SdesNegotiator`**: called synchronously during SDP answer construction on the PJSIP event thread.
- **Plugin certificate checks** (`JamiPluginManager::checkPluginCertificateValidity`): called on the io_context thread during plugin install.

---

## Estimated Instrumentation Value

**Medium.** Certificate validation failures are security-relevant and occur at connection establishment (low frequency, high importance). Tracing cert chain validation outcomes, SDES cipher selection, and TLS handshake failures (with error codes) would give visibility into security-related connectivity problems. Routine successes do not need to be traced.

---

## Open Questions

1. Which specific GnuTLS verification flags are enforced by `TlsValidator` — hostname, expiry, revocation (OCSP/CRL)?
2. Is there certificate pinning for known JAMS servers, or is it validated against the system trust store?
3. Is SDES used for all SIP calls, or only for calls where DTLS-SRTP is not negotiated?
4. How is the `dhtnet::CertificateStore` populated — are there pre-loaded trust anchors, or only peer-pinned certificates from previous interactions?
5. What is the key storage mechanism for account identity private keys — filesystem with file permissions, or a system keychain?
