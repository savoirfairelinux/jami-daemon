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
    CallServicesManager(PluginManager& pm);

    /**
     *   unload all media handlers
     **/
    ~CallServicesManager();

    NON_COPYABLE(CallServicesManager);

    /**
     * @brief createAVSubject
     * @param data
     * Creates an av frame subject with properties StreamData
     */
    void createAVSubject(const StreamData& data, AVSubjectSPtr subject);

    void clearAVSubject(const std::string& callId);

    /**
     * @brief registerComponentsLifeCycleManagers
     * Exposes components life cycle managers to the main API
     */
    void registerComponentsLifeCycleManagers(PluginManager& pm);

    /**
     * @brief getCallMediaHandlers
     * List all call media handlers
     * @return
     */
    std::vector<std::string> getCallMediaHandlers();

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
                                const bool toggle);

    /**
     * @brief getCallMediaHandlerDetails
     * @param id of the call media handler
     * @return map of Call Media Handler Details
     */
    std::map<std::string, std::string> getCallMediaHandlerDetails(
        const std::string& mediaHandlerIdStr);

    bool isVideoType(const CallMediaHandlerPtr& mediaHandler);

    bool isAttached(const CallMediaHandlerPtr& mediaHandler);

    std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId);

    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath);

    void clearCallHandlerMaps(const std::string& callId);

private:
    /**
     * @brief notifyAVSubject
     * @param callMediaHandlerPtr
     * @param data
     * @param subject
     */
    void notifyAVSubject(CallMediaHandlerPtr& callMediaHandlerPtr,
                         const StreamData& data,
                         AVSubjectSPtr& subject);

    void toggleCallMediaHandler(const uintptr_t mediaHandlerId,
                                const std::string& callId,
                                const bool toggle);

    /**
     * @brief callMediaHandlers_
     * Components that a plugin can register through registerCallMediaHandler service
     * These objects can then be notified with notifySubject
     * whenever there is a new CallAVSubject like a video receive
     */
    std::list<CallMediaHandlerPtr> callMediaHandlers_;

    /// When there is a SIPCall, AVSubjects are created there.
    /// Here we store their references in order to make them interact with MediaHandlers.
    /// For easy access they are mapped with the callId they belong to.
    std::map<std::string, std::list<std::pair<const StreamData, AVSubjectSPtr>>> callAVsubjects_;

    /// Component that stores MediaHandlers' status for each existing call.
    /// A map of callIds and MediaHandler-status pairs.
    std::map<std::string, std::map<uintptr_t, bool>> mediaHandlerToggled_;
};
} // namespace jami
