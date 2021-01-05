﻿/**
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
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
// Utils
#include "noncopyable.h"
#include "logger.h"
#include "manager.h"
#include "sip/sipcall.h"
// Plugin Manager
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
// STL
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
    ~CallServicesManager() { callMediaHandlers.clear(); }

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
        callAVsubjects.emplace_back(data, subject);

        for (auto& callMediaHandler : callMediaHandlers) {
            std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
            auto preferences = getPluginPreferencesValuesMapInternal(
                callMediaHandler->id().substr(0, found));
#ifndef __ANDROID__
            if (preferences.at("always") == "1")
                toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
            else
#endif
                for (const auto& toggledMediaHandler : mediaHandlerToggled_[data.id]) {
                    toggleCallMediaHandler(toggledMediaHandler, data.id, true);
                }
        }
    }

    void clearAVSubject(const std::string& callId)
    {
        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end();) {
            if (it->first.id == callId) {
                it = callAVsubjects.erase(it);
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

            if (ptr) {
                callMediaHandlers.emplace_back(std::move(ptr));
            }
            return 0;
        };

        auto unregisterMediaHandler = [this](void* data) {
            for (auto it = callMediaHandlers.begin(); it != callMediaHandlers.end(); ++it) {
                if (it->get() == data) {
                    callMediaHandlers.erase(it);
                    break;
                }
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
        res.reserve(callMediaHandlers.size());
        for (const auto& mediaHandler : callMediaHandlers) {
            res.emplace_back(std::to_string((uintptr_t) mediaHandler.get()));
        }
        return res;
    }

    /**
     * @brief toggleCallMediaHandler
     * Toggle CallMediaHandler, if on, notify with new subjects
     * if off, detach it
     * @param id
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
        for (auto& mediaHandler : callMediaHandlers) {
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
        for (auto& mediaHandler : callMediaHandlers) {
            if (scopeStr.find(mediaHandler->getCallMediaHandlerDetails()["name"])
                != std::string::npos) {
                mediaHandler->setPreferenceAttribute(key, value);
            }
        }
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

    /**
     * @brief toggleCallMediaHandler
     * Toggle CallMediaHandler, if on, notify with new subjects
     * if off, detach it
     * @param id
     */
    void toggleCallMediaHandler(const uintptr_t mediaHandlerId,
                                const std::string& callId,
                                const bool toggle)
    {
        auto& handlers = mediaHandlerToggled_[callId];

        bool applyRestart = false;
        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end(); ++it) {
            if (it->first.id == callId) {
                for (auto& mediaHandler : callMediaHandlers) {
                    if ((uintptr_t) mediaHandler.get() == mediaHandlerId) {
                        if (toggle) {
                            notifyAVSubject(mediaHandler, it->first, it->second);
                            if (isAttached(mediaHandler)
                                && handlers.find(mediaHandlerId) == handlers.end())
                                handlers.insert(mediaHandlerId);
                        } else {
                            mediaHandler->detach();
                            handlers.erase(mediaHandlerId);
                        }
                        if (it->first.type == StreamType::video && isVideoType(mediaHandler))
                            applyRestart = true;
                        break;
                    }
                }
            }
        }

        /* In the case when the mediaHandler receives a hardware format
         * frame and converts it to main memory, we need to restart the
         * sender to unlink ours encoder and decoder.
         *
         * When we deactivate a mediaHandler, we try to relink the encoder
         * and decoder by restarting the sender.
         */
        if (applyRestart)
            Manager::instance().callFactory.getCall<SIPCall>(callId)->getVideoRtp().restartSender();
    }

    /**
     * @brief callMediaHandlers
     * Components that a plugin can register through registerCallMediaHandler service
     * These objects can then be notified with notifySubject
     * whenever there is a new CallAVSubject like a video receive
     */
    std::list<CallMediaHandlerPtr> callMediaHandlers;

    /**
     * @brief callAVsubjects
     * When there is a SIPCall, CallAVSubjects are created there
     * Here we keep a reference to them in order to make them interact with
     * CallMediaHandlers
     * It is pushed to this list list
     */
    std::list<std::pair<const StreamData, AVSubjectSPtr>> callAVsubjects;

    std::map<std::string, std::set<uintptr_t>> mediaHandlerToggled_; // callId, list of mediaHandlers
};

} // namespace jami
