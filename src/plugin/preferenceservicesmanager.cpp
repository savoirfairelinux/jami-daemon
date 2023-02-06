/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#include "preferenceservicesmanager.h"

#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "manager.h"
#include "sip/sipcall.h"
#include "fileutils.h"
#include "logger.h"

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
    auto preferenceHandlerId = std::stoull(preferenceHandlerIdStr);
    for (auto& preferenceHandler : handlers_) {
        if ((uintptr_t) preferenceHandler.get() == preferenceHandlerId) {
            return preferenceHandler->getHandlerDetails();
        }
    }
    return {};
}

bool
PreferenceServicesManager::setPreference(const std::string& key,
                                         const std::string& value,
                                         const std::string& rootPath,
                                         const std::string& accountId)
{
    bool status {true};
    for (auto& preferenceHandler : handlers_) {
        if (preferenceHandler->id().find(rootPath) != std::string::npos) {
            if (preferenceHandler->preferenceMapHasKey(key)) {
                preferenceHandler->setPreferenceAttribute(accountId, key, value);
                // We can return here since we expect plugins to have a single preferencehandler
                return false;
            }
        }
    }
    return status;
}

void
PreferenceServicesManager::resetPreferences(const std::string& rootPath,
                                            const std::string& accountId)
{
    for (auto& preferenceHandler : handlers_) {
        if (preferenceHandler->id().find(rootPath) != std::string::npos) {
            preferenceHandler->resetPreferenceAttributes(accountId);
        }
    }
}

void
PreferenceServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // registerHandler may be called by the PluginManager upon loading a plugin.
    auto registerHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
        PreferenceHandlerPtr ptr {(static_cast<PreferenceHandler*>(data))};

        if (!ptr)
            return -1;
        handlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterHandler may be called by the PluginManager while unloading.
    auto unregisterHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
        auto handlerIt = std::find_if(handlers_.begin(),
                                      handlers_.end(),
                                      [data](PreferenceHandlerPtr& handler) {
                                          return (handler.get() == data);
                                      });

        if (handlerIt != handlers_.end()) {
            handlers_.erase(handlerIt);
        }
        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("PreferenceHandlerManager",
                                           registerHandler,
                                           unregisterHandler);
}
} // namespace jami
