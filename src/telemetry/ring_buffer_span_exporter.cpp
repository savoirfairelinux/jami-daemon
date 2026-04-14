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
#include "logger.h"

#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/sdk/common/attribute_utils.h>
#include <opentelemetry/nostd/variant.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace trace_sdk = opentelemetry::sdk::trace;
using ExportResult  = opentelemetry::sdk::common::ExportResult;

namespace jami {
namespace telemetry {

RingBufferSpanExporter::RingBufferSpanExporter(std::size_t maxSpans)
    : maxSpans_(maxSpans)
{}

std::unique_ptr<trace_sdk::Recordable>
RingBufferSpanExporter::MakeRecordable() noexcept
{
    return std::make_unique<trace_sdk::SpanData>();
}

ExportResult
RingBufferSpanExporter::Export(
    const opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>>& spans) noexcept
{
    std::lock_guard lk(mutex_);
    if (shutdown_)
        return ExportResult::kFailure;

    for (const auto& r : spans) {
        const auto* sd = dynamic_cast<const trace_sdk::SpanData*>(r.get());
        if (!sd)
            continue;
        // Evict oldest when at capacity.
        if (buffer_.size() >= maxSpans_)
            buffer_.pop_front();
        buffer_.push_back(std::make_unique<trace_sdk::SpanData>(*sd));
    }
    return ExportResult::kSuccess;
}

bool
RingBufferSpanExporter::ForceFlush(std::chrono::microseconds) noexcept
{
    return true;
}

bool
RingBufferSpanExporter::Shutdown(std::chrono::microseconds) noexcept
{
    std::lock_guard lk(mutex_);
    shutdown_ = true;
    return true;
}

std::vector<std::unique_ptr<trace_sdk::SpanData>>
RingBufferSpanExporter::drain()
{
    std::lock_guard lk(mutex_);
    std::vector<std::unique_ptr<trace_sdk::SpanData>> result;
    result.reserve(buffer_.size());
    for (auto& sp : buffer_)
        result.push_back(std::move(sp));
    buffer_.clear();
    return result;
}

std::vector<std::unique_ptr<trace_sdk::SpanData>>
RingBufferSpanExporter::snapshot() const
{
    std::lock_guard lk(mutex_);
    std::vector<std::unique_ptr<trace_sdk::SpanData>> result;
    result.reserve(buffer_.size());
    for (const auto& sp : buffer_)
        result.push_back(std::make_unique<trace_sdk::SpanData>(*sp));
    return result;
}

std::size_t
RingBufferSpanExporter::size() const
{
    std::lock_guard lk(mutex_);
    return buffer_.size();
}

// ── JSON serialization helpers ─────────────────────────────────────────

namespace {

/// Convert 16-byte trace ID to 32-hex-char string.
std::string traceIdHex(const opentelemetry::trace::TraceId& tid)
{
    char buf[33];
    tid.ToLowerBase16(opentelemetry::nostd::span<char, 32>{buf, 32});
    buf[32] = '\0';
    return buf;
}

/// Convert 8-byte span ID to 16-hex-char string.
std::string spanIdHex(const opentelemetry::trace::SpanId& sid)
{
    char buf[17];
    sid.ToLowerBase16(opentelemetry::nostd::span<char, 16>{buf, 16});
    buf[16] = '\0';
    return buf;
}

/// Escape a string for JSON output.
std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "\\u%04x",
                              static_cast<unsigned>(c));
                out += hex;
            } else {
                out += c;
            }
        }
    }
    return out;
}

/// Convert an OTel attribute value to a JSON string fragment.
/// Uses explicit type checks since OwnedAttributeValue may be absl::variant.
std::string attrValueToJson(const opentelemetry::sdk::common::OwnedAttributeValue& val)
{
    namespace nostd = opentelemetry::nostd;
    if (nostd::holds_alternative<bool>(val))
        return nostd::get<bool>(val) ? "true" : "false";
    if (nostd::holds_alternative<int32_t>(val))
        return std::to_string(nostd::get<int32_t>(val));
    if (nostd::holds_alternative<uint32_t>(val))
        return std::to_string(nostd::get<uint32_t>(val));
    if (nostd::holds_alternative<int64_t>(val))
        return std::to_string(nostd::get<int64_t>(val));
    if (nostd::holds_alternative<uint64_t>(val))
        return std::to_string(nostd::get<uint64_t>(val));
    if (nostd::holds_alternative<double>(val))
        return std::to_string(nostd::get<double>(val));
    if (nostd::holds_alternative<std::string>(val))
        return "\"" + jsonEscape(nostd::get<std::string>(val)) + "\"";
    return "null";
}

/// Convert nanosecond timestamp to ISO-8601 string.
std::string nanosToIso(opentelemetry::common::SystemTimestamp ts)
{
    auto ns = ts.time_since_epoch().count();
    auto secs = static_cast<time_t>(ns / 1000000000LL);
    auto frac = ns % 1000000000LL;
    struct tm utc {};
    gmtime_r(&secs, &utc);
    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec, static_cast<long>(frac));
    return buf;
}

} // anonymous namespace

std::string
RingBufferSpanExporter::toJson() const
{
    std::lock_guard lk(mutex_);
    std::ostringstream os;
    os << "[\n";

    bool first = true;
    for (const auto& sp : buffer_) {
        if (!first) os << ",\n";
        first = false;

        os << "  {\n";
        os << "    \"name\": \"" << jsonEscape(std::string(sp->GetName())) << "\",\n";
        os << "    \"traceId\": \"" << traceIdHex(sp->GetTraceId()) << "\",\n";
        os << "    \"spanId\": \"" << spanIdHex(sp->GetSpanId()) << "\",\n";
        os << "    \"parentSpanId\": \"" << spanIdHex(sp->GetParentSpanId()) << "\",\n";
        os << "    \"startTime\": \"" << nanosToIso(sp->GetStartTime()) << "\",\n";
        // OTel SDK stores start + duration; compute end time.
        auto endNs = sp->GetStartTime().time_since_epoch() + sp->GetDuration();
        opentelemetry::common::SystemTimestamp endTs(endNs);
        os << "    \"endTime\": \"" << nanosToIso(endTs) << "\",\n";
        os << "    \"status\": " << static_cast<int>(sp->GetStatus()) << ",\n";

        // Attributes
        os << "    \"attributes\": {";
        {
            bool attrFirst = true;
            for (const auto& [key, val] : sp->GetAttributes()) {
                if (!attrFirst) os << ",";
                attrFirst = false;
                os << "\n      \"" << jsonEscape(std::string(key)) << "\": "
                   << attrValueToJson(val);
            }
        }
        if (!sp->GetAttributes().empty()) os << "\n    ";
        os << "},\n";

        // Events
        os << "    \"events\": [";
        {
            bool evFirst = true;
            for (const auto& ev : sp->GetEvents()) {
                if (!evFirst) os << ",";
                evFirst = false;
                os << "\n      {\"name\": \"" << jsonEscape(std::string(ev.GetName()))
                   << "\", \"timestamp\": \"" << nanosToIso(ev.GetTimestamp()) << "\""
                   << ", \"attributes\": {";
                bool eaFirst = true;
                for (const auto& [k, v] : ev.GetAttributes()) {
                    if (!eaFirst) os << ",";
                    eaFirst = false;
                    os << "\"" << jsonEscape(std::string(k)) << "\": "
                       << attrValueToJson(v);
                }
                os << "}}";
            }
        }
        if (!sp->GetEvents().empty()) os << "\n    ";
        os << "]\n";

        os << "  }";
    }

    os << "\n]\n";
    return os.str();
}

bool
RingBufferSpanExporter::exportToFile(const std::string& path) const
{
    try {
        auto json = toJson();
        std::ofstream ofs(path, std::ios::out | std::ios::trunc);
        if (!ofs) {
            JAMI_ERROR("[otel] Failed to open export file: {}", path);
            return false;
        }
        ofs << json;
        ofs.close();
        JAMI_LOG("[otel] Exported {} spans to {}", size(), path);
        return true;
    } catch (const std::exception& e) {
        JAMI_ERROR("[otel] Failed to export spans to file: {}", e.what());
        return false;
    }
}

} // namespace telemetry
} // namespace jami
