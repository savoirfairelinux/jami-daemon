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

#include "callservicesmanager.h"

#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "manager.h"
#include "sip/sipcall.h"
#include "fileutils.h"
#include "logger.h"

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
    auto predicate = [&data](std::pair<const StreamData, AVSubjectSPtr> item) {
        return data.id == item.first.id && data.direction == item.first.direction
               && data.type == item.first.type;
    };
    callAVsubjects_[data.id].remove_if(predicate);

    // callAVsubjects_ emplaces data and subject with callId key to easy of access
    // When call is ended, subjects from this call are erased.
    callAVsubjects_[data.id].emplace_back(data, subject);

    // Search for activation flag.
    for (auto& callMediaHandler : callMediaHandlers_) {
        std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
        // toggle is true if we should automatically activate the MediaHandler.
        bool toggle = PluginPreferencesUtils::getAlwaysPreference(
            callMediaHandler->id().substr(0, found),
            callMediaHandler->getCallMediaHandlerDetails().at("name"),
            data.source);
        // toggle may be overwritten if the MediaHandler was previously activated/deactivated
        // for the given call.
        for (const auto& toggledMediaHandlerPair : mediaHandlerToggled_[data.id]) {
            if (toggledMediaHandlerPair.first == (uintptr_t) callMediaHandler.get()) {
                toggle = toggledMediaHandlerPair.second;
                break;
            }
        }
        if (toggle)
#ifndef __ANDROID__
            // If activation is expected, we call activation function
            toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
#else
            // Due to Android's camera activation process, we don't automaticaly
            // activate the MediaHandler here. But we set it as active
            // and the client-android will handle its activation.
            mediaHandlerToggled_[data.id].insert({(uintptr_t) callMediaHandler.get(), true});
#endif
    }
}

void
CallServicesManager::clearAVSubject(const std::string& callId)
{
    callAVsubjects_.erase(callId);
}

void
CallServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    // registerMediaHandler may be called by the PluginManager upon loading a plugin.
    auto registerMediaHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
        CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

        if (!ptr)
            return -1;
        std::size_t found = ptr->id().find_last_of(DIR_SEPARATOR_CH);
        // Adding preference that tells us to automatically activate a MediaHandler.
        PluginPreferencesUtils::addAlwaysHandlerPreference(ptr->getCallMediaHandlerDetails().at(
                                                               "name"),
                                                           ptr->id().substr(0, found));
        callMediaHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    // unregisterMediaHandler may be called by the PluginManager while unloading.
    auto unregisterMediaHandler = [this](void* data, std::mutex& pmMtx_) {
        std::lock_guard<std::mutex> lk(pmMtx_);
        auto handlerIt = std::find_if(callMediaHandlers_.begin(),
                                      callMediaHandlers_.end(),
                                      [data](CallMediaHandlerPtr& handler) {
                                          return (handler.get() == data);
                                      });

        if (handlerIt != callMediaHandlers_.end()) {
            for (auto& toggledList : mediaHandlerToggled_) {
                auto handlerId = std::find_if(toggledList.second.begin(),
                                              toggledList.second.end(),
                                              [handlerIt](
                                                  std::pair<uintptr_t, bool> handlerIdPair) {
                                                  return handlerIdPair.first
                                                             == (uintptr_t) handlerIt->get()
                                                         && handlerIdPair.second;
                                              });
                // If MediaHandler we're trying to destroy is currently in use, we deactivate it.
                if (handlerId != toggledList.second.end())
                    toggleCallMediaHandler((*handlerId).first, toggledList.first, false);
            }
            callMediaHandlers_.erase(handlerIt);
        }
        return true;
    };

    // Services are registered to the PluginManager.
    pluginManager.registerComponentManager("CallMediaHandlerManager",
                                           registerMediaHandler,
                                           unregisterMediaHandler);
}

std::vector<std::string>
CallServicesManager::getCallMediaHandlers()
{
    std::vector<std::string> res;
    res.reserve(callMediaHandlers_.size());
    for (const auto& mediaHandler : callMediaHandlers_) {
        res.emplace_back(std::to_string((uintptr_t) mediaHandler.get()));
    }
    return res;
}

void
CallServicesManager::toggleCallMediaHandler(const std::string& mediaHandlerId,
                                            const std::string& callId,
                                            const bool toggle)
{
    try {
        toggleCallMediaHandler(std::stoull(mediaHandlerId), callId, toggle);
    } catch (const std::exception& e) {
        JAMI_ERR("Error toggling media handler: %s", e.what());
    }
}

std::map<std::string, std::string>
CallServicesManager::getCallMediaHandlerDetails(const std::string& mediaHandlerIdStr)
{
    auto mediaHandlerId = std::stoull(mediaHandlerIdStr);
    for (auto& mediaHandler : callMediaHandlers_) {
        if ((uintptr_t) mediaHandler.get() == mediaHandlerId) {
            return mediaHandler->getCallMediaHandlerDetails();
        }
    }
    return {};
}

bool
CallServicesManager::isVideoType(const CallMediaHandlerPtr& mediaHandler)
{
    // "dataType" is known from the MediaHandler implementation.
    const auto& details = mediaHandler->getCallMediaHandlerDetails();
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
CallServicesManager::isAttached(const CallMediaHandlerPtr& mediaHandler)
{
    // "attached" is known from the MediaHandler implementation.
    const auto& details = mediaHandler->getCallMediaHandlerDetails();
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
    std::vector<std::string> ret;
    const auto& it = mediaHandlerToggled_.find(callId);
    if (it != mediaHandlerToggled_.end())
        for (const auto& mediaHandlerId : it->second)
            if (mediaHandlerId.second) // Only return active MediaHandler ids
                ret.emplace_back(std::to_string(mediaHandlerId.first));
    return ret;
}

bool
CallServicesManager::setPreference(const std::string& key,
                                   const std::string& value,
                                   const std::string& rootPath)
{
    bool status {true};
    for (auto& mediaHandler : callMediaHandlers_) {
        if (mediaHandler->id().find(rootPath) != std::string::npos) {
            if (mediaHandler->preferenceMapHasKey(key)) {
                mediaHandler->setPreferenceAttribute(key, value);
                status &= false;
            }
        }
    }
    return status;
}

void
CallServicesManager::clearCallHandlerMaps(const std::string& callId)
{
    mediaHandlerToggled_.erase(callId);
}

void
CallServicesManager::notifyAVSubject(CallMediaHandlerPtr& callMediaHandlerPtr,
                                     const StreamData& data,
                                     AVSubjectSPtr& subject)
{
    if (auto soSubject = subject.lock())
        callMediaHandlerPtr->notifyAVFrameSubject(data, soSubject);
}

void
CallServicesManager::toggleCallMediaHandler(const uintptr_t mediaHandlerId,
                                            const std::string& callId,
                                            const bool toggle)
{
    auto& handlers = mediaHandlerToggled_[callId];
    bool applyRestart = false;

    for (auto subject : callAVsubjects_[callId]) {
        auto handlerIt = std::find_if(callMediaHandlers_.begin(),
                                      callMediaHandlers_.end(),
                                      [mediaHandlerId](CallMediaHandlerPtr& handler) {
                                          return ((uintptr_t) handler.get() == mediaHandlerId);
                                      });

        if (handlerIt != callMediaHandlers_.end()) {
            if (toggle) {
                notifyAVSubject((*handlerIt), subject.first, subject.second);
                if (isAttached((*handlerIt)))
                    handlers[mediaHandlerId] = true;
            } else {
                (*handlerIt)->detach();
                handlers[mediaHandlerId] = false;
            }
            if (subject.first.type == StreamType::video && isVideoType((*handlerIt)))
                applyRestart = true;
        }
    }
#ifndef __ANDROID__
#ifdef ENABLE_VIDEO
    if (applyRestart) {
        auto call = Manager::instance().callFactory.getCall<SIPCall>(callId);
        if (call && !call->isConferenceParticipant()) {
            for (auto const& videoRtp: call->getRtpSessionList(MediaType::MEDIA_VIDEO))
                videoRtp->restartSender();
        }
    }
#endif
#endif
}
} // namespace jami
