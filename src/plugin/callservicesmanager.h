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

#include "noncopyable.h"
#include "logger.h"
#include "manager.h"
#include "sip/sipcall.h"
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
#include <list>
#include <set>
#include <tuple>

namespace jami {
using MediaHandlerPtr = std::unique_ptr<MediaHandler>;
using CallMediaHandlerPtr = std::unique_ptr<CallMediaHandler>;
using AVSubjectSPtr = std::weak_ptr<Observable<AVFrame*>>;

class CallServicesManager
{
public:
    CallServicesManager(PluginManager& pm) { registerComponentsLifeCycleManagers(pm); }

    /**
     *   unload all media handlers
     **/
    ~CallServicesManager() { callMediaHandlers_.clear(); }

    NON_COPYABLE(CallServicesManager);

public:
    /**
     * @brief createAVSubject
     * @param data
     * Creates an av frame subject with properties StreamData
     */
    void createAVSubject(const StreamData& data, AVSubjectSPtr subject)
    {
        // This guarantees unicity of subjects by id
        callAVsubjects_.emplace_back(data, subject);
        auto& callDenySet = denyList_[data.id];

        for (auto& callMediaHandler : callMediaHandlers_) {
            if (callDenySet.find((uintptr_t) callMediaHandler.get()) == callDenySet.end()) {
                std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
                bool toggle = PluginPreferencesUtils::getAlwaysPreference(
                    callMediaHandler->id().substr(0, found), callMediaHandler->getCallMediaHandlerDetails().at("name"));

                if (!toggle)
                    for (const auto& toggledMediaHandler : mediaHandlerToggled_[data.id]) {
                        if (toggledMediaHandler == (uintptr_t) callMediaHandler.get()) {
                            toggleCallMediaHandler(toggledMediaHandler, data.id, true);
                            break;
                        }
                    }
                else {
#ifndef __ANDROID__
                    toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
#else
                    mediaHandlerToggled_[data.id].insert((uintptr_t) callMediaHandler.get());
#endif
                }
            }
        }
    }

    void clearAVSubject(const std::string& callId)
    {
        for (auto it = callAVsubjects_.begin(); it != callAVsubjects_.end();) {
            if (it->first.id == callId) {
                it = callAVsubjects_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief registerComponentsLifeCycleManagers
     * Exposes components life cycle managers to the main API
     */
    void registerComponentsLifeCycleManagers(PluginManager& pm)
    {
        auto registerCallMediaHandler = [this](void* data) {
            CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

            if (!ptr)
                return -1;
            std::size_t found = ptr->id().find_last_of(DIR_SEPARATOR_CH);
            PluginPreferencesUtils::addAlwaysHandlerPreference(ptr->getCallMediaHandlerDetails().at("name"), ptr->id().substr(0, found));
            callMediaHandlers_.emplace_back(std::move(ptr));
            return 0;
        };

        auto unregisterMediaHandler = [this](void* data) {
            auto handlerIt = std::find_if(callMediaHandlers_.begin(), callMediaHandlers_.end(),
                                [data](CallMediaHandlerPtr& handler) {
                                    return (handler.get() == data);
                                    });

            if (handlerIt != callMediaHandlers_.end()) {
                for (auto& toggledList: mediaHandlerToggled_) {
                    auto handlerId = std::find_if(toggledList.second.begin(), toggledList.second.end(),
                                [this, handlerIt](uintptr_t handlerId) {
                                    return (handlerId == (uintptr_t) handlerIt->get());
                                    });
                    if (handlerId != toggledList.second.end()) {
                        (*handlerIt)->detach();
                        toggledList.second.erase(handlerId);
                    }
                }
                callMediaHandlers_.erase(handlerIt);
                delete (*handlerIt).get();
            }
            return 0;
        };

        pm.registerComponentManager("CallMediaHandlerManager",
                                    registerCallMediaHandler,
                                    unregisterMediaHandler);
    }

    /**
     * @brief getCallMediaHandlers
     * List all call media handlers
     * @return
     */
    std::vector<std::string> getCallMediaHandlers()
    {
        std::vector<std::string> res;
        res.reserve(callMediaHandlers_.size());
        for (const auto& mediaHandler : callMediaHandlers_) {
            res.emplace_back(std::to_string((uintptr_t) mediaHandler.get()));
        }
        return res;
    }

    /**
     * @brief toggleCallMediaHandler
     * Toggle CallMediaHandler, if on, notify with new subjects
     * if off, detach it
     * @param mediaHandler ID handler ID
     * @param callId call ID
     * @param toggle notify with new subjects if true, detach if false.
     * 
     * In the case when the mediaHandler receives a hardware format
     * frame and converts it to main memory, we need to restart the
     * sender to unlink ours encoder and decoder.
     *
     * When we deactivate a mediaHandler, we try to relink the encoder
     * and decoder by restarting the sender.
     */
    void toggleCallMediaHandler(const std::string& mediaHandlerId,
                                const std::string& callId,
                                const bool toggle)
    {
        toggleCallMediaHandler(std::stoull(mediaHandlerId), callId, toggle);
    }

    /**
     * @brief getCallMediaHandlerDetails
     * @param id of the call media handler
     * @return map of Call Media Handler Details
     */
    std::map<std::string, std::string> getCallMediaHandlerDetails(
        const std::string& mediaHandlerIdStr)
    {
        auto mediaHandlerId = std::stoull(mediaHandlerIdStr);
        for (auto& mediaHandler : callMediaHandlers_) {
            if ((uintptr_t) mediaHandler.get() == mediaHandlerId) {
                return mediaHandler->getCallMediaHandlerDetails();
            }
        }
        return {};
    }

    bool isVideoType(const CallMediaHandlerPtr& mediaHandler)
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

    bool isAttached(const CallMediaHandlerPtr& mediaHandler)
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

    std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId)
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

    void setPreference(const std::string& key, const std::string& value, const std::string& scopeStr)
    {
        for (auto& mediaHandler : callMediaHandlers_) {
            if (scopeStr.find(mediaHandler->getCallMediaHandlerDetails()["name"])
                != std::string::npos) {
                mediaHandler->setPreferenceAttribute(key, value);
            }
        }
    }

    void clearCallHandlerMaps(const std::string& callId) {
        mediaHandlerToggled_.erase(callId);
        denyList_.erase(callId);
    }

private:
    /**
     * @brief notifyAVSubject
     * @param callMediaHandlerPtr
     * @param data
     * @param subject
     */
    void notifyAVSubject(CallMediaHandlerPtr& callMediaHandlerPtr,
                         const StreamData& data,
                         AVSubjectSPtr& subject)
    {
        if (auto soSubject = subject.lock())
            callMediaHandlerPtr->notifyAVFrameSubject(data, soSubject);
    }

    void toggleCallMediaHandler(const uintptr_t mediaHandlerId,
                                const std::string& callId,
                                const bool toggle)
    {
        auto& handlers = mediaHandlerToggled_[callId];
        auto& callDenySet = denyList_[callId];

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
                        callDenySet.erase(mediaHandlerId);
                    } else {
                        (*handlerIt)->detach();
                        handlers.erase(mediaHandlerId);
                        callDenySet.insert(mediaHandlerId);
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

    /**
     * @brief callMediaHandlers_
     * Components that a plugin can register through registerCallMediaHandler service
     * These objects can then be notified with notifySubject
     * whenever there is a new CallAVSubject like a video receive
     */
    std::list<CallMediaHandlerPtr> callMediaHandlers_;

    /**
     * @brief callAVsubjects_
     * When there is a SIPCall, CallAVSubjects_ are created there
     * Here we keep a reference to them in order to make them interact with
     * CallMediaHandlers_
     * It is pushed to this list list
     */
    std::list<std::pair<const StreamData, AVSubjectSPtr>> callAVsubjects_;

    /**
     * @brief mediaHandlerToggled_
     * A map of callId and list of mediaHandlers pointers str
     */
    std::map<std::string, std::set<uintptr_t>> mediaHandlerToggled_; // callId, list of mediaHandlers

    std::map<std::string, std::set<uintptr_t>> denyList_{};
};

} // namespace jami
