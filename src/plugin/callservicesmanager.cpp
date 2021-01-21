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

#pragma once

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
    // This guarantees unicity of subjects by id
    callAVsubjects_.emplace_back(data, subject);

    for (auto& callMediaHandler : callMediaHandlers_) {
        std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
        auto preferences = PluginPreferencesUtils::getPreferencesValuesMap(callMediaHandler->id().substr(0, found));
#ifndef __ANDROID__
        if (preferences.at("always") == "1")
            toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
        else
#endif
            for (const auto& toggledMediaHandler : mediaHandlerToggled_[data.id]) {
                if (toggledMediaHandler == (uintptr_t) callMediaHandler.get()) {
                    toggleCallMediaHandler(toggledMediaHandler, data.id, true);
                    break;
                }
            }
    }
}

void
CallServicesManager::clearAVSubject(const std::string& callId)
{
    for (auto it = callAVsubjects_.begin(); it != callAVsubjects_.end();) {
        if (it->first.id == callId) {
            it = callAVsubjects_.erase(it);
        } else {
            ++it;
        }
    }
}

void
CallServicesManager::registerComponentsLifeCycleManagers(PluginManager& pm)
{
    auto registerCallMediaHandler = [this](void* data) {
        CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

        if (!ptr)
            return -1;
        callMediaHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    auto unregisterMediaHandler = [this](void* data) {
        for (auto it = callMediaHandlers_.begin(); it != callMediaHandlers_.end(); ++it) {
            if (it->get() == data) {
                callMediaHandlers_.erase(it);
                break;
            }
        }
        return 0;
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
    toggleCallMediaHandler(std::stoull(mediaHandlerId), callId, toggle);
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
    if (it != mediaHandlerToggled_.end()) {
        ret.reserve(it->second.size());
        for (const auto& mediaHandlerId : it->second)
            ret.emplace_back(std::to_string(mediaHandlerId));
    }
    return ret;
}

void
CallServicesManager::setPreference(const std::string& key, const std::string& value, const std::string& scopeStr)
{
    for (auto& mediaHandler : callMediaHandlers_) {
        if (scopeStr.find(mediaHandler->getCallMediaHandlerDetails()["name"])
            != std::string::npos) {
            mediaHandler->setPreferenceAttribute(key, value);
        }
    }
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

    for (auto subject : callAVsubjects_) {
        if (subject.first.id == callId) {

            auto handlerIt = std::find_if(callMediaHandlers_.begin(), callMediaHandlers_.end(),
                    [mediaHandlerId](CallMediaHandlerPtr& handler) {
                        return ((uintptr_t) handler.get() == mediaHandlerId);
                        });

            if (handlerIt != callMediaHandlers_.end()) {
                if (toggle) {
                    notifyAVSubject((*handlerIt), subject.first, subject.second);
                    if (isAttached((*handlerIt))
                        && handlers.find(mediaHandlerId) == handlers.end())
                        handlers.insert(mediaHandlerId);
                } else {
                    (*handlerIt)->detach();
                    handlers.erase(mediaHandlerId);
                }
                if (subject.first.type == StreamType::video && isVideoType((*handlerIt)))
                    applyRestart = true;
            }
        }
    }
#ifndef __ANDROID__
    if (applyRestart)
        Manager::instance().callFactory.getCall<SIPCall>(callId)->getVideoRtp().restartSender();
#endif
}
} // namespace jami
