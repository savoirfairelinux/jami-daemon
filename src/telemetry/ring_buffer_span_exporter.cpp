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
#include "ring_buffer_span_exporter.h"

namespace jami::telemetry::detail {

/**
 * @brief RingBufferSpanExporter Builds the exporter that snapshots spans into the store.
 * @param store Shared ring buffer used as the telemetry source of truth.
 * @param notifyCallback Callback invoked after spans are buffered.
 * @return void
 */
RingBufferSpanExporter::RingBufferSpanExporter(TraceStore& store, NotifyCallback notifyCallback)
    : store_(store)
    , notifyCallback_(std::move(notifyCallback))
{}

std::unique_ptr<trace_sdk::Recordable>
RingBufferSpanExporter::MakeRecordable() noexcept
{
    return std::make_unique<trace_sdk::SpanData>();
}

/**
 * @brief Export Snapshots the completed SDK spans into the trace store.
 * @param spans Completed recordables provided by the SDK processor.
 * @return Success result for the SDK exporter pipeline.
 */
opentelemetry::sdk::common::ExportResult
RingBufferSpanExporter::Export(
    const opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>& spans) noexcept
{
    bool bufferedSpan = false;
    for (const auto& recordable : spans) {
        auto* spanData = static_cast<trace_sdk::SpanData*>(*recordable);
        if (!spanData)
            continue;

        bufferedSpan = store_.appendCompletedSpan(*spanData) || bufferedSpan;
    }

    if (bufferedSpan && notifyCallback_)
        notifyCallback_();

    return opentelemetry::sdk::common::ExportResult::kSuccess;
}

bool
RingBufferSpanExporter::ForceFlush(std::chrono::microseconds) noexcept
{
    return true;
}

bool
RingBufferSpanExporter::Shutdown(std::chrono::microseconds) noexcept
{
    return true;
}

} // namespace jami::telemetry::detail