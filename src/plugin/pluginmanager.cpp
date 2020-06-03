/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

PluginManager::PluginManager() {
  pluginApi_.context = reinterpret_cast<void *>(this);
}

PluginManager::~PluginManager() {
  for (auto func : exitFuncVec_) {
    try {
      (*func)();
    } catch (...) {
      JAMI_ERR() << "Exception caught during plugin exit";
    }
  }

  dynPluginMap_.clear();
  exactMatchMap_.clear();
  wildCardVec_.clear();
  exitFuncVec_.clear();
}

bool PluginManager::load(const std::string &path) {
  // Don't load the same dynamic library twice
  if (dynPluginMap_.find(path) != dynPluginMap_.end()) {
    JAMI_WARN() << "Plugin: already loaded";
    return true;
  }

  std::string error;
  std::unique_ptr<Plugin> plugin(Plugin::load(path, error));
  if (!plugin) {
    JAMI_ERR() << "Plugin: " << error;
    return false;
  }

  const auto &init_func = plugin->getInitFunction();
  if (!init_func) {
    JAMI_ERR() << "Plugin: no init symbol" << error;
    return false;
  }

  if (!registerPlugin(plugin))
    return false;

  dynPluginMap_[path] = std::move(plugin);
  return true;
}

bool PluginManager::unload(const std::string& path) {
    bool returnValue{false};
    destroyPluginComponents(path);
    PluginMap::iterator it = dynPluginMap_.find(path);
    if ( it != dynPluginMap_.end()) {
        dynPluginMap_.erase(it);
        returnValue = true;
    }

    return returnValue;
}

std::vector<std::string> PluginManager::listLoadedPlugins() const
{
    std::vector<std::string> res{};
    for(const auto& pair : dynPluginMap_) {
        res.push_back(pair.first);
    }
    return res;
}

void PluginManager::destroyPluginComponents(const std::string &path)
{
    auto itComponents = pluginComponentsMap_.find(path);
    if(itComponents != pluginComponentsMap_.end()) {
        for(const auto& pair : itComponents->second) {
            auto clcm = componentsLifeCycleManagers_.find(pair.first);
            if(clcm != componentsLifeCycleManagers_.end()) {
                clcm->second.destroyComponent(pair.second);
            }
        }
    }
}


bool PluginManager::callPluginInitFunction(const std::string &path){
    bool returnValue{false};
    PluginMap::iterator it = dynPluginMap_.find(path);
    if ( it != dynPluginMap_.end()) {
        // Plugin found
        // Since the Plugin was found it is of type DLPlugin with a valid init symbol
        std::shared_ptr<DLPlugin> plugin = std::static_pointer_cast<DLPlugin>(it->second);
        const auto &initFunc = plugin->getInitFunction();
        JAMI_PluginExitFunc exitFunc = nullptr;

        try {
            // Call Plugin Init function
            exitFunc = initFunc(&plugin->api_);
        } catch (const std::runtime_error &e) {
            JAMI_ERR() << e.what();
            return false;
        }

        if (!exitFunc) {
            JAMI_ERR() << "Plugin: init failed";
            returnValue = false;
        } else {
            returnValue = true;
        }
    }

    return returnValue;
}

bool PluginManager::registerPlugin(std::unique_ptr<Plugin>& plugin) {
  // Here we know that Plugin is of type DLPlugin with a valid init symbol
  const auto &initFunc = plugin->getInitFunction();
  JAMI_PluginExitFunc exitFunc = nullptr;

  DLPlugin* pluginPtr = static_cast<DLPlugin*>(plugin.get());

  pluginPtr->apiContext_ = this;
  pluginPtr->api_.version = {JAMI_PLUGIN_ABI_VERSION, JAMI_PLUGIN_API_VERSION};
  pluginPtr->api_.registerObjectFactory = registerObjectFactory_;
  /**
   * Implements JAMI_PluginAPI.invokeService().
   * Must be C accessible.
   */
  pluginPtr->api_.invokeService = [](const JAMI_PluginAPI *api,
                                const char *name, void *data) {
      auto plugin = static_cast<DLPlugin*>(api->context);
      auto manager = reinterpret_cast<PluginManager *>(plugin->apiContext_);
      if (!manager) {
          JAMI_ERR() << "invokeService called with null plugin API";
          return -1;
      }

      return manager->invokeService(plugin, name, data);
  };

  /**
   * Implements JAMI_PluginAPI.invokeService().
   * Must be C accessible.
   */
  pluginPtr->api_.manageComponent = [](const JAMI_PluginAPI* api, const char* name, void *data){
      auto plugin = static_cast<DLPlugin*>(api->context);
      auto manager = reinterpret_cast<PluginManager *>(plugin->apiContext_);
      if (!manager) {
          JAMI_ERR() << "createComponent called with null plugin API";
          return -1;
      } else if(!plugin){
          JAMI_ERR() << "createComponent called with null context";
          return -1;
      }

      return manager->manageComponent(plugin, name, data);
  };

  try {
      exitFunc = initFunc(&pluginPtr->api_);
  } catch (const std::runtime_error &e) {
    JAMI_ERR() << e.what();
  }

  if (!exitFunc) {
    tempExactMatchMap_.clear();
    tempWildCardVec_.clear();
    JAMI_ERR() << "Plugin: init failed";
    return false;
  }

  exitFuncVec_.push_back(exitFunc);
  exactMatchMap_.insert(tempExactMatchMap_.begin(), tempExactMatchMap_.end());
  wildCardVec_.insert(wildCardVec_.end(), tempWildCardVec_.begin(),
                      tempWildCardVec_.end());
  return true;
}

bool PluginManager::registerService(const std::string &name,
                                    ServiceFunction &&func) {
  services_[name] = std::forward<ServiceFunction>(func);
  return true;
}

void PluginManager::unRegisterService(const std::string &name) {
  services_.erase(name);
}

int32_t PluginManager::invokeService(const DLPlugin* plugin, const std::string &name, void *data) {
  const auto &iterFunc = services_.find(name);
  if (iterFunc == services_.cend()) {
    JAMI_ERR() << "Services not found: " << name;
    return -1;
  }

  const auto &func = iterFunc->second;

  try {
    return func(plugin, data);
  } catch (const std::runtime_error &e) {
    JAMI_ERR() << e.what();
    return -1;
  }
}

int32_t PluginManager::manageComponent(const DLPlugin* plugin, const std::string& name, void *data) {
    const auto& iter = componentsLifeCycleManagers_.find(name);
    if(iter == componentsLifeCycleManagers_.end()) {
        JAMI_ERR() << "Component lifecycle manager not found: " << name;
        return -1;
    }

    const auto& componentLifecycleManager = iter->second;

    try {
        int32_t r =  componentLifecycleManager.takeComponentOwnership(data);
        if(r == 0) {
            pluginComponentsMap_[plugin->getPath()].emplace_back(name,data);
        }
        return r;
    } catch(const std::runtime_error &e) {
        JAMI_ERR() << e.what();
        return -1;
    }
}

/* WARNING: exposed to plugins through JAMI_PluginAPI */
bool PluginManager::registerObjectFactory(
    const char *type, const JAMI_PluginObjectFactory &factoryData) {
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
  auto deleter = [factoryData](void *o) {
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

bool PluginManager::registerComponentManager(const std::string &name,
                                             ComponentFunction &&takeOwnership,
                                             ComponentFunction &&destroyComponent)
{
    componentsLifeCycleManagers_[name] = {std::forward<ComponentFunction>(takeOwnership),
                                          std::forward<ComponentFunction>(destroyComponent)};
    return true;
}

std::unique_ptr<void, PluginManager::ObjectDeleter>
PluginManager::createObject(const std::string &type) {
  if (type == "*")
    return {nullptr, nullptr};

  JAMI_PluginObjectParams op = {
      /*.pluginApi = */ &pluginApi_,
      /*.type = */ type.c_str(),
  };

  // Try to find an exact match
  const auto &factoryIter = exactMatchMap_.find(type);
  if (factoryIter != exactMatchMap_.end()) {
    const auto &factory = factoryIter->second;
    auto object = factory.data.create(&op, factory.data.closure);
    if (object)
      return {object, factory.deleter};
  }

  // Try to find a wildcard match
  for (const auto &factory : wildCardVec_) {
    auto object = factory.data.create(&op, factory.data.closure);
    if (object) {
      // promote registration to exactMatch_
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

/* WARNING: exposed to plugins through JAMI_PluginAPI */
int32_t PluginManager::registerObjectFactory_(const JAMI_PluginAPI *api,
                                              const char *type, void *data) {
  auto manager = reinterpret_cast<PluginManager *>(api->context);
  if (!manager) {
    JAMI_ERR() << "registerObjectFactory called with null plugin API";
    return -1;
  }

  if (!data) {
    JAMI_ERR() << "registerObjectFactory called with null factory data";
    return -1;
  }

  const auto factory = reinterpret_cast<JAMI_PluginObjectFactory *>(data);
  return manager->registerObjectFactory(type, *factory) ? 0 : -1;
}

} // namespace jami
