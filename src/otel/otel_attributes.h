// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
#pragma once

#include <string_view>

// ─────────────────────────────────────────────────────────────────────────────
// Project-specific OpenTelemetry attribute key constants.
//
// All jami.* keys follow the convention:
//   namespace.component.detail  — lowercase, dot-separated, no camelCase
//
// ⚠  PRIVACY RULE: NEVER use actual account IDs, SIP URIs, peer IDs,
//    usernames, phone numbers, or raw IP addresses as attribute VALUES.
//    All user-identifiable data must be hashed (e.g. SHA-256 truncated) or
//    omitted entirely.
// ─────────────────────────────────────────────────────────────────────────────

namespace jami {
namespace otel {
namespace attr {

// ── Account ───────────────────────────────────────────────────────────────────
/// Account protocol family: "sip" | "jami"
constexpr std::string_view ACCOUNT_TYPE       = "jami.account.type";
/// SHA-256 of the account ID, truncated to 16 hex chars (privacy-safe)
constexpr std::string_view ACCOUNT_ID_HASH    = "jami.account.id_hash";
/// Whether the account is currently registered: bool
constexpr std::string_view ACCOUNT_REGISTERED = "jami.account.registered";

// ── Call ─────────────────────────────────────────────────────────────────────
/// SHA-256 of the call ID, truncated to 16 hex chars (privacy-safe)
constexpr std::string_view CALL_ID_HASH  = "jami.call.id_hash";
/// Media content of the call: "audio" | "video"
constexpr std::string_view CALL_TYPE     = "jami.call.type";
/// Who initiated the call: "inbound" | "outbound"
constexpr std::string_view CALL_DIRECTION = "jami.call.direction";
/// Current state machine state (low-cardinality): "ringing" | "connected" | etc.
constexpr std::string_view CALL_STATE    = "jami.call.state";

// ── Media ─────────────────────────────────────────────────────────────────────
/// Media type: "audio" | "video"
constexpr std::string_view MEDIA_TYPE      = "jami.media.type";
/// Codec name (e.g. "opus", "vp8", "h264")
constexpr std::string_view MEDIA_CODEC     = "jami.media.codec";
/// RTP flow direction: "send" | "recv" | "sendrecv"
constexpr std::string_view MEDIA_DIRECTION = "jami.media.direction";
/// Whether the media stream is encrypted (SRTP): bool
constexpr std::string_view MEDIA_SECURE    = "jami.media.secure";

// ── DHT ───────────────────────────────────────────────────────────────────────
/// DHT operation type: "lookup" | "put" | "get" | "announce"
constexpr std::string_view DHT_OPERATION       = "jami.dht.operation";
/// First 16 hex characters of the info-hash (privacy-safe prefix)
constexpr std::string_view DHT_INFO_HASH_PREFIX = "jami.dht.info_hash_prefix";

// ── Network / ICE / Transport ─────────────────────────────────────────────────
/// ICE negotiation outcome: "success" | "failure" | "timeout"
constexpr std::string_view ICE_RESULT         = "jami.ice.result";
/// ICE candidate type that was selected: "host" | "srflx" | "relay"
constexpr std::string_view ICE_CANDIDATE_TYPE = "jami.ice.candidate_type";
/// Transport protocol used for the session: "tls" | "tcp" | "udp" | "dtls"
constexpr std::string_view TRANSPORT_TYPE     = "jami.transport.type";

// ── Standard semantic conventions (error) ────────────────────────────────────
/// Low-cardinality error classification (e.g. "timeout", "rejected", "no_route")
constexpr std::string_view ERROR_TYPE        = "error.type";
/// Human-readable error description attached to an exception event
constexpr std::string_view EXCEPTION_MESSAGE = "exception.message";

// ── Resource attributes (used when creating the OTel Resource) ───────────────
/// The logical name of the service (maps to OTel service.name)
constexpr std::string_view SERVICE_NAME    = "service.name";
/// The version string of the running daemon binary
constexpr std::string_view SERVICE_VERSION = "service.version";
/// The OS process ID of the running daemon
constexpr std::string_view PROCESS_PID     = "process.pid";

} // namespace attr
} // namespace otel
} // namespace jami
