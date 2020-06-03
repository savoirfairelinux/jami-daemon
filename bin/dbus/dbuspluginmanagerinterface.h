/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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
 */

#pragma once

#include <vector>
#include <map>
#include <string>

#include "dring/def.h"
#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbuspluginmanagerinterface.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class DRING_PUBLIC DBusPluginManagerInterface :
    public cx::ring::Ring::PluginManagerInterface_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
    public:
        DBusPluginManagerInterface(DBus::Connection& connection);

        // Methods
        bool loadPlugin(const std::string& path);
        bool unloadPlugin(const std::string& path);
        void togglePlugin(const std::string& path, const bool& toggle);
        std::map<std::string,std::string> getPluginDetails(const std::string& path);
        std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
        bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
        std::map<std::string,std::string> getPluginPreferencesValues(const std::string& path);
        bool resetPluginPreferencesValues(const std::string& path);
        std::vector<std::string> listAvailablePlugins();
        std::vector<std::string> listLoadedPlugins();
        int installPlugin(const std::string& jplPath, const bool& force);
        int uninstallPlugin(const std::string& pluginRootPath);
        std::vector<std::string> listCallMediaHandlers();
        void toggleCallMediaHandler(const std::string& id, const bool& toggle);
        std::map<std::string,std::string> getCallMediaHandlerDetails(const std::string& id);
};
