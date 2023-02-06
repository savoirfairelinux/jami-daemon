/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#include "pluginmanager.h"
#include "logger.h"

#include <utility>

namespace jami {

PluginManager::PluginManager()
{
    pluginApi_.context = reinterpret_cast<void*>(this);
}

PluginManager::~PluginManager()
{
    for (auto& func : exitFunc_) {
        try {
            func.second();
        } catch (...) {
            JAMI_ERR() << "Exception caught during plugin exit";
        }
    }

    dynPluginMap_.clear();
    exactMatchMap_.clear();
    wildCardVec_.clear();
    exitFunc_.clear();
}

bool
PluginManager::load(const std::string& path)
{
    auto it = dynPluginMap_.find(path);
    if (it != dynPluginMap_.end()) {
        unload(path);
    }

    std::string error;
    // Load plugin library
    std::unique_ptr<Plugin> plugin(Plugin::load(path, error));
    if (!plugin) {
        JAMI_ERR() << "Plugin: " << error;
        return false;
    }

    // Get init function from loaded library
    const auto& init_func = plugin->getInitFunction();
    if (!init_func) {
        JAMI_ERR() << "Plugin: no init symbol" << error;
        return false;
    }

    // Register plugin by running init function
    if (!registerPlugin(plugin))
        return false;

    // Put Plugin loader into loaded plugins Map.
    dynPluginMap_[path] = {std::move(plugin), true};
    return true;
}

bool
PluginManager::unload(const std::string& path)
{
    destroyPluginComponents(path);
    auto it = dynPluginMap_.find(path);
    if (it != dynPluginMap_.end()) {
        std::lock_guard<std::mutex> lk(mtx_);
        exitFunc_[path]();
        dynPluginMap_.erase(it);
        exitFunc_.erase(path);
    }

    return true;
}

bool
PluginManager::checkLoadedPlugin(const std::string& rootPath) const
{
    for (const auto& item : dynPluginMap_) {
        if (item.first.find(rootPath) != std::string::npos && item.second.second)
            return true;
    }
    return false;
}

std::vector<std::string>
PluginManager::getLoadedPlugins() const
{
    std::vector<std::string> res {};
    for (const auto& pair : dynPluginMap_) {
        if (pair.second.second)
            res.push_back(pair.first);
    }
    return res;
}

void
PluginManager::destroyPluginComponents(const std::string& path)
{
    auto itComponents = pluginComponentsMap_.find(path);
    if (itComponents != pluginComponentsMap_.end()) {
        for (auto pairIt = itComponents->second.begin(); pairIt != itComponents->second.end();) {
            auto clcm = componentsLifeCycleManagers_.find(pairIt->first);
            if (clcm != componentsLifeCycleManagers_.end()) {
                clcm->second.destroyComponent(pairIt->second, mtx_);
                pairIt = itComponents->second.erase(pairIt);
            }
        }
    }
}

bool
PluginManager::callPluginInitFunction(const std::string& path)
{
    bool returnValue {false};
    auto it = dynPluginMap_.find(path);
    if (it != dynPluginMap_.end()) {
        // Since the Plugin was found it's of type DLPlugin with a valid init symbol
        std::shared_ptr<DLPlugin> plugin = std::static_pointer_cast<DLPlugin>(it->second.first);
        const auto& initFunc = plugin->getInitFunction();
        JAMI_PluginExitFunc exitFunc = nullptr;

        try {
            // Call Plugin Init function
            exitFunc = initFunc(&plugin->api_);
        } catch (const std::runtime_error& e) {
            JAMI_ERR() << e.what();
            return false;
        }

        if (!exitFunc) {
            JAMI_ERR() << "Plugin: init failed";
            // emit signal with error message to let user know that jamid could not load plugin
            returnValue = false;
        } else {
            returnValue = true;
        }
    }

    return returnValue;
}

bool
PluginManager::registerPlugin(std::unique_ptr<Plugin>& plugin)
{
    // Here we already know that Plugin is of type DLPlugin with a valid init symbol
    const auto& initFunc = plugin->getInitFunction();
    DLPlugin* pluginPtr = static_cast<DLPlugin*>(plugin.get());
    JAMI_PluginExitFunc exitFunc = nullptr;

    pluginPtr->apiContext_ = this;
    pluginPtr->api_.version = {JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION};
    pluginPtr->api_.registerObjectFactory = registerObjectFactory_;

    // Implements JAMI_PluginAPI.invokeService().
    // Must be C accessible.
    pluginPtr->api_.invokeService = [](const JAMI_PluginAPI* api, const char* name, void* data) {
        auto plugin = static_cast<DLPlugin*>(api->context);
        auto manager = reinterpret_cast<PluginManager*>(plugin->apiContext_);
        if (!manager) {
            JAMI_ERR() << "invokeService called with null plugin API";
            return -1;
        }

        return manager->invokeService(plugin, name, data);
    };

    // Implements JAMI_PluginAPI.manageComponents().
    // Must be C accessible.
    pluginPtr->api_.manageComponent = [](const JAMI_PluginAPI* api, const char* name, void* data) {
        auto plugin = static_cast<DLPlugin*>(api->context);
        if (!plugin) {
            JAMI_ERR() << "createComponent called with null context";
            return -1;
        }
        auto manager = reinterpret_cast<PluginManager*>(plugin->apiContext_);
        if (!manager) {
            JAMI_ERR() << "createComponent called with null plugin API";
            return -1;
        }
        return manager->manageComponent(plugin, name, data);
    };

    try {
        exitFunc = initFunc(&pluginPtr->api_);
    } catch (const std::runtime_error& e) {
        JAMI_ERR() << e.what();
    }

    if (!exitFunc) {
        JAMI_ERR() << "Plugin: init failed";
        return false;
    }

    exitFunc_[pluginPtr->getPath()] = exitFunc;
    return true;
}

bool
PluginManager::registerService(const std::string& name, ServiceFunction&& func)
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
PluginManager::invokeService(const DLPlugin* plugin, const std::string& name, void* data)
{
    // Search if desired service exists
    const auto& iterFunc = services_.find(name);
    if (iterFunc == services_.cend()) {
        JAMI_ERR() << "Services not found: " << name;
        return -1;
    }

    const auto& func = iterFunc->second;

    try {
        // Call service with data
        return func(plugin, data);
    } catch (const std::runtime_error& e) {
        JAMI_ERR() << e.what();
        return -1;
    }
}

int32_t
PluginManager::manageComponent(const DLPlugin* plugin, const std::string& name, void* data)
{
    const auto& iter = componentsLifeCycleManagers_.find(name);
    if (iter == componentsLifeCycleManagers_.end()) {
        JAMI_ERR() << "Component lifecycle manager not found: " << name;
        return -1;
    }

    const auto& componentLifecycleManager = iter->second;

    try {
        int32_t r = componentLifecycleManager.takeComponentOwnership(data, mtx_);
        if (r == 0) {
            pluginComponentsMap_[plugin->getPath()].emplace_back(name, data);
        }
        return r;
    } catch (const std::runtime_error& e) {
        JAMI_ERR() << e.what();
        return -1;
    }
}

bool
PluginManager::registerObjectFactory(const char* type, const JAMI_PluginObjectFactory& factoryData)
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

    // Wildcard registration
    if (key == "*") {
        wildCardVec_.push_back(factory);
        return true;
    }

    // Fails on duplicate for exactMatch map
    if (exactMatchMap_.find(key) != exactMatchMap_.end())
        return false;

    exactMatchMap_[key] = factory;
    return true;
}

bool
PluginManager::registerComponentManager(const std::string& name,
                                        ComponentFunction&& takeOwnership,
                                        ComponentFunction&& destroyComponent)
{
    componentsLifeCycleManagers_[name] = {std::forward<ComponentFunction>(takeOwnership),
                                          std::forward<ComponentFunction>(destroyComponent)};
    return true;
}

std::unique_ptr<void, PluginManager::ObjectDeleter>
PluginManager::createObject(const std::string& type)
{
    if (type == "*")
        return {nullptr, nullptr};

    JAMI_PluginObjectParams op = {
        /*.pluginApi = */ &pluginApi_,
        /*.type = */ type.c_str(),
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
    for (const auto& factory : wildCardVec_) {
        auto object = factory.data.create(&op, factory.data.closure);
        if (object) {
            // Promote registration to exactMatch_
            // (but keep also wildcard registration for other object types)
            int32_t res = registerObjectFactory(op.type, factory.data);
            if (res < 0) {
                JAMI_ERR() << "failed to register object " << op.type;
                return {nullptr, nullptr};
            }

            return {object, factory.deleter};
        }
    }

    return {nullptr, nullptr};
}

int32_t
PluginManager::registerObjectFactory_(const JAMI_PluginAPI* api, const char* type, void* data)
{
    auto manager = reinterpret_cast<PluginManager*>(api->context);
    if (!manager) {
        JAMI_ERR() << "registerObjectFactory called with null plugin API";
        return -1;
    }

    if (!data) {
        JAMI_ERR() << "registerObjectFactory called with null factory data";
        return -1;
    }

    const auto factory = reinterpret_cast<JAMI_PluginObjectFactory*>(data);
    return manager->registerObjectFactory(type, *factory) ? 0 : -1;
}
} // namespace jami
