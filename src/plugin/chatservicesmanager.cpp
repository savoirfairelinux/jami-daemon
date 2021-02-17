/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#include "chatservicesmanager.h"
#include "logger.h"
#include "manager.h"
#include "fileutils.h"

namespace jami {

ChatServicesManager::ChatServicesManager(PluginManager& pm)
{
    registerComponentsLifeCycleManagers(pm);
    registerChatService(pm);
    PluginPreferencesUtils::getAllowDenyListPreferences(allowDenyList_);
}

void
ChatServicesManager::registerComponentsLifeCycleManagers(PluginManager& pm)
{
    auto registerChatHandler = [this](void* data) {
        ChatHandlerPtr ptr {(static_cast<ChatHandler*>(data))};

        if (!ptr)
            return -1;
        handlersNameMap_[ptr->getChatHandlerDetails().at("name")] = (uintptr_t) ptr.get();
        chatHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    auto unregisterChatHandler = [this](void* data) {
        auto handlerIt = std::find_if(chatHandlers_.begin(),
                                      chatHandlers_.end(),
                                      [data](ChatHandlerPtr& handler) {
                                          return (handler.get() == data);
                                      });

        if (handlerIt != chatHandlers_.end()) {
            for (auto& toggledList : chatHandlerToggled_) {
                auto handlerId = std::find_if(toggledList.second.begin(),
                                              toggledList.second.end(),
                                              [this, handlerIt](uintptr_t handlerId) {
                                                  return (handlerId == (uintptr_t) handlerIt->get());
                                              });
                if (handlerId != toggledList.second.end()) {
                    (*handlerIt)->detach(chatSubjects_[toggledList.first]);
                    toggledList.second.erase(handlerId);
                }
            }
            handlersNameMap_.erase((*handlerIt)->getChatHandlerDetails().at("name"));
            chatHandlers_.erase(handlerIt);
        }
        return true;
    };

    pm.registerComponentManager("ChatHandlerManager", registerChatHandler, unregisterChatHandler);
}

void
ChatServicesManager::registerChatService(PluginManager& pm)
{
    auto sendTextMessage = [this](const DLPlugin*, void* data) {
        auto cm = static_cast<JamiMessage*>(data);
        jami::Manager::instance().sendTextMessage(cm->accountId, cm->peerId, cm->data, true);
        return 0;
    };

    pm.registerService("sendTextMessage", sendTextMessage);
}

std::vector<std::string>
ChatServicesManager::getChatHandlers()
{
    std::vector<std::string> res;
    res.reserve(chatHandlers_.size());
    for (const auto& chatHandler : chatHandlers_) {
        res.emplace_back(std::to_string((uintptr_t) chatHandler.get()));
    }
    return res;
}

void
ChatServicesManager::publishMessage(pluginMessagePtr& cm)
{
    if (cm->fromPlugin)
        return;
    std::pair<std::string, std::string> mPair(cm->accountId, cm->peerId);
    auto& handlers = chatHandlerToggled_[mPair];
    auto& chatAllowDenySet = allowDenyList_[mPair];

    for (auto& chatHandler : chatHandlers_) {
        std::string chatHandlerName = chatHandler->getChatHandlerDetails().at("name");
        std::size_t found = chatHandler->id().find_last_of(DIR_SEPARATOR_CH);
        auto preferences = PluginPreferencesUtils::getPreferencesValuesMap(
            chatHandler->id().substr(0, found));
        bool toggle = preferences.at("always") == "1";
        auto allowedIt = chatAllowDenySet.find(chatHandlerName);
        if (allowedIt != chatAllowDenySet.end())
            toggle = (*allowedIt).second;
        bool toggled = handlers.find((uintptr_t) chatHandler.get()) != handlers.end();
        if (toggle || toggled) {
            chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>());
            if (!toggled) {
                handlers.insert((uintptr_t) chatHandler.get());
                chatHandler->notifyChatSubject(mPair, chatSubjects_[mPair]);
                chatAllowDenySet[chatHandlerName] = true;
                PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList_);
            }
            chatSubjects_[mPair]->publish(cm);
        }
    }
}

void
ChatServicesManager::cleanChatSubjects(const std::string& accountId, const std::string& peerId)
{
    std::pair<std::string, std::string> mPair(accountId, peerId);
    for (auto it = chatSubjects_.begin(); it != chatSubjects_.end();) {
        if (peerId.empty() && it->first.first == accountId)
            it = chatSubjects_.erase(it);
        else if (!peerId.empty() && it->first == mPair)
            it = chatSubjects_.erase(it);
        else
            ++it;
    }
}

void
ChatServicesManager::toggleChatHandler(const std::string& chatHandlerId,
                                       const std::string& accountId,
                                       const std::string& peerId,
                                       const bool toggle)
{
    toggleChatHandler(std::stoull(chatHandlerId), accountId, peerId, toggle);
}

std::vector<std::string>
ChatServicesManager::getChatHandlerStatus(const std::string& accountId, const std::string& peerId)
{
    std::pair<std::string, std::string> mPair(accountId, peerId);
    const auto& it = allowDenyList_.find(mPair);
    std::vector<std::string> ret;
    if (it != allowDenyList_.end()) {
        for (const auto& chatHandlerName : it->second)
            if (chatHandlerName.second)
                ret.emplace_back(std::to_string(handlersNameMap_.at(chatHandlerName.first)));
    }

    return ret;
}

std::map<std::string, std::string>
ChatServicesManager::getChatHandlerDetails(const std::string& chatHandlerIdStr)
{
    auto chatHandlerId = std::stoull(chatHandlerIdStr);
    for (auto& chatHandler : chatHandlers_) {
        if ((uintptr_t) chatHandler.get() == chatHandlerId) {
            return chatHandler->getChatHandlerDetails();
        }
    }
    return {};
}

void
ChatServicesManager::setPreference(const std::string& key,
                                   const std::string& value,
                                   const std::string& scopeStr)
{
    for (auto& chatHandler : chatHandlers_) {
        if (scopeStr.find(chatHandler->getChatHandlerDetails()["name"]) != std::string::npos) {
            chatHandler->setPreferenceAttribute(key, value);
        }
    }
}

void
ChatServicesManager::toggleChatHandler(const uintptr_t chatHandlerId,
                                       const std::string& accountId,
                                       const std::string& peerId,
                                       const bool toggle)
{
    std::pair<std::string, std::string> mPair(accountId, peerId);
    auto& handlers = chatHandlerToggled_[mPair];
    auto& chatAllowDenySet = allowDenyList_[mPair];
    chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>());

    auto chatHandlerIt = std::find_if(chatHandlers_.begin(),
                                      chatHandlers_.end(),
                                      [chatHandlerId](ChatHandlerPtr& handler) {
                                          return ((uintptr_t) handler.get() == chatHandlerId);
                                      });

    if (chatHandlerIt != chatHandlers_.end()) {
        if (toggle) {
            (*chatHandlerIt)->notifyChatSubject(mPair, chatSubjects_[mPair]);
            if (handlers.find(chatHandlerId) == handlers.end())
                handlers.insert(chatHandlerId);
            chatAllowDenySet[(*chatHandlerIt)->getChatHandlerDetails().at("name")] = true;
        } else {
            (*chatHandlerIt)->detach(chatSubjects_[mPair]);
            handlers.erase(chatHandlerId);
            chatAllowDenySet[(*chatHandlerIt)->getChatHandlerDetails().at("name")] = false;
        }
        PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList_);
    }
}
} // namespace jami
