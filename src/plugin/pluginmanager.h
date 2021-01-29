/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <list>

#include <inttypes.h>

namespace jami {

class Plugin;

class PluginManager
{
public:
    using ObjectDeleter = std::function<void(void*)>;
    using ServiceFunction = std::function<int32_t(const DLPlugin*, void*)>;
    using ComponentFunction = std::function<int32_t(void*)>;
    // A vector to a pair<componentType, componentPtr>
    using ComponentTypePtrVector = std::list<std::pair<std::string, void*>>;

private:
    struct ObjectFactory
    {
        JAMI_PluginObjectFactory data;
        ObjectDeleter deleter;
    };

    struct ComponentLifeCycleManager
    {
        ComponentFunction takeComponentOwnership;
        ComponentFunction destroyComponent;
    };

    using PluginMap = std::map<std::string, std::pair<std::shared_ptr<Plugin>, bool>>;
    using PluginComponentsMap = std::map<std::string, ComponentTypePtrVector>;
    using ExitFuncVec = std::vector<JAMI_PluginExitFunc>;
    using ObjectFactoryVec = std::vector<ObjectFactory>;
    using ObjectFactoryMap = std::map<std::string, ObjectFactory>;

public:
    PluginManager();
    ~PluginManager();

    /**
     * Load a dynamic plugin by filename.
     *
     * @param path fully qualified pathname on a loadable plugin binary
     * @return true if success
     */
    bool load(const std::string& path);

    /**
     * @brief unloads the plugin with pathname path
     * @param path
     * @return true if success
     */
    bool unload(const std::string& path);

    /**
     * @brief getLoadedPlugins
     * @return vector of strings of so files of the loaded plugins
     */
    std::vector<std::string> getLoadedPlugins() const;

    /**
     * @brief checkLoadedPlugin
     * @return bool True if plugin is loaded, false otherwise
     */
    bool checkLoadedPlugin(const std::string& rootPath) const;

    /**
     * @brief destroyPluginComponents
     * @param path
     */
    void destroyPluginComponents(const std::string& path);

    /**
     * @brief callPluginInitFunction
     * @param path: plugin path used as an id in the plugin map
     * @return true if succes
     */
    bool callPluginInitFunction(const std::string& path);
    /**
     * Register a plugin.
     *
     * @param initFunc plugin init function
     * @return true if success
     */
    bool registerPlugin(std::unique_ptr<Plugin>& plugin);

    /**
     * Register a new service for plugin.
     *
     * @param name The service name
     * @param func The function called by Ring_PluginAPI.invokeService
     * @return true if success
     */
    bool registerService(const std::string& name, ServiceFunction&& func);

    void unRegisterService(const std::string& name);

    /**
     * Register a new public objects factory.
     *
     * @param type unique identifier of the object
     * @param params object factory details
     * @return true if success
     *
     * Note: type can be the string "*" meaning that the factory
     * will be called if no exact match factories are found for a given type.
     */
    bool registerObjectFactory(const char* type, const JAMI_PluginObjectFactory& factory);
    /**
     * @brief registerComponentManager
     * Registers a component manager that will have two functions, one to take
     * ownership of the component and the other one to destroy it
     * @param name : name of the component manager
     * @param takeOwnership function that takes ownership on created objet in memory
     * @param destroyComponent desotry the component
     * @return true if success
     */
    bool registerComponentManager(const std::string& name,
                                  ComponentFunction&& takeOwnership,
                                  ComponentFunction&& destroyComponent);

    /**
     * Create a new plugin's exported object.
     *
     * @param type unique identifier of the object to create.
     * @return unique pointer on created object.
     */
    std::unique_ptr<void, ObjectDeleter> createObject(const std::string& type);

private:
    NON_COPYABLE(PluginManager);

    /**
     * Implements JAMI_PluginAPI.registerObjectFactory().
     * Must be C accessible.
     */
    static int32_t registerObjectFactory_(const JAMI_PluginAPI* api, const char* type, void* data);
    int32_t invokeService(const DLPlugin* plugin, const std::string& name, void* data);

    int32_t manageComponent(const DLPlugin* plugin, const std::string& name, void* data);

    std::mutex mutex_ {};
    JAMI_PluginAPI pluginApi_ = {{JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION},
                                 nullptr, // set by PluginManager constructor
                                 registerObjectFactory_,
                                 nullptr,
                                 nullptr};
    PluginMap dynPluginMap_ {}; // Only dynamic loaded plugins
    ExitFuncVec exitFuncVec_ {};
    ObjectFactoryMap exactMatchMap_ {};
    ObjectFactoryVec wildCardVec_ {};

    // Storage used during plugin initialisation.
    // Will be copied into previous ones only if the initialisation success.
    ObjectFactoryMap tempExactMatchMap_ {};
    ObjectFactoryVec tempWildCardVec_ {};

    // registered services
    std::map<std::string, ServiceFunction> services_ {};
    // registered component lifecycle managers
    std::map<std::string, ComponentLifeCycleManager> componentsLifeCycleManagers_ {};

    // references to plugins components, used for cleanup
    PluginComponentsMap pluginComponentsMap_ {};
};
} // namespace jami
