/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#ifdef ENABLE_VIDEO
#include "sip/sipcall.h"
#include "client/videomanager.h"
#include "video/video_input.h"
#include "video/video_mixer.h"
#endif

#include "call_factory.h"

#include "logger.h"
#include "dring/media_const.h"
#include "audio/ringbufferpool.h"

using namespace std::literals;

namespace jami {

Conference::Conference()
    : id_(Manager::instance().getNewCallID())
#ifdef ENABLE_VIDEO
    , mediaInput_(Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice())
#endif
{
    JAMI_INFO("Create new conference %s", id_.c_str());

    // TODO: For now, add all accounts on the same device as
    // conference master. In the future, this should be
    // retrieven with another way
    auto accounts = jami::Manager::instance().getAllAccounts<JamiAccount>();
    for (const auto& account : accounts) {
        if (!account)
            continue;
        moderators_.emplace(account->getUsername());
    }

#ifdef ENABLE_VIDEO
    getVideoMixer()->setOnSourcesUpdated([this](const std::vector<video::SourceInfo>&& infos) {
        runOnMainThread([w = weak(), infos = std::move(infos)] {
            auto shared = w.lock();
            if (!shared)
                return;
            ConfInfo newInfo;
            auto subCalls = shared->participants_;
            // Handle participants showing their video
            std::unique_lock<std::mutex> lk(shared->videoToCallMtx_);
            for (const auto& info : infos) {
                std::string uri = "";
                auto it = shared->videoToCall_.find(info.source);
                if (it == shared->videoToCall_.end())
                    it = shared->videoToCall_.emplace_hint(it, info.source, std::string());
                // If not local
                if (!it->second.empty()) {
                    // Retrieve calls participants
                    // TODO: this is a first version, we assume that the peer is not
                    // a master of a conference and there is only one remote
                    // In the future, we should retrieve confInfo from the call
                    // To merge layouts informations
                    if (auto call = Manager::instance().callFactory.getCall<SIPCall>(it->second))
                        uri = call->getPeerNumber();
                }
                auto active = false;
                if (auto videoMixer = shared->getVideoMixer())
                    active = info.source == videoMixer->getActiveParticipant();
                subCalls.erase(it->second);
                std::string_view partURI = uri;
                partURI = string_remove_suffix(partURI, '@');
                auto isModerator = shared->isModerator(partURI);
                if (uri.empty())
                    partURI = "host";
                auto isMuted = shared->isMuted(partURI);
                newInfo.emplace_back(ParticipantInfo {std::move(uri),
                                                      "",
                                                      active,
                                                      info.x,
                                                      info.y,
                                                      info.w,
                                                      info.h,
                                                      !info.hasVideo,
                                                      isMuted,
                                                      isModerator});
            }
            lk.unlock();
            // Handle participants not present in the video mixer
            for (const auto& subCall : subCalls) {
                std::string uri = "";
                if (auto call = Manager::instance().callFactory.getCall<SIPCall>(subCall))
                    uri = call->getPeerNumber();
                auto isModerator = shared->isModerator(uri);
                newInfo.emplace_back(
                    ParticipantInfo {std::move(uri), "", false, 0, 0, 0, 0, true, false, isModerator});
            }

            {
                std::lock_guard<std::mutex> lk2(shared->confInfoMutex_);
                shared->confInfo_ = std::move(newInfo);
            }

            shared->sendConferenceInfos();
        });
    });
#endif
}

Conference::~Conference()
{
    JAMI_INFO("Destroy conference %s", id_.c_str());

#ifdef ENABLE_VIDEO
    for (const auto& participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            call->getVideoRtp().exitConference();
            // Reset distant callInfo
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                continue;
            call->sendTextMessage(std::map<std::string, std::string> {{"application/confInfo+json",
                                                                       "[]"}},
                                  account->getFromUri());
            // Continue the recording for the call if the conference was recorded
            if (this->isRecording()) {
                JAMI_DBG("Stop recording for conf %s", getConfID().c_str());
                this->toggleRecording();
                if (not call->isRecording()) {
                    JAMI_DBG("Conference was recorded, start recording for conf %s",
                             call->getCallId().c_str());
                    call->toggleRecording();
                }
            }
            // Notify that the remaining peer is still recording after conference
            if (call->isPeerRecording())
                call->setRemoteRecording(true);
        }
    }
#endif // ENABLE_VIDEO
}

Conference::State
Conference::getState() const
{
    return confState_;
}

void
Conference::setState(State state)
{
    confState_ = state;
}

void
Conference::add(const std::string& participant_id)
{
    if (participants_.insert(participant_id).second) {
        // Check if participant was muted before conference
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            if (call->isPeerMuted()) {
                auto uri = call->getPeerNumber();
                uri = string_remove_suffix(uri, '@');
                participantsMuted_.emplace(uri);
            }
        }
#ifdef ENABLE_VIDEO
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            call->getVideoRtp().enterConference(this);
            // Continue the recording for the conference if one participant was recording
            if (call->isRecording()) {
                JAMI_DBG("Stop recording for call %s", call->getCallId().c_str());
                call->toggleRecording();
                if (not this->isRecording()) {
                    JAMI_DBG("One participant was recording, start recording for conference %s",
                             getConfID().c_str());
                    this->toggleRecording();
                }
            }
        } else
            JAMI_ERR("no call associate to participant %s", participant_id.c_str());
#endif // ENABLE_VIDEO
    }
}

void
Conference::setActiveParticipant(const std::string& participant_id)
{
    if (!videoMixer_)
        return;
    if (isHost(participant_id)) {
        videoMixer_->setActiveHost();
        return;
    }
    for (const auto& item : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(item)) {
            if (participant_id == item
                || call->getPeerNumber().find(participant_id) != std::string::npos) {
                videoMixer_->setActiveParticipant(call->getVideoRtp().getVideoReceive().get());
                return;
            }
        }
    }
    // Unset active participant by default
    videoMixer_->setActiveParticipant(nullptr);
}

void
Conference::setLayout(int layout)
{
    switch (layout) {
    case 0:
        getVideoMixer()->setVideoLayout(video::Layout::GRID);
        // The layout shouldn't have an active participant
        if (videoMixer_->getActiveParticipant())
            videoMixer_->setActiveParticipant(nullptr);
        break;
    case 1:
        getVideoMixer()->setVideoLayout(video::Layout::ONE_BIG_WITH_SMALL);
        break;
    case 2:
        getVideoMixer()->setVideoLayout(video::Layout::ONE_BIG);
        break;
    default:
        break;
    }
}

std::vector<std::map<std::string, std::string>>
ConfInfo::toVectorMapStringString() const
{
    std::vector<std::map<std::string, std::string>> infos;
    infos.reserve(size());
    auto it = cbegin();
    while (it != cend()) {
        infos.emplace_back(it->toMap());
        ++it;
    }
    return infos;
}

void
Conference::sendConferenceInfos()
{
    // Inform calls that the layout has changed
    for (const auto& participant_id : participants_) {
        // Produce specific JSON for each participant (2 separate accounts can host ...
        // a conference on a same device, the conference is not link to one account).
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                continue;

            ConfInfo confInfo = std::move(getConfInfoHostUri(account->getUsername()+ "@ring.dht"));
            Json::Value jsonArray = {};
            {
                std::lock_guard<std::mutex> lk2(confInfoMutex_);
                for (const auto& info : confInfo) {
                    jsonArray.append(info.toJson());
                }
            }

            Json::StreamWriterBuilder builder = {};
            const auto confInfoStr = Json::writeString(builder, jsonArray);
            call->sendTextMessage(std::map<std::string, std::string> {{"application/confInfo+json",
                                                                    confInfoStr}},
                                account->getFromUri());
        }
    }

    // Inform client that layout has changed
    jami::emitSignal<DRing::CallSignal::OnConferenceInfosUpdated>(id_, confInfo_.toVectorMapStringString());
}

void
Conference::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame, const std::string& callId)
{
    std::lock_guard<std::mutex> lk(videoToCallMtx_);
    videoToCall_.emplace(frame, callId);
    frame->attach(getVideoMixer().get());
}

void
Conference::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    std::lock_guard<std::mutex> lk(videoToCallMtx_);
    auto it = videoToCall_.find(frame);
    if (it != videoToCall_.end()) {
        it->first->detach(getVideoMixer().get());
        videoToCall_.erase(it);
    }
}

void
Conference::remove(const std::string& participant_id)
{
    if (participants_.erase(participant_id)) {
#ifdef ENABLE_VIDEO
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            call->getVideoRtp().exitConference();
            if (call->isPeerRecording())
                call->setRemoteRecording(false);
        }
#endif // ENABLE_VIDEO
    }
}

void
Conference::attach()
{
    JAMI_INFO("Attach local participant to conference %s", id_.c_str());

    if (getState() == State::ACTIVE_DETACHED) {
        auto& rbPool = Manager::instance().getRingBufferPool();
        for (const auto& participant : getParticipantList()) {
            rbPool.bindCallID(participant, RingBufferPool::DEFAULT_ID);
            // Reset ringbuffer's readpointers
            rbPool.flush(participant);
        }
        rbPool.flush(RingBufferPool::DEFAULT_ID);

#ifdef ENABLE_VIDEO
        if (auto mixer = getVideoMixer()) {
            mixer->switchInput(mediaInput_);
        }
#endif
        setState(State::ACTIVE_ATTACHED);
    } else {
        JAMI_WARN(
            "Invalid conference state in attach participant: current \"%s\" - expected \"%s\"",
            getStateStr(),
            "ACTIVE_DETACHED");
    }
}

void
Conference::detach()
{
    JAMI_INFO("Detach local participant from conference %s", id_.c_str());

    if (getState() == State::ACTIVE_ATTACHED) {
        Manager::instance().getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);
#ifdef ENABLE_VIDEO
        if (auto mixer = getVideoMixer()) {
            mixer->stopInput();
        }
#endif
        setState(State::ACTIVE_DETACHED);
    } else {
        JAMI_WARN(
            "Invalid conference state in detach participant: current \"%s\" - expected \"%s\"",
            getStateStr(),
            "ACTIVE_ATTACHED");
    }
}

void
Conference::bindParticipant(const std::string& participant_id)
{
    JAMI_INFO("Bind participant %s to conference %s", participant_id.c_str(), id_.c_str());

    auto& rbPool = Manager::instance().getRingBufferPool();

    for (const auto& item : participants_) {
        if (participant_id != item)
            rbPool.bindCallID(participant_id, item);
        rbPool.flush(item);
    }

    // Bind local participant to other participants only if the
    // local is attached to the conference.
    if (getState() == State::ACTIVE_ATTACHED) {
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

    for (const auto& item : participants_) {
        if (auto call = Manager::instance().getCallFromCallID(item)) {
            std::string_view uri = call->getPeerNumber();
            uri = string_remove_suffix(uri, '@');
            if (isMuted(uri))
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

const ParticipantSet&
Conference::getParticipantList() const
{
    return participants_;
}

std::vector<std::string>
Conference::getDisplayNames() const
{
    std::vector<std::string> result;
    result.reserve(participants_.size());

    for (const auto& p : participants_) {
        auto details = Manager::instance().getCallDetails(p);
        const auto tmp = details["DISPLAY_NAME"];
        result.emplace_back(tmp.empty() ? details["PEER_NUMBER"] : tmp);
    }
    return result;
}

bool
Conference::toggleRecording()
{
    bool newState = not isRecording();
    if (newState)
        initRecorder(recorder_);
    else
        deinitRecorder(recorder_);

    // Notify each participant
    for (const auto& participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            call->updateRecState(newState);
        }
    }

    return Recordable::toggleRecording();
}

const std::string&
Conference::getConfID() const
{
    return id_;
}

void
Conference::switchInput(const std::string& input)
{
#ifdef ENABLE_VIDEO
    mediaInput_ = input;
    getVideoMixer()->switchInput(input);
#endif
}

#ifdef ENABLE_VIDEO
std::shared_ptr<video::VideoMixer>
Conference::getVideoMixer()
{
    if (!videoMixer_)
        videoMixer_.reset(new video::VideoMixer(id_));
    return videoMixer_;
}
#endif

void
Conference::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    // Video
    if (videoMixer_) {
        if (auto ob = rec->addStream(videoMixer_->getStream("v:mixer"))) {
            videoMixer_->attach(ob);
        }
    }

    // Audio
    // Create ghost participant for ringbufferpool
    auto& rbPool = Manager::instance().getRingBufferPool();
    ghostRingBuffer_ = rbPool.createRingBuffer(getConfID());

    // Bind it to ringbufferpool in order to get the all mixed frames
    bindParticipant(getConfID());

    // Add stream to recorder
    audioMixer_ = jami::getAudioInput(getConfID());
    if (auto ob = rec->addStream(audioMixer_->getInfo("a:mixer"))) {
        audioMixer_->attach(ob);
    }
}

void
Conference::deinitRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    // Video
    if (videoMixer_) {
        if (auto ob = rec->getStream("v:mixer")) {
            videoMixer_->detach(ob);
        }
    }

    // Audio
    if (auto ob = rec->getStream("a:mixer"))
        audioMixer_->detach(ob);
    audioMixer_.reset();
    Manager::instance().getRingBufferPool().unBindAll(getConfID());
    ghostRingBuffer_.reset();
}

void
Conference::onConfOrder(const std::string& callId, const std::string& confOrder)
{
    // Check if the peer is a master
    if (auto call = Manager::instance().getCallFromCallID(callId)) {
        std::string_view uri = call->getPeerNumber();
        uri = string_remove_suffix(uri, '@');
        if (!isModerator(uri)) {
            JAMI_WARN("Received conference order from a non master (%s)", uri.data());
            return;
        }

        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(confOrder.c_str(), confOrder.c_str() + confOrder.size(), &root, &err)) {
            JAMI_WARN("Couldn't parse conference order from %s", uri.data());
            return;
        }
        if (root.isMember("layout")) {
            setLayout(root["layout"].asUInt());
        }
        if (root.isMember("activeParticipant")) {
            setActiveParticipant(root["activeParticipant"].asString());
        }
        if (root.isMember("muteParticipant")  and root.isMember("muteState")) {
            muteParticipant(root["muteParticipant"].asString(), root["muteState"].asString() == "true");
        }
        if (root.isMember("hangupParticipant")) {
            hangupParticipant(root["hangupParticipant"].asString());
        }
    }
}

bool
Conference::isModerator(std::string_view uri) const
{
    return std::find_if(moderators_.begin(),
                        moderators_.end(),
                        [&uri](std::string_view moderator) {
                            return moderator.find(uri) != std::string_view::npos;
                        })
           != moderators_.end() or isHost(uri);
}

void
Conference::setModerator(const std::string& uri, const bool& state)
{
    for (const auto& p : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(p)) {
            std::string_view partURI = call->getPeerNumber();
            partURI = string_remove_suffix(partURI, '@');
            if (partURI == uri) {
                if (state and not isModerator(uri)) {
                    JAMI_DBG("Add %s as moderator", partURI.data());
                    moderators_.emplace(uri);
                    updateModerators();
                } else if (not state and isModerator(uri)) {
                    JAMI_DBG("Remove %s as moderator", partURI.data());
                    moderators_.erase(uri);
                    updateModerators();
                }
                return;
            }
        }
    }
    JAMI_WARN("Fail to set %s as moderator (participant not found)", uri.c_str());
}

void
Conference::updateModerators()
{
    {
        std::lock_guard<std::mutex> lk2(confInfoMutex_);
        for (auto& info : confInfo_) {
            auto uri = info.uri;
            uri = string_remove_suffix(uri, '@');
            info.isModerator = isModerator(uri);
        }
    }
    sendConferenceInfos();
}

bool
Conference::isMuted(std::string_view uri) const
{
    return std::find_if(participantsMuted_.begin(),
                        participantsMuted_.end(),
                        [&uri](std::string_view pMuted) {
                            return pMuted.find(uri) != std::string_view::npos;
                        })
           != participantsMuted_.end();
}

void
Conference::muteParticipant(const std::string& uri, const bool& state, const std::string& mediaType)
{
    // Mute host
    if (isHost(uri)) {
        if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_AUDIO) == 0) {
            if (state and not isMuted("host")) {
                JAMI_DBG("Mute host");
                participantsMuted_.emplace("host");
                unbindHost();
                updateMuted();
            } else if (not state and isMuted("host")) {
                JAMI_DBG("Unmute host");
                participantsMuted_.erase("host");
                bindHost();
                updateMuted();
            }
            emitSignal<DRing::CallSignal::AudioMuted>(id_, state);
            return;
        } else if (mediaType.compare(DRing::Media::Details::MEDIA_TYPE_VIDEO) == 0) {
#ifdef ENABLE_VIDEO
            if (state) {
                if (auto mixer = getVideoMixer()) {
                    mixer->stopInput();
                }
            } else {
                if (auto mixer = getVideoMixer()) {
                    mixer->switchInput(mediaInput_);
                }
            }
            return;
#endif
        }
    }

    // Mute participant
    for (const auto& p : participants_) {
        std::string_view peerURI = string_remove_suffix(uri, '@');
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(p)) {
            std::string_view partURI = call->getPeerNumber();
            partURI = string_remove_suffix(partURI, '@');
            if (partURI == peerURI) {
                if (state and not isMuted(partURI)) {
                    JAMI_DBG("Mute participant %s", partURI.data());
                    participantsMuted_.emplace(std::string(partURI));
                    unbindParticipant(p);
                    updateMuted();
                } else if (not state and isMuted(partURI)) {
                    JAMI_DBG("Unmute participant %s", partURI.data());
                    participantsMuted_.erase(std::string(partURI));
                    bindParticipant(p);
                    updateMuted();
                }
                return;
            }
        }
    }
}

void
Conference::updateMuted()
{
    {
        std::lock_guard<std::mutex> lk2(confInfoMutex_);
        for (auto& info : confInfo_) {
            auto uri = string_remove_suffix(info.uri, '@');
            if (uri.empty())
                uri = "host";
            info.audioMuted = isMuted(uri);
        }
    }
    sendConferenceInfos();
}


ConfInfo
Conference::getConfInfoHostUri(std::string_view uri)
{
    ConfInfo newInfo = confInfo_;
    for (auto& info : newInfo) {
        if (info.uri.empty()) {
            info.uri = uri;
            break;
        }
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
    for (const auto& p : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(p)) {
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                continue;
            if (account->getUsername() == uri) {
                return true;
            }
        }
    }
    return false;
}

void
Conference::updateConferenceInfo(ConfInfo confInfo)
{
    confInfo_ = std::move(confInfo);
    sendConferenceInfos();
}

void
Conference::hangupParticipant(const std::string& participant_id)
{
    if (isHost(participant_id)) {
        Manager::instance().detachLocalParticipant(id_);
        return;
    }

    for (const auto& p : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(p)) {
            std::string_view partURI = call->getPeerNumber();
            partURI = string_remove_suffix(partURI, '@');
            if (partURI == participant_id) {
                Manager::instance().hangupCall(call->getCallId());
                return;
            }
        }
    }
}

} // namespace jami
