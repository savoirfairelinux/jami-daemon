/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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
#include "ringplugin.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <inttypes.h>

namespace jami {

class Plugin;

class PluginManager {
public:
  using ObjectDeleter = std::function<void(void *)>;
  using ServiceFunction = std::function<int32_t(void *)>;

private:
  struct ObjectFactory {
    RING_PluginObjectFactory data;
    ObjectDeleter deleter;
  };

  using PluginMap = std::map<std::string, std::shared_ptr<Plugin>>;
  using ExitFuncVec = std::vector<RING_PluginExitFunc>;
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
  bool load(const std::string &path);

  /**
   * Register a plugin.
   *
   * @param initFunc plugin init function
   * @return true if success
   */
  bool registerPlugin(RING_PluginInitFunc initFunc);

  /**
   * Register a new service for plugin.
   *
   * @param name The service name
   * @param func The function called by Ring_PluginAPI.invokeService
   * @return true if success
   */
  bool registerService(const std::string &name, ServiceFunction &&func);

  void unRegisterService(const std::string &name);

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
  bool registerObjectFactory(const char *type,
                             const RING_PluginObjectFactory &factory);

  /**
   * Create a new plugin's exported object.
   *
   * @param type unique identifier of the object to create.
   * @return unique pointer on created object.
   */
  std::unique_ptr<void, ObjectDeleter> createObject(const std::string &type);

  const RING_PluginAPI &getPluginAPI() const { return pluginApi_; }

private:
  NON_COPYABLE(PluginManager);

  /**
   * Implements RING_PluginAPI.registerObjectFactory().
   * Must be C accessible.
   */
  static int32_t registerObjectFactory_(const RING_PluginAPI *api,
                                        const char *type, void *data);

  /**
   * Implements RING_PluginAPI.invokeService().
   * Must be C accessible.
   */
  static int32_t invokeService_(const RING_PluginAPI *api, const char *name,
                                void *data);

  int32_t invokeService(const std::string &name, void *data);

  std::mutex mutex_{};
  RING_PluginAPI pluginApi_ = {
      {RING_PLUGIN_ABI_VERSION, RING_PLUGIN_API_VERSION},
      nullptr, // set by PluginManager constructor
      registerObjectFactory_,
      invokeService_,
  };
  PluginMap dynPluginMap_{}; // Only dynamic loaded plugins
  ExitFuncVec exitFuncVec_{};
  ObjectFactoryMap exactMatchMap_{};
  ObjectFactoryVec wildCardVec_{};

  // Storage used during plugin initialisation.
  // Will be copied into previous ones only if the initialisation success.
  ObjectFactoryMap tempExactMatchMap_{};
  ObjectFactoryVec tempWildCardVec_{};

  // registered services
  std::map<std::string, ServiceFunction> services_{};
};

} // namespace jami
