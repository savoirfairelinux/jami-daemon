/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "plugin_manager.h"
#include "plugin_loader.h"
#include "logger.h"

#include <utility>

PluginManager::PluginManager()
{
    pluginApi_.context = reinterpret_cast<void*>(this);
}

PluginManager::~PluginManager()
{
    int32_t error = 0;

    for (auto func : exitFuncVec_) {
        try {
            error |= (*func)();
        } catch (...) {
            error = -1;
        }
    }

    if (error)
        SFL_WARN("Some plugins have reported failure at exit");

    dynPluginMap_.clear();
    exactMatchMap_.clear();
    wildCardVec_.clear();
    exitFuncVec_.clear();
}

bool
PluginManager::load(const std::string& path)
{
    // TODO: Resolve symbolic links and make path absolute

    // Don't load the same dynamic library twice
    if (dynPluginMap_.find(path) != dynPluginMap_.end()) {
        SFL_WARN("plugin: already loaded");
        return true;
    }

    std::string error;
    std::unique_ptr<Plugin> plugin(Plugin::load(path, error));
    if (!plugin) {
        SFL_ERR("plugin: %s", error.c_str());
        return false;
    }

    const auto& init_func = plugin->getInitFunction();
    if (!init_func) {
        SFL_ERR("plugin: no init symbol");
        return false;
    }

    if (!registerPlugin(init_func))
        return false;

    dynPluginMap_[path] = std::move(plugin);
    return true;
}

bool
PluginManager::registerPlugin(RING_PluginInitFunc initFunc)
{
    RING_PluginExitFunc exitFunc = nullptr;

    try {
        exitFunc = initFunc(&pluginApi_);
    } catch (const std::runtime_error& e) {
        SFL_ERR("%s", e.what());
    }

    if (!exitFunc) {
        tempExactMatchMap_.clear();
        tempWildCardVec_.clear();
        SFL_ERR("plugin: init failed");
        return false;
    }

    exitFuncVec_.push_back(exitFunc);
    exactMatchMap_.insert(tempExactMatchMap_.begin(),
                          tempExactMatchMap_.end());
    wildCardVec_.insert(wildCardVec_.end(),
                        tempWildCardVec_.begin(),
                        tempWildCardVec_.end());
    return true;
}

bool
PluginManager::registerService(const std::string& name,
                               ServiceFunction&& func)
{
    services_[name] = std::forward<ServiceFunction>(func);
    return true;
}

void
PluginManager::unRegisterService(const std::string& name)
{
    services_.erase(name);
}

int32_t
PluginManager::invokeService(const std::string& name, void* data)
{
    const auto& iterFunc = services_.find(name);
    if (iterFunc == services_.cend()) {
        SFL_ERR("Services not found: %s", name.c_str());
        return -1;
    }

    const auto& func = iterFunc->second;

    try {
        return func(data);
    } catch (const std::runtime_error &e) {
        SFL_ERR("%s", e.what());
        return -1;
    }
}

/* WARNING: exposed to plugins through RING_PluginAPI */
bool
PluginManager::registerObjectFactory(const char* type,
                                     const RING_PluginObjectFactory& factoryData)
{
    if (!type)
        return false;

    if (!factoryData.create || !factoryData.destroy)
        return false;

    // Strict compatibility on ABI
    if (factoryData.version.abi != pluginApi_.version.abi)
        return false;

    // Backward compatibility on API
    if (factoryData.version.api < pluginApi_.version.api)
        return false;

    const std::string key(type);
    auto deleter = [factoryData](void* o) {
        factoryData.destroy(o, factoryData.closure);
    };
    ObjectFactory factory = {factoryData, deleter};

    // wildcard registration?
    if (key == "*") {
        wildCardVec_.push_back(factory);
        return true;
    }

    // fails on duplicate for exactMatch map
    if (exactMatchMap_.find(key) != exactMatchMap_.end())
        return false;

    exactMatchMap_[key] = factory;
    return true;
}

std::unique_ptr<void, PluginManager::ObjectDeleter>
PluginManager::createObject(const std::string& type)
{
    if (type == "*")
        return {nullptr, nullptr};

    RING_PluginObjectParams op = {
        .pluginApi = &pluginApi_,
        .type = type.c_str(),
    };

    // Try to find an exact match
    const auto& factoryIter = exactMatchMap_.find(type);
    if (factoryIter != exactMatchMap_.end()) {
        const auto& factory = factoryIter->second;
        auto object = factory.data.create(&op, factory.data.closure);
        if (object)
            return {object, factory.deleter};
    }

    // Try to find a wildcard match
    for (const auto& factory : wildCardVec_)
    {
        auto object = factory.data.create(&op, factory.data.closure);
        if (object) {
            // promote registration to exactMatch_
            // (but keep also wildcard registration for other object types)
            int32_t res = registerObjectFactory(op.type, factory.data);
            if (res < 0) {
                SFL_ERR("failed to register object %s", op.type);
                return {nullptr, nullptr};
            }

            return {object, factory.deleter};
        }
    }

    return {nullptr, nullptr};
}

/* WARNING: exposed to plugins through RING_PluginAPI */
int32_t
PluginManager::registerObjectFactory_(const RING_PluginAPI* api,
                                      const char* type, void* data)
{
    auto manager = reinterpret_cast<PluginManager*>(api->context);
    if (!manager) {
        SFL_ERR("registerObjectFactory called with null plugin API");
        return -1;
    }

    if (!data) {
        SFL_ERR("registerObjectFactory called with null factory data");
        return -1;
    }

    const auto factory = reinterpret_cast<RING_PluginObjectFactory*>(data);
    return manager->registerObjectFactory(type, *factory) ? 0 : -1;
}

/* WARNING: exposed to plugins through RING_PluginAPI */
int32_t
PluginManager::invokeService_(const RING_PluginAPI* api, const char* name,
                              void* data)
{
    auto manager = reinterpret_cast<PluginManager*>(api->context);
    if (!manager) {
        SFL_ERR("invokeService called with null plugin API");
        return -1;
    }

    return manager->invokeService(name, data);
}
