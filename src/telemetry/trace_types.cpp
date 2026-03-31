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
#include "trace_types.h"

namespace jami::telemetry::detail {

/**
 * @brief copyAttributes Copies SDK-owned attributes into the snapshot map.
 * @param attributes Attributes read from SDK span data.
 * @return Stable attribute map stored in daemon-owned memory.
 */
OwnedAttributes
copyAttributes(const std::unordered_map<std::string, sdk_common::OwnedAttributeValue>& attributes)
{
    return OwnedAttributes(attributes.begin(), attributes.end());
}

/**
 * @brief ownedAttributeToJson Converts one stored attribute value to JSON.
 * @param value Stored attribute value.
 * @return JSON value for local trace export.
 */
Json::Value
ownedAttributeToJson(const sdk_common::OwnedAttributeValue& value)
{
    return opentelemetry::nostd::visit(Overloaded {
        [](bool item) { return Json::Value(item); },
        [](std::int32_t item) { return Json::Value(item); },
        [](std::uint32_t item) { return Json::Value(item); },
        [](std::int64_t item) { return Json::Value(Json::Int64(item)); },
        [](double item) { return Json::Value(item); },
        [](const std::string& item) { return Json::Value(item); },
        [](const std::vector<bool>& item) {
            Json::Value array(Json::arrayValue);
            for (bool value : item)
                array.append(value);
            return array;
        },
        [](const std::vector<std::int32_t>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(value);
            return array;
        },
        [](const std::vector<std::uint32_t>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(value);
            return array;
        },
        [](const std::vector<std::int64_t>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(Json::Int64(value));
            return array;
        },
        [](const std::vector<double>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(value);
            return array;
        },
        [](const std::vector<std::string>& item) {
            Json::Value array(Json::arrayValue);
            for (const auto& value : item)
                array.append(value);
            return array;
        },
        [](std::uint64_t item) { return Json::Value(Json::UInt64(item)); },
        [](const std::vector<std::uint64_t>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(Json::UInt64(value));
            return array;
        },
        [](const std::vector<std::uint8_t>& item) {
            Json::Value array(Json::arrayValue);
            for (auto value : item)
                array.append(value);
            return array;
        },
    }, value);
}

/**
 * @brief attributesToJson Converts a stored attribute map to a JSON object.
 * @param attributes Stored attribute map.
 * @return JSON object containing the attributes.
 */
Json::Value
attributesToJson(const OwnedAttributes& attributes)
{
    Json::Value json(Json::objectValue);
    for (const auto& [key, value] : attributes)
        json[key] = ownedAttributeToJson(value);
    return json;
}

/**
 * @brief spanToJson Serializes one buffered span snapshot to JSON.
 * @param span Span snapshot from the ring buffer.
 * @return JSON object representing the span.
 */
Json::Value
spanToJson(const SpanSnapshot& span)
{
    Json::Value json(Json::objectValue);
    json["sequence"] = Json::UInt64(span.sequence);
    json["trace_id"] = toHex(span.traceId);
    json["span_id"] = toHex(span.spanId);
    json["parent_span_id"] = toHex(span.parentSpanId);
    json["trace_flags"] = span.traceFlags;
    json["is_remote"] = span.isRemote;
    json["name"] = span.name;
    json["kind"] = static_cast<int>(span.kind);
    json["status"] = static_cast<int>(span.status);
    json["status_description"] = span.statusDescription;
    json["start_time_unix_nanos"] = Json::Int64(span.startTimeNs);
    json["duration_nanos"] = Json::Int64(span.durationNs);
    json["resource_schema_url"] = span.resourceSchemaUrl;
    json["resource_attributes"] = attributesToJson(span.resourceAttributes);
    json["instrumentation_scope"]["name"] = span.instrumentationScope.name;
    json["instrumentation_scope"]["version"] = span.instrumentationScope.version;
    json["instrumentation_scope"]["schema_url"] = span.instrumentationScope.schemaUrl;
    json["instrumentation_scope"]["attributes"] = attributesToJson(span.instrumentationScope.attributes);
    json["attributes"] = attributesToJson(span.attributes);

    Json::Value events(Json::arrayValue);
    for (const auto& event : span.events) {
        Json::Value eventJson(Json::objectValue);
        eventJson["name"] = event.name;
        eventJson["timestamp_unix_nanos"] = Json::Int64(event.timestampNs);
        eventJson["attributes"] = attributesToJson(event.attributes);
        events.append(eventJson);
    }
    json["events"] = std::move(events);

    Json::Value links(Json::arrayValue);
    for (const auto& link : span.links) {
        Json::Value linkJson(Json::objectValue);
        linkJson["trace_id"] = toHex(link.traceId);
        linkJson["span_id"] = toHex(link.spanId);
        linkJson["trace_flags"] = link.traceFlags;
        linkJson["is_remote"] = link.isRemote;
        linkJson["attributes"] = attributesToJson(link.attributes);
        links.append(linkJson);
    }
    json["links"] = std::move(links);
    return json;
}

/**
 * @brief measureSpanSize Measures the JSON payload size of a snapshot.
 * @param span Span snapshot to measure.
 * @return Serialized JSON size in bytes.
 */
std::size_t
measureSpanSize(const SpanSnapshot& span)
{
    return jami::json::toString(spanToJson(span)).size();
}

/**
 * @brief snapshotSpan Copies completed SDK span data into the daemon snapshot model.
 * @param span Completed SDK span data.
 * @param sequence Sequence value assigned by the store.
 * @return Stable snapshot ready for buffering and export.
 */
SpanSnapshot
snapshotSpan(const trace_sdk::SpanData& span, std::uint64_t sequence)
{
    SpanSnapshot snapshot;
    snapshot.sequence = sequence;
    snapshot.traceId = copyIdBytes<trace_api::TraceId::kSize>(span.GetTraceId());
    snapshot.spanId = copyIdBytes<trace_api::SpanId::kSize>(span.GetSpanId());
    snapshot.parentSpanId = copyIdBytes<trace_api::SpanId::kSize>(span.GetParentSpanId());
    snapshot.traceFlags = span.GetFlags().flags();
    snapshot.isRemote = span.GetSpanContext().IsRemote();
    snapshot.name = std::string(span.GetName());
    snapshot.kind = span.GetSpanKind();
    snapshot.status = span.GetStatus();
    snapshot.statusDescription = std::string(span.GetDescription());
    snapshot.startTimeNs = span.GetStartTime().time_since_epoch().count();
    snapshot.durationNs = span.GetDuration().count();
    snapshot.resourceAttributes = copyAttributes(span.GetResource().GetAttributes());
    snapshot.resourceSchemaUrl = span.GetResource().GetSchemaURL();
    snapshot.instrumentationScope.name = span.GetInstrumentationScope().GetName();
    snapshot.instrumentationScope.version = span.GetInstrumentationScope().GetVersion();
    snapshot.instrumentationScope.schemaUrl = span.GetInstrumentationScope().GetSchemaURL();
    snapshot.instrumentationScope.attributes = copyAttributes(span.GetInstrumentationScope().GetAttributes());
    snapshot.attributes = copyAttributes(span.GetAttributes());

    snapshot.events.reserve(span.GetEvents().size());
    for (const auto& event : span.GetEvents()) {
        snapshot.events.push_back({event.GetName(),
                                   event.GetTimestamp().time_since_epoch().count(),
                                   copyAttributes(event.GetAttributes())});
    }

    snapshot.links.reserve(span.GetLinks().size());
    for (const auto& link : span.GetLinks()) {
        snapshot.links.push_back({copyIdBytes<trace_api::TraceId::kSize>(link.GetSpanContext().trace_id()),
                                  copyIdBytes<trace_api::SpanId::kSize>(link.GetSpanContext().span_id()),
                                  link.GetSpanContext().trace_flags().flags(),
                                  link.GetSpanContext().IsRemote(),
                                  copyAttributes(link.GetAttributes())});
    }

    snapshot.serializedSize = measureSpanSize(snapshot);
    return snapshot;
}

} // namespace jami::telemetry::detail