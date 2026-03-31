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

#include "trace_types.h"

#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/span_data.h>

#include <chrono>

namespace jami::telemetry::detail {

namespace resource = opentelemetry::sdk::resource;

class ExportableSpanData final : public trace_sdk::Recordable
{
public:
    /**
     * @brief ExportableSpanData Rebuilds an SDK recordable from a stored span snapshot.
     * @param snapshot Span snapshot taken from the daemon ring buffer.
     * @return void
     */
    explicit ExportableSpanData(const SpanSnapshot& snapshot);

    void SetIdentity(const trace_api::SpanContext& spanContext,
                     trace_api::SpanId parentSpanId) noexcept override;
    void SetAttribute(opentelemetry::nostd::string_view key,
                      const opentelemetry::common::AttributeValue& value) noexcept override;
    void AddEvent(opentelemetry::nostd::string_view name,
                  opentelemetry::common::SystemTimestamp timestamp,
                  const opentelemetry::common::KeyValueIterable& attributes) noexcept override;
    void AddLink(const trace_api::SpanContext& spanContext,
                 const opentelemetry::common::KeyValueIterable& attributes) noexcept override;
    void SetStatus(trace_api::StatusCode code,
                   opentelemetry::nostd::string_view description) noexcept override;
    void SetName(opentelemetry::nostd::string_view name) noexcept override;
    void SetSpanKind(trace_api::SpanKind spanKind) noexcept override;
    void SetResource(const resource::Resource& resourceValue) noexcept override;
    void SetStartTime(opentelemetry::common::SystemTimestamp startTime) noexcept override;
    void SetDuration(std::chrono::nanoseconds duration) noexcept override;
    void SetInstrumentationScope(
        const opentelemetry::sdk::instrumentationscope::InstrumentationScope& instrumentationScope) noexcept override;

    explicit operator trace_sdk::SpanData*() const override;

private:
    trace_sdk::SpanData spanData_;
    resource::Resource resource_ = resource::Resource::GetEmpty();
    opentelemetry::nostd::unique_ptr<opentelemetry::sdk::instrumentationscope::InstrumentationScope>
        instrumentationScope_;
};

} // namespace jami::telemetry::detail