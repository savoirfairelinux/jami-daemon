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
#pragma once

#include "noncopyable.h"
#include "jamiplugin.h"
#include "pluginloader.h"

#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <list>

#include <inttypes.h>

namespace jami {

/**
 * @class  PluginManager
 * @brief This class manages plugin (un)loading. Those process include:
 * (1) plugin libraries (un)loading;
 * (2) call plugin initial function;
 * (3) handlers registration and destruction, and;
 * (4) services registration.
 */
class PluginManager
{
public:
    PluginManager();
    ~PluginManager();

private:
    using ObjectDeleter = std::function<void(void*)>;

    // Function that may be called from plugin implementation
    using ServiceFunction = std::function<int32_t(const DLPlugin*, void*)>;

    // A Component is either a MediaHandler or a ChatHandler.
    // A ComponentFunction is a function that may start or end a component life.
    using ComponentFunction = std::function<int32_t(void*, std::mutex&)>;

    // A list of component type (MediaHandler or ChatHandler), and component pointer pairs
    using ComponentPtrList = std::list<std::pair<std::string, void*>>;

    struct ObjectFactory
    {
        JAMI_PluginObjectFactory data;
        ObjectDeleter deleter;
    };

    /**
     * @struct ComponentLifeCycleManager
     * @brief Component functions for registration and destruction.
     */
    struct ComponentLifeCycleManager
    {
        // Register component to servicesmanager
        ComponentFunction takeComponentOwnership;

        // Destroys component in servicesmanager
        ComponentFunction destroyComponent;
    };

    // Map between plugin's library path and loader pointer
    using PluginMap = std::map<std::string, std::pair<std::shared_ptr<Plugin>, bool>>;

    // Map between plugins' library path and their components list
    using PluginComponentsMap = std::map<std::string, ComponentPtrList>;

    // Map with plugins' destruction functions
    using ExitFuncMap = std::map<std::string, JAMI_PluginExitFunc>;
    using ObjectFactoryVec = std::vector<ObjectFactory>;
    using ObjectFactoryMap = std::map<std::string, ObjectFactory>;

public:
    /**
     * @brief Load a dynamic plugin by filename.
     * @param path fully qualified pathname on a loadable plugin binary
     * @return True if success
     */
    bool load(const std::string& path);

    /**
     * @brief Unloads the plugin
     * @param path
     * @return True if success
     */
    bool unload(const std::string& path);

    /**
     * @brief Returns vector with loaded plugins' libraries paths
     */
    std::vector<std::string> getLoadedPlugins() const;

    /**
     * @brief Returns True if plugin is loaded
     */
    bool checkLoadedPlugin(const std::string& rootPath) const;

    /**
     * @brief Register a new service in the Plugin System.
     * @param name The service name
     * @param func The function that may be called by Ring_PluginAPI.invokeService
     * @return True if success
     */
    bool registerService(const std::string& name, ServiceFunction&& func);

    /**
     * @brief Unregister a service from the Plugin System.
     * @param name The service name
     */
    void unRegisterService(const std::string& name);

    /**
     * @brief Function called from plugin implementation register a new object factory.
     *
     * Note: type can be the string "*" meaning that the factory
     * will be called if no exact match factories are found for a given type.
     * @param type unique identifier of the object
     * @param params object factory details
     * @return True if success
     */
    bool registerObjectFactory(const char* type, const JAMI_PluginObjectFactory& factory);

    /**
     * @brief Registers a component manager that will have two functions, one to take
     * ownership of the component and the other one to destroy it
     * @param name name of the component manager
     * @param takeOwnership function that takes ownership on created objet in memory
     * @param destroyComponent destroy the component
     * @return True if success
     */
    bool registerComponentManager(const std::string& name,
                                  ComponentFunction&& takeOwnership,
                                  ComponentFunction&& destroyComponent);

private:
    NON_COPYABLE(PluginManager);

    /**
     * @brief Untoggle and destroys all plugin's handlers from handlerservices
     * @param path
     */
    void destroyPluginComponents(const std::string& path);

    /**
     * @brief Returns True if success
     * @param path plugin path used as an id in the plugin map
     */
    bool callPluginInitFunction(const std::string& path);

    /**
     * @brief Returns True if success
     * @param initFunc plugin init function
     */
    bool registerPlugin(std::unique_ptr<Plugin>& plugin);

    /**
     * @brief Creates a new plugin's exported object.
     * @param type unique identifier of the object to create.
     * @return Unique pointer on created object.
     */
    std::unique_ptr<void, ObjectDeleter> createObject(const std::string& type);

    /**
     * WARNING: exposed to plugins through JAMI_PluginAPI
     * @brief Implements JAMI_PluginAPI.registerObjectFactory().
     * Must be C accessible.
     * @param api
     * @param type
     * @param data
     */
    static int32_t registerObjectFactory_(const JAMI_PluginAPI* api, const char* type, void* data);

    /** WARNING: exposed to plugins through JAMI_PluginAPI
     * @brief Function called from plugin implementation to perform a service.
     * @param name The service name
     */
    int32_t invokeService(const DLPlugin* plugin, const std::string& name, void* data);

    /**
     * WARNING: exposed to plugins through JAMI_PluginAPI
     * @brief Function called from plugin implementation to manage a component.
     * @param name The component type
     */
    int32_t manageComponent(const DLPlugin* plugin, const std::string& name, void* data);

    std::mutex mutex_ {};
    JAMI_PluginAPI pluginApi_ = {{JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION},
                                 nullptr, // set by PluginManager constructor
                                 registerObjectFactory_,
                                 nullptr,
                                 nullptr};
    // Keeps a map between plugin library path and a Plugin instance
    // for dynamically loaded plugins.
    PluginMap dynPluginMap_ {};

    // Should keep reference to plugins' destruction functions read during library loading.
    ExitFuncMap exitFunc_ {};

    ObjectFactoryMap exactMatchMap_ {};
    ObjectFactoryVec wildCardVec_ {};

    // Keeps a map between services names and service functions.
    std::map<std::string, ServiceFunction> services_ {};

    // Keeps a ComponentsLifeCycleManager for each available Handler API.
    std::map<std::string, ComponentLifeCycleManager> componentsLifeCycleManagers_ {};

    // Keeps a map between plugins' library path and their components list.
    PluginComponentsMap pluginComponentsMap_ {};

    std::mutex mtx_;
};
} // namespace jami
