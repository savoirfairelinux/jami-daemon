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
#include "logger.h"

PluginManager::PluginManager()
{
    pluginApi_.context = static_cast<void*>(this);
}

PluginManager::~PluginManager()
{ shutdown(); }

int32_t PluginManager::shutdown()
{
    int32_t result = 0;

    for (auto func : exitFuncVec_) {
        try {
            result |= (*func)();
        } catch (...) {
            result = -1;
        }
    }

    dynPluginMap_.clear();
    exactMatchMap_.clear();
    wildCardVec_.clear();
    exitFuncVec_.clear();

    return result;
}

int PluginManager::initPlugin(SFLPluginInitFunc initFunc)
{
    SFLPluginExitFunc exitFunc = initFunc(&pluginApi_);
    if (!exitFunc) {
        tempExactMatchMap_.clear();
        tempWildCardVec_.clear();
        DEBUG("plugin: init failed");
        return -1;
    }

    exitFuncVec_.push_back(exitFunc);
    exactMatchMap_.insert(tempExactMatchMap_.begin(),
                          tempExactMatchMap_.end());
    wildCardVec_.insert(wildCardVec_.end(),
                        tempWildCardVec_.begin(),
                        tempWildCardVec_.end());
    return 0;
}

static bool isValidObject(const int8_t* type,
                          const SFLPluginRegisterParams* params)
{
    if (!type || !(*type))
        return false;
    if (!params ||!params->create || !params->destroy)
        return false;
    return true;
}

/* WARNING: exposed to plugin through SFLPluginAPI */
int32_t PluginManager::registerObject_(const int8_t* type,
                                       const SFLPluginRegisterParams* params)
{
    if (!isValidObject(type, params))
        return -1;

    SFLPluginVersion pm_version = pluginApi_.version;

    // Strict compatibility on ABI
    if (pm_version.abi != params->version.abi)
        return -1;

    // Backware compatibility on API
    if (pm_version.api > params->version.api)
        return -1;

    std::string key((const char *)type);

    // wild card registration?
    if (key == std::string("*")) {
        wildCardVec_.push_back(*params);
        return 0;
    }

    // fails on duplicate for exactMatch map
    if (exactMatchMap_.find(key) != exactMatchMap_.end())
        return -1;

    exactMatchMap_[key] = *params;
    return 0;
}

int PluginManager::load(const std::string& path)
{
    // TODO: Resolve symbolic links and make path absolute

    // Don't load the same dynamic library twice
    if (dynPluginMap_.find(std::string(path)) != dynPluginMap_.end()) {
        DEBUG("plugin: already loaded");
        return -1;
    }

    std::string error;
    Plugin *plugin = Plugin::load(std::string(path), error);
    if (!plugin) {
        DEBUG("plugin: %s", error.c_str());
        return -1;
    }

    SFLPluginInitFunc init_func;
    init_func = (SFLPluginInitFunc)(plugin->getInitFunction());
    if (!init_func) {
        DEBUG("plugin: no init symbol");
        return -1;
    }

    if (initPlugin(init_func))
        return -1;

    dynPluginMap_[path] = std::shared_ptr<Plugin>(plugin);
    return 0;
}

const PluginManager::RegisterParamsMap& PluginManager::getRegisters()
{
    return exactMatchMap_;
}

SFLPluginAPI& PluginManager::getPluginAPI()
{
    return pluginApi_;
}

void* PluginManager::createObject(const std::string& type)
{
    if (type == "*")
        return nullptr;

    SFLPluginObjectParams op;
    op.type = (const int8_t*)type.c_str();
    op.pluginApi = &pluginApi_;

    // Try to find an exact match
    if (exactMatchMap_.find(type) != exactMatchMap_.end()) {
        SFLPluginRegisterParams &rp = exactMatchMap_[type];
        void *object = rp.create(&op);
        if (object)
            return object;
    }

    // Try to find a wildcard match
    for (size_t i = 0; i < wildCardVec_.size(); ++i)
    {
        SFLPluginRegisterParams &rp = wildCardVec_[i];
        void *object = rp.create(&op);
        if (object) {
            // promote registration to exactMatch_
            // (but keep also the wild card registration for other object types)
            int32_t res = registerObject_(op.type, &rp);
            if (res < 0) {
                ERROR("failed to register object %s", op.type);
                rp.destroy(object);
                return nullptr;
            }

            return object;
        }
    }

    return nullptr;
}
