# OpenTelemetry Semantic Conventions — Relevant to jami-daemon

| Field        | Value                                       |
|--------------|---------------------------------------------|
| Status       | draft                                       |
| Last Updated | 2026-03-13                                  |
| Semconv Ver  | 1.40.0 (latest as of early 2026)            |

---

## 1. Span Naming Convention

**Format**: `<verb>.<noun>` or `<namespace>.<operation>`, all **lowercase**, dot-separated.

| ✅ Good | ❌ Bad |
|---------|-------|
| `call.setup` | `CallSetup`, `call setup`, `CALL_SETUP` |
| `dht.lookup` | `DHTlookup`, `lookup_dht` |
| `sip.invite.send` | `SendSIPInvite`, `SIP INVITE` |
| `ice.negotiation` | `ICE-Negotiation` |
| `account.registration` | `RegisterAccount` |
| `message.send` | `sendMessage` |
| `tls.handshake` | `TLSHandshake` |
| `media.rtp.send` | `rtpSend` |

**OTel spec rule**: Span names should be of **low cardinality**. Do NOT include dynamic values (IDs, addresses) in span names — put them in attributes instead.

```
✅ span name:  "call.setup"
   attribute:  "jami.call.id" = "abc123-hashed"

❌ span name:  "call.setup/abc123xyz"   ← dynamic value in name, bad!
```

---

## 2. Attribute Naming Convention

**Format**: `namespace.component.detail` — all **lowercase**, dot-separated, no hyphens, no underscores in standard conventions (underscores allowed in custom namespaces for readability).

| Rule | Example |
|------|---------|
| Standard semconv attributes use dots | `rpc.system`, `net.transport`, `error.type` |
| Custom jami attributes use `jami.` prefix | `jami.account.type`, `jami.call.direction` |
| Values are lowercase strings where enumerable | `"ring"`, `"sip"`, `"outgoing"` |
| Numbers are `int64_t` or `double`, not strings | `jami.packet.size` = `int64_t(1400)` |

---

## 3. RPC Spans (Standard Semconv)

Schema: https://opentelemetry.io/docs/specs/semconv/rpc/rpc-spans/

| Attribute | Type | Required | Example | Notes |
|-----------|------|----------|---------|-------|
| `rpc.system` | string | Required | `"jami"`, `"grpc"`, `"sip"` | Custom value for jami's native protocol |
| `rpc.method` | string | If known | `"call.setup"`, `"register"` | Low-cardinality method name |
| `rpc.service` | string | If known | `"jami.CallService"` | Service/interface name |
| `server.address` | string | If outbound | `"stun.jami.net"` | DNS name or IP, NOT dynamic peer ID |
| `server.port` | int | If known | `5060`, `4317` | |
| `error.type` | string | On failure | `"timeout"`, `"rejected"`, `"no_route"` | Low-cardinality error classification |
| `rpc.response.status_code` | string | If available | `"OK"`, `"DEADLINE_EXCEEDED"` | |

### jami-specific RPC span example

```cpp
auto span = tracer->StartSpan("sip.invite.send");
span->SetAttribute("rpc.system",  std::string("sip"));
span->SetAttribute("rpc.method",  std::string("INVITE"));
span->SetAttribute("rpc.service", std::string("jami.SipService"));
span->SetAttribute("server.port", static_cast<int64_t>(5060));
// ── DO NOT ── set peer user URI as server.address (unbounded + PII)
```

---

## 4. Network Attributes (Standard Semconv)

Schema: https://opentelemetry.io/docs/specs/semconv/general/attributes/

| Attribute | Type | Example | Notes |
|-----------|------|---------|-------|
| `net.transport` | string | `"ip_tcp"`, `"ip_udp"`, `"ip_tls"`, `"ip_dtls"` | Transport protocol |
| `net.peer.ip` | string | `"203.0.113.1"` | **Use only in traces; never as metric label** |
| `net.peer.port` | int | `5060` | Fine in traces only |
| `net.host.ip` | string | `"10.0.0.1"` | Local bind address; traces only |
| `network.transport` | string | `"tcp"`, `"udp"` | Newer semconv name (stable) |
| `network.peer.address` | string | `"203.0.113.1"` | Newer semconv name (stable) |
| `network.peer.port` | int | `5060` | |

> **Privacy rule**: `net.peer.ip` / `network.peer.address` MUST be omitted or hashed in metrics. In traces, include only when necessary for debugging and under a sampling policy.

---

## 5. Messaging Attributes (Standard Semconv)

Schema: https://opentelemetry.io/docs/specs/semconv/messaging/

Applicable to jami's swarm messaging and IM delivery subsystems.

| Attribute | Type | Example | Notes |
|-----------|------|---------|-------|
| `messaging.system` | string | `"jami"` | Custom value for jami swarm |
| `messaging.destination.name` | string | `"swarm"`, `"dm"` | Type of destination; NOT the actual conversation ID |
| `messaging.operation.type` | string | `"send"`, `"receive"`, `"process"` | |
| `messaging.message.id` | string | hashed | **Must be hashed** — original IDs are PII-adjacent |
| `messaging.client.id` | string | hashed | Sender client identifier |

```cpp
// Producer span for sending a text message:
trace_api::StartSpanOptions opts;
opts.kind = trace_api::SpanKind::kProducer;
auto span = tracer->StartSpan("message.send", {}, opts);
span->SetAttribute("messaging.system",           std::string("jami"));
span->SetAttribute("messaging.destination.name", std::string("swarm"));
span->SetAttribute("messaging.operation.type",   std::string("send"));
```

---

## 6. Error Attributes (Standard Semconv)

Schema: https://opentelemetry.io/docs/specs/semconv/general/recording-errors/

| Attribute | Type | Example | Notes |
|-----------|------|---------|-------|
| `error.type` | string | `"timeout"`, `"rejected"`, `"ice_failure"`, `"_OTHER"` | **Must be low cardinality** |
| `exception.type` | string | `"std::runtime_error"` | Exception class name |
| `exception.message` | string | `"ICE negotiation failed after 30s"` | Human-readable; scrub PII |
| `exception.stacktrace` | string | Full stack | Debug traces only; high volume |

```cpp
// On error:
span->SetStatus(trace_api::StatusCode::kError, "ICE negotiation failed");
span->SetAttribute("error.type",         std::string("ice_failure"));
span->SetAttribute("exception.message",  std::string("No candidates exchanged within timeout"));
// Do NOT include peer address in exception.message
```

### Recommended `error.type` vocabulary for jami-daemon

| Value | Meaning |
|-------|---------|
| `"timeout"` | Operation exceeded time limit |
| `"rejected"` | Remote explicitly rejected |
| `"unreachable"` | No route to peer |
| `"ice_failure"` | ICE negotiation failed |
| `"tls_failure"` | TLS handshake failed |
| `"auth_failure"` | Authentication / SRTP keying failed |
| `"codec_error"` | Codec negotiation failure |
| `"dht_not_found"` | DHT lookup returned no result |
| `"account_disabled"` | Account not in registered state |
| `"_OTHER"` | Catch-all |

---

## 7. Process & Resource Attributes (Standard Semconv)

Schema: https://opentelemetry.io/docs/specs/semconv/resource/

These go in the `Resource`, not in spans:

| Attribute | Type | Example |
|-----------|------|---------|
| `service.name` | string | `"jami-daemon"` |
| `service.version` | string | `"13.0.0"` |
| `service.instance.id` | string | hostname or UUID (NOT user ID) |
| `process.pid` | int | `12345` |
| `process.executable.name` | string | `"jamid"` |
| `os.type` | string | `"linux"`, `"android"`, `"darwin"` |
| `host.arch` | string | `"amd64"`, `"arm64"` |

---

## 8. jami-Specific Attribute Namespace (`jami.*`)

All attributes specific to jami that have no standard semconv equivalent use the `jami.` prefix.

### Convention

```
jami.<subsystem>.<detail>
```

### Defined custom attributes

| Attribute | Type | Values / Notes |
|-----------|------|----------------|
| `jami.account.type` | string | `"ring"` \| `"sip"` | Low cardinality |
| `jami.call.direction` | string | `"incoming"` \| `"outgoing"` |
| `jami.call.media.type` | string | `"audio"` \| `"video"` \| `"av"` \| `"none"` |
| `jami.call.hold` | bool | Whether call is on hold |
| `jami.ice.component` | int | ICE component index (1=RTP, 2=RTCP) |
| `jami.ice.candidate.type` | string | `"host"` \| `"srflx"` \| `"relay"` |
| `jami.dht.key_prefix` | string | First 4 hex chars of DHT key (NOT full key) |
| `jami.dht.op` | string | `"get"` \| `"put"` \| `"announce"` |
| `jami.rtp.payload_type` | int | RTP payload type number |
| `jami.sip.response_code` | int | SIP response code (100–699) |
| `jami.sip.method` | string | `"INVITE"` \| `"BYE"` \| `"REGISTER"` \| `"OPTIONS"` |

---

## 9. Privacy Rules — NEVER Use as Attribute Values

The following data **must never** appear as span or metric attribute values:

| Forbidden Data | Why | Safe Alternative |
|----------------|-----|-----------------|
| Full peer username / URI | PII — identifies user | Omit, or SHA-256 hash → first 8 hex chars |
| Phone number (`tel:+33...`) | PII | Omit |
| Account ID / ring ID | Can identify user | Use account.type only in metrics |
| IP address in metrics | High cardinality + potentially PII | Use in traces only, with sampling |
| Device name / hostname | PII on some platforms | service.instance.id (UUID is fine) |
| Message body content | Confidential communications | Never log |
| Contact display name | PII | Never log |
| Certificate DN / fingerprint | Can identify user | Omit |
| Call ID (`Call-ID:` SIP header) | Links calls to users | Hash to 8 chars if needed |

### Hashing helper

```cpp
// Produce a privacy-safe short ID for use in attributes
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

std::string SafeId(const std::string& sensitive_value) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(sensitive_value.c_str()),
           sensitive_value.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < 4; ++i)  // 8 hex chars = 32-bit prefix
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}
```

---

## 10. Cardinality Traps — What NOT to Use as Metric Labels

| Attribute | Problem | Fix |
|-----------|---------|-----|
| Per-call ID | Unique per call → millions of label combinations | Remove from metrics; use in traces |
| Peer URI / IP address | Unbounded (one per peer) | Remove from metrics |
| Dynamic hostname | Unbounded | Use fixed service.instance.id |
| Timestamp/date | Always unique | Never use |
| Exact SIP `Contact:` header | Unbounded | Use `jami.sip.method` (low cardinality) |
| DHT full key | 256-bit → effectively unbounded | Use `jami.dht.op` + omit key |
| Exact error message string | High cardinality | Use `error.type` from vocabulary above |

**Rule**: If a label can take more than ~20 distinct values across your deployment, it is likely too high cardinality for a metric attribute.

---

## Source References

- [OTel Semantic Conventions 1.40.0](https://opentelemetry.io/docs/specs/semconv/)
- [Semconv: RPC Spans](https://opentelemetry.io/docs/specs/semconv/rpc/rpc-spans/)
- [Semconv: General Attributes](https://opentelemetry.io/docs/specs/semconv/general/attributes/)
- [Semconv: Messaging](https://opentelemetry.io/docs/specs/semconv/messaging/)
- [Semconv: Exceptions](https://opentelemetry.io/docs/specs/semconv/exceptions/)
- [Semconv: Resource](https://opentelemetry.io/docs/specs/semconv/resource/)
- [Semconv: Recording Errors](https://opentelemetry.io/docs/specs/semconv/general/recording-errors/)
- [OTel Span naming guidelines](https://opentelemetry.io/docs/specs/otel/trace/api/#span)

---

## Open Questions

1. **`rpc.system` value**: What string should identify Jami's native P2P protocol? `"jami"` is reasonable, but `"sip"` applies for SIP operations and `"dht"` for DHT. Should there be separate `rpc.system` values per subsystem?
2. **DHT key hashing**: The 8-char prefix of the SHA-256 of a DHT key is likely still enough to correlate debug traces internally. Is there a policy requirement to omit even hashed DHT keys from telemetry?
3. **swarm conversation ID**: Conversation IDs in jami's swarm are semi-public (shared with all participants). Can they be included in traces (hashed) or must they be omitted entirely?
4. **SIP response code cardinality**: There are ~70 standard SIP response codes. Is `jami.sip.response_code` as an integer acceptable for metrics, or should it be bucketed (e.g., `"1xx"`, `"2xx"`, `"4xx"`, `"5xx"`)?
5. **Semconv for P2P protocols**: OTel semconv covers HTTP, gRPC, messaging. Is there published guidance for P2P overlay protocols (DHT, ICE)? If not, should the team publish a JAMI-specific semconv extension?
