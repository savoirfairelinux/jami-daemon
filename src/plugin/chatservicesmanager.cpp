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
#include "streamdata.h"
#include "jamidht/commit_message.h"
#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation.h"
#include "fileutils.h"
#include "jami/conversation_interface.h"

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
        std::lock_guard const lk(pmMtx_);
        ChatHandlerPtr ptr {(static_cast<ChatHandler*>(data))};

        if (!ptr) {
            return -1;
        }
        const auto handlerName = ptr->getChatHandlerDetails().at("name");
        auto* rawPtr = ptr.get();
        PluginPreferencesUtils::addAlwaysHandlerPreference(handlerName,
                                                           ptr->id().substr(0, ptr->id().find_last_of(DIR_SEPARATOR_CH)));

        // Snapshot under mtx_, notify outside: notifyChatSubject may call back into
        // ChatServicesManager and deadlock if mtx_ is still held.
        using NotifyItem = std::pair<std::pair<std::string, std::string>, chatSubjectPtr>;
        std::vector<NotifyItem> toNotify;
        {
            std::lock_guard const guard(mtx_);
            handlersNameMap_[handlerName] = reinterpret_cast<uintptr_t>(rawPtr);
            chatHandlers_.emplace_back(std::move(ptr));

            // Re-activate for conversations that had this handler enabled before (re)load.
            for (auto& [key, allowDenySet] : allowDenyList_) {
                if (const auto it = allowDenySet.find(handlerName);
                    it == allowDenySet.end() || !it->second) {
                    continue;
                }
                auto& subject = chatSubjects_.emplace(key, std::make_shared<PublishObservable<pluginMessagePtr>>())
                                    .first->second;
                chatHandlerToggled_[key].insert(reinterpret_cast<uintptr_t>(rawPtr));
                toNotify.emplace_back(key, subject);
            }
        } // release mtx_

        for (auto& [connection, subject] : toNotify) {
            JAMI_DEBUG("registerChatHandler: re-activating {} for {}/{}",
                       handlerName,
                       connection.first,
                       connection.second);
            rawPtr->notifyChatSubject(connection, subject);
        }

        return 0;
    };

    // unregisterChatHandler may be called by the PluginManager while unloading.
    auto unregisterChatHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard const lk(pmMtx_);

        // Snapshot under mtx_, detach and destroy outside: destructor calls
        // getEnabledConversationsForPlugin which acquires mtx_ — would deadlock.
        ChatHandlerPtr handlerToDestroy;
        using DetachItem = std::pair<ChatHandler*, chatSubjectPtr>;
        std::vector<DetachItem> toDetach;
        {
            std::lock_guard const guard(mtx_);
            const auto handlerIt = std::ranges::find_if(chatHandlers_, [data](const ChatHandlerPtr& h) {
                return h.get() == data;
            });

            if (handlerIt != chatHandlers_.end()) {
                for (auto& [key, toggledSet] : chatHandlerToggled_) {
                    if (toggledSet.erase(reinterpret_cast<uintptr_t>(handlerIt->get()))) {
                        if (auto subjIt = chatSubjects_.find(key);
                            subjIt != chatSubjects_.end()) {
                            toDetach.emplace_back(handlerIt->get(), subjIt->second);
                        }
                    }
                }
                handlersNameMap_.erase((*handlerIt)->getChatHandlerDetails().at("name"));
                handlerToDestroy = std::move(*handlerIt);
                chatHandlers_.erase(handlerIt);
            }
        } // release mtx_

        for (auto& [handler, subject] : toDetach) {
            handler->detach(subject);
        }
        handlerToDestroy.reset(); // destructor acquires mtx_ via getEnabledConversationsForPlugin

        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("ChatHandlerManager", registerChatHandler, unregisterChatHandler);
}

void
ChatServicesManager::registerChatService(PluginManager& pluginManager)
{
    pluginManager.registerService("sendTextMessage",
        [](const DLPlugin* p, void* d) { return sendTextMessage(p, static_cast<JamiMessage*>(d)); });
    pluginManager.registerService("editMessage",
        [this](const DLPlugin* p, void* d) { return editMessage(p, static_cast<const JamiMessage*>(d)); });
    pluginManager.registerService("reloadBodyOverwriteConversations",
        [this](const DLPlugin* p, void* d) { return reloadBodyOverwriteConversations(p, d); });
    pluginManager.registerService("updateMessageBodyOverwrite",
        [this](const DLPlugin* p, void* d) { return updateMessageBodyOverwrite(p, static_cast<const MessageBodyOverwriteUpdate*>(d)); });
    pluginManager.registerService("clearSwarmBodyOverwrite",
        [this](const DLPlugin* p, void* d) { return clearSwarmBodyOverwrite(p, d); });
    pluginManager.registerService("bodyOverwriteLoadedConversations",
        [this](const DLPlugin* p, void* d) { return bodyOverwriteLoadedConversations(p, d); });
    pluginManager.registerService("clearSwarmBodyOverwriteConversation",
        [this](const DLPlugin* p, void* d) { return clearSwarmBodyOverwriteConversation(p, static_cast<const std::pair<std::string, std::string>*>(d)); });
    pluginManager.registerService("bodyOverwriteLoadedConversation",
        [this](const DLPlugin* p, void* d) { return bodyOverwriteLoadedConversation(p, static_cast<const std::pair<std::string, std::string>*>(d)); });
}

int
ChatServicesManager::sendTextMessage(const DLPlugin*, const JamiMessage* cm)
{
    if (!cm || cm->accountId.empty() || cm->peerId.empty()) {
        return -1;
    }
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(cm->accountId)) {
        try {
            if (cm->isSwarm) {
                acc->convModule()->sendMessage(cm->peerId, cm->data.at("body"));
            } else {
                jami::Manager::instance().sendTextMessage(cm->accountId, cm->peerId, cm->data, true);
            }
        } catch (const std::exception& e) {
            JAMI_ERROR("Exception during text message sending: {}", e.what());
        }
    }
    return 0;
}

int
ChatServicesManager::reloadBodyOverwriteConversations(const DLPlugin* plugin, void*)
{
    for (const auto& [accountId, convId] : getEnabledConversationsForPlugin(plugin)) {
        if (const auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
            if (auto* convMod = acc->convModule()) {
                if (const auto conv = convMod->getConversation(convId)) {
                    conv->reloadBodyOverwriteMessages();
                }
            }
        }
    }
    return 0;
}

int
ChatServicesManager::updateMessageBodyOverwrite(const DLPlugin* plugin, const MessageBodyOverwriteUpdate* upd)
{
    if (!upd || upd->accountId.empty() || upd->conversationId.empty()) {
        return -1;
    }
    if (const auto enabled = getEnabledConversationsForPlugin(plugin);
        !enabled.contains({upd->accountId, upd->conversationId})) {
        JAMI_WARNING("updateMessageBodyOverwrite: plugin not enabled for {}/{}, rejecting",
                     upd->accountId,
                     upd->conversationId);
        return -1;
    }
    JAMI_DEBUG("updateMessageBodyOverwrite: plugin authorized for {}/{}", upd->accountId, upd->conversationId);
    if (const auto acc = Manager::instance().getAccount<JamiAccount>(upd->accountId)) {
        if (auto* convMod = acc->convModule()) {
            if (const auto conv = convMod->getConversation(upd->conversationId)) {
                conv->updateMessageBodyOverwrite(upd->messageId, upd->bodyOverwrite);
            }
        }
    }
    return 0;
}

int
ChatServicesManager::clearSwarmBodyOverwrite(const DLPlugin* plugin, void*)
{
    for (const auto& [accountId, convId] : getEnabledConversationsForPlugin(plugin)) {
        if (const auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
            if (auto* convMod = acc->convModule()) {
                if (const auto conv = convMod->getConversation(convId)) {
                    conv->clearBodyOverwrites();
                }
            }
        }
    }
    return 0;
}

int
ChatServicesManager::bodyOverwriteLoadedConversations(const DLPlugin* plugin, void*)
{
    for (const auto& [accountId, convId] : getEnabledConversationsForPlugin(plugin)) {
        if (const auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
            if (auto* convMod = acc->convModule()) {
                if (const auto conv = convMod->getConversation(convId)) {
                    conv->loadMissingBodyOverwrites();
                }
            }
        }
    }
    return 0;
}

int
ChatServicesManager::editMessage(const DLPlugin* plugin, const JamiMessage* cm)
{
    if (!cm || cm->accountId.empty() || cm->peerId.empty()) {
        return -1;
    }
    if (const auto enabled = getEnabledConversationsForPlugin(plugin);
        !enabled.contains({cm->accountId, cm->peerId})) {
        JAMI_WARNING("editMessage: plugin not enabled for {}/{}, rejecting", cm->accountId, cm->peerId);
        return -1;
    }
    JAMI_DEBUG("editMessage: plugin authorized for {}/{}", cm->accountId, cm->peerId);
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(cm->accountId)) {
        try {
            const auto bodyIt = cm->data.find(CommitKey::BODY);
            const auto editIt = cm->data.find(CommitKey::EDIT);
            if (bodyIt != cm->data.end() && editIt != cm->data.end()) {
                acc->convModule()->editMessage(cm->peerId, bodyIt->second, editIt->second);
            }
        } catch (const std::exception& e) {
            JAMI_ERROR("Exception during plugin editMessage: {}", e.what());
        }
    }
    return 0;
}

int
ChatServicesManager::clearSwarmBodyOverwriteConversation(const DLPlugin* plugin, const std::pair<std::string, std::string>* p)
{
    if (!p) {
        return 0;
    }
    if (const auto enabled = getEnabledConversationsForPlugin(plugin);
        !enabled.contains({p->first, p->second})) {
        return -1;
    }
    if (const auto acc = Manager::instance().getAccount<JamiAccount>(p->first)) {
        if (auto* convMod = acc->convModule()) {
            if (const auto conv = convMod->getConversation(p->second)) {
                conv->clearBodyOverwrites();
            }
        }
    }
    return 0;
}

int
ChatServicesManager::bodyOverwriteLoadedConversation(const DLPlugin* plugin, const std::pair<std::string, std::string>* p)
{
    if (!p) {
        return 0;
    }
    if (const auto enabled = getEnabledConversationsForPlugin(plugin);
        !enabled.contains({p->first, p->second})) {
        return -1;
    }
    if (const auto acc = Manager::instance().getAccount<JamiAccount>(p->first)) {
        if (auto* convMod = acc->convModule()) {
            if (const auto conv = convMod->getConversation(p->second)) {
                conv->loadMissingBodyOverwrites();
            }
        }
    }
    return 0;
}

void
ChatServicesManager::transformSwarmMessages(std::vector<libjami::SwarmMessage>& messages,
                                            const std::string& accountId,
                                            const std::string& conversationId)
{
    std::lock_guard const lk(mtx_);
    std::pair<std::string, std::string> const key(accountId, conversationId);
    const auto toggledIt = chatHandlerToggled_.find(key);
    const auto allowIt = allowDenyList_.find(key);
    for (auto& handler : chatHandlers_) {
        const auto handlerName = handler->getChatHandlerDetails().at("name");
        const bool toggled = toggledIt != chatHandlerToggled_.end()
                             && toggledIt->second.contains(reinterpret_cast<uintptr_t>(handler.get()));
        bool allowed = false;
        if (allowIt != allowDenyList_.end()) {
            if (const auto it = allowIt->second.find(handlerName); it != allowIt->second.end()) {
                allowed = it->second;
            }
        }
        if (toggled || allowed) {
            handler->transformSwarmMessages(messages, accountId, conversationId);
        }
    }
}

bool
ChatServicesManager::hasHandlers() const
{
    std::lock_guard const lk(mtx_);
    return !chatHandlers_.empty();
}

std::vector<std::string>
ChatServicesManager::getChatHandlers() const
{
    std::lock_guard const lk(mtx_);
    std::vector<std::string> res;
    res.reserve(chatHandlers_.size());
    for (const auto& chatHandler : chatHandlers_) {
        res.emplace_back(std::to_string(reinterpret_cast<uintptr_t>(chatHandler.get())));
    }
    return res;
}

void
ChatServicesManager::publishMessage(const pluginMessagePtr& message)
{
    if (message->fromPlugin) {
        return;
    }

    std::pair<std::string, std::string> mPair(message->accountId, message->peerId);

    // Collect activation work and the subject to publish under the lock, then
    // call notifyChatSubject and publish outside it. notifyChatSubject can block
    // (e.g. a handler that initialises a model synchronously) and may call back
    // into ChatServicesManager, both of which would deadlock if mtx_ is held.
    using ActivateItem = std::pair<ChatHandler*, chatSubjectPtr>;
    std::vector<ActivateItem> toActivate;
    chatSubjectPtr subjectToPublish;

    {
        std::lock_guard const lk(mtx_);
        if (chatHandlers_.empty()) {
            return;
        }

        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatAllowDenySet = allowDenyList_[mPair];

        // All handlers share one subject per {accountId, peerId} — publish once outside
        // the loop to avoid duplicate delivery when multiple handlers are active.
        bool shouldPublish = false;
        for (auto& chatHandler : chatHandlers_) {
            const auto chatHandlerName = chatHandler->getChatHandlerDetails().at("name");
            bool toggle = PluginPreferencesUtils::getAlwaysPreference(
                chatHandler->id().substr(0, chatHandler->id().find_last_of(DIR_SEPARATOR_CH)),
                chatHandlerName,
                message->accountId);
            if (const auto allowedIt = chatAllowDenySet.find(chatHandlerName);
                allowedIt != chatAllowDenySet.end()) {
                toggle = allowedIt->second;
            }
            const bool toggled = handlers.contains(reinterpret_cast<uintptr_t>(chatHandler.get()));
            if (toggle || toggled) {
                auto& subject = chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>())
                                    .first->second;
                if (!toggled) {
                    JAMI_DEBUG("publishMessage: auto-activating handler {} for {}/{}",
                               chatHandlerName,
                               mPair.first,
                               mPair.second);
                    handlers.insert(reinterpret_cast<uintptr_t>(chatHandler.get()));
                    chatAllowDenySet[chatHandlerName] = true;
                    toActivate.emplace_back(chatHandler.get(), subject);
                }
                shouldPublish = true;
            }
        }

        if (shouldPublish) {
            if (const auto subjectIt = chatSubjects_.find(mPair); subjectIt != chatSubjects_.end()) {
                subjectToPublish = subjectIt->second;
            }
        }
    } // release mtx_

    // Activate new handlers outside the lock (notifyChatSubject may call back into us).
    if (!toActivate.empty()) {
        for (auto& [handler, subject] : toActivate) {
            handler->notifyChatSubject(mPair, subject);
        }
        std::lock_guard const lk(mtx_);
        PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList_);
    }

    // Publish outside the lock: plugin callbacks (editMessage → publishMessage) would deadlock.
    if (subjectToPublish) {
        subjectToPublish->publish(message);
    }
}

void
ChatServicesManager::cleanChatSubjects(const std::string& accountId, const std::string& peerId)
{
    JAMI_DEBUG("cleanChatSubjects: removing subjects for account={} peer={}",
               accountId,
               peerId.empty() ? "(all)" : peerId);
    std::lock_guard const lk(mtx_);
    const std::pair<std::string, std::string> mPair(accountId, peerId);
    for (auto it = chatSubjects_.begin(); it != chatSubjects_.end();) {
        if ((peerId.empty() && it->first.first == accountId) || (!peerId.empty() && it->first == mPair)) {
            it = chatSubjects_.erase(it);
        } else {
            ++it;
        }
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
    std::lock_guard const lk(mtx_);
    const std::pair<std::string, std::string> mPair(accountId, peerId);
    const auto it = allowDenyList_.find(mPair);
    std::vector<std::string> ret;
    if (it != allowDenyList_.end()) {
        for (const auto& [name, enabled] : it->second) {
            if (enabled && handlersNameMap_.contains(name)) {
                ret.emplace_back(std::to_string(handlersNameMap_.at(name)));
            }
        }
    }
    return ret;
}

std::map<std::string, std::string>
ChatServicesManager::getChatHandlerDetails(const std::string& chatHandlerIdStr)
{
    std::lock_guard const lk(mtx_);
    const auto chatHandlerId = std::stoull(chatHandlerIdStr);
    for (auto& chatHandler : chatHandlers_) {
        if (reinterpret_cast<uintptr_t>(chatHandler.get()) == chatHandlerId) {
            return chatHandler->getChatHandlerDetails();
        }
    }
    return {};
}

std::set<std::pair<std::string, std::string>>
ChatServicesManager::getEnabledConversationsForPlugin(const DLPlugin* plugin) const
{
    if (!plugin) {
        return {};
    }

    std::lock_guard const lk(mtx_);
    // Strip the .so filename to get the plugin's rootPath.
    const auto& soPath = plugin->getPath();
    const auto sep = soPath.find_last_of(DIR_SEPARATOR_CH);
    const std::string rootPath = sep != std::string::npos ? soPath.substr(0, sep) : soPath;

    // Collect handler pointers and names that belong to this plugin.
    std::set<uintptr_t> handlerPtrs;
    std::set<std::string> handlerNames;
    for (const auto& handler : chatHandlers_) {
        const auto& hid = handler->id();
        const auto sep2 = hid.find_last_of(DIR_SEPARATOR_CH);
        const std::string handlerRoot = sep2 != std::string::npos ? hid.substr(0, sep2) : hid;
        if (handlerRoot == rootPath) {
            handlerPtrs.insert(reinterpret_cast<uintptr_t>(handler.get()));
            handlerNames.insert(handler->getChatHandlerDetails().at("name"));
        }
    }

    std::set<std::pair<std::string, std::string>> result;

    for (const auto& [key, toggledSet] : chatHandlerToggled_) {
        for (auto ptr : handlerPtrs) {
            if (toggledSet.contains(ptr)) {
                result.insert(key);
                break;
            }
        }
    }

    for (const auto& [key, nameMap] : allowDenyList_) {
        for (const auto& name : handlerNames) {
            if (const auto it = nameMap.find(name); it != nameMap.end() && it->second) {
                result.insert(key);
                break;
            }
        }
    }

    for (const auto& [a, c] : result) {
        JAMI_DEBUG("getEnabledConversationsForPlugin: plugin {} -> {}/{}", rootPath, a, c);
    }

    return result;
}

bool
ChatServicesManager::setPreference(const std::string& key, const std::string& value, const std::string& rootPath)
{
    // Collect matching handlers under mtx_, then call setPreferenceAttribute outside the lock.
    // setPreferenceAttribute callbacks into invokeService (clearSwarmBodyOverwrite,
    // reloadBodyOverwriteConversations) which acquires mtx_ — holding it here deadlocks.
    std::vector<ChatHandler*> toNotify;
    {
        std::lock_guard const lk(mtx_);
        for (auto& chatHandler : chatHandlers_) {
            if (chatHandler->id().find(rootPath) != std::string::npos && chatHandler->preferenceMapHasKey(key)) {
                toNotify.push_back(chatHandler.get());
            }
        }
    } // release mtx_

    for (auto* handler : toNotify) {
        handler->setPreferenceAttribute(key, value);
    }

    return toNotify.empty(); // false = key was handled → caller skips plugin reload
}

void
ChatServicesManager::toggleChatHandler(const uintptr_t chatHandlerId,
                                       const std::string& accountId,
                                       const std::string& peerId,
                                       const bool toggle)
{
    std::pair<std::string, std::string> mPair(accountId, peerId);
    chatSubjectPtr subject;
    ChatHandler* handler = nullptr;

    {
        std::lock_guard const lk(mtx_);
        auto& handlers = chatHandlerToggled_[mPair];
        auto& chatAllowDenySet = allowDenyList_[mPair];
        subject = chatSubjects_.emplace(mPair, std::make_shared<PublishObservable<pluginMessagePtr>>()).first->second;

        auto chatHandlerIt = std::find_if(chatHandlers_.begin(),
                                          chatHandlers_.end(),
                                          [chatHandlerId](const ChatHandlerPtr& h) {
                                              return reinterpret_cast<uintptr_t>(h.get()) == chatHandlerId;
                                          });

        if (chatHandlerIt != chatHandlers_.end()) {
            handler = chatHandlerIt->get();
            const auto handlerName = (*chatHandlerIt)->getChatHandlerDetails().at("name");
            if (toggle) {
                handlers.insert(chatHandlerId);
                chatAllowDenySet[handlerName] = true;
            } else {
                handlers.erase(chatHandlerId);
                chatAllowDenySet[handlerName] = false;
            }
        }
    } // release mtx_

    if (handler) {
        JAMI_DEBUG("toggleChatHandler: {} handler {} for {}/{}",
                   toggle ? "activating" : "deactivating",
                   chatHandlerId,
                   accountId,
                   peerId);
        if (toggle) {
            handler->notifyChatSubject(mPair, subject);
        } else {
            handler->detach(subject);
            if (const auto acc = Manager::instance().getAccount<JamiAccount>(accountId)) {
                if (auto* convMod = acc->convModule()) {
                    if (const auto conv = convMod->getConversation(peerId)) {
                        conv->clearBodyOverwrites();
                        conv->loadMissingBodyOverwrites();
                    }
                }
            }
        }
        std::lock_guard const lk(mtx_);
        PluginPreferencesUtils::setAllowDenyListPreferences(allowDenyList_);
    } else {
        JAMI_DEBUG("toggleChatHandler: handler {} not found", chatHandlerId);
    }
}
} // namespace jami
