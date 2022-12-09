/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

#include <regex>
#include <sstream>

#include "conference.h"
#include "manager.h"
#include "audio/audiolayer.h"
#include "jamidht/jamiaccount.h"
#include "string_utils.h"
#include "sip/siptransport.h"

#include "client/videomanager.h"
#include "tracepoint.h"
#ifdef ENABLE_VIDEO
#include "call.h"
#include "video/video_input.h"
#include "video/video_mixer.h"
#endif

#ifdef ENABLE_PLUGIN
#include "plugin/jamipluginmanager.h"
#endif

#include "call_factory.h"

#include "logger.h"
#include "jami/media_const.h"
#include "audio/ringbufferpool.h"
#include "sip/sipcall.h"

#include <opendht/thread_pool.h>

using namespace std::literals;

namespace jami {

Conference::Conference(const std::shared_ptr<Account>& account,
                       const std::string& confId,
                       bool attachHost)
    : id_(confId.empty() ? Manager::instance().callFactory.getNewCallID() : confId)
    , account_(account)
#ifdef ENABLE_VIDEO
    , videoEnabled_(account->isVideoEnabled())
    , attachHost_(attachHost)
#endif
{
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
    setLocalHostDefaultMediaSource();
    duration_start_ = clock::now();

#ifdef ENABLE_VIDEO
    auto itVideo = std::find_if(hostSources_.begin(), hostSources_.end(), [&](auto attr) {
        return attr.type_ == MediaType::MEDIA_VIDEO;
    });
    // Only set host source if creating conference from joining calls
    auto hasVideo = videoEnabled_ && itVideo != hostSources_.end() && attachHost_;
    auto source = hasVideo ? itVideo->sourceUri_ : "";
    callStreamsMgr_ = std::make_unique<video::VideoMixer>(id_, source, hasVideo);
    callStreamsMgr_->setOnInfoUpdated([this](const StreamInfoMap& info) {
        ConfInfo newInfo;
        {
            std::lock_guard<std::mutex> lock(confInfoMutex_);
            newInfo.w = confInfo_.w;
            newInfo.h = confInfo_.h;
            newInfo.layout = static_cast<int>(callStreamsMgr_->getLayout());
        }
        for (const auto& [_k, v] : info) {
            newInfo.emplace_back(v);
        }
        updateConferenceInfo(std::move(newInfo));
    });
#endif

    parser_.onVersion([&](uint32_t) {}); // TODO
    parser_.onCheckAuthorization([&](std::string_view peerId) { return isModerator(peerId); });
    parser_.onHangupParticipant([&](const auto& accountUri, const auto& deviceId) {
        hangupParticipant(accountUri, deviceId);
    });
    parser_.onRaiseHand([&](const auto& uri, const auto& deviceId, bool state) { callStreamsMgr_->setHandRaised(uri, deviceId, state); });
    parser_.onSetActiveStream(
        [&](const auto& streamId, bool state) { callStreamsMgr_->setActiveStream(streamId, state); });
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
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCallFromPeerID(uri)))
            setHandRaised(uri, call->getRemoteDeviceId(), state);
    });

    parser_.onVoiceActivity(
        [&](const auto& streamId, bool state) { setVoiceActivity(streamId, state); });
    jami_tracepoint(conference_begin, id_.c_str());
}

Conference::~Conference()
{
    JAMI_INFO("Destroying conference %s", id_.c_str());

#ifdef ENABLE_VIDEO
    foreachCall([&](auto call) {
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
    if (videoMixer_) {
        auto& sink = videoMixer_->getSink();
        for (auto it = confSinksMap_.begin(); it != confSinksMap_.end();) {
            sink->detach(it->second.get());
            it->second->stop();
            it = confSinksMap_.erase(it);
        }
    }
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
    jami_tracepoint(conference_end, id_.c_str());
}

Conference::State
Conference::getState() const
{
    return confState_;
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
Conference::setLocalHostDefaultMediaSource()
{
    hostSources_.clear();
    // Setup local audio source
    MediaAttribute audioAttr;
    if (confState_ == State::ACTIVE_ATTACHED) {
        audioAttr
            = {MediaType::MEDIA_AUDIO, false, false, true, {}, sip_utils::DEFAULT_AUDIO_STREAMID};
    }

    JAMI_DEBUG("[conf {:s}] Setting local host audio source to [{:s}]", id_, audioAttr.toString());
    hostSources_.emplace_back(audioAttr);

#ifdef ENABLE_VIDEO
    if (isVideoEnabled()) {
        MediaAttribute videoAttr;
        // Setup local video source
        if (confState_ == State::ACTIVE_ATTACHED) {
            videoAttr
                = {MediaType::MEDIA_VIDEO,
                   false,
                   false,
                   true,
                   Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice(),
                   sip_utils::DEFAULT_VIDEO_STREAMID};
        }
        JAMI_DEBUG("[conf {:s}] Setting local host video source to [{:s}]",
                   id_,
                   videoAttr.toString());
        hostSources_.emplace_back(videoAttr);
    }
#endif

    reportMediaNegotiationStatus();
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
    return MediaAttribute::mediaAttributesToMediaMaps(hostSources_);
}

#ifdef ENABLE_PLUGIN
void
Conference::createConfAVStreams()
{
    std::string accountId = getAccountId();

    auto audioMap = [](const std::shared_ptr<jami::MediaFrame>& m) -> AVFrame* {
        return std::static_pointer_cast<AudioFrame>(m)->pointer();
    };

    // Preview and Received
    if ((audioMixer_ = jami::getAudioInput(getConfId()))) {
        auto audioSubject = std::make_shared<MediaStreamSubject>(audioMap);
        StreamData previewStreamData {getConfId(), false, StreamType::audio, getConfId(), accountId};
        createConfAVStream(previewStreamData, *audioMixer_, audioSubject);
        StreamData receivedStreamData {getConfId(), true, StreamType::audio, getConfId(), accountId};
        createConfAVStream(receivedStreamData, *audioMixer_, audioSubject);
    }

#ifdef ENABLE_VIDEO

    if (videoMixer_) {
        // Review
        auto receiveSubject = std::make_shared<MediaStreamSubject>(pluginVideoMap_);
        StreamData receiveStreamData {getConfId(), true, StreamType::video, getConfId(), accountId};
        createConfAVStream(receiveStreamData, *videoMixer_, receiveSubject);

        // Preview
        if (auto videoPreview = videoMixer_->getVideoLocal()) {
            auto previewSubject = std::make_shared<MediaStreamSubject>(pluginVideoMap_);
            StreamData previewStreamData {getConfId(),
                                          false,
                                          StreamType::video,
                                          getConfId(),
                                          accountId};
            createConfAVStream(previewStreamData, *videoPreview, previewSubject);
        }
    }
#endif // ENABLE_VIDEO
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

void
Conference::setLocalHostMuteState(MediaType type, bool muted)
{
    for (auto& source : hostSources_)
        if (source.type_ == type)
            source.muted_ = muted;
}

bool
Conference::isMediaSourceMuted(MediaType type) const
{
    if (getState() != State::ACTIVE_ATTACHED) {
        // Assume muted if not attached.
        return true;
    }

    if (type != MediaType::MEDIA_AUDIO and type != MediaType::MEDIA_VIDEO) {
        JAMI_ERR("Unsupported media type");
        return true;
    }

    for (const auto& source : hostSources_) {
        if (source.muted_ && source.type_ == type)
            return true;
        if (source.type_ == MediaType::MEDIA_NONE) {
            JAMI_WARN("The host source for %s is not set. The mute state is meaningless",
                      source.mediaTypeToString(source.type_));
            // Assume muted if the media is not present.
            return true;
        }
    }
    return false;
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
            if (participants_.size() == 1) {
                setLocalHostMuteState(iter->type_, iter->muted_);
            } else {
                setLocalHostMuteState(iter->type_, iter->muted_ and isMediaSourceMuted(iter->type_));
            }
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
            JAMI_WARN("Take over [AUDIO] control from call %s - current local source state [%s]",
                      callId.c_str(),
                      muted ? "muted" : "un-muted");
            emitSignal<libjami::CallSignal::AudioMuted>(id_, muted);
        } else {
            bool muted = isMediaSourceMuted(MediaType::MEDIA_VIDEO);
            JAMI_WARN("Take over [VIDEO] control from call %s - current local source state [%s]",
                      callId.c_str(),
                      muted ? "muted" : "un-muted");
            emitSignal<libjami::CallSignal::VideoMuted>(id_, muted);
        }
    }
}

bool
Conference::requestMediaChange(const std::vector<libjami::MediaMap>& mediaList)
{
    if (getState() != State::ACTIVE_ATTACHED) {
        JAMI_ERR("[conf %s] Request media change can be performed only in attached mode",
                 getConfId().c_str());
        return false;
    }

    JAMI_DEBUG("[conf {:s}] Request media change", getConfId());

    auto mediaAttrList = MediaAttribute::buildMediaAttributesList(mediaList, false);

    for (auto const& mediaAttr : mediaAttrList) {
        JAMI_DEBUG("[conf {:s}] New requested media: {:s}", getConfId(), mediaAttr.toString(true));
    }

    std::vector<std::string> newVideoInputs;
    for (auto const& mediaAttr : mediaAttrList) {
        // Find media
        auto oldIdx = std::find_if(hostSources_.begin(), hostSources_.end(), [&](auto oldAttr) {
            return oldAttr.sourceUri_ == mediaAttr.sourceUri_ && oldAttr.type_ == mediaAttr.type_;
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

#ifdef ENABLE_VIDEO
    if (videoMixer_)
        videoMixer_->switchInputs(newVideoInputs);
#endif
    hostSources_ = mediaAttrList; // New medias

    // It's host medias, so no need to negotiate anything, but inform the client.
    reportMediaNegotiationStatus();
    return true;
}

void
Conference::handleMediaChangeRequest(const std::shared_ptr<Call>& call,
                                     const std::vector<libjami::MediaMap>& remoteMediaList)
{
    JAMI_DEBUG("Conf [{:s}] Answer to media change request", getConfId());
    auto currentMediaList = hostSources_;

#ifdef ENABLE_VIDEO
    // If the new media list has video, remove the participant from audioonlylist.
    auto remoteHasVideo
        = MediaAttribute::hasMediaType(MediaAttribute::buildMediaAttributesList(remoteMediaList,
                                                                                false),
                                       MediaType::MEDIA_VIDEO);
    if (videoMixer_ && remoteHasVideo) {
        auto callId = call->getCallId();
        videoMixer_->removeAudioOnlySource(
            callId, std::string(sip_utils::streamId(callId, sip_utils::DEFAULT_AUDIO_STREAMID)));
    }
#endif

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
    for (auto const& media : currentMediaList) {
        if (media.enabled_ and not media.muted_)
            newMediaList.emplace_back(MediaAttribute::toMediaMap(media));
    }
    for (auto idx = newMediaList.size(); idx < remoteMediaList.size(); idx++)
        newMediaList.emplace_back(remoteMediaList[idx]);

    // NOTE:
    // Since this is a conference, newly added media will be also
    // accepted.
    // This also means that if original call was an audio-only call,
    // the local camera will be enabled, unless the video is disabled
    // in the account settings.
    call->answerMediaChangeRequest(newMediaList);
    call->enterConference(shared_from_this());
}

void
Conference::bindCall(const std::shared_ptr<Call>& call)
{
    const auto& callId = call->getCallId();
    JAMI_DEBUG("Adding call {:s} to conference {:s}", callId, id_);

    jami_tracepoint(conference_add_participant, id_.c_str(), callId.c_str());

    {
        std::lock_guard<std::mutex> lk(participantsMtx_);
        if (!participants_.insert(callId).second)
            return;
    }

    // NOTE:
    // When a call joins a conference, the media source of the call
    // will be set to the output of the conference mixer.
    takeOverMediaSourceControl(callId);
    auto w = call->getAccount();
    auto account = w.lock();
    if (account) {
        // Check for allModeratorEnabled preference
        auto isModerator = account->isAllModerators();
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
Conference::setActiveParticipant(const std::string& participant_id)
{
#ifdef ENABLE_VIDEO
    if (!callStreamsMgr_)
        return;
    if (isHost(participant_id)) {
        callStreamsMgr_->setActiveStream(sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID), true);
        return;
    }
    if (auto call = getCallFromPeerID(participant_id)) {
        callStreamsMgr_->setActiveStream(
            sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_VIDEO_STREAMID), true);
        return;
    }

    auto remoteHost = findHostforRemoteParticipant(participant_id);
    if (not remoteHost.empty()) {
        // This logic will be handled client side
        JAMI_WARN("Change remote layout is not supported");
        return;
    }
#endif
}

void
Conference::setActiveStream(const std::string& streamId, bool state)
{
    if (callStreamsMgr_)
        callStreamsMgr_->setActiveStream(streamId, state);
}

void
Conference::setRecording(const std::string& uri, const std::string& deviceId, bool state)
{
    if (callStreamsMgr_)
        callStreamsMgr_->setRecording(uri, deviceId, state);
}


void
Conference::setLayout(int layout)
{
    callStreamsMgr_->setLayout(layout);
}

std::vector<std::map<std::string, std::string>>
ConfInfo::toVectorMapStringString() const
{
    std::vector<std::map<std::string, std::string>> infos;
    infos.reserve(size());
    for (const auto& info : *this)
        infos.emplace_back(info.toMap());
    return infos;
}

std::string
ConfInfo::toString() const
{
    Json::Value val = {};
    for (const auto& info : *this) {
        val["p"].append(info.toJson());
    }
    val["w"] = w;
    val["h"] = h;
    val["v"] = v;
    val["layout"] = layout;
    return Json::writeString(Json::StreamWriterBuilder {}, val);
}

void
Conference::sendConferenceInfos()
{
    // Inform calls that the layout has changed
    foreachCall([&](auto call) {
        // Produce specific JSON for each participant (2 separate accounts can host ...
        // a conference on a same device, the conference is not link to one account).
        auto w = call->getAccount();
        auto account = w.lock();
        if (!account)
            return;

        dht::ThreadPool::io().run(
            [call,
             confInfo = getConfInfoHostUri(account->getUsername() + "@ring.dht",
                                           call->getPeerNumber())] {
                call->sendConfInfo(confInfo.toString());
            });
    });

    auto confInfo = getConfInfoHostUri("", "");
#ifdef ENABLE_VIDEO
    createSinks(confInfo);
#endif

    // Inform client that layout has changed
    jami::emitSignal<libjami::CallSignal::OnConferenceInfosUpdated>(id_,
                                                                    confInfo
                                                                        .toVectorMapStringString());
}

#ifdef ENABLE_VIDEO
void
Conference::createSinks(const ConfInfo& infos)
{
    std::lock_guard<std::mutex> lk(sinksMtx_);
    if (!videoMixer_)
        return;
    auto& sink = videoMixer_->getSink();
    Manager::instance().createSinkClients(getConfId(),
                                          infos,
                                          {std::static_pointer_cast<video::VideoFrameActiveWriter>(
                                              sink)},
                                          confSinksMap_);
}
#endif

void
Conference::removeParticipant(const std::string& participant_id)
{
    JAMI_DEBUG("Remove call {:s} in conference {:s}", participant_id, id_);
    {
        std::lock_guard<std::mutex> lk(participantsMtx_);
        if (!participants_.erase(participant_id))
            return;
    }
    if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(participant_id))) {
        const auto& peerId = call->getRemoteUri();
        const auto& deviceId = call->getRemoteDeviceId();
        callStreamsMgr_->removeCall(peerId, deviceId);
        call->exitConference();
        if (call->isPeerRecording())
            call->peerRecording(false);
    }
}

void
Conference::attachLocalParticipant()
{
    JAMI_INFO("Attach local participant to conference %s", id_.c_str());

    if (getState() == State::ACTIVE_DETACHED) {
        setState(State::ACTIVE_ATTACHED);
        setLocalHostDefaultMediaSource();

        auto& rbPool = Manager::instance().getRingBufferPool();
        for (const auto& participant : getParticipantList()) {
            if (auto call = Manager::instance().getCallFromCallID(participant)) {
                if (callStreamsMgr_->isMuted(sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_AUDIO_STREAMID)))
                    rbPool.bindHalfDuplexOut(participant, RingBufferPool::DEFAULT_ID);
                else
                    rbPool.bindCallID(participant, RingBufferPool::DEFAULT_ID);
                rbPool.flush(participant);
            }

            // Reset ringbuffer's readpointers
            rbPool.flush(participant);
        }
        rbPool.flush(RingBufferPool::DEFAULT_ID);

#ifdef ENABLE_VIDEO
        if (videoMixer_) {
            std::vector<std::string> videoInputs;
            for (const auto& source : hostSources_) {
                if (source.type_ == MediaType::MEDIA_VIDEO)
                    videoInputs.emplace_back(source.sourceUri_);
            }
            videoMixer_->switchInputs(videoInputs);
        }
#endif
    } else {
        JAMI_WARN(
            "Invalid conference state in attach participant: current \"%s\" - expected \"%s\"",
            getStateStr(),
            "ACTIVE_DETACHED");
    }
}

void
Conference::detachLocalParticipant()
{
    JAMI_INFO("Detach local participant from conference %s", id_.c_str());

    if (getState() == State::ACTIVE_ATTACHED) {
        foreachCall([&](auto call) {
            Manager::instance().getRingBufferPool().unBindCallID(call->getCallId(),
                                                                 RingBufferPool::DEFAULT_ID);
        });

#ifdef ENABLE_VIDEO
        if (videoMixer_)
            videoMixer_->stopInputs();
#endif
    } else {
        JAMI_WARN(
            "Invalid conference state in detach participant: current \"%s\" - expected \"%s\"",
            getStateStr(),
            "ACTIVE_ATTACHED");
        return;
    }

    setLocalHostDefaultMediaSource();
    setState(State::ACTIVE_DETACHED);
}

void
Conference::bindParticipant(const std::string& participant_id)
{
    JAMI_INFO("Bind participant %s to conference %s", participant_id.c_str(), id_.c_str());

    auto& rbPool = Manager::instance().getRingBufferPool();

    for (const auto& item : getParticipantList()) {
        if (participant_id != item) {
            // Do not attach muted participants
            if (auto call = Manager::instance().getCallFromCallID(item)) {
                if (callStreamsMgr_->isMuted(sip_utils::streamId(participant_id, sip_utils::DEFAULT_AUDIO_STREAMID)))
                    rbPool.bindHalfDuplexOut(item, participant_id);
                else
                    rbPool.bindCallID(participant_id, item);
            }
        }
        rbPool.flush(item);
    }

    // Bind local participant to other participants only if the
    // local is attached to the conference.
    if (getState() == State::ACTIVE_ATTACHED) {
        if (isMediaSourceMuted(MediaType::MEDIA_AUDIO))
            rbPool.bindHalfDuplexOut(RingBufferPool::DEFAULT_ID, participant_id);
        else
            rbPool.bindCallID(participant_id, RingBufferPool::DEFAULT_ID);
        rbPool.flush(RingBufferPool::DEFAULT_ID);
    }
}

void
Conference::unbindParticipant(const std::string& participant_id)
{
    JAMI_INFO("Unbind participant %s from conference %s", participant_id.c_str(), id_.c_str());
    Manager::instance().getRingBufferPool().unBindAllHalfDuplexOut(participant_id);
}

void
Conference::bindHost()
{
    JAMI_INFO("Bind host to conference %s", id_.c_str());

    auto& rbPool = Manager::instance().getRingBufferPool();

    for (const auto& item : getParticipantList()) {
        if (auto call = Manager::instance().getCallFromCallID(item)) {
            if (callStreamsMgr_->isMuted(sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_AUDIO_STREAMID)))
                continue;
            rbPool.bindCallID(item, RingBufferPool::DEFAULT_ID);
            rbPool.flush(RingBufferPool::DEFAULT_ID);
        }
    }
}

void
Conference::unbindHost()
{
    JAMI_INFO("Unbind host from conference %s", id_.c_str());
    Manager::instance().getRingBufferPool().unBindAllHalfDuplexOut(RingBufferPool::DEFAULT_ID);
}

ParticipantSet
Conference::getParticipantList() const
{
    std::lock_guard<std::mutex> lk(participantsMtx_);
    return participants_;
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
    foreachCall([&](auto call) { call->updateRecState(newState); });

    auto res = Recordable::toggleRecording();
    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock()))
        callStreamsMgr_->setRecording(acc->getUsername(), std::string(acc->currentDeviceId()), newState);
    return res;
}

std::string
Conference::getAccountId() const
{
    if (auto acc = account_.lock())
        return acc->getAccountID();
    return {};
}

void
Conference::switchInput(const std::string& input)
{
#ifdef ENABLE_VIDEO
    JAMI_DEBUG("[Conf:{:s}] Setting video input to {:s}", id_, input);
    std::vector<MediaAttribute> newSources;
    auto firstVideo = true;
    // Rewrite hostSources (remove all except one video input)
    // This method is replaced by requestMediaChange
    for (auto& source : hostSources_) {
        if (source.type_ == MediaType::MEDIA_VIDEO) {
            if (firstVideo) {
                firstVideo = false;
                source.sourceUri_ = input;
                newSources.emplace_back(source);
            }
        } else {
            newSources.emplace_back(source);
        }
    }

    // Done if the video is disabled
    if (not isVideoEnabled())
        return;

    if (auto mixer = videoMixer_) {
        mixer->switchInputs({input});
#ifdef ENABLE_PLUGIN
        // Preview
        if (auto videoPreview = mixer->getVideoLocal()) {
            auto previewSubject = std::make_shared<MediaStreamSubject>(pluginVideoMap_);
            StreamData previewStreamData {getConfId(),
                                          false,
                                          StreamType::video,
                                          getConfId(),
                                          getAccountId()};
            createConfAVStream(previewStreamData, *videoPreview, previewSubject, true);
        }
#endif
    }
#endif
}

bool
Conference::isVideoEnabled() const
{
    if (auto shared = account_.lock())
        return shared->isVideoEnabled();
    return false;
}

#ifdef ENABLE_VIDEO
std::shared_ptr<video::VideoMixer>
Conference::getVideoMixer()
{
    return videoMixer_;
}

std::string
Conference::getVideoInput() const
{
    for (const auto& source : hostSources_) {
        if (source.type_ == MediaType::MEDIA_VIDEO)
            return source.sourceUri_;
    }
    return {};
}
#endif

void
Conference::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
#ifdef ENABLE_VIDEO
    // Video
    if (videoMixer_) {
        if (auto ob = rec->addStream(videoMixer_->getStream("v:mixer"))) {
            videoMixer_->attach(ob);
        }
    }
#endif

    // Audio
    // Create ghost participant for ringbufferpool
    auto& rbPool = Manager::instance().getRingBufferPool();
    ghostRingBuffer_ = rbPool.createRingBuffer(getConfId());

    // Bind it to ringbufferpool in order to get the all mixed frames
    bindParticipant(getConfId());

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
    // Video
    if (videoMixer_) {
        if (auto ob = rec->getStream("v:mixer")) {
            videoMixer_->detach(ob);
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
Conference::onConfOrder(const std::string& callId, const std::string& confOrder)
{
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

std::shared_ptr<Call>
Conference::getCall(const std::string& callId)
{
    return Manager::instance().callFactory.getCall(callId);
}

bool
Conference::isModerator(std::string_view uri) const
{
    return moderators_.find(uri) != moderators_.end() or isHost(uri);
}

void
Conference::setHandRaised(const std::string& uri, const std::string& deviceId, const bool& state)
{
    if (callStreamsMgr_)
        callStreamsMgr_->setHandRaised(uri, deviceId, state);
}

void
Conference::setVoiceActivity(const std::string& streamId, const bool& newState)
{
    if (callStreamsMgr_)
        callStreamsMgr_->setVoiceActivity(streamId, newState);
}

void
Conference::setModerator(const std::string& participant_id, const bool& state)
{
    for (const auto& p : getParticipantList()) {
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(p))) {
            auto isPeerModerator = isModerator(participant_id);
            if (participant_id == call->getRemoteUri()) {
                if (state and not isPeerModerator) {
                    JAMI_DEBUG("Add {:s} as moderator", participant_id);
                    moderators_.emplace(participant_id);
                    updateModerators();
                } else if (not state and isPeerModerator) {
                    JAMI_DEBUG("Remove {:s} as moderator", participant_id);
                    moderators_.erase(participant_id);
                    updateModerators();
                }
                return;
            }
        }
    }
    JAMI_WARN("Fail to set %s as moderator (participant not found)", participant_id.c_str());
}

void
Conference::updateModerators()
{
    std::lock_guard<std::mutex> lk(confInfoMutex_);
    for (auto& info : confInfo_) {
        info.isModerator = isModerator(string_remove_suffix(info.uri, '@'));
    }
    sendConferenceInfos();
}

void
Conference::foreachCall(const std::function<void(const std::shared_ptr<Call>& call)>& cb)
{
    for (const auto& p : getParticipantList())
        if (auto call = getCall(p))
            cb(call);
}

void
Conference::muteStream(const std::string& accountUri,
                       const std::string& deviceId,
                       const std::string& streamId,
                       const bool& state)
{
    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock())) {
        // TODO accountUri, deviceId
        callStreamsMgr_->muteStream(streamId, state);
    }
}

void
Conference::muteHost(bool state)
{
    // TODO move
    auto isHostMuted = callStreamsMgr_->isMuted(sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID));
    if (state and not isHostMuted) {
        if (not isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DBG("Mute host");
            unbindHost();
        }
    } else if (not state and isHostMuted) {
        if (not isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DBG("Unmute host");
            bindHost();
        }
    }
    updateMuted();
}

void
Conference::muteCall(const std::string& callId, bool state)
{
    // TODO move
    auto isPartMuted = callStreamsMgr_->isMuted(sip_utils::streamId(callId, sip_utils::DEFAULT_AUDIO_STREAMID));
    if (state and not isPartMuted) {
        JAMI_DEBUG("Mute participant {:s}", callId);
        unbindParticipant(callId);
        updateMuted();
    } else if (not state and isPartMuted) {
        JAMI_DEBUG("Unmute participant {:s}", callId);
        bindParticipant(callId);
        updateMuted();
    }
}

void
Conference::muteParticipant(const std::string& participant_id, const bool& state)
{
    // Prioritize remote mute, otherwise the mute info is lost during
    // the conference merge (we don't send back info to remoteHost,
    // cf. getConfInfoHostUri method)

    // Transfert remote participant mute
    auto remoteHost = findHostforRemoteParticipant(participant_id);
    if (not remoteHost.empty()) {
        if (auto call = getCallFromPeerID(string_remove_suffix(remoteHost, '@'))) {
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                return;
            Json::Value root;
            root["muteParticipant"] = participant_id;
            root["muteState"] = state ? TRUE_STR : FALSE_STR;
            call->sendConfOrder(root);
            return;
        }
    }

    // NOTE: For now we only have one audio per call, and no way to only
    // mute one stream
    if (isHost(participant_id))
        muteHost(state);
    else if (auto call = getCallFromPeerID(participant_id))
        muteCall(call->getCallId(), state);
}

void
Conference::updateMuted()
{
    std::lock_guard<std::mutex> lk(confInfoMutex_);
    for (auto& info : confInfo_) {
        if (info.uri.empty()) {
            info.audioModeratorMuted = callStreamsMgr_->isMuted(sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID));
            info.audioLocalMuted = isMediaSourceMuted(MediaType::MEDIA_AUDIO);
        } else if (auto call = getCallWith(std::string(string_remove_suffix(info.uri, '@')),
                                           info.device)) {
            info.audioModeratorMuted = callStreamsMgr_->isMuted(sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_AUDIO_STREAMID));
            info.audioLocalMuted = call->isPeerMuted();
        }
    }
    sendConferenceInfos();
}

ConfInfo
Conference::getConfInfoHostUri(std::string_view localHostURI, std::string_view destURI)
{
    ConfInfo newInfo = confInfo_;

    for (auto it = newInfo.begin(); it != newInfo.end();) {
        bool isRemoteHost = remoteHosts_.find(it->uri) != remoteHosts_.end();
        if (it->uri.empty() and not destURI.empty()) {
            // fill the empty uri with the local host URI, let void for local client
            it->uri = localHostURI;
        }
        if (isRemoteHost) {
            // Don't send back the ParticipantInfo for remote Host
            // For other than remote Host, the new info is in remoteHosts_
            it = newInfo.erase(it);
        } else {
            ++it;
        }
    }
    // Add remote Host info
    for (const auto& [hostUri, confInfo] : remoteHosts_) {
        // Add remote info for remote host destination
        // Example: ConfA, ConfB & ConfC
        // ConfA send ConfA and ConfB for ConfC
        // ConfA send ConfA and ConfC for ConfB
        // ...
        if (destURI != hostUri)
            newInfo.insert(newInfo.end(), confInfo.begin(), confInfo.end());
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
    for (const auto& p : getParticipantList()) {
        if (auto call = getCall(p)) {
            if (auto account = call->getAccount().lock()) {
                if (account->getUsername() == uri)
                    return true;
            }
        }
    }
    return false;
}

bool
Conference::isHostDevice(std::string_view deviceId) const
{
    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock()))
        return deviceId == acc->currentDeviceId();
    return false;
}

void
Conference::updateConferenceInfo(ConfInfo confInfo)
{
    std::lock_guard<std::mutex> lk(confInfoMutex_);
    confInfo_ = std::move(confInfo);
    sendConferenceInfos();
}

void
Conference::hangupParticipant(const std::string& accountUri, const std::string& deviceId)
{
    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock())) {
        if (deviceId.empty()) {
            // If deviceId is empty, hangup all calls with device
            while (auto call = getCallFromPeerID(accountUri)) {
                Manager::instance().hangupCall(acc->getAccountID(), call->getCallId());
            }
            return;
        } else {
            if (accountUri == acc->getUsername() && deviceId == acc->currentDeviceId()) {
                Manager::instance().detachLocalParticipant(shared_from_this());
                return;
            } else if (auto call = getCallWith(accountUri, deviceId)) {
                Manager::instance().hangupCall(acc->getAccountID(), call->getCallId());
                return;
            }
        }
        // Else, it may be a remote host
        auto remoteHost = findHostforRemoteParticipant(accountUri, deviceId);
        if (remoteHost.empty()) {
            JAMI_WARN("Can't hangup %s, peer not found", accountUri.c_str());
            return;
        }
        if (auto call = getCallFromPeerID(string_remove_suffix(remoteHost, '@'))) {
            // Forward to the remote host.
            libjami::hangupParticipant(acc->getAccountID(), call->getCallId(), accountUri, deviceId);
        }
    }
}

void
Conference::muteLocalHost(bool is_muted, const std::string& mediaType)
{
    if (mediaType.compare(libjami::Media::Details::MEDIA_TYPE_AUDIO) == 0) {
        if (is_muted == isMediaSourceMuted(MediaType::MEDIA_AUDIO)) {
            JAMI_DEBUG("Local audio source already in [{:s}] state",
                       is_muted ? "muted" : "un-muted");
            return;
        }

        auto isHostMuted = callStreamsMgr_->isMuted(sip_utils::streamId("", sip_utils::DEFAULT_AUDIO_STREAMID));
        if (is_muted and not isMediaSourceMuted(MediaType::MEDIA_AUDIO) and not isHostMuted) {
            JAMI_DBG("Muting local audio source");
            unbindHost();
        } else if (not is_muted and isMediaSourceMuted(MediaType::MEDIA_AUDIO) and not isHostMuted) {
            JAMI_DBG("Un-muting local audio source");
            bindHost();
        }
        setLocalHostMuteState(MediaType::MEDIA_AUDIO, is_muted);
        updateMuted();
        emitSignal<libjami::CallSignal::AudioMuted>(id_, is_muted);
        return;
    } else if (mediaType.compare(libjami::Media::Details::MEDIA_TYPE_VIDEO) == 0) {
#ifdef ENABLE_VIDEO
        if (not isVideoEnabled()) {
            JAMI_ERR("Cant't mute, the video is disabled!");
            return;
        }

        if (is_muted == isMediaSourceMuted(MediaType::MEDIA_VIDEO)) {
            JAMI_DEBUG("Local video source already in [{:s}] state",
                       is_muted ? "muted" : "un-muted");
            return;
        }
        setLocalHostMuteState(MediaType::MEDIA_VIDEO, is_muted);
        if (is_muted) {
            if (auto mixer = videoMixer_) {
                JAMI_DBG("Muting local video sources");
                mixer->stopInputs();
            }
        } else {
            if (auto mixer = videoMixer_) {
                JAMI_DBG("Un-muting local video sources");
                std::vector<std::string> videoInputs;
                for (const auto& source : hostSources_) {
                    if (source.type_ == MediaType::MEDIA_VIDEO)
                        videoInputs.emplace_back(source.sourceUri_);
                }
                mixer->switchInputs(videoInputs);
            }
        }
        emitSignal<libjami::CallSignal::VideoMuted>(id_, is_muted);
        return;
#endif
    }
}

#ifdef ENABLE_VIDEO
void
Conference::resizeRemoteParticipants(ConfInfo& confInfo, std::string_view peerURI)
{
    int remoteFrameHeight = confInfo.h;
    int remoteFrameWidth = confInfo.w;

    if (remoteFrameHeight == 0 or remoteFrameWidth == 0) {
        // get the size of the remote frame from receiveThread
        // if the one from confInfo is empty
        if (auto call = std::dynamic_pointer_cast<SIPCall>(
                getCallFromPeerID(string_remove_suffix(peerURI, '@')))) {
            for (auto const& videoRtp : call->getRtpSessionList(MediaType::MEDIA_VIDEO)) {
                auto recv = std::static_pointer_cast<video::VideoRtpSession>(videoRtp)
                                ->getVideoReceive();
                remoteFrameHeight = recv->getHeight();
                remoteFrameWidth = recv->getWidth();
                // NOTE: this may be not the behavior we want, but this is only called
                // when we receive conferences informations from a call, so the peer is
                // mixing the video and send only one stream, so we can break here
                break;
            }
        }
    }

    if (remoteFrameHeight == 0 or remoteFrameWidth == 0) {
        JAMI_WARN("Remote frame size not found.");
        return;
    }

    // get the size of the local frame
    StreamInfo localCell;
    for (const auto& p : confInfo_) {
        if (p.uri == peerURI) {
            localCell = p;
            break;
        }
    }

    const float zoomX = (float) remoteFrameWidth / localCell.w;
    const float zoomY = (float) remoteFrameHeight / localCell.h;
    // Do the resize for each remote participant
    for (auto& remoteCell : confInfo) {
        remoteCell.x = remoteCell.x / zoomX + localCell.x;
        remoteCell.y = remoteCell.y / zoomY + localCell.y;
        remoteCell.w = remoteCell.w / zoomX;
        remoteCell.h = remoteCell.h / zoomY;
    }
}
#endif

void
Conference::mergeConfInfo(ConfInfo& newInfo, const std::string& peerURI)
{
    if (newInfo.empty()) {
        JAMI_DBG("confInfo empty, remove remoteHost");
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        remoteHosts_.erase(peerURI);
        sendConferenceInfos();
        return;
    }

#ifdef ENABLE_VIDEO
    resizeRemoteParticipants(newInfo, peerURI);
#endif

    bool updateNeeded = false;
    auto it = remoteHosts_.find(peerURI);
    if (it != remoteHosts_.end()) {
        // Compare confInfo before update
        if (it->second != newInfo) {
            it->second = newInfo;
            updateNeeded = true;
        } else
            JAMI_WARN("No change in confInfo, don't update");
    } else {
        remoteHosts_.emplace(peerURI, newInfo);
        updateNeeded = true;
    }
    // Send confInfo only if needed to avoid loops
#ifdef ENABLE_VIDEO
    if (updateNeeded and videoMixer_) {
        // Trigger the layout update in the mixer because the frame resolution may
        // change from participant to conference and cause a mismatch between
        // confInfo layout and rendering layout.
        videoMixer_->updateLayout();
    }
#endif
}

std::string_view
Conference::findHostforRemoteParticipant(std::string_view uri, std::string_view deviceId)
{
    for (const auto& host : remoteHosts_) {
        for (const auto& p : host.second) {
            if (uri == string_remove_suffix(p.uri, '@') && (deviceId == "" || deviceId == p.device))
                return host.first;
        }
    }
    return "";
}

std::shared_ptr<Call>
Conference::getCallFromPeerID(std::string_view peerId)
{
    for (const auto& p : getParticipantList()) {
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(p))) {
            if (call->getRemoteUri() == peerId)
                return call;
        }
    }
    return nullptr;
}

std::shared_ptr<Call>
Conference::getCallWith(const std::string& accountUri, const std::string& deviceId)
{
    for (const auto& p : getParticipantList()) {
        if (auto call = std::dynamic_pointer_cast<SIPCall>(getCall(p))) {
            if (accountUri == call->getRemoteUri()
                && deviceId == call->getRemoteDeviceId()) {
                return call;
            }
        }
    }
    return {};
}

void
Conference::stopRecording()
{
    Recordable::stopRecording();

    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock()))
        callStreamsMgr_->setRecording(acc->getUsername(), std::string(acc->currentDeviceId()), false);
}

bool
Conference::startRecording(const std::string& path)
{
    auto res = Recordable::startRecording(path);

    if (auto acc = std::dynamic_pointer_cast<JamiAccount>(account_.lock()))
        callStreamsMgr_->setRecording(acc->getUsername(), std::string(acc->currentDeviceId()), true);
    return res;
}

} // namespace jami
