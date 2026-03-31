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

#include "telemetry.h"

#include "telemetry_runtime.h"

namespace jami {
namespace telemetry {

/**
 * @brief initTelemetry Forwards facade initialization to the shared runtime.
 * @param serviceName Service name reported in telemetry resources.
 * @param version Service version reported in telemetry resources.
 * @param deviceId Local device identifier attached to spans.
 * @return void
 */
void
initTelemetry(const std::string& serviceName,
              const std::string& version,
              const std::string& deviceId)
{
    detail::runtime().init(serviceName, version, deviceId);
}

/**
 * @brief shutdownTelemetry Forwards facade shutdown to the shared runtime.
 * @return void
 */
void
shutdownTelemetry()
{
    detail::runtime().shutdown();
}

/**
 * @brief isInitialized Reports runtime availability through the public facade.
 * @return true when telemetry is initialized.
 */
bool
isInitialized() noexcept
{
    return detail::runtime().isInitialized();
}

/**
 * @brief startSpan Starts a root span through the shared runtime.
 * @param name Span name.
 * @param options Span scope and initial attributes.
 * @return Move-only handle that controls the live span.
 */
SpanHandle
startSpan(std::string_view name, const SpanStartOptions& options)
{
    return detail::runtime().startSpan(name, options);
}

/**
 * @brief startChildSpan Starts a child span through the shared runtime.
 * @param parent Parent span handle.
 * @param name Child span name.
 * @param options Span scope and initial attributes.
 * @return Move-only handle that controls the live child span.
 */
SpanHandle
startChildSpan(const SpanHandle& parent,
               std::string_view name,
               const SpanStartOptions& options)
{
    return detail::runtime().startChildSpan(parent, name, options);
}

/**
 * @brief recordTrace Converts string attributes and records a one-shot span.
 * @param name Span name.
 * @param attributes String attributes to attach to the span.
 * @return void
 */
void
recordTrace(const std::string& name,
            const std::map<std::string, std::string>& attributes)
{
    Attributes converted;
    converted.reserve(attributes.size());
    for (const auto& [key, value] : attributes)
        converted.emplace_back(key, value);
    detail::runtime().recordTrace(name, converted);
}

/**
 * @brief exportTraces Forwards local trace export to the shared runtime.
 * @param destinationPath Target file path or empty for the default cache path.
 * @return Exported file path, or an empty string on failure.
 */
std::string
exportTraces(const std::string& destinationPath)
{
    return detail::runtime().exportTraces(destinationPath);
}

} // namespace telemetry
} // namespace jami
