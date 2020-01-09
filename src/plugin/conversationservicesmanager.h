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
#include "logger.h"
//Manager
#include "manager.h"

namespace jami {

using ConversationHandlerPtr = std::unique_ptr<ConversationHandler>;

class ConversationServicesManager{
public:
    ConversationServicesManager(PluginManager& pm){
        registerComponentsLifeCycleManagers(pm);

        auto sendTextMessage = [this](const DLPlugin* plugin, void* data) {
            auto cm = static_cast<ConversationMessage*>(data);
            jami::Manager::instance().sendTextMessage(cm->from_, cm->to_, cm->data_);
            return 0;
        };

        pm.registerService("sendTextMessage", sendTextMessage);
    }
    NON_COPYABLE(ConversationServicesManager);

public:
    void registerComponentsLifeCycleManagers(PluginManager& pm) {
        auto registerConversationHandler = [this](void* data) {
            ConversationHandlerPtr ptr{(static_cast<ConversationHandler*>(data))};

            if(ptr) {
                conversationHandlers.push_back(std::make_pair(false, std::move(ptr)));
                if(!conversationHandlers.empty()) {
                    listAvailableSubjects(conversationHandlers.back().second);
                }
            }

            return 0;
        };

        auto unregisterConversationHandler = [this](void* data) {
            for(auto it = conversationHandlers.begin(); it != conversationHandlers.end(); ++it) {
                if(it->second.get() == data) {
                    conversationHandlers.erase(it);
                }
            }
            return 0;
        };

        pm.registerComponentManager("ConversationHandlerManager",
                                    registerConversationHandler, unregisterConversationHandler);
    }

    void onTextMessage(std::shared_ptr<ConversationMessage>& cm) {
        if(!conversationHandlers.empty()) {
            receiveSubject->publish(cm);
        }
    }

    void sendTextMessage(std::shared_ptr<ConversationMessage>& cm) {
        if(!conversationHandlers.empty()) {
            sendSubject->publish(cm);
        }
    }

private:

    void listAvailableSubjects(ConversationHandlerPtr& plugin) {
        plugin->notifyStrMapSubject(false, sendSubject);
        plugin->notifyStrMapSubject(true, receiveSubject);
    }

private:
    std::list<std::pair<bool, ConversationHandlerPtr>> conversationHandlers;
    strMapSubjectPtr sendSubject = std::make_shared<PublishObservable<std::shared_ptr<ConversationMessage>>>();
    strMapSubjectPtr receiveSubject = std::make_shared<PublishObservable<std::shared_ptr<ConversationMessage>>>();
};
}
