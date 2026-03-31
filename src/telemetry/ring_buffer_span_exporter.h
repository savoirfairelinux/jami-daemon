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

#include "trace_store.h"

#include <opentelemetry/sdk/trace/exporter.h>

#include <functional>

namespace jami::telemetry::detail {

class RingBufferSpanExporter final : public trace_sdk::SpanExporter
{
public:
    using NotifyCallback = std::function<void()>;

    /**
     * @brief RingBufferSpanExporter Builds the exporter that writes spans to the trace store.
     * @param store Shared ring buffer used as the source of truth.
     * @param notifyCallback Callback invoked after spans are buffered.
     * @return void
     */
    explicit RingBufferSpanExporter(TraceStore& store, NotifyCallback notifyCallback = {});

    std::unique_ptr<trace_sdk::Recordable> MakeRecordable() noexcept override;

    /**
     * @brief Export Snapshots completed SDK spans into the ring buffer.
     * @param spans Completed recordables produced by the SDK processor.
     * @return Export success result for the SDK pipeline.
     */
    opentelemetry::sdk::common::ExportResult Export(
        const opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>& spans) noexcept override;

    bool ForceFlush(std::chrono::microseconds timeout) noexcept override;
    bool Shutdown(std::chrono::microseconds timeout) noexcept override;

private:
    TraceStore& store_;
    NotifyCallback notifyCallback_;
};

} // namespace jami::telemetry::detail