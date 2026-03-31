/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "json_utils.h"

#include <opentelemetry/sdk/common/attribute_utils.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/span_id.h>
#include <opentelemetry/trace/trace_id.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace jami::telemetry::detail {

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace sdk_common = opentelemetry::sdk::common;

template<typename... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template<typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

using OwnedAttributes = std::map<std::string, sdk_common::OwnedAttributeValue>;

struct EventSnapshot
{
    std::string name;
    std::int64_t timestampNs {};
    OwnedAttributes attributes;
};

struct LinkSnapshot
{
    std::array<std::uint8_t, trace_api::TraceId::kSize> traceId {};
    std::array<std::uint8_t, trace_api::SpanId::kSize> spanId {};
    std::uint8_t traceFlags {};
    bool isRemote {};
    OwnedAttributes attributes;
};

struct InstrumentationScopeSnapshot
{
    std::string name;
    std::string version;
    std::string schemaUrl;
    OwnedAttributes attributes;
};

struct SpanSnapshot
{
    std::uint64_t sequence {};
    std::array<std::uint8_t, trace_api::TraceId::kSize> traceId {};
    std::array<std::uint8_t, trace_api::SpanId::kSize> spanId {};
    std::array<std::uint8_t, trace_api::SpanId::kSize> parentSpanId {};
    std::uint8_t traceFlags {};
    bool isRemote {};
    std::string name;
    trace_api::SpanKind kind {trace_api::SpanKind::kInternal};
    trace_api::StatusCode status {trace_api::StatusCode::kUnset};
    std::string statusDescription;
    std::int64_t startTimeNs {};
    std::int64_t durationNs {};
    OwnedAttributes resourceAttributes;
    std::string resourceSchemaUrl;
    InstrumentationScopeSnapshot instrumentationScope;
    OwnedAttributes attributes;
    std::vector<EventSnapshot> events;
    std::vector<LinkSnapshot> links;
    std::size_t serializedSize {};
};

/**
 * @brief copyAttributes Copies SDK-owned attributes into the daemon snapshot map.
 * @param attributes Attributes taken from SDK span data.
 * @return Stable attribute map stored in the ring buffer.
 */
OwnedAttributes copyAttributes(
    const std::unordered_map<std::string, sdk_common::OwnedAttributeValue>& attributes);

/**
 * @brief copyIdBytes Copies a trace or span identifier into a fixed byte array.
 * @param id OpenTelemetry identifier object.
 * @return Identifier bytes stored in daemon-owned memory.
 */
template<std::size_t Size, typename IdType>
std::array<std::uint8_t, Size>
copyIdBytes(const IdType& id)
{
    std::array<std::uint8_t, Size> bytes {};
    id.CopyBytesTo(opentelemetry::nostd::span<std::uint8_t, Size>(bytes.data(), bytes.size()));
    return bytes;
}

/**
 * @brief toHex Converts raw trace or span identifier bytes to lowercase hex.
 * @param bytes Identifier bytes stored in the snapshot.
 * @return Hex string used by JSON export.
 */
template<std::size_t Size>
std::string toHex(const std::array<std::uint8_t, Size>& bytes)
{
    std::array<char, Size * 2> hex {};
    if constexpr (Size == trace_api::TraceId::kSize) {
        trace_api::TraceId(opentelemetry::nostd::span<const std::uint8_t, Size>(bytes.data(), bytes.size()))
            .ToLowerBase16(opentelemetry::nostd::span<char, Size * 2>(hex.data(), hex.size()));
    } else {
        trace_api::SpanId(opentelemetry::nostd::span<const std::uint8_t, Size>(bytes.data(), bytes.size()))
            .ToLowerBase16(opentelemetry::nostd::span<char, Size * 2>(hex.data(), hex.size()));
    }
    return std::string(hex.data(), hex.size());
}

/**
 * @brief ownedAttributeToJson Converts one stored attribute value to JSON.
 * @param value Stored attribute value.
 * @return JSON representation of the attribute.
 */
Json::Value ownedAttributeToJson(const sdk_common::OwnedAttributeValue& value);
/**
 * @brief attributesToJson Converts a stored attribute map to a JSON object.
 * @param attributes Stored attribute map.
 * @return JSON object containing the attributes.
 */
Json::Value attributesToJson(const OwnedAttributes& attributes);
/**
 * @brief spanToJson Converts a stored span snapshot to JSON.
 * @param span Span snapshot from the ring buffer.
 * @return JSON object ready for local export.
 */
Json::Value spanToJson(const SpanSnapshot& span);
/**
 * @brief measureSpanSize Estimates the serialized size of a stored span.
 * @param span Span snapshot to measure.
 * @return JSON payload size in bytes.
 */
std::size_t measureSpanSize(const SpanSnapshot& span);
/**
 * @brief snapshotSpan Copies SDK span data into the daemon snapshot model.
 * @param span Completed SDK span data.
 * @param sequence Monotonic sequence number assigned by the store.
 * @return Stable span snapshot for buffering and export.
 */
SpanSnapshot snapshotSpan(const trace_sdk::SpanData& span, std::uint64_t sequence);

} // namespace jami::telemetry::detail