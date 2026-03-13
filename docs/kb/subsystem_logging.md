# Logging

## Status: draft

## Last Updated: 2026-03-13

---

## Purpose

The logging subsystem provides a thread-safe, level-driven, zero-heap-allocation-on-hot-path logging facility for all libjami subsystems. It supports four severity levels (DEBUG, INFO, WARNING, ERROR), integrates with platform-specific sinks (syslog on Linux/macOS, Android NDK logcat, Windows EventLog via `winsyslog`), and exposes both printf-style and C++ stream-style logging macros. It also bridges OpenDHT's internal logger so DHT library messages are routed through the same pipeline. The `{fmt}` library is used for formatting to avoid `snprintf` allocation overhead.

---

## Key Files

- `src/logger.h` — `Logger` class, `JAMI_DBG` / `JAMI_WARN` / `JAMI_ERR` / `JAMI_INFO` macros, `JAMI_LOG` variadic macro
- `src/logger.cpp` — `Logger::Handler` implementation, sink dispatch, Android/syslog/Windows backends
- `src/jami/def.h` — `LIBJAMI_PUBLIC` visibility macro (used in logger exports)
- `src/winsyslog.h` / `src/winsyslog.c` — Windows EventLog syslog shim

---

## Key Classes

| Class | Role | File |
|---|---|---|
| `Logger` | Stream-style logger; destructor flushes accumulated `ostringstream` to `log()` | `src/logger.h` |
| `Logger::Handler` | Singleton output handler; dispatches to platform sink; thread-safe via internal mutex | `src/logger.h` |
| `Logger::Msg` | Immutable log record: level, file, line, message string | `src/logger.h` |

---

## Macro API

```cpp
JAMI_DBG("format %s", arg);      // LOG_DEBUG
JAMI_INFO("format %s", arg);     // LOG_INFO
JAMI_WARN("format %s", arg);     // LOG_WARNING
JAMI_ERR("format %s", arg);      // LOG_ERR

// Stream style:
JAMI_LOG << "value=" << val;

// fmt-style (preferred for new code):
JAMI_DBG(FMT_STRING("value={}"), val);
```

---

## External Dependencies

- **{fmt}** (`fmt/core.h`, `fmt/format.h`, `fmt/chrono.h`, `fmt/compile.h`, `fmt/printf.h`, `fmt/std.h`) — format string rendering
- **OpenDHT logger** (`opendht/logger.h`) — OpenDHT uses a `dht::Logger` interface; libjami provides an implementation routing to `Logger::Handler`
- **syslog** (POSIX) — production sink on Linux/macOS
- **Android NDK logcat** (`android/log.h`) — sink on Android
- **winsyslog** — Windows EventLog sink

---

## Threading Model

- `Logger` objects are stack-allocated per log call site; non-copyable, moveable.
- Flush (destructor) calls `log()` which acquires `Logger::Handler`'s internal mutex → thread-safe for concurrent callers.
- No background logging thread; all I/O is synchronous in the calling thread's destructor.
- High-rate DEBUG logging from media threads can therefore add measurable latency; users are advised to disable DEBUG level in production.

---

## Estimated Instrumentation Value

**Very High — as infrastructure.** The logger is the mechanism through which all other subsystems are observable. Its own instrumentation value (tracing the logger itself) is negligible, but ensuring consistent log levels and structured fields (call-id, account-id) in log messages across all subsystems is the primary leverage point for operability.

---

## Open Questions

1. Is there a log-rate limiter to prevent media-thread DEBUG floods from impacting call quality?
2. Can the log handler be replaced at runtime (e.g. to redirect logs to a file or a UI panel) after `init()`?
3. Are there any asynchronous buffering options for high-throughput logging contexts (Android, embedded)?
4. Does the Windows winsyslog implementation support Unicode paths/message strings correctly?
