/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
// Utils
#include "noncopyable.h"
// Plugin Manager
#include "pluginmanager.h"
#include "conversationhandler.h"

namespace jami {

using ConversationHandlerPtr = std::unique_ptr<ConversationHandler>;

class ConversationServicesManager{
public:
    ConversationServicesManager(){
    }
    NON_COPYABLE(ConversationServicesManager);

public:
    void registerComponentsLifeCycleManagers(PluginManager& pm) {
        auto registerConversationHandler = [this](const DLPlugin* plugin, void* data) {
            ConversationHandlerPtr ptr{(static_cast<ConversationHandler*>(data))};

            if(ptr) {
                const std::string& pluginId = plugin->getPath();

                conversationHandlers.push_back(std::make_pair(pluginId, std::move(ptr)));
                if(!conversationHandlers.empty()) {
                    listAvailableSubjects(conversationHandlers.back().second);
                }
            }

            return 0;
        };

        auto unregisterConversationHandler = [this](const DLPlugin* plugin, void* data) {
            (void) plugin;
            for(auto it = conversationHandlers.begin(); it != conversationHandlers.end(); ++it) {
                // Remove all possible duplicates, before unloading a plugin
                if(it->second.get() == data) {
                    conversationHandlers.erase(it);
                }
            }
            return 0;
        };

        pm.registerComponentManager("ConversationHandlerManager",
                                    registerConversationHandler, unregisterConversationHandler);
    }

private:

    void listAvailableSubjects(ConversationHandlerPtr& plugin) {
        plugin->notifyStrMapSubject(false, sendSubject);
        plugin->notifyStrMapSubject(true, receiveSubject);
    }

private:
    std::list<std::pair<const std::string, ConversationHandlerPtr>> conversationHandlers;
    strMapSubjectPtr sendSubject = std::make_shared<Observable<ConversationMessage>>();
    strMapSubjectPtr receiveSubject = std::make_shared<Observable<ConversationMessage>>();
};
}
