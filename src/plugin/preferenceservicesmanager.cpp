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

#include "preferenceservicesmanager.h"

#include "pluginmanager.h"

#include <algorithm>

namespace jami {

PreferenceServicesManager::PreferenceServicesManager(PluginManager& pluginManager)
{
    registerComponentsLifeCycleManagers(pluginManager);
}

PreferenceServicesManager::~PreferenceServicesManager()
{
    handlers_.clear();
}

std::vector<std::string>
PreferenceServicesManager::getHandlers() const
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    std::vector<std::string> res;
    res.reserve(handlers_.size());
    for (const auto& preferenceHandler : handlers_) {
        res.emplace_back(std::to_string((uintptr_t) preferenceHandler.get()));
    }
    return res;
}

std::map<std::string, std::string>
PreferenceServicesManager::getHandlerDetails(const std::string& preferenceHandlerIdStr) const
{
    auto operation = operationState_.acquire();
    auto preferenceHandlerId = std::stoull(preferenceHandlerIdStr);
    PreferenceHandler* handler = nullptr;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (const auto& preferenceHandler : handlers_) {
            if (reinterpret_cast<uintptr_t>(preferenceHandler.get()) == preferenceHandlerId) {
                handler = preferenceHandler.get();
                break;
            }
        }
    }
    if (handler)
        return handler->getHandlerDetails();
    return {};
}

bool
PreferenceServicesManager::setPreference(const std::string& key,
                                         const std::string& value,
                                         const std::string& rootPath,
                                         const std::string& accountId)
{
    auto operation = operationState_.acquire();
    std::vector<PreferenceHandler*> handlers;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& preferenceHandler : handlers_) {
            if (preferenceHandler->id().find(rootPath) != std::string::npos)
                handlers.emplace_back(preferenceHandler.get());
        }
    }

    bool status {true};
    for (auto* preferenceHandler : handlers) {
        if (preferenceHandler->preferenceMapHasKey(key)) {
            preferenceHandler->setPreferenceAttribute(accountId, key, value);
            return false;
        }
    }
    return status;
}

void
PreferenceServicesManager::resetPreferences(const std::string& rootPath, const std::string& accountId)
{
    auto operation = operationState_.acquire();
    std::vector<PreferenceHandler*> handlers;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& preferenceHandler : handlers_) {
            if (preferenceHandler->id().find(rootPath) != std::string::npos)
                handlers.emplace_back(preferenceHandler.get());
        }
    }
    for (auto* preferenceHandler : handlers) {
        preferenceHandler->resetPreferenceAttributes(accountId);
    }
}

void
PreferenceServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // registerHandler may be called by the PluginManager upon loading a plugin.
    auto registerHandler = [this](void* data, std::mutex&) {
        PreferenceHandlerPtr ptr {(static_cast<PreferenceHandler*>(data))};

        if (!ptr)
            return -1;
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.waitUntilReady(lk);
        handlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterHandler may be called by the PluginManager while unloading.
    auto unregisterHandler = [this](void* data, std::mutex&) {
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.beginUnload(lk);
        auto handlerIt = std::find_if(handlers_.begin(), handlers_.end(), [data](PreferenceHandlerPtr& handler) {
            return (handler.get() == data);
        });

        if (handlerIt != handlers_.end())
            handlers_.erase(handlerIt);
        operationState_.endUnload(lk);
        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("PreferenceHandlerManager", registerHandler, unregisterHandler);
}
} // namespace jami
