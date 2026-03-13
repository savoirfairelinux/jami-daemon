# OpenTelemetry Logs — C++ Bridge API Guide

| Field        | Value       |
|--------------|-------------|
| Status       | draft       |
| Last Updated | 2026-03-13  |

---

## 1. LoggerProvider and Logger Acquisition

```cpp
#include "opentelemetry/logs/provider.h"

namespace logs_api = opentelemetry::logs;

// Acquire once; cache in a static or class member.
auto logger = logs_api::Provider::GetLoggerProvider()
                  ->GetLogger("jami.sip",          // logger name (instrumentation scope)
                              "",                   // library version (optional)
                              "https://jami.net/schema");  // schema URL (optional)
```

If no `LoggerProvider` has been installed (SDK not initialised), the API returns a no-op logger — all `Emit()` calls are discarded with zero overhead.

---

## 2. LogRecord Structure

A `LogRecord` carries the following fields:

| Field | Type | Notes |
|-------|------|-------|
| `body` | `AttributeValue` (usually `std::string`) | Human-readable message text |
| `severity` | `Severity` enum | See §4 Severity Mapping |
| `severity_text` | `std::string` | Optional string label ("ERROR", "WARN", …) |
| `timestamp` | `SystemTimestamp` | When the event occurred |
| `observed_time_stamp` | `SystemTimestamp` | When it was observed/collected |
| `trace_id` | 16-byte array | From active span context |
| `span_id` | 8-byte array | From active span context |
| `trace_flags` | `TraceFlags` | Sampled/not-sampled |
| Attributes | key-value pairs | Structured fields; see §5 |

---

## 3. Emitting a Log Record

```cpp
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/common/timestamp.h"

namespace logs_api = opentelemetry::logs;
namespace common   = opentelemetry::common;

void EmitLog(opentelemetry::nostd::shared_ptr<logs_api::Logger> logger,
             logs_api::Severity severity,
             const std::string& message)
{
    auto record = logger->CreateLogRecord();
    if (!record) return;  // no-op logger returns nullptr

    record->SetSeverity(severity);
    record->SetSeverityText(logs_api::SeverityNumToText(severity));
    record->SetBody(message);
    record->SetTimestamp(common::SystemTimestamp(std::chrono::system_clock::now()));

    logger->EmitLogRecord(std::move(record));
}
```

---

## 4. Severity Level Mapping

jami-daemon's `logger.h` uses a custom severity macro `JAMI_LOG(level, ...)`. Map to OTel as follows:

| jami-daemon level | Logger macro | OTel `Severity` | Numeric |
|-------------------|-------------|-----------------|---------|
| `LOG_DEBUG` / `JAMI_DBG` | debug logging | `kDebug` (or `kDebug4`) | 5–8 |
| `LOG_INFO` / `JAMI_INFO` | informational | `kInfo` | 9–12 |
| `LOG_WARN` / `JAMI_WARN` | warning | `kWarn` | 13–16 |
| `LOG_ERR` / `JAMI_ERR` | error | `kError` | 17–20 |
| `LOG_CRIT` (if used) | fatal/critical | `kFatal` | 21–24 |

```cpp
#include "opentelemetry/logs/severity.h"

// Mapping function:
static opentelemetry::logs::Severity JamiLevelToOtelSeverity(int jami_level) {
    switch (jami_level) {
        case LOG_DEBUG:   return opentelemetry::logs::Severity::kDebug;
        case LOG_INFO:    return opentelemetry::logs::Severity::kInfo;
        case LOG_WARNING: return opentelemetry::logs::Severity::kWarn;
        case LOG_ERR:     return opentelemetry::logs::Severity::kError;
        case LOG_CRIT:    return opentelemetry::logs::Severity::kFatal;
        default:          return opentelemetry::logs::Severity::kTrace;
    }
}
```

---

## 5. Injecting trace_id and span_id from Active Span Context

To correlate logs with traces, inject the current span's trace and span IDs into each log record.

```cpp
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/span_context.h"

void EmitLogWithTraceContext(
    opentelemetry::nostd::shared_ptr<logs_api::Logger> logger,
    logs_api::Severity severity,
    const std::string& message,
    const std::map<std::string, std::string>& extra_attrs = {})
{
    auto record = logger->CreateLogRecord();
    if (!record) return;

    record->SetSeverity(severity);
    record->SetBody(message);
    record->SetTimestamp(
        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));

    // Inject trace context from current active span
    auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto span = opentelemetry::trace::GetSpan(ctx);
    auto span_ctx = span->GetContext();

    if (span_ctx.IsValid()) {
        record->SetTraceId(span_ctx.trace_id());
        record->SetSpanId(span_ctx.span_id());
        record->SetTraceFlags(span_ctx.trace_flags());
    }

    // Add structured attributes
    for (const auto& [key, val] : extra_attrs) {
        record->SetAttribute(key, val);
    }

    logger->EmitLogRecord(std::move(record));
}
```

---

## 6. Bridging the jami-daemon Logger

jami-daemon uses a printf-style logger accessed via macros (`JAMI_DBG`, `JAMI_INFO`, `JAMI_ERR`, etc.) in `src/logger.h` / `src/logger.cpp`.

### Design: Handler-based bridge

The existing `Logger` class in jami-daemon allows registering custom log handlers. The bridge installs a handler that forwards to OTel Logs.

```cpp
// src/otel/otel_log_bridge.h  (new file to be created)
#pragma once

#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/context/runtime_context.h"

#include "logger.h"   // jami-daemon's Logger

namespace jami::otel {

class OtelLogBridge {
public:
    static void Install()
    {
        // jami uses jami::Logger::setLogHandler() or similar hook
        jami::Logger::setLogHandler([](jami::LogLevel level,
                                        const char* file,
                                        int line,
                                        const char* message) {
            Forward(level, file, line, message);
        });
    }

private:
    static void Forward(jami::LogLevel level,
                        const char* file,
                        int line,
                        const char* message)
    {
        namespace logs_api = opentelemetry::logs;
        namespace common   = opentelemetry::common;

        static auto logger = logs_api::Provider::GetLoggerProvider()
                                 ->GetLogger("jami.logger");

        auto record = logger->CreateLogRecord();
        if (!record) return;

        record->SetSeverity(JamiLevelToOtelSeverity(level));
        record->SetSeverityText(JamiLevelName(level));
        record->SetBody(std::string(message));
        record->SetTimestamp(common::SystemTimestamp(std::chrono::system_clock::now()));

        // Structured source location attributes
        record->SetAttribute("code.filepath", std::string(file));
        record->SetAttribute("code.lineno",   static_cast<int64_t>(line));

        // Inject active span context for log-trace correlation
        auto ctx      = opentelemetry::context::RuntimeContext::GetCurrent();
        auto span     = opentelemetry::trace::GetSpan(ctx);
        auto span_ctx = span->GetContext();
        if (span_ctx.IsValid()) {
            record->SetTraceId(span_ctx.trace_id());
            record->SetSpanId(span_ctx.span_id());
            record->SetTraceFlags(span_ctx.trace_flags());
        }

        logger->EmitLogRecord(std::move(record));
    }

    static opentelemetry::logs::Severity JamiLevelToOtelSeverity(jami::LogLevel lvl)
    {
        using S = opentelemetry::logs::Severity;
        switch (lvl) {
            case jami::LogLevel::Debug:   return S::kDebug;
            case jami::LogLevel::Info:    return S::kInfo;
            case jami::LogLevel::Warning: return S::kWarn;
            case jami::LogLevel::Error:   return S::kError;
            default:                      return S::kTrace;
        }
    }

    static const char* JamiLevelName(jami::LogLevel lvl)
    {
        switch (lvl) {
            case jami::LogLevel::Debug:   return "DEBUG";
            case jami::LogLevel::Info:    return "INFO";
            case jami::LogLevel::Warning: return "WARN";
            case jami::LogLevel::Error:   return "ERROR";
            default:                      return "TRACE";
        }
    }
};

} // namespace jami::otel
```

### Integration point

In `src/manager.cpp` (or wherever the OTel SDK is initialized):

```cpp
#ifdef WITH_OPENTELEMETRY
#include "otel/otel_log_bridge.h"
// ...
void Manager::init(/* ... */) {
    OtelInit();                         // initialize SDK first
    jami::otel::OtelLogBridge::Install(); // then install bridge
    // ... rest of init
}
#endif
```

---

## 7. Severity Filtering Recommendation

To avoid flooding the OTel collector with debug logs in production:

```cpp
// Only forward WARN and above in release builds:
static void Forward(jami::LogLevel level, ...) {
#ifdef NDEBUG
    if (level < jami::LogLevel::Warning) return;
#endif
    // ... rest of bridge
}
```

Or configure the `SimpleLogRecordProcessor` / `BatchLogRecordProcessor` with a minimum severity filter — though as of v1.25.0 this must be done in the bridge itself (the SDK does not have a severity-based pre-filter on the processor).

---

## Source References

- [OTel Logs API namespace docs](https://opentelemetry-cpp.readthedocs.io/en/latest/otel_docs/namespace_opentelemetry__logs.html)
- [OTel C++ Exporters — Console (Logs)](https://opentelemetry.io/docs/languages/cpp/exporters/#console)
- [examples/logs_simple](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/logs_simple)
- [OTel Log Data Model spec](https://opentelemetry.io/docs/specs/otel/logs/data-model/)
- [OTel Logs bridge API spec](https://opentelemetry.io/docs/specs/otel/logs/bridge-api/)

---

## Open Questions

1. **jami Logger API**: Does `jami::Logger` have a public hook (`setLogHandler`, observer, etc.) that the bridge can use, or must the bridge modify `logger.cpp` directly? This needs code inspection of `src/logger.h`.
2. **Performance overhead**: The bridge captures the current trace context on every log call. For high-frequency `JAMI_DBG` calls (e.g., RTP packet processing), is this overhead acceptable? Should log bridging be async?
3. **BatchLogRecordProcessor vs Simple**: `SimpleLogRecordProcessorFactory` is synchronous and blocks the calling thread on export. `BatchLogRecordProcessorFactory` is asynchronous. Should `JAMI_ERR` use synchronous and `JAMI_DBG` use async?
4. **Structured fields**: jami's log messages are currently free-form strings. Are there parsable fields (e.g., `[AccountID]` prefixes) that should be extracted into attributes rather than left in the body?
5. **Log volume**: In debug mode, jami-daemon emits extremely high log volumes. Should the bridge rate-limit or sample debug logs? OTel does not have built-in log sampling; this would need custom logic.
