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

#include <sstream>

#include "conference.h"
#include "manager.h"
#include "audio/audiolayer.h"
#include "audio/ringbufferpool.h"

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
#ifdef ENABLE_VIDEO
    getVideoMixer()->setOnSourcesUpdated([this](const std::vector<video::SourceInfo>&& infos) {
        runOnMainThread([w = weak(), infos = std::move(infos)] {
            auto shared = w.lock();
            if (!shared)
                return;
            ConfInfo newInfo;
            std::unique_lock<std::mutex> lk(shared->videoToCallMtx_);
            for (const auto& info : infos) {
                std::string uri = "local";
                auto it         = shared->videoToCall_.find(info.source);
                if (it == shared->videoToCall_.end())
                    it = shared->videoToCall_.emplace_hint(it, info.source, std::string());
                // If not local
                if (!it->second.empty()) {
                    // Retrieve calls participants
                    // TODO: this is a first version, we assume that the peer is not
                    // a master of a conference and there is only one remote
                    // In the future, we should retrieve confInfo from the call
                    // To merge layouts informations
                    if (auto call = Manager::instance().callFactory.getCall<SIPCall>(it->second)) {
                        uri = call->getPeerNumber();
                    }
                }
                newInfo.emplace_back(
                    ParticipantInfo {std::move(uri), info.x, info.y, info.w, info.h});
            }
            lk.unlock();

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
#ifdef ENABLE_VIDEO
    for (const auto& participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().exitConference();
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
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().enterConference(this);
        else
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
        if (participant_id == item) {
            if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
                videoMixer_->setActiveParticipant(call->getVideoRtp().getVideoReceive().get());
                return;
            }
        }
    }
    // Set local by default
    videoMixer_->setActiveParticipant(nullptr);
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
            call->sendTextMessage(std::map<std::string, std::string> {{"application/confInfo+json",
                                                                       confInfo}},
                                  call->getAccount().getFromUri());
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
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().exitConference();
#endif // ENABLE_VIDEO
    }
}

void
Conference::attach()
{
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
        JAMI_WARN("Invalid conference state in attach participant");
    }
}

void
Conference::detach()
{
    if (getState() == State::ACTIVE_ATTACHED) {
        Manager::instance().getRingBufferPool().unBindAll(RingBufferPool::DEFAULT_ID);
#ifdef ENABLE_VIDEO
        if (auto mixer = getVideoMixer()) {
            mixer->stopInput();
        }
#endif
        setState(State::ACTIVE_DETACHED);
    } else {
        JAMI_WARN("Invalid conference state in detach participant");
    }
}

void
Conference::bindParticipant(const std::string& participant_id)
{
    auto& rbPool = Manager::instance().getRingBufferPool();

    for (const auto& item : participants_) {
        if (participant_id != item)
            rbPool.bindCallID(participant_id, item);
        rbPool.flush(item);
    }

    rbPool.bindCallID(participant_id, RingBufferPool::DEFAULT_ID);
    rbPool.flush(RingBufferPool::DEFAULT_ID);
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
        auto details   = Manager::instance().getCallDetails(p);
        const auto tmp = details["DISPLAY_NAME"];
        result.emplace_back(tmp.empty() ? details["PEER_NUMBER"] : tmp);
    }
    return result;
}

bool
Conference::toggleRecording()
{
    const bool startRecording = Recordable::toggleRecording();
    return startRecording;
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

} // namespace jami
