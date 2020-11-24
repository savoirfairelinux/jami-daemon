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
// Plugin Manager
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
// STL
#include <list>

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
     * @brief notifyAllAVSubject
     * @param subject
     * @param av
     * @param local
     * @param peerId
     * This function is called whenever there is a new AVFrame subject available
     */
    void notifyAllAVSubject(const StreamData& data, AVSubjectSPtr& subject)
    {
        for (auto& pair : callMediaHandlers) {
            auto& callMediaHandlerPtr = pair.second;
            if (pair.first) {
                notifyAVSubject(callMediaHandlerPtr, data, subject);
            }
        }
    }

    /**
     * @brief createAVSubject
     * @param data
     * Creates an av frame subject with properties StreamData
     */
    void createAVSubject(const StreamData& data, AVSubjectSPtr subject)
    {
        // This guarantees unicity of subjects by id
        callAVsubjects.push_back(std::make_pair(data, subject));
    }

    void clearAVSubject(const std::string& callID)
    {
        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end();) {
            if (it->first.id == callID) {
                it = callAVsubjects.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& callMediaHandler : callMediaHandlers) {
            if (mediaHandlerToggled_[getCallHandlerId(callMediaHandler.second)] == callID) {
                callMediaHandler.first = false;
                callMediaHandler.second->detach();
                mediaHandlerToggled_[getCallHandlerId(callMediaHandler.second)] = "";
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
                callMediaHandlers.push_back(std::make_pair(false, std::move(ptr)));
            }
            return 0;
        };

        auto unregisterMediaHandler = [this](void* data) {
            for (auto it = callMediaHandlers.begin(); it != callMediaHandlers.end(); ++it) {
                if (it->second.get() == data) {
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
        for (const auto& pair : callMediaHandlers) {
            if (pair.second) {
                res.push_back(getCallHandlerId(pair.second));
            }
        }
        return res;
    }

    /**
     * @brief toggleCallMediaHandler
     * Toggle CallMediaHandler, if on, notify with new subjects
     * if off, detach it
     * @param id
     */
    void toggleCallMediaHandler(const std::string& id, const bool toggle)
    {
        std::string callID = Manager::instance().getCurrentCallId();

        if (toggle) {
            for (auto& pair : mediaHandlerToggled_) {
                if (pair.second == callID && pair.first != id) {
                    toggleCallMediaHandler(pair.first, false);
                }
            }
        }

        if (id.empty())
            return;

        auto find = mediaHandlerToggled_.find(id);
        if (find == mediaHandlerToggled_.end())
            mediaHandlerToggled_[id] = "";

        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end(); ++it) {
            if (it->first.id == callID) {
                for (auto& pair : callMediaHandlers) {
                    if (pair.second && getCallHandlerId(pair.second) == id) {
                        pair.first = toggle;
                        if (pair.first) {
                            mediaHandlerToggled_[id] = callID;
                            listAvailableSubjects(callID, pair.second);
                        } else {
                            pair.second->detach();
                            mediaHandlerToggled_[id] = "";
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
    std::map<std::string, std::string> getCallMediaHandlerDetails(const std::string& id)
    {
        for (auto& pair : callMediaHandlers) {
            if (pair.second && getCallHandlerId(pair.second) == id) {
                return pair.second->getCallMediaHandlerDetails();
            }
        }
        return {};
    }

    std::map<std::string, std::string> getCallMediaHandlerStatus()
    {
        for (auto& pair : mediaHandlerToggled_) {
            if (pair.second == Manager::instance().getCurrentCallId()) {
                return {{"name", pair.first}};
            }
        }

        return {{"name", ""}};
    }

    void setPreference(const std::string& key, const std::string& value, const std::string& scopeStr)
    {
        for (auto& pair : callMediaHandlers) {
            if (pair.second
                && scopeStr.find(pair.second->getCallMediaHandlerDetails()["name"])
                       != std::string::npos) {
                pair.second->setPreferenceAttribute(key, value);
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

    void cleanCallAVSubjects()
    {
        // remove subjects from not available calls callAVsubjects
        for (auto it = callAVsubjects.begin(); it != callAVsubjects.end();) {
            std::vector<std::string> callids = jami::Manager::instance().getCallList();
            if (std::find(callids.begin(), callids.end(), it->first.id) == callids.end()) {
                it = callAVsubjects.erase(it);
            } else {
                ++it;
            }
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
    std::list<std::pair<bool, CallMediaHandlerPtr>> callMediaHandlers;

    /**
     * @brief callAVsubjects
     * When there is a SIPCall, CallAVSubjects are created there
     * Here we keep a reference to them in order to make them interact with
     * CallMediaHandlers
     * It is pushed to this list list
     */
    std::list<std::pair<const StreamData, AVSubjectSPtr>> callAVsubjects;

    std::map<std::string, std::string> mediaHandlerToggled_;
};

} // namespace jami
