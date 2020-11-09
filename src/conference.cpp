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
#include "audio/ringbufferpool.h"
#include "jamidht/jamiaccount.h"

#ifdef ENABLE_VIDEO
#include "sip/sipcall.h"
#include "client/videomanager.h"
#include "video/video_input.h"
#include "video/video_mixer.h"
#endif

#include "call_factory.h"

#include "logger.h"

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
                    active = info.source == videoMixer->getActiveParticipant()
                             or (uri.empty()
                                 and not videoMixer->getActiveParticipant()); // by default, local
                                                                              // is shown as active
                subCalls.erase(it->second);
                auto isModerator = shared->isModerator(uri);
                newInfo.emplace_back(ParticipantInfo {std::move(uri),
                                                      active,
                                                      info.x,
                                                      info.y,
                                                      info.w,
                                                      info.h,
                                                      !info.hasVideo,
                                                      false,
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
                    ParticipantInfo {std::move(uri), false, 0, 0, 0, 0, true, false, isModerator});
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
    for (const auto& item : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(item)) {
            if (participant_id == item
                || call->getPeerNumber().find(participant_id) != std::string::npos) {
                videoMixer_->setActiveParticipant(call->getVideoRtp().getVideoReceive().get());
                return;
            }
        }
    }
    // Set local by default
    videoMixer_->setActiveParticipant(nullptr);
}

void
Conference::setLayout(int layout)
{
    switch (layout) {
    case 0:
        getVideoMixer()->setVideoLayout(video::Layout::GRID);
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
    Json::Value jsonArray;
    std::vector<std::map<std::string, std::string>> toSend;
    {
        std::lock_guard<std::mutex> lk2(confInfoMutex_);
        for (const auto& info : confInfo_) {
            jsonArray.append(info.toJson());
        }
        toSend = confInfo_.toVectorMapStringString();
    }

    Json::StreamWriterBuilder builder;
    const auto confInfo = Json::writeString(builder, jsonArray);
    // Inform calls that the layout has changed
    for (const auto& participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            auto w = call->getAccount();
            auto account = w.lock();
            if (!account)
                continue;
            call->sendTextMessage(std::map<std::string, std::string> {{"application/confInfo+json",
                                                                       confInfo}},
                                  account->getFromUri());
        }
    }

    // Inform client that layout has changed
    jami::emitSignal<DRing::CallSignal::OnConferenceInfosUpdated>(id_, std::move(toSend));
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
        auto uri = call->getPeerNumber();
        auto separator = uri.find('@');
        if (separator != std::string::npos)
            uri = uri.substr(0, separator);
        if (!isModerator(uri)) {
            JAMI_WARN("Received conference order from a non master (%s)", uri.c_str());
            return;
        }

        std::string err;
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (!reader->parse(confOrder.c_str(), confOrder.c_str() + confOrder.size(), &root, &err)) {
            JAMI_WARN("Couldn't parse conference order from %s", uri.c_str());
            return;
        }
        if (root.isMember("layout")) {
            setLayout(root["layout"].asUInt());
        }
        if (root.isMember("activeParticipant")) {
            setActiveParticipant(root["activeParticipant"].asString());
        }
    }
}

bool
Conference::isModerator(const std::string& uri) const
{
    return std::find_if(moderators_.begin(),
                        moderators_.end(),
                        [&uri](const std::string& moderator) {
                            return moderator.find(uri) != std::string::npos;
                        })
           != moderators_.end();
}

void
Conference::setModerator(const std::string& uri, const bool& state)
{
    for (const auto& p : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(p)) {
            auto partURI = call->getPeerNumber();
            auto separator = partURI.find('@');
            if (separator != std::string::npos)
                partURI = partURI.substr(0, separator);
            if (partURI == uri) {
                if (state and not isModerator(uri)) {
                    JAMI_DBG("Add %s as moderator", partURI.c_str());
                    moderators_.emplace(uri);
                    updateModerators();
                    sendConferenceInfos();
                }
                else if (not state and isModerator(uri)) {
                    JAMI_DBG("Remove %s as moderator", partURI.c_str());
                    moderators_.erase(uri);
                    updateModerators();
                    sendConferenceInfos();
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
    std::lock_guard<std::mutex> lk2(confInfoMutex_);
    for (auto& info : confInfo_) {
        auto uri = info.uri;
        auto separator = uri.find('@');
        if (separator != std::string::npos)
            uri = uri.substr(0, separator);
        info.isModerator = isModerator(uri);
    }
}

} // namespace jami
