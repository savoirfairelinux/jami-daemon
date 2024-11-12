/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include <string>

namespace jami {
/**
 * @struct WebViewMessage
 * @brief Contains data about a web view message
 * Used by WebViewServicesManager. Passed from a plugin to the daemon. After that, this struct is no
 * longer used.
 */
struct WebViewMessage
{
    // Which webview is this message about
    const std::string webViewId;

    // Message identifier
    const std::string messageId;

    // The actual message itself. Can be a path, JSON, XML, or anything,
    // as long as it fits in a string
    const std::string payload;
};
} // namespace jami
