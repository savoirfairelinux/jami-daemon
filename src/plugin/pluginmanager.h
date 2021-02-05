/*!
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
#include <mutex>
#include <list>

#include <inttypes.h>

namespace jami {

/*! \class  PluginManager
 * \brief This class manages plugin (un)loading. Those process include:
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

    /// Function that may be called from plugin implementation
    using ServiceFunction = std::function<int32_t(const DLPlugin*, void*)>;

    /// a Component is either a MediaHandler or a ChatHandler.
    /// o ComponentFunction is a function that may start or end a component life.
    using ComponentFunction = std::function<int32_t(void*)>;

    /// A list of component type (MediaHandler or ChatHandler), and component pointer pairs
    using ComponentPtrList = std::list<std::pair<std::string, void*>>;

    struct ObjectFactory
    {
        JAMI_PluginObjectFactory data;
        ObjectDeleter deleter;
    };

    /*! \struct ComponentLifeCycleManager
     * \brief Component functions for registration and destruction.
     */
    struct ComponentLifeCycleManager
    {
        ComponentFunction takeComponentOwnership; //!< register component to servicesmanager
        ComponentFunction destroyComponent;       //!< destroys component in servicesmanager
    };

    /// Map between plugin's library path and loader pointer
    using PluginMap = std::map<std::string, std::shared_ptr<Plugin>>;

    /// Map between plugins' library path and their components list
    using PluginComponentsMap = std::map<std::string, ComponentPtrList>;

    /// Vector with plugins' destruction functions
    using ExitFuncVec = std::vector<JAMI_PluginExitFunc>;
    using ObjectFactoryVec = std::vector<ObjectFactory>;
    using ObjectFactoryMap = std::map<std::string, ObjectFactory>;

public:
    bool load(const std::string& path);

    bool unload(const std::string& path);

    std::vector<std::string> getLoadedPlugins() const;

    bool checkLoadedPlugin(const std::string& rootPath) const;

    bool registerService(const std::string& name, ServiceFunction&& func);

    void unRegisterService(const std::string& name);

    bool registerObjectFactory(const char* type, const JAMI_PluginObjectFactory& factory);

    bool registerComponentManager(const std::string& name,
                                  ComponentFunction&& takeOwnership,
                                  ComponentFunction&& destroyComponent);

private:
    NON_COPYABLE(PluginManager);

    bool destroyPluginComponents(const std::string& path);

    bool callPluginInitFunction(const std::string& path);

    bool registerPlugin(std::unique_ptr<Plugin>& plugin);

    std::unique_ptr<void, ObjectDeleter> createObject(const std::string& type);

    static int32_t registerObjectFactory_(const JAMI_PluginAPI* api, const char* type, void* data);
    int32_t invokeService(const DLPlugin* plugin, const std::string& name, void* data);

    int32_t manageComponent(const DLPlugin* plugin, const std::string& name, void* data);

    std::mutex mutex_ {};
    JAMI_PluginAPI pluginApi_ = {{JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION},
                                 nullptr, // set by PluginManager constructor
                                 registerObjectFactory_,
                                 nullptr,
                                 nullptr};
    /// Keeps a map between plugin library path and a Plugin instance
    /// for dynamically loaded plugins.
    PluginMap dynPluginMap_ {};

    /// Should keep referen e to plugins' destructin functions read during library loading.
    ExitFuncVec exitFuncVec_ {};

    ObjectFactoryMap exactMatchMap_ {};
    ObjectFactoryVec wildCardVec_ {};

    /// Keeps a map between services names and service functions.
    std::map<std::string, ServiceFunction> services_ {};

    /// Keeps a ComponentsLifeCycleManager for each available Handler API.
    std::map<std::string, ComponentLifeCycleManager> componentsLifeCycleManagers_ {};

    /// Keeps a map between plugins' library path and their components list.
    PluginComponentsMap pluginComponentsMap_ {};
};
} // namespace jami
