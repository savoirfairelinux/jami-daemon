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

#include <map>
#include <string>

namespace jami {

/**
 * @brief This is an abstract class (API) that needs to be implemented by a plugin.
 * Any plugin that wants to open a WebView needs to implement this class.
 */
class WebViewHandler
{
public:
    virtual ~WebViewHandler() {}

    /**
     * @brief Returns the dataPath of the plugin that created this WebViewHandler.
     */
    std::string id() const { return id_; }

    /**
     * @brief Should be called by the WebViewHandler creator to set the plugin's id_ variable.
     */
    virtual void setId(const std::string& id) final { id_ = id; }

    /**
     * @brief Returns a map with handler's name, iconPath, and pluginId.
     */
    virtual std::map<std::string, std::string> getWebViewHandlerDetails() = 0;

    // theses are called by the client, must be implemented by the plugin

    virtual void pluginWebViewMessage(const std::string& pluginId,
                                             const std::string& webViewId,
                                             const std::string& messageId,
                                             const std::string& payload)
        = 0;

    // returns the path of html file to load
    virtual std::string pluginWebViewAttach(const std::string& pluginId,
                                            const std::string& accountId,
                                            const std::string& webViewId,
                                            const std::string& action)
        = 0;

    virtual void pluginWebViewDetach(const std::string& pluginId,
                                     const std::string& accountId,
                                     const std::string& webViewId)
        = 0;

private:
    // the dataPath of the plugin that created this WebViewHandler
    // aka pluginId
    std::string id_;
};
} // namespace jami
