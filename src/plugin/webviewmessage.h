/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *
 *  Author: Tobias Hildebrandt <tobias.hildebrandt@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once
#include <string>

/**
 * @struct WebViewMessage
 * @brief Contains data about a web view message
 * Used by WebViewServicesManager. Passed from a plugin to the daemon (and eventually to the client
 * webview).
 */
struct WebViewMessage
{
    /**
     * @param pluginId
     * @param accountId
     * @param webViewId
     * @param returnValuePtr
     * @param payload
     */
    WebViewMessage(const std::string& pluginId,
                   const std::string& webViewId,
                   const std::string& messageId,
                   const std::string& payload)
        : pluginId {pluginId}
        , webViewId {webViewId}
        , messageId {messageId}
        , payload {payload}
    {}

    // Which plugin is this message about
    const std::string pluginId;

    // Which webview is this message about
    const std::string webViewId;

    // Message identifier
    const std::string& messageId;

    // The actual message itself. Can be a path, JSON, XML, or anything, as long as it fits in a string.
    const std::string payload;
};