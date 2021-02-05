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

#include "callservicesmanager.h"

#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "manager.h"
#include "sip/sipcall.h"
#include "fileutils.h"
#include "logger.h"

namespace jami {

/*!
 * \brief Constructor registers MediaHandler API services to the PluginManager
 * instance. These functions will store MediaHandler pointers or clean them
 * from the Plugin System once a plugin is loaded or unloaded.
 * \param pluginManager
 */
CallServicesManager::CallServicesManager(PluginManager& pluginManager)
{
    registerComponentsLifeCycleManagers(pluginManager);
}

/*!
 * \brief Destructor clears callMediaHandlers_, callAVsubjects_, and mediaHandlerToggled_.
 */
CallServicesManager::~CallServicesManager()
{
    callMediaHandlers_.clear();
    callAVsubjects_.clear();
    mediaHandlerToggled_.clear();
}

/*!
 * \brief Stores a AV stream subject with StreamData properties. During the storage process,
 * if a MediaHandler is suposed to be activated for the call to which the subject is
 * related, the activation function is called.
 * \param data
 * \param subject
 */
void
CallServicesManager::createAVSubject(const StreamData& data, AVSubjectSPtr subject)
{
    /// callAVsubjects_ emplaces data and subject with callId key to easy of access
    callAVsubjects_[data.id].emplace_back(data, subject);

    /// search for activation flag.
    for (auto& callMediaHandler : callMediaHandlers_) {
        std::size_t found = callMediaHandler->id().find_last_of(DIR_SEPARATOR_CH);
        /// flag is True if we have a preference that tells us to automatically activate
        /// the MediaHandler
        bool toggle = PluginPreferencesUtils::getAlwaysPreference(
            callMediaHandler->id().substr(0, found),
            callMediaHandler->getCallMediaHandlerDetails().at("name"));
        /// flag may be overwritten if the MediaHandler was previously activated/deactivated
        /// for the given call.
        for (const auto& toggledMediaHandlerPair : mediaHandlerToggled_[data.id]) {
            if (toggledMediaHandlerPair.first == (uintptr_t) callMediaHandler.get()) {
                toggle = toggledMediaHandlerPair.second;
                break;
            }
        }
        if (toggle)
#ifndef __ANDROID__
            /// if activation is expected, we call activation function
            toggleCallMediaHandler((uintptr_t) callMediaHandler.get(), data.id, true);
#else
            /// Due to Android's camera activation process, we don't automaticaly
            /// activate the mediahandler here. But we set it as active
            /// and the client-android will handle it's activation.
            mediaHandlerToggled_[data.id].insert({(uintptr_t) callMediaHandler.get(), true});
#endif
    }
}

/*!
 * \brief Clears all stream subjects related to the callId.
 * \param callId
 */
void
CallServicesManager::clearAVSubject(const std::string& callId)
{
    callAVsubjects_.erase(callId);
}

/*!
 * \brief Exposes MediaHandlers' life cycle managers services to the main API.
 * \param pluginManager
 */
void
CallServicesManager::registerComponentsLifeCycleManagers(PluginManager& pluginManager)
{
    /// registerMediaHandler may be called by the PluginManager upon loading a plugin.
    auto registerMediaHandler = [this](void* data) {
        CallMediaHandlerPtr ptr {(static_cast<CallMediaHandler*>(data))};

        if (!ptr)
            return -1;
        std::size_t found = ptr->id().find_last_of(DIR_SEPARATOR_CH);
        /// adding preference that tells us to automatically activate a MediaHandler.
        PluginPreferencesUtils::addAlwaysHandlerPreference(ptr->getCallMediaHandlerDetails().at(
                                                               "name"),
                                                           ptr->id().substr(0, found));
        callMediaHandlers_.emplace_back(std::move(ptr));
        return 0;
    };

    /// unregisterMediaHandler may be called by the PluginManager while unloading.
    auto unregisterMediaHandler = [this](void* data) {
        bool status {true};
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
                /// if MediaHandler we're trying to destroy is currently is use, we deactivate it.
                if (handlerId != toggledList.second.end()) {
                    toggleCallMediaHandler((*handlerId).first, toggledList.first, false);
                    /// Due to lifetime of Daemon and MediaHandler shared data, we cannot safely
                    /// detroy MH.
                    status = false;
                }
            }
            /// if it's safe to detroy MH, we erase it from service storage.
            if (status) {
                callMediaHandlers_.erase(handlerIt);
                delete (*handlerIt).get();
            }
        }
        return status;
    };

    /// services are registered to the PluginManager.
    pluginManager.registerComponentManager("CallMediaHandlerManager",
                                           registerMediaHandler,
                                           unregisterMediaHandler);
}

/*!
 * \brief List all MediaHandlers available.
 * \return Vector with stored MediaHandlers pointers' adress
 */
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

/*!
 * \brief Activates or deactivate a given MediaHandler to a given call.
 * If the mediaHandler receives a hardware format video
 * frame and transports it to main memory, we need to restart the
 * sender to unlink ours encoder and decoder.
 * When we deactivate a mediaHandler, we try to relink the encoder
 * and decoder by restarting the sender.
 *
 * \param mediaHandlerId
 * \param callId
 * \param toggle notify with new subjects if true, detach if false.
 */
void
CallServicesManager::toggleCallMediaHandler(const std::string& mediaHandlerId,
                                            const std::string& callId,
                                            const bool toggle)
{
    toggleCallMediaHandler(std::stoull(mediaHandlerId), callId, toggle);
}

/*!
 * \brief Returns details Map from MediaHandler implementation.
 * \param mediaHandlerIdStr
 * \return Details map from the MediaHandler implementation
 */
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

/*!
 * \brief Checks if the MediaHandler being (de)activated expects a video stream.
 * It's used to reduce restartSender call.
 * \param mediaHandler
 * \return True if a MediaHandler expects a video stream.
 */
bool
CallServicesManager::isVideoType(const CallMediaHandlerPtr& mediaHandler)
{
    /// dataType is known from the MediaHandler implementation.
    const auto& details = mediaHandler->getCallMediaHandlerDetails();
    const auto& it = details.find("dataType");
    if (it != details.end()) {
        bool status;
        std::istringstream(it->second) >> status;
        return status;
    }
    /// If there is no "dataType" returned, it's safer to return True and allow
    /// sender to restart.
    return true;
}

/*!
 * \brief Checks if the MediaHandler was properly attached to a AV stream.
 * It's used to avoid saving wrong MediaHandler status.
 * \param mediaHandler
 * \return True if a MediaHandler is attached to a AV stream.
 */
bool
CallServicesManager::isAttached(const CallMediaHandlerPtr& mediaHandler)
{
    /// attached is known from the MediaHandler implementation.
    const auto& details = mediaHandler->getCallMediaHandlerDetails();
    const auto& it = details.find("attached");
    if (it != details.end()) {
        bool status;
        std::istringstream(it->second) >> status;
        return status;
    }
    return true;
}

/*!
 * \brief Returns a list of active MediaHandlers for a given call.
 * \param callId
 * \return Vector with active MediaHandler ids for a given call.
 */
std::vector<std::string>
CallServicesManager::getCallMediaHandlerStatus(const std::string& callId)
{
    std::vector<std::string> ret;
    const auto& it = mediaHandlerToggled_.find(callId);
    if (it != mediaHandlerToggled_.end())
        for (const auto& mediaHandlerId : it->second)
            if (mediaHandlerId.second) /// we only return active MediaHandler ids
                ret.emplace_back(std::to_string(mediaHandlerId.first));
    return ret;
}

/*!
 * \brief Sets a preference that may be changed while MediaHandler is active.
 * \param key
 * \param value
 * \param rootPath
 * \return False if preference was changed.
 */
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

/*!
 * \brief Removes call from mediaHandlerToggled_ mapping.
 * \param callId
 */
void
CallServicesManager::clearCallHandlerMaps(const std::string& callId)
{
    mediaHandlerToggled_.erase(callId);
}

/*!
 * \brief Calls MediaHandler API function that attaches a data process to the given
 * AV stream.
 * \param callMediaHandlerPtr
 * \param data
 * \param subject
 */
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
    if (applyRestart)
        Manager::instance().callFactory.getCall<SIPCall>(callId)->getVideoRtp().restartSender();
#endif
}
} // namespace jami
