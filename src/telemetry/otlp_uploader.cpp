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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "otlp_uploader.h"

#include "logger.h"
#include "otel_recordable_bridge.h"

#ifdef JAMI_OTEL_EXPORT_ENABLED
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>
#endif

namespace jami::telemetry::detail {

/**
 * @brief OtlpUploader Builds the replay uploader around the shared trace store.
 * @param store Shared ring buffer used as the upload source.
 * @return void
 */
OtlpUploader::OtlpUploader(TraceStore& store)
    : store_(store)
{}

/**
 * @brief ~OtlpUploader Stops the background uploader during destruction.
 * @return void
 */
OtlpUploader::~OtlpUploader()
{
    stop();
}

/**
 * @brief start Creates the OTLP exporter and starts the retry thread when configured.
 * @return void
 */
void
OtlpUploader::start()
{
#ifdef JAMI_OTEL_EXPORT_ENABLED
    if (exporter_)
        return;

    namespace otlp = opentelemetry::exporter::otlp;

    const auto endpoint = resolveOtlpTracesEndpoint();
    if (endpoint.empty()) {
        JAMI_LOG("[otel] OTLP HTTP export compiled in but no endpoint is configured in the environment");
        return;
    }

    otlp::OtlpHttpExporterOptions opts;
    opts.url = endpoint;
    opts.retry_policy_max_attempts = 3;
    opts.retry_policy_initial_backoff = std::chrono::duration<float>(1.0f);
    opts.retry_policy_max_backoff = std::chrono::duration<float>(10.0f);
    opts.retry_policy_backoff_multiplier = 2.0f;

    exporter_ = otlp::OtlpHttpExporterFactory::Create(opts);
    stopUploadThread_ = false;
    lastUploadedSequence_ = 0;
    uploadThread_ = std::thread(&OtlpUploader::uploadThreadMain, this);
    JAMI_LOG("[otel] Buffered OTLP HTTP export enabled -> {}", endpoint);
#endif
}

/**
 * @brief stop Stops the retry thread and flushes any remaining buffered uploads.
 * @return void
 */
void
OtlpUploader::stop()
{
#ifdef JAMI_OTEL_EXPORT_ENABLED
    if (!exporter_ && !uploadThread_.joinable())
        return;

    {
        std::lock_guard lk {uploadMutex_};
        stopUploadThread_ = true;
    }
    uploadCv_.notify_one();
    if (uploadThread_.joinable())
        uploadThread_.join();

    if (exporter_)
        uploadBufferedSpans();
    if (exporter_)
        exporter_->Shutdown();
    exporter_.reset();
    stopUploadThread_ = false;
    lastUploadedSequence_ = 0;
#endif
}

/**
 * @brief notifySpanBuffered Wakes the upload thread after new spans are buffered.
 * @return void
 */
void
OtlpUploader::notifySpanBuffered()
{
#ifdef JAMI_OTEL_EXPORT_ENABLED
    uploadCv_.notify_one();
#endif
}

#ifdef JAMI_OTEL_EXPORT_ENABLED
/**
 * @brief resolveOtlpTracesEndpoint Resolves the OTLP traces endpoint from the environment.
 * @return Endpoint URL, or an empty string when export is not configured.
 */
std::string
OtlpUploader::resolveOtlpTracesEndpoint() const
{
    if (const char* endpoint = std::getenv("OTEL_EXPORTER_OTLP_TRACES_ENDPOINT"); endpoint && endpoint[0] != '\0')
        return endpoint;

    if (const char* endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT"); endpoint && endpoint[0] != '\0') {
        std::string url(endpoint);
        if (url.find("/v1/traces") == std::string::npos)
            url += "/v1/traces";
        return url;
    }

    return {};
}

/**
 * @brief uploadBufferedSpans Replays pending buffered spans to the OTLP exporter.
 * @return true when the current pending batch was uploaded successfully.
 */
bool
OtlpUploader::uploadBufferedSpans()
{
    while (true) {
        auto pending = store_.pendingSpansSince(lastUploadedSequence_, kUploadBatchSize);
        if (pending.empty() || !exporter_)
            return true;

        const auto lastSequence = pending.back().sequence;

        std::vector<std::unique_ptr<trace_sdk::Recordable>> recordables;
        recordables.reserve(pending.size());
        for (const auto& span : pending)
            recordables.emplace_back(std::make_unique<ExportableSpanData>(span));

        auto result = exporter_->Export(recordables);
        if (result != opentelemetry::sdk::common::ExportResult::kSuccess) {
            JAMI_WARNING("[otel] Failed to upload buffered traces, will retry later");
            return false;
        }

        exporter_->ForceFlush();
        lastUploadedSequence_ = std::max(lastUploadedSequence_, lastSequence);
    }
}

/**
 * @brief uploadThreadMain Runs the periodic retry loop for buffered OTLP export.
 * @return void
 */
void
OtlpUploader::uploadThreadMain()
{
    constexpr auto kUploadRetryInterval = std::chrono::minutes(1);

    std::unique_lock lk {uploadMutex_};
    while (!stopUploadThread_) {
        uploadCv_.wait_for(lk, kUploadRetryInterval, [this] { return stopUploadThread_; });
        if (stopUploadThread_)
            break;

        lk.unlock();
        try {
            uploadBufferedSpans();
        } catch (const std::exception& e) {
            JAMI_WARNING("[otel] Upload thread failed: {}", e.what());
        } catch (...) {
            JAMI_WARNING("[otel] Upload thread failed with an unknown error");
        }
        lk.lock();
    }
}
#endif

} // namespace jami::telemetry::detail