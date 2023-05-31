/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
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

#include "def.h"
#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbuspluginmanagerinterface.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class LIBJAMI_PUBLIC DBusPluginManagerInterface
    : public cx::ring::Ring::PluginManagerInterface_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::ObjectAdaptor
{
public:
    DBusPluginManagerInterface(DBus::Connection& connection);

    // Methods
    bool loadPlugin(const std::string& path);
    bool unloadPlugin(const std::string& path);
    std::map<std::string, std::string> getPluginDetails(const std::string& path);
    std::vector<std::map<std::string, std::string>> getPluginPreferences(
        const std::string& path, const std::string& accountId);
    bool setPluginPreference(const std::string& path,
                             const std::string& accountId,
                             const std::string& key,
                             const std::string& value);
    std::map<std::string, std::string> getPluginPreferencesValues(const std::string& path,
                                                                  const std::string& accountId);
    bool resetPluginPreferencesValues(const std::string& path, const std::string& accountId);
    std::vector<std::string> getInstalledPlugins();
    std::vector<std::string> getLoadedPlugins();
    int installPlugin(const std::string& jplPath, const bool& force);
    int uninstallPlugin(const std::string& pluginRootPath);
    std::vector<std::string> getCallMediaHandlers();
    std::vector<std::string> getChatHandlers();
    void toggleCallMediaHandler(const std::string& mediaHandlerId,
                                const std::string& callId,
                                const bool& toggle);
    void toggleChatHandler(const std::string& chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool& toggle);
    std::map<std::string, std::string> getCallMediaHandlerDetails(const std::string& mediaHandlerId);
    std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId);
    std::map<std::string, std::string> getChatHandlerDetails(const std::string& chatHandlerId);
    std::vector<std::string> getChatHandlerStatus(const std::string& accontId,
                                                  const std::string& peerId);

    bool getPluginsEnabled();
    void setPluginsEnabled(const bool& state);

    void sendWebViewMessage(const std::string& pluginId,
                            const std::string& webViewId,
                            const std::string& messageId,
                            const std::string& payload);

    std::string sendWebViewAttach(const std::string& pluginId,
                                  const std::string& accountId,
                                  const std::string& webViewId,
                                  const std::string& action);

    void sendWebViewDetach(const std::string& pluginId, const std::string& webViewId);
};
