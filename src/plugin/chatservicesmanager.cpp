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
#include "pluginmanager.h"
#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "fileutils.h"

namespace jami {

ChatServicesManager::ChatServicesManager(PluginManager& pluginManager)
{
    registerComponentsLifeCycleManagers(pluginManager);
    registerChatService(pluginManager);
    PluginPreferencesUtils::getAllowDenyListPreferences(allowDenyList_);
}

void
ChatServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // registerChatHandler may be called by the PluginManager upon loading a plugin.
    auto registerChatHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
        ChatHandlerPtr ptr {(static_cast<ChatHandler*>(data))};

        if (!ptr)
            return -1;
        handlersNameMap_[ptr->getChatHandlerDetails().at("name")] = (uintptr_t) ptr.get();
        std::size_t found = ptr->id().find_last_of(DIR_SEPARATOR_CH);
        // Adding preference that tells us to automatically activate a ChatHandler.
        PluginPreferencesUtils::addAlwaysHandlerPreference(ptr->getChatHandlerDetails().at("name"),
                                                           ptr->id().substr(0, found));
        chatHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterChatHandler may be called by the PluginManager while unloading.
    auto unregisterChatHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
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
                // If ChatHandler we're trying to destroy is currently in use, we deactivate it.
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

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("ChatHandlerManager",
                                           registerChatHandler,
                                           unregisterChatHandler);
}

void
ChatServicesManager::registerChatService(PluginManager& pluginManager)
{
    // sendTextMessage is a service that allows plugins to send a message in a conversation.
    auto sendTextMessage = [this](const DLPlugin*, void* data) {
        auto cm = static_cast<JamiMessage*>(data);
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(
                cm->accountId)) {
            try {
                if (cm->isSwarm)
                    acc->sendMessage(cm->peerId, cm->data.at("body"));
                else
                    jami::Manager::instance().sendTextMessage(cm->accountId,
                                                              cm->peerId,
                                                              cm->data,
                                                              true);
            } catch (const std::exception& e) {
                JAMI_ERR("Exception during text message sending: %s", e.what());
            }
        }
        return 0;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerService("sendTextMessage", sendTextMessage);
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
ChatServicesManager::publishMessage(pluginMessagePtr& message)
{
    if (message->fromPlugin)
        return;
    std::pair<std::string, std::string> mPair(message->accountId, message->peerId);
    auto& handlers = chatHandlerToggled_[mPair];
    auto& chatAllowDenySet = allowDenyList_[mPair];

    // Search for activation flag.
    for (auto& chatHandler : chatHandlers_) {
        std::string chatHandlerName = chatHandler->getChatHandlerDetails().at("name");
        std::size_t found = chatHandler->id().find_last_of(DIR_SEPARATOR_CH);
        // toggle is true if we should automatically activate the ChatHandler.
        bool toggle = PluginPreferencesUtils::getAlwaysPreference(chatHandler->id().substr(0, found),
                                                                  chatHandlerName);
        // toggle is overwritten if we have previously activated/deactivated the ChatHandler
        // for the given conversation.
        auto allowedIt = chatAllowDenySet.find(chatHandlerName);
        if (allowedIt != chatAllowDenySet.end())
            toggle = (*allowedIt).second;
        bool toggled = handlers.find((uintptr_t) chatHandler.get()) != handlers.end();
        if (toggle || toggled) {
            // Creates chat subjects if it doesn't exist yet.
            chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>());
            if (!toggled) {
                // If activation is expected, and not yet performed, we perform activation
                handlers.insert((uintptr_t) chatHandler.get());
                chatHandler->notifyChatSubject(mPair, chatSubjects_[mPair]);
                chatAllowDenySet[chatHandlerName] = true;
                PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList_);
            }
            // Finally we feed Chat subject with the message.
            chatSubjects_[mPair]->publish(message);
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
            if (chatHandlerName.second) // We only return active ChatHandler ids
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

bool
ChatServicesManager::setPreference(const std::string& key,
                                   const std::string& value,
                                   const std::string& rootPath)
{
    bool status {true};
    for (auto& chatHandler : chatHandlers_) {
        if (chatHandler->id().find(rootPath) != std::string::npos) {
            if (chatHandler->preferenceMapHasKey(key)) {
                chatHandler->setPreferenceAttribute(key, value);
                status &= false;
            }
        }
    }
    return status;
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
