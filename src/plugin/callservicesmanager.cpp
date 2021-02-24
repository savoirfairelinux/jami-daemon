/**
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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
#include "logger.h"
#include "sip/sipcall.h"
#include "fileutils.h"
#include "pluginpreferencesutils.h"
#include "manager.h"

namespace jami {

CallServicesManager::CallServicesManager(PluginManager& pm)
{
    registerComponentsLifeCycleManagers(pm);
}

CallServicesManager::~CallServicesManager()
{
    callMediaHandlers_.clear();
}

void
CallServicesManager::createAVSubject(const StreamData& data, AVSubjectSPtr subject)
{
    /// callAVsubjects_ emplaces data and subject with callId key to easy of access
    /// When call is ended, subjects from this call are erased.
    callAVsubjects_[data.id].emplace_back(data, subject);

    for (auto& callMediaHandler : callMediaHandlers_) {
        std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
        bool toggle = PluginPreferencesUtils::getAlwaysPreference(
            callMediaHandler->id().substr(0, found),
            callMediaHandler->getCallMediaHandlerDetails().at("name"));
        for (const auto& toggledMediaHandlerPair : mediaHandlerToggled_[data.id]) {
            if (toggledMediaHandlerPair.first == (uintptr_t) callMediaHandler.get()) {
                toggle = toggledMediaHandlerPair.second;
                break;
            }
        }
        if (toggle)
#ifndef __ANDROID__
            toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
#else
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
CallServicesManager::registerComponentsLifeCycleManagers(PluginManager& pm)
{
    auto registerCallMediaHandler = [this](void* data) {
        CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

        if (!ptr)
            return -1;
        std::size_t found = ptr->id().find_last_of(DIR_SEPARATOR_CH);
        PluginPreferencesUtils::addAlwaysHandlerPreference(ptr->getCallMediaHandlerDetails().at(
                                                               "name"),
                                                           ptr->id().substr(0, found));
        callMediaHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    auto unregisterMediaHandler = [this](void* data) {
        auto handlerIt = std::find_if(callMediaHandlers_.begin(),
                                      callMediaHandlers_.end(),
                                      [data](CallMediaHandlerPtr& handler) {
                                          return (handler.get() == data);
                                      });

        if (handlerIt != callMediaHandlers_.end()) {
            for (auto& toggledList : mediaHandlerToggled_) {
                auto handlerId = std::find_if(toggledList.second.begin(),
                                              toggledList.second.end(),
                                              [this, handlerIt](
                                                  std::pair<uintptr_t, bool> handlerIdPair) {
                                                  return handlerIdPair.first
                                                             == (uintptr_t) handlerIt->get()
                                                         && handlerIdPair.second;
                                              });
                if (handlerId != toggledList.second.end())
                    toggleCallMediaHandler((*handlerId).first, toggledList.first, false);
            }
            callMediaHandlers_.erase(handlerIt);
        }
        return true;
    };

    pm.registerComponentManager("CallMediaHandlerManager",
                                registerCallMediaHandler,
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
    const auto& details = mediaHandler->getCallMediaHandlerDetails();
    const auto& it = details.find("dataType");
    if (it != details.end()) {
        bool status;
        std::istringstream(it->second) >> status;
        return status;
    }
    return true;
}

bool
CallServicesManager::isAttached(const CallMediaHandlerPtr& mediaHandler)
{
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
            if (mediaHandlerId.second)
                ret.emplace_back(std::to_string(mediaHandlerId.first));
    return ret;
}

void
CallServicesManager::setPreference(const std::string& key,
                                   const std::string& value,
                                   const std::string& scopeStr)
{
    for (auto& mediaHandler : callMediaHandlers_) {
        if (scopeStr.find(mediaHandler->getCallMediaHandlerDetails()["name"]) != std::string::npos) {
            mediaHandler->setPreferenceAttribute(key, value);
        }
    }
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
        if (subject.first.id == callId) {
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
    }
#ifndef __ANDROID__
    if (applyRestart) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(callId)) {
            call->getVideoRtp().restartSender();
        }
    }
#endif
}
} // namespace jami
