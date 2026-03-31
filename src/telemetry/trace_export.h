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

#include "trace_types.h"

#include <string>
#include <vector>

namespace jami::telemetry::detail {

/**
 * @brief exportBufferedTraces Writes buffered span snapshots to a JSON trace file.
 * @param serviceName Service name recorded in the export metadata.
 * @param serviceVersion Service version recorded in the export metadata.
 * @param deviceId Local device identifier recorded in the export metadata.
 * @param spans Span snapshots to serialize.
 * @param destinationPath Target file path or empty for the default cache path.
 * @return Exported file path, or an empty string on failure.
 */
std::string exportBufferedTraces(const std::string& serviceName,
                                 const std::string& serviceVersion,
                                 const std::string& deviceId,
                                 const std::vector<SpanSnapshot>& spans,
                                 const std::string& destinationPath);

} // namespace jami::telemetry::detail