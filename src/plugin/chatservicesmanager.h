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
#include "chathandler.h"
#include "logger.h"
// Manager
#include "manager.h"

namespace jami {

using ChatHandlerPtr = std::shared_ptr<ChatHandler>;

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
                chatHandlers.emplace_back(std::move(ptr));
            }

            return 0;
        };

        auto unregisterChatHandler = [this](void* data) {
            for (auto it = chatHandlers.begin(); it != chatHandlers.end(); ++it) {
                if (it->get() == data) {
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

    std::vector<std::string> listChatHandlers()
    {
        std::vector<std::string> res;
        for (const auto& chatHandler : chatHandlers) {
            res.emplace_back(getChatHandlerId(chatHandler));
        }
        return res;
    }

    void publishMessage(pluginMessagePtr& cm)
    {
        std::pair<std::string, std::string> mPair(cm->accountId, cm->peerId);
        for (auto& chatHandler : chatHandlers) {
            std::size_t found = chatHandler->id().find_last_of(DIR_SEPARATOR_CH);
            auto toDir = chatHandler->id().substr(0, found);
            auto preferences = getPluginPreferencesValuesMapInternal(toDir);
            std::string always = "0";
            if (preferences.find("always") != preferences.end())
                always = preferences["always"];
            if (always == "1" || chatHandlerToggled_.find(mPair) != chatHandlerToggled_.end()) {
                if (chatSubjects.find(mPair) == chatSubjects.end())
                    chatSubjects[mPair] = std::make_shared<PublishObservable<pluginMessagePtr>>();
                if (chatHandlerToggled_.find(mPair) == chatHandlerToggled_.end()) {
                    chatHandlerToggled_[mPair].insert(chatHandler);
                    chatHandler->notifyChatSubject(mPair, chatSubjects[mPair]);
                }
                if (!cm->fromPlugin)
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
        if (chatHandlerId.empty() || peerId.empty())
            return;

        std::pair<std::string, std::string> mPair(accountId, peerId);

        auto find = chatHandlerToggled_.find(mPair);
        if (find == chatHandlerToggled_.end())
            chatHandlerToggled_[mPair] = {};

        auto peerChatSubjectIt = chatSubjects.find(mPair);
        if (peerChatSubjectIt == chatSubjects.end())
            chatSubjects[mPair] = std::make_shared<PublishObservable<pluginMessagePtr>>();

        for (auto& chatHandler : chatHandlers) {
            if (getChatHandlerId(chatHandler) == chatHandlerId) {
                if (toggle) {
                    chatHandler->notifyChatSubject(mPair, chatSubjects[mPair]);
                    if (chatHandlerToggled_[mPair].find(chatHandler)
                        == chatHandlerToggled_[mPair].end())
                        chatHandlerToggled_[mPair].insert(chatHandler);
                } else {
                    chatHandler->detach(chatSubjects[mPair]);
                    if (chatHandlerToggled_[mPair].find(chatHandler)
                        != chatHandlerToggled_[mPair].end())
                        chatHandlerToggled_[mPair].erase(chatHandler);
                }
                break;
            }
        }
    }

    /**
     * @brief getCallMediaHandlerDetails
     * @param id of the call media handler
     * @return map of Call Media Handler Details
     */
    std::map<std::string, std::string> getChatHandlerDetails(const std::string& chatHandlerId)
    {
        for (auto& chatHandler : chatHandlers) {
            if (getChatHandlerId(chatHandler) == chatHandlerId) {
                return chatHandler->getChatHandlerDetails();
            }
        }
        return {};
    }

    /**
     * @brief getChatHandlerId
     * Returns the chatHandlerId id from a chatHandlerId pointer
     * @param chatHandlerId
     * @return string id
     */
    std::string getChatHandlerId(const ChatHandlerPtr& chatHandlerId)
    {
        if (chatHandlerId) {
            std::ostringstream chatHandlerIdStream;
            chatHandlerIdStream << chatHandlerId.get();
            return chatHandlerIdStream.str();
        }
        return "";
    }

private:
    std::list<ChatHandlerPtr> chatHandlers;
    std::map<std::pair<std::string, std::string>, std::set<ChatHandlerPtr>>
        chatHandlerToggled_; // {account,peer}, list of chatHandlers

    std::map<std::pair<std::string, std::string>, chatSubjectPtr> chatSubjects;
};
} // namespace jami
