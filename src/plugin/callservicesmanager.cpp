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

#include "callservicesmanager.h"

#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "manager.h"
#include "sip/sipcall.h"
#include "logger.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <vector>

namespace jami {

CallServicesManager::CallServicesManager(PluginManager& pluginManager)
{
    registerComponentsLifeCycleManagers(pluginManager);
}

CallServicesManager::~CallServicesManager()
{
    callMediaHandlers_.clear();
    callAVsubjects_.clear();
    mediaHandlerToggled_.clear();
}

void
CallServicesManager::createAVSubject(const StreamData& data, AVSubjectSPtr subject)
{
    auto operation = operationState_.acquire();
    std::vector<uintptr_t> handlersToToggle;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        auto predicate = [&data](const std::pair<const StreamData, AVSubjectSPtr>& item) {
            return data.id == item.first.id && data.direction == item.first.direction && data.type == item.first.type;
        };
        callAVsubjects_[data.id].remove_if(predicate);

        // callAVsubjects_ emplaces data and subject with callId key to easy of access
        // When call is ended, subjects from this call are erased.
        callAVsubjects_[data.id].emplace_back(data, subject);

        // Search for activation flag.
        auto& toggledHandlers = mediaHandlerToggled_[data.id];
        for (auto& callMediaHandler : callMediaHandlers_) {
            const auto handlerId = reinterpret_cast<uintptr_t>(callMediaHandler.get());
            const auto nameIt = handlerNames_.find(handlerId);
            if (nameIt == handlerNames_.end())
                continue;

            // toggle is true if we should automatically activate the MediaHandler.
            bool toggle = PluginPreferencesUtils::getAlwaysPreference(
                std::filesystem::path(callMediaHandler->id()).parent_path().string(),
                nameIt->second,
                data.source);
            // toggle may be overwritten if the MediaHandler was previously activated/deactivated
            // for the given call.
            if (const auto toggledIt = toggledHandlers.find(handlerId); toggledIt != toggledHandlers.end()) {
                toggle = toggledIt->second;
            }
            if (toggle)
#ifndef __ANDROID__
                handlersToToggle.emplace_back(handlerId);
#else
                // Due to Android's camera activation process, we don't automaticaly
                // activate the MediaHandler here. But we set it as active
                // and the client-android will handle its activation.
                toggledHandlers[handlerId] = true;
#endif
        }
    }
#ifndef __ANDROID__
    for (const auto handlerId : handlersToToggle)
        toggleCallMediaHandler(handlerId, data.id, true);
#endif
}

void
CallServicesManager::clearAVSubject(const std::string& callId)
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    callAVsubjects_.erase(callId);
}

void
CallServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // registerMediaHandler may be called by the PluginManager upon loading a plugin.
    auto registerMediaHandler = [this](void* data, std::mutex&) {
        CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

        if (!ptr)
            return -1;
        const auto details = ptr->getCallMediaHandlerDetails();
        const auto handlerName = details.at("name");
        const auto handlerId = reinterpret_cast<uintptr_t>(ptr.get());
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.waitUntilReady(lk);
        // Adding preference that tells us to automatically activate a MediaHandler.
        PluginPreferencesUtils::addAlwaysHandlerPreference(
            handlerName, std::filesystem::path(ptr->id()).parent_path().string());
        handlerNames_[handlerId] = handlerName;
        callMediaHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterMediaHandler may be called by the PluginManager while unloading.
    auto unregisterMediaHandler = [this](void* data, std::mutex&) {
        CallMediaHandlerPtr removedHandler;
        std::vector<std::string> affectedCalls;
        std::unique_lock<std::mutex> lk(operationState_.mutex());
        operationState_.beginUnload(lk);
        auto handlerIt = std::find_if(callMediaHandlers_.begin(),
                                      callMediaHandlers_.end(),
                                      [data](CallMediaHandlerPtr& handler) { return (handler.get() == data); });

        if (handlerIt != callMediaHandlers_.end()) {
            const auto handlerId = reinterpret_cast<uintptr_t>(handlerIt->get());
            removedHandler = std::move(*handlerIt);
            callMediaHandlers_.erase(handlerIt);
            handlerNames_.erase(handlerId);

            std::set<std::string> callsToDetach;
            for (auto& toggledList : mediaHandlerToggled_) {
                if (const auto activeIt = toggledList.second.find(handlerId); activeIt != toggledList.second.end()) {
                    if (activeIt->second)
                        callsToDetach.insert(toggledList.first);
                    toggledList.second.erase(activeIt);
                }
            }
            affectedCalls.assign(callsToDetach.begin(), callsToDetach.end());
        }

        lk.unlock();
        const bool restartVideo = removedHandler && isVideoType(*removedHandler);
        if (removedHandler && !affectedCalls.empty())
            removedHandler->detach();
        if (restartVideo) {
            for (const auto& callId : affectedCalls)
                restartSender(callId);
        }
        lk.lock();
        operationState_.endUnload(lk);
        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("CallMediaHandlerManager", registerMediaHandler, unregisterMediaHandler);
}

std::vector<std::string>
CallServicesManager::getCallMediaHandlers()
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    std::vector<std::string> res;
    res.reserve(callMediaHandlers_.size());
    for (const auto& mediaHandler : callMediaHandlers_) {
        res.emplace_back(std::to_string(reinterpret_cast<uintptr_t>(mediaHandler.get())));
    }
    return res;
}

void
CallServicesManager::toggleCallMediaHandler(const std::string& mediaHandlerId,
                                            const std::string& callId,
                                            const bool toggle)
{
    auto operation = operationState_.acquire();
    try {
        toggleCallMediaHandler(std::stoull(mediaHandlerId), callId, toggle);
    } catch (const std::exception& e) {
        JAMI_ERR("Error toggling media handler: %s", e.what());
    }
}

std::map<std::string, std::string>
CallServicesManager::getCallMediaHandlerDetails(const std::string& mediaHandlerIdStr)
{
    auto operation = operationState_.acquire();
    auto mediaHandlerId = std::stoull(mediaHandlerIdStr);
    CallMediaHandler* handler = nullptr;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& mediaHandler : callMediaHandlers_) {
            if (reinterpret_cast<uintptr_t>(mediaHandler.get()) == mediaHandlerId) {
                handler = mediaHandler.get();
                break;
            }
        }
    }
    if (handler)
        return handler->getCallMediaHandlerDetails();
    return {};
}

bool
CallServicesManager::isVideoType(CallMediaHandler& mediaHandler)
{
    // "dataType" is known from the MediaHandler implementation.
    const auto& details = mediaHandler.getCallMediaHandlerDetails();
    const auto& it = details.find("dataType");
    if (it != details.end()) {
        bool status;
        std::istringstream(it->second) >> status;
        return status;
    }
    // If there is no "dataType" returned, it's safer to return True and allow
    // sender to restart.
    return true;
}

bool
CallServicesManager::isAttached(CallMediaHandler& mediaHandler)
{
    // "attached" is known from the MediaHandler implementation.
    const auto& details = mediaHandler.getCallMediaHandlerDetails();
    const auto& it = details.find("attached");
    if (it != details.end()) {
        bool status;
        std::istringstream(it->second) >> status;
        return status;
    }
    return true;
}

std::vector<std::string>
CallServicesManager::getCallMediaHandlerStatus(const std::string& callId)
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    std::vector<std::string> ret;
    const auto& it = mediaHandlerToggled_.find(callId);
    if (it != mediaHandlerToggled_.end())
        for (const auto& mediaHandlerId : it->second)
            if (mediaHandlerId.second) // Only return active MediaHandler ids
                ret.emplace_back(std::to_string(mediaHandlerId.first));
    return ret;
}

bool
CallServicesManager::setPreference(const std::string& key, const std::string& value, const std::string& rootPath)
{
    auto operation = operationState_.acquire();
    std::vector<CallMediaHandler*> handlers;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        for (auto& mediaHandler : callMediaHandlers_) {
            if (mediaHandler->id().find(rootPath) != std::string::npos)
                handlers.emplace_back(mediaHandler.get());
        }
    }

    bool status {true};
    for (auto* mediaHandler : handlers) {
        if (mediaHandler->preferenceMapHasKey(key)) {
            mediaHandler->setPreferenceAttribute(key, value);
            status &= false;
        }
    }
    return status;
}

void
CallServicesManager::clearCallHandlerMaps(const std::string& callId)
{
    auto operation = operationState_.acquire();
    std::lock_guard<std::mutex> lk(operationState_.mutex());
    mediaHandlerToggled_.erase(callId);
}

void
CallServicesManager::notifyAVSubject(CallMediaHandler& callMediaHandlerPtr,
                                     const StreamData& data,
                                     AVSubjectSPtr& subject)
{
    if (auto soSubject = subject.lock())
        callMediaHandlerPtr.notifyAVFrameSubject(data, soSubject);
}

void
CallServicesManager::toggleCallMediaHandler(const uintptr_t mediaHandlerId, const std::string& callId, const bool toggle)
{
    CallMediaHandler* handler = nullptr;
    std::vector<std::pair<StreamData, AVSubjectSPtr>> subjects;
    {
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        auto handlerIt = std::find_if(callMediaHandlers_.begin(),
                                      callMediaHandlers_.end(),
                                      [mediaHandlerId](CallMediaHandlerPtr& currentHandler) {
                                          return reinterpret_cast<uintptr_t>(currentHandler.get()) == mediaHandlerId;
                                      });
        if (handlerIt == callMediaHandlers_.end())
            return;

        handler = handlerIt->get();
        if (const auto subjectIt = callAVsubjects_.find(callId); subjectIt != callAVsubjects_.end()) {
            subjects.reserve(subjectIt->second.size());
            for (const auto& subject : subjectIt->second)
                subjects.emplace_back(subject.first, subject.second);
        }
    }

    const bool hasVideoSubject = std::any_of(subjects.begin(), subjects.end(), [](const auto& subject) {
        return subject.first.type == StreamType::video;
    });
    const bool restartVideo = hasVideoSubject && isVideoType(*handler);

    if (toggle) {
        bool attached = false;
        for (auto& subject : subjects) {
            notifyAVSubject(*handler, subject.first, subject.second);
            attached = isAttached(*handler) || attached;
        }
        if (attached) {
            std::lock_guard<std::mutex> lk(operationState_.mutex());
            mediaHandlerToggled_[callId][mediaHandlerId] = true;
        }
    } else {
        if (!subjects.empty())
            handler->detach();
        std::lock_guard<std::mutex> lk(operationState_.mutex());
        mediaHandlerToggled_[callId][mediaHandlerId] = false;
    }

#ifndef __ANDROID__
#ifdef ENABLE_VIDEO
    if (restartVideo)
        restartSender(callId);
#endif
#endif
}

void
CallServicesManager::restartSender(const std::string& callId)
{
#ifdef ENABLE_VIDEO
    auto call = Manager::instance().callFactory.getCall<SIPCall>(callId);
    if (call && !call->isConferenceParticipant()) {
        for (auto const& videoRtp : call->getRtpSessionList(MediaType::MEDIA_VIDEO))
            videoRtp->restartSender();
    }
#else
    (void) callId;
#endif
}
} // namespace jami
