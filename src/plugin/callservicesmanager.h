/*!
 *  Copyright (C) 2020-2021 Savoir-faire Linux Inc.
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

#include "mediahandler.h"
#include "streamdata.h"

#include "noncopyable.h"

#include <list>
#include <map>
#include <tuple>

namespace jami {

class PluginManager;

using CallMediaHandlerPtr = std::unique_ptr<CallMediaHandler>;
using AVSubjectSPtr = std::weak_ptr<Observable<AVFrame*>>;

/*! \class  CallServicesManager
 * \brief This class provides the interface between loaded MediaHandlers
 * and call's audio/video streams. Besides it:
 * (1) stores pointers to all loaded MediaHandlers;
 * (2) stores pointers to to availables streams subjects, and;
 * (3) lists MediaHandler state with respect to each call. In other words,
 * for a given call, we store if a MediaHandler is active or not.
 */
class CallServicesManager
{
public:
    CallServicesManager(PluginManager& pluginManager);

    ~CallServicesManager();

    NON_COPYABLE(CallServicesManager);

    void createAVSubject(const StreamData& data, AVSubjectSPtr subject);

    void clearAVSubject(const std::string& callId);

    std::vector<std::string> getCallMediaHandlers();

    void toggleCallMediaHandler(const std::string& mediaHandlerId,
                                const std::string& callId,
                                const bool toggle);

    std::map<std::string, std::string> getCallMediaHandlerDetails(
        const std::string& mediaHandlerIdStr);

    std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId);

    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath);

    void clearCallHandlerMaps(const std::string& callId);

private:
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    void notifyAVSubject(CallMediaHandlerPtr& callMediaHandlerPtr,
                         const StreamData& data,
                         AVSubjectSPtr& subject);

    void toggleCallMediaHandler(const uintptr_t mediaHandlerId,
                                const std::string& callId,
                                const bool toggle);

    bool isVideoType(const CallMediaHandlerPtr& mediaHandler);

    bool isAttached(const CallMediaHandlerPtr& mediaHandler);

    /// Components that a plugin can register through registerMediaHandler service.
    /// These objects can then be activated with toggleCallMediaHandler.
    std::list<CallMediaHandlerPtr> callMediaHandlers_;

    /// When there is a SIPCall, AVSubjects are created there.
    /// Here we store their references in order to make them interact with MediaHandlers.
    /// For easy access they are mapped with the call they belong to.
    std::map<std::string, std::list<std::pair<const StreamData, AVSubjectSPtr>>> callAVsubjects_;

    /// Component that stores MediaHandlers' status for each existing call.
    /// A map of callIds and MediaHandler-status pairs.
    std::map<std::string, std::map<uintptr_t, bool>> mediaHandlerToggled_;
};
} // namespace jami
