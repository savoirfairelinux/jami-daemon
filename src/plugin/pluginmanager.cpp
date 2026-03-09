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
    bool alreadyLoaded = false;
    {
        std::lock_guard lk(mutex_);
        alreadyLoaded = dynPluginMap_.contains(path);
    }
    if (alreadyLoaded) {
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
    {
        std::lock_guard lk(mutex_);
        dynPluginMap_[path] = {std::move(plugin), true};
    }
    return true;
}

bool
PluginManager::unload(const std::string& path)
{
    destroyPluginComponents(path);
    JAMI_PluginExitFunc exitFunc = nullptr;
    std::shared_ptr<Plugin> pluginKeeper;
    {
        std::lock_guard lk(mutex_);
        auto it = dynPluginMap_.find(path);
        if (it == dynPluginMap_.end())
            return true;
        pluginKeeper = it->second.first;
        if (auto exitIt = exitFunc_.find(path); exitIt != exitFunc_.end()) {
            exitFunc = exitIt->second;
            exitFunc_.erase(exitIt);
        }
        dynPluginMap_.erase(it);
    }

    if (exitFunc) {
        try {
            exitFunc();
        } catch (...) {
            JAMI_ERR() << "Exception caught during plugin exit";
        }
    }
    pluginKeeper.reset();

    return true;
}

bool
PluginManager::checkLoadedPlugin(const std::string& rootPath) const
{
    std::lock_guard lk(mutex_);
    for (const auto& item : dynPluginMap_) {
        if (item.first.find(rootPath) != std::string::npos && item.second.second)
            return true;
    }
    return false;
}

std::vector<std::string>
PluginManager::getLoadedPlugins() const
{
    std::lock_guard lk(mutex_);
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
    ComponentPtrList components;
    {
        std::lock_guard lk(mutex_);
        if (auto itComponents = pluginComponentsMap_.find(path); itComponents != pluginComponentsMap_.end()) {
            components = std::move(itComponents->second);
            pluginComponentsMap_.erase(itComponents);
        }
    }

    for (auto& component : components) {
        ComponentFunction destroyComponent;
        {
            std::lock_guard lk(mutex_);
            auto clcm = componentsLifeCycleManagers_.find(component.first);
            if (clcm == componentsLifeCycleManagers_.end())
                continue;
            destroyComponent = clcm->second.destroyComponent;
        }
        destroyComponent(component.second, mutex_);
    }
}

bool
PluginManager::callPluginInitFunction(const std::string& path)
{
    bool returnValue {false};
    std::shared_ptr<Plugin> loadedPlugin;
    {
        std::lock_guard lk(mutex_);
        auto it = dynPluginMap_.find(path);
        if (it != dynPluginMap_.end())
            loadedPlugin = it->second.first;
    }
    if (loadedPlugin) {
        // Since the Plugin was found it's of type DLPlugin with a valid init symbol
        std::shared_ptr<DLPlugin> plugin = std::static_pointer_cast<DLPlugin>(loadedPlugin);
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
            // emit signal with error message to let user know that jamid was unable to load plugin
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
    DLPlugin* pluginPtr = dynamic_cast<DLPlugin*>(plugin.get());
    JAMI_PluginExitFunc exitFunc = nullptr;

    pluginPtr->apiContext_ = this;
    pluginPtr->api_.version = {JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION};
    pluginPtr->api_.registerObjectFactory = registerObjectFactory_;

    // Implements JAMI_PluginAPI.invokeService().
    // Must be C accessible.
    pluginPtr->api_.invokeService = [](const JAMI_PluginAPI* api, const char* name, void* data) {
        auto* plugin = static_cast<DLPlugin*>(api->context);
        auto* manager = reinterpret_cast<PluginManager*>(plugin->apiContext_);
        if (!manager) {
            JAMI_ERR() << "invokeService called with null plugin API";
            return -1;
        }

        return manager->invokeService(plugin, name, data);
    };

    // Implements JAMI_PluginAPI.manageComponents().
    // Must be C accessible.
    pluginPtr->api_.manageComponent = [](const JAMI_PluginAPI* api, const char* name, void* data) {
        auto* plugin = static_cast<DLPlugin*>(api->context);
        if (!plugin) {
            JAMI_ERR() << "createComponent called with null context";
            return -1;
        }
        auto* manager = reinterpret_cast<PluginManager*>(plugin->apiContext_);
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

    {
        std::lock_guard lk(mutex_);
        exitFunc_[pluginPtr->getPath()] = exitFunc;
    }
    return true;
}

bool
PluginManager::registerService(const std::string& name, ServiceFunction&& func)
{
    std::lock_guard lk(mutex_);
    services_[name] = std::forward<ServiceFunction>(func);
    return true;
}

void
PluginManager::unRegisterService(const std::string& name)
{
    std::lock_guard lk(mutex_);
    services_.erase(name);
}

int32_t
PluginManager::invokeService(const DLPlugin* plugin, const std::string& name, void* data)
{
    // Search if desired service exists
    ServiceFunction func;
    {
        std::lock_guard lk(mutex_);
        const auto& iterFunc = services_.find(name);
        if (iterFunc == services_.cend()) {
            JAMI_ERR() << "Services not found: " << name;
            return -1;
        }
        func = iterFunc->second;
    }

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
    ComponentLifeCycleManager componentLifecycleManager;
    {
        std::lock_guard lk(mutex_);
        const auto& iter = componentsLifeCycleManagers_.find(name);
        if (iter == componentsLifeCycleManagers_.end()) {
            JAMI_ERR() << "Component lifecycle manager not found: " << name;
            return -1;
        }
        componentLifecycleManager = iter->second;
    }

    try {
        int32_t result = componentLifecycleManager.takeComponentOwnership(data, mutex_);
        if (result == 0) {
            std::lock_guard lk(mutex_);
            pluginComponentsMap_[plugin->getPath()].emplace_back(name, data);
        }
        return result;
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
    std::lock_guard lk(mutex_);

    // Wildcard registration
    if (key == "*") {
        wildCardVec_.push_back(factory);
        return true;
    }

    // Fails on duplicate for exactMatch map
    if (exactMatchMap_.contains(key))
        return false;

    exactMatchMap_[key] = factory;
    return true;
}

bool
PluginManager::registerComponentManager(const std::string& name,
                                        ComponentFunction&& takeOwnership,
                                        ComponentFunction&& destroyComponent)
{
    std::lock_guard lk(mutex_);
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
    ObjectFactory factory;
    bool hasExactFactory = false;
    ObjectFactoryVec wildCardFactories;
    {
        std::lock_guard lk(mutex_);
        if (const auto factoryIter = exactMatchMap_.find(type); factoryIter != exactMatchMap_.end()) {
            factory = factoryIter->second;
            hasExactFactory = true;
        } else {
            wildCardFactories = wildCardVec_;
        }
    }

    if (hasExactFactory) {
        auto* object = factory.data.create(&op, factory.data.closure);
        if (object)
            return {object, factory.deleter};
    }

    // Try to find a wildcard match
    for (const auto& wildCardFactory : wildCardFactories) {
        auto* object = wildCardFactory.data.create(&op, wildCardFactory.data.closure);
        if (object) {
            // Promote registration to exactMatch_
            // (but keep also wildcard registration for other object types)
            int32_t res = registerObjectFactory(op.type, wildCardFactory.data);
            if (res < 0) {
                JAMI_ERR() << "failed to register object " << op.type;
                return {nullptr, nullptr};
            }

            return {object, wildCardFactory.deleter};
        }
    }

    return {nullptr, nullptr};
}

int32_t
PluginManager::registerObjectFactory_(const JAMI_PluginAPI* api, const char* type, void* data)
{
    auto* manager = reinterpret_cast<PluginManager*>(api->context);
    if (!manager) {
        JAMI_ERR() << "registerObjectFactory called with null plugin API";
        return -1;
    }

    if (!data) {
        JAMI_ERR() << "registerObjectFactory called with null factory data";
        return -1;
    }

    auto* const factory = reinterpret_cast<JAMI_PluginObjectFactory*>(data);
    return manager->registerObjectFactory(type, *factory) ? 0 : -1;
}
} // namespace jami
