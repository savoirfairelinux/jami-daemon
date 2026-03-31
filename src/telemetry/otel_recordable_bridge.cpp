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
#include "otel_recordable_bridge.h"

#include <opentelemetry/common/key_value_iterable.h>

#include <memory>

namespace jami::telemetry::detail {

namespace common = opentelemetry::common;
namespace instrumentation = opentelemetry::sdk::instrumentationscope;

namespace {

class OwnedAttributeIterable final : public common::KeyValueIterable
{
public:
    explicit OwnedAttributeIterable(const OwnedAttributes& attributes)
        : attributes_(attributes)
    {}

    /**
     * @brief ForEachKeyValue Replays stored attributes through the SDK iterable API.
     * @param callback Visitor called for each stored attribute.
     * @return false when the visitor stops iteration early.
     */
    bool ForEachKeyValue(opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view,
                                                                 common::AttributeValue)> callback) const noexcept override
    {
        for (const auto& [key, value] : attributes_) {
            const bool keepGoing = opentelemetry::nostd::visit(Overloaded {
                [&](bool item) { return callback(key, item); },
                [&](std::int32_t item) { return callback(key, item); },
                [&](std::uint32_t item) { return callback(key, item); },
                [&](std::int64_t item) { return callback(key, item); },
                [&](double item) { return callback(key, item); },
                [&](const std::string& item) { return callback(key, opentelemetry::nostd::string_view(item)); },
                [&](const std::vector<bool>& item) {
                    std::unique_ptr<bool[]> buffer(new bool[item.size()]);
                    for (std::size_t index = 0; index < item.size(); ++index)
                        buffer[index] = item[index];
                    return callback(key, opentelemetry::nostd::span<const bool>(buffer.get(), item.size()));
                },
                [&](const std::vector<std::int32_t>& item) {
                    return callback(key, opentelemetry::nostd::span<const std::int32_t>(item.data(), item.size()));
                },
                [&](const std::vector<std::uint32_t>& item) {
                    return callback(key, opentelemetry::nostd::span<const std::uint32_t>(item.data(), item.size()));
                },
                [&](const std::vector<std::int64_t>& item) {
                    return callback(key, opentelemetry::nostd::span<const std::int64_t>(item.data(), item.size()));
                },
                [&](const std::vector<double>& item) {
                    return callback(key, opentelemetry::nostd::span<const double>(item.data(), item.size()));
                },
                [&](const std::vector<std::string>& item) {
                    std::vector<opentelemetry::nostd::string_view> values;
                    values.reserve(item.size());
                    for (const auto& entry : item)
                        values.emplace_back(entry);
                    return callback(key,
                                    opentelemetry::nostd::span<const opentelemetry::nostd::string_view>(values.data(),
                                                                                                          values.size()));
                },
                [&](std::uint64_t item) { return callback(key, item); },
                [&](const std::vector<std::uint64_t>& item) {
                    return callback(key, opentelemetry::nostd::span<const std::uint64_t>(item.data(), item.size()));
                },
                [&](const std::vector<std::uint8_t>& item) {
                    return callback(key, opentelemetry::nostd::span<const std::uint8_t>(item.data(), item.size()));
                },
            }, value);
            if (!keepGoing)
                return false;
        }
        return true;
    }

    std::size_t size() const noexcept override { return attributes_.size(); }

private:
    const OwnedAttributes& attributes_;
};

/**
 * @brief setAttributes Copies stored attributes back onto an SDK span recordable.
 * @param spanData SDK span recordable being rebuilt for export.
 * @param attributes Stored attributes from the daemon snapshot.
 * @return void
 */
void
setAttributes(trace_sdk::SpanData& spanData, const OwnedAttributes& attributes)
{
    for (const auto& [key, value] : attributes) {
        opentelemetry::nostd::visit(Overloaded {
            [&](bool item) { spanData.SetAttribute(key, item); },
            [&](std::int32_t item) { spanData.SetAttribute(key, item); },
            [&](std::uint32_t item) { spanData.SetAttribute(key, item); },
            [&](std::int64_t item) { spanData.SetAttribute(key, item); },
            [&](double item) { spanData.SetAttribute(key, item); },
            [&](const std::string& item) { spanData.SetAttribute(key, opentelemetry::nostd::string_view(item)); },
            [&](const std::vector<bool>& item) {
                std::unique_ptr<bool[]> buffer(new bool[item.size()]);
                for (std::size_t index = 0; index < item.size(); ++index)
                    buffer[index] = item[index];
                spanData.SetAttribute(key, opentelemetry::nostd::span<const bool>(buffer.get(), item.size()));
            },
            [&](const std::vector<std::int32_t>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const std::int32_t>(item.data(), item.size()));
            },
            [&](const std::vector<std::uint32_t>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const std::uint32_t>(item.data(), item.size()));
            },
            [&](const std::vector<std::int64_t>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const std::int64_t>(item.data(), item.size()));
            },
            [&](const std::vector<double>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const double>(item.data(), item.size()));
            },
            [&](const std::vector<std::string>& item) {
                std::vector<opentelemetry::nostd::string_view> values;
                values.reserve(item.size());
                for (const auto& entry : item)
                    values.emplace_back(entry);
                spanData.SetAttribute(key,
                                      opentelemetry::nostd::span<const opentelemetry::nostd::string_view>(values.data(),
                                                                                                            values.size()));
            },
            [&](std::uint64_t item) { spanData.SetAttribute(key, item); },
            [&](const std::vector<std::uint64_t>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const std::uint64_t>(item.data(), item.size()));
            },
            [&](const std::vector<std::uint8_t>& item) {
                spanData.SetAttribute(key, opentelemetry::nostd::span<const std::uint8_t>(item.data(), item.size()));
            },
        }, value);
    }
}

} // namespace

/**
 * @brief ExportableSpanData Rehydrates one buffered snapshot into SDK span data.
 * @param snapshot Span snapshot taken from the daemon ring buffer.
 * @return void
 */
ExportableSpanData::ExportableSpanData(const SpanSnapshot& snapshot)
{
    trace_sdk::SpanData spanData;
    spanData.SetIdentity(trace_api::SpanContext(trace_api::TraceId(opentelemetry::nostd::span<const std::uint8_t,
                                                                                               trace_api::TraceId::kSize>(
                                                  snapshot.traceId.data(), snapshot.traceId.size())),
                                              trace_api::SpanId(opentelemetry::nostd::span<const std::uint8_t,
                                                                                           trace_api::SpanId::kSize>(
                                                  snapshot.spanId.data(), snapshot.spanId.size())),
                                              trace_api::TraceFlags(snapshot.traceFlags),
                                              snapshot.isRemote),
                         trace_api::SpanId(opentelemetry::nostd::span<const std::uint8_t,
                                                                      trace_api::SpanId::kSize>(
                             snapshot.parentSpanId.data(), snapshot.parentSpanId.size())));
    spanData.SetTraceFlags(trace_api::TraceFlags(snapshot.traceFlags));
    spanData.SetName(snapshot.name);
    spanData.SetSpanKind(snapshot.kind);
    spanData.SetStatus(snapshot.status, snapshot.statusDescription);
    spanData.SetStartTime(common::SystemTimestamp(std::chrono::nanoseconds(snapshot.startTimeNs)));
    spanData.SetDuration(std::chrono::nanoseconds(snapshot.durationNs));

    sdk_common::AttributeMap resourceAttributes;
    for (const auto& [key, value] : snapshot.resourceAttributes)
        resourceAttributes[key] = value;
    resource_ = resource::Resource::Create(resourceAttributes, snapshot.resourceSchemaUrl);
    spanData.SetResource(resource_);

    instrumentation::InstrumentationScopeAttributes scopeAttributes;
    for (const auto& [key, value] : snapshot.instrumentationScope.attributes)
        scopeAttributes[key] = value;
    instrumentationScope_ = instrumentation::InstrumentationScope::Create(snapshot.instrumentationScope.name,
                                                                          snapshot.instrumentationScope.version,
                                                                          snapshot.instrumentationScope.schemaUrl,
                                                                          scopeAttributes);
    spanData.SetInstrumentationScope(*instrumentationScope_);

    setAttributes(spanData, snapshot.attributes);
    for (const auto& event : snapshot.events)
        spanData.AddEvent(event.name,
                          common::SystemTimestamp(std::chrono::nanoseconds(event.timestampNs)),
                          OwnedAttributeIterable(event.attributes));
    for (const auto& link : snapshot.links) {
        spanData.AddLink(trace_api::SpanContext(trace_api::TraceId(opentelemetry::nostd::span<const std::uint8_t,
                                                                                              trace_api::TraceId::kSize>(
                                                 link.traceId.data(), link.traceId.size())),
                                             trace_api::SpanId(opentelemetry::nostd::span<const std::uint8_t,
                                                                                          trace_api::SpanId::kSize>(
                                                 link.spanId.data(), link.spanId.size())),
                                             trace_api::TraceFlags(link.traceFlags),
                                             link.isRemote),
                         OwnedAttributeIterable(link.attributes));
    }

    spanData_ = std::move(spanData);
}

void
ExportableSpanData::SetIdentity(const trace_api::SpanContext& spanContext,
                                trace_api::SpanId parentSpanId) noexcept
{
    spanData_.SetIdentity(spanContext, parentSpanId);
}

void
ExportableSpanData::SetAttribute(opentelemetry::nostd::string_view key,
                                 const common::AttributeValue& value) noexcept
{
    spanData_.SetAttribute(key, value);
}

void
ExportableSpanData::AddEvent(opentelemetry::nostd::string_view name,
                             common::SystemTimestamp timestamp,
                             const common::KeyValueIterable& attributes) noexcept
{
    spanData_.AddEvent(name, timestamp, attributes);
}

void
ExportableSpanData::AddLink(const trace_api::SpanContext& spanContext,
                            const common::KeyValueIterable& attributes) noexcept
{
    spanData_.AddLink(spanContext, attributes);
}

void
ExportableSpanData::SetStatus(trace_api::StatusCode code,
                              opentelemetry::nostd::string_view description) noexcept
{
    spanData_.SetStatus(code, description);
}

void
ExportableSpanData::SetName(opentelemetry::nostd::string_view name) noexcept
{
    spanData_.SetName(name);
}

void
ExportableSpanData::SetSpanKind(trace_api::SpanKind spanKind) noexcept
{
    spanData_.SetSpanKind(spanKind);
}

/**
 * @brief SetResource Stores a durable resource copy before passing it to the SDK span.
 * @param resourceValue Resource metadata carried by the buffered span.
 * @return void
 */
void
ExportableSpanData::SetResource(const resource::Resource& resourceValue) noexcept
{
    resource_ = resourceValue;
    spanData_.SetResource(resource_);
}

void
ExportableSpanData::SetStartTime(common::SystemTimestamp startTime) noexcept
{
    spanData_.SetStartTime(startTime);
}

void
ExportableSpanData::SetDuration(std::chrono::nanoseconds duration) noexcept
{
    spanData_.SetDuration(duration);
}

/**
 * @brief SetInstrumentationScope Stores a durable instrumentation scope copy for export.
 * @param instrumentationScope Scope metadata carried by the buffered span.
 * @return void
 */
void
ExportableSpanData::SetInstrumentationScope(const instrumentation::InstrumentationScope& instrumentationScope) noexcept
{
    instrumentationScope_ = instrumentation::InstrumentationScope::Create(instrumentationScope.GetName(),
                                                                          instrumentationScope.GetVersion(),
                                                                          instrumentationScope.GetSchemaURL(),
                                                                          instrumentationScope.GetAttributes());
    spanData_.SetInstrumentationScope(*instrumentationScope_);
}

ExportableSpanData::operator trace_sdk::SpanData*() const
{
    return const_cast<trace_sdk::SpanData*>(&spanData_);
}

} // namespace jami::telemetry::detail