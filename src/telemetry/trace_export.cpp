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
#include "trace_export.h"

#include "fileutils.h"
#include "json_utils.h"
#include "logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace jami::telemetry::detail {
namespace {

/**
 * @brief buildTraceFileJson Builds the JSON document written by local trace export.
 * @param serviceName Service name recorded in the export metadata.
 * @param serviceVersion Service version recorded in the export metadata.
 * @param deviceId Local device identifier recorded in the export metadata.
 * @param spans Buffered spans to serialize.
 * @return JSON document for the export file.
 */
Json::Value
buildTraceFileJson(const std::string& serviceName,
                   const std::string& serviceVersion,
                   const std::string& deviceId,
                   const std::vector<SpanSnapshot>& spans)
{
    Json::Value root(Json::objectValue);
    root["format"] = "jami-traces/1";
    root["service_name"] = serviceName;
    root["service_version"] = serviceVersion;
    root["device_id"] = deviceId;
    root["generated_at_unix_nanos"] = Json::Int64(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    root["span_count"] = Json::UInt64(spans.size());

    Json::Value spanValues(Json::arrayValue);
    for (const auto& span : spans)
        spanValues.append(spanToJson(span));
    root["spans"] = std::move(spanValues);
    return root;
}

/**
 * @brief defaultTraceExportPath Chooses the fallback cache path for local exports.
 * @return Default JSON file path under the daemon cache directory.
 */
std::filesystem::path
defaultTraceExportPath()
{
    auto telemetryDir = jami::fileutils::get_cache_dir() / "telemetry";
    std::error_code ec;
    std::filesystem::create_directories(telemetryDir, ec);
    if (ec)
        JAMI_WARNING("[otel] Unable to create telemetry cache dir {}: {}", telemetryDir, ec.message());

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    return telemetryDir / ("traces-" + std::to_string(now.count()) + ".json");
}

} // namespace

std::string
exportBufferedTraces(const std::string& serviceName,
                     const std::string& serviceVersion,
                     const std::string& deviceId,
                     const std::vector<SpanSnapshot>& spans,
                     const std::string& destinationPath)
{
    auto outputPath = destinationPath.empty() ? defaultTraceExportPath() : std::filesystem::path(destinationPath);

    std::error_code ec;
    if (outputPath.has_parent_path())
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        JAMI_WARNING("[otel] Unable to create export directory for {}: {}", outputPath, ec.message());
        return {};
    }

    std::ofstream output(outputPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        JAMI_WARNING("[otel] Unable to create trace export file {}", outputPath);
        return {};
    }

    output << jami::json::toString(buildTraceFileJson(serviceName, serviceVersion, deviceId, spans));
    output.close();
    return outputPath.string();
}

} // namespace jami::telemetry::detail