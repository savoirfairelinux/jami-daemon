// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024-2026 Savoir-faire Linux Inc.
/**
 * OTel Logging Bridge for jami-daemon
 *
 * Forwards jami log records to the OpenTelemetry Logs API.
 * Injects trace_id and span_id from the active span context.
 *
 * Usage:
 *   Call installOtelLogBridge() after initOtel() and before daemon startup.
 *   The bridge registers itself as a jami Logger handler via
 *   Logger::setExtraHandler().
 *   No changes to existing JAMI_LOG / JAMI_DEBUG / JAMI_ERR call sites are
 *   needed.
 */
#pragma once

namespace jami {
namespace otel {

/**
 * Install the OTel log bridge as a jami Logger extra handler.
 * Must be called after initOtel() has been called.
 *
 * The bridge will forward all log records to the OTel Logs API with:
 *   - Severity mapped from the jami/syslog level to OTel Severity
 *   - trace_id and span_id injected from the active span context
 *   - Source file, line injected as "code.filepath" / "code.lineno" attributes
 *
 * @param min_severity  Only forward records whose rank is >= min_severity,
 *                      where the rank encoding is:
 *                        0 = DEBUG, 1 = INFO, 2 = WARN, 3 = ERROR, 4 = FATAL
 *                      Default: 2 (WARN) — avoids forwarding high-frequency
 *                      DEBUG/INFO messages to the OTel collector.
 *                      Pass 0 to forward everything including DEBUG.
 * @return true if the bridge was installed successfully.
 *         false if ENABLE_OTEL is not defined (no-op build).
 *         Always returns true when ENABLE_OTEL is defined (the bridge installs
 *         even if the OTel provider is the no-op provider; it will simply emit
 *         to the no-op logger until a real provider is installed).
 */
bool installOtelLogBridge(int min_severity = 2 /* WARN */);

/**
 * Remove the OTel log bridge from jami Logger handlers.
 * Must be called before shutdownOtel() to prevent the bridge from trying to
 * emit log records into a shut-down provider.
 * Safe to call even if installOtelLogBridge() was never called.
 */
void removeOtelLogBridge();

} // namespace otel
} // namespace jami
