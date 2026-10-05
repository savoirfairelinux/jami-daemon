/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <vector>
#include <cstring>

#include "callmanager_interface.h"
#include "call_factory.h"
#include "jami/media_const.h"

#include "sip/siptransport.h"
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "string_utils.h"

#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation_module.h"
#include "media/browser_conference_media.h"

#include <opendht/thread_pool.h>
#include <mutex>
#include <map>
#include <optional>
#include <future>

namespace libjami {

namespace {
struct BrowserHost
{
    std::string confId;
    std::shared_ptr<jami::BrowserConferenceMedia> media;
    std::string conversationId;
    std::string requestId;
};

class BrowserHostRegistry
{
public:
    using Key = std::pair<std::string, std::string>;

    bool reserve(const Key& key, const std::string& confId, const std::string& conversationId)
    {
        std::lock_guard lock(mutex_);
        if (shuttingDown_)
            return false;
        for (const auto& [existingKey, host] : hosts_)
            if (existingKey.first == key.first && host.conversationId == conversationId)
                return false;
        return hosts_.emplace(key, BrowserHost {confId, {}, conversationId, key.second}).second;
    }

    bool install(const Key& key, const std::string& confId, std::shared_ptr<jami::BrowserConferenceMedia> media)
    {
        std::lock_guard lock(mutex_);
        auto it = hosts_.find(key);
        if (it == hosts_.end() || it->second.confId != confId || it->second.media)
            return false;
        it->second.media = std::move(media);
        return true;
    }

    bool contains(const Key& key, const std::string& confId)
    {
        std::lock_guard lock(mutex_);
        auto it = hosts_.find(key);
        return it != hosts_.end() && it->second.confId == confId;
    }

    std::optional<BrowserHost> take(const Key& key, const std::string& confId = {})
    {
        std::lock_guard lock(mutex_);
        auto it = hosts_.find(key);
        if (it == hosts_.end() || (!confId.empty() && it->second.confId != confId))
            return std::nullopt;
        auto host = std::move(it->second);
        hosts_.erase(it);
        return host;
    }

    BrowserHost takeConference(const std::string& accountId, const std::string& confId)
    {
        std::lock_guard lock(mutex_);
        for (auto it = hosts_.begin(); it != hosts_.end(); ++it) {
            if (it->first.first == accountId && it->second.confId == confId) {
                auto host = std::move(it->second);
                hosts_.erase(it);
                return host;
            }
        }
        return {};
    }

    std::vector<BrowserHost> takeAccount(const std::string& accountId)
    {
        std::vector<BrowserHost> result;
        std::lock_guard lock(mutex_);
        for (auto it = hosts_.begin(); it != hosts_.end();) {
            if (it->first.first == accountId) {
                result.emplace_back(std::move(it->second));
                it = hosts_.erase(it);
            } else {
                ++it;
            }
        }
        return result;
    }

    std::vector<std::pair<std::string, BrowserHost>> takeAll()
    {
        std::vector<std::pair<std::string, BrowserHost>> result;
        std::lock_guard lock(mutex_);
        shuttingDown_ = true;
        for (auto& [key, host] : hosts_)
            result.emplace_back(key.first, std::move(host));
        hosts_.clear();
        return result;
    }

    void allow()
    {
        std::lock_guard lock(mutex_);
        shuttingDown_ = false;
    }

private:
    std::mutex mutex_;
    std::map<Key, BrowserHost> hosts_;
    bool shuttingDown_ {false};
};

std::shared_ptr<BrowserHostRegistry>
browserHosts()
{
    static auto registry = std::make_shared<BrowserHostRegistry>();
    return registry;
}

void
stopBrowserOwner(const std::shared_ptr<jami::BrowserConferenceMedia>& media)
{
    if (!media)
        return;
    auto context = jami::Manager::instance().ioContext();
    if (context->get_executor().running_in_this_thread()) {
        media->stop();
        return;
    }
    auto completion = std::make_shared<std::promise<void>>();
    auto finished = completion->get_future();
    asio::post(*context, [media, completion] {
        try {
            media->stop();
            completion->set_value();
        } catch (...) {
            completion->set_exception(std::current_exception());
        }
    });
    finished.get();
}

void
stopAndHangupBrowserHosts(const std::string& accountId, std::vector<BrowserHost> hosts)
{
    for (auto& host : hosts) {
        try {
            stopBrowserOwner(host.media);
        } catch (const std::exception& e) {
            JAMI_ERROR("Cannot stop browser conference {}: {}", host.confId, e.what());
        }
        if (!host.confId.empty())
            jami::Manager::instance().hangupConference(accountId, host.confId);
    }
}
} // namespace

void
registerCallHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

std::string
placeCall(const std::string& accountId, const std::string& to)
{
    // TODO. Remove ASAP.
    JAMI_WARNING("This API is deprecated, use placeCallWithMedia() instead");
    return placeCallWithMedia(accountId, to, {});
}

std::string
placeCallWithMedia(const std::string& accountId, const std::string& to, const std::vector<libjami::MediaMap>& mediaList)
{
    // Check if a destination number is available
    if (to.empty()) {
        JAMI_LOG("No number entered - Call aborted");
        return {};
    } else {
        return jami::Manager::instance().outgoingCall(accountId, to, mediaList);
    }
}

std::string
placeCallWithExternalMedia(const std::string& accountId, const std::string& to, const std::string& sdpOffer)
{
    if (to.empty() or sdpOffer.empty()) {
        JAMI_LOG("Missing destination or SDP offer - Call aborted");
        return {};
    }
    std::vector<libjami::MediaMap> mediaList;
    mediaList.emplace_back(libjami::MediaMap {{libjami::Media::MediaAttributeKey::EXTERNAL_SDP, sdpOffer}});
    return jami::Manager::instance().outgoingCall(accountId, to, mediaList);
}

bool
requestMediaChange(const std::string& accountId,
                   const std::string& callId,
                   const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            dht::ThreadPool::io().run([accountId, callId, mediaList] {
                if (auto account = jami::Manager::instance().getAccount(accountId)) {
                    if (auto call = account->getCall(callId)) {
                        try {
                            call->requestMediaChange(mediaList);
                        } catch (const std::runtime_error& e) {
                            JAMI_ERROR("{}", e.what());
                        }
                    }
                }
            });
            return true;
        } else if (auto conf = account->getConference(callId)) {
            dht::ThreadPool::io().run([accountId, callId, mediaList] {
                if (auto account = jami::Manager::instance().getAccount(accountId))
                    if (auto conf = account->getConference(callId))
                        conf->requestMediaChange(mediaList);
            });
            return true;
        }
    }
    return false;
}

bool
refuse(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().refuseCall(accountId, callId);
}

bool
accept(const std::string& accountId, const std::string& callId)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (account->getCall(callId)) {
            dht::ThreadPool::io().run([accountId, callId] { jami::Manager::instance().acceptCall(accountId, callId); });
            return true;
        }
    }
    return false;
}

bool
acceptWithMedia(const std::string& accountId, const std::string& callId, const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (account->getCall(callId)) {
            dht::ThreadPool::io().run(
                [accountId, callId, mediaList] { jami::Manager::instance().acceptCall(accountId, callId, mediaList); });
            return true;
        }
    }
    return false;
}

bool
acceptWithExternalMedia(const std::string& accountId, const std::string& callId, const std::string& sdpAnswer)
{
    if (sdpAnswer.empty())
        return false;
    std::vector<libjami::MediaMap> mediaList;
    mediaList.emplace_back(libjami::MediaMap {{libjami::Media::MediaAttributeKey::EXTERNAL_SDP, sdpAnswer}});
    return acceptWithMedia(accountId, callId, mediaList);
}

bool
answerMediaChangeRequestWithExternalMedia(const std::string& accountId,
                                          const std::string& callId,
                                          const std::string& sdpAnswer)
{
    if (sdpAnswer.empty())
        return false;
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = std::dynamic_pointer_cast<jami::SIPCall>(account->getCall(callId))) {
            dht::ThreadPool::io().run([call, sdpAnswer] {
                try {
                    call->answerMediaChangeRequestWithExternalSdp(sdpAnswer);
                } catch (const std::runtime_error& e) {
                    JAMI_ERROR("{}", e.what());
                }
            });
            return true;
        }
    }
    return false;
}

bool
setVideoOrientation(const std::string& accountId,
                    const std::string& callId,
                    int streamIdx,
                    int rotation)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            call->setVideoOrientation(streamIdx, rotation);
            return true;
        }
    }
    return false;
}

bool
answerMediaChangeRequest(const std::string& accountId,
                         const std::string& callId,
                         const std::vector<libjami::MediaMap>& mediaList)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId)) {
            dht::ThreadPool::io().run([accountId, callId, mediaList] {
                if (auto account = jami::Manager::instance().getAccount(accountId))
                    if (auto call = account->getCall(callId)) {
                        try {
                            call->answerMediaChangeRequest(mediaList);
                        } catch (const std::runtime_error& e) {
                            JAMI_ERROR("{}", e.what());
                        }
                    }
            });
            return true;
        }
    return false;
}

bool
hangUp(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().hangupCall(accountId, callId);
}

bool
hangUpConference(const std::string& accountId, const std::string& confId)
{
    auto host = browserHosts()->takeConference(accountId, confId);
    if (host.media)
        host.media->stop();
    return jami::Manager::instance().hangupConference(accountId, confId);
}

void
browserConferenceRemoved(const std::string& accountId, const std::string& confId)
{
    auto host = browserHosts()->takeConference(accountId, confId);
    if (host.media) {
        host.media->stop();
        jami::emitSignal<CallSignal::BrowserConferenceHostFailure>(accountId,
                                                                  host.requestId,
                                                                  "Conference closed");
    }
}

void
cancelBrowserConferencesForAccount(const std::string& accountId)
{
    stopAndHangupBrowserHosts(accountId, browserHosts()->takeAccount(accountId));
}

void
cancelAllBrowserConferences()
{
    auto hosts = browserHosts()->takeAll();
    for (auto& [accountId, host] : hosts) {
        try {
            stopBrowserOwner(host.media);
        } catch (const std::exception& e) {
            JAMI_ERROR("Cannot stop browser conference {}: {}", host.confId, e.what());
        }
        if (!host.confId.empty())
            jami::Manager::instance().hangupConference(accountId, host.confId);
    }
}

void
allowBrowserConferences()
{
    browserHosts()->allow();
}

bool
startBrowserConference(const std::string& accountId,
                       const std::string& conversationId,
                       const std::string& requestId,
                       const std::string& sdpOffer)
{
    if (accountId.empty() || conversationId.empty() || requestId.empty() || sdpOffer.empty())
        return false;
    auto account = std::dynamic_pointer_cast<jami::JamiAccount>(jami::Manager::instance().getAccount(accountId));
    if (!account)
        return false;
    auto* module = account->convModule(true);
    if (!module || !module->getConversation(conversationId))
        return false;
    const BrowserHostRegistry::Key key {accountId, requestId};
    auto registry = browserHosts();
    const auto confId = jami::Manager::instance().callFactory.getNewCallID();
    if (!registry->reserve(key, confId, conversationId))
        return false;
    std::weak_ptr<BrowserHostRegistry> weakRegistry = registry;
    auto conf = module->hostBrowserConference(conversationId, confId, [weakRegistry, key, confId] {
        if (auto registry = weakRegistry.lock()) {
            auto host = registry->take(key, confId);
            if (host && host->media)
                host->media->stop();
        }
    });
    if (!conf) {
        registry->take(key, confId);
        return false;
    }

    if (!registry->contains(key, confId) || account->getConference(confId) != conf) {
        registry->take(key, confId);
        jami::Manager::instance().hangupConference(accountId, confId);
        return false;
    }
    auto media = std::make_shared<jami::BrowserConferenceMedia>(account, conf);
    if (!registry->install(key, confId, media)) {
        media.reset();
        jami::Manager::instance().hangupConference(accountId, confId);
        return false;
    }
    media->start(sdpOffer,
                 [weakRegistry, key, confId](const std::string& answer) {
                     if (auto registry = weakRegistry.lock(); registry && registry->contains(key, confId))
                         jami::emitSignal<CallSignal::BrowserConferenceHostAnswer>(key.first, key.second, confId, answer);
                 },
                 [weakRegistry, key, confId](const std::string& reason) {
                     if (auto registry = weakRegistry.lock()) {
                         auto host = registry->take(key, confId);
                         if (!host || !host->media)
                             return;
                         jami::emitSignal<CallSignal::BrowserConferenceHostFailure>(key.first, key.second, reason);
                         jami::Manager::instance().hangupConference(key.first, confId);
                     }
                 },
                 [weakRegistry, key, confId] {
                     if (auto registry = weakRegistry.lock(); registry && registry->contains(key, confId))
                         jami::emitSignal<CallSignal::BrowserConferenceHostReady>(key.first, key.second, confId);
                 });
    return true;
}

bool
cancelBrowserConference(const std::string& accountId, const std::string& requestId)
{
    if (accountId.empty() || requestId.empty())
        return false;
    auto host = browserHosts()->take({accountId, requestId});
    if (!host)
        return false;
    if (host->media)
        host->media->stop();
    if (!host->confId.empty())
        jami::Manager::instance().hangupConference(accountId, host->confId);
    return true;
}

bool
hold(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().holdCall(accountId, callId);
}

bool
resume(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().resumeCall(accountId, callId);
}

bool
muteLocalMedia(const std::string& accountId, const std::string& callId, const std::string& mediaType, bool mute)
{
    if (auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            JAMI_LOG("Muting [{}] for call {}", mediaType, callId);
            call->muteMedia(mediaType, mute);
            return true;
        } else if (auto conf = account->getConference(callId)) {
            JAMI_DEBUG("[conf:{}] Muting local host [{}]", callId, mediaType);
            conf->muteLocalHost(mute, mediaType);
            return true;
        } else {
            JAMI_WARNING("ID {} doesn't match any call or conference", callId);
        }
    }
    return false;
}

bool
transfer(const std::string& accountId, const std::string& callId, const std::string& to)
{
    return jami::Manager::instance().transferCall(accountId, callId, to);
}

bool
attendedTransfer(const std::string& accountId, const std::string& transferID, const std::string& targetID)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(transferID))
            return call->attendedTransfer(targetID);
    return false;
}

bool
joinParticipant(const std::string& accountId,
                const std::string& sel_callId,
                const std::string& account2Id,
                const std::string& drag_callId)
{
    return jami::Manager::instance().joinParticipant(accountId, sel_callId, account2Id, drag_callId);
}

void
createConfFromParticipantList(const std::string& accountId, const std::vector<std::string>& participants)
{
    jami::Manager::instance().createConfFromParticipantList(accountId, participants);
}

void
setConferenceLayout(const std::string& accountId, const std::string& confId, uint32_t layout)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setLayout(static_cast<int>(layout));
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["layout"] = layout;
            call->sendConfOrder(root);
        }
    }
}

bool
isConferenceParticipant(const std::string& accountId, const std::string& callId)
{
    if (auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            return call->isConferenceParticipant();
    return false;
}

bool
addParticipant(const std::string& accountId,
               const std::string& callId,
               const std::string& account2Id,
               const std::string& confId)
{
    return jami::Manager::instance().addSubCall(accountId, callId, account2Id, confId);
}

bool
addMainParticipant(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().addMainParticipant(accountId, confId);
}

bool
detachLocalParticipant()
{
    return jami::Manager::instance().detachHost();
}

bool
detachParticipant(const std::string&, const std::string& callId)
{
    return jami::Manager::instance().detachParticipant(callId);
}

bool
joinConference(const std::string& accountId,
               const std::string& sel_confId,
               const std::string& account2Id,
               const std::string& drag_confId)
{
    return jami::Manager::instance().joinConference(accountId, sel_confId, account2Id, drag_confId);
}

bool
holdConference(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().holdConference(accountId, confId);
}

bool
resumeConference(const std::string& accountId, const std::string& confId)
{
    return jami::Manager::instance().resumeConference(accountId, confId);
}

std::map<std::string, std::string>
getConferenceDetails(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto conf = account->getConference(confId))
            return {{"ID", confId},
                    {"STATE", conf->getStateStr()},
#ifdef ENABLE_VIDEO
                    {"VIDEO_SOURCE", conf->getVideoInput()},
#endif
                    {"RECORDING", conf->isRecording() ? jami::TRUE_STR : jami::FALSE_STR}};
    return {};
}

std::vector<std::map<std::string, std::string>>
currentMediaList(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return call->currentMediaList();
        } else if (auto conf = account->getConference(callId)) {
            return conf->currentMediaList();
        }
    }
    JAMI_WARNING("Call not found {}", callId);
    return {};
}

std::vector<std::string>
getConferenceList(const std::string& accountId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        return account->getConferenceList();
    return {};
}

std::vector<std::string>
getParticipantList(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto conf = account->getConference(confId)) {
            const auto& subcalls(conf->getSubCalls());
            return {subcalls.begin(), subcalls.end()};
        }
    return {};
}

std::string
getConferenceId(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            if (auto conf = call->getConference())
                return conf->getConfId();
    return {};
}

bool
startRecordedFilePlayback(const std::string& filepath)
{
    return jami::Manager::instance().startRecordedFilePlayback(filepath);
}

void
stopRecordedFilePlayback()
{
    jami::Manager::instance().stopRecordedFilePlayback();
}

bool
toggleRecording(const std::string& accountId, const std::string& callId)
{
    return jami::Manager::instance().toggleRecordingCall(accountId, callId);
}

void
setRecording(const std::string& accountId, const std::string& callId)
{
    toggleRecording(accountId, callId);
}

void
recordPlaybackSeek(double value)
{
    jami::Manager::instance().recordingPlaybackSeek(value);
}

bool
getIsRecording(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto call = account->getCall(callId)) {
            return call->isRecording();
        } else if (auto conf = account->getConference(callId)) {
            return conf->isRecording();
        }
    }
    return false;
}

std::map<std::string, std::string>
getCallDetails(const std::string& accountId, const std::string& callId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId))
        if (auto call = account->getCall(callId))
            return call->getDetails();
    return {};
}

std::vector<std::string>
getCallList()
{
    return jami::Manager::instance().getCallList();
}

std::vector<std::string>
getCallList(const std::string& accountId)
{
    if (accountId.empty())
        return jami::Manager::instance().getCallList();
    else if (const auto account = jami::Manager::instance().getAccount(accountId))
        return account->getCallList();
    JAMI_WARNING("Unknown account: {}", accountId);
    return {};
}

std::vector<std::map<std::string, std::string>>
getConferenceInfos(const std::string& accountId, const std::string& confId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId))
            return conf->getConferenceInfos();
        else if (auto call = account->getCall(confId))
            return call->getConferenceInfos();
    }
    return {};
}

void
playDTMF(const std::string& key)
{
    auto code = key.data()[0];
    jami::Manager::instance().playDtmf(code);

    if (auto current_call = jami::Manager::instance().getCurrentCall())
        current_call->carryingDTMFdigits(code);
}

void
startTone(int32_t start, int32_t type)
{
    if (start) {
        if (type == 0)
            jami::Manager::instance().playTone();
        else
            jami::Manager::instance().playToneWithMessage();
    } else
        jami::Manager::instance().stopTone();
}

bool
switchInput(const std::string& accountId, const std::string& callId, const std::string& resource)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(callId)) {
            conf->switchInput(resource);
            return true;
        } else if (auto call = account->getCall(callId)) {
            call->switchInput(resource);
            return true;
        }
    }
    return false;
}

void
sendTextMessage(const std::string& accountId,
                const std::string& callId,
                const std::map<std::string, std::string>& messages,
                const std::string& from,
                bool isMixed)
{
    jami::runOnMainThread([accountId, callId, messages, from, isMixed] {
        jami::Manager::instance().sendCallTextMessage(accountId, callId, messages, from, isMixed);
    });
}

void
setModerator(const std::string& accountId, const std::string& confId, const std::string& peerId, const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setModerator(peerId, state);
        } else {
            JAMI_WARNING("[conf:{}] Failed to change moderator {} (conference not found)", confId, peerId);
        }
    }
}

void
muteParticipant(const std::string& accountId, const std::string& confId, const std::string& peerId, const bool& state)
{
    JAMI_ERROR("muteParticipant is deprecated, please use muteStream");
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->muteParticipant(peerId, state);
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["muteParticipant"] = peerId;
            root["muteState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
            call->sendConfOrder(root);
        }
    }
}

void
muteStream(const std::string& accountId,
           const std::string& confId,
           const std::string& accountUri,
           const std::string& deviceId,
           const std::string& streamId,
           const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->muteStream(accountUri, deviceId, streamId, state);
        } else if (auto call = account->getCall(confId)) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value sinkVal;
                sinkVal["muteAudio"] = state;
                Json::Value mediasObj;
                mediasObj[streamId] = sinkVal;
                Json::Value deviceVal;
                deviceVal["medias"] = mediasObj;
                Json::Value deviceObj;
                deviceObj[deviceId] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                root[accountUri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["muteParticipant"] = accountUri;
                root["muteState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
                call->sendConfOrder(root);
            }
        }
    }
}

void
setActiveParticipant(const std::string& accountId, const std::string& confId, const std::string& participant)
{
    JAMI_ERROR("setActiveParticipant is deprecated, please use setActiveStream");
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setActiveParticipant(participant);
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["activeParticipant"] = participant;
            call->sendConfOrder(root);
        }
    }
}

void
setActiveStream(const std::string& accountId,
                const std::string& confId,
                const std::string& accountUri,
                const std::string& deviceId,
                const std::string& streamId,
                const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->setActiveStream(streamId, state);
        } else if (auto call = std::static_pointer_cast<jami::SIPCall>(account->getCall(confId))) {
            call->setActiveMediaStream(accountUri, deviceId, streamId, state);
        }
    }
}

void
hangupParticipant(const std::string& accountId,
                  const std::string& confId,
                  const std::string& accountUri,
                  const std::string& deviceId)
{
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            conf->hangupParticipant(accountUri, deviceId);
        } else if (auto call = std::static_pointer_cast<jami::SIPCall>(account->getCall(confId))) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value deviceVal;
                deviceVal["hangup"] = jami::TRUE_STR;
                Json::Value deviceObj;
                deviceObj[deviceId] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                root[accountUri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["hangupParticipant"] = accountUri;
                call->sendConfOrder(root);
            }
        }
    }
}

void
raiseParticipantHand(const std::string& accountId,
                     const std::string& confId,
                     const std::string& peerId,
                     const bool& state)
{
    JAMI_ERROR("raiseParticipantHand is deprecated, please use raiseHand");
    if (const auto account = jami::Manager::instance().getAccount(accountId)) {
        if (auto conf = account->getConference(confId)) {
            if (auto call = std::static_pointer_cast<jami::SIPCall>(conf->getCallFromPeerID(peerId))) {
                if (auto* transport = call->getTransport())
                    conf->setHandRaised(std::string(transport->deviceId()), state);
            }
        } else if (auto call = account->getCall(confId)) {
            Json::Value root;
            root["handRaised"] = peerId;
            root["handState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
            call->sendConfOrder(root);
        }
    }
}

void
raiseHand(const std::string& accountId,
          const std::string& confId,
          const std::string& accountUri,
          const std::string& deviceId,
          const bool& state)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto conf = account->getConference(confId)) {
            auto device = deviceId;
            if (device.empty())
                device = std::string(account->currentDeviceId());
            conf->setHandRaised(device, state);
        } else if (auto call = std::static_pointer_cast<jami::SIPCall>(account->getCall(confId))) {
            if (call->conferenceProtocolVersion() == 1) {
                Json::Value deviceVal;
                deviceVal["raiseHand"] = state;
                Json::Value deviceObj;
                std::string device = deviceId.empty() ? std::string(account->currentDeviceId()) : deviceId;
                deviceObj[device] = deviceVal;
                Json::Value accountVal;
                deviceVal["devices"] = deviceObj;
                Json::Value root;
                std::string uri = accountUri.empty() ? account->getUsername() : accountUri;
                root[uri] = deviceVal;
                root["version"] = 1;
                call->sendConfOrder(root);
            } else if (call->conferenceProtocolVersion() == 0) {
                Json::Value root;
                root["handRaised"] = account->getUsername();
                root["handState"] = state ? jami::TRUE_STR : jami::FALSE_STR;
                call->sendConfOrder(root);
            }
        }
    }
}

} // namespace libjami
