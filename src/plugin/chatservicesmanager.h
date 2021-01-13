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
#include "PluginPreferences.h"
#include "chathandler.h"
#include "logger.h"
// Manager
#include "manager.h"

namespace jami {

using ChatHandlerPtr = std::unique_ptr<ChatHandler>;

class ChatServicesManager
{
public:
    ChatServicesManager(PluginManager& pm)
    {
        registerComponentsLifeCycleManagers(pm);
        registerChatService(pm);
    }
    NON_COPYABLE(ChatServicesManager);

public:
    void registerComponentsLifeCycleManagers(PluginManager& pm)
    {
        auto registerChatHandler = [this](void* data) {
            ChatHandlerPtr ptr {(static_cast<ChatHandler*>(data))};

            if (ptr) {
                handlersNameMap[ptr->getChatHandlerDetails().at("name")] = (uintptr_t) ptr.get();
                chatHandlers.emplace_back(std::move(ptr));
            }

            return 0;
        };

        auto unregisterChatHandler = [this](void* data) {
            for (auto it = chatHandlers.begin(); it != chatHandlers.end(); ++it) {
                if (it->get() == data) {
                    for (auto toggledIt = chatHandlerToggled_.begin();
                         toggledIt != chatHandlerToggled_.end();
                         ++toggledIt)
                        for (auto handlerIdIt = toggledIt->second.begin();
                             handlerIdIt != toggledIt->second.end();)
                            if (*handlerIdIt == (uintptr_t) it->get()) {
                                handlerIdIt = toggledIt->second.erase(handlerIdIt);
                                (*it)->detach(chatSubjects[toggledIt->first]);
                            } else
                                ++handlerIdIt;
                    clearAllowDenyLists((*it)->id());
                    handlersNameMap.erase((*it)->getChatHandlerDetails().at("name"));
                    chatHandlers.erase(it);
                    break;
                }
            }
            return 0;
        };

        pm.registerComponentManager("ChatHandlerManager",
                                    registerChatHandler,
                                    unregisterChatHandler);
    }

    void registerChatService(PluginManager& pm)
    {
        auto sendTextMessage = [this](const DLPlugin*, void* data) {
            auto cm = static_cast<JamiMessage*>(data);
            jami::Manager::instance().sendTextMessage(cm->accountId, cm->peerId, cm->data, true);
            return 0;
        };

        pm.registerService("sendTextMessage", sendTextMessage);
    }

    std::vector<std::string> getChatHandlers()
    {
        std::vector<std::string> res;
        res.reserve(chatHandlers.size());
        for (const auto& chatHandler : chatHandlers) {
            res.emplace_back(std::to_string((uintptr_t) chatHandler.get()));
        }
        return res;
    }

    void publishMessage(pluginMessagePtr& cm)
    {
        if (cm->fromPlugin)
            return;
        std::pair<std::string, std::string> mPair(cm->accountId, cm->peerId);
        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatDenySet = denyList[mPair];
        auto& chatAllowSet = allowList[mPair];

        for (auto& chatHandler : chatHandlers) {
            std::string chatHandlerName = chatHandler->getChatHandlerDetails().at("name");
            std::size_t found = chatHandler->id().find_last_of(DIR_SEPARATOR_CH);
            auto preferences = getPluginPreferencesValuesMapInternal(
                chatHandler->id().substr(0, found));
            bool toggled = false;
            if (handlers.find((uintptr_t) chatHandler.get()) != handlers.end())
                toggled = true;
            bool denyListed = false;
            if (chatDenySet.find(chatHandlerName) != chatDenySet.end())
                denyListed = true;
            if ((preferences.at("always") == "1" || toggled) && !denyListed) {
                chatSubjects.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>());
                if (!toggled) {
                    handlers.insert((uintptr_t) chatHandler.get());
                    chatHandler->notifyChatSubject(mPair, chatSubjects[mPair]);
                    chatAllowSet.insert(chatHandlerName);
                    setAllowDenyListPreferences(allowList);
                }
                chatSubjects[mPair]->publish(cm);
            }
        }
    }

    void cleanChatSubjects(const std::string& accountId, const std::string& peerId = "")
    {
        std::pair<std::string, std::string> mPair(accountId, peerId);
        for (auto it = chatSubjects.begin(); it != chatSubjects.end();) {
            if (peerId.empty() && it->first.first == accountId)
                it = chatSubjects.erase(it);
            else if (!peerId.empty() && it->first == mPair)
                it = chatSubjects.erase(it);
            else
                ++it;
        }
    }

    void toggleChatHandler(const std::string& chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle)
    {
        toggleChatHandler(std::stoull(chatHandlerId), accountId, peerId, toggle);
    }

    std::vector<std::string> getChatHandlerStatus(const std::string& accountId,
                                                  const std::string& peerId)
    {
        std::pair<std::string, std::string> mPair(accountId, peerId);
        const auto& it = allowList.find(mPair);
        std::vector<std::string> ret;
        if (it != allowList.end()) {
            ret.reserve(it->second.size());
            for (const auto& chatHandlerName : it->second)
                ret.emplace_back(std::to_string(handlersNameMap.at(chatHandlerName)));
            return ret;
        }

        return ret;
    }

    /**
     * @brief getChatHandlerDetails
     * @param id of the chat handler
     * @return map of Chat Handler Details
     */
    std::map<std::string, std::string> getChatHandlerDetails(const std::string& chatHandlerIdStr)
    {
        auto chatHandlerId = std::stoull(chatHandlerIdStr);
        for (auto& chatHandler : chatHandlers) {
            if ((uintptr_t) chatHandler.get() == chatHandlerId) {
                return chatHandler->getChatHandlerDetails();
            }
        }
        return {};
    }

    void setPreference(const std::string& key, const std::string& value, const std::string& scopeStr)
    {
        for (auto& chatHandler : chatHandlers) {
            if (scopeStr.find(chatHandler->getChatHandlerDetails()["name"]) != std::string::npos) {
                chatHandler->setPreferenceAttribute(key, value);
            }
        }
    }

    void setAllowDenyListsFromPreferences() {
        getAllowDenyListPreferences(allowList);
        getAllowDenyListPreferences(denyList, false);
    }

private:
    void toggleChatHandler(const uintptr_t chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle)
    {
        std::pair<std::string, std::string> mPair(accountId, peerId);
        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatDenySet = denyList[mPair];
        auto& chatAllowSet = allowList[mPair];
        chatSubjects.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>());

        for (auto& chatHandler : chatHandlers) {
            if ((uintptr_t) chatHandler.get() == chatHandlerId) {
                if (toggle) {
                    chatHandler->notifyChatSubject(mPair, chatSubjects[mPair]);
                    if (handlers.find(chatHandlerId) == handlers.end())
                        handlers.insert(chatHandlerId);
                        chatAllowSet.insert(chatHandler->getChatHandlerDetails().at("name"));
                        chatDenySet.erase(chatHandler->getChatHandlerDetails().at("name"));
                } else {
                    chatHandler->detach(chatSubjects[mPair]);
                    handlers.erase(chatHandlerId);
                    chatAllowSet.erase(chatHandler->getChatHandlerDetails().at("name"));
                    chatDenySet.insert(chatHandler->getChatHandlerDetails().at("name"));
                }
                setAllowDenyListPreferences(allowList);
                setAllowDenyListPreferences(denyList, false);
                break;
            }
        }
    }

    void clearAllowDenyLists(const std::string& pluginPath) {
        for (auto& chatHandler : chatHandlers) {
            if (chatHandler->id() == pluginPath) {
                std::string handlerName = chatHandler->getChatHandlerDetails()["name"];
                for (auto& mapItem : allowList) {
                    mapItem.second.erase(handlerName);
                }
                for (auto& mapItem : denyList) {
                    mapItem.second.erase(handlerName);
                }
            }
        }
    }

    std::map<std::string, uintptr_t> handlersNameMap{};
    std::list<ChatHandlerPtr> chatHandlers;
    std::map<std::pair<std::string, std::string>, std::set<uintptr_t>>
        chatHandlerToggled_; // {account,peer}, list of chatHandlers

    std::map<std::pair<std::string, std::string>, chatSubjectPtr> chatSubjects;

    std::map<std::pair<std::string, std::string>, std::set<std::string>> allowList{};
    std::map<std::pair<std::string, std::string>, std::set<std::string>> denyList{};
};
} // namespace jami
