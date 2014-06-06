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

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H 1

#include "plugin.h"
#include "plugin_loader.h"
#include "noncopyable.h"

#include <map>
#include <vector>
#include <memory>
#include <mutex>

#include <inttypes.h>

class PluginManager
{
    typedef std::map<std::string, std::shared_ptr<Plugin> > PluginMap;
    typedef std::vector<SFLPluginExitFunc> ExitFuncVec;
    typedef std::vector<SFLPluginRegisterParams> RegisterParamsVec;

public:
    typedef std::map<std::string, SFLPluginRegisterParams> RegisterParamsMap;

    PluginManager();
    ~PluginManager();

    int initPlugin(SFLPluginInitFunc initFunc);
    static int32_t registerObject(const SFLPluginAPI* api,
                                  const int8_t* type,
                                  const SFLPluginRegisterParams* params) {
        static_cast<PluginManager*>(api->context)->registerObject_(type, params);
    }

    int load(const std::string& path);
    void* createObject(const std::string& type);
    const RegisterParamsMap& getRegisters();
    SFLPluginAPI& getPluginAPI();

private:
    NON_COPYABLE(PluginManager);

    int32_t shutdown();
    int32_t registerObject_(const int8_t* type,
                            const SFLPluginRegisterParams* params);

    std::mutex          mutex_ = {};
    SFLPluginAPI        pluginApi_ = {
        { SFL_PLUGIN_ABI_VERSION, SFL_PLUGIN_API_VERSION },
        nullptr, registerObject, nullptr };
    PluginMap           dynPluginMap_ = {}; // Only dynamic loaded plugins
    ExitFuncVec         exitFuncVec_ = {};
    RegisterParamsMap   exactMatchMap_ = {};
    RegisterParamsVec   wildCardVec_ = {};

    // Storage used during plugin initialisation.
    // Will be copied into previous ones only if the initialisation success.
    RegisterParamsMap   tempExactMatchMap_ = {};
    RegisterParamsVec   tempWildCardVec_ = {};
};

#endif /* PLUGIN_MANAGER_H */
