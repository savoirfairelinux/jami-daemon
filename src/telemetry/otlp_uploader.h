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

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace jami::telemetry::detail {

class OtlpUploader
{
public:
    /**
     * @brief OtlpUploader Builds the optional OTLP replay uploader around the trace store.
     * @param store Shared ring buffer used as the upload source.
     * @return void
     */
    explicit OtlpUploader(TraceStore& store);

    /**
     * @brief ~OtlpUploader Stops the upload worker before destruction.
     * @return void
     */
    ~OtlpUploader();

    /**
     * @brief start Configures the exporter and starts the background upload thread.
     * @return void
     */
    void start();
    /**
     * @brief stop Stops the upload thread and flushes any buffered OTLP work.
     * @return void
     */
    void stop();
    /**
     * @brief notifySpanBuffered Wakes the uploader after new spans are buffered.
     * @return void
     */
    void notifySpanBuffered();

private:
#ifdef JAMI_OTEL_EXPORT_ENABLED
    /**
     * @brief resolveOtlpTracesEndpoint Resolves the OTLP traces endpoint from the environment.
     * @return Endpoint URL, or an empty string when export is not configured.
     */
    std::string resolveOtlpTracesEndpoint() const;
    /**
     * @brief uploadBufferedSpans Replays pending buffered spans to the OTLP exporter.
     * @return true when the current pending batch was uploaded successfully.
     */
    bool uploadBufferedSpans();
    /**
     * @brief uploadThreadMain Runs the periodic OTLP retry loop.
     * @return void
     */
    void uploadThreadMain();

    static constexpr std::size_t kUploadBatchSize = 256;
#endif

    TraceStore& store_;

#ifdef JAMI_OTEL_EXPORT_ENABLED
    std::unique_ptr<trace_sdk::SpanExporter> exporter_;
    std::thread uploadThread_;
    std::mutex uploadMutex_;
    std::condition_variable uploadCv_;
    bool stopUploadThread_ {false};
    std::uint64_t lastUploadedSequence_ {};
#endif
};

} // namespace jami::telemetry::detail