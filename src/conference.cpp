/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "conference.h"

#include "audio/audiolayer.h"
#include "audio/ringbufferpool.h"
#include "call_factory.h"
#include "client/videomanager.h"
#include "jami/media_const.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"
#include "string_utils.h"
#include "tracepoint.h"
#ifdef ENABLE_VIDEO
#include "video/video_input.h"
#include "video/video_mixer.h"
#endif
#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#endif

#include <opendht/thread_pool.h>

using namespace std::literals;

namespace jami {

Conference::Conference(const std::shared_ptr<Account>& account,
                       const std::string& confId,
                       bool attachHost,
                       const std::vector<MediaAttribute>& hostAttr)
    : id_(confId.empty() ? Manager::instance().callFactory.getNewCallID() : confId)
    , account_(account)
    , attachHost_(attachHost)
{
    auto acc = account_.lock();
    if (!acc)
        return;
    accountId_ = acc->getAccountID();
    if (auto jacc = std::dynamic_pointer_cast<JamiAccount>(acc)) {
        accountUri_ = jacc->getUsername();
        accountDevice_ = std::string(jacc->currentDeviceId());
    }
    /** NOTE:
     *
     *** Handling mute state of the local host.
     *
     * When a call is added to a conference, the media source of the
     * call is set to the audio/video mixers output, and the host media
     * source (e.g. camera), is added as a source for the mixer.
     * Note that, by design, the mixers are never muted, but the mixer
     * can produce audio/video frames with no content (silence or black
     * video frames) if all the participants are muted.
     *
     * The mute state of the local host is set as follows:
     *
     * 1. If the video is disabled, the mute state is irrelevant.
     * 2. If the local is not attached, the mute state is irrelevant.
     * 3. When the conference is created from existing calls:
     *  the mute state is set to true if the local mute state of
     *  all participating calls are true.
     * 4. Attaching the local host to an existing conference:
     *  the audio and video is set to the default capture device
     *  (microphone and/or camera), and set to un-muted state.
     */

    JAMI_INFO("Create new conference %s", id_.c_str());
    if (hostAttr.empty()) {
        setLocalHostDefaultMediaSource();
    } else {
        hostSources_ = hostAttr;
        reportMediaNegotiationStatus();
    }
    duration_start_ = clock::now();

    parser_.onVersion([&](uint32_t) {}); // TODO
    parser_.onCheckAuthorization([&](std::string_view peerId) { return callStreamsMgr_->isModerator(peerId); });
    parser_.onHangupParticipant([&](const auto& accountUri, const auto& deviceId) {
        hangupParticipant(accountUri, deviceId);
    });
    parser_.onRaiseHand([&](const auto& uri, const auto& deviceId, bool state) { callStreamsMgr_->setHandRaised(uri, deviceId, state); });
    parser_.onSetActiveStream(
        [&](const auto& uri, const auto& deviceId, const auto& streamId, bool state) {
            callStreamsMgr_->setActiveStream(uri, deviceId, streamId, state); });
    parser_.onMuteStreamAudio(
        [&](const auto& accountUri, const auto& deviceId, const auto& streamId, bool state) {
            muteStream(accountUri, deviceId, streamId, state);
        });
    parser_.onSetLayout([&](int layout) { setLayout(layout); });

    // Version 0, deprecated
    parser_.onKickParticipant([&](const auto& participantId) { hangupParticipant(participantId); });
    parser_.onSetActiveParticipant(
        [&](const auto& participantId) { setActiveParticipant(participantId); });
    parser_.onMuteParticipant(
        [&](const auto& participantId, bool state) { muteParticipant(participantId, state); });
    parser_.onRaiseHandUri([&](const auto& uri, bool state) {
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCallWith(uri)))
            setHandRaised(uri, call->getRemoteDeviceId(), state);
    });

    parser_.onVoiceActivity(
        [&](const auto& uri, const auto& deviceId, const auto& streamId, bool state) { setVoiceActivity(uri, deviceId, streamId, state); });
    jami_tracepoint(conference_begin, id_.c_str());
}

void
Conference::initLayout()
{
    // Passing weak() to subsequent lambdas must take a constructor already built (so outside constructor)
#ifdef ENABLE_VIDEO
    auto account = account_.lock();
    auto itVideo = std::find_if(hostSources_.begin(), hostSources_.end(), [&](auto attr) {
        return attr.type_ == MediaType::MEDIA_VIDEO;
    });
    // Only set host source if creating conference from joining calls
    auto hasVideo = account->isVideoEnabled() && itVideo != hostSources_.end() && attachHost_;
    auto source = hasVideo ? itVideo->sourceUri_ : "";
    callStreamsMgr_ = std::make_shared<video::VideoMixer>(id_, source, hasVideo);
    callStreamsMgr_->setOnInfoUpdated([w=weak()](const auto& info) {
        dht::ThreadPool::io().run(
            [w, info] {
                if (auto shared = w.lock()) {
                    if (shared->destroying_)
                        return;
                    {
                        std::lock_guard<std::mutex> lk(shared->confInfoMutex_);
                        shared->confInfo_.callInfo_ = info;
                    }
                    shared->sendConferenceInfos();
                }
            });
    });
    if (auto jacc = std::dynamic_pointer_cast<JamiAccount>(account)) {
        callStreamsMgr_->setAccountInfo(jacc->getUsername(), std::string(jacc->currentDeviceId()));
        callStreamsMgr_->setStreams(jacc->getUsername(), std::string(jacc->currentDeviceId()), hostSources_);
    }
#endif
}

Conference::~Conference()
{
    JAMI_INFO("Destroying conference %s", id_.c_str());
    destroying_ = true;

#ifdef ENABLE_VIDEO
    foreachCall([&](const auto& call) {
        call->exitConference();
        // Reset distant callInfo
        call->resetConfInfo();
        // Trigger the SIP negotiation to update the resolution for the remaining call
        // ideally this sould be done without renegotiation
        call->switchInput(
            Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice());

        // Continue the recording for the call if the conference was recorded
        if (isRecording()) {
            JAMI_DEBUG("Stop recording for conf {:s}", getConfId());
            toggleRecording();
            if (not call->isRecording()) {
                JAMI_DEBUG("Conference was recorded, start recording for conf {:s}",
                           call->getCallId());
                call->toggleRecording();
            }
        }
        // Notify that the remaining peer is still recording after conference
        if (call->isPeerRecording())
            call->peerRecording(true);
    });
#endif // ENABLE_VIDEO
#ifdef ENABLE_PLUGIN
    {
        std::lock_guard<std::mutex> lk(avStreamsMtx_);
        jami::Manager::instance()
            .getJamiPluginManager()
            .getCallServicesManager()
            .clearCallHandlerMaps(getConfId());
        Manager::instance().getJamiPluginManager().getCallServicesManager().clearAVSubject(
            getConfId());
        confAVStreams.clear();
    }
#endif // ENABLE_PLUGIN
    if (shutdownCb_)
        shutdownCb_(getDuration().count());
    // do not propagate sharing from conf host to calls
    closeMediaPlayer(mediaPlayerId_);
    jami_tracepoint(conference_end, id_.c_str());
}

void
Conference::setState(State state)
{
    JAMI_DEBUG("[conf {:s}] Set state to [{:s}] (was [{:s}])",
               id_,
               getStateStr(state),
               getStateStr());

    confState_ = state;
}

void
Conference::setLocalMuteState(const std::string& uri, const std::string& deviceId, const std::string& streamId, const bool& newState)
{
    callStreamsMgr_->muteStream(uri, deviceId, streamId, newState, true);
}

// Layout informations (passed to CallStreamsManager)
void
Conference::setVoiceActivity(const std::string& uri, const std::string& deviceId, const std::string& streamId, const bool& newState)
{
    callStreamsMgr_->setVoiceActivity(uri, deviceId, streamId, newState);
}

void
Conference::setHandRaised(const std::string& uri, const std::string& deviceId, const bool& state)
{
    auto remoteHost = findHostforRemoteParticipant(uri, deviceId);
    if (!remoteHost.first.empty()) {
        if (auto call = getCallWith(remoteHost.first, remoteHost.second)) {
            Json::Value deviceVal;
            deviceVal["raiseHand"] = state;
            Json::Value deviceObj;
            deviceObj[deviceId] = deviceVal;
            Json::Value accountVal;
            deviceVal["devices"] = deviceObj;
            Json::Value root;
            root[uri] = deviceVal;
            root["version"] = 1;
            call->sendConfOrder(root);
        }
        return;
    }

    callStreamsMgr_->setHandRaised(uri, deviceId, state);
}

void
Conference::setActiveParticipant(const std::string& callId)
{
#ifdef ENABLE_VIDEO
    if (isHost(callId)) {
        if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock()))
            callStreamsMgr_->setActiveStream(acc->getUsername(), std::string(acc->currentDeviceId()), sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID), true);
        return;
    }
    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(callId))) {
        callStreamsMgr_->setActiveStream(call->getRemoteUri(), call->getRemoteDeviceId(),
            sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_VIDEO_STREAMID), true);
        return;
    }
#endif
}

void
Conference::setRecording(const std::string& uri, const std::string& deviceId, bool state)
{
    callStreamsMgr_->setRecording(uri, deviceId, state);
}

void
Conference::setActiveStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool state)
{
    auto remoteHost = findHostforRemoteParticipant(uri, deviceId);
    if (!remoteHost.first.empty()) {
        if (auto call = getCallWith(remoteHost.first, remoteHost.second)) {
            std::dynamic_pointer_cast<SIPCall>(call)->setActiveMediaStream(uri, deviceId, streamId, state);
        }
        return;
    }
    callStreamsMgr_->setActiveStream(uri, deviceId, streamId, state);
}

void
Conference::setLayout(int layout)
{
    callStreamsMgr_->setLayout(layout);
}

void
Conference::setModerator(const std::string& uri, const bool& state)
{
    callStreamsMgr_->setModerator(uri, state);
}

void
Conference::removeAudioStreams(const std::string& uri, const std::string& deviceId)
{
    callStreamsMgr_->removeAudioStreams(uri, deviceId);
}

void
Conference::hangupParticipant(const std::string& accountUri, const std::string& deviceId)
{
    auto remoteHost = findHostforRemoteParticipant(accountUri, deviceId);
    if (!remoteHost.first.empty()) {
        if (auto call = getCallWith(remoteHost.first, remoteHost.second)) {
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
        }
        return;
    }

    if (deviceId.empty()) {
        // If deviceId is empty, hangup all calls with device
        while (auto call = getCallWith(accountUri)) {
            Manager::instance().hangupCall(accountId_, call->getCallId());
        }
        return;
    } else {
        if (accountUri == accountUri_ && deviceId == accountDevice_) {
            JAMI_LOG("Detach local participant from conference {}", getConfId());
            detachLocal();
            emitSignal<libjami::CallSignal::ConferenceChanged>(getAccountId(),
                                                            getConfId(),
                                                            getStateStr());
            return;
        } else if (auto call = getCallWith(accountUri, deviceId)) {
            Manager::instance().hangupCall(accountId_, call->getCallId());
            return;
        }
    }
}

void
Conference::muteStream(const std::string& accountUri,
                       const std::string& deviceId,
                       const std::string& streamId,
                       const bool& state)
{
    auto remoteHost = findHostforRemoteParticipant(accountUri, deviceId);
    if (!remoteHost.first.empty()) {
        if (auto call = getCallWith(remoteHost.first, remoteHost.second)) {
            Json::Value sinkVal;
            sinkVal["muteAudio"] = state;
            Json::Value mediasObj;
            std::string sid = streamId;
            string_replace(sid, call->getCallId(), "host");
            mediasObj[sid] = sinkVal;
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
        }
        return;
    }

    if (accountUri == accountUri_ && deviceId == accountDevice_)
        muteHost(state);
    else if (auto call = getCallWith(accountUri, deviceId))
        muteCall(call->getCallId(), state);
    else
        JAMI_WARNING("No call with {:s} - {:s}", accountUri_, accountDevice_);
}

// Media management
bool
Conference::isMediaSourceMuted(MediaType type) const
{
    // Assume muted if not attached.
    if (getState() != State::ACTIVE_ATTACHED)
        return true;

    return callStreamsMgr_->isMuted(accountUri_, accountDevice_, sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID), true);
}

bool
Conference::requestMediaChange(const std::vector<libjami::MediaMap>& mediaList)
{
    if (getState() != State::ACTIVE_ATTACHED) {
        JAMI_ERROR("[conf {:s}] Request media change can be performed only in attached mode", getConfId());
        return false;
    }

    JAMI_DEBUG("[conf {:s}] Request media change", getConfId());

    std::unique_lock<std::mutex> lk(hostSourcesMtx_);
    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, false);

    bool hasFileSharing = false;
    for (const auto& media : mediaAttrList) {
        if (!media.enabled_ || media.sourceUri_.empty())
            continue;

        // Supported MRL schemes
        static const std::string sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;

        const auto pos = media.sourceUri_.find(sep);
        if (pos == std::string::npos)
            continue;

        const auto prefix = media.sourceUri_.substr(0, pos);
        if ((pos + sep.size()) >= media.sourceUri_.size())
            continue;

        if (prefix == libjami::Media::VideoProtocolPrefix::FILE) {
            hasFileSharing = true;
            mediaPlayerId_ = media.sourceUri_;
            JAMI_ERROR("@@@ CREATED {}", mediaPlayerId_);
            createMediaPlayer(mediaPlayerId_);
        }
    }

    if (!hasFileSharing) {
        closeMediaPlayer(mediaPlayerId_);
        mediaPlayerId_ = "";
    }
    std::vector<std::string> newVideoInputs;
    for (auto const& mediaAttr : mediaAttrList) {
        JAMI_DEBUG("[conf {:s}] New requested media: {:s}", getConfId(), mediaAttr.toString(true));
        // Find media
        auto oldIdx = std::find_if(hostSources_.begin(), hostSources_.end(), [&](auto oldAttr) {
            return oldAttr.sourceUri_ == mediaAttr.sourceUri_ && oldAttr.type_ == mediaAttr.type_ && oldAttr.label_ == mediaAttr.label_;
        });
        // If video, add to newVideoInputs
        // NOTE: For now, only supports video
        if (mediaAttr.type_ == MediaType::MEDIA_VIDEO)
            newVideoInputs.emplace_back(mediaAttr.sourceUri_);
        if (oldIdx != hostSources_.end()) {
            // Check if muted status changes
            if (mediaAttr.muted_ != oldIdx->muted_) {
                // If the current media source is muted, just call un-mute, it
                // will set the new source as input.
                muteLocalHost(mediaAttr.muted_,
                              mediaAttr.type_ == MediaType::MEDIA_AUDIO
                                  ? libjami::Media::Details::MEDIA_TYPE_AUDIO
                                  : libjami::Media::Details::MEDIA_TYPE_VIDEO);
            }
        }
    }

    hostSources_ = mediaAttrList; // New medias
    callStreamsMgr_->setStreams(accountUri_, accountDevice_, hostSources_);
    lk.unlock();
    bool isHostMuted = callStreamsMgr_->isMuted(accountUri_, accountDevice_, sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID));
    if (!isHostMuted && !isMediaSourceMuted(MediaType::MEDIA_AUDIO))
        bindHost();

    // It's host medias, so no need to negotiate anything, but inform the client.
    reportMediaNegotiationStatus();
    return true;
}

void
Conference::handleMediaChangeRequest(const std::shared_ptr<Call>& call,
                                     const std::vector<libjami::MediaMap>& remoteMediaList)
{
    // Retrigger on iO pool cause it's from a io event and we're answering
    // so avoid SIPCall->Conference->SIPCall
    dht::ThreadPool::io().run([w=weak(), c=std::weak_ptr<Call>(call), remoteMediaList] {
        auto conf = w.lock(); auto call = c.lock();
        JAMI_DEBUG("Conf [{:s}] Answer to media change request", conf->getConfId());

        auto remoteList = remoteMediaList;
        for (auto it = remoteList.begin(); it != remoteList.end();) {
            if (it->at(libjami::Media::MediaAttributeKey::MUTED) == TRUE_STR
                or it->at(libjami::Media::MediaAttributeKey::ENABLED) == FALSE_STR) {
                it = remoteList.erase(it);
            } else {
                ++it;
            }
        }
        // Create minimum media list (ignore muted and disabled medias)
        std::vector<libjami::MediaMap> newMediaList;
        newMediaList.reserve(remoteMediaList.size());
        std::vector<MediaAttribute> currentMediaList;
        {
            std::lock_guard<std::mutex> lk(conf->hostSourcesMtx_);
            currentMediaList = conf->hostSources_;
        }
        for (auto const& media : currentMediaList) {
            if (media.enabled_ and not media.muted_)
                newMediaList.emplace_back(MediaAttribute::toMediaMap(media));
        }
        for (auto idx = newMediaList.size(); idx < remoteMediaList.size(); idx++)
            newMediaList.emplace_back(remoteMediaList[idx]);

        // NOTE:
        // Since this is a conference, newly added media will be also accepted.
        // This also means that if original call was an audio-only call,
        // the local camera will be enabled, unless the video is disabled
        // in the account settings.
        call->answerMediaChangeRequest(newMediaList);
        call->enterConference(conf->shared_from_this());
    });
}

void
Conference::bindCall(const std::shared_ptr<SIPCall>& call)
{
    const auto& callId = call->getCallId();
    JAMI_DEBUG("Adding call {:s} to conference {:s}", callId, id_);

    jami_tracepoint(conference_add_participant, id_.c_str(), callId.c_str());

    // NOTE:
    // When a call joins a conference, the media source of the call
    // will be set to the output of the conference mixer.
    takeOverMediaSourceControl(callId);
    auto w = call->getAccount();
    auto account = w.lock();
    auto isModerator = false;
    if (account) {
        // Check for allModeratorEnabled preference
        isModerator = account->isAllModerators();
        if (auto sipCall = std::dynamic_pointer_cast<SIPCall>(call)) {
            auto peerUri = sipCall->getRemoteUri();

            if (!isModerator) {
                const auto& defaultMods = account->getDefaultModerators();
                isModerator = std::find(defaultMods.cbegin(), defaultMods.cend(), peerUri) != defaultMods.cend();
            }

            // Check for localModeratorsEnabled preference
            if (account->isLocalModeratorsEnabled() and not isModerator) {
                auto accounts = jami::Manager::instance().getAllAccounts<JamiAccount>();
                isModerator = std::find_if(accounts.cbegin(), accounts.cend(), [&](const auto& value) {
                    return account->getUsername() == peerUri;
                }) != accounts.cend();
            }
        }
    }
    call->enterConference(shared_from_this());
    callStreamsMgr_->bindCall(call, isModerator);
    // Continue the recording for the conference if one participant was recording
    if (call->isRecording()) {
        JAMI_DEBUG("Stop recording for call {:s}", call->getCallId());
        call->toggleRecording();
        if (not this->isRecording()) {
            JAMI_DEBUG("One participant was recording, start recording for conference {:s}",
                        getConfId());
            this->toggleRecording();
        }
    }
#ifdef ENABLE_PLUGIN
    createConfAVStreams();
#endif
}

void
Conference::bindCallId(const std::string& callId)
{
    JAMI_LOG("Bind call {:s} to conference {:s}", callId, id_);

    auto& rbPool = Manager::instance().getRingBufferPool();
    // Bind each of the new participant's audio streams to each of the other participants audio streams
    if (auto participantCall = getCall(callId)) {
        auto participantStreams = participantCall->getAudioStreams();
        for (auto stream : participantStreams) {
            for (const auto& other : getCallIds()) {
                auto otherCall = other != callId ? std::dynamic_pointer_cast<SIPCall>(getCall(other)) : nullptr;
                if (otherCall) {
                    auto otherStreams = otherCall->getAudioStreams();
                    for (const auto& [otherStream, _b] : otherStreams) {
                        bool isMuted = callStreamsMgr_->isMuted(otherCall->getRemoteUri(), otherCall->getRemoteDeviceId(), otherStream);
                        if (isMuted)
                            rbPool.bindHalfDuplexOut(otherStream, stream.first);
                        else
                            rbPool.bindRingbuffers(stream.first, otherStream);
                        rbPool.flush(otherStream);
                    }
                }
            }
            // Bind local participant to other participants only if the
            // local is attached to the conference.
            if (getState() == State::ACTIVE_ATTACHED) {
                if (isMediaSourceMuted(MediaType::MEDIA_AUDIO))
                    rbPool.bindHalfDuplexOut(RingBufferPool::DEFAULT_ID, stream.first);
                else
                    rbPool.bindRingbuffers(stream.first, RingBufferPool::DEFAULT_ID);
                rbPool.flush(RingBufferPool::DEFAULT_ID);
            }
        }
    }
}

void
Conference::removeCall(const std::shared_ptr<SIPCall>& call)
{
    JAMI_DEBUG("Remove call {:s} in conference {:s}", call->getCallId(), id_);
    callStreamsMgr_->removeCall(call);
    call->exitConference();
    if (call->isPeerRecording())
        call->peerRecording(false);
}

void
Conference::attachLocal()
{
    JAMI_LOG("Attach local participant to conference {}", id_);

    if (getState() == State::ACTIVE_DETACHED) {
        setState(State::ACTIVE_ATTACHED);
        setLocalHostDefaultMediaSource();
        bindHost();
        std::lock_guard<std::mutex> lk(hostSourcesMtx_);
        if (callStreamsMgr_) callStreamsMgr_->setStreams(accountUri_, accountDevice_, hostSources_);
    } else {
        JAMI_WARNING(
            "Invalid conference state in attach participant: current \"{:s}\" - expected \"{:s}\"",
            getStateStr(),
            "ACTIVE_DETACHED");
    }
}

void
Conference::detachLocal()
{
    JAMI_LOG("Detach local participant from conference {:s}", id_);

    if (getState() == State::ACTIVE_ATTACHED) {
        unbindHost();
        if (callStreamsMgr_) callStreamsMgr_->setStreams(accountUri_, accountDevice_, {});
    } else {
        JAMI_WARNING(
            "Invalid conference state in detach participant: current \"{:s}\" - expected \"{:s}\"",
            getStateStr(),
            "ACTIVE_ATTACHED");
        return;
    }

    setState(State::ACTIVE_DETACHED);
    setLocalHostDefaultMediaSource();
}

void
Conference::bindHost()
{
    JAMI_LOG("Bind host to conference {}", id_);
    auto& rbPool = Manager::instance().getRingBufferPool();
    for (const auto& item : getCallIds()) {
        if (auto call = getCall(item)) {
            auto medias = call->getAudioStreams();
            for (const auto& [id, muted] : medias) {
                for (const auto& source : hostSources_) {
                    JAMI_ERROR("@@@ source {}", source.sourceUri_);
                    if (source.type_ == MediaType::MEDIA_AUDIO) {
                        if (source.label_ == sip_utils::DEFAULT_AUDIO_STREAMID) {
                            if (muted)
                                rbPool.bindHalfDuplexOut(id, RingBufferPool::DEFAULT_ID);
                            else
                                rbPool.bindRingbuffers(id, RingBufferPool::DEFAULT_ID);
                        } else {
                            auto buffer = source.sourceUri_;
                            static const std::string& sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;
                            const auto pos = source.sourceUri_.find(sep);
                            if (pos != std::string::npos)
                                buffer = source.sourceUri_.substr(pos + sep.size());
                            JAMI_ERROR("@@@ {} {} {}", id, buffer, muted);
                            if (muted)
                                rbPool.bindHalfDuplexOut(id, buffer);
                            else
                                rbPool.bindRingbuffers(id, buffer);
                        }
                    }
                }
                rbPool.flush(id);
            }
        }
    }
    rbPool.flush(RingBufferPool::DEFAULT_ID);
}

void
Conference::unbindCallId(const std::string& callId)
{
    JAMI_LOG("Unbind call {:s} from conference {:s}", callId, id_);
    if (auto call = getCall(callId)) {
        auto medias = call->getAudioStreams();
        auto& rbPool = Manager::instance().getRingBufferPool();
        for (const auto& [id, muted] : medias) {
            rbPool.unBindAllHalfDuplexOut(id);
        }
    }
}

void
Conference::unbindHost()
{
    JAMI_LOG("Unbind host from conference {:s}", id_);
    for (const auto& source : hostSources_) {
        if (source.type_ == MediaType::MEDIA_AUDIO) {
            if (source.label_ == sip_utils::DEFAULT_AUDIO_STREAMID) {
                Manager::instance().getRingBufferPool().unBindAllHalfDuplexOut(RingBufferPool::DEFAULT_ID);
            } else {
                auto buffer = source.sourceUri_;
                static const std::string& sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;
                const auto pos = source.sourceUri_.find(sep);
                if (pos != std::string::npos)
                    buffer = source.sourceUri_.substr(pos + sep.size());
                Manager::instance().getRingBufferPool().unBindAllHalfDuplexOut(buffer);
            }
        }
    }
}

std::set<std::string>
Conference::getCallIds() const
{
    return callStreamsMgr_->getCallIds();
}

bool
Conference::toggleRecording()
{
    bool newState = not isRecording();
    if (newState)
        initRecorder(recorder_);
    else if (recorder_)
        deinitRecorder(recorder_);

    // Notify each participant
    foreachCall([&](const auto& call) { call->updateRecState(newState); });

    auto res = Recordable::toggleRecording();
    callStreamsMgr_->setRecording(accountUri_, accountDevice_, newState);
    return res;
}

bool
Conference::startRecording(const std::string& path)
{
    auto res = Recordable::startRecording(path);
    callStreamsMgr_->setRecording(accountUri_, accountDevice_, true);
    return res;
}

void
Conference::stopRecording()
{
    Recordable::stopRecording();
    callStreamsMgr_->setRecording(accountUri_, accountDevice_, false);
}

void
Conference::muteParticipant(const std::string& participantId, const bool& state)
{
    // Prioritize remote mute, otherwise the mute info is lost during
    // the conference merge (we don't send back info to remoteHost,
    // cf. getConfInfoHostUri method)

    // Transfert remote participant mute
    JAMI_ERROR("@@@ MUTE {}", participantId);
    auto remoteHost = findHostforRemoteParticipant(participantId);
    if (not remoteHost.first.empty()) {
        JAMI_ERROR("@@@@ SEND MUTE");
        if (auto call = getCallWith(remoteHost.first, remoteHost.second)) {
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                return;
            Json::Value root;
            root["muteParticipant"] = participantId;
            root["muteState"] = state ? TRUE_STR : FALSE_STR;
            call->sendConfOrder(root);
            return;
        }
    }

    // NOTE: For now we only have one audio per call, and no way to only
    // mute one stream
    if (isHost(participantId))
        muteHost(state);
    else if (auto call = getCallWith(participantId))
        muteCall(call->getCallId(), state);
}

void
Conference::muteLocalHost(bool is_muted, const std::string& mediaType)
{
    auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock());
    if (!acc)
        return;
    if (mediaType.compare(libjami::Media::Details::MEDIA_TYPE_AUDIO) == 0) {
        if (is_muted == isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DEBUG("Local audio source already in [{:s}] state",
                       is_muted ? "muted" : "un-muted");
            return;
        }

        bool isHostMuted = callStreamsMgr_->isMuted(accountUri_, accountDevice_, sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID));
        if (is_muted and not isMediaSourceMuted(MediaType::MEDIA_AUDIO) and not isHostMuted) {
            JAMI_DEBUG("Muting local audio source");
            unbindHost();
        } else if (not is_muted and isMediaSourceMuted(MediaType::MEDIA_AUDIO) and isHostMuted) {
            JAMI_DEBUG("Un-muting local audio source");
            bindHost();
        }
        callStreamsMgr_->muteStream(accountUri_, accountDevice_, sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID), is_muted, true);
        emitSignal<libjami::CallSignal::AudioMuted>(id_, is_muted);
    } else if (mediaType.compare(libjami::Media::Details::MEDIA_TYPE_VIDEO) == 0) {
#ifdef ENABLE_VIDEO
        if (not acc->isVideoEnabled()) {
            JAMI_ERR("Cant't mute, the video is disabled!");
            return;
        }

        if (is_muted == isMediaSourceMuted(MediaType::MEDIA_VIDEO)) {
            JAMI_DEBUG("Local video source already in [{:s}] state",
                       is_muted ? "muted" : "un-muted");
            return;
        }
        callStreamsMgr_->muteStream(accountUri_, accountDevice_, sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID), is_muted, true);
        emitSignal<libjami::CallSignal::VideoMuted>(id_, is_muted);
        return;
#endif
    }
}

void
Conference::reportMediaNegotiationStatus()
{
    emitSignal<libjami::CallSignal::MediaNegotiationStatus>(
        getConfId(),
        libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS,
        currentMediaList());
}

std::vector<std::map<std::string, std::string>>
Conference::currentMediaList() const
{
    std::lock_guard<std::mutex> lk(hostSourcesMtx_);
    return MediaAttribute::mediaAttributesToMediaMaps(hostSources_);
}

// Parser informations
void
Conference::onConfOrder(const std::string& callId, const std::string& confOrder)
{
    JAMI_ERROR("@@@ ON CONF ORDER {} {}", callId, confOrder);
    // Check if the peer is a master
    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(callId))) {
        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(confOrder.c_str(), confOrder.c_str() + confOrder.size(), &root, &err)) {
            JAMI_WARNING("Couldn't parse conference order from {:s}", call->getRemoteUri());
            return;
        }

        parser_.initData(std::move(root), call->getRemoteUri());
        parser_.parse();
    }
}

void
Conference::mergeConfInfo(ConfInfo& newInfo, const std::string& callId)
{
    // This is actually broken: https://git.jami.net/savoirfairelinux/jami-daemon/-/issues/828
    std::pair<std::string, std::string> key;
    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(callId)))
        key = {call->getRemoteUri(), call->getRemoteDeviceId()};
    if (newInfo.callInfo_.empty()) {
        JAMI_DEBUG("confInfo empty, remove remoteHost");
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        remoteHosts_.erase(key);
        sendConferenceInfos();
        return;
    }

#ifdef ENABLE_VIDEO
    resizeRemoteParticipants(newInfo, callId);
#endif
    auto it = remoteHosts_.find(key);
    if (it != remoteHosts_.end()) {
        // Compare confInfo before update
        if (it->second != newInfo) {
            it->second = newInfo;
        } else {
            JAMI_WARNING("No change in confInfo, don't update");
            return;
        }
    } else
        remoteHosts_.emplace(key, newInfo);
}

#ifdef ENABLE_VIDEO
void
Conference::createSinks(const ConfInfo& infos)
{
    std::lock_guard<std::mutex> lk(sinksMtx_);
    if (auto vm = std::dynamic_pointer_cast<video::VideoMixer>(callStreamsMgr_)) {
        auto& sink = vm->getSink();
        Manager::instance().createSinkClients(getConfId(),
                                            infos,
                                            {std::static_pointer_cast<video::VideoFrameActiveWriter>(
                                                sink)},
                                            confSinksMap_);
    }
}

std::string
Conference::getVideoInput() const
{
    std::lock_guard<std::mutex> lk(hostSourcesMtx_);
    for (const auto& source : hostSources_) {
        if (source.type_ == MediaType::MEDIA_VIDEO)
            return source.sourceUri_;
    }
    return {};
}
#endif

std::shared_ptr<Call>
Conference::getCall(const std::string& callId)
{
    return Manager::instance().callFactory.getCall(callId);
}

void
Conference::foreachCall(const std::function<void(const std::shared_ptr<Call>& call)>& cb)
{
    for (const auto& p : getCallIds())
        if (auto call = getCall(p))
            cb(call);
}

std::shared_ptr<Call>
Conference::getCallWith(const std::string& accountUri, const std::string& deviceId)
{
    for (const auto& p : getCallIds()) {
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(p))) {
            if (accountUri == call->getRemoteUri()
                && (deviceId == call->getRemoteDeviceId() || deviceId.empty())) {
                return call;
            }
        }
    }
    return {};
}

void
Conference::muteHost(bool state)
{
    std::string streamId = sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID);
    auto isHostMuted = callStreamsMgr_->isMuted(accountUri_, accountDevice_, streamId);
    if (state and not isHostMuted) {
        if (not isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DEBUG("Mute host");
            unbindHost();
        }
    } else if (not state and isHostMuted) {
        if (not isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DEBUG("Unmute host");
            bindHost();
        }
    }
    callStreamsMgr_->muteStream(accountUri_, accountDevice_, streamId, state);
}

void
Conference::muteCall(const std::string& callId, bool state)
{
    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(callId))) {
        std::string streamId = sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_AUDIO_STREAMID);
        auto isPartMuted = callStreamsMgr_->isMuted(call->getRemoteUri(), call->getRemoteDeviceId(), streamId);
        if (state and not isPartMuted) {
            JAMI_DEBUG("Mute participant {:s}", callId);
            unbindCallId(callId);
        } else if (not state and isPartMuted) {
            JAMI_DEBUG("Unmute participant {:s}", callId);
            bindCallId(callId);
        }
        callStreamsMgr_->muteStream(call->getRemoteUri(), call->getRemoteDeviceId(), streamId, state);
    }
}

void
Conference::setLocalHostDefaultMediaSource()
{
    std::unique_lock<std::mutex> lk(hostSourcesMtx_);
    hostSources_.clear();
    // Setup local audio source
    if (confState_ == State::ACTIVE_ATTACHED) {
        MediaAttribute audioAttr
            = {MediaType::MEDIA_AUDIO, false, false, true, {}, sip_utils::DEFAULT_AUDIO_STREAMID};
        JAMI_DEBUG("[conf {:s}] Setting local host audio source to [{:s}]", id_, audioAttr.toString());
        hostSources_.emplace_back(audioAttr);

#ifdef ENABLE_VIDEO
        auto acc = account_.lock();
        if (acc && acc->isVideoEnabled()) {
            MediaAttribute videoAttr = {MediaType::MEDIA_VIDEO,
                    false,
                    false,
                    true,
                    Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice(),
                    sip_utils::DEFAULT_VIDEO_STREAMID};
            JAMI_DEBUG("[conf {:s}] Setting local host video source to [{:s}]",
                    id_,
                    videoAttr.toString());
            hostSources_.emplace_back(videoAttr);
        }
#endif
    }
    if (callStreamsMgr_) callStreamsMgr_->setStreams(accountUri_, accountDevice_, hostSources_);
    lk.unlock();

    reportMediaNegotiationStatus();
}

void
Conference::takeOverMediaSourceControl(const std::string& callId)
{
    auto call = getCall(callId);
    if (not call) {
        JAMI_ERR("No call matches participant %s", callId.c_str());
        return;
    }

    auto account = call->getAccount().lock();
    if (not account) {
        JAMI_ERR("No account detected for call %s", callId.c_str());
        return;
    }
    std::string deviceId;
    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account))
        deviceId = acc->currentDeviceId();

    auto mediaList = call->getMediaAttributeList();

    std::vector<MediaType> mediaTypeList {MediaType::MEDIA_AUDIO, MediaType::MEDIA_VIDEO};

    for (auto mediaType : mediaTypeList) {
        // Try to find a media with a valid source type
        auto check = [mediaType](auto const& mediaAttr) {
            return (mediaAttr.type_ == mediaType);
        };

        auto iter = std::find_if(mediaList.begin(), mediaList.end(), check);

        if (iter == mediaList.end()) {
            // Nothing to do if the call does not have a stream with
            // the requested media.
            JAMI_DEBUG("[Call: {:s}] Does not have an active [{:s}] media source",
                       callId,
                       MediaAttribute::mediaTypeToString(mediaType));
            continue;
        }

        if (getState() == State::ACTIVE_ATTACHED) {
            // To mute the local source, all the sources of the participating
            // calls must be muted. If it's the first participant, just use
            // its mute state.
            std::string sid = sip_utils::streamId("", iter->label_);
            callStreamsMgr_->muteStream(account->getUsername(), deviceId, sid, iter->muted_ or isMediaSourceMuted(iter->type_), true);
        }

        // Un-mute media in the call. The mute/un-mute state will be handled
        // by the conference/mixer from now on.
        iter->muted_ = false;
    }

    // Update the media states in the newly added call.
    call->requestMediaChange(MediaAttribute::mediaAttributesToMediaMaps(mediaList));

    // Notify the client
    for (auto mediaType : mediaTypeList) {
        if (mediaType == MediaType::MEDIA_AUDIO) {
            bool muted = isMediaSourceMuted(MediaType::MEDIA_AUDIO);
            JAMI_WARNING("Take over [AUDIO] control from call {:s} - current local source state [{:s}]",
                      callId,
                      muted ? "muted" : "un-muted");
            emitSignal<libjami::CallSignal::AudioMuted>(id_, muted);
        } else {
            bool muted = isMediaSourceMuted(MediaType::MEDIA_VIDEO);
            JAMI_WARNING("Take over [VIDEO] control from call {:s} - current local source state [{:s}]",
                      callId,
                      muted ? "muted" : "un-muted");
            emitSignal<libjami::CallSignal::VideoMuted>(id_, muted);
        }
    }
}

#ifdef ENABLE_VIDEO
void
Conference::resizeRemoteParticipants(ConfInfo& confInfo, const std::string& callId)
{
    int remoteFrameHeight = confInfo.h;
    int remoteFrameWidth = confInfo.w;
    std::pair<std::string, std::string> key;

    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(callId))) {
        key = {call->getRemoteUri(), call->getRemoteDeviceId()};
        if (remoteFrameHeight == 0 or remoteFrameWidth == 0) {
        // get the size of the remote frame from receiveThread
        // if the one from confInfo is empty
            for (auto const& videoRtp : call->getRtpSessionList(MediaType::MEDIA_VIDEO)) {
                if (auto recv = std::static_pointer_cast<video::VideoRtpSession>(videoRtp)->getVideoReceive()) {
                    remoteFrameHeight = recv->getHeight();
                    remoteFrameWidth = recv->getWidth();
                }
                // NOTE: this may be not the behavior we want, but this is only called
                // when we receive conferences informations from a call, so the peer is
                // mixing the video and send only one stream, so we can break here
                break;
            }
        }
    }

    // Rename the streamns to avoid remote host_video_0
    std::vector<std::string> toReplace;
    for (auto& [k,c] : confInfo.callInfo_) {
        for (auto& [id, remoteCell]: c.streams) {
            if (id.find("host_") == 0) {
                toReplace.emplace_back(id);
            }
        }
        for (const auto& oldId: toReplace) {
            auto newId = oldId;
            string_replace(newId, "host", callId);
            c.streams[newId] = std::move(c.streams[oldId]);
            c.streams.erase(oldId);
        }
    }

    if (remoteFrameHeight == 0 or remoteFrameWidth == 0) {
        JAMI_WARNING("Remote frame size not found.");
        return;
    }

    // get the size of the local frame
    StreamInfo localCell;
    std::lock_guard<std::mutex> lk(confInfoMutex_);
    for (const auto& [k,v] : confInfo_.callInfo_) {
        if (k == key && !v.streams.empty()) {
            localCell = v.streams.begin()->second;
            break;
        }
    }
    if (localCell.w == 0 || localCell.h == 0) {
        JAMI_WARNING("Incorrect local cell");
        return; // Cannot be empty
    }

    const float zoomX = (float) remoteFrameWidth / localCell.w;
    const float zoomY = (float) remoteFrameHeight / localCell.h;
    // Do the resize for each remote participant
    for (auto& [k,c] : confInfo.callInfo_) {
        for (auto& [id, remoteCell]: c.streams) {
            remoteCell.x = remoteCell.x / zoomX + localCell.x;
            remoteCell.y = remoteCell.y / zoomY + localCell.y;
            remoteCell.w = remoteCell.w / zoomX;
            remoteCell.h = remoteCell.h / zoomY;
        }
    }
}
#endif

void
Conference::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
#ifdef ENABLE_VIDEO
    if (callStreamsMgr_) {
        if (auto ob = rec->addStream(callStreamsMgr_->getStream("v:mixer"))) {
            callStreamsMgr_->attach(ob);
        }
    }
#endif

    // Audio
    // Create ghost participant for ringbufferpool
    auto& rbPool = Manager::instance().getRingBufferPool();
    ghostRingBuffer_ = rbPool.createRingBuffer(getConfId());

    // Bind it to ringbufferpool in order to get the all mixed frames
    bindCallId(getConfId());

    // Add stream to recorder
    audioMixer_ = jami::getAudioInput(getConfId());
    if (auto ob = rec->addStream(audioMixer_->getInfo("a:mixer"))) {
        audioMixer_->attach(ob);
    }
}

void
Conference::deinitRecorder(std::shared_ptr<MediaRecorder>& rec)
{
#ifdef ENABLE_VIDEO
    if (callStreamsMgr_) {
        if (auto ob = rec->getStream("v:mixer")) {
            callStreamsMgr_->detach(ob);
        }
    }
#endif

    // Audio
    if (auto ob = rec->getStream("a:mixer"))
        audioMixer_->detach(ob);
    audioMixer_.reset();
    Manager::instance().getRingBufferPool().unBindAll(getConfId());
    ghostRingBuffer_.reset();
}

void
Conference::sendConferenceInfos()
{
    // Inform calls that the layout has changed
    foreachCall([&](const auto& call) {
        // Produce specific JSON for each participant (2 separate accounts can host ...
        // a conference on a same device, the conference is not link to one account).
        auto w = call->getAccount();
        auto account = w.lock();
        if (!account)
            return;
        if (auto c = std::dynamic_pointer_cast<SIPCall>(call)) {
            dht::ThreadPool::io().run(
                [wc = std::weak_ptr<SIPCall>(c),
                confInfo = getConfInfoHostUri(account->getUsername() + "@ring.dht",
                                            c->getRemoteUri(), c->getRemoteDeviceId())] {
                    if (auto call = wc.lock())
                        call->sendConfInfo(confInfo.toString());
                });
        }
    });

    auto confInfo = getConfInfoHostUri();
#ifdef ENABLE_VIDEO
    createSinks(confInfo);
#endif

    JAMI_ERROR("@@@@ {}", confInfo.toString());

    // Inform client that layout has changed
    jami::emitSignal<libjami::CallSignal::OnConferenceInfosUpdated>(id_,
                                                                    confInfo
                                                                        .toVectorMapStringString());
}

ConfInfo
Conference::getConfInfoHostUri(std::string_view localHostURI, const std::string& peerUri, const std::string& peerDevice)
{
    ConfInfo newInfo;
    {
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        newInfo = confInfo_;
    }

    for (auto it = newInfo.callInfo_.begin(); it != newInfo.callInfo_.end();) {
        bool isRemoteHost = remoteHosts_.find(it->first) != remoteHosts_.end();
        if (isRemoteHost) {
            // Don't send back the ParticipantInfo for remote Host
            // For other than remote Host, the new info is in remoteHosts_
            it = newInfo.callInfo_.erase(it);
        } else {
            ++it;
        }
    }
    // Add remote Host info
    for (const auto& [hostUriDevice, confInfo] : remoteHosts_) {
        // Add remote info for remote host destination
        // Example: ConfA, ConfB & ConfC
        // ConfA send ConfA and ConfB for ConfC
        // ConfA send ConfA and ConfC for ConfB
        // ...
        if (peerUri != hostUriDevice.first || peerDevice != hostUriDevice.second)
            newInfo.callInfo_.insert(confInfo.callInfo_.begin(), confInfo.callInfo_.end());
    }
    return newInfo;
}

bool
Conference::isHost(std::string_view uri) const
{
    if (uri.empty())
        return true;

    // Check if the URI is a local URI (AccountID) for at least one of the subcall
    // (a local URI can be in the call with another device)
    for (const auto& p : getCallIds()) {
        if (auto call = getCall(p)) {
            if (auto account = call->getAccount().lock()) {
                if (account->getUsername() == uri)
                    return true;
            }
        }
    }
    return false;
}

std::pair<std::string, std::string>
Conference::findHostforRemoteParticipant(std::string_view uri, std::string_view deviceId)
{
    for (const auto& [host, confInfo] : remoteHosts_) {
        for (const auto& [key, _] : confInfo.callInfo_)
            if (uri == key.first && (deviceId == "" || deviceId == key.second))
                return host;
    }
    return {};
}

#ifdef ENABLE_PLUGIN
void
Conference::createConfAVStreams()
{
    auto audioMap = [](const std::shared_ptr<jami::MediaFrame>& m) -> AVFrame* {
        return std::static_pointer_cast<AudioFrame>(m)->pointer();
    };

    // Preview and Received
    if ((audioMixer_ = jami::getAudioInput(getConfId()))) {
        auto audioSubject = std::make_shared<MediaStreamSubject>(audioMap);
        StreamData previewStreamData {getConfId(), false, StreamType::audio, getConfId(), accountId_};
        createConfAVStream(previewStreamData, *audioMixer_, audioSubject);
        StreamData receivedStreamData {getConfId(), true, StreamType::audio, getConfId(), accountId_};
        createConfAVStream(receivedStreamData, *audioMixer_, audioSubject);
    }

#ifdef ENABLE_VIDEO
    if (callStreamsMgr_) {
        // Review
        auto receiveSubject = std::make_shared<MediaStreamSubject>(pluginVideoMap_);
        StreamData receiveStreamData {getConfId(), true, StreamType::video, getConfId(), accountId_};
        createConfAVStream(receiveStreamData, *callStreamsMgr_, receiveSubject);

        // Preview
        if (auto videoPreview = callStreamsMgr_->getVideoLocal()) {
            auto previewSubject = std::make_shared<MediaStreamSubject>(pluginVideoMap_);
            StreamData previewStreamData {getConfId(),
                                          false,
                                          StreamType::video,
                                          getConfId(),
                                          accountId_};
            createConfAVStream(previewStreamData, *videoPreview, previewSubject);
        }
    }
#endif
}

void
Conference::createConfAVStream(const StreamData& StreamData,
                               AVMediaStream& streamSource,
                               const std::shared_ptr<MediaStreamSubject>& mediaStreamSubject,
                               bool force)
{
    std::lock_guard<std::mutex> lk(avStreamsMtx_);
    const std::string AVStreamId = StreamData.id + std::to_string(static_cast<int>(StreamData.type))
                                   + std::to_string(StreamData.direction);
    auto it = confAVStreams.find(AVStreamId);
    if (!force && it != confAVStreams.end())
        return;

    confAVStreams.erase(AVStreamId);
    confAVStreams[AVStreamId] = mediaStreamSubject;
    streamSource.attachPriorityObserver(mediaStreamSubject);
    jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .createAVSubject(StreamData, mediaStreamSubject);
}
#endif // ENABLE_PLUGIN

} // namespace jami
