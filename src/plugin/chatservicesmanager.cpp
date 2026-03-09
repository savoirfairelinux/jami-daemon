/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "chatservicesmanager.h"
#include "pluginmanager.h"
#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "fileutils.h"

#include <filesystem>
#include <vector>

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
    auto registerChatHandler = [this](void* data, std::mutex&) {
        ChatHandlerPtr ptr {(static_cast<ChatHandler*>(data))};

        if (!ptr)
            return -1;
        const auto details = ptr->getChatHandlerDetails();
        const auto handlerName = details.at("name");
        const auto handlerId = reinterpret_cast<uintptr_t>(ptr.get());
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.waitUntilReady(lk);
        handlersNameMap_[handlerName] = handlerId;
        handlerNames_[handlerId] = handlerName;
        // Adding preference that tells us to automatically activate a ChatHandler.
        PluginPreferencesUtils::addAlwaysHandlerPreference(
            handlerName, std::filesystem::path(ptr->id()).parent_path().string());
        chatHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterChatHandler may be called by the PluginManager while unloading.
    auto unregisterChatHandler = [this](void* data, std::mutex&) {
        ChatHandlerPtr removedHandler;
        std::vector<chatSubjectPtr> subjectsToDetach;
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.beginUnload(lk);
        auto handlerIt = std::find_if(chatHandlers_.begin(), chatHandlers_.end(), [data](ChatHandlerPtr& handler) {
            return handler.get() == data;
        });

        if (handlerIt != chatHandlers_.end()) {
            const auto handlerId = reinterpret_cast<uintptr_t>(handlerIt->get());
            removedHandler = std::move(*handlerIt);
            chatHandlers_.erase(handlerIt);

            for (auto& toggledList : chatHandlerToggled_) {
                if (toggledList.second.erase(handlerId) != 0) {
                    auto subjectIt = chatSubjects_.find(toggledList.first);
                    if (subjectIt != chatSubjects_.end())
                        subjectsToDetach.emplace_back(subjectIt->second);
                }
            }

            if (const auto nameIt = handlerNames_.find(handlerId); nameIt != handlerNames_.end()) {
                handlersNameMap_.erase(nameIt->second);
                handlerNames_.erase(nameIt);
            }
        }

        lk.unlock();
        if (removedHandler) {
            for (auto& subject : subjectsToDetach)
                removedHandler->detach(subject);
        }
        lk.lock();
        operationState_.endUnload(lk);
        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("ChatHandlerManager", registerChatHandler, unregisterChatHandler);
}

void
ChatServicesManager::registerChatService(PluginManager& pluginManager)
{
    // sendTextMessage is a service that allows plugins to send a message in a conversation.
    auto sendTextMessage = [](const DLPlugin*, void* data) {
        auto* cm = static_cast<JamiMessage*>(data);
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(cm->accountId)) {
            try {
                if (cm->isSwarm)
                    acc->convModule()->sendMessage(cm->peerId, cm->data.at("body"));
                else
                    jami::Manager::instance().sendTextMessage(cm->accountId, cm->peerId, cm->data, true);
            } catch (const std::exception& e) {
                JAMI_ERR("Exception during text message sending: %s", e.what());
            }
        }
        return 0;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerService("sendTextMessage", sendTextMessage);
}

bool
ChatServicesManager::hasHandlers() const
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    return not chatHandlers_.empty();
}

std::vector<std::string>
ChatServicesManager::getChatHandlers() const
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    std::vector<std::string> res;
    res.reserve(chatHandlers_.size());
    for (const auto& chatHandler : chatHandlers_) {
        res.emplace_back(std::to_string((uintptr_t) chatHandler.get()));
    }
    return res;
}

void
ChatServicesManager::publishMessage(const pluginMessagePtr& message)
{
    if (message->fromPlugin)
        return;

    auto operation = operationState_.acquire();
    std::pair<std::string, std::string> mPair(message->accountId, message->peerId);
    std::vector<std::pair<ChatHandler*, chatSubjectPtr>> attachments;
    ChatHandlerList allowDenyList;
    chatSubjectPtr subject;
    std::size_t publishCount = 0;
    bool persistAllowDeny = false;

    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        if (chatHandlers_.empty())
            return;

        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatAllowDenySet = allowDenyList_[mPair];

        // Search for activation flag.
        for (auto& chatHandler : chatHandlers_) {
            const auto handlerId = reinterpret_cast<uintptr_t>(chatHandler.get());
            const auto nameIt = handlerNames_.find(handlerId);
            if (nameIt == handlerNames_.end())
                continue;

            bool toggle = PluginPreferencesUtils::getAlwaysPreference(
                std::filesystem::path(chatHandler->id()).parent_path().string(),
                nameIt->second,
                message->accountId);
            auto allowedIt = chatAllowDenySet.find(nameIt->second);
            if (allowedIt != chatAllowDenySet.end())
                toggle = allowedIt->second;

            const bool toggled = handlers.contains(handlerId);
            if (!(toggle || toggled))
                continue;

            if (!subject) {
                subject = chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>())
                              .first->second;
            }

            if (!toggled) {
                handlers.insert(handlerId);
                chatAllowDenySet[nameIt->second] = true;
                attachments.emplace_back(chatHandler.get(), subject);
                persistAllowDeny = true;
            }
            ++publishCount;
        }

        if (persistAllowDeny)
            allowDenyList = allowDenyList_;
    }

    for (auto& attachment : attachments) {
        auto subjectConnection = mPair;
        attachment.first->notifyChatSubject(subjectConnection, attachment.second);
    }

    if (subject) {
        for (std::size_t i = 0; i < publishCount; ++i)
            subject->publish(message);
    }

    if (persistAllowDeny)
        PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList);
}

void
ChatServicesManager::cleanChatSubjects(const std::string& accountId, const std::string& peerId)
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
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
    auto operation = operationState_.acquire();
    toggleChatHandler(std::stoull(chatHandlerId), accountId, peerId, toggle);
}

std::vector<std::string>
ChatServicesManager::getChatHandlerStatus(const std::string& accountId, const std::string& peerId)
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    std::pair<std::string, std::string> mPair(accountId, peerId);
    const auto& it = allowDenyList_.find(mPair);
    std::vector<std::string> ret;
    if (it != allowDenyList_.end()) {
        for (const auto& chatHandlerName : it->second)
            if (chatHandlerName.second
                && handlersNameMap_.contains(chatHandlerName.first)) { // We only return active ChatHandler ids
                ret.emplace_back(std::to_string(handlersNameMap_.at(chatHandlerName.first)));
            }
    }

    return ret;
}

std::map<std::string, std::string>
ChatServicesManager::getChatHandlerDetails(const std::string& chatHandlerIdStr)
{
    auto operation = operationState_.acquire();
    auto chatHandlerId = std::stoull(chatHandlerIdStr);
    ChatHandler* handler = nullptr;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& chatHandler : chatHandlers_) {
            if (reinterpret_cast<uintptr_t>(chatHandler.get()) == chatHandlerId) {
                handler = chatHandler.get();
                break;
            }
        }
    }
    if (handler)
        return handler->getChatHandlerDetails();
    return {};
}

bool
ChatServicesManager::setPreference(const std::string& key, const std::string& value, const std::string& rootPath)
{
    auto operation = operationState_.acquire();
    std::vector<ChatHandler*> handlers;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& chatHandler : chatHandlers_) {
            if (chatHandler->id().find(rootPath) != std::string::npos)
                handlers.emplace_back(chatHandler.get());
        }
    }

    bool status {true};
    for (auto* chatHandler : handlers) {
        if (chatHandler->preferenceMapHasKey(key)) {
            chatHandler->setPreferenceAttribute(key, value);
            status &= false;
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
    ChatHandler* handler = nullptr;
    chatSubjectPtr subject;
    ChatHandlerList allowDenyList;
    bool persistAllowDeny = false;

    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatAllowDenySet = allowDenyList_[mPair];
        subject = chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>()).first->second;

        auto chatHandlerIt = std::find_if(chatHandlers_.begin(),
                                          chatHandlers_.end(),
                                          [chatHandlerId](ChatHandlerPtr& chatHandler) {
                                              return reinterpret_cast<uintptr_t>(chatHandler.get()) == chatHandlerId;
                                          });

        if (chatHandlerIt == chatHandlers_.end())
            return;

        handler = chatHandlerIt->get();
        const auto nameIt = handlerNames_.find(chatHandlerId);
        if (nameIt == handlerNames_.end())
            return;

        if (toggle)
            handlers.insert(chatHandlerId);
        else
            handlers.erase(chatHandlerId);

        chatAllowDenySet[nameIt->second] = toggle;
        allowDenyList = allowDenyList_;
        persistAllowDeny = true;
    }

    if (toggle) {
        auto subjectConnection = mPair;
        handler->notifyChatSubject(subjectConnection, subject);
    } else {
        handler->detach(subject);
    }

    if (persistAllowDeny)
        PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList);
}
} // namespace jami
