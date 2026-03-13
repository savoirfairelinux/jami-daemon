// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
//
// otel_log_bridge.cpp
// ───────────────────
// Forwards jami log records to the OpenTelemetry Logs API.
//
// Architecture
// ────────────
// jami's Logger dispatches every log record through a background thread
// (LogDispatcher::loop).  The bridge installs itself via
// Logger::setExtraHandler(), which registers a std::function<> that is
// invoked from that background thread for every record that reaches the
// dispatcher.
//
// Re-entrancy guard
// ─────────────────
// The OTel SDK's BatchLogRecordProcessor may call internal diagnostic
// functions on failure.  To prevent those from re-entering the bridge (and
// causing an infinite loop), a thread-local boolean flag is checked at entry;
// the bridge silently skips any call that occurs while the flag is set.
//
// Thread safety
// ─────────────
// The cached OTel logger (sLogger) is assigned once under a call_once guard
// in installOtelLogBridge() and then read-only from the dispatch thread.
// No additional locking is needed after installation.

#include "otel_log_bridge.h"

#ifdef ENABLE_OTEL

// ── jami headers ─────────────────────────────────────────────────────────────
#include "logger.h"
#include "otel/otel_init.h"

// ── Standard library ─────────────────────────────────────────────────────────
#include <atomic>
#include <chrono>
#include <mutex>

// ── OTel API ─────────────────────────────────────────────────────────────────
#include <opentelemetry/common/timestamp.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/context.h>

// Platform headers — LOG_DEBUG etc. are resolved in logger.h already.
#ifdef __ANDROID__
#  include <android/log.h>
#elif defined(_WIN32)
#  include "winsyslog.h"
#else
#  include <syslog.h>
#endif

namespace {

namespace logs_api  = opentelemetry::logs;
namespace common    = opentelemetry::common;
namespace trace_api = opentelemetry::trace;
namespace ctx_api   = opentelemetry::context;

// ── Re-entrancy guard ─────────────────────────────────────────────────────────
// Set to true on the dispatch thread while the bridge callback is running.
// Any nested call (e.g. from the OTel SDK itself) returns immediately.
thread_local bool gInBridgeCallback = false;

// ── Shared logger handle ──────────────────────────────────────────────────────
// Initialised once by installOtelLogBridge(); read-only thereafter.
opentelemetry::nostd::shared_ptr<logs_api::Logger> gBridgeLogger;
std::once_flag                                      gLoggerInitFlag;

// ── Installation state ────────────────────────────────────────────────────────
std::atomic<bool> gBridgeInstalled {false};

// ─────────────────────────────────────────────────────────────────────────────
// Severity helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Map a jami/syslog level integer to a 0-4 severity rank where
/// higher values represent higher urgency:
///   0 = DEBUG, 1 = INFO/NOTICE, 2 = WARNING, 3 = ERROR, 4 = CRITICAL/FATAL
///
/// The mapping uses direct comparisons against the platform macros (which take
/// different numeric values on Linux/syslog, Android, and Windows) so that it
/// is correct on all supported platforms.
static int
levelToRank(int level)
{
#if defined(__ANDROID__)
    // Android: ANDROID_LOG_DEBUG=3 .. ANDROID_LOG_FATAL=7 (ascending urgency)
    if (level >= ANDROID_LOG_FATAL)   return 4;
    if (level >= ANDROID_LOG_ERROR)   return 3;
    if (level >= ANDROID_LOG_WARN)    return 2;
    if (level >= ANDROID_LOG_INFO)    return 1;
    return 0; // DEBUG or below
#elif defined(_WIN32)
    // Windows EventLog defines  (lower = more urgent, same as syslog):
    //   EVENTLOG_ERROR_TYPE=1, EVENTLOG_WARNING_TYPE=2, EVENTLOG_INFORMATION_TYPE=4
    if (level <= EVENTLOG_ERROR_TYPE)   return 3;
    if (level <= EVENTLOG_WARNING_TYPE) return 2;
    if (level <= EVENTLOG_INFORMATION_TYPE) return 1;
    return 0; // EVENTLOG_SUCCESS (debug)
#else
    // POSIX syslog: lower value = higher urgency
    //   LOG_EMERG=0 .. LOG_DEBUG=7
    if (level <= LOG_CRIT)    return 4;
    if (level <= LOG_ERR)     return 3;
    if (level <= LOG_WARNING) return 2;
    if (level <= LOG_INFO)    return 1;
    return 0; // LOG_DEBUG or below
#endif
}

static logs_api::Severity
levelToOtelSeverity(int level)
{
    switch (levelToRank(level)) {
    case 0:  return logs_api::Severity::kDebug;
    case 1:  return logs_api::Severity::kInfo;
    case 2:  return logs_api::Severity::kWarn;
    case 3:  return logs_api::Severity::kError;
    case 4:  return logs_api::Severity::kFatal;
    default: return logs_api::Severity::kTrace;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// The bridge callback — registered with Logger::setExtraHandler()
// ─────────────────────────────────────────────────────────────────────────────

void
bridgeCallback(int              level,
               std::string_view file,
               unsigned         line,
               std::string_view message,
               int              minSeverity)
{
    // ── Re-entrancy guard ────────────────────────────────────────────────────
    if (gInBridgeCallback)
        return;
    gInBridgeCallback = true;
    struct Guard {
        ~Guard() { gInBridgeCallback = false; }
    } guard;

    // ── Severity filter ──────────────────────────────────────────────────────
    // Apply before constructing the log record to avoid wasted allocations.
    if (levelToRank(level) < minSeverity)
        return;

    // ── Obtain the cached OTel logger ────────────────────────────────────────
    auto& logger = gBridgeLogger;
    if (!logger)
        return;

    // ── Create and populate the log record ───────────────────────────────────
    auto record = logger->CreateLogRecord();
    if (!record)   // no-op logger returns nullptr
        return;

    const logs_api::Severity severity = levelToOtelSeverity(level);
    record->SetSeverity(severity);

    record->SetBody(opentelemetry::nostd::string_view(message.data(), message.size()));

    record->SetTimestamp(
        common::SystemTimestamp(std::chrono::system_clock::now()));

    // ── Source-location attributes ────────────────────────────────────────────
    record->SetAttribute("code.filepath",
                         opentelemetry::nostd::string_view(file.data(), file.size()));
    record->SetAttribute("code.lineno", static_cast<int64_t>(line));

    // ── Inject active span context (log–trace correlation) ───────────────────
    auto ctx = ctx_api::RuntimeContext::GetCurrent();
    auto active_span = opentelemetry::trace::GetSpan(ctx);
    auto span_ctx = active_span->GetContext();
    if (span_ctx.IsValid()) {
        record->SetTraceId(span_ctx.trace_id());
        record->SetSpanId(span_ctx.span_id());
        record->SetTraceFlags(span_ctx.trace_flags());
    }

    logger->EmitLogRecord(std::move(record));
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

namespace jami {
namespace otel {

bool
installOtelLogBridge(int min_severity)
{
    // Prevent double installation.
    bool expected = false;
    if (!gBridgeInstalled.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel))
        return true; // already installed

    // Acquire the OTel logger once.  If the provider is the no-op provider
    // (OTel not yet initialised), getOtelLogger() returns a no-op logger whose
    // Create/Emit calls are free-function no-ops — harmless.
    std::call_once(gLoggerInitFlag, []() {
        gBridgeLogger = getOtelLogger("jami.daemon");
    });

    // Register the callback.  Capture min_severity by value.
    jami::Logger::setExtraHandler(
        [min_severity](int level, std::string_view file, unsigned ln, std::string_view msg) {
            bridgeCallback(level, file, ln, msg, min_severity);
        });

    return true;
}

void
removeOtelLogBridge()
{
    bool expected = true;
    if (!gBridgeInstalled.compare_exchange_strong(expected, false,
                                                   std::memory_order_acq_rel))
        return; // was not installed

    jami::Logger::clearExtraHandler();

    // Release the logger handle so the refcount drops before shutdownOtel().
    gBridgeLogger = opentelemetry::nostd::shared_ptr<logs_api::Logger>{};
}

} // namespace otel
} // namespace jami

#else // !ENABLE_OTEL ─────────────────────────────────────────────────────────

namespace jami {
namespace otel {

bool  installOtelLogBridge(int /*min_severity*/) { return false; }
void  removeOtelLogBridge()                       {}

} // namespace otel
} // namespace jami

#endif // ENABLE_OTEL
