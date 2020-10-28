/**
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
        callAVsubjects.push_back(std::make_pair(data, subject));
        for (const auto& toggledMediaHandler : mediaHandlerToggled_[data.id]) {
            toggleCallMediaHandler(toggledMediaHandler, data.id, true);
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
     * @brief listCallMediaHandlers
     * List all call media handlers
     * @return
     */
    std::vector<std::string> listCallMediaHandlers()
    {
        std::vector<std::string> res;
        for (const auto& mediaHandler : callMediaHandlers) {
            res.emplace_back(getCallHandlerId(mediaHandler));
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
        if (mediaHandlerId.empty() || callId.empty())
            return;

        auto find = mediaHandlerToggled_.find(callId);
        if (find == mediaHandlerToggled_.end())
            mediaHandlerToggled_[callId] = {};

        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end(); ++it) {
            if (it->first.id == callId) {
                for (auto& mediaHandler : callMediaHandlers) {
                    if (getCallHandlerId(mediaHandler) == mediaHandlerId) {
                        if (toggle) {
                            if (mediaHandlerToggled_[callId].find(mediaHandlerId)
                                == mediaHandlerToggled_[callId].end())
                                mediaHandlerToggled_[callId].insert(mediaHandlerId);
                            listAvailableSubjects(callId, mediaHandler);
                            /* In the case when the mediHandler receives a hardware format
                             * frame and converts it to main memory, we need to restart the
                             * sender to unlink ours encoder and decoder. 
                             */
                            Manager::instance()
                                .callFactory.getCall<SIPCall>(callId)
                                ->getVideoRtp()
                                .restartSender();
                        } else {
                            mediaHandler->detach();
                            if (mediaHandlerToggled_[callId].find(mediaHandlerId)
                                != mediaHandlerToggled_[callId].end())
                                mediaHandlerToggled_[callId].erase(mediaHandlerId);
                            /* When we deactivate a mediaHandler, we try to relink the encoder
                             * and decoder by restarting the sender.
                             */
                            Manager::instance()
                                .callFactory.getCall<SIPCall>(callId)
                                ->getVideoRtp()
                                .restartSender();
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief getCallMediaHandlerDetails
     * @param id of the call media handler
     * @return map of Call Media Handler Details
     */
    std::map<std::string, std::string> getCallMediaHandlerDetails(const std::string& mediaHandlerId)
    {
        for (auto& mediaHandler : callMediaHandlers) {
            if (getCallHandlerId(mediaHandler) == mediaHandlerId) {
                return mediaHandler->getCallMediaHandlerDetails();
            }
        }
        return {};
    }

    std::map<std::string, std::vector<std::string>> getCallMediaHandlerStatus(
        const std::string& callId)
    {
        const auto& it = mediaHandlerToggled_.find(callId);
        if (it != mediaHandlerToggled_.end()) {
            std::vector<std::string> ret;
            for (const auto& mediaHandlerId : it->second)
                ret.push_back(mediaHandlerId);
            return {{callId, ret}};
        }

        return {{callId, {}}};
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
        if (auto soSubject = subject.lock()) {
            callMediaHandlerPtr->notifyAVFrameSubject(data, soSubject);
        }
    }

    /**
     * @brief listAvailableSubjects
     * @param callMediaHandlerPtr
     * This functions lets the call media handler component know which subjects are available
     */
    void listAvailableSubjects(const std::string& callID, CallMediaHandlerPtr& callMediaHandlerPtr)
    {
        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end(); ++it) {
            if (it->first.id == callID)
                notifyAVSubject(callMediaHandlerPtr, it->first, it->second);
        }
    }

    /**
     * @brief getCallHandlerId
     * Returns the callMediaHandler id from a callMediaHandler pointer
     * @param callMediaHandler
     * @return string id
     */
    std::string getCallHandlerId(const CallMediaHandlerPtr& callMediaHandler)
    {
        if (callMediaHandler) {
            std::ostringstream callHandlerIdStream;
            callHandlerIdStream << callMediaHandler.get();
            return callHandlerIdStream.str();
        }
        return "";
    }

private:
    /**
     * @brief callMediaHandlers
     * Components that a plugin can register through registerCallMediaHandler service
     * These objects can then be notified with notify notifyAVFrameSubject
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
    // std::map<std::string, std::tuple<const StreamData, AVSubjectSPtr>> callAVsubjects;

    std::map<std::string, std::set<std::string>> mediaHandlerToggled_;
};

} // namespace jami
