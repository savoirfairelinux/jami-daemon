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

#include "webviewservicesmanager.h"
#include "client/ring_signal.h"
#include "pluginmanager.h"
#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "fileutils.h"
#include "plugin_manager_interface.h"
#include "webviewmessage.h"
#include <cstdint>
#include <vector>

namespace jami {

WebViewServicesManager::WebViewServicesManager(PluginManager& pluginManager)
{
    registerComponentsLifeCycleManagers(pluginManager);
    registerWebViewService(pluginManager);
}

WebViewHandler*
WebViewServicesManager::getWebViewHandlerPointer(const std::string& pluginId)
{
    // check if handler with specified pluginId does not exist
    if (handlersIdMap.find(pluginId) == handlersIdMap.end()) {
        JAMI_ERR("handler with pluginId %s was not found!", pluginId.c_str());
        return nullptr;
    }

    // we know that the pointer exists
    return handlersIdMap[pluginId].get();
}

std::vector<std::string>
WebViewServicesManager::getWebViewHandlers() const
{
    std::vector<std::string> res;
    // preallocate memory since we know the size in advance
    res.reserve(handlersIdMap.size());

    for (const auto& pair : handlersIdMap) {
        auto* handler = pair.second.get();
        res.emplace_back(std::to_string((uintptr_t) handler));
    }
    return res;
}

std::map<std::string, std::string>
WebViewServicesManager::getWebViewHandlerDetails(const std::string& handlerAddress)
{
    // transform the string into a real memory address
    auto address = std::stoull(handlerAddress);
    for (const auto& pair : handlersIdMap) {
        auto* handler = pair.second.get();
        // check if the address matches our handler pointer
        if ((uintptr_t) handler == address) {
            return handler->getWebViewHandlerDetails();
        }
    }

    return {};
}

void
WebViewServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // called by the plugin manager whenever a plugin is loaded
    auto registerWebViewHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);

        WebViewHandlerPtr ptr {(static_cast<WebViewHandler*>(data))};

        // make sure pointer is valid
        if (!ptr) {
            JAMI_ERR("trying to register a webview handler with invalid pointer!");
            return -1;
        }

        // pointer is valid, get details
        auto id = ptr->id();

        // add the handler to our map
        handlersIdMap[id] = std::move(ptr);

        return 0;
    };

    // called by the plugin manager whenever a plugin is unloaded
    auto unregisterWebViewHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_); // obtain mutex

        WebViewHandler* ptr {(static_cast<WebViewHandler*>(data))};

        // make sure pointer is valid
        if (!ptr) {
            JAMI_ERR("trying to unregister a webview handler with invalid pointer!");
            return false;
        }

        // pointer is valid, get details
        auto id = ptr->id();

        // remove from our map, unique_ptr gets destroyed
        handlersIdMap.erase(id);

        return true;
    };

    // register the functions
    pluginManager.registerComponentManager("WebViewHandlerManager",
                                           registerWebViewHandler,
                                           unregisterWebViewHandler);
}

void
WebViewServicesManager::registerWebViewService(PluginManager& pluginManager)
{
    // NOTE: These are API calls that can be called by the plugin
    auto pluginWebViewMessage = [](const DLPlugin*, void* data) {
        // the plugin must pass data as a WebViewMessage pointer
        auto* message = static_cast<WebViewMessage*>(data);

        emitSignal<DRing::WebViewMessageReceived>(message->pluginId,
                                                  message->webViewId,
                                                  message->messageId,
                                                  message->payload);

        return 0;
    };

    // register the service.
    pluginManager.registerService("pluginWebViewMessage", pluginWebViewMessage);
}

void
WebViewServicesManager::sendWebViewMessage(const std::string& pluginId,
                                           const std::string& webViewId,
                                           const std::string& messageId,
                                           const std::string& payload)
{
    auto* handler = getWebViewHandlerPointer(pluginId);

    if (handler) {
        handler->pluginWebViewMessage(pluginId, webViewId, messageId, payload);
    } else {
        JAMI_ERR("handler pointer is null for pluginId: %s!", pluginId.c_str());
    }
}

std::string
WebViewServicesManager::sendWebViewAttach(const std::string& pluginId,
                                          const std::string& accountId,
                                          const std::string& webViewId,
                                          const std::string& action)
{
    auto* handler = getWebViewHandlerPointer(pluginId);

    if (handler) {
        return handler->pluginWebViewAttach(pluginId, accountId, webViewId, action);
    }

    JAMI_ERR("handler pointer is null for pluginId: %s!", pluginId.c_str());
    return "";
}

void
WebViewServicesManager::sendWebViewDetach(const std::string& pluginId,
                                          const std::string& accountId,
                                          const std::string& webViewId)
{
    auto* handler = getWebViewHandlerPointer(pluginId);

    if (handler) {
        handler->pluginWebViewDetach(pluginId, accountId, webViewId);
    } else {
        JAMI_ERR("handler pointer is null for pluginId: %s!", pluginId.c_str());
    }
}

} // namespace jami
