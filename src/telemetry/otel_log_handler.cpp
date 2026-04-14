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

#include "otel_log_handler.h"

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/severity.h>
#include <opentelemetry/common/timestamp.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/context/runtime_context.h>

#include <chrono>

namespace logs_api = opentelemetry::logs;

namespace jami {

opentelemetry::nostd::shared_ptr<logs_api::Logger>
OTelLogHandler::otelLogger()
{
    return logs_api::Provider::GetLoggerProvider()->GetLogger(
        "jami.daemon", PACKAGE_VERSION);
}

logs_api::Severity
OTelLogHandler::mapSeverity(int level) noexcept
{
    // Jami uses syslog-compatible levels (lower number = higher severity).
    switch (level) {
    case LOG_ERR:     return logs_api::Severity::kError;
    case LOG_WARNING: return logs_api::Severity::kWarn;
    case LOG_INFO:    return logs_api::Severity::kInfo;
    case LOG_DEBUG:   return logs_api::Severity::kDebug;
    default:          return logs_api::Severity::kInfo;
    }
}

void
OTelLogHandler::consume(const Logger::Msg& msg)
{
    auto logger = otelLogger();
    if (!logger)
        return;

    auto severity = mapSeverity(msg.level_);

    // Build the log record.  The OTel Logs API allows passing the body
    // and attributes directly via EmitLogRecord().
    auto record = logger->CreateLogRecord();
    if (!record)
        return;

    record->SetSeverity(severity);
    record->SetBody(opentelemetry::nostd::string_view{
        msg.payload_.data(), msg.payload_.size()});
    record->SetTimestamp(
        opentelemetry::common::SystemTimestamp{std::chrono::system_clock::now()});

    // Semantic conventions for source code location.
    if (!msg.file_.empty())
        record->SetAttribute("code.filepath",
                             opentelemetry::nostd::string_view{
                                 msg.file_.data(), msg.file_.size()});
    if (msg.line_ > 0)
        record->SetAttribute("code.lineno", static_cast<int64_t>(msg.line_));
    if (!msg.tag_.empty())
        record->SetAttribute("log.tag",
                             opentelemetry::nostd::string_view{
                                 msg.tag_.data(), msg.tag_.size()});

    // Attach the current trace context (if any) so logs can be correlated
    // with spans in a trace backend.
    auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto span = opentelemetry::trace::GetSpan(ctx);
    if (span && span->GetContext().IsValid()) {
        record->SetTraceId(span->GetContext().trace_id());
        record->SetSpanId(span->GetContext().span_id());
        record->SetTraceFlags(span->GetContext().trace_flags());
    }

    logger->EmitLogRecord(std::move(record));
}

} // namespace jami
