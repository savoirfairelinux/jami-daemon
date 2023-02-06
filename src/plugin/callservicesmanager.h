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

/**
 * @brief This class provides the interface between loaded MediaHandlers
 * and call's audio/video streams. Besides it:
 * (1) stores pointers to all loaded MediaHandlers;
 * (2) stores pointers to available streams subjects, and;
 * (3) lists MediaHandler state with respect to each call. In other words,
 * for a given call, we store if a MediaHandler is active or not.
 */
class CallServicesManager
{
public:
    /**
     * @brief Constructor registers MediaHandler API services to the PluginManager
     * instance. These services will store MediaHandler pointers or clean them
     * from the Plugin System once a plugin is loaded or unloaded.
     * @param pluginManager
     */
    CallServicesManager(PluginManager& pluginManager);

    ~CallServicesManager();

    NON_COPYABLE(CallServicesManager);

    /**
     * @brief Stores a AV stream subject with StreamData properties. During the storage process,
     * if a MediaHandler is supposed to be activated for the call to which the subject is
     * related, the activation function is called.
     * @param data
     * @param subject
     */
    void createAVSubject(const StreamData& data, AVSubjectSPtr subject);

    /**
     * @brief Clears all stream subjects related to the callId.
     * @param callId
     */
    void clearAVSubject(const std::string& callId);

    /**
     * @brief List all MediaHandlers available.
     * @return Vector with stored MediaHandlers pointers.
     */
    std::vector<std::string> getCallMediaHandlers();

    /**
     * @brief (De)Activates a given MediaHandler to a given call.
     * If the MediaHandler receives video frames from a hardware decoder,
     * we need to restart the sender to unlink our encoder and decoder.
     * When we deactivate a MediaHandler, we try to relink the encoder
     * and decoder by restarting the sender.
     *
     * @param mediaHandlerId
     * @param callId
     * @param toggle notify with new subjects if true, detach if false.
     */
    void toggleCallMediaHandler(const std::string& mediaHandlerId,
                                const std::string& callId,
                                const bool toggle);

    /**
     * @brief Returns details Map from MediaHandler implementation.
     * @param mediaHandlerIdStr
     * @return Details map from the MediaHandler implementation
     */
    std::map<std::string, std::string> getCallMediaHandlerDetails(
        const std::string& mediaHandlerIdStr);

    /**
     * @brief Returns a list of active MediaHandlers for a given call.
     * @param callId
     * @return Vector with active MediaHandler ids for a given call.
     */
    std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId);

    /**
     * @brief Sets a preference that may be changed while MediaHandler is active.
     * @param key
     * @param value
     * @param rootPath
     * @return False if preference was changed.
     */
    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath);

    /**
     * @brief Removes call from mediaHandlerToggled_ mapping.
     * @param callId
     */
    void clearCallHandlerMaps(const std::string& callId);

private:
    /**
     * @brief Exposes MediaHandlers' life cycle managers services to the main API.
     * @param pluginManager
     */
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    /**
     * @brief Calls MediaHandler API function that attaches a data process to the given
     * AV stream.
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
     * @brief Checks if the MediaHandler being (de)activated expects a video stream.
     * It's used to reduce restartSender call.
     * @param mediaHandler
     * @return True if a MediaHandler expects a video stream.
     */
    bool isVideoType(const CallMediaHandlerPtr& mediaHandler);

    /**
     * @brief Checks if the MediaHandler was properly attached to a AV stream.
     * It's used to avoid saving wrong MediaHandler status.
     * @param mediaHandler
     * @return True if a MediaHandler is attached to a AV stream.
     */
    bool isAttached(const CallMediaHandlerPtr& mediaHandler);

    // Components that a plugin can register through registerMediaHandler service.
    // These objects can then be activated with toggleCallMediaHandler.
    std::list<CallMediaHandlerPtr> callMediaHandlers_;

    // When there is a SIPCall, AVSubjects are created there.
    // Here we store their references in order to make them interact with MediaHandlers.
    // For easy access they are mapped with the call they belong to.
    std::map<std::string, std::list<std::pair<const StreamData, AVSubjectSPtr>>> callAVsubjects_;

    // Component that stores MediaHandlers' status for each existing call.
    // A map of callIds and MediaHandler-status pairs.
    std::map<std::string, std::map<uintptr_t, bool>> mediaHandlerToggled_;
};
} // namespace jami
