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

/**
 * @file otel_log_handler.h
 * @brief Bridge between Jami's Logger::Handler and the OpenTelemetry
 *        Logs signal type.
 *
 * This handler converts every Jami log message (JAMI_LOG, JAMI_DEBUG,
 * JAMI_WARNING, JAMI_ERROR, …) into an OTel LogRecord emitted via the
 * Logs SDK.  The LogRecord carries:
 *
 *   - severity  (mapped from LOG_DEBUG / LOG_INFO / LOG_WARNING / LOG_ERR)
 *   - body      (the formatted message payload)
 *   - attributes: code.filepath, code.lineno, log.tag
 *   - timestamp (wall-clock time at the point of emission)
 *
 * The handler is designed to be registered as an additional handler in
 * LogDispatcher, alongside ConsoleLog, SysLog, etc.  It is only active
 * when telemetry is initialized and the handler is explicitly enabled.
 *
 * Thread-safety: consume() may be called from the LogDispatcher's
 * background thread.  The OTel Logs SDK's LoggerProvider and Logger
 * instances are themselves thread-safe.
 */

#include "logger.h"

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/severity.h>

#include <atomic>
#include <string>

namespace jami {

class OTelLogHandler final : public Logger::Handler
{
public:
    OTelLogHandler() = default;

    void consume(const Logger::Msg& msg) override;

    /**
     * Return the shared OTel Logger instance, creating it on first call.
     * Uses the global LoggerProvider set by initTelemetry().
     */
    static opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>
    otelLogger();

    /**
     * Map a Jami log level (LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR)
     * to the corresponding OTel Severity.
     */
    static opentelemetry::logs::Severity
    mapSeverity(int level) noexcept;
};

} // namespace jami
